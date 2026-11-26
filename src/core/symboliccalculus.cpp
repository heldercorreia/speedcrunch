// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/symboliccalculus.h"

#include "core/anglemode.h"
#include "core/evaluator.h"
#include "core/numericalintegration.h"
#include "core/regexpatterns.h"
#include "core/settings.h"
#include "core/unicodechars.h"
#include "core/units.h"
#include "math/floatnum/floatcommon.h"
#include "math/rational.h"

#include <QCoreApplication>
#include <QMap>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QStringList>
#include <algorithm>
#include <deque>
#include <optional>
#include <variant>

namespace {

constexpr int MaxDepth = 96;
constexpr int MaxNodes = 4096;
constexpr int MaxWork = 60000;
constexpr int MaxOrderSteps = 256;
constexpr int MaxScalarOpcodes = 128;
// Limits for the passes that look for a simpler form of a result.
constexpr int MaxPasses = 4;
constexpr int PassWork = 20000;
constexpr int CandidateWork = 4000;
constexpr int MaxCommonFactors = 8;

struct Failure {
    QString message;
};

// A numeric value reaches the calculus as a literal with all its digits, but
// most of them are rounded. Only short literals are taken as exact numbers.
constexpr int LiteralDigits = 36;
// Coefficients can grow much longer, for example in diff(exp(2*x); 125).
// Keep room for cross products and their sum in exact arithmetic.
constexpr int ExactDigits = 120;
static_assert(2 * ExactDigits + 1 <= MAXDIGITS);

// An exact integer. HNumber rounds to its working precision, which is too
// short for large coefficients.
class Integer {
public:
    Integer(int value = 0)
    {
        float_create(&m_value);
        float_setinteger(&m_value, value);
    }

    Integer(const Integer& other)
    {
        float_create(&m_value);
        float_copy(&m_value, &other.m_value, EXACT);
    }

    Integer& operator=(const Integer& other)
    {
        if (this != &other)
            float_copy(&m_value, &other.m_value, EXACT);
        return *this;
    }

    ~Integer() { float_free(&m_value); }

    // Decimal digits with an optional leading minus sign.
    static Integer fromDigits(const QString& digits)
    {
        Integer result;
        float_setscientific(&result.m_value, digits.toLatin1().constData(), NULLTERMINATED);
        return result;
    }

    bool isValid() const { return !float_isnan(&m_value); }
    bool isZero() const { return float_iszero(&m_value); }
    bool isNegative() const { return float_getsign(&m_value) < 0; }
    bool isPositive() const { return float_getsign(&m_value) > 0; }

    // The number of decimal digits, or 0 for zero.
    int digits() const { return isZero() ? 0 : float_getexponent(&m_value) + 1; }

    // Only for values that fit, such as small exponents.
    int toInt() const { return float_asinteger(&m_value); }

    QString toString() const
    {
        if (!isValid())
            return QStringLiteral("NaN");
        if (isZero())
            return QStringLiteral("0");
        const int length = float_getlength(&m_value);
        QByteArray significand(length, '0');
        float_getsignificand(significand.data(), length, &m_value);
        return (isNegative() ? QStringLiteral("-") : QString()) + QString::fromLatin1(significand)
            + QString(digits() - length, QLatin1Char('0'));
    }

    HNumber toHNumber() const
    {
        if (!isValid())
            return HMath::nan();
        if (digits() <= DECPRECISION)
            return HNumber(toString().toLatin1().constData());
        // A number result only shows the leading digits.
        const QString text = toString();
        const int sign = isNegative() ? 1 : 0;
        const QString scientific = text.left(sign + 1) + QLatin1Char('.')
            + text.mid(sign + 1, DECPRECISION - 1) + QStringLiteral("e%1").arg(digits() - 1);
        return HNumber(scientific.toLatin1().constData());
    }

    Integer operator-() const
    {
        Integer result(*this);
        float_neg(&result.m_value);
        return result;
    }

    friend Integer abs(const Integer& value) { return value.isNegative() ? -value : value; }
    friend Integer operator+(const Integer& a, const Integer& b) { return apply(a, b, float_add); }
    friend Integer operator-(const Integer& a, const Integer& b) { return apply(a, b, float_sub); }
    friend Integer operator*(const Integer& a, const Integer& b) { return apply(a, b, float_mul); }

    // Truncates towards zero, like integer division in C++.
    static void divide(const Integer& dividend, const Integer& divisor, Integer& quotient,
                       Integer& remainder)
    {
        float_divmod(&quotient.m_value, &remainder.m_value, &dividend.m_value, &divisor.m_value, INTQUOT);
        float_geterror();
    }

    friend Integer operator/(const Integer& dividend, const Integer& divisor)
    {
        Integer quotient;
        Integer remainder;
        divide(dividend, divisor, quotient, remainder);
        return quotient;
    }

    friend Integer operator%(const Integer& dividend, const Integer& divisor)
    {
        Integer quotient;
        Integer remainder;
        divide(dividend, divisor, quotient, remainder);
        return remainder;
    }

    friend Integer gcd(Integer a, Integer b)
    {
        a = abs(a);
        b = abs(b);
        while (b.isValid() && !b.isZero()) {
            const Integer remainder = a % b;
            a = b;
            b = remainder;
        }
        return b.isValid() ? a : b;
    }

    friend Integer lcm(const Integer& a, const Integer& b)
    {
        if (a.isZero() || b.isZero())
            return 0;
        return abs(a) / gcd(a, b) * abs(b);
    }

    // The largest integer whose square is at most a non-negative value.
    friend Integer floorSqrt(const Integer& value)
    {
        if (value.isZero())
            return 0;
        // Newton's method falls to the root from any value above it.
        Integer root = fromDigits(QLatin1Char('1') + QString((value.digits() + 1) / 2, QLatin1Char('0')));
        for (;;) {
            const Integer next = (root + value / root) / 2;
            if (!next.isValid() || next >= root)
                return root;
            root = next;
        }
    }

    friend bool operator==(const Integer& a, const Integer& b) { return float_cmp(&a.m_value, &b.m_value) == 0; }
    friend bool operator!=(const Integer& a, const Integer& b) { return !(a == b); }
    friend bool operator<(const Integer& a, const Integer& b) { return float_cmp(&a.m_value, &b.m_value) < 0; }
    friend bool operator>(const Integer& a, const Integer& b) { return b < a; }
    friend bool operator<=(const Integer& a, const Integer& b) { return !(b < a); }
    friend bool operator>=(const Integer& a, const Integer& b) { return !(a < b); }

private:
    using Operation = char (*)(floatnum, cfloatnum, cfloatnum, int);

    static Integer apply(const Integer& a, const Integer& b, Operation operation)
    {
        Integer result;
        operation(&result.m_value, &a.m_value, &b.m_value, EXACT);
        // HMath reads the shared error state, so leave it clean.
        float_geterror();
        return result;
    }

    floatstruct m_value;
};

struct ExactNumber {
    Integer numerator = 0;
    Integer denominator = 1;
};

std::optional<ExactNumber> fraction(Integer numerator, Integer denominator)
{
    if (!numerator.isValid() || !denominator.isValid() || denominator.isZero())
        return std::nullopt;
    if (denominator.isNegative()) {
        numerator = -numerator;
        denominator = -denominator;
    }
    if (denominator != 1) {
        const Integer divisor = gcd(numerator, denominator);
        numerator = numerator / divisor;
        denominator = denominator / divisor;
    }
    if (!numerator.isValid() || !denominator.isValid()
        || numerator.digits() > ExactDigits || denominator.digits() > ExactDigits) {
        return std::nullopt;
    }
    return ExactNumber{numerator, denominator};
}

std::optional<ExactNumber> exactLiteral(const QString& text, int maxDigits = LiteralDigits)
{
    static const QRegularExpression decimal(QStringLiteral(
        R"(^([0-9]*)(?:\.([0-9]*))?(?:[eE]([+-]?[0-9]+))?$)"));
    const auto match = decimal.match(text);
    if (!match.hasMatch())
        return std::nullopt;

    const QString fractionalDigits = match.captured(2);
    QString digits = match.captured(1) + fractionalDigits;
    bool exponentOk = false;
    const int exponent = match.captured(3).toInt(&exponentOk);
    if ((!exponentOk && !match.captured(3).isEmpty()) || digits.isEmpty())
        return std::nullopt;

    qsizetype first = 0;
    while (first < digits.size() && digits.at(first) == QLatin1Char('0'))
        ++first;
    if (first == digits.size())
        return ExactNumber{};

    qsizetype last = digits.size();
    qint64 scale = qint64(exponent) - fractionalDigits.size();
    while (last > first && digits.at(last - 1) == QLatin1Char('0')) {
        --last;
        ++scale;
    }
    const qsizetype length = last - first;
    if (scale <= -maxDigits || length + qMax(qint64(0), scale) > maxDigits)
        return std::nullopt;

    digits = digits.mid(first, length);
    if (scale > 0)
        digits += QString(scale, QLatin1Char('0'));
    const QString denominator = scale < 0
        ? QStringLiteral("1") + QString(-scale, QLatin1Char('0')) : QStringLiteral("1");
    return fraction(Integer::fromDigits(digits), Integer::fromDigits(denominator));
}

using Expr = int;
using CalculusResult = std::variant<Expr, Quantity>;
constexpr Expr NoExpression = -1;

struct Key {
    Opcode::Type type;
    QString text;
    QVector<Expr> children;

    bool operator==(const Key& other) const
    {
        return type == other.type && text == other.text && children == other.children;
    }
};

size_t qHash(const Key& key, size_t seed)
{
    return qHashMulti(seed, static_cast<int>(key.type), key.text,
                      qHashRange(key.children.cbegin(), key.children.cend()));
}

struct Node {
    Key key;
    std::optional<ExactNumber> number;
    int size;
    int depth;
    bool containsUnit;
    // The complexity used to choose between equal forms: one per operation,
    // one per name and one per digit. A leading minus sign is free.
    int cost;
};

struct Span {
    int first;
    int end;
};

class Calculus {
    Q_DECLARE_TR_FUNCTIONS(Calculus)

public:
    Calculus(const Evaluator& evaluator, const QVector<Opcode>& opcodes,
               const QVector<Quantity>& constants, const QStringList& identifiers,
               const SymbolicCalculus::EvaluateSpan& evalSpan,
               const SymbolicCalculus::UnitLookup& lookupUnit,
               const SymbolicCalculus::VariableLookup& lookupVariable)
        : m_evaluator(evaluator)
        , m_opcodes(opcodes)
        , m_constants(constants)
        , m_identifiers(identifiers)
        , m_evalSpan(evalSpan)
        , m_lookupUnit(lookupUnit)
        , m_lookupVariable(lookupVariable)
    {
        m_zero = number(0);
        m_one = number(1);
        m_minusOne = number(-1);
    }

    Quantity evaluate(int first)
    {
        const auto type = m_opcodes.at(first).type;
        if (type == Opcode::Integrate || type == Opcode::Limit) {
            const CalculusResult result = type == Opcode::Limit ? limit(first, 0) : integrate(first, 0);
            if (const auto* numeric = std::get_if<Quantity>(&result))
                return *numeric;
            return resultValue(std::get<Expr>(result), type == Opcode::Limit);
        }
        const Call call = readCall(first);
        if (call.order.isZero()) {
            const QString& source = m_opcodes.at(first).text;
            if (source.isEmpty())
                limitExceeded();
            return Quantity::symbolic(source);
        }
        return resultValue(differentiate(call, 0));
    }

    void refineRule3(int first, int end, Quantity& result)
    {
        m_readingProportion = true;
        const QVector<Span> arguments = readArguments(first, end);
        if (arguments.size() != 3)
            return;

        const Expr a = simplify(readExpression(arguments.at(0), 0));
        const Expr b = simplify(readExpression(arguments.at(1), 0));
        const Expr c = simplify(readExpression(arguments.at(2), 0));
        const Expr base = divide(multiply({b, c}), a);
        const auto& rational = node(base).number;
        const HNumber numeric = rational
            ? HMath::multiplyDivide(HNumber(rational->numerator.toString().toLatin1().constData()),
                                    HNumber(1),
                                    HNumber(rational->denominator.toString().toLatin1().constData()))
            : result.numericValue().real;
        if (numeric.isNan())
            return;

        Quantity exactResult(numeric);
        exactResult.copyDimension(result);
        if (result.hasUnit())
            exactResult.setDisplayUnit(result.unit(), result.unitName());
        else if (result.hasDimension())
            Units::findUnit(exactResult);

        const Expr displayed = exactResult.hasUnit()
            ? divide(base, proportionScale(exactResult.unit(), exactResult.unitName())) : base;
        const auto& displayedNumber = node(displayed).number;
        if (!displayedNumber || displayedNumber->denominator != 1
            || displayedNumber->numerator.digits() > DECPRECISION) {
            QString text;
            render(displayed, 0, text);
            exactResult.setExactExpression(text);
        }
        result = exactResult;
    }

private:
    Quantity resultValue(Expr result, bool preserveLargeIntegers = false)
    {
        const auto& exact = node(result).number;
        if (exact && exact->denominator == 1
            && (!preserveLargeIntegers || exact->numerator.digits() <= DECPRECISION)) {
            return Quantity(exact->numerator.toHNumber());
        }
        QString rendered;
        render(result, 0, rendered);
        return Quantity::symbolic(rendered);
    }

    struct Call {
        Span expression;
        QString variable;
        HNumber order;
    };

    struct Bound {
        Expr expression = NoExpression;
        Quantity value = DMath::nan();
        int infinity = 0;
    };

    struct Interval {
        HNumber lower;
        HNumber upper;
    };

    struct PowerKernel {
        Expr base;
        Expr exponent;
    };

    struct Endpoint {
        int side = 0;
        int substitutionPower = 0;
    };

    struct SquareOffset {
        Expr argument;
        Expr offset;
    };

    struct DomainUnknown {};

    enum class LimitForm { Unknown, NoDomain, Term, Wave, Bound, Oscillatory, Vanishing, Divergent };

    // With h > 0 tending to zero, the term is (coefficient + o(1)) times
    // h^order * log(1/h)^logOrder * exp(exponent). The exponent is itself
    // an expression in the limiting variable. A wave has the same scale
    // and takes both signs arbitrarily close to the endpoint. A bound
    // gives an upper limit on the magnitude at that scale.
    struct LimitTerm {
        LimitForm form = LimitForm::Unknown;
        Expr coefficient = NoExpression;
        Expr order = NoExpression;
        Expr logOrder = NoExpression;
        Expr exponent = NoExpression;
        bool constant = false;
        bool analytic = false;
    };

    struct LimitPath {
        QString variable;
        Expr point;
        int infinity;
        int side;
        QHash<Expr, LimitTerm> terms;
        int depth = 0;
        int hopitalSteps = 0;
        bool oscillating = false;
    };

    enum class LimitStatus { Unknown, DoesNotExist, Finite, Infinite };

    struct LimitValue {
        LimitStatus status = LimitStatus::Unknown;
        Expr expression = NoExpression;
        int infinity = 0;
    };

    struct LimitEstimate { HNumber value; HNumber error; };

    struct LimitPoint {
        Expr expression = NoExpression;
        Quantity units{1};
        HNumber numeric = HMath::nan();
        int infinity = 0;
        bool approximate = false;
    };

    CalculusResult limit(int first, int nesting);
    LimitPoint readLimitPoint(Span span, int nesting);
    std::optional<int> readDirection(Span span);
    int namedInfinity(Span span) const;
    Expr stripLimitUnits(Expr expression, const QString& variable, const Quantity& variableUnits,
                         Quantity& units, bool& approximate);
    Expr attachLimitUnits(Expr expression, Quantity units);
    LimitTerm term(Expr coefficient, bool constant = false, bool analytic = false,
                   Expr order = NoExpression, Expr logOrder = NoExpression,
                   Expr exponent = NoExpression) const;
    LimitTerm limitTerm(Expr expression, LimitPath& path);
    LimitTerm analyzeLimit(Expr expression, LimitPath& path);
    LimitTerm limitSum(Expr expression, LimitPath& path);
    LimitTerm limitProduct(const LimitTerm& left, const LimitTerm& right, bool quotient, LimitPath& path);
    LimitTerm limitQuotient(Expr expression, LimitPath& path);
    LimitTerm limitPower(Expr expression, LimitPath& path, bool cubeRoot = false);
    LimitTerm limitFunction(Expr expression, LimitPath& path);
    LimitTerm taylorTerm(Expr expression, LimitPath& path);
    bool smoothOnPath(Expr expression, LimitPath& path);
    bool canEstimateLimit(Expr expression, LimitPath& path);
    std::optional<int> limitSign(Expr expression);
    std::optional<int> circularSign(Expr angle, bool sine);
    std::optional<int> termSign(const LimitTerm& term);
    std::optional<int> compareGrowth(const LimitTerm& left, const LimitTerm& right, LimitPath& path);
    LimitValue classifyLimit(const LimitTerm& term, LimitPath& path);
    std::optional<LimitEstimate> estimateLimit(Expr raw, const LimitPoint& point, LimitPath& path);

    CalculusResult integrate(int first, int nesting);
    Bound readBound(Span span, int nesting);
    Expr antiderivative(Expr expression, const QString& variable, int depth = 0);
    std::optional<PowerKernel> powerKernel(Expr expression);
    std::optional<PowerKernel> scaledPower(Expr expression, Expr& coefficient);
    std::optional<SquareOffset> squareWithPositiveOffset(Expr expression);
    std::optional<QVector<Expr>> polynomial(Expr expression, const QString& variable);
    Expr integratePolynomial(const QVector<Expr>& coefficients, Expr variable);
    Expr powerPrimitive(const PowerKernel& kernel);
    Expr kernelPrimitive(Expr expression, Expr& argument);
    std::optional<QVector<Expr>> exponentPolynomial(const QVector<Expr>& factors, const QString& variable,
                                                    QVector<Expr>& kernels, QVector<Expr>& rest);
    Expr gaussianPrimitive(const QVector<Expr>& factors, const QString& variable);
    int signAtInfinity(const QVector<Expr>& coefficients, int direction);
    Expr limitAtInfinity(Expr expression, const QString& variable, int direction);
    bool dependsOn(Expr expression, const QString& variable);
    Expr substitute(Expr expression, Expr variable, Expr value, bool simplifyResult = true);
    HNumber magnitude(Expr expression, const QString& variable = {}, const HNumber& x = 0);
    std::optional<Interval> enclosure(Expr expression, const QString& variable,
                                      const std::optional<Interval>& interval, HNumber* phaseWidth,
                                      bool forLimit = false);
    void verifyDomain(Expr raw, const QString& variable, const std::optional<Interval>& interval,
                      HNumber* phaseWidth);
    Endpoint verifyPowerIntegral(Expr raw, const QString& variable, const Interval& interval,
                                 HNumber* phaseWidth);
    int logarithmEndpoint(Expr raw, const QString& variable, const Interval& interval);
    Expr infiniteLimit(Expr raw, Expr primitive, const QString& variable, const Bound& lower,
                       const Bound& upper);

    const Evaluator& m_evaluator;
    const QVector<Opcode>& m_opcodes;
    const QVector<Quantity>& m_constants;
    const QStringList& m_identifiers;
    const SymbolicCalculus::EvaluateSpan& m_evalSpan;
    const SymbolicCalculus::UnitLookup& m_lookupUnit;
    const SymbolicCalculus::VariableLookup& m_lookupVariable;
    std::deque<Node> m_nodes;
    QHash<Key, Expr> m_interned;
    int m_remainingWork = MaxWork;
    bool m_samplingDomain = false;
    bool m_readingProportion = false;
    Expr m_zero;
    Expr m_one;
    Expr m_minusOne = NoExpression;
    // Sums by their set of terms, so that x+1 and 1+x are the same sum.
    QHash<QVector<Expr>, Expr> m_sums;
    QHash<Expr, bool> m_depends;
    QString m_dependsVariable;
    QHash<Expr, Quantity> m_limitUnits;

    [[noreturn]] static void limitExceeded()
    {
        throw Failure{tr("symbolic calculus limit exceeded")};
    }

    void spend(int work = 1)
    {
        m_remainingWork -= work;
        if (m_remainingWork < 0 || m_evaluator.shouldStopForTimeLimit())
            limitExceeded();
    }

    const Node& node(Expr expression) const { return m_nodes[expression]; }

    Expr make(Opcode::Type type, const QVector<Expr>& children = {},
              const QString& text = {}, const std::optional<ExactNumber>& exact = std::nullopt)
    {
        if (text.size() > SymbolicCalculus::MaxSourceLength)
            limitExceeded();
        spend(1 + children.size());
        const Key key{type, text, children};
        const auto existing = m_interned.constFind(key);
        if (existing != m_interned.constEnd())
            return existing.value();

        int size = 1;
        int depth = 1;
        bool containsUnit = type == Opcode::UnitRef;
        int cost = 1;
        if (type == Opcode::Load && exact) {
            cost = qMax(1, exact->numerator.digits());
            if (exact->denominator != 1)
                cost += exact->denominator.digits() + 1;
        } else if (type == Opcode::Load) {
            cost = qMax(1, int(text.size()));
        } else if (type == Opcode::Add || type == Opcode::Mul) {
            cost = int(children.size()) - 1;
        }
        for (Expr child : children) {
            const Node& operand = node(child);
            size += operand.size;
            depth = qMax(depth, operand.depth + 1);
            containsUnit = containsUnit || operand.containsUnit;
            cost += operand.cost;
        }
        if (type == Opcode::Mul && !children.isEmpty() && children.at(0) == m_minusOne)
            cost -= 2;
        if (size > MaxNodes || depth > MaxDepth)
            limitExceeded();

        const Expr expression = static_cast<int>(m_nodes.size());
        m_nodes.push_back({key, exact, size, depth, containsUnit, cost});
        m_interned.insert(key, expression);
        return expression;
    }

    Expr number(const ExactNumber& value)
    {
        QString text = value.numerator.toString();
        if (value.denominator != 1)
            text += QLatin1Char('/') + value.denominator.toString();
        return make(Opcode::Load, {}, text, value);
    }

    // Every coefficient must fold into one number, or the result could show
    // a long chain of numeric factors and terms that were not combined.
    static ExactNumber exact(const Integer& numerator, const Integer& denominator)
    {
        const auto value = fraction(numerator, denominator);
        if (!value)
            limitExceeded();
        return *value;
    }

    Expr number(int value, int denominator = 1)
    {
        return number(ExactNumber{value, denominator});
    }

    Expr reference(const QString& name) { return make(Opcode::Ref, {}, name); }

    bool isConstant(const QString& name) const
    {
        return name == QLatin1String("pi") || name == QStringLiteral("π")
            || name == QLatin1String("e") || name == QStringLiteral("ℯ")
            || name == QLatin1String("ans")
            || name == QString(UnicodeChars::Infinity)
            || m_evaluator.getVariable(name).type() == Variable::BuiltIn;
    }

    bool isVariable(const QString& name) const
    {
        return !name.isEmpty() && !isConstant(name)
            && !FunctionRepo::instance()->find(name) && !m_evaluator.hasUserFunction(name);
    }

    int callEnd(int first, int end) const
    {
        const unsigned index = m_opcodes.at(first).index;
        if (index <= static_cast<unsigned>(first) || index >= static_cast<unsigned>(end)
            || m_opcodes.at(index).type != Opcode::Function) {
            throw Failure{tr("invalid expression")};
        }
        return static_cast<int>(index);
    }

    QVector<Span> argumentsFor(int first)
    {
        const int end = callEnd(first, m_opcodes.size());
        const unsigned count = m_opcodes.at(end).index;
        const auto type = m_opcodes.at(first).type;
        if (count < (type == Opcode::Limit ? 2u : 1u) || count > (type == Opcode::Diff ? 3u : 4u))
            throw Failure{tr("wrong number of arguments")};
        const QVector<Span> arguments = readArguments(first + 1, end);
        if (arguments.size() != count)
            throw Failure{tr("invalid expression")};
        return arguments;
    }

    QVector<Span> readArguments(int first, int end)
    {
        if (end - first > MaxNodes)
            limitExceeded();

        QVector<Span> arguments;
        for (int pc = first; pc < end; ++pc) {
            spend();
            const Opcode& opcode = m_opcodes.at(pc);
            if (opcode.type == Opcode::Nop)
                continue;
            if (opcode.isCalculus()) {
                const int close = callEnd(pc, end);
                arguments.append({pc, close + 1});
                pc = close;
                continue;
            }
            const int operands = opcode.operandCount();
            if (operands < 0 || operands > arguments.size())
                throw Failure{tr("invalid expression")};
            const int start = operands ? arguments.at(arguments.size() - operands).first : pc;
            arguments.resize(arguments.size() - operands);
            arguments.append({start, pc + 1});
        }
        return arguments;
    }

    QString variableArgument(Span span)
    {
        const Opcode& opcode = m_opcodes.at(span.first);
        const QString name = m_identifiers.value(opcode.index);
        if (span.end == span.first + 1 && opcode.type == Opcode::Ref && isVariable(name))
            return name;
        return {};
    }

    Call readCall(int first)
    {
        const QVector<Span> arguments = argumentsFor(first);
        const auto count = arguments.size();
        const QString variable = count >= 2 ? variableArgument(arguments.at(1)) : QString();
        if (count == 3 && variable.isEmpty())
            throw Failure{tr("expected a variable name as the second argument")};

        HNumber order(1);
        if (count == 3 || (count == 2 && variable.isEmpty()))
            order = readOrder(arguments.constLast());
        return {arguments.at(0), variable, order};
    }

    bool canEvaluateScalar(Span span) const
    {
        for (int pc = span.first; pc < span.end; ++pc) {
            const Opcode& opcode = m_opcodes.at(pc);
            switch (opcode.type) {
            case Opcode::Ref: {
                const QString name = m_identifiers.value(opcode.index);
                if (const Function* function = FunctionRepo::instance()->find(name)) {
                    const QString& identifier = function->identifier();
                    if (!functionRules().contains(identifier)
                        && identifier != QLatin1String("floor") && identifier != QLatin1String("ceil")
                        && identifier != QLatin1String("abs") && identifier != QLatin1String("round")) {
                        return false;
                    }
                } else if (m_evaluator.hasUserFunction(name)) {
                    return false;
                }
                break;
            }
            case Opcode::Load: case Opcode::UnitRef: case Opcode::Unit: case Opcode::Function:
            case Opcode::Neg: case Opcode::Add: case Opcode::Sub: case Opcode::Mul:
            case Opcode::Div: case Opcode::Pow: case Opcode::DivUnit: case Opcode::Percent:
            case Opcode::IntDiv: case Opcode::Modulo:
                break;
            default:
                return false;
            }
        }

        return true;
    }

    HNumber readOrder(Span span)
    {
        const int length = span.end - span.first;
        if (length > MaxScalarOpcodes)
            limitExceeded();
        const Quantity value = canEvaluateScalar(span)
            ? m_evalSpan(span.first, length, {}, Quantity()) : DMath::nan();
        if (value.isNan() || value.isSymbolic() || !value.isInteger() || value.isNegative())
            throw Failure{tr("invalid order (expected a non-negative, dimensionless integer)")};
        return value.numericValue().real;
    }

    Expr scalarLiteral(const HNumber& value)
    {
        auto format = HNumber::Format::Scientific() + HNumber::Format::Point()
            + HNumber::Format::Precision(DECPRECISION);
        format.removeTrailingZeroes = true;
        const QString text = HMath::format(HMath::abs(value), format);
        const auto exact = exactLiteral(text);
        const Expr literal = exact ? number(*exact) : make(Opcode::Load, {}, text);
        return value.isNegative() ? make(Opcode::Neg, {literal}) : literal;
    }

    Expr lastAnswer()
    {
        const QString name = QStringLiteral("ans");
        const Quantity value = m_evaluator.getVariable(name).value();
        if (!m_evaluator.hasVariable(name) || value.isNan() || value.isSymbolic()
            || value.isCollection() || value.hasDimension() || value.hasUnit()
            || (!value.isReal() && !Settings::instance()->complexNumbers)) {
            throw Failure{tr("ans must hold a unitless number inside the expression")};
        }
        // Capture the value before the derivative becomes the new answer.
        const CNumber scalar = value.numericValue();
        const Expr real = scalarLiteral(scalar.real);
        if (scalar.imag.isZero())
            return real;
        const Expr imaginary = make(Opcode::Mul, {scalarLiteral(scalar.imag), reference(QStringLiteral("i"))});
        return make(Opcode::Add, {real, imaginary});
    }

    Expr proportionScale(const CNumber& scale, const QString& unitName)
    {
        if (!scale.isReal() || !scale.isPositive()
            || Units::isAffineUnitName(unitName) || Units::isExplicitAngleUnitName(unitName)) {
            throw Failure{};
        }
        const Expr literal = scalarLiteral(scale.real);
        if (!node(literal).number)
            throw Failure{};
        return literal;
    }

    Expr proportionReference(const QString& name, const Quantity& binding)
    {
        if (!binding.isReal() || binding.isCollection() || binding.isApproximate())
            throw Failure{};

        if (m_evaluator.isBuiltInVariable(name) && binding == m_evaluator.getVariable(name).value()) {
            if (name == QLatin1String("pi") || name == QStringLiteral("π"))
                return reference(QStringLiteral("π"));
            if (name == QLatin1String("e") || name == QStringLiteral("ℯ"))
                return reference(QStringLiteral("e"));
        }

        const QString& stored = binding.exactExpression();
        if (!stored.isEmpty()) {
            const auto match = RegExpPatterns::integerOrFraction().match(stored);
            if (!match.hasMatch() || stored.size() > 2 * ExactDigits + 2)
                throw Failure{};
            const Integer numerator = Integer::fromDigits(match.captured(1));
            const Integer denominator = match.capturedStart(2) < 0
                ? Integer(1) : Integer::fromDigits(match.captured(2));
            const Expr value = number(exact(numerator, denominator));
            return binding.hasUnit()
                ? multiply({value, proportionScale(binding.unit(), binding.unitName())}) : value;
        }

        const HNumber value = binding.numericValue().real;
        const Expr magnitude = scalarLiteral(HMath::abs(value));
        if (!node(magnitude).number)
            throw Failure{};
        return value.isNegative() ? negate(magnitude) : magnitude;
    }

    Expr readExpression(Span span, int nesting, bool* limitApproximation = nullptr)
    {
        if (nesting >= MaxDepth)
            limitExceeded();
        QVector<Expr> stack;
        for (int pc = span.first; pc < span.end; ++pc) {
            spend();
            const Opcode& opcode = m_opcodes.at(pc);
            const int count = opcode.operandCount();
            if (count < 0 || count > stack.size())
                throw Failure{tr("invalid expression")};

            switch (opcode.type) {
            case Opcode::Nop:
                break;
            case Opcode::Diff: {
                if (m_readingProportion)
                    throw Failure{};
                const int end = callEnd(pc, span.end);
                const Call inner = readCall(pc);
                stack.append(inner.order.isZero() ? readExpression(inner.expression, nesting + 1)
                                                  : differentiate(inner, nesting + 1));
                pc = end;
                break;
            }
            case Opcode::Integrate:
            case Opcode::Limit: {
                if (m_readingProportion)
                    throw Failure{};
                const int end = callEnd(pc, span.end);
                const bool isLimit = opcode.type == Opcode::Limit;
                const CalculusResult inner = isLimit ? limit(pc, nesting + 1) : integrate(pc, nesting + 1);
                const auto* expression = std::get_if<Expr>(&inner);
                if (!expression) {
                    throw Failure{isLimit ? tr("the nested limit needs an exact symbolic result")
                                          : tr("the nested integral needs a symbolic result")};
                }
                stack.append(*expression);
                pc = end;
                break;
            }
            case Opcode::Load: {
                const Quantity value = m_constants.value(opcode.index);
                if (value.isNan())
                    throw Failure{tr("invalid number")};
                const QString text = opcode.text.isEmpty()
                    ? DMath::format(value, Quantity::Format::Fixed() + Quantity::Format::Point())
                    : opcode.text;
                if (text.size() > SymbolicCalculus::MaxSourceLength)
                    limitExceeded();
                spend(1 + text.size() / 32);
                auto exact = exactLiteral(text, m_readingProportion ? ExactDigits : LiteralDigits);
                static const QRegularExpression integerWithBase(
                    QStringLiteral(R"(^0[boxd][0-9A-Fa-f]+$)"),
                    QRegularExpression::CaseInsensitiveOption);
                if (!exact && text.size() <= LiteralDigits && value.isInteger()
                    && integerWithBase.match(text).hasMatch()) {
                    const auto format = HNumber::Format::Fixed() + HNumber::Format::Point()
                        + HNumber::Format::Precision(0);
                    exact = fraction(Integer::fromDigits(HMath::format(value.numericValue().real, format)), 1);
                }
                stack.append(exact ? number(*exact) : make(Opcode::Load, {}, text));
                break;
            }
            case Opcode::Ref: {
                QString name = m_identifiers.value(opcode.index);
                if (m_readingProportion) {
                    const auto binding = m_lookupVariable(name);
                    if (binding)
                        stack.append(proportionReference(name, *binding));
                    else if (FunctionRepo::instance()->find(name))
                        stack.append(reference(name));
                    else
                        throw Failure{};
                    break;
                }
                if (name == QLatin1String("ans")) {
                    stack.append(lastAnswer());
                    if (limitApproximation)
                        *limitApproximation |= m_evaluator.getVariable(name).value().isApproximate();
                    break;
                }
                if (name == QLatin1String("pi"))
                    name = QStringLiteral("π");
                else if (name == QStringLiteral("ℯ"))
                    name = QStringLiteral("e");
                stack.append(reference(name));
                break;
            }
            case Opcode::UnitRef: {
                const QString name = m_identifiers.value(opcode.index);
                Quantity unit;
                if (!m_lookupUnit(name, &unit))
                    throw Failure{tr("unknown unit %1").arg(name.toHtmlEscaped())};
                if (m_readingProportion) {
                    if (unit.isApproximate() || Units::isAffineUnitName(name)
                        || Units::isExplicitAngleUnitName(name)) {
                        throw Failure{};
                    }
                    stack.append(proportionScale(unit.numericValue(), unit.unitName()));
                    break;
                }
                if (Units::isAffineUnitName(name) || Units::isAffineUnitName(unit.unitName()))
                    throw Failure{tr("units with an offset are unsupported in symbolic differentiation")};
                stack.append(make(Opcode::UnitRef, {}, name));
                break;
            }
            case Opcode::Neg:
                stack.append(make(Opcode::Neg, {stack.takeLast()}));
                break;
            case Opcode::Add: case Opcode::Sub: case Opcode::Mul: case Opcode::Div:
            case Opcode::Pow: case Opcode::Unit: {
                const Expr right = stack.takeLast();
                const Expr left = stack.takeLast();
                stack.append(make(opcode.type, {left, right}));
                break;
            }
            case Opcode::Function: {
                const int first = stack.size() - count;
                const Node& callable = node(stack.at(first));
                if (callable.key.type != Opcode::Ref)
                    throw Failure{tr("invalid expression")};
                const QString name = callable.key.text;
                const Function* function = FunctionRepo::instance()->find(name);
                const QString identifier = function ? function->identifier() : name;
                if (m_readingProportion && identifier == QLatin1String("rule3") && count == 4) {
                    const Expr a = simplify(stack.at(first + 1));
                    const Expr b = simplify(stack.at(first + 2));
                    const Expr c = simplify(stack.at(first + 3));
                    stack[first] = divide(multiply({b, c}), a);
                    stack.resize(first + 1);
                    break;
                }
                const auto rule = functionRules().constFind(identifier);
                const bool limitFunction = limitApproximation && (identifier == QLatin1String("floor")
                    || identifier == QLatin1String("ceil") || identifier == QLatin1String("gamma"));
                if (rule == functionRules().constEnd() && !limitFunction)
                    throw Failure{tr("unsupported function %1").arg(name.toHtmlEscaped())};
                if ((limitFunction ? 1 : rule->arity) != count - 1)
                    throw Failure{tr("wrong number of arguments to %1").arg(name.toHtmlEscaped())};
                stack[first] = make(Opcode::Function, stack.sliced(first + 1, count - 1), identifier);
                stack.resize(first + 1);
                break;
            }
            default:
                throw Failure{limitApproximation ? tr("unsupported expression in limit")
                                                 : tr("unsupported expression in symbolic differentiation")};
            }
        }
        if (stack.size() != 1)
            throw Failure{tr("invalid expression")};
        return stack.constLast();
    }

    void infer(Expr expression, QSet<QString>& variables, bool forLimit = false)
    {
        spend();
        const Key& key = node(expression).key;
        if (key.type == Opcode::Ref) {
            if (key.text == QString(UnicodeChars::Infinity)
                || (forLimit && SymbolicCalculus::isNamedInfinity(key.text) && !m_lookupVariable(key.text))) {
                throw Failure{forLimit ? tr("infinity is only supported as an approach point")
                                       : tr("infinity is only supported as an integration bound")};
            }
            if (isVariable(key.text))
                variables.insert(key.text);
            else if (!isConstant(key.text) || m_evaluator.getVariable(key.text).value().isSymbolic())
                throw Failure{tr("unsupported symbol %1").arg(key.text.toHtmlEscaped())};
        }
        for (Expr child : key.children)
            infer(child, variables, forLimit);
    }

    Expr differentiate(const Call& call, int nesting)
    {
        Expr expression = readExpression(call.expression, nesting);
        QSet<QString> variables;
        infer(expression, variables);
        QString variable = call.variable;
        if (variable.isEmpty()) {
            if (variables.size() > 1)
                throw Failure{tr("ambiguous variable (specify which variable to differentiate)")};
            if (variables.isEmpty())
                return m_zero;
            variable = *variables.constBegin();
        }

        expression = simplify(expression);
        for (int step = 0; HNumber(step) < call.order && expression != m_zero; ++step) {
            if (step == MaxOrderSteps)
                limitExceeded();
            expression = derivative(expression, variable);
        }
        return simplified(expression, variable);
    }

    Expr multiply(QVector<Expr> factors)
    {
        QVector<Expr> terms;
        for (Expr factor : factors) {
            const Key& key = node(factor).key;
            if (key.type == Opcode::Mul)
                terms += key.children;
            else
                terms.append(factor);
        }
        spend(terms.size());
        if (terms.size() == 1)
            return terms.at(0);

        // Keep a single fraction bar, so that factors such as x*2/x^3 cancel.
        QVector<Expr> numerators;
        QVector<Expr> denominators;
        for (Expr factor : terms) {
            const Key& key = node(factor).key;
            if (key.type == Opcode::Div) {
                numerators.append(key.children.at(0));
                denominators.append(key.children.at(1));
            } else {
                numerators.append(factor);
            }
        }
        if (!denominators.isEmpty())
            return divide(multiply(numerators), multiply(denominators));

        ExactNumber coefficient{1, 1};
        QVector<Expr> product;
        QMap<Expr, int> powers;
        for (Expr factor : terms) {
            const Node& value = node(factor);
            if (factor == m_zero)
                return m_zero;
            if (value.number) {
                coefficient = exact(coefficient.numerator * value.number->numerator,
                                    coefficient.denominator * value.number->denominator);
                continue;
            }
            const auto [base, exponent] = factorPower(factor);
            powers[base] += exponent;
        }
        mergeRadicals(powers, powers);
        const Expr scalar = number(coefficient);
        if (scalar != m_one || (product.isEmpty() && powers.isEmpty()))
            product.prepend(scalar);
        for (auto it = powers.constBegin(); it != powers.constEnd(); ++it)
            product.append(it.value() == 1 ? it.key() : power(it.key(), number(it.value())));

        return product.size() == 1 ? product.at(0) : make(Opcode::Mul, product);
    }

    Expr add(QVector<Expr> operands)
    {
        QVector<Expr> terms;
        for (Expr operand : operands) {
            const Key& key = node(operand).key;
            if (key.type == Opcode::Add)
                terms += key.children;
            else
                terms.append(operand);
        }
        spend(terms.size());

        QHash<Expr, ExactNumber> coefficients;
        QVector<Expr> bases;
        QVector<Expr> sum;
        for (Expr term : terms) {
            auto [coefficient, base] = coefficientOf(term);
            const Key& key = node(term).key;
            if (key.type == Opcode::Div) {
                // Like terms can also be fractions, as in 3/x^2 and -2/x^2.
                const auto [above, numerator] = coefficientOf(key.children.at(0));
                const auto [below, denominator] = coefficientOf(key.children.at(1));
                coefficient = exact(above.numerator * below.denominator, above.denominator * below.numerator);
                base = denominator == m_one ? numerator : make(Opcode::Div, {numerator, denominator});
            }
            auto existing = coefficients.find(base);
            if (existing == coefficients.end()) {
                bases.append(base);
                coefficients.insert(base, coefficient);
            } else {
                existing.value() = exact(
                    existing->numerator * coefficient.denominator + coefficient.numerator * existing->denominator,
                    existing->denominator * coefficient.denominator);
            }
        }
        for (Expr base : bases) {
            const Expr coefficient = number(coefficients.value(base));
            if (coefficient != m_zero)
                sum.append(multiply({coefficient, base}));
        }
        if (sum.isEmpty())
            return m_zero;
        if (sum.size() == 1)
            return sum.at(0);
        // The first order of a set of terms is kept for all later sums with
        // the same terms, so that equal sums are equal expressions.
        QVector<Expr> set = sum;
        std::sort(set.begin(), set.end());
        const auto existing = m_sums.constFind(set);
        if (existing != m_sums.constEnd())
            return existing.value();
        const Expr result = make(Opcode::Add, sum);
        m_sums.insert(set, result);
        return result;
    }

    Expr negate(Expr operand)
    {
        const Key& key = node(operand).key;
        if (key.type == Opcode::Add) {
            QVector<Expr> terms;
            for (Expr child : key.children)
                terms.append(negate(child));
            return add(terms);
        }
        return multiply({number(-1), operand});
    }

    Expr subtract(Expr left, Expr right) { return add({left, negate(right)}); }

    // Splits c*rest into its exact coefficient c and the rest.
    QPair<ExactNumber, Expr> coefficientOf(Expr expression)
    {
        const Node& value = node(expression);
        if (value.number)
            return {*value.number, m_one};
        if (value.key.type == Opcode::Mul) {
            const auto& scalar = node(value.key.children.at(0)).number;
            if (scalar) {
                const ExactNumber coefficient = *scalar;
                const auto rest = value.key.children.sliced(1);
                return {coefficient, rest.size() == 1 ? rest.at(0) : make(Opcode::Mul, rest)};
            }
        }
        return {ExactNumber{1, 1}, expression};
    }

    QPair<Expr, int> factorPower(Expr factor) const
    {
        const Key& key = node(factor).key;
        if (key.type == Opcode::Pow) {
            const auto& exponent = node(key.children.at(1)).number;
            if (exponent && exponent->denominator == 1
                && exponent->numerator > 0 && exponent->numerator <= MaxNodes) {
                return {key.children.at(0), exponent->numerator.toInt()};
            }
        }
        return {factor, 1};
    }

    QMap<Expr, int> factorPowers(Expr expression)
    {
        const Key& key = node(expression).key;
        const QVector<Expr> factors = key.type == Opcode::Mul ? key.children : QVector<Expr>{expression};
        spend(factors.size());
        QMap<Expr, int> powers;
        for (Expr factor : factors) {
            const auto [base, exponent] = factorPower(factor);
            powers[base] += exponent;
        }
        return powers;
    }

    // The radicand u when the expression is sqrt(u).
    Expr radicand(Expr expression) const
    {
        const Key& key = node(expression).key;
        return key.type == Opcode::Function && key.text == QLatin1String("sqrt")
                && key.children.size() == 1
            ? key.children.at(0) : NoExpression;
    }

    // Where sqrt(u) is a factor, u is sqrt(u)^2, so that x*sqrt(x) is
    // sqrt(x)^3 and u^3/sqrt(u)^11 cancels to 1/sqrt(u)^5. The expression
    // already needs sqrt(u), so this keeps its domain. Both maps can be the
    // same, for a single product. Returns whether anything changed.
    bool mergeRadicals(QMap<Expr, int>& first, QMap<Expr, int>& second)
    {
        QVector<Expr> roots;
        for (const QMap<Expr, int>* powers : {&first, &second}) {
            for (auto it = powers->cbegin(); it != powers->cend(); ++it) {
                if (radicand(it.key()) != NoExpression && !roots.contains(it.key()))
                    roots.append(it.key());
            }
        }
        bool changed = false;
        for (Expr root : roots) {
            const Expr inner = radicand(root);
            for (QMap<Expr, int>* powers : {&first, &second}) {
                const auto plain = powers->find(inner);
                if (plain == powers->end())
                    continue;
                const int exponent = plain.value();
                powers->erase(plain);
                (*powers)[root] += 2 * exponent;
                changed = true;
            }
        }
        return changed;
    }

    Expr productOfPowers(const QMap<Expr, int>& powers)
    {
        QVector<Expr> factors;
        for (auto factor = powers.constBegin(); factor != powers.constEnd(); ++factor) {
            if (factor.value() > 0)
                factors.append(power(factor.key(), number(factor.value())));
        }
        return multiply(factors);
    }

    Expr divide(Expr left, Expr right)
    {
        if (right == m_zero)
            throw Failure{tr("division by zero")};
        if (left == m_zero || right == m_one)
            return left;
        // Keep a single fraction bar, so that common factors can cancel.
        if (node(left).key.type == Opcode::Div) {
            const QVector<Expr> parts = node(left).key.children;
            return divide(parts.at(0), multiply({parts.at(1), right}));
        }
        if (node(right).key.type == Opcode::Div) {
            const QVector<Expr> parts = node(right).key.children;
            return divide(multiply({left, parts.at(1)}), parts.at(0));
        }
        const auto& denominator = node(right).number;
        if (denominator) {
            const auto inverse = fraction(denominator->denominator, denominator->numerator);
            if (inverse)
                return multiply({left, number(*inverse)});
        }
        if (left == right)
            return m_one;

        auto numeratorPowers = factorPowers(left);
        auto denominatorPowers = factorPowers(right);
        bool cancelled = mergeRadicals(numeratorPowers, denominatorPowers);
        for (auto divisor = denominatorPowers.begin(); divisor != denominatorPowers.end(); ++divisor) {
            auto dividend = numeratorPowers.find(divisor.key());
            if (dividend != numeratorPowers.end()) {
                const int shared = qMin(dividend.value(), divisor.value());
                dividend.value() -= shared;
                divisor.value() -= shared;
                cancelled = true;
            }
        }
        if (cancelled) {
            left = productOfPowers(numeratorPowers);
            right = productOfPowers(denominatorPowers);
            if (node(right).number)
                return divide(left, right);
        }
        // Keep an integer coefficient above the bar and a positive one below,
        // as in 3/(8*x), so that equal fractions are equal expressions.
        const auto [above, numerator] = coefficientOf(left);
        const auto [below, rest] = coefficientOf(right);
        const ExactNumber scale = exact(above.numerator * below.denominator, above.denominator * below.numerator);
        left = multiply({number(ExactNumber{scale.numerator, 1}), numerator});
        right = multiply({number(ExactNumber{scale.denominator, 1}), rest});
        return make(Opcode::Div, {left, right});
    }

    Expr power(Expr base, Expr exponent)
    {
        if (exponent == m_zero || base == m_one)
            return m_one;
        if (exponent == m_one)
            return base;

        const auto& value = node(base).number;
        const auto& exp = node(exponent).number;
        const auto [innerBase, innerExponent] = factorPower(base);
        if (innerBase != base && exp && exp->denominator == 1
            && exp->numerator > 0 && exp->numerator <= MaxNodes) {
            return power(innerBase, number(innerExponent * exp->numerator.toInt()));
        }
        if (base == m_zero && exp && exp->numerator.isPositive())
            return m_zero;
        // u^(n/2) is sqrt(u)^n, so that radicals and rational powers look the
        // same and can combine.
        if (exp && exp->denominator == 2) {
            const Expr root = function(QStringLiteral("sqrt"), base);
            if (exp->numerator.isNegative())
                return divide(m_one, power(root, number(ExactNumber{-exp->numerator, 1})));
            return power(root, number(ExactNumber{exp->numerator, 1}));
        }
        if (value && exp && exp->denominator == 1 && abs(exp->numerator) <= MaxOrderSteps) {
            int n = exp->numerator.toInt();
            if (n < 0 && base == m_zero)
                throw Failure{tr("division by zero")};
            ExactNumber result{1, 1};
            const ExactNumber factor = n < 0
                ? ExactNumber{value->denominator, value->numerator} : *value;
            for (n = qAbs(n); n > 0; --n) {
                spend();
                const auto next = fraction(result.numerator * factor.numerator,
                                           result.denominator * factor.denominator);
                if (!next)
                    return make(Opcode::Pow, {base, exponent});
                result = *next;
            }
            return number(result);
        }
        return make(Opcode::Pow, {base, exponent});
    }

    Expr function(const QString& name, Expr argument)
    {
        const auto& exact = node(argument).number;
        if (name == QLatin1String("abs") && exact)
            return number(ExactNumber{abs(exact->numerator), exact->denominator});
        if (name == QLatin1String("ln")) {
            if (argument == m_one)
                return m_zero;
            if (argument == reference(QStringLiteral("e")))
                return m_one;
        }
        if (name == QLatin1String("exp")) {
            if (argument == m_zero)
                return m_one;
            if (argument == m_one)
                return reference(QStringLiteral("e"));
        }
        if ((name == QLatin1String("sqrt") || name == QLatin1String("cbrt"))
            && (argument == m_zero || argument == m_one)) {
            return argument;
        }
        if ((name == QLatin1String("erf") || name == QLatin1String("erfi")) && argument == m_zero)
            return m_zero;
        // The error functions erf and erfi are odd.
        if ((name == QLatin1String("erf") || name == QLatin1String("erfi")) && negative(argument))
            return negate(function(name, negate(argument)));
        if (name == QLatin1String("erfc") && argument == m_zero)
            return m_one;
        if (name == QLatin1String("arctan") && exact) {
            if (argument == m_zero)
                return m_zero;
            if (exact->denominator == 1 && abs(exact->numerator) == 1) {
                const Expr quarterTurn = multiply({number(1, 4), reference(QStringLiteral("π"))});
                return divide(exact->numerator.isNegative() ? negate(quarterTurn) : quarterTurn,
                              radiansPerAngle());
            }
        }
        if (name == QLatin1String("sin") || name == QLatin1String("cos")) {
            const Expr radians = multiply({argument, radiansPerAngle()});
            if (radians == m_zero)
                return name == QLatin1String("sin") ? m_zero : m_one;
            const Expr pi = reference(QStringLiteral("π"));
            const Key& angle = node(radians).key;
            if (radians == pi || (angle.type == Opcode::Mul
                && angle.children.size() == 2 && angle.children.at(1) == pi
                && node(angle.children.at(0)).number)) {
                const ExactNumber multiplier = radians == pi
                    ? ExactNumber{1, 1} : *node(angle.children.at(0)).number;
                const Integer turns = multiplier.numerator * 2;
                if ((turns % multiplier.denominator).isZero()) {
                    const int quadrant = (((turns / multiplier.denominator) % 4).toInt() + 4) % 4;
                    const int sine[] = {0, 1, 0, -1};
                    const int cosine[] = {1, 0, -1, 0};
                    return number(name == QLatin1String("sin") ? sine[quadrant] : cosine[quadrant]);
                }
            }
        }
        return make(Opcode::Function, {argument}, name);
    }

    Expr simplify(Expr expression)
    {
        spend();
        const Key& key = node(expression).key;
        if (key.children.isEmpty())
            return expression;
        QVector<Expr> children;
        children.reserve(key.children.size());
        for (Expr child : key.children)
            children.append(simplify(child));
        return rebuild(key, children);
    }

    Expr rebuild(const Key& key, const QVector<Expr>& children)
    {
        const Expr first = children.at(0);
        const Expr second = children.value(1, m_zero);
        switch (key.type) {
        case Opcode::Neg: return negate(first);
        case Opcode::Add: return add(children);
        case Opcode::Mul: return multiply(children);
        case Opcode::Sub: return subtract(first, second);
        case Opcode::Unit: return multiply({first, second});
        case Opcode::Div: return divide(first, second);
        case Opcode::Pow: return power(first, second);
        case Opcode::Function:
            if (m_readingProportion && key.text == QLatin1String("sqrt")
                && node(first).number && node(first).number->numerator.isPositive()) {
                return squareRoot(first).first;
            }
            if (key.text == QLatin1String("log"))
                return divide(function(QStringLiteral("ln"), second), function(QStringLiteral("ln"), first));
            if (children.size() == 1)
                return function(key.text, first);
            return make(key.type, children, key.text);
        default: return make(key.type, children, key.text);
        }
    }

    // Multiplies out products and small powers of sums, such as 3*(a-2*b)
    // and (1+a)^2, so that like terms can combine.
    Expr distribute(Expr expression)
    {
        spend();
        const Key key = node(expression).key;
        if (key.type == Opcode::Add) {
            QVector<Expr> terms;
            for (Expr child : key.children)
                terms.append(distribute(child));
            return add(terms);
        }
        QVector<Expr> factors = key.children;
        if (key.type == Opcode::Pow) {
            const auto& exponent = node(key.children.at(1)).number;
            if (!exponent || exponent->denominator != 1 || exponent->numerator < 2
                || exponent->numerator > 8 || node(key.children.at(0)).key.type != Opcode::Add) {
                return expression;
            }
            factors = QVector<Expr>(exponent->numerator.toInt(), key.children.at(0));
        } else if (key.type != Opcode::Mul) {
            return expression;
        }
        QVector<Expr> sum{m_one};
        for (Expr child : factors) {
            const Expr factor = distribute(child);
            const Key& part = node(factor).key;
            const QVector<Expr> terms = part.type == Opcode::Add ? part.children : QVector<Expr>{factor};
            QVector<Expr> product;
            for (Expr left : sum) {
                for (Expr right : terms)
                    product.append(multiply({left, right}));
            }
            sum = product;
        }
        return add(sum);
    }

    // Builds an optional, simpler form with part of the remaining work. When
    // that work runs out, the caller keeps the form it has. A time limit still
    // stops the whole calculation.
    template <typename Build>
    Expr attempt(int work, Build build)
    {
        const int budget = qMin(m_remainingWork / 2, work);
        const int reserve = m_remainingWork - budget;
        m_remainingWork = budget;
        Expr result = NoExpression;
        try {
            result = build();
        } catch (const Failure&) {
        }
        m_remainingWork += reserve;
        spend(0);
        return result;
    }

    int cost(Expr expression) const { return node(expression).cost; }

    QVector<Expr> termsOf(Expr expression) const
    {
        const Key& key = node(expression).key;
        return key.type == Opcode::Add ? key.children : QVector<Expr>{expression};
    }

    // Multiplies out an expression. A polynomial in the variable becomes a
    // sum of powers in rising order.
    Expr expansion(Expr expression, const QString& variable)
    {
        if (dependsOn(expression, variable)) {
            if (const auto coefficients = polynomial(expression, variable)) {
                const Expr x = reference(variable);
                QVector<Expr> terms;
                for (int i = 0; i < coefficients->size(); ++i)
                    terms.append(multiply({distribute(coefficients->at(i)), power(x, number(i))}));
                return add(terms);
            }
        }
        return distribute(expression);
    }

    // Multiplies out an expression between two steps of a derivative when
    // that makes it smaller, so that terms such as -2*(1+x^2)+8*x^2 combine.
    // This counts parts and not digits: large expanded coefficients are fine
    // here, and the passes at the end choose the form that is shown.
    Expr expanded(Expr expression, const QString& variable)
    {
        const Expr candidate = attempt(CandidateWork, [&] { return expansion(expression, variable); });
        return candidate != NoExpression && node(candidate).size < node(expression).size
            ? candidate : expression;
    }

    // Takes the greatest common factor out of a sum, as in 24*x-24*x^3 =
    // 24*x*(1-x^2): the greatest common divisor of the coefficients and the
    // lowest power of each shared factor. Its sign follows the first term, so
    // the sum inside starts with a positive term. A sum with a fraction among
    // its terms is left as it is.
    Expr factorOut(Expr sum)
    {
        if (sum == NoExpression || node(sum).key.type != Opcode::Add)
            return NoExpression;
        const QVector<Expr> terms = node(sum).key.children;
        QVector<QPair<ExactNumber, QMap<Expr, int>>> parts;
        Integer numerators = 0;
        Integer denominators = 1;
        std::optional<QMap<Expr, int>> shared;
        for (Expr term : terms) {
            if (node(term).key.type == Opcode::Div)
                return NoExpression;
            const auto [coefficient, rest] = coefficientOf(term);
            const QMap<Expr, int> powers = rest == m_one ? QMap<Expr, int>{} : factorPowers(rest);
            numerators = gcd(numerators, coefficient.numerator);
            denominators = lcm(denominators, coefficient.denominator);
            if (!shared) {
                shared = powers;
            } else {
                for (auto it = shared->begin(); it != shared->end();) {
                    const int exponent = qMin(it.value(), powers.value(it.key()));
                    if (exponent > 0) {
                        it.value() = exponent;
                        ++it;
                    } else {
                        it = shared->erase(it);
                    }
                }
            }
            parts.append({coefficient, powers});
        }
        ExactNumber content = exact(numerators, denominators);
        if (parts.first().first.numerator.isNegative())
            content.numerator = -content.numerator;
        if (content.numerator == 1 && content.denominator == 1 && shared->isEmpty())
            return NoExpression;

        QVector<Expr> inner;
        for (const auto& [coefficient, powers] : parts) {
            QVector<Expr> factors{number(exact(coefficient.numerator * content.denominator,
                                               coefficient.denominator * content.numerator))};
            for (auto it = powers.cbegin(); it != powers.cend(); ++it) {
                const int exponent = it.value() - shared->value(it.key());
                if (exponent > 0)
                    factors.append(power(it.key(), number(exponent)));
            }
            inner.append(multiply(factors));
        }
        QVector<Expr> outer{number(content)};
        for (auto it = shared->cbegin(); it != shared->cend(); ++it)
            outer.append(power(it.key(), number(it.value())));
        outer.append(add(inner));
        return multiply(outer);
    }

    // Writes terms over one denominator, as in 1/x+1/x^2 = (x+1)/x^2. Powers
    // of the same base share the highest one, and u counts as sqrt(u)^2 when
    // sqrt(u) is also below a bar.
    Expr overCommonDenominator(const QVector<Expr>& terms)
    {
        struct Part {
            Expr numerator;
            Integer scale;
            QMap<Expr, int> powers;
        };
        QVector<Part> parts;
        bool fractions = false;
        Integer scale = 1;
        for (Expr term : terms) {
            Expr numerator = term;
            Expr denominator = m_one;
            if (node(term).key.type == Opcode::Div) {
                numerator = node(term).key.children.at(0);
                denominator = node(term).key.children.at(1);
                fractions = true;
            }
            const auto [below, rest] = coefficientOf(denominator);
            if (below.denominator != 1 || !below.numerator.isPositive())
                return NoExpression;
            parts.append({numerator, below.numerator,
                          rest == m_one ? QMap<Expr, int>{} : factorPowers(rest)});
            scale = lcm(scale, below.numerator);
        }
        if (!fractions)
            return NoExpression;

        // Every sqrt(u) below a bar, so that u becomes sqrt(u)^2 in all terms.
        QMap<Expr, int> roots;
        for (const Part& part : parts) {
            for (auto it = part.powers.cbegin(); it != part.powers.cend(); ++it) {
                if (radicand(it.key()) != NoExpression)
                    roots.insert(it.key(), 0);
            }
        }
        QMap<Expr, int> common;
        for (Part& part : parts) {
            mergeRadicals(part.powers, roots);
            for (auto it = part.powers.cbegin(); it != part.powers.cend(); ++it)
                common[it.key()] = qMax(common.value(it.key()), it.value());
        }
        if (common.size() > MaxCommonFactors)
            return NoExpression;

        QVector<Expr> numerators;
        for (const Part& part : parts) {
            QVector<Expr> factors{part.numerator, number(ExactNumber{scale / part.scale, 1})};
            for (auto it = common.cbegin(); it != common.cend(); ++it) {
                const int missing = it.value() - part.powers.value(it.key());
                if (missing > 0)
                    factors.append(power(it.key(), number(missing)));
            }
            numerators.append(multiply(factors));
        }
        QVector<Expr> below{number(ExactNumber{scale, 1})};
        for (auto it = common.cbegin(); it != common.cend(); ++it)
            below.append(power(it.key(), number(it.value())));
        return divide(add(numerators), multiply(below));
    }

    // The bases below the bar of a term, with sqrt(u) counted as u.
    QSet<Expr> denominatorBases(Expr term)
    {
        QSet<Expr> bases;
        if (node(term).key.type != Opcode::Div)
            return bases;
        const auto [below, rest] = coefficientOf(node(term).key.children.at(1));
        if (rest == m_one)
            return bases;
        const QMap<Expr, int> powers = factorPowers(rest);
        for (auto it = powers.cbegin(); it != powers.cend(); ++it) {
            const Expr inner = radicand(it.key());
            bases.insert(inner != NoExpression ? inner : it.key());
        }
        return bases;
    }

    // Combines only the fractions whose denominators share a base, as in
    // cos(x)+1/x+1/x^2 = cos(x)+(x+1)/x^2, and keeps the other terms.
    Expr groupedFractions(const QVector<Expr>& terms, const QString& variable)
    {
        QVector<QVector<Expr>> groups;
        QVector<QSet<Expr>> groupBases;
        QVector<Expr> rest;
        for (Expr term : terms) {
            QSet<Expr> bases = denominatorBases(term);
            if (bases.isEmpty()) {
                rest.append(term);
                continue;
            }
            QVector<Expr> group{term};
            for (int i = groups.size() - 1; i >= 0; --i) {
                if (groupBases.at(i).intersects(bases)) {
                    group = groups.at(i) + group;
                    bases.unite(groupBases.at(i));
                    groups.removeAt(i);
                    groupBases.removeAt(i);
                }
            }
            groups.append(group);
            groupBases.append(bases);
        }
        bool combined = false;
        for (const QVector<Expr>& group : groups) {
            if (group.size() < 2) {
                rest += group;
                continue;
            }
            const Expr fraction = overCommonDenominator(group);
            if (fraction == NoExpression) {
                rest += group;
                continue;
            }
            rest.append(node(fraction).key.type == Opcode::Div ? refineFraction(fraction, variable) : fraction);
            combined = true;
        }
        return combined ? add(rest) : NoExpression;
    }

    // Inside a fraction whose denominator has sqrt(u), the fraction is only
    // defined where sqrt(u) is, so even powers of sqrt(u) above the bar can be
    // written as powers of u. Then they can be multiplied out.
    Expr withoutEvenRadicals(Expr numerator, Expr denominator)
    {
        QSet<Expr> roots;
        const QMap<Expr, int> below = factorPowers(denominator);
        for (auto it = below.cbegin(); it != below.cend(); ++it) {
            if (radicand(it.key()) != NoExpression)
                roots.insert(it.key());
        }
        if (roots.isEmpty())
            return numerator;
        QVector<Expr> terms;
        for (Expr term : termsOf(numerator)) {
            const auto [coefficient, rest] = coefficientOf(term);
            const QMap<Expr, int> powers = rest == m_one ? QMap<Expr, int>{} : factorPowers(rest);
            QVector<Expr> factors{number(coefficient)};
            for (auto it = powers.cbegin(); it != powers.cend(); ++it) {
                if (roots.contains(it.key()) && it.value() >= 2) {
                    factors.append(power(radicand(it.key()), number(it.value() / 2)));
                    if (it.value() % 2)
                        factors.append(it.key());
                } else {
                    factors.append(power(it.key(), number(it.value())));
                }
            }
            terms.append(multiply(factors));
        }
        return add(terms);
    }

    // The exact coefficients of a polynomial in the variable, lowest first.
    std::optional<QVector<ExactNumber>> exactPolynomial(Expr expression, const QString& variable)
    {
        const auto coefficients = polynomial(expression, variable);
        if (!coefficients)
            return std::nullopt;
        QVector<ExactNumber> values;
        for (Expr coefficient : *coefficients) {
            const auto& value = node(coefficient).number;
            if (!value)
                return std::nullopt;
            values.append(*value);
        }
        while (values.size() > 1 && values.last().numerator.isZero())
            values.removeLast();
        return values;
    }

    // The quotient when the divisor divides the polynomial exactly.
    std::optional<QVector<ExactNumber>> exactQuotient(QVector<ExactNumber> dividend,
                                                      const QVector<ExactNumber>& divisor)
    {
        const int degree = int(divisor.size()) - 1;
        if (degree < 1 || dividend.size() <= degree)
            return std::nullopt;
        const ExactNumber& lead = divisor.last();
        QVector<ExactNumber> quotient(dividend.size() - degree, ExactNumber{0, 1});
        for (int i = int(dividend.size()) - 1; i >= degree; --i) {
            spend();
            const ExactNumber& top = dividend.at(i);
            const ExactNumber factor = exact(top.numerator * lead.denominator, top.denominator * lead.numerator);
            quotient[i - degree] = factor;
            for (int j = 0; j <= degree; ++j) {
                ExactNumber& term = dividend[i - degree + j];
                const ExactNumber& d = divisor.at(j);
                const Integer numerator = factor.numerator * d.numerator;
                const Integer denominator = factor.denominator * d.denominator;
                term = exact(term.numerator * denominator - numerator * term.denominator,
                             term.denominator * denominator);
            }
        }
        for (int i = 0; i < degree; ++i) {
            if (!dividend.at(i).numerator.isZero())
                return std::nullopt;
        }
        return quotient;
    }

    // Cancels polynomial factors of the denominator that divide the
    // numerator, as in (x^3+x^2)/(x+1)^2 = x^2/(x+1). Only polynomials with
    // exact numeric coefficients take part. Like x/x = 1, this can remove a
    // point where the expression is undefined.
    Expr cancelPolynomialFactors(Expr numerator, Expr denominator, const QString& variable)
    {
        auto above = exactPolynomial(numerator, variable);
        if (!above || above->size() < 2)
            return NoExpression;
        QMap<Expr, int> below = factorPowers(denominator);
        bool cancelled = false;
        for (auto it = below.begin(); it != below.end(); ++it) {
            if (node(it.key()).key.type != Opcode::Add)
                continue;
            const auto divisor = exactPolynomial(it.key(), variable);
            if (!divisor)
                continue;
            while (it.value() > 0) {
                const auto quotient = exactQuotient(*above, *divisor);
                if (!quotient)
                    break;
                *above = *quotient;
                --it.value();
                cancelled = true;
            }
        }
        if (!cancelled)
            return NoExpression;
        const Expr x = reference(variable);
        QVector<Expr> terms;
        for (int i = 0; i < above->size(); ++i)
            terms.append(multiply({number(above->at(i)), power(x, number(i))}));
        return divide(add(terms), productOfPowers(below));
    }

    // Simplifies the numerator of a fraction and cancels the factors that it
    // shares with the denominator.
    Expr refineFraction(Expr fraction, const QString& variable)
    {
        const QVector<Expr> parts = node(fraction).key.children;
        const Expr denominator = parts.at(1);
        Expr best = fraction;
        const auto consider = [&](Expr numerator) {
            if (numerator == NoExpression)
                return;
            const Expr candidate = attempt(CandidateWork, [&] { return divide(numerator, denominator); });
            if (candidate != NoExpression && cost(candidate) < cost(best))
                best = candidate;
        };
        const Expr numerator = attempt(CandidateWork, [&] {
            return withoutEvenRadicals(parts.at(0), denominator);
        });
        if (numerator == NoExpression)
            return best;
        const Expr expandedNumerator = attempt(CandidateWork, [&] { return expansion(numerator, variable); });
        consider(numerator);
        consider(expandedNumerator);
        consider(attempt(CandidateWork, [&] { return factorOut(numerator); }));
        consider(attempt(CandidateWork, [&] { return factorOut(expandedNumerator); }));
        const Expr cancelled = attempt(CandidateWork, [&] {
            return cancelPolynomialFactors(numerator, denominator, variable);
        });
        if (cancelled != NoExpression && cost(cancelled) < cost(best))
            best = cancelled;
        return best;
    }

    Expr refineSum(Expr sum, const QString& variable)
    {
        const QVector<Expr> terms = node(sum).key.children;
        Expr best = sum;
        const auto consider = [&](Expr candidate) {
            if (candidate != NoExpression && cost(candidate) < cost(best))
                best = candidate;
        };
        const Expr expandedSum = attempt(CandidateWork, [&] { return expansion(sum, variable); });
        consider(expandedSum);
        consider(attempt(CandidateWork, [&] { return factorOut(sum); }));
        consider(attempt(CandidateWork, [&] { return factorOut(expandedSum); }));
        consider(attempt(CandidateWork, [&] {
            const Expr fraction = overCommonDenominator(terms);
            return fraction != NoExpression && node(fraction).key.type == Opcode::Div
                ? refineFraction(fraction, variable) : fraction;
        }));
        consider(attempt(CandidateWork, [&] { return groupedFractions(terms, variable); }));
        return best;
    }

    // One pass from the leaves up. A part only changes when its new form has
    // a lower complexity.
    Expr refine(Expr expression, const QString& variable)
    {
        spend();
        const Key key = node(expression).key;
        if (key.children.isEmpty())
            return expression;
        QVector<Expr> children;
        bool changed = false;
        for (Expr child : key.children) {
            const Expr refined = refine(child, variable);
            changed = changed || refined != child;
            children.append(refined);
        }
        Expr current = expression;
        if (changed) {
            const Expr rebuilt = rebuild(key, children);
            if (cost(rebuilt) < cost(current))
                current = rebuilt;
        }
        switch (node(current).key.type) {
        case Opcode::Add: return refineSum(current, variable);
        case Opcode::Div: return refineFraction(current, variable);
        default: return current;
        }
    }

    // Repeats the passes until nothing changes. Every change lowers the
    // complexity, so expanding and factoring cannot undo each other, and the
    // number of passes and the work they use are limited too. Of two forms
    // with the same complexity, the current one stays. The passes use no
    // trigonometric identities and do not combine products of exponentials,
    // as in exp(x^2)*exp(-x^2). Only diff() uses them: they would factor
    // polynomial antiderivatives, which then read worse.
    Expr simplified(Expr expression, const QString& variable)
    {
        for (int pass = 0; pass < MaxPasses; ++pass) {
            const Expr next = attempt(PassWork, [&] { return refine(expression, variable); });
            if (next == NoExpression || next == expression)
                break;
            expression = next;
        }
        return expression;
    }

    Expr derivative(Expr expression, const QString& variable)
    {
        spend();
        const Key& key = node(expression).key;
        switch (key.type) {
        case Opcode::Load:
        case Opcode::UnitRef:
            return m_zero;
        case Opcode::Ref:
            return key.text == variable ? m_one : m_zero;
        case Opcode::Add: {
            QVector<Expr> terms;
            for (Expr child : key.children)
                terms.append(derivative(child, variable));
            return add(terms);
        }
        case Opcode::Mul: {
            QVector<Expr> terms;
            for (int i = 0; i < key.children.size(); ++i) {
                const Expr diff = derivative(key.children.at(i), variable);
                if (diff != m_zero) {
                    QVector<Expr> factors = key.children;
                    factors[i] = diff;
                    terms.append(multiply(factors));
                }
            }
            return add(terms);
        }
        case Opcode::Div: {
            const Expr u = key.children.at(0);
            const Expr v = key.children.at(1);
            const Expr du = derivative(u, variable);
            // With v = c*b1^n1*b2^n2..., write the derivative over v*b1*b2...
            // instead of v^2. Repeated steps then add one power to each base,
            // instead of doubling the denominator each time.
            const QMap<Expr, int> powers = factorPowers(v);
            QVector<Expr> bases;
            QVector<Expr> slopes;
            QVector<int> exponents;
            for (auto it = powers.constBegin(); it != powers.constEnd(); ++it) {
                const Expr slope = derivative(it.key(), variable);
                if (slope != m_zero) {
                    bases.append(it.key());
                    slopes.append(slope);
                    exponents.append(it.value());
                }
            }
            if (bases.isEmpty())
                return divide(du, v);
            QVector<Expr> terms;
            for (int i = 0; i < bases.size(); ++i) {
                QVector<Expr> factors{number(exponents.at(i)), slopes.at(i)};
                for (int j = 0; j < bases.size(); ++j) {
                    if (j != i)
                        factors.append(bases.at(j));
                }
                terms.append(multiply(factors));
            }
            // The numerator is du*radical - u*(sum of terms). Only the outer
            // sums of du and u are multiplied out, so that inner factors such
            // as (ln(x)+1) stay as they are and can combine.
            const Expr radical = multiply(bases);
            const Expr logSlope = add(terms);
            QVector<Expr> numeratorTerms;
            for (Expr term : termsOf(du))
                numeratorTerms.append(multiply({term, radical}));
            for (Expr term : termsOf(u)) {
                for (Expr slope : termsOf(logSlope))
                    numeratorTerms.append(negate(multiply({term, slope})));
            }
            const Expr numerator = add(numeratorTerms);
            const Expr denominator = multiply({v, radical});
            // Over a sum such as (1+x^2)^4, one fraction with the numerator
            // multiplied out is shortest. Over powers such as x^4, each term is
            // divided on its own, so that it can cancel and combine with like
            // terms, as in the derivatives of x^x.
            const bool sumBelow = std::any_of(bases.cbegin(), bases.cend(), [this](Expr base) {
                return node(base).key.type == Opcode::Add;
            });
            if (sumBelow)
                return divide(expanded(numerator, variable), denominator);
            QVector<Expr> parts;
            for (Expr term : termsOf(numerator))
                parts.append(divide(term, denominator));
            return add(parts);
        }
        case Opcode::Pow: {
            const Expr u = key.children.at(0);
            const Expr v = key.children.at(1);
            const Expr du = derivative(u, variable);
            const Expr dv = derivative(v, variable);
            if (du == m_zero && dv == m_zero)
                return m_zero;
            if (u == m_zero && dv != m_zero)
                return m_zero;
            if (dv == m_zero)
                return multiply({v, power(u, subtract(v, m_one)), du});
            if (node(expression).containsUnit)
                throw Failure{tr("variable exponents with units are unsupported in symbolic differentiation")};
            const Expr exponentTerm = multiply({dv, function(QStringLiteral("ln"), u)});
            const Expr baseTerm = du == m_zero ? m_zero : divide(multiply({v, du}), u);
            return multiply({expression, add({exponentTerm, baseTerm})});
        }
        case Opcode::Function:
            return derivativeOfFunction(expression, variable);
        default:
            throw Failure{tr("unsupported expression in symbolic differentiation")};
        }
    }

    enum class Rule {
        Exp, Ln, Lg, Lb, Log, Abs, Sqrt, Cbrt, Sin, Cos, Tan, Cot, Sec, Csc,
        Arcsin, Arccos, Arctan, Arctan2, Sinh, Cosh, Tanh, Arsinh, Arcosh, Artanh, Erf, Erfc, Erfi
    };

    struct FunctionRule {
        Rule rule;
        int arity = 1;
    };

    static const QHash<QString, FunctionRule>& functionRules()
    {
        static const QHash<QString, FunctionRule> rules{
            {QStringLiteral("exp"), {Rule::Exp}},
            {QStringLiteral("ln"), {Rule::Ln}},
            {QStringLiteral("lg"), {Rule::Lg}},
            {QStringLiteral("log10"), {Rule::Lg}},
            {QStringLiteral("lb"), {Rule::Lb}},
            {QStringLiteral("log2"), {Rule::Lb}},
            {QStringLiteral("log"), {Rule::Log, 2}},
            {QStringLiteral("abs"), {Rule::Abs}},
            {QStringLiteral("sqrt"), {Rule::Sqrt}},
            {QStringLiteral("cbrt"), {Rule::Cbrt}},
            {QStringLiteral("sin"), {Rule::Sin}},
            {QStringLiteral("cos"), {Rule::Cos}},
            {QStringLiteral("tan"), {Rule::Tan}},
            {QStringLiteral("cot"), {Rule::Cot}},
            {QStringLiteral("sec"), {Rule::Sec}},
            {QStringLiteral("csc"), {Rule::Csc}},
            {QStringLiteral("arcsin"), {Rule::Arcsin}},
            {QStringLiteral("arccos"), {Rule::Arccos}},
            {QStringLiteral("arctan"), {Rule::Arctan}},
            {QStringLiteral("arctan2"), {Rule::Arctan2, 2}},
            {QStringLiteral("sinh"), {Rule::Sinh}},
            {QStringLiteral("cosh"), {Rule::Cosh}},
            {QStringLiteral("tanh"), {Rule::Tanh}},
            {QStringLiteral("arsinh"), {Rule::Arsinh}},
            {QStringLiteral("arcosh"), {Rule::Arcosh}},
            {QStringLiteral("artanh"), {Rule::Artanh}},
            {QStringLiteral("erf"), {Rule::Erf}},
            {QStringLiteral("erfc"), {Rule::Erfc}},
            {QStringLiteral("erfi"), {Rule::Erfi}}
        };
        return rules;
    }

    Expr radiansPerAngle()
    {
        const Expr pi = reference(QStringLiteral("π"));
        switch (Settings::instance()->angleUnit) {
        case AngleMode::Degree: return multiply({number(1, 180), pi});
        case AngleMode::Gradian: return multiply({number(1, 200), pi});
        case AngleMode::Turn: case AngleMode::Revolution: return multiply({number(2), pi});
        default: return m_one;
        }
    }

    // u^2, where sqrt(v)^2 is v. Only for the derivatives and integrals of
    // the error functions, which keep a factor u' or u, so that sqrt(v) stays
    // in the result and the domain does not grow.
    Expr errorSquare(Expr u)
    {
        const Expr inner = radicand(u);
        return inner != NoExpression ? inner : power(u, number(2));
    }

    // exp(-u^2), whose integral is the error function.
    Expr gaussian(Expr u)
    {
        return function(QStringLiteral("exp"), negate(errorSquare(u)));
    }

    // The square root of a positive value and its inverse. For a fraction n/d
    // both keep a whole number under the root, as sqrt(n*d)/d and sqrt(n*d)/n,
    // so that equal roots can cancel.
    QPair<Expr, Expr> squareRoot(Expr value)
    {
        const auto& ratio = node(value).number;
        if (ratio && ratio->numerator.isPositive()) {
            Integer product = ratio->numerator * ratio->denominator;
            Integer outside = 1;
            const Integer whole = floorSqrt(product);
            if (whole * whole == product) {
                outside = whole;
                product = 1;
            }
            // Take square factors below 100 out of the root, as in sqrt(8) =
            // 2*sqrt(2). Larger ones stay inside, which is still exact.
            for (int factor = 2; factor < 100 && Integer(factor * factor) <= product; ++factor) {
                while ((product % (factor * factor)).isZero()) {
                    spend();
                    product = product / (factor * factor);
                    outside = outside * factor;
                }
            }
            const Expr root = multiply({number(exact(outside, 1)),
                                        function(QStringLiteral("sqrt"), number(exact(product, 1)))});
            return {divide(root, number(ExactNumber{ratio->denominator, 1})),
                    divide(root, number(ExactNumber{ratio->numerator, 1}))};
        }
        const Expr root = function(QStringLiteral("sqrt"), value);
        return {root, divide(m_one, root)};
    }

    // The coefficients of a polynomial, also over a constant denominator,
    // without leading zeros.
    std::optional<QVector<Expr>> scaledPolynomial(Expr expression, const QString& variable)
    {
        const Key& key = node(expression).key;
        const bool scaled = key.type == Opcode::Div && !dependsOn(key.children.at(1), variable);
        auto coefficients = polynomial(scaled ? key.children.at(0) : expression, variable);
        if (!coefficients)
            return std::nullopt;
        if (scaled) {
            for (Expr& coefficient : *coefficients)
                coefficient = divide(coefficient, key.children.at(1));
        }
        while (coefficients->size() > 1 && coefficients->constLast() == m_zero)
            coefficients->removeLast();
        return coefficients;
    }

    Expr derivativeOfFunction(Expr expression, const QString& variable)
    {
        const Key& key = node(expression).key;
        const auto rule = functionRules().constFind(key.text);
        const Expr u = key.children.at(0);
        const Key& argument = node(u).key;
        if (rule->rule == Rule::Ln && argument.type == Opcode::Function
            && argument.text == QLatin1String("abs")) {
            const Expr inner = argument.children.at(0);
            return divide(derivative(inner, variable), inner);
        }
        const Expr du = derivative(u, variable);
        const Expr v = rule->arity == 2 ? key.children.at(1) : m_zero;
        const Expr dv = rule->arity == 2 ? derivative(v, variable) : m_zero;
        if (du == m_zero && dv == m_zero)
            return m_zero;
        if (node(expression).containsUnit && rule->rule != Rule::Sqrt && rule->rule != Rule::Cbrt)
            throw Failure{tr("units in this function are unsupported in symbolic differentiation")};

        const auto unary = [&](const char* name) { return function(QLatin1String(name), u); };
        const auto square = [&](Expr x) { return power(x, number(2)); };
        const auto sqrt = [&](Expr x) { return function(QStringLiteral("sqrt"), x); };
        const auto forward = [&](Expr factor) { return multiply({du, radiansPerAngle(), factor}); };
        const auto inverse = [&](Expr numerator, Expr denominator) {
            return divide(numerator, multiply({radiansPerAngle(), denominator}));
        };

        switch (rule->rule) {
        case Rule::Exp: return multiply({du, expression});
        case Rule::Ln: return divide(du, u);
        case Rule::Abs:
            if (Settings::instance()->complexNumbers)
                throw Failure{tr("symbolic differentiation of abs needs real numbers")};
            return divide(multiply({u, du}), expression);
        case Rule::Lg:
        case Rule::Lb: {
            const Expr base = number(rule->rule == Rule::Lg ? 10 : 2);
            return divide(du, multiply({u, function(QStringLiteral("ln"), base)}));
        }
        case Rule::Sqrt: return divide(du, multiply({number(2), expression}));
        case Rule::Cbrt: return divide(du, multiply({number(3), square(expression)}));
        case Rule::Sin: return forward(unary("cos"));
        case Rule::Cos: return forward(negate(unary("sin")));
        case Rule::Tan: return forward(square(unary("sec")));
        case Rule::Cot: return forward(negate(square(unary("csc"))));
        case Rule::Sec: return forward(multiply({expression, unary("tan")}));
        case Rule::Csc: return forward(negate(multiply({expression, unary("cot")})));
        case Rule::Arcsin: return inverse(du, sqrt(subtract(m_one, square(u))));
        case Rule::Arccos: return inverse(negate(du), sqrt(subtract(m_one, square(u))));
        case Rule::Arctan: return inverse(du, add({m_one, square(u)}));
        case Rule::Arctan2:
            return inverse(subtract(multiply({u, dv}), multiply({v, du})),
                           add({square(u), square(v)}));
        case Rule::Sinh: return multiply({du, unary("cosh")});
        case Rule::Cosh: return multiply({du, unary("sinh")});
        case Rule::Tanh: return divide(du, square(unary("cosh")));
        case Rule::Arsinh: return divide(du, sqrt(add({m_one, square(u)})));
        case Rule::Arcosh:
            return divide(du, multiply({sqrt(subtract(u, m_one)), sqrt(add({u, m_one}))}));
        case Rule::Artanh: return divide(du, subtract(m_one, square(u)));
        case Rule::Erf: case Rule::Erfc: {
            const Expr slope = divide(multiply({number(2), du, gaussian(u)}),
                                      sqrt(reference(QStringLiteral("π"))));
            return rule->rule == Rule::Erf ? slope : negate(slope);
        }
        case Rule::Erfi:
            return divide(multiply({number(2), du, function(QStringLiteral("exp"), errorSquare(u))}),
                          sqrt(reference(QStringLiteral("π"))));
        case Rule::Log: break; // Simplified to a quotient of natural logarithms.
        }
        throw Failure{tr("unsupported function %1").arg(key.text.toHtmlEscaped())};
    }

    bool negative(Expr expression) const
    {
        const Node& value = node(expression);
        if (value.number)
            return value.number->numerator.isNegative();
        return (value.key.type == Opcode::Mul || value.key.type == Opcode::Div)
            && negative(value.key.children.at(0));
    }

    void render(Expr expression, int minPrecedence, QString& text)
    {
        spend();
        const Node& value = node(expression);
        const Key& key = value.key;
        int precedence = 5;
        if (key.type == Opcode::Add)
            precedence = 1;
        else if (key.type == Opcode::Div || key.type == Opcode::Mul
                 || (value.number && value.number->denominator != 1))
            precedence = 2;
        else if (negative(expression))
            precedence = 3;
        else if (key.type == Opcode::Pow)
            precedence = 4;

        const bool wrap = precedence < minPrecedence;
        if (wrap)
            text += QLatin1Char('(');
        const auto& children = key.children;
        switch (key.type) {
        case Opcode::Load: case Opcode::Ref:
            text += key.text;
            break;
        case Opcode::UnitRef:
            text += QLatin1Char('[') + key.text + QLatin1Char(']');
            break;
        case Opcode::Add:
            for (int i = 0; i < children.size(); ++i) {
                if (i > 0 && !negative(children.at(i)))
                    text += QLatin1Char('+');
                render(children.at(i), 1, text);
            }
            break;
        case Opcode::Mul: {
            const auto& coefficient = node(children.at(0)).number;
            // A fractional coefficient goes below a bar at the end, as in
            // x*(x+1)/2 instead of 1/2*x*(x+1).
            const bool fractional = coefficient && coefficient->denominator != 1;
            const bool unit = coefficient && abs(coefficient->numerator) == 1
                && (fractional || coefficient->numerator.isNegative());
            if (unit && coefficient->numerator.isNegative())
                text += QLatin1Char('-');
            if (fractional && !unit)
                text += coefficient->numerator.toString() + QLatin1Char('*');
            const int start = unit || fractional ? 1 : 0;
            for (int i = start; i < children.size(); ++i) {
                if (i > start)
                    text += QLatin1Char('*');
                render(children.at(i), i > start && negative(children.at(i)) ? 4 : 2, text);
            }
            if (fractional)
                text += QLatin1Char('/') + coefficient->denominator.toString();
            break;
        }
        case Opcode::Div:
            render(children.at(0), 2, text);
            text += QLatin1Char('/');
            render(children.at(1), 3, text);
            break;
        case Opcode::Pow:
            render(children.at(0), 5, text);
            text += QLatin1Char('^');
            render(children.at(1), 4, text);
            break;
        case Opcode::Function:
            text += key.text + QLatin1Char('(');
            for (int i = 0; i < children.size(); ++i) {
                if (i > 0)
                    text += QLatin1Char(';');
                render(children.at(i), 0, text);
            }
            text += QLatin1Char(')');
            break;
        default:
            throw Failure{tr("unsupported expression in symbolic differentiation")};
        }
        if (wrap)
            text += QLatin1Char(')');
        if (text.size() > SymbolicCalculus::MaxSourceLength)
            limitExceeded();
    }
};

bool Calculus::dependsOn(Expr expression, const QString& variable)
{
    // Expressions never change, so the answer is kept for each variable.
    if (variable != m_dependsVariable) {
        m_depends.clear();
        m_dependsVariable = variable;
    }
    const auto known = m_depends.constFind(expression);
    if (known != m_depends.constEnd())
        return known.value();
    spend();
    const Key& key = node(expression).key;
    bool result = key.type == Opcode::Ref && key.text == variable;
    for (int i = 0; !result && i < key.children.size(); ++i)
        result = dependsOn(key.children.at(i), variable);
    m_depends.insert(expression, result);
    return result;
}

Expr Calculus::substitute(Expr expression, Expr variable, Expr value, bool simplifyResult)
{
    spend();
    if (expression == variable)
        return value;
    const Key& key = node(expression).key;
    if (key.children.isEmpty())
        return expression;
    QVector<Expr> children;
    children.reserve(key.children.size());
    for (Expr child : key.children)
        children.append(substitute(child, variable, value, simplifyResult));
    return simplifyResult ? rebuild(key, children) : make(key.type, children, key.text);
}

std::optional<Calculus::PowerKernel> Calculus::powerKernel(Expr expression)
{
    const Key& key = node(expression).key;
    if (key.type == Opcode::Pow) {
        const Expr exponent = simplify(key.children.at(1));
        if (node(exponent).number) {
            // sqrt(u)^n is u^(n/2).
            const Expr inner = radicand(key.children.at(0));
            if (inner != NoExpression)
                return PowerKernel{inner, divide(exponent, number(2))};
            return PowerKernel{key.children.at(0), exponent};
        }
    }
    if (key.type == Opcode::Function && key.text == QLatin1String("sqrt"))
        return PowerKernel{key.children.at(0), number(1, 2)};
    if (key.type == Opcode::Function && key.text == QLatin1String("cbrt"))
        return PowerKernel{key.children.at(0), number(1, 3)};
    if (key.type == Opcode::Div && key.children.at(0) == m_one) {
        const Expr denominator = key.children.at(1);
        const auto power = powerKernel(denominator);
        return power ? PowerKernel{power->base, negate(power->exponent)}
                     : PowerKernel{denominator, number(-1)};
    }
    return std::nullopt;
}

std::optional<Calculus::PowerKernel> Calculus::scaledPower(Expr expression, Expr& coefficient)
{
    const Key& key = node(expression).key;
    if (key.type == Opcode::Div) {
        coefficient = key.children.at(0);
        return powerKernel(make(Opcode::Div, {m_one, key.children.at(1)}));
    }
    if (key.type == Opcode::Mul) {
        std::optional<PowerKernel> result;
        QVector<Expr> rest;
        for (Expr factor : key.children) {
            Expr scale = m_one;
            const auto power = scaledPower(factor, scale);
            if (power && node(power->exponent).number->numerator.isNegative()) {
                if (result)
                    return std::nullopt;
                result = power;
                rest.append(scale);
            } else {
                rest.append(factor);
            }
        }
        coefficient = make(Opcode::Mul, rest);
        return result;
    }
    coefficient = m_one;
    return powerKernel(expression);
}

std::optional<Calculus::SquareOffset> Calculus::squareWithPositiveOffset(Expr expression)
{
    const Key& sum = node(expression).key;
    if (sum.type != Opcode::Add || sum.children.size() != 2)
        return std::nullopt;

    for (int i = 0; i < 2; ++i) {
        const Expr offset = sum.children.at(i);
        const auto& exact = node(offset).number;
        if (!exact || !exact->numerator.isPositive())
            continue;
        const auto square = powerKernel(sum.children.at(1 - i));
        if (square && square->exponent == number(2))
            return SquareOffset{square->base, offset};
    }
    return std::nullopt;
}

std::optional<QVector<Expr>> Calculus::polynomial(Expr expression, const QString& variable)
{
    // Higher degrees are not supported. This also limits the integrals of a
    // polynomial times an exponential or an error function.
    constexpr int maxDegree = 24;
    spend();
    if (!dependsOn(expression, variable))
        return QVector<Expr>{expression};

    const Key& key = node(expression).key;
    if (key.type == Opcode::Ref)
        return QVector<Expr>{m_zero, m_one};

    // Numeric coefficients use exact arithmetic directly, which is much
    // cheaper than building an expression for every partial sum.
    const auto plus = [&](Expr a, Expr b) {
        const auto& left = node(a).number;
        const auto& right = node(b).number;
        if (!left || !right)
            return add({a, b});
        spend();
        return number(exact(left->numerator * right->denominator + right->numerator * left->denominator,
                            left->denominator * right->denominator));
    };
    const auto times = [&](Expr a, Expr b) {
        const auto& left = node(a).number;
        const auto& right = node(b).number;
        if (!left || !right)
            return multiply({a, b});
        spend();
        return number(exact(left->numerator * right->numerator, left->denominator * right->denominator));
    };
    const auto product = [&](const QVector<Expr>& left, const QVector<Expr>& right) {
        const int count = left.size() + right.size() - 1;
        if (count > maxDegree + 1)
            return QVector<Expr>{};
        QVector<Expr> coefficients(count, m_zero);
        for (int i = 0; i < left.size(); ++i) {
            for (int j = 0; j < right.size(); ++j)
                coefficients[i + j] = plus(coefficients.at(i + j), times(left.at(i), right.at(j)));
        }
        return coefficients;
    };
    if (key.type == Opcode::Add || key.type == Opcode::Mul) {
        QVector<Expr> coefficients{key.type == Opcode::Add ? m_zero : m_one};
        for (Expr child : key.children) {
            const auto part = polynomial(child, variable);
            if (!part)
                return std::nullopt;
            if (key.type == Opcode::Add) {
                if (coefficients.size() < part->size())
                    coefficients += QVector<Expr>(part->size() - coefficients.size(), m_zero);
                for (int i = 0; i < part->size(); ++i)
                    coefficients[i] = plus(coefficients.at(i), part->at(i));
            } else {
                coefficients = product(coefficients, *part);
                if (coefficients.isEmpty())
                    return std::nullopt;
            }
        }
        return coefficients;
    }
    if (key.type == Opcode::Pow) {
        const auto& exponent = node(key.children.at(1)).number;
        if (!exponent || exponent->denominator != 1 || exponent->numerator < 1
            || exponent->numerator > maxDegree) {
            return std::nullopt;
        }
        const auto base = polynomial(key.children.at(0), variable);
        if (!base)
            return std::nullopt;
        const int degree = exponent->numerator.toInt();
        // A single term c*x^k gives c^n*x^(k*n) directly.
        const auto nonzero = std::count_if(base->cbegin(), base->cend(), [this](Expr c) { return c != m_zero; });
        if (nonzero == 1) {
            const int index = int(std::find_if(base->cbegin(), base->cend(),
                                               [this](Expr c) { return c != m_zero; }) - base->cbegin());
            if (index * degree > maxDegree)
                return std::nullopt;
            QVector<Expr> coefficients(index * degree + 1, m_zero);
            coefficients[index * degree] = power(base->at(index), number(degree));
            return coefficients;
        }
        QVector<Expr> coefficients{m_one};
        for (int i = 0; i < degree; ++i) {
            coefficients = product(coefficients, *base);
            if (coefficients.isEmpty())
                return std::nullopt;
        }
        return coefficients;
    }
    return std::nullopt;
}

Expr Calculus::integratePolynomial(const QVector<Expr>& coefficients, Expr variable)
{
    QVector<Expr> terms;
    for (int i = 0; i < coefficients.size(); ++i) {
        if (coefficients.at(i) != m_zero) {
            const Expr exponent = number(i + 1);
            terms.append(multiply({divide(coefficients.at(i), exponent), power(variable, exponent)}));
        }
    }
    return add(terms);
}

Expr Calculus::powerPrimitive(const PowerKernel& kernel)
{
    const Expr exponent = add({kernel.exponent, m_one});
    if (exponent == m_zero)
        return function(QStringLiteral("ln"), function(QStringLiteral("abs"), kernel.base));
    return divide(power(kernel.base, exponent), exponent);
}

Expr Calculus::kernelPrimitive(Expr expression, Expr& argument)
{
    const Key& key = node(expression).key;
    if (key.type == Opcode::Ref) {
        argument = expression;
        return powerPrimitive({argument, m_one});
    }
    if (const auto kernel = powerKernel(expression)) {
        argument = kernel->base;
        if (kernel->exponent == number(-1)) {
            if (const auto square = squareWithPositiveOffset(argument)) {
                argument = square->argument;
                const Expr scale = function(QStringLiteral("sqrt"), square->offset);
                return divide(multiply({radiansPerAngle(),
                                        function(QStringLiteral("arctan"), divide(argument, scale))}),
                              scale);
            }
        }
        return powerPrimitive(*kernel);
    }
    if (key.type == Opcode::Pow) {
        const Expr base = key.children.at(0);
        const HNumber value = magnitude(base);
        if (!value.isNan() && value > 0 && value != 1) {
            argument = key.children.at(1);
            return divide(expression, function(QStringLiteral("ln"), base));
        }
        return NoExpression;
    }
    if (key.type != Opcode::Function || key.children.size() != 1)
        return NoExpression;
    argument = key.children.at(0);
    if (node(argument).containsUnit)
        return NoExpression;

    const auto unary = [&](const char* name) { return function(QLatin1String(name), argument); };
    const auto logAbs = [&](const char* name) {
        return function(QStringLiteral("ln"), function(QStringLiteral("abs"), unary(name)));
    };
    const auto rule = functionRules().constFind(key.text);
    switch (rule->rule) {
    case Rule::Exp: return expression;
    case Rule::Ln: return subtract(multiply({argument, expression}), argument);
    case Rule::Lg: case Rule::Lb: {
        const Expr base = number(rule->rule == Rule::Lg ? 10 : 2);
        return subtract(multiply({argument, expression}),
                        divide(argument, function(QStringLiteral("ln"), base)));
    }
    case Rule::Sin: return divide(negate(unary("cos")), radiansPerAngle());
    case Rule::Cos: return divide(unary("sin"), radiansPerAngle());
    case Rule::Tan: return divide(negate(logAbs("cos")), radiansPerAngle());
    case Rule::Cot: return divide(logAbs("sin"), radiansPerAngle());
    case Rule::Sec: case Rule::Csc: {
        const Expr term = rule->rule == Rule::Sec ? add({expression, unary("tan")})
                                                : subtract(expression, unary("cot"));
        return divide(function(QStringLiteral("ln"), function(QStringLiteral("abs"), term)),
                      radiansPerAngle());
    }
    case Rule::Sinh: return unary("cosh");
    case Rule::Cosh: return unary("sinh");
    case Rule::Tanh: return function(QStringLiteral("ln"), unary("cosh"));
    case Rule::Abs: return divide(multiply({argument, expression}), number(2));
    case Rule::Erf: case Rule::Erfc: case Rule::Erfi: {
        const Expr pi = reference(QStringLiteral("π"));
        const Expr bump = rule->rule == Rule::Erfi
            ? divide(function(QStringLiteral("exp"), errorSquare(argument)), function(QStringLiteral("sqrt"), pi))
            : divide(gaussian(argument), function(QStringLiteral("sqrt"), pi));
        const Expr term = multiply({argument, expression});
        return rule->rule == Rule::Erf ? add({term, bump}) : subtract(term, bump);
    }
    default: return NoExpression;
    }
}

// The factors that are exponentials, such as exp(u) and 2^u, go to kernels and
// the others to rest. Returns the polynomial in the variable that is the sum of
// their natural exponents, or nothing if one of them is not a polynomial or has
// units.
std::optional<QVector<Expr>> Calculus::exponentPolynomial(const QVector<Expr>& factors, const QString& variable,
                                                          QVector<Expr>& kernels, QVector<Expr>& rest)
{
    QVector<Expr> sum{m_zero};
    for (Expr factor : factors) {
        const Key& key = node(factor).key;
        Expr exponent = NoExpression;
        if (key.type == Opcode::Function && key.text == QLatin1String("exp")
            && dependsOn(key.children.at(0), variable)) {
            exponent = key.children.at(0);
        } else if (key.type == Opcode::Pow && !dependsOn(key.children.at(0), variable)
                   && dependsOn(key.children.at(1), variable)) {
            const HNumber base = magnitude(key.children.at(0));
            if (base.isNan() || base <= 0 || base == 1)
                return std::nullopt;
            exponent = multiply({key.children.at(1), function(QStringLiteral("ln"), key.children.at(0))});
        }
        if (exponent == NoExpression) {
            rest.append(factor);
            continue;
        }
        if (node(factor).containsUnit)
            return std::nullopt;
        const auto part = scaledPolynomial(exponent, variable);
        if (!part)
            return std::nullopt;
        if (sum.size() < part->size())
            sum.resize(part->size(), m_zero);
        for (int i = 0; i < part->size(); ++i)
            sum[i] = add({sum.at(i), part->at(i)});
        kernels.append(factor);
    }
    while (sum.size() > 1 && sum.constLast() == m_zero)
        sum.removeLast();
    return sum;
}

// The integral of p(x)*exp(q(x)), where p is a polynomial and q is a quadratic.
// Dividing p by q' leaves p = q'*r + k, and q'*r*exp(q) integrates by parts to
// r*exp(q) minus the integral of r'*exp(q). So each step lowers the degree of p
// by two, down to a constant times erf if q opens downwards, or erfi if it opens
// upwards. The sign of q2 must be known, so a variable in it is not supported.
// When the exponents cancel, as in exp(x^2)*exp(-x^2), q is not a quadratic and
// this rule does not apply.
Expr Calculus::gaussianPrimitive(const QVector<Expr>& factors, const QString& variable)
{
    QVector<Expr> kernels;
    QVector<Expr> rest;
    const auto q = exponentPolynomial(factors, variable, kernels, rest);
    if (!q || q->size() != 3)
        return NoExpression;
    const HNumber curvature = magnitude(q->at(2));
    if (curvature.isNan() || curvature.isZero())
        return NoExpression;
    const bool opensDown = curvature.isNegative();
    auto p = scaledPolynomial(multiply(rest), variable);
    if (!p)
        return NoExpression;

    const Expr x = reference(variable);
    const auto polynomialOf = [&](const QVector<Expr>& coefficients) {
        QVector<Expr> terms;
        for (int i = 0; i < coefficients.size(); ++i)
            terms.append(multiply({coefficients.at(i), power(x, number(i))}));
        return add(terms);
    };
    const Expr slope = multiply({number(2), q->at(2)});
    QVector<Expr> parts;
    while (p->size() > 1) {
        spend(p->size());
        QVector<Expr> remainder = *p;
        QVector<Expr> quotient(p->size() - 1, m_zero);
        for (int i = remainder.size() - 1; i > 0; --i) {
            quotient[i - 1] = divide(remainder.at(i), slope);
            remainder[i - 1] = subtract(remainder.at(i - 1), multiply({quotient.at(i - 1), q->at(1)}));
        }
        if (parts.size() < quotient.size())
            parts.resize(quotient.size(), m_zero);
        for (int i = 0; i < quotient.size(); ++i)
            parts[i] = add({parts.at(i), quotient.at(i)});
        QVector<Expr> next(qMax(1, int(quotient.size()) - 1), m_zero);
        next[0] = remainder.at(0);
        for (int i = 1; i < quotient.size(); ++i)
            next[i - 1] = subtract(next.at(i - 1), multiply({number(i), quotient.at(i)}));
        while (next.size() > 1 && next.constLast() == m_zero)
            next.removeLast();
        *p = next;
    }

    // If q opens downwards, q = c - u^2 with s = sqrt(-q2) and u = s*x - q1/(2*s).
    // If it opens upwards, q = c + u^2 with s = sqrt(q2) and u = s*x + q1/(2*s).
    // In both cases c = q0 - q1^2/(4*q2).
    QVector<Expr> terms{multiply({polynomialOf(parts), multiply(kernels)})};
    if (p->at(0) != m_zero) {
        const auto [root, inverse] = squareRoot(opensDown ? negate(q->at(2)) : q->at(2));
        const Expr shift = multiply({q->at(1), inverse, number(1, 2)});
        const Expr argument = opensDown ? subtract(multiply({root, x}), shift) : add({multiply({root, x}), shift});
        const Expr peak = function(QStringLiteral("exp"),
                                   subtract(q->at(0), divide(power(q->at(1), number(2)),
                                                             multiply({number(4), q->at(2)}))));
        terms.append(multiply({p->at(0), peak, function(QStringLiteral("sqrt"), reference(QStringLiteral("π"))),
                               inverse, number(1, 2),
                               function(QLatin1String(opensDown ? "erf" : "erfi"), argument)}));
    }
    return add(terms);
}

// The sign of a polynomial of degree one or more at direction*inf, or 0.
int Calculus::signAtInfinity(const QVector<Expr>& coefficients, int direction)
{
    const HNumber leading = magnitude(coefficients.constLast());
    if (coefficients.size() < 2 || leading.isNan() || leading.isZero())
        return 0;
    const bool odd = coefficients.size() % 2 == 0;
    return leading.isNegative() != (odd && direction < 0) ? -1 : 1;
}

// The limit as the variable goes to direction*inf of a sum of products of
// constants, polynomials, exponentials of polynomials and error functions of
// polynomials. Such a sum is continuous everywhere. Returns NoExpression for
// other forms and when the limit is not finite. Limits that need terms or
// factors to cancel, as in x*(erf(x)-1), are also NoExpression.
Expr Calculus::limitAtInfinity(Expr expression, const QString& variable, int direction)
{
    spend();
    if (!dependsOn(expression, variable))
        return expression;
    const Key& key = node(expression).key;
    if (key.type == Opcode::Add) {
        QVector<Expr> terms;
        for (Expr child : key.children) {
            const Expr limit = limitAtInfinity(child, variable, direction);
            if (limit == NoExpression)
                return NoExpression;
            terms.append(limit);
        }
        return add(terms);
    }
    if (key.type == Opcode::Div) {
        const Expr denominator = key.children.at(1);
        if (dependsOn(denominator, variable))
            return NoExpression;
        const Expr limit = limitAtInfinity(key.children.at(0), variable, direction);
        return limit == NoExpression ? NoExpression : divide(limit, denominator);
    }

    const auto sign = [&](const QVector<Expr>& coefficients) { return signAtInfinity(coefficients, direction); };
    // Where u grows, erfc(u) is below exp(-u^2), and erfi(u) is always below
    // exp(u^2) in size. That is enough to show that a product goes to zero, but
    // for no other limit, and the check below allows no other.
    QVector<Expr> factors = key.type == Opcode::Mul ? key.children : QVector<Expr>{expression};
    for (Expr& factor : factors) {
        const Key& part = node(factor).key;
        const bool complement = part.type == Opcode::Function && part.text == QLatin1String("erfc");
        if (!complement && (part.type != Opcode::Function || part.text != QLatin1String("erfi")))
            continue;
        const auto argument = scaledPolynomial(part.children.at(0), variable);
        const int end = argument ? sign(*argument) : 0;
        if (!end)
            return NoExpression;
        if (complement && end < 0)
            continue;
        const Expr square = power(part.children.at(0), number(2));
        factor = function(QStringLiteral("exp"), complement ? negate(square) : square);
    }
    QVector<Expr> kernels;
    QVector<Expr> rest;
    const auto exponent = exponentPolynomial(factors, variable, kernels, rest);
    if (!exponent)
        return NoExpression;
    // An exponential that falls wins over any polynomial.
    const int growth = kernels.isEmpty() ? 0 : sign(*exponent);
    if (!kernels.isEmpty() && growth >= 0)
        return NoExpression;

    QVector<Expr> limits;
    bool grows = false;
    for (Expr factor : rest) {
        if (!dependsOn(factor, variable)) {
            limits.append(factor);
            continue;
        }
        const Key& part = node(factor).key;
        const auto coefficients = scaledPolynomial(factor, variable);
        if (coefficients) {
            if (coefficients->size() > 1)
                grows = true;
            else
                limits.append(coefficients->constFirst());
        } else if (part.type == Opcode::Function
                   && (part.text == QLatin1String("erf") || part.text == QLatin1String("erfc"))) {
            const auto argument = scaledPolynomial(part.children.at(0), variable);
            const int end = argument ? sign(*argument) : 0;
            if (!end)
                return NoExpression;
            limits.append(number(part.text == QLatin1String("erf") ? end : 1 - end));
        } else if (part.type == Opcode::Add) {
            const Expr limit = limitAtInfinity(factor, variable, direction);
            if (limit == NoExpression)
                return NoExpression;
            limits.append(limit);
        } else {
            return NoExpression;
        }
    }
    if (growth < 0)
        return m_zero;
    return grows ? NoExpression : multiply(limits);
}

Expr Calculus::antiderivative(Expr expression, const QString& variable, int depth)
{
    spend();
    if (depth > 12)
        return NoExpression;
    const Expr x = reference(variable);
    if (!dependsOn(expression, variable))
        return multiply({expression, x});

    const Key& key = node(expression).key;
    if (key.type == Opcode::Add) {
        QVector<Expr> terms;
        for (Expr child : key.children) {
            const Expr term = antiderivative(child, variable, depth + 1);
            if (term == NoExpression)
                return NoExpression;
            terms.append(term);
        }
        return add(terms);
    }
    if (key.type == Opcode::Div) {
        const Expr numerator = key.children.at(0);
        const Expr denominator = key.children.at(1);
        if (!dependsOn(denominator, variable)) {
            const Expr result = antiderivative(numerator, variable, depth + 1);
            return result == NoExpression ? NoExpression : divide(result, denominator);
        }
        if (node(numerator).key.type == Opcode::Add) {
            QVector<Expr> terms;
            for (Expr term : node(numerator).key.children)
                terms.append(divide(term, denominator));
            return antiderivative(add(terms), variable, depth + 1);
        }
    }

    QVector<Expr> factors = key.type == Opcode::Mul ? key.children : QVector<Expr>{expression};
    if (key.type == Opcode::Div && key.children.at(0) != m_one) {
        const Expr numerator = key.children.at(0);
        const Key& product = node(numerator).key;
        factors = product.type == Opcode::Mul ? product.children : QVector<Expr>{numerator};
        factors.append(divide(m_one, key.children.at(1)));
    }

    QVector<Expr> constants;
    QVector<Expr> dependent;
    for (Expr factor : factors) {
        if (dependsOn(factor, variable))
            dependent.append(factor);
        else
            constants.append(factor);
    }
    if (!constants.isEmpty() && !dependent.isEmpty()) {
        const Expr result = antiderivative(multiply(dependent), variable, depth + 1);
        if (result != NoExpression)
            return multiply({multiply(constants), result});
        return NoExpression;
    }

    for (int i = 0; i < factors.size(); ++i) {
        QVector<Expr> rest = factors;
        rest.removeAt(i);
        const Expr otherFactors = multiply(rest);
        const auto applySubstitution = [&](Expr primitive, Expr argument) {
            if (primitive == NoExpression || !dependsOn(argument, variable))
                return NoExpression;
            const Expr slope = derivative(argument, variable);
            if (slope == m_zero)
                return NoExpression;
            const Expr ratio = divide(otherFactors, slope);
            return dependsOn(ratio, variable) ? NoExpression : multiply({ratio, primitive});
        };
        Expr argument = NoExpression;
        const Expr primitive = kernelPrimitive(factors.at(i), argument);
        const Expr result = applySubstitution(primitive, argument);
        if (result != NoExpression)
            return result;
        const auto power = powerKernel(factors.at(i));
        if (power && power->base != argument) {
            const Expr alternative = applySubstitution(powerPrimitive(*power), power->base);
            if (alternative != NoExpression)
                return alternative;
        }
    }

    const Expr gaussianTerm = gaussianPrimitive(factors, variable);
    if (gaussianTerm != NoExpression)
        return gaussianTerm;

    // Keep integration by parts to a polynomial times a function of a linear argument.
    if (factors.size() >= 2) {
        for (int i = 0; i < factors.size(); ++i) {
            const Key& wave = node(factors.at(i)).key;
            if (wave.type != Opcode::Function || wave.children.size() != 1)
                continue;
            const auto rule = functionRules().value(wave.text).rule;
            if (rule != Rule::Exp && rule != Rule::Sin && rule != Rule::Cos
                && rule != Rule::Sinh && rule != Rule::Cosh) {
                continue;
            }
            const Expr slope = derivative(wave.children.at(0), variable);
            if (slope == m_zero || dependsOn(slope, variable))
                continue;

            auto other = factors;
            other.removeAt(i);
            const Expr polynomialFactor = multiply(other);
            const auto coefficients = polynomial(polynomialFactor, variable);
            if (!coefficients || coefficients->size() > 8)
                continue;
            const Expr wavePrimitive = antiderivative(factors.at(i), variable, depth + 1);
            if (wavePrimitive == NoExpression)
                continue;
            const Expr remainder = antiderivative(
                multiply({derivative(polynomialFactor, variable), wavePrimitive}), variable, depth + 1);
            if (remainder != NoExpression)
                return subtract(multiply({polynomialFactor, wavePrimitive}), remainder);
        }

        // A polynomial times an error function: integrate the polynomial and
        // differentiate the error function, which leaves a polynomial times an
        // exponential. The integrated polynomial must stay within the degree
        // limit of polynomial(), so the polynomial here can have degree 23.
        // Other products, such as x*exp(-x^2)*erf(x), are not supported.
        for (int i = 0; i < factors.size(); ++i) {
            const Key& part = node(factors.at(i)).key;
            if (part.type != Opcode::Function)
                continue;
            const auto rule = functionRules().value(part.text).rule;
            if (rule != Rule::Erf && rule != Rule::Erfc && rule != Rule::Erfi)
                continue;
            auto other = factors;
            other.removeAt(i);
            const auto coefficients = polynomial(multiply(other), variable);
            if (!coefficients)
                continue;
            const Expr integral = integratePolynomial(*coefficients, x);
            const Expr remainder = antiderivative(
                multiply({integral, derivative(factors.at(i), variable)}), variable, depth + 1);
            if (remainder != NoExpression)
                return subtract(multiply({integral, factors.at(i)}), remainder);
        }
    }
    if (const auto coefficients = polynomial(expression, variable))
        return integratePolynomial(*coefficients, x);

    if (key.type == Opcode::Div) {
        const Expr denominator = key.children.at(1);
        const auto divisor = polynomial(denominator, variable);
        const auto dividend = polynomial(key.children.at(0), variable);
        if (dividend && divisor && divisor->size() == 2 && divisor->at(1) != m_zero) {
            QVector<Expr> remainder = *dividend;
            QVector<Expr> quotient(qMax(0, int(dividend->size()) - 1), m_zero);
            for (int i = remainder.size() - 1; i > 0; --i) {
                quotient[i - 1] = divide(remainder.at(i), divisor->at(1));
                remainder[i - 1] = subtract(remainder.at(i - 1),
                                             multiply({quotient.at(i - 1), divisor->at(0)}));
            }
            return add({integratePolynomial(quotient, x),
                        multiply({divide(remainder.at(0), divisor->at(1)),
                                  function(QStringLiteral("ln"), function(QStringLiteral("abs"), denominator))})});
        }
    }
    return NoExpression;
}

HNumber Calculus::magnitude(Expr expression, const QString& variable, const HNumber& x)
{
    spend();
    const Node& value = node(expression);
    const Key& key = value.key;
    switch (key.type) {
    case Opcode::Load:
        return value.number ? value.number->numerator.toHNumber() / value.number->denominator.toHNumber()
                            : HNumber(key.text.toLatin1().constData());
    case Opcode::Ref:
        if (!variable.isEmpty() && key.text == variable)
            return x;
        if (key.text == QStringLiteral("π"))
            return HMath::pi();
        if (key.text == QLatin1String("e"))
            return HMath::e();
        if (m_samplingDomain) {
            const auto value = m_lookupVariable(key.text);
            if (value && value->isReal() && !value->isNan() && !value->isCollection())
                return value->numericValue().real;
        }
        return HMath::nan();
    case Opcode::UnitRef: {
        Quantity unit;
        if (m_lookupUnit(key.text, &unit) && unit.isReal())
            return unit.numericValue().real;
        return HMath::nan();
    }
    default:
        break;
    }

    const auto at = [&](int i) { return magnitude(key.children.at(i), variable, x); };
    const HNumber first = at(0);
    if (first.isNan())
        return first;
    switch (key.type) {
    case Opcode::Neg: return -first;
    case Opcode::Sub: return first - at(1);
    case Opcode::Div: return first / at(1);
    case Opcode::Pow: return HMath::raise(first, at(1));
    case Opcode::Add: case Opcode::Mul: case Opcode::Unit: {
        HNumber result = first;
        for (int i = 1; i < key.children.size(); ++i)
            result = key.type == Opcode::Add ? result + at(i) : result * at(i);
        return result;
    }
    case Opcode::Function: {
        Function::ArgumentList args{Quantity(first)};
        if (key.children.size() == 2)
            args.append(Quantity(at(1)));
        const auto* repo = FunctionRepo::instance();
        const Quantity result = repo->find(key.text)->exec(args);
        return result.isReal() && !result.isNan() && !result.isCollection()
            ? result.numericValue().real : HMath::nan();
    }
    default:
        return HMath::nan();
    }
}

std::optional<Calculus::Interval> Calculus::enclosure(
    Expr expression, const QString& variable, const std::optional<Interval>& interval,
    HNumber* phaseWidth, bool forLimit)
{
    spend();
    const Key& key = node(expression).key;
    if (key.children.isEmpty()) {
        if (key.type == Opcode::Ref && key.text == variable)
            return interval;
        const HNumber value = magnitude(expression);
        return value.isNan() ? std::nullopt : std::optional<Interval>{{value, value}};
    }

    const auto enclosesZero = [](const Interval& range) {
        return range.lower <= 0 && range.upper >= 0;
    };
    const auto ordered = [](const HNumber& a, const HNumber& b) {
        if (a.isNan() || b.isNan())
            throw DomainUnknown{};
        return Interval{qMin(a, b), qMax(a, b)};
    };
    const auto product = [&](const Interval& a, const Interval& b) {
        const Interval one = ordered(a.lower * b.lower, a.lower * b.upper);
        const Interval two = ordered(a.upper * b.lower, a.upper * b.upper);
        return Interval{qMin(one.lower, two.lower), qMax(one.upper, two.upper)};
    };
    const auto at = [&](int i) {
        return enclosure(key.children.at(i), variable, interval, phaseWidth, forLimit);
    };
    const auto first = at(0);

    switch (key.type) {
    case Opcode::Neg:
        return first ? std::optional<Interval>{{-first->upper, -first->lower}} : std::nullopt;
    case Opcode::Add: case Opcode::Mul: case Opcode::Unit: {
        auto result = first;
        for (int i = 1; i < key.children.size(); ++i) {
            const auto child = at(i);
            if (!result || !child)
                result.reset();
            else if (key.type == Opcode::Add)
                result = ordered(result->lower + child->lower, result->upper + child->upper);
            else
                result = product(*result, *child);
        }
        return result;
    }
    case Opcode::Sub: {
        const auto second = at(1);
        if (first && second)
            return ordered(first->lower - second->upper, first->upper - second->lower);
        return std::nullopt;
    }
    case Opcode::Div: {
        const auto second = at(1);
        if (!second && squareWithPositiveOffset(key.children.at(1)))
            return std::nullopt;
        if (!second || enclosesZero(*second))
            throw DomainUnknown{};
        if (m_samplingDomain && phaseWidth) {
            const HNumber left = HMath::abs(second->lower);
            const HNumber right = HMath::abs(second->upper);
            *phaseWidth = qMax(*phaseWidth, HMath::ln(qMax(left, right)) - HMath::ln(qMin(left, right)));
        }
        return first ? std::optional<Interval>{product(*first, {HNumber(1) / second->upper,
                                                                HNumber(1) / second->lower})}
                     : std::nullopt;
    }
    case Opcode::Pow: {
        const auto second = at(1);
        if (second && second->lower == second->upper) {
            const HNumber exponent = second->lower;
            if (exponent.isZero())
                return Interval{1, 1};
            const bool integer = exponent.isInteger();
            if (!first) {
                if (integer && exponent.isPositive())
                    return std::nullopt;
                throw DomainUnknown{};
            }
            if ((!integer && first->lower < 0) || (exponent.isNegative() && enclosesZero(*first)))
                throw DomainUnknown{};
            auto range = ordered(HMath::raise(first->lower, exponent), HMath::raise(first->upper, exponent));
            if (integer && exponent.isPositive() && (exponent % 2).isZero() && enclosesZero(*first))
                range.lower = 0;
            return range;
        }
        if (!first || first->lower <= 0)
            throw DomainUnknown{};
        if (!second)
            return std::nullopt;
        const Interval one = ordered(HMath::raise(first->lower, second->lower),
                                     HMath::raise(first->lower, second->upper));
        const Interval two = ordered(HMath::raise(first->upper, second->lower),
                                     HMath::raise(first->upper, second->upper));
        return Interval{qMin(one.lower, two.lower), qMax(one.upper, two.upper)};
    }
    case Opcode::Function:
        break;
    default:
        throw DomainUnknown{};
    }

    if (forLimit && key.text == QLatin1String("gamma")) {
        if (!first || HMath::ceil(first->lower) <= qMin(HNumber(0), HMath::floor(first->upper)))
            throw DomainUnknown{};
        return std::nullopt;
    }
    if (forLimit && !functionRules().contains(key.text))
        throw DomainUnknown{};
    const Rule rule = functionRules().value(key.text).rule;
    const bool circular = rule == Rule::Sin || rule == Rule::Cos || rule == Rule::Tan
        || rule == Rule::Cot || rule == Rule::Sec || rule == Rule::Csc;
    if (node(expression).containsUnit && (circular || !m_samplingDomain)
        && rule != Rule::Sqrt && rule != Rule::Cbrt && rule != Rule::Abs) {
        throw DomainUnknown{};
    }
    if (key.children.size() != 1) {
        const auto second = at(1);
        if (rule == Rule::Log && first && second && first->lower > 0 && second->lower > 0
            && (first->upper < 1 || first->lower > 1)) {
            return product({HMath::ln(second->lower), HMath::ln(second->upper)},
                           {HNumber(1) / HMath::ln(first->upper), HNumber(1) / HMath::ln(first->lower)});
        }
        if (rule == Rule::Arctan2 && first && second
            && (first->lower > 0 || second->lower > 0 || second->upper < 0)) {
            return std::nullopt;
        }
        throw DomainUnknown{};
    }
    const auto evaluate = [&](const HNumber& x) {
        const Quantity value = FunctionRepo::instance()->find(key.text)->exec({Quantity(x)});
        return value.isReal() && !value.isNan() ? value.numericValue().real : HMath::nan();
    };
    if (circular) {
        if (m_samplingDomain) {
            QSet<QString> names;
            infer(key.children.at(0), names);
            for (const QString& name : names) {
                if (name == variable)
                    continue;
                const auto value = m_lookupVariable(name);
                if (value && Units::isExplicitAngleUnitName(value->unitName()))
                    throw Failure{tr("use unitless angles in the integrand")};
            }
        }
        if (!first) {
            if (forLimit)
                throw DomainUnknown{};
            if (rule == Rule::Sin || rule == Rule::Cos)
                return Interval{-1, 1};
            throw DomainUnknown{};
        }
        const HNumber scale = magnitude(radiansPerAngle());
        const Interval radians{first->lower * scale, first->upper * scale};
        const HNumber pi = HMath::pi();
        const HNumber halfPi = pi / HNumber(2);
        const HNumber roundoff = HMath::raise(HNumber(10), 8 - DECPRECISION)
            * qMax(HNumber(1), qMax(HMath::abs(radians.lower), HMath::abs(radians.upper)));
        const auto crosses = [&](const HNumber& offset, const HNumber& period) {
            return HMath::ceil((radians.lower - offset - roundoff) / period)
                <= HMath::floor((radians.upper - offset + roundoff) / period);
        };
        if (phaseWidth)
            *phaseWidth = qMax(*phaseWidth, radians.upper - radians.lower);
        if (rule == Rule::Sin || rule == Rule::Cos) {
            Interval range = ordered(evaluate(first->lower), evaluate(first->upper));
            const HNumber peak = rule == Rule::Sin ? halfPi : HNumber(0);
            if (crosses(peak, HNumber(2) * pi))
                range.upper = 1;
            if (crosses(peak + pi, HNumber(2) * pi))
                range.lower = -1;
            if (crosses(peak - halfPi, pi)) {
                range.lower = qMin(range.lower, HNumber(0));
                range.upper = qMax(range.upper, HNumber(0));
            }
            return range;
        }
        const HNumber pole = (rule == Rule::Tan || rule == Rule::Sec) ? halfPi : HNumber(0);
        if (crosses(pole, pi))
            throw DomainUnknown{};
        if (rule == Rule::Tan || rule == Rule::Cot)
            return ordered(evaluate(first->lower), evaluate(first->upper));
        // The secant and cosecant have extrema between their poles.
        return std::nullopt;
    }

    switch (rule) {
    case Rule::Exp: case Rule::Abs: case Rule::Cbrt: case Rule::Sinh: case Rule::Cosh:
    case Rule::Tanh: case Rule::Arsinh: case Rule::Arctan: case Rule::Erf: case Rule::Erfc: case Rule::Erfi:
        break;
    case Rule::Ln: case Rule::Lg: case Rule::Lb:
        if (!first || first->lower <= 0)
            throw DomainUnknown{};
        break;
    case Rule::Sqrt:
        if (!first || first->lower < 0)
            throw DomainUnknown{};
        break;
    case Rule::Arcsin: case Rule::Arccos:
        if (!first || first->lower < -1 || first->upper > 1)
            throw DomainUnknown{};
        break;
    case Rule::Arcosh:
        if (!first || first->lower < 1)
            throw DomainUnknown{};
        break;
    case Rule::Artanh:
        if (!first || first->lower <= -1 || first->upper >= 1)
            throw DomainUnknown{};
        break;
    default:
        throw DomainUnknown{};
    }
    if (!first)
        return std::nullopt;
    if (m_samplingDomain && phaseWidth && rule == Rule::Exp)
        *phaseWidth = qMax(*phaseWidth, first->upper - first->lower);
    auto range = ordered(evaluate(first->lower), evaluate(first->upper));
    if (enclosesZero(*first) && (rule == Rule::Abs || rule == Rule::Cosh))
        range.lower = rule == Rule::Abs ? 0 : 1;
    return range;
}

void Calculus::verifyDomain(Expr raw, const QString& variable,
                            const std::optional<Interval>& interval, HNumber* phaseWidth)
{
    struct Part {
        std::optional<Interval> interval;
        int depth;
    };
    QVector<Part> pending{{interval, 0}};
    int checks = 0;
    while (!pending.isEmpty()) {
        const Part part = pending.takeLast();
        try {
            enclosure(raw, variable, part.interval, phaseWidth);
        } catch (const DomainUnknown&) {
            if (!part.interval || part.depth >= 12 || ++checks >= 128)
                throw Failure{tr("could not verify the integral's domain or convergence")};
            const Interval& range = *part.interval;
            const HNumber middle = range.lower + (range.upper - range.lower) / HNumber(2);
            if (middle == range.lower || middle == range.upper)
                throw Failure{tr("could not verify the integral's domain or convergence")};
            pending.append({Interval{middle, range.upper}, part.depth + 1});
            pending.append({Interval{range.lower, middle}, part.depth + 1});
        }
    }
}

int Calculus::namedInfinity(Span span) const
{
    const int length = span.end - span.first;
    const Opcode& first = m_opcodes.at(span.first);
    if (first.type == Opcode::Ref && (length == 1
        || (length == 2 && m_opcodes.at(span.first + 1).type == Opcode::Neg))) {
        const QString name = m_identifiers.value(first.index);
        if (name == QString(UnicodeChars::Infinity)
            || (SymbolicCalculus::isNamedInfinity(name) && !m_lookupVariable(name))) {
            return length == 1 ? 1 : -1;
        }
    }
    return 0;
}

Calculus::Bound Calculus::readBound(Span span, int nesting)
{
    const int length = span.end - span.first;
    if (length > MaxScalarOpcodes)
        limitExceeded();
    if (const int infinity = namedInfinity(span))
        return {NoExpression, DMath::nan(), infinity};

    const Quantity value = canEvaluateScalar(span)
        ? m_evalSpan(span.first, length, {}, Quantity()) : DMath::nan();
    if (!value.isNan() && (value.isSymbolic() || value.isCollection() || !value.isReal()))
        throw Failure{tr("integration bounds must be real scalar values")};
    if (!value.isNan() && (value.isApproximate() || value.hasUnit() || value.hasDimension())) {
        return {scalarLiteral(value.numericValue().real), value, 0};
    }

    Expr raw;
    try {
        raw = readExpression(span, nesting);
    } catch (const Failure&) {
        if (value.isNan())
            throw;
        return {scalarLiteral(value.numericValue().real), value, 0};
    }
    QSet<QString> variables;
    infer(raw, variables);
    for (auto it = variables.begin(); it != variables.end();) {
        const QString name = *it;
        const auto saved = m_lookupVariable(name);
        if (!saved) {
            ++it;
            continue;
        }
        if (!saved->isReal() || saved->isNan() || saved->isCollection() || saved->hasUnit()
            || saved->hasDimension() || saved->isApproximate()) {
            if (!value.isNan())
                return {scalarLiteral(value.numericValue().real), value, 0};
            throw Failure{tr("invalid integration bound")};
        }
        raw = substitute(raw, reference(name), scalarLiteral(saved->numericValue().real));
        it = variables.erase(it);
    }
    if (!value.isNan() && !variables.isEmpty())
        return {scalarLiteral(value.numericValue().real), value, 0};
    if (value.isNan()) {
        if (variables.isEmpty())
            throw Failure{tr("invalid integration bound")};
        verifyDomain(raw, {}, std::nullopt, nullptr);
    }
    return {simplify(raw), value, 0};
}

Calculus::Endpoint Calculus::verifyPowerIntegral(Expr raw, const QString& variable,
                                                const Interval& interval, HNumber* phaseWidth)
{
    // Keep the raw denominator. Simplification can remove a zero from it.
    const HNumber& a = interval.lower;
    const HNumber& b = interval.upper;
    Expr coefficient = m_one;
    const auto kernel = scaledPower(raw, coefficient);
    if (!kernel || !node(kernel->exponent).number
        || !node(kernel->exponent).number->numerator.isNegative()) {
        return {};
    }
    const HNumber exponent = magnitude(kernel->exponent);
    const HNumber left = magnitude(kernel->base, variable, a);
    const HNumber right = magnitude(kernel->base, variable, b);
    if (left.isNan() || right.isNan())
        return {};

    const Expr simplifiedBase = simplify(kernel->base);
    const Expr slope = derivative(simplifiedBase, variable);
    const bool affine = slope != m_zero && !dependsOn(slope, variable);
    const auto crossesZero = [](const HNumber& x, const HNumber& y) {
        return x.isZero() || y.isZero() || x.isNegative() != y.isNegative();
    };
    bool hasZero = crossesZero(left, right);
    if (!affine) {
        const Expr curvature = derivative(slope, variable);
        if (curvature == m_zero || dependsOn(curvature, variable))
            return {};
        const HNumber second = magnitude(curvature);
        const HNumber first = magnitude(slope, variable, a);
        if (second.isNan() || second.isZero() || first.isNan())
            return {};
        const HNumber vertex = a - first / second;
        if (vertex > a && vertex < b) {
            const HNumber atVertex = magnitude(kernel->base, variable, vertex);
            hasZero = hasZero || (!atVertex.isNan() && crossesZero(left, atVertex));
        }
    }
    if (!hasZero)
        return {};
    verifyDomain(kernel->base, variable, interval, phaseWidth);
    verifyDomain(coefficient, variable, interval, phaseWidth);
    if (exponent <= -1) {
        try {
            const auto scale = enclosure(coefficient, variable, interval, nullptr);
            if (scale && (scale->lower > 0 || scale->upper < 0))
                throw Failure{tr("divergent integral")};
        } catch (const DomainUnknown&) {
        }
        return {};
    }
    if (affine && left >= 0 && right >= 0 && left.isZero() != right.isZero()) {
        const Integer denominator = node(kernel->exponent).number->denominator;
        return {left.isZero() ? -1 : 1, denominator <= 12 ? denominator.toInt() : 0};
    }
    return {};
}

int Calculus::logarithmEndpoint(Expr raw, const QString& variable, const Interval& interval)
{
    const Key& key = node(raw).key;
    if (key.type != Opcode::Function)
        return 0;
    const Rule rule = functionRules().value(key.text).rule;
    if (rule != Rule::Ln && rule != Rule::Lg && rule != Rule::Lb)
        return 0;
    const Expr argument = key.children.at(0);
    const HNumber left = magnitude(argument, variable, interval.lower);
    const HNumber right = magnitude(argument, variable, interval.upper);
    if (left.isNan() || right.isNan() || !((left.isZero() && right > 0) || (right.isZero() && left > 0)))
        return 0;
    const Expr slope = derivative(simplify(argument), variable);
    if (!dependsOn(slope, variable) && slope != m_zero) {
        verifyDomain(argument, variable, interval, nullptr);
        return left.isZero() ? -1 : 1;
    }
    return 0;
}

Expr Calculus::infiniteLimit(Expr raw, Expr primitive, const QString& variable, const Bound& lower,
                             const Bound& upper)
{
    const Expr expression = simplify(raw);
    if (expression == m_zero) {
        verifyDomain(raw, variable, std::nullopt, nullptr);
        return m_zero;
    }
    if (!dependsOn(expression, variable)) {
        verifyDomain(raw, variable, std::nullopt, nullptr);
        const HNumber constant = magnitude(expression);
        if (!constant.isNan() && !constant.isZero())
            throw Failure{tr("divergent integral")};
        return NoExpression;
    }
    const Expr x = reference(variable);
    if (primitive != NoExpression) {
        // A primitive made of polynomials, exponentials and error functions is
        // continuous everywhere, so finite limits show that the integral
        // converges.
        const Expr a = lower.infinity ? limitAtInfinity(primitive, variable, -1)
                                      : substitute(primitive, x, lower.expression);
        const Expr b = upper.infinity ? limitAtInfinity(primitive, variable, 1)
                                      : substitute(primitive, x, upper.expression);
        if (a != NoExpression && b != NoExpression) {
            verifyDomain(raw, variable, std::nullopt, nullptr);
            return subtract(b, a);
        }
    }

    // An integrand that goes to a limit other than zero, or that falls no faster
    // than 1/x, has no finite integral up to infinity. So has one that grows
    // like an exponential. Some integrands cannot be decided, such as
    // exp(-x^2)*erfi(x), which falls like 1/x, or erf(x)-1, which falls like a
    // Gaussian. They give the error about convergence later on.
    const auto diverges = [&](int direction) {
        const Expr limit = limitAtInfinity(expression, variable, direction);
        if (limit != NoExpression) {
            const HNumber value = magnitude(limit);
            return !value.isNan() && !value.isZero();
        }
        const Key& top = node(expression).key;
        const Expr above = top.type == Opcode::Div ? top.children.at(0) : expression;
        QVector<Expr> below{top.type == Opcode::Div ? top.children.at(1) : m_one};
        QVector<Expr> factors = node(above).key.type == Opcode::Mul ? node(above).key.children
                                                                     : QVector<Expr>{above};
        for (Expr& factor : factors) {
            // Where u grows, erfi(u) grows faster than exp(u^2)/u^2.
            const Key& part = node(factor).key;
            if (part.type == Opcode::Function && part.text == QLatin1String("erfi")) {
                const Expr square = power(part.children.at(0), number(2));
                factor = function(QStringLiteral("exp"), square);
                below.append(square);
            }
        }
        QVector<Expr> kernels;
        QVector<Expr> rest;
        const auto exponent = exponentPolynomial(factors, variable, kernels, rest);
        const auto numerator = scaledPolynomial(multiply(rest), variable);
        const auto denominator = scaledPolynomial(multiply(below), variable);
        if (!exponent || !numerator || !denominator)
            return false;
        if (!kernels.isEmpty() && exponent->size() > 1)
            return signAtInfinity(*exponent, direction) > 0;
        const HNumber leading = magnitude(numerator->constLast());
        const HNumber divisor = magnitude(denominator->constLast());
        return numerator->size() + 1 >= denominator->size() && !leading.isNan() && !leading.isZero()
            && !divisor.isNan() && !divisor.isZero();
    };
    // These forms are defined for any large x, which is all that the test needs.
    if ((lower.infinity && diverges(-1)) || (upper.infinity && diverges(1)))
        throw Failure{tr("divergent integral")};

    const Bound& finite = lower.infinity ? upper : lower;
    const int direction = lower.infinity ? -1 : 1;
    const Key& key = node(expression).key;
    Expr kernelExpr = expression;
    if (key.type == Opcode::Mul) {
        for (Expr factor : key.children) {
            if (dependsOn(factor, variable)) {
                if (kernelExpr != expression)
                    return NoExpression;
                kernelExpr = factor;
            }
        }
    }

    const Key& kernel = node(kernelExpr).key;
    if (kernel.type == Opcode::Function && kernel.text == QLatin1String("exp")) {
        const Expr slope = derivative(kernel.children.at(0), variable);
        if (dependsOn(slope, variable))
            return NoExpression;
        const HNumber rate = magnitude(slope);
        if (rate.isNan() || rate.isZero())
            return NoExpression;
        verifyDomain(raw, variable, std::nullopt, nullptr);
        if ((lower.infinity && rate < 0) || (upper.infinity && rate > 0))
            throw Failure{tr("divergent integral")};
        if (primitive != NoExpression && !finite.infinity)
            return direction > 0 ? negate(substitute(primitive, x, finite.expression))
                                 : substitute(primitive, x, finite.expression);
    }
    Expr coefficient = m_one;
    const auto power = scaledPower(raw, coefficient);
    if (power && power->exponent == number(-1) && primitive != NoExpression
        && !dependsOn(coefficient, variable)) {
        if (const auto square = squareWithPositiveOffset(power->base)) {
            const Expr slope = derivative(simplify(square->argument), variable);
            const HNumber rate = magnitude(slope);
            if (!dependsOn(slope, variable) && !rate.isNan() && !rate.isZero()) {
                verifyDomain(raw, variable, std::nullopt, nullptr);
                const Expr halfTurn = multiply({number(1, 2), reference(QStringLiteral("π"))});
                const Expr limit = divide(multiply({simplify(coefficient), halfTurn}),
                                          multiply({function(QStringLiteral("sqrt"), square->offset),
                                                    rate.isNegative() ? negate(slope) : slope}));
                const Expr a = lower.infinity ? negate(limit) : substitute(primitive, x, lower.expression);
                const Expr b = upper.infinity ? limit : substitute(primitive, x, upper.expression);
                return subtract(b, a);
            }
        }
    }
    if (!finite.infinity && !finite.value.isNan()) {
        if (power) {
            const Expr slope = derivative(simplify(power->base), variable);
            if (!dependsOn(slope, variable) && !dependsOn(coefficient, variable)) {
                const HNumber rate = magnitude(slope);
                const HNumber exponent = magnitude(power->exponent);
                const HNumber atBound = magnitude(power->base, variable, finite.value.numericValue().real);
                if (rate.isNan() || atBound.isNan() || rate.isZero())
                    return NoExpression;
                verifyDomain(power->base, variable, std::nullopt, nullptr);
                verifyDomain(coefficient, variable, std::nullopt, nullptr);
                if (atBound.isZero() || (rate * HNumber(direction)).isNegative() != atBound.isNegative()) {
                    if (exponent <= -1)
                        throw Failure{tr("divergent integral")};
                    return NoExpression;
                }
                if (!exponent.isInteger() && atBound.isNegative())
                    return NoExpression;
                if (exponent >= -1)
                    throw Failure{tr("divergent integral")};
                if (primitive != NoExpression)
                    return direction > 0 ? negate(substitute(primitive, x, finite.expression))
                                         : substitute(primitive, x, finite.expression);
            }
        }
    }
    return NoExpression;
}

Calculus::LimitTerm Calculus::term(Expr coefficient, bool constant, bool analytic,
                                   Expr order, Expr logOrder, Expr exponent) const
{
    return {LimitForm::Term, coefficient, order == NoExpression ? m_zero : order,
            logOrder == NoExpression ? m_zero : logOrder,
            exponent == NoExpression ? m_zero : exponent, constant, analytic};
}

std::optional<int> Calculus::limitSign(Expr expression)
{
    spend();
    const Node& value = node(expression);
    const Key& key = value.key;
    if (value.number)
        return value.number->numerator.isZero() ? 0 : value.number->numerator.isNegative() ? -1 : 1;
    if (key.type == Opcode::Load || (key.type == Opcode::Ref && isConstant(key.text))) {
        const HNumber numeric = magnitude(expression);
        if (!numeric.isNan() && !numeric.isZero())
            return numeric.isNegative() ? -1 : 1;
    }
    const auto sign = [&](int index) { return limitSign(key.children.at(index)); };
    if (key.type == Opcode::Neg) {
        if (const auto inner = sign(0))
            return -*inner;
    }
    if (key.type == Opcode::Mul || key.type == Opcode::Div) {
        int result = 1;
        for (Expr child : key.children) {
            const auto factor = limitSign(child);
            if (!factor)
                return std::nullopt;
            result *= *factor;
        }
        return result;
    }
    if (key.type == Opcode::Add) {
        int result = 0;
        for (Expr child : key.children) {
            const auto next = limitSign(child);
            if (next && *next && result && result != *next)
                return std::nullopt;
            if (next && *next)
                result = *next;
        }
        if (!result)
            return std::nullopt;
        for (Expr child : key.children) {
            const auto next = limitSign(child);
            if (next)
                continue;
            const Key& part = node(child).key;
            const auto [base, n] = factorPower(child);
            if (result < 0 || ((base == child || n % 2) && radicand(child) == NoExpression
                               && !(part.type == Opcode::Function && part.text == QLatin1String("abs")))) {
                return std::nullopt;
            }
        }
        return result;
    }
    if (key.type == Opcode::Pow) {
        const auto base = sign(0);
        const auto& exponent = node(key.children.at(1)).number;
        if (base && *base >= 0)
            return base;
        if (base && exponent && exponent->denominator == 1)
            return (exponent->numerator % 2).isZero() ? 1 : -1;
    }
    if (key.type != Opcode::Function || key.children.size() != 1)
        return std::nullopt;

    const Expr u = key.children.at(0);
    if (key.text == QLatin1String("sin") || key.text == QLatin1String("cos"))
        return circularSign(u, key.text == QLatin1String("sin"));
    if (key.text == QLatin1String("exp") || key.text == QLatin1String("erfc")
        || key.text == QLatin1String("cosh")) {
        return 1;
    }
    if (key.text == QLatin1String("ln") || key.text == QLatin1String("lg") || key.text == QLatin1String("lb"))
        return limitSign(subtract(u, m_one));
    if (key.text == QLatin1String("cbrt") || key.text == QLatin1String("sqrt")
        || key.text == QLatin1String("erf") || key.text == QLatin1String("erfi")
        || key.text == QLatin1String("arctan") || key.text == QLatin1String("sinh")
        || key.text == QLatin1String("tanh") || key.text == QLatin1String("arsinh")) {
        return sign(0);
    }
    if (key.text == QLatin1String("abs")) {
        if (const auto inner = sign(0))
            return qAbs(*inner);
    }
    return std::nullopt;
}

std::optional<int> Calculus::circularSign(Expr angle, bool sine)
{
    const auto coefficients = scaledPolynomial(multiply({angle, radiansPerAngle()}), QStringLiteral("π"));
    if (!coefficients || coefficients->size() > 2)
        return std::nullopt;
    const Expr offset = coefficients->at(0);
    const Expr multiplier = coefficients->value(1, m_zero);
    if (!node(offset).number || !node(multiplier).number)
        return std::nullopt;
    // Locate the angle in an open quadrant using rational bounds on pi.
    const auto quadrant = [&](const QString& bound) {
        const Expr pi = number(*exactLiteral(bound));
        const Expr scaled = multiply({number(2), add({multiplier, divide(offset, pi)})});
        const auto& value = *node(scaled).number;
        Integer whole;
        Integer remainder;
        Integer::divide(value.numerator, value.denominator, whole, remainder);
        return std::pair{remainder.isNegative() ? whole - 1 : whole, !remainder.isZero()};
    };
    const auto [first, firstOpen] = quadrant(QStringLiteral("3.141592653589793238462643383279"));
    const auto [last, lastOpen] = quadrant(QStringLiteral("3.141592653589793238462643383280"));
    if (first != last)
        return std::nullopt;
    const int index = ((first % 4).toInt() + 4) % 4;
    if (!firstOpen || !lastOpen) {
        if (offset != m_zero)
            return std::nullopt;
        const int values[] = {0, 1, 0, -1};
        return values[(index + (sine ? 0 : 1)) % 4];
    }
    return sine ? (index < 2 ? 1 : -1) : (index == 0 || index == 3 ? 1 : -1);
}

std::optional<int> Calculus::termSign(const LimitTerm& value)
{
    if (value.form != LimitForm::Term)
        return std::nullopt;
    const auto sign = limitSign(value.coefficient);
    return sign && (*sign || value.constant) ? sign : std::nullopt;
}

std::optional<int> Calculus::compareGrowth(const LimitTerm& left, const LimitTerm& right, LimitPath& path)
{
    spend();
    if (path.depth >= MaxDepth)
        limitExceeded();
    const QScopedValueRollback<int> depth(path.depth, path.depth + 1);
    const Expr order = subtract(right.order, left.order);
    const int power = limitSign(order).value_or(0);
    const int logarithm = limitSign(subtract(left.logOrder, right.logOrder)).value_or(0);
    const int algebraic = power ? power : logarithm;
    if (left.exponent == right.exponent)
        return algebraic;

    const LimitTerm exponent = limitTerm(subtract(left.exponent, right.exponent), path);
    if (exponent.form == LimitForm::Term) {
        const auto sign = termSign(exponent);
        const auto relativeToLog = compareGrowth(exponent, term(m_one, false, false, m_zero, m_one), path);
        // An exponent that grows faster than log(1/h) dominates every
        // power and logarithm. A bounded exponent changes only the coefficient.
        if (sign && *sign && relativeToLog && *relativeToLog > 0)
            return sign;
        if (exponent.exponent == m_zero) {
            if (exponent.order == m_zero && exponent.logOrder == m_one) {
                const auto net = limitSign(add({exponent.coefficient, order}));
                return net && *net ? net : std::nullopt;
            }
            const auto value = classifyLimit(exponent, path);
            if (value.status == LimitStatus::Finite && (algebraic || value.expression == m_zero))
                return algebraic;
        }
    }
    if (exponent.form == LimitForm::Vanishing)
        return algebraic;
    return std::nullopt;
}

Calculus::LimitValue Calculus::classifyLimit(const LimitTerm& value, LimitPath& path)
{
    if (value.form == LimitForm::NoDomain || value.form == LimitForm::Oscillatory)
        return {LimitStatus::DoesNotExist};
    if (value.form == LimitForm::Vanishing)
        return {LimitStatus::Finite, m_zero};
    if (value.form == LimitForm::Divergent)
        return {LimitStatus::Infinite, NoExpression, value.coefficient == m_minusOne ? -1 : 1};
    if (value.form != LimitForm::Term && value.form != LimitForm::Wave && value.form != LimitForm::Bound)
        return {};
    Expr coefficient = value.coefficient;
    LimitTerm rate = value;
    if (value.exponent != m_zero) {
        const auto exponent = classifyLimit(limitTerm(value.exponent, path), path);
        if (exponent.status == LimitStatus::Finite) {
            coefficient = multiply({coefficient, function(QStringLiteral("exp"), exponent.expression)});
            rate.exponent = m_zero;
        }
    }
    const auto growth = compareGrowth(rate, term(m_one), path);
    if (!growth)
        return {};
    if (*growth < 0 || (value.constant && coefficient == m_zero))
        return {LimitStatus::Finite, m_zero};
    if (value.form != LimitForm::Term) {
        if (value.form == LimitForm::Wave && limitSign(coefficient).value_or(0))
            return {LimitStatus::DoesNotExist};
        return {};
    }
    if (*growth == 0)
        return {LimitStatus::Finite, coefficient};
    const int sign = limitSign(coefficient).value_or(0);
    return sign ? LimitValue{LimitStatus::Infinite, NoExpression, sign} : LimitValue{};
}

Calculus::LimitTerm Calculus::limitTerm(Expr expression, LimitPath& path)
{
    spend();
    const auto cached = path.terms.constFind(expression);
    if (cached != path.terms.constEnd())
        return cached.value();
    if (path.depth >= MaxDepth)
        limitExceeded();
    const QScopedValueRollback<int> depth(path.depth, path.depth + 1);
    path.terms.insert(expression, {});
    const auto result = analyzeLimit(expression, path);
    path.terms.insert(expression, result);
    return result;
}

Calculus::LimitTerm Calculus::analyzeLimit(Expr expression, LimitPath& path)
{
    const Key& key = node(expression).key;
    const auto at = [&](int index) { return limitTerm(key.children.at(index), path); };
    switch (key.type) {
    case Opcode::Load:
        return term(expression, true, true);
    case Opcode::Ref:
        if (key.text == path.variable) {
            if (path.infinity)
                return term(number(path.infinity), false, false, m_minusOne);
            if (!path.side)
                return term(path.point, true, true);
            return path.point == m_zero ? term(number(path.side), false, true, m_one)
                                       : term(path.point, false, true);
        }
        if (key.text == QLatin1String("i") && Settings::instance()->complexNumbers)
            return {LimitForm::NoDomain};
        if (isConstant(key.text) && key.text != QLatin1String("π") && key.text != QLatin1String("e")) {
            const auto value = m_lookupVariable(key.text);
            if (value && (!value->isReal() || value->isNan() || value->isCollection()))
                return {LimitForm::NoDomain};
        }
        return term(expression, true, true);
    case Opcode::Neg: {
        auto result = at(0);
        if (result.form == LimitForm::Term || result.form == LimitForm::Wave || result.form == LimitForm::Divergent)
            result.coefficient = negate(result.coefficient);
        return result;
    }
    case Opcode::Add: case Opcode::Sub:
        return limitSum(expression, path);
    case Opcode::Mul: {
        auto result = at(0);
        for (int i = 1; i < key.children.size(); ++i)
            result = limitProduct(result, at(i), false, path);
        return result;
    }
    case Opcode::Div:
        return limitQuotient(expression, path);
    case Opcode::Pow:
        return limitPower(expression, path);
    case Opcode::Function:
        return limitFunction(expression, path);
    default:
        return {};
    }
}

Calculus::LimitTerm Calculus::limitSum(Expr expression, LimitPath& path)
{
    const Key& key = node(expression).key;
    QVector<LimitTerm> terms;
    bool analytic = true;
    QVector<Expr> endpoints;
    for (int i = 0; i < key.children.size(); ++i) {
        auto next = limitTerm(key.children.at(i), path);
        if (next.form == LimitForm::NoDomain || next.form == LimitForm::Unknown)
            return next;
        if (next.form == LimitForm::Oscillatory || next.form == LimitForm::Divergent)
            return {};
        if (i == 1 && key.type == Opcode::Sub && next.coefficient != NoExpression)
            next.coefficient = negate(next.coefficient);
        analytic &= next.analytic;
        const auto value = classifyLimit(next, path);
        if (value.status == LimitStatus::Finite)
            endpoints.append(value.expression);
        terms.append(next);
    }

    auto leading = term(m_zero, true, analytic);
    bool cancelled = false;
    for (const auto& next : terms) {
        if (next.constant && next.coefficient == m_zero)
            continue;
        if (leading.constant && leading.coefficient == m_zero) {
            leading = next;
            continue;
        }
        if (leading.form == LimitForm::Vanishing || next.form == LimitForm::Vanishing) {
            auto other = leading.form == LimitForm::Vanishing ? next : leading;
            const auto value = classifyLimit(other, path);
            if (value.status == LimitStatus::Infinite
                || (value.status == LimitStatus::Finite && value.expression != m_zero)) {
                leading = other;
                leading.constant = false;
                continue;
            }
            cancelled = true;
            break;
        }
        const auto relation = compareGrowth(leading, next, path);
        if (!relation || (!*relation && (leading.form != LimitForm::Term || next.form != LimitForm::Term))) {
            cancelled = true;
            break;
        }
        if (*relation) {
            if (*relation < 0)
                leading = next;
            leading.constant = false;
        } else {
            const Expr coefficient = add({leading.coefficient, next.coefficient});
            if (coefficient == m_zero && !(leading.constant && next.constant)) {
                cancelled = true;
                break;
            }
            leading.coefficient = coefficient;
            leading.constant &= next.constant;
        }
    }
    if (!cancelled) {
        leading.analytic = analytic;
        return leading;
    }

    // Domains have been checked on the original operands. Simplification
    // can now cancel identical terms without hiding a domain restriction.
    const Expr reduced = simplify(expression);
    if (reduced != expression) {
        const auto result = limitTerm(reduced, path);
        if (result.form != LimitForm::Unknown && result.form != LimitForm::Vanishing)
            return result;
    }
    const Expr fraction = attempt(CandidateWork, [&] {
        return overCommonDenominator(termsOf(reduced));
    });
    if (fraction != NoExpression && fraction != reduced && fraction != expression) {
        const auto result = limitTerm(fraction, path);
        if (result.form != LimitForm::Unknown && result.form != LimitForm::Vanishing)
            return result;
    }
    const auto parts = termsOf(reduced);
    if (parts.size() == 2) {
        for (int i = 0; i < 2; ++i) {
            const Expr offset = parts.at(i);
            const auto [factor, rest] = coefficientOf(parts.at(1 - i));
            const Key& kernel = node(rest).key;
            if (!node(offset).number || kernel.type != Opcode::Function || kernel.text != QLatin1String("erf"))
                continue;
            const Expr scale = number(factor);
            const bool sum = offset == scale;
            if (!sum && offset != negate(scale))
                continue;
            // erf(u) + 1 = erfc(-u), and erf(u) - 1 = -erfc(u).
            const Expr u = kernel.children.at(0);
            const Expr tail = function(QStringLiteral("erfc"), sum ? negate(u) : u);
            const auto result = limitTerm(multiply({offset, tail}), path);
            if (result.form != LimitForm::Unknown)
                return result;
        }
    }
    if (analytic && !path.infinity && path.side) {
        const auto expanded = taylorTerm(expression, path);
        if (expanded.form != LimitForm::Unknown)
            return expanded;
    }
    if (path.infinity) {
        if (const auto coefficients = scaledPolynomial(reduced, path.variable)) {
            const int degree = int(coefficients->size()) - 1;
            const Expr coefficient = coefficients->constLast();
            return term(path.infinity < 0 && degree % 2 ? negate(coefficient) : coefficient,
                        degree == 0, degree == 0, number(-degree));
        }
    }
    if (key.type == Opcode::Sub) {
        const Expr u = key.children.at(0);
        const Expr v = key.children.at(1);
        if (radicand(u) != NoExpression || radicand(v) != NoExpression) {
            const auto squared = [&](Expr part) {
                const Expr root = radicand(part);
                return root != NoExpression ? root : power(part, number(2));
            };
            const Expr numerator = make(Opcode::Sub, {squared(u), squared(v)});
            const Expr denominator = make(Opcode::Add, {u, v});
            const auto result = limitTerm(make(Opcode::Div, {numerator, denominator}), path);
            if (result.form != LimitForm::NoDomain && result.form != LimitForm::Unknown)
                return result;
        }
    }
    if (endpoints.size() != terms.size())
        return {};
    const Expr endpoint = add(endpoints);
    return endpoint == m_zero ? LimitTerm{LimitForm::Vanishing, NoExpression, NoExpression, NoExpression,
                                          NoExpression, false, analytic}
                              : term(endpoint, false, analytic);
}

Calculus::LimitTerm Calculus::limitProduct(const LimitTerm& left, const LimitTerm& right,
                                          bool quotient, LimitPath& path)
{
    if (left.form == LimitForm::NoDomain || right.form == LimitForm::NoDomain)
        return {LimitForm::NoDomain};
    if (left.form == LimitForm::Unknown || right.form == LimitForm::Unknown)
        return {};
    if (left.form == LimitForm::Oscillatory || right.form == LimitForm::Oscillatory
        || left.form == LimitForm::Divergent || right.form == LimitForm::Divergent) {
        return {};
    }
    if (quotient && right.constant && right.coefficient == m_zero)
        return {LimitForm::NoDomain};
    if (quotient && !termSign(right).value_or(0))
        return {};
    const bool analytic = left.analytic && right.analytic
        && (!quotient || (right.order == m_zero && right.logOrder == m_zero && right.exponent == m_zero));
    if ((left.constant && left.coefficient == m_zero) || (!quotient && right.constant && right.coefficient == m_zero))
        return term(m_zero, true, analytic);
    if (left.form == LimitForm::Vanishing || right.form == LimitForm::Vanishing) {
        const auto& other = left.form == LimitForm::Vanishing ? right : left;
        if (other.form == LimitForm::Vanishing)
            return {LimitForm::Vanishing};
        const auto growth = compareGrowth(other, term(m_one), path);
        return growth && (quotient ? *growth >= 0 : *growth <= 0) ? LimitTerm{LimitForm::Vanishing}
                                                                : LimitTerm{};
    }

    const Expr coefficient = quotient ? divide(left.coefficient, right.coefficient)
                                      : multiply({left.coefficient, right.coefficient});
    const auto combine = [&](Expr a, Expr b) { return quotient ? subtract(a, b) : add({a, b}); };
    auto result = term(coefficient, left.constant && right.constant, analytic, combine(left.order, right.order),
                       combine(left.logOrder, right.logOrder), combine(left.exponent, right.exponent));
    if (left.form == LimitForm::Wave && right.form == LimitForm::Wave) {
        const auto growth = compareGrowth(result, term(m_one), path);
        return growth && *growth < 0 ? LimitTerm{LimitForm::Vanishing} : LimitTerm{};
    }
    if (left.form == LimitForm::Bound || right.form == LimitForm::Bound)
        result.form = LimitForm::Bound;
    else if (left.form == LimitForm::Wave || right.form == LimitForm::Wave)
        result.form = LimitForm::Wave;
    return result;
}

Calculus::LimitTerm Calculus::taylorTerm(Expr expression, LimitPath& path)
{
    if (path.infinity || !path.side || !smoothOnPath(expression, path))
        return {};
    LimitPath fixed{path.variable, path.point, 0, 0, {}};
    Expr differentiated = simplify(expression);
    int factorial = 1;
    constexpr int maxTerms = 8;
    for (int order = 1; order <= maxTerms; ++order) {
        differentiated = derivative(differentiated, path.variable);
        if (differentiated == m_zero)
            return term(m_zero, true, true);
        factorial *= order;
        const auto value = classifyLimit(limitTerm(differentiated, fixed), fixed);
        if (value.status != LimitStatus::Finite)
            return {};
        if (value.expression != m_zero) {
            const int sign = path.side < 0 && order % 2 ? -1 : 1;
            return term(multiply({number(sign, factorial), value.expression}), false, true, number(order));
        }
    }
    return {};
}

bool Calculus::smoothOnPath(Expr expression, LimitPath& path)
{
    spend();
    const Key& key = node(expression).key;
    if (key.type == Opcode::Function) {
        const auto rule = functionRules().constFind(key.text);
        if (rule == functionRules().constEnd())
            return false;
        const Expr argument = key.children.at(0);
        if (rule->rule == Rule::Abs || rule->rule == Rule::Sqrt || rule->rule == Rule::Cbrt) {
            if ((rule->rule == Rule::Abs && Settings::instance()->complexNumbers)
                || !termSign(limitTerm(argument, path)).value_or(0)) {
                return false;
            }
        }
        if (rule->rule == Rule::Arcsin || rule->rule == Rule::Arccos || rule->rule == Rule::Arcosh) {
            const auto offset = [&](Expr end) {
                return termSign(limitTerm(make(Opcode::Sub, {argument, end}), path)).value_or(0);
            };
            if (!offset(m_one) || (rule->rule != Rule::Arcosh && !offset(m_minusOne)))
                return false;
        }
        // The derivative of atan2 needs a continuous choice of branch.
        if (rule->rule == Rule::Arctan2)
            return false;
    }
    if (key.type == Opcode::Pow) {
        const auto exponent = limitTerm(key.children.at(1), path);
        const auto& exact = exponent.constant ? node(exponent.coefficient).number : std::nullopt;
        if ((!exact || exact->denominator != 1 || !exact->numerator.isPositive())
            && !termSign(limitTerm(key.children.at(0), path)).value_or(0)) {
            return false;
        }
    }
    return std::all_of(key.children.cbegin(), key.children.cend(),
                       [&](Expr child) { return smoothOnPath(child, path); });
}

Calculus::LimitTerm Calculus::limitQuotient(Expr expression, LimitPath& path)
{
    const auto& children = node(expression).key.children;
    const auto numerator = limitTerm(children.at(0), path);
    const auto denominator = limitTerm(children.at(1), path);
    const auto direct = limitProduct(numerator, denominator, true, path);
    if (!path.side || direct.form == LimitForm::NoDomain || path.hopitalSteps >= 6)
        return direct;
    if (classifyLimit(direct, path).status != LimitStatus::Unknown)
        return direct;
    const auto a = classifyLimit(numerator, path);
    const auto b = classifyLimit(denominator, path);
    const bool zeroOverZero = a.status == LimitStatus::Finite && a.expression == m_zero
        && b.status == LimitStatus::Finite && b.expression == m_zero;
    const bool infinityOverInfinity = a.status == LimitStatus::Infinite && b.status == LimitStatus::Infinite;
    if ((!zeroOverZero && !infinityOverInfinity)
        || !smoothOnPath(children.at(0), path) || !smoothOnPath(children.at(1), path)) {
        return direct;
    }

    // Both operands are differentiable along this approach. Check that the
    // derivative of the denominator stays nonzero there before using the rule.
    ++path.hopitalSteps;
    const Expr below = derivative(simplify(children.at(1)), path.variable);
    if (!termSign(limitTerm(below, path)).value_or(0))
        return direct;
    const Expr above = derivative(simplify(children.at(0)), path.variable);
    const auto result = classifyLimit(limitTerm(make(Opcode::Div, {above, below}), path), path);
    // The rule gives the limit of the quotient, but does not give its rate.
    if (result.status == LimitStatus::Infinite)
        return {LimitForm::Divergent, number(result.infinity)};
    if (result.status == LimitStatus::Finite) {
        if (result.expression == m_zero)
            return {LimitForm::Vanishing};
        return term(result.expression);
    }
    return direct;
}

Calculus::LimitTerm Calculus::limitPower(Expr expression, LimitPath& path, bool cubeRoot)
{
    const auto& children = node(expression).key.children;
    const Expr base = children.at(0);
    const Expr exponent = children.at(1);
    const auto u = limitTerm(base, path);
    const auto v = limitTerm(exponent, path);
    if (u.form == LimitForm::NoDomain || v.form == LimitForm::NoDomain)
        return {LimitForm::NoDomain};
    if (u.form == LimitForm::Unknown || v.form == LimitForm::Unknown
        || u.form == LimitForm::Oscillatory || v.form == LimitForm::Oscillatory) {
        return {};
    }
    const auto sign = termSign(u);
    const auto endBase = classifyLimit(u, path);
    const auto endPower = classifyLimit(v, path);
    const auto valuePower = [&](Expr value, Expr degree) {
        if (cubeRoot)
            return function(QStringLiteral("cbrt"), value);
        if (degree == number(1, 2) && limitSign(value).value_or(0) > 0)
            return squareRoot(value).first;
        return power(value, degree);
    };
    const auto& exact = v.constant ? node(v.coefficient).number : std::nullopt;
    if (exact) {
        const bool integer = exact->denominator == 1;
        const bool positive = exact->numerator.isPositive();
        if (sign && *sign == 0) {
            return positive ? term(m_zero, true, u.analytic && integer)
                            : LimitTerm{LimitForm::NoDomain};
        }
        if (!integer && !cubeRoot && sign && *sign < 0)
            return {LimitForm::NoDomain};
        if (!positive && !sign)
            return {};
        if (!sign && !integer && !cubeRoot) {
            const Expr reduced = simplify(base);
            const auto [part, degree] = factorPower(reduced);
            if (part == reduced || degree % 2)
                return {};
        }
        const Expr degree = v.coefficient;
        if (degree == m_zero && sign)
            return term(m_one, true, u.analytic && endBase.status == LimitStatus::Finite
                        && endBase.expression != m_zero && limitSign(endBase.expression).value_or(0));
        const bool analytic = u.analytic && ((integer && positive)
            || (endBase.status == LimitStatus::Finite && limitSign(endBase.expression).value_or(0)));
        const bool boundedPower = integer && positive && (u.form == LimitForm::Wave || u.form == LimitForm::Bound);
        if (u.form == LimitForm::Term || boundedPower) {
            auto result = term(valuePower(u.coefficient, degree), u.constant, analytic,
                               multiply({u.order, degree}), multiply({u.logOrder, degree}),
                               multiply({u.exponent, degree}));
            if (boundedPower)
                result.form = LimitForm::Bound;
            return result;
        }
        if (positive && endBase.status == LimitStatus::Finite && endBase.expression == m_zero)
            return {LimitForm::Vanishing};
        return {};
    }

    if (sign && *sign == 0) {
        const auto exponentSign = termSign(v);
        if (!exponentSign)
            return {};
        return *exponentSign > 0 ? term(m_zero, true) : LimitTerm{LimitForm::NoDomain};
    }
    if (!sign || *sign <= 0)
        return {};
    if (endBase.status == LimitStatus::Finite && limitSign(endBase.expression).value_or(0) > 0
        && endPower.status == LimitStatus::Finite) {
        return term(power(endBase.expression, endPower.expression), u.constant && v.constant,
                    u.analytic && v.analytic);
    }
    // The logarithmic rewrite handles 0^0, 1^inf and inf^0 on positive bases.
    const Expr log = make(Opcode::Function, {base}, QStringLiteral("ln"));
    return limitTerm(make(Opcode::Function, {make(Opcode::Mul, {exponent, log})}, QStringLiteral("exp")), path);
}

Calculus::LimitTerm Calculus::limitFunction(Expr expression, LimitPath& path)
{
    const Key& key = node(expression).key;
    const auto rule = functionRules().constFind(key.text);
    const Expr argument = key.children.at(0);
    const auto arg = limitTerm(argument, path);
    if (arg.form == LimitForm::Unknown || arg.form == LimitForm::NoDomain)
        return arg;

    const auto endpoint = classifyLimit(arg, path);
    const auto sign = termSign(arg);
    const auto call = [&](const char* name, Expr child = NoExpression) {
        return make(Opcode::Function, {child == NoExpression ? argument : child}, QLatin1String(name));
    };
    const auto offset = [&](Expr from) {
        return limitTerm(make(Opcode::Sub, {argument, from}), path);
    };
    if (rule == functionRules().constEnd()) {
        if (key.text == QLatin1String("gamma")) {
            if (endpoint.status != LimitStatus::Finite)
                return {};
            const auto& exact = node(endpoint.expression).number;
            if (arg.constant && exact && !exact->numerator.isPositive() && exact->denominator == 1)
                return {LimitForm::NoDomain};
            const bool nonInteger = exact && exact->denominator != 1;
            if (limitSign(endpoint.expression).value_or(0) > 0 || nonInteger)
                return term(function(key.text, endpoint.expression), arg.constant);
            return {};
        }
        // floor and ceil are constant between adjacent integers.
        if (endpoint.status != LimitStatus::Finite)
            return {};
        const auto& exact = node(endpoint.expression).number;
        if (!exact)
            return arg.constant ? term(function(key.text, endpoint.expression), true) : LimitTerm{};
        Integer whole;
        Integer remainder;
        Integer::divide(exact->numerator, exact->denominator, whole, remainder);
        const bool floor = key.text == QLatin1String("floor");
        if (!remainder.isZero()) {
            if ((floor && remainder.isNegative()) || (!floor && remainder.isPositive()))
                whole = whole + (floor ? -1 : 1);
        } else if (!arg.constant) {
            const auto direction = termSign(offset(endpoint.expression));
            if (!direction)
                return {};
            if ((floor && *direction < 0) || (!floor && *direction > 0))
                whole = whole + *direction;
        }
        return term(number(ExactNumber{whole, 1}), true);
    }

    const auto quarterTurn = [&] {
        return divide(reference(QStringLiteral("π")), multiply({number(2), radiansPerAngle()}));
    };
    switch (rule->rule) {
    case Rule::Sqrt: case Rule::Cbrt:
        return limitPower(make(Opcode::Pow, {argument, number(1, rule->rule == Rule::Sqrt ? 2 : 3)}),
                          path, rule->rule == Rule::Cbrt);
    case Rule::Abs:
        if (arg.form == LimitForm::Term) {
            auto result = arg;
            result.coefficient = sign ? (*sign < 0 ? negate(arg.coefficient) : arg.coefficient)
                                      : function(key.text, arg.coefficient);
            result.analytic = arg.analytic && (arg.constant || (endpoint.status == LimitStatus::Finite
                && limitSign(endpoint.expression).value_or(0)));
            return result;
        }
        return arg.form == LimitForm::Vanishing ? arg : LimitTerm{};
    case Rule::Tan: case Rule::Cot: case Rule::Sec: case Rule::Csc: {
        const bool cosineDenominator = rule->rule == Rule::Tan || rule->rule == Rule::Sec;
        if (endpoint.status == LimitStatus::Infinite && smoothOnPath(argument, path)) {
            path.oscillating = true;
            return {LimitForm::Oscillatory};
        }
        const bool reciprocal = rule->rule == Rule::Sec || rule->rule == Rule::Csc;
        const Expr top = reciprocal ? m_one : call(cosineDenominator ? "sin" : "cos");
        return limitTerm(make(Opcode::Div, {top, call(cosineDenominator ? "cos" : "sin")}), path);
    }
    case Rule::Log: {
        const Expr value = key.children.at(1);
        return limitTerm(make(Opcode::Div, {call("ln", value), call("ln")}), path);
    }
    case Rule::Lg: case Rule::Lb:
        return limitTerm(make(Opcode::Div, {call("ln"), call("ln", number(rule->rule == Rule::Lg ? 10 : 2))}), path);
    case Rule::Arctan2: {
        const Expr y = key.children.at(1);
        const auto ordinate = limitTerm(y, path);
        if (ordinate.form == LimitForm::NoDomain)
            return ordinate;
        const auto ySign = termSign(ordinate);
        if (!sign || (*sign <= 0 && !ySign))
            return {};
        if (*sign == 0) {
            if (!*ySign)
                return {LimitForm::NoDomain};
            return term(multiply({number(*ySign), quarterTurn()}), true);
        }
        const Expr principal = call("arctan", make(Opcode::Div, {y, argument}));
        if (*sign > 0)
            return limitTerm(principal, path);
        const Expr turn = multiply({number(*ySign < 0 ? -2 : 2), quarterTurn()});
        return limitTerm(make(Opcode::Add, {turn, principal}), path);
    }
    case Rule::Arcsin: case Rule::Arccos: case Rule::Arcosh: case Rule::Artanh: {
        const bool acosh = rule->rule == Rule::Arcosh;
        const auto above = termSign(offset(acosh ? m_one : m_minusOne));
        const auto below = acosh ? std::optional<int>{-1} : termSign(offset(m_one));
        const bool strict = rule->rule == Rule::Artanh;
        if ((above && (*above < 0 || (strict && !*above)))
            || (below && (*below > 0 || (strict && !*below)))) {
            return {LimitForm::NoDomain};
        }
        if (!above || !below)
            return {};
        if (strict) {
            const Expr a = make(Opcode::Add, {m_one, argument});
            const Expr b = make(Opcode::Sub, {m_one, argument});
            return limitTerm(make(Opcode::Mul, {number(1, 2), call("ln", make(Opcode::Div, {a, b}))}), path);
        }
        break;
    }
    case Rule::Ln: {
        if (sign && *sign <= 0)
            return {LimitForm::NoDomain};
        if (!sign || arg.form != LimitForm::Term)
            return {};
        if (endpoint.status == LimitStatus::Finite && endpoint.expression == m_one)
            return arg.constant ? term(m_zero, true, arg.analytic) : offset(m_one);
        if (endpoint.status == LimitStatus::Finite && endpoint.expression != m_zero)
            break;
        if (arg.exponent == m_zero && arg.order != m_zero)
            return term(negate(arg.order), false, false, m_zero, m_one);
        if (arg.exponent != m_zero) {
            const auto dominant = limitTerm(arg.exponent, path);
            if (dominant.form == LimitForm::Term && dominant.exponent == m_zero
                && limitSign(dominant.order).value_or(0) < 0) {
                return dominant;
            }
        }
        return {};
    }
    default:
        break;
    }

    if (endpoint.status == LimitStatus::Infinite) {
        const Expr direction = number(endpoint.infinity);
        switch (rule->rule) {
        case Rule::Exp: {
            const Expr reduced = simplify(argument);
            const auto [factor, rest] = coefficientOf(reduced);
            const Key& inner = node(rest).key;
            if (inner.type == Opcode::Function && inner.text == QLatin1String("ln"))
                return limitTerm(make(Opcode::Pow, {inner.children.at(0), number(factor)}), path);
            return term(m_one, false, false, m_zero, m_zero, reduced);
        }
        case Rule::Sin: case Rule::Cos: {
            if (!smoothOnPath(argument, path))
                return {};
            path.oscillating = true;
            auto wave = term(m_one);
            wave.form = LimitForm::Wave;
            return wave;
        }
        case Rule::Arctan:
            return term(multiply({direction, quarterTurn()}));
        case Rule::Erf: case Rule::Tanh:
            return term(direction);
        case Rule::Erfc: case Rule::Erfi: {
            const bool complement = rule->rule == Rule::Erfc;
            if (complement && endpoint.infinity < 0)
                return term(number(2));
            if (arg.form != LimitForm::Term)
                return {};
            // erfc(u) and erfi(u) have tails exp(-u^2)/(sqrt(pi)*u)
            // and exp(u^2)/(sqrt(pi)*u), respectively.
            const Expr rootPi = function(QStringLiteral("sqrt"), reference(QStringLiteral("π")));
            const Expr square = errorSquare(simplify(argument));
            const auto tail = term(divide(m_one, rootPi), false, false, m_zero, m_zero,
                                   complement ? negate(square) : square);
            return limitProduct(tail, arg, true, path);
        }
        case Rule::Sinh: case Rule::Cosh:
            return term(number(rule->rule == Rule::Cosh ? 1 : endpoint.infinity, 2), false, false,
                        m_zero, m_zero, multiply({direction, simplify(argument)}));
        case Rule::Arsinh: case Rule::Arcosh: {
            const auto log = call("ln", make(Opcode::Mul, {direction, argument}));
            return limitTerm(make(Opcode::Mul, {direction, log}), path);
        }
        default:
            return {};
        }
    }
    if (endpoint.status != LimitStatus::Finite)
        return rule->rule == Rule::Exp && (arg.form == LimitForm::Term || arg.form == LimitForm::Wave)
            ? term(m_one, false, false, m_zero, m_zero, simplify(argument)) : LimitTerm{};

    const Expr value = endpoint.expression;
    Expr result = function(key.text, value);
    bool analytic = arg.analytic;
    switch (rule->rule) {
    case Rule::Arcsin: case Rule::Arccos: case Rule::Arcosh:
        if (value == m_zero && rule->rule == Rule::Arcsin)
            result = m_zero;
        if (value == m_zero && rule->rule == Rule::Arccos)
            result = quarterTurn();
        if (value == m_one || value == m_minusOne) {
            analytic = false;
            if (rule->rule == Rule::Arcsin)
                result = multiply({value, quarterTurn()});
            else
                result = value == m_one ? m_zero : multiply({number(2), quarterTurn()});
        }
        break;
    case Rule::Sinh: case Rule::Tanh: case Rule::Arsinh:
        if (value == m_zero)
            result = m_zero;
        break;
    case Rule::Cosh:
        if (value == m_zero)
            result = m_one;
        break;
    default:
        break;
    }
    if (result != m_zero || arg.constant)
        return term(result, arg.constant, analytic);

    if (analytic) {
        const auto expanded = taylorTerm(expression, path);
        if (expanded.form != LimitForm::Unknown)
            return expanded;
    }
    // Linearize at a simple zero even if the argument itself has no
    // Taylor series there, as in sin(1/x) at infinity.
    const Expr local = reference(QStringLiteral("$limit"));
    const Expr slope = derivative(call(key.text.toLatin1().constData(), local), node(local).key.text);
    LimitPath fixed{node(local).key.text, value, 0, 0, {}};
    const auto derivativeAtZero = classifyLimit(limitTerm(slope, fixed), fixed);
    if (derivativeAtZero.status == LimitStatus::Finite && limitSign(derivativeAtZero.expression).value_or(0)) {
        const auto delta = value == m_zero ? arg : offset(value);
        return limitProduct(term(derivativeAtZero.expression, true, analytic), delta, false, path);
    }
    return {LimitForm::Vanishing};
}

bool Calculus::canEstimateLimit(Expr expression, LimitPath& path)
{
    spend();
    const auto endpoint = classifyLimit(limitTerm(expression, path), path);
    if (endpoint.status == LimitStatus::Finite)
        return true;
    if (endpoint.status != LimitStatus::Unknown)
        return false;
    const Key& key = node(expression).key;
    // An unresolved calculation that involves growth or an unresolved
    // oscillation can look stable at every scale we can sample.
    return !key.children.isEmpty()
        && std::all_of(key.children.cbegin(), key.children.cend(),
                       [&](Expr child) { return canEstimateLimit(child, path); });
}

std::optional<Calculus::LimitEstimate> Calculus::estimateLimit(Expr raw, const LimitPoint& point, LimitPath& path)
{
    if (!path.side || path.oscillating || (!point.infinity && point.numeric.isNan())
        || !canEstimateLimit(raw, path)) {
        return std::nullopt;
    }
    const int precision = Settings::instance()->resultPrecision;
    const int digits = qBound(12, precision < 0 ? 24 : precision, 32);
    const HNumber tolerance = HMath::raise(HNumber(10), -digits);
    const HNumber scale = point.infinity ? HNumber(1) : qMax(HNumber(1), HMath::abs(point.numeric));
    // Use independent grids and extra working digits on the finer check.
    // Extrapolate in the distance to the point, or in 1/|x| at infinity.
    const auto sampleGrid = [&](const HNumber& offset, int extraDigits) -> std::optional<LimitEstimate> {
        const HMath::ScopedExtraPrecision precision(extraDigits);
        constexpr int window = 4;
        constexpr int maxSamples = 12;
        const HNumber noise = HMath::raise(HNumber(10), 6 - DECPRECISION);
        QVector<HNumber> distances;
        QVector<HNumber> samples;
        HNumber previousPoint = HMath::nan();
        HNumber previousError = HMath::nan();
        HNumber previousEstimate = HMath::nan();
        bool varied = false;
        int stable = 0;
        HNumber step = offset;
        for (int i = 0; i < maxSamples; ++i, step /= HNumber(100)) {
            spend();
            const HNumber x = point.infinity ? HNumber(point.infinity) / step
                                             : point.numeric + HNumber(path.side) * scale * step;
            if (x.isNan() || (!point.infinity && x == point.numeric))
                return std::nullopt;
            if (!previousPoint.isNan()) {
                HNumber phaseWidth(0);
                try {
                    enclosure(raw, path.variable, Interval{qMin(previousPoint, x), qMax(previousPoint, x)},
                              &phaseWidth, true);
                } catch (const DomainUnknown&) {
                    return std::nullopt;
                }
                if (phaseWidth > HMath::pi() / HNumber(2))
                    return std::nullopt;
            }
            previousPoint = x;
            const HNumber value = magnitude(raw, path.variable, x);
            if (value.isNan())
                return std::nullopt;
            varied |= !samples.isEmpty() && value != samples.constLast();
            samples.append(value);
            distances.append(step);
            if (samples.size() < window)
                continue;

            QVector<HNumber> fitted = samples;
            for (int n = 1; n < window; ++n) {
                for (int j = 0; j + n < window; ++j) {
                    fitted[j] = (distances.at(j) * fitted.at(j + 1) - distances.at(j + n) * fitted.at(j))
                        / (distances.at(j) - distances.at(j + n));
                }
            }
            samples.removeFirst();
            distances.removeFirst();
            const HNumber estimate = fitted.constFirst();
            if (estimate.isNan())
                return std::nullopt;
            if (!previousEstimate.isNan()) {
                const HNumber error = HMath::abs(estimate - previousEstimate);
                const HNumber size = qMax(HNumber(1), HMath::abs(estimate));
                const HNumber roundoff = qMax(HMath::abs(estimate), HMath::abs(previousEstimate)) * noise;
                const bool contracts = !previousError.isNan()
                    && (error * HNumber(4) < previousError || (error <= roundoff && previousError <= roundoff));
                stable = contracts ? stable + 1 : 0;
                const HNumber budget = size * tolerance;
                const HNumber observed = qMax(previousError.isNan() ? error : previousError, error);
                const HNumber uncertainty = HNumber(4) * observed + roundoff;
                if (varied && stable >= 2 && uncertainty <= budget)
                    return LimitEstimate{estimate, uncertainty};
                previousError = error;
            }
            previousEstimate = estimate;
        }
        return std::nullopt;
    };
    const auto coarse = sampleGrid(HNumber("0.01"), 6);
    if (!coarse)
        return std::nullopt;
    const auto fine = sampleGrid(HNumber("0.0037"), 14);
    if (!fine)
        return std::nullopt;
    const HNumber disagreement = HMath::abs(coarse->value - fine->value);
    if (disagreement > coarse->error + fine->error)
        return std::nullopt;
    const HNumber value = (coarse->value + fine->value) / HNumber(2);
    // Only display digits supported by the requested error target.
    const auto format = HNumber::Format::Scientific() + HNumber::Format::Point() + HNumber::Format::Precision(digits);
    const HNumber rounded(HMath::format(value, format).toLatin1().constData());
    if (rounded.isNan())
        return std::nullopt;
    return LimitEstimate{rounded, coarse->error + fine->error + disagreement + HMath::abs(rounded - value)};
}

std::optional<int> Calculus::readDirection(Span span)
{
    const int length = span.end - span.first;
    if (length > MaxScalarOpcodes)
        limitExceeded();
    if (!canEvaluateScalar(span))
        return std::nullopt;
    const Quantity value = m_evalSpan(span.first, length, {}, Quantity());
    if (value.isApproximate() || !value.isInteger())
        return std::nullopt;
    const HNumber direction = value.numericValue().real;
    if (direction < -1 || direction > 1)
        return std::nullopt;
    return direction.toInt();
}

Expr Calculus::attachLimitUnits(Expr expression, Quantity units)
{
    if (!units.hasDimension())
        return expression;
    Units::findUnit(units);
    if (units.unitName().isEmpty() || !units.unit().isReal() || units.unit().real <= 0)
        throw Failure{tr("could not express the limit's units")};
    const Expr unit = make(Opcode::UnitRef, {}, units.unitName());
    Quantity scale(units.unit());
    scale.copyDimension(units);
    m_limitUnits.insert(unit, scale);
    // Keep the unit on zero and infinity as well.
    const Expr scalar = units.unit().real == 1 ? expression : divide(expression, scalarLiteral(units.unit().real));
    return make(Opcode::Mul, {scalar, unit});
}

// Work in coherent units and check dimensions before doing scalar algebra.
Expr Calculus::stripLimitUnits(Expr expression, const QString& variable, const Quantity& variableUnits,
                               Quantity& units, bool& approximate)
{
    spend();
    const Key& key = node(expression).key;
    if (key.type == Opcode::Ref && key.text == variable) {
        units = variableUnits;
        return expression;
    }
    if (key.type == Opcode::UnitRef) {
        Quantity unit;
        const auto generated = m_limitUnits.constFind(expression);
        if (generated != m_limitUnits.constEnd())
            unit = generated.value();
        else if (!m_lookupUnit(key.text, &unit))
            throw Failure{tr("unknown unit %1").arg(key.text.toHtmlEscaped())};
        if (!unit.isReal() || unit.isNan())
            throw Failure{tr("limit units must have a real scale")};
        approximate |= unit.isApproximate();
        units.copyDimension(unit);
        return scalarLiteral(unit.numericValue().real);
    }
    if (key.children.isEmpty())
        return expression;

    QVector<Expr> children;
    QVector<Quantity> dimensions;
    for (Expr child : key.children) {
        Quantity dimension(1);
        children.append(stripLimitUnits(child, variable, variableUnits, dimension, approximate));
        dimensions.append(dimension);
    }
    units = dimensions.constFirst();
    switch (key.type) {
    case Opcode::Add: case Opcode::Sub:
        for (const auto& dimension : dimensions) {
            if (!dimension.sameDimension(units))
                throw Failure{tr("incompatible units in limit expression")};
        }
        break;
    case Opcode::Div:
        units /= dimensions.at(1);
        break;
    case Opcode::Unit: case Opcode::Mul:
        for (int i = 1; i < dimensions.size(); ++i)
            units *= dimensions.at(i);
        break;
    case Opcode::Pow:
        if (dimensions.at(1).hasDimension())
            throw Failure{tr("limit exponents must be dimensionless")};
        if (units.hasDimension()) {
            const auto& exponent = node(simplify(children.at(1))).number;
            if (!exponent || abs(exponent->numerator) > MaxNodes || exponent->denominator > MaxNodes)
                throw Failure{tr("unit-bearing powers need a constant rational exponent")};
            units = DMath::raise(units, Quantity(exponent->numerator.toHNumber() / exponent->denominator.toHNumber()));
            if (units.isNan())
                throw Failure{tr("unsupported power of units in limit expression")};
        }
        break;
    case Opcode::Function:
        if (key.text == QLatin1String("sqrt") || key.text == QLatin1String("cbrt")) {
            units = key.text == QLatin1String("sqrt") ? DMath::sqrt(units) : DMath::cbrt(units);
        } else if (key.text != QLatin1String("abs")) {
            for (const auto& dimension : dimensions) {
                if (dimension.hasDimension())
                    throw Failure{tr("this function needs dimensionless arguments in a limit")};
            }
        }
        break;
    default:
        break;
    }
    return make(key.type == Opcode::Unit ? Opcode::Mul : key.type, children, key.text);
}

Calculus::LimitPoint Calculus::readLimitPoint(Span span, int nesting)
{
    if (span.end - span.first > MaxScalarOpcodes)
        limitExceeded();
    LimitPoint point;
    point.infinity = namedInfinity(span);
    if (point.infinity)
        return point;

    const Expr raw = readExpression(span, nesting, &point.approximate);
    const Key& key = node(raw).key;
    if (key.type == Opcode::Ref && key.text == QString(UnicodeChars::Infinity)) {
        point.infinity = 1;
        return point;
    }
    const bool negative = key.type == Opcode::Neg || (key.type == Opcode::Mul && key.children.size() == 2
        && key.children.constFirst() == m_minusOne);
    if (negative && node(key.children.constLast()).key.text == QString(UnicodeChars::Infinity)) {
        point.infinity = -1;
        return point;
    }
    QSet<QString> variables;
    infer(raw, variables, true);
    Expr expression = raw;
    for (const QString& name : variables) {
        const auto saved = m_lookupVariable(name);
        if (!saved)
            continue;
        if (!saved->isReal() || saved->isNan() || saved->isCollection())
            throw Failure{tr("the approach point must be a real scalar")};
        if (Units::isExplicitAngleUnitName(saved->unitName()) || Units::isAffineUnitName(saved->unitName()))
            throw Failure{tr("use linear units and the active angle mode for the approach point")};
        expression = substitute(expression, reference(name),
                                attachLimitUnits(scalarLiteral(saved->numericValue().real), *saved), false);
        point.approximate |= saved->isApproximate();
    }

    const Quantity scalar = canEvaluateScalar(span)
        ? m_evalSpan(span.first, span.end - span.first, {}, Quantity()) : DMath::nan();
    if (Units::isExplicitAngleUnitName(scalar.unitName()) || Units::isAffineUnitName(scalar.unitName()))
        throw Failure{tr("use linear units and the active angle mode for the approach point")};
    expression = stripLimitUnits(expression, {}, Quantity(1), point.units, point.approximate);
    LimitPath fixed{{}, NoExpression, 0, 0, {}};
    const LimitValue value = classifyLimit(limitTerm(expression, fixed), fixed);
    if (value.status == LimitStatus::Unknown)
        throw Failure{tr("unable to determine limit (could not establish a real approach point)")};
    if (value.status != LimitStatus::Finite)
        throw Failure{tr("invalid approach point (expected a defined real value)")};
    point.expression = value.expression;
    point.numeric = magnitude(point.expression);
    return point;
}

CalculusResult Calculus::limit(int first, int nesting)
{
    if (nesting >= MaxDepth)
        limitExceeded();
    const QVector<Span> arguments = argumentsFor(first);
    bool approximate = false;
    const Expr source = readExpression(arguments.at(0), nesting, &approximate);
    QSet<QString> variables;
    infer(source, variables, true);
    const QString inferred = variables.size() == 1 ? *variables.constBegin() : QString();

    QString variable;
    int pointIndex = 1;
    int direction = 0;
    if (arguments.size() >= 3) {
        variable = namedInfinity(arguments.at(1)) ? QString() : variableArgument(arguments.at(1));
        if (arguments.size() == 4 && variable.isEmpty())
            throw Failure{tr("expected a variable name as the second argument")};
        if (variable.isEmpty() || arguments.size() == 4) {
            const auto specified = readDirection(arguments.constLast());
            if (!specified)
                throw Failure{tr("invalid direction (expected -1, 0 or 1 without units)")};
            direction = *specified;
        } else if (variables.size() <= 1) {
            const auto specified = readDirection(arguments.constLast());
            if (specified) {
                const auto saved = m_lookupVariable(variable);
                const auto measure = inferred.isEmpty() ? std::nullopt : m_lookupVariable(inferred);
                const bool implicitPoint = saved
                    ? saved->isReal() && !saved->isNan() && !saved->isCollection()
                        && !Units::isExplicitAngleUnitName(saved->unitName())
                        && !Units::isAffineUnitName(saved->unitName())
                        && (!measure || measure->isNan() || measure->isSymbolic() || measure->isCollection()
                            || saved->sameDimension(*measure))
                    : variable != inferred && (!measure || !measure->hasDimension());
                if (implicitPoint) {
                    if (!saved || !saved->hasDimension()) {
                        throw Failure{tr("ambiguous arguments (use four arguments or write the approach point as an expression)")};
                    }
                    variable.clear();
                    direction = *specified;
                }
            }
        }
        if (!variable.isEmpty())
            pointIndex = 2;
    }
    if (variable.isEmpty()) {
        if (variables.size() > 1)
            throw Failure{tr("ambiguous variable (specify the limiting variable)")};
        variable = inferred;
    }

    LimitPoint point = readLimitPoint(arguments.at(pointIndex), nesting);
    if (point.infinity && direction == point.infinity)
        throw Failure{tr("incompatible direction for an infinite approach point")};
    if (!point.infinity && !variable.isEmpty() && dependsOn(point.expression, variable))
        throw Failure{tr("the approach point depends on the limiting variable")};
    if (!variable.isEmpty()) {
        const auto saved = m_lookupVariable(variable);
        if (saved && !saved->isNan() && !saved->isSymbolic() && !saved->isCollection()) {
            if (point.infinity)
                point.units.copyDimension(*saved);
            else if (!point.units.sameDimension(*saved))
                throw Failure{tr("the limiting variable and approach point need compatible units")};
        }
    }

    Quantity units(1);
    approximate |= point.approximate && variables.contains(variable);
    const Expr raw = stripLimitUnits(source, variable, point.units, units, approximate);
    LimitPath left{variable, point.expression, point.infinity, -1, {}};
    LimitPath right{variable, point.expression, point.infinity, 1, {}};
    if (point.infinity)
        direction = -point.infinity;
    const auto evaluate = [&](LimitPath& path) {
        if (variable.isEmpty())
            path.side = 0;
        return classifyLimit(limitTerm(raw, path), path);
    };
    const LimitValue a = direction > 0 ? LimitValue{} : evaluate(left);
    const LimitValue b = direction < 0 ? LimitValue{} : evaluate(right);
    if (a.status == LimitStatus::DoesNotExist || b.status == LimitStatus::DoesNotExist)
        throw Failure{tr("limit does not exist")};

    LimitValue result = direction > 0 ? b : a;
    if (!direction) {
        if (a.status == LimitStatus::Unknown || b.status == LimitStatus::Unknown) {
            result = {};
        } else if (a.status != b.status || a.infinity != b.infinity) {
            throw Failure{tr("limit does not exist")};
        } else if (a.status == LimitStatus::Finite && a.expression != b.expression) {
            const Expr difference = simplified(subtract(a.expression, b.expression), variable);
            if (difference != m_zero) {
                if (limitSign(difference).value_or(0))
                    throw Failure{tr("limit does not exist")};
                result = {};
            }
        }
    }
    if (result.status == LimitStatus::Infinite) {
        const Expr infinity = reference(QString(UnicodeChars::Infinity));
        return attachLimitUnits(result.infinity < 0 ? negate(infinity) : infinity, units);
    }
    const auto numericResult = [&](const HNumber& value, bool approximation) {
        Quantity result(value);
        result.copyDimension(units);
        if (approximation)
            result.markApproximate();
        return result;
    };
    if (result.status == LimitStatus::Finite) {
        const Expr exact = simplified(result.expression, variable);
        if (!approximate) {
            if (units.hasDimension() && nesting == 0) {
                const auto& scalar = node(exact).number;
                if (scalar && scalar->denominator == 1 && scalar->numerator.digits() <= DECPRECISION)
                    return numericResult(scalar->numerator.toHNumber(), false);
            }
            return attachLimitUnits(exact, units);
        }
        const HNumber value = magnitude(exact);
        if (!value.isNan())
            return numericResult(value, true);
    } else if (variables.isEmpty() || (variables.size() == 1 && variables.contains(variable))) {
        const auto estimate = [&](const LimitValue& known, LimitPath& path) -> std::optional<LimitEstimate> {
            if (known.status == LimitStatus::Finite) {
                const HNumber value = magnitude(known.expression);
                if (!value.isNan())
                    return LimitEstimate{value, 0};
            }
            return known.status == LimitStatus::Unknown ? estimateLimit(raw, point, path) : std::nullopt;
        };
        const auto l = direction <= 0 ? estimate(a, left) : std::nullopt;
        const auto r = direction >= 0 ? estimate(b, right) : std::nullopt;
        if (direction < 0 && l)
            return numericResult(l->value, true);
        if (direction > 0 && r)
            return numericResult(r->value, true);
        if (!direction && l && r && HMath::abs(l->value - r->value) <= l->error + r->error)
            return numericResult((l->value + r->value) / HNumber(2), true);
    }
    throw Failure{tr("unable to determine limit")};
}

CalculusResult Calculus::integrate(int first, int nesting)
{
    if (nesting >= MaxDepth)
        limitExceeded();
    const QVector<Span> arguments = argumentsFor(first);
    const bool definite = arguments.size() >= 3;
    const bool explicitVariable = arguments.size() == 2 || arguments.size() == 4;
    QString variable = explicitVariable ? variableArgument(arguments.at(1)) : QString();
    if (explicitVariable && variable.isEmpty())
        throw Failure{tr("expected a variable name as the second argument")};

    const Span source = arguments.at(0);
    const Expr raw = readExpression(source, nesting);
    QSet<QString> variables;
    infer(raw, variables);
    if (variable.isEmpty()) {
        if (variables.size() > 1)
            throw Failure{tr("ambiguous variable (specify which variable to integrate)")};
        variable = variables.isEmpty() ? QStringLiteral("x") : *variables.constBegin();
    }
    const Expr expression = simplify(raw);
    if (!definite) {
        const Expr primitive = antiderivative(expression, variable);
        if (primitive == NoExpression)
            throw Failure{tr("symbolic integration is unsupported for this expression")};
        // Not simplified() like derivatives, which would factor polynomial
        // antiderivatives and make them harder to read.
        return primitive;
    }

    Bound lower = readBound(arguments.at(arguments.size() - 2), nesting);
    Bound upper = readBound(arguments.constLast(), nesting);
    bool reversed = lower.infinity > upper.infinity;
    const bool infinite = lower.infinity || upper.infinity;
    const bool bothNumeric = !lower.value.isNan() && !upper.value.isNan();
    if (bothNumeric) {
        if (!lower.value.sameDimension(upper.value))
            throw Failure{tr("integration bounds need compatible units")};
        reversed = lower.value > upper.value;
    }
    if (reversed)
        std::swap(lower, upper);
    const bool dimensioned = lower.value.hasDimension() || upper.value.hasDimension()
        || lower.value.hasUnit() || upper.value.hasUnit();
    if (dimensioned && !bothNumeric)
        throw Failure{tr("unit-bearing bounds need two compatible numeric values")};
    if (Units::isExplicitAngleUnitName(lower.value.unitName())
        || Units::isExplicitAngleUnitName(upper.value.unitName())) {
        throw Failure{tr("use the active angle mode for integration bounds")};
    }
    if (Units::isAffineUnitName(lower.value.unitName()) || Units::isAffineUnitName(upper.value.unitName()))
        throw Failure{tr("units with an offset are unsupported in integration bounds")};
    if (infinite && (lower.infinity == upper.infinity
        || (!lower.infinity && lower.value.isNan()) || (!upper.infinity && upper.value.isNan()))) {
        throw Failure{tr("invalid infinite integration bounds")};
    }

    const bool scalarProgram = canEvaluateScalar(source);
    const auto evaluateAt = [&](const HNumber& x) {
        Quantity argument(x);
        if (dimensioned)
            argument.copyDimension(lower.value);
        return m_evalSpan(source.first, source.end - source.first, variable, argument);
    };
    const auto validSample = [](const Quantity& value) {
        return !value.isNan() && !value.isSymbolic() && !value.isCollection() && value.isReal();
    };
    const auto withUnits = [&](const HNumber& value, const Quantity& model) {
        Quantity integral(value);
        integral.copyDimension(model);
        Quantity measure(1);
        if (dimensioned)
            measure.copyDimension(lower.value);
        return integral * measure;
    };

    std::optional<Interval> interval;
    HNumber phaseWidth(0);
    if (bothNumeric) {
        interval = Interval{lower.value.numericValue().real, upper.value.numericValue().real};
        if (interval->lower == interval->upper && scalarProgram) {
            const Quantity value = evaluateAt(interval->lower);
            if (validSample(value))
                return withUnits(0, value);
            // A symbolic coefficient may prevent sampling a regular expression.
            if (dimensioned || node(raw).containsUnit || variables.size() <= 1)
                throw Failure{tr("invalid integrand at the integration bound")};
        }
    }

    const Expr primitive = antiderivative(expression, variable);
    const Expr x = reference(variable);
    Endpoint endpoint;
    HNumber tailError(0);
    bool mirrored = false;
    const int precision = Settings::instance()->resultPrecision;
    const int digits = qBound(28, precision < 0 ? 34 : qMin(precision, DECPRECISION) + 6, DECPRECISION - 12);
    const HNumber tolerance = HMath::raise(HNumber(10), -digits);

    if (infinite) {
        const Expr exact = infiniteLimit(raw, primitive, variable, lower, upper);
        if (exact != NoExpression)
            return reversed ? negate(exact) : exact;

        // A Gaussian has a positive, bounded tail on either side of its center.
        const Key& kernel = node(expression).key;
        if (kernel.type != Opcode::Function || kernel.text != QLatin1String("exp"))
            throw Failure{tr("could not establish convergence for the infinite bounds")};
        const Expr exponent = kernel.children.at(0);
        const Expr firstDerivative = derivative(exponent, variable);
        const Expr secondDerivative = derivative(firstDerivative, variable);
        const HNumber curvature = magnitude(secondDerivative);
        if (dependsOn(secondDerivative, variable) || curvature.isNan() || curvature >= 0) {
            throw Failure{tr("could not establish convergence for the infinite bounds")};
        }
        verifyDomain(raw, variable, std::nullopt, nullptr);
        const HNumber decay = -curvature / HNumber(2);
        const HNumber center = -magnitude(firstDerivative, variable, 0) / curvature;
        const HNumber height = HMath::exp(magnitude(exponent, variable, center));
        const HNumber radius = HMath::sqrt(HNumber(digits + 8) * HMath::ln(HNumber(10)) / decay);
        if (center.isNan() || height.isNan() || radius.isNan() || radius.isZero())
            throw Failure{tr("could not establish convergence for the infinite bounds")};

        const auto tail = [&](const HNumber& distance) {
            return height * HMath::exp(-decay * distance * distance) / (HNumber(2) * decay * distance);
        };
        HNumber lo = lower.infinity ? center - radius : lower.value.numericValue().real;
        HNumber hi = upper.infinity ? center + radius : upper.value.numericValue().real;
        if (lower.infinity) {
            lo = qMin(lo, hi - radius);
            tailError += tail(center - lo);
        }
        if (upper.infinity) {
            hi = qMax(hi, lo + radius);
            tailError += tail(hi - center);
        }
        interval = Interval{lo, hi};
        // A Gaussian is symmetric about its center, so over the whole line
        // one half is enough.
        if (lower.infinity && upper.infinity) {
            interval->lower = center;
            mirrored = true;
        }
    } else {
        if (interval)
            endpoint = verifyPowerIntegral(raw, variable, *interval, &phaseWidth);
        const int logEndpoint = interval && primitive != NoExpression
            ? logarithmEndpoint(raw, variable, *interval) : 0;
        if (!endpoint.side && !logEndpoint && (primitive != NoExpression || !interval))
            verifyDomain(raw, variable, interval, &phaseWidth);
        if (primitive != NoExpression) {
            const Expr a = logEndpoint < 0 ? m_zero : substitute(primitive, x, lower.expression);
            const Expr b = logEndpoint > 0 ? m_zero : substitute(primitive, x, upper.expression);
            const Expr difference = reversed ? subtract(a, b) : subtract(b, a);
            if (!dimensioned && !node(raw).containsUnit && !lower.value.isApproximate()
                && !upper.value.isApproximate()) {
                return difference;
            }
            if (interval && scalarProgram) {
                const Quantity model = evaluateAt(interval->lower + (interval->upper - interval->lower) / HNumber(2));
                const HNumber value = magnitude(difference);
                if (!validSample(model))
                    throw Failure{tr("invalid integrand or incompatible units")};
                if (!value.isNan()) {
                    Quantity integral = withUnits(value, model);
                    if (model.isApproximate() || lower.value.isApproximate() || upper.value.isApproximate())
                        integral.markApproximate();
                    return integral;
                }
            }
        }
    }

    if (!interval || !scalarProgram)
        throw Failure{tr("symbolic integration is unsupported and numerical integration needs numeric inputs")};
    if (interval->lower == interval->upper)
        throw Failure{tr("invalid integrand at the integration bound")};
    if (endpoint.side && !endpoint.substitutionPower)
        throw Failure{tr("numerical integration cannot handle this endpoint singularity")};

    const Quantity model = evaluateAt(interval->lower + (interval->upper - interval->lower) / HNumber(2));
    if (!validSample(model))
        throw Failure{tr("invalid integrand or incompatible units")};
    if (!infinite) {
        const QScopedValueRollback<bool> sampling(m_samplingDomain, true);
        if (endpoint.side)
            verifyPowerIntegral(raw, variable, *interval, &phaseWidth);
        else
            verifyDomain(raw, variable, interval, &phaseWidth);
    }
    const HNumber panels = HMath::ceil(phaseWidth / HMath::pi());
    if (panels.isNan() || panels > 64)
        throw Failure{tr("numerical integration limit exceeded for a rapidly varying integrand")};
    int sampleWork = 240000;
    const HNumber width = interval->upper - interval->lower;
    const Interval quadrature = endpoint.side ? Interval{0, 1} : *interval;
    const auto result = NumericalIntegration::integrate(
        quadrature.lower, quadrature.upper, tolerance / HNumber(2), qMax(2, panels.toInt()),
        [&](const HNumber& point) {
            sampleWork -= source.end - source.first;
            if (sampleWork < 0 || m_evaluator.shouldStopForTimeLimit())
                return HMath::nan(TooExpensive);
            HNumber samplePoint = point;
            HNumber jacobian(1);
            if (endpoint.side) {
                const HNumber offset = width * HMath::raise(point, endpoint.substitutionPower);
                samplePoint = endpoint.side < 0 ? interval->lower + offset : interval->upper - offset;
                jacobian = width * HNumber(endpoint.substitutionPower)
                    * HMath::raise(point, endpoint.substitutionPower - 1);
            }
            const Quantity sample = evaluateAt(samplePoint);
            if (!validSample(sample) || !sample.sameDimension(model))
                return HMath::nan();
            return sample.numericValue().real * jacobian;
        });
    if (result.status == NumericalIntegration::Result::InvalidSample)
        throw Failure{tr("invalid integrand value or incompatible units")};
    const HNumber halves(mirrored ? 2 : 1);
    const HNumber integral = halves * result.value;
    const HNumber integralError = halves * result.error;
    if (result.status != NumericalIntegration::Result::Converged || tailError.isNan()
        || (!tailError.isZero() && tailError + integralError > tolerance * HMath::abs(integral))) {
        throw Failure{tr("numerical integration did not converge within the calculation limits")};
    }
    Quantity value = withUnits(reversed ? -integral : integral, model);
    value.markApproximate();
    return value;
}

}

bool SymbolicCalculus::isNamedInfinity(const QString& name)
{
    return name.compare(QLatin1String("inf"), Qt::CaseInsensitive) == 0
        || name.compare(QLatin1String("infinity"), Qt::CaseInsensitive) == 0;
}

Quantity SymbolicCalculus::evaluate(const Evaluator& evaluator, const QVector<Opcode>& opcodes,
                                      int first, const QVector<Quantity>& constants,
                                      const QStringList& identifiers, const EvaluateSpan& evalSpan,
                                      const UnitLookup& lookupUnit, const VariableLookup& lookupVariable,
                                      QString* error)
{
    try {
        return Calculus(evaluator, opcodes, constants, identifiers, evalSpan,
                        lookupUnit, lookupVariable).evaluate(first);
    } catch (const Failure& failure) {
        *error = failure.message;
        return DMath::nan();
    }
}

void SymbolicCalculus::refineRule3(const Evaluator& evaluator, const QVector<Opcode>& opcodes,
                                 int first, int end, const QVector<Quantity>& constants,
                                 const QStringList& identifiers, const QVector<Quantity>& arguments,
                                 const UnitLookup& lookupUnit, const VariableLookup& lookupVariable,
                                 Quantity& result)
{
    if (result.isZero() || result.isInfinite()
        || std::any_of(arguments.cbegin(), arguments.cend(), [](const Quantity& arg) {
            return arg.isApproximate() || arg.isInfinite();
        })) {
        return;
    }
    if (!result.isApproximate() && result.isInteger() && end - first == 3
        && std::all_of(opcodes.cbegin() + first, opcodes.cbegin() + end, [](const Opcode& opcode) {
            return opcode.type == Opcode::Load;
        })) {
        return;
    }

    try {
        const EvaluateSpan evaluateSpan;
        Calculus(evaluator, opcodes, constants, identifiers, evaluateSpan,
                 lookupUnit, lookupVariable).refineRule3(first, end, result);
    } catch (const Failure&) {
        // The numeric result already uses the real values of the arguments.
    }
}
