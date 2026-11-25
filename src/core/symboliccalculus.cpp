// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/symboliccalculus.h"

#include "core/anglemode.h"
#include "core/evaluator.h"
#include "core/numericalintegration.h"
#include "core/settings.h"
#include "core/unicodechars.h"
#include "core/units.h"

#include <QCoreApplication>
#include <QMap>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QStringList>
#include <deque>
#include <optional>
#include <variant>

namespace {

constexpr int MaxDepth = 96;
constexpr int MaxNodes = 4096;
constexpr int MaxWork = 60000;
constexpr int MaxOrderSteps = 256;
constexpr int MaxScalarOpcodes = 128;

struct Failure {
    QString message;
};

// Keep room for cross products and their sum in the numeric engine.
constexpr int ExactDigits = 36;
static_assert(2 * ExactDigits + 1 < DECPRECISION);

struct ExactNumber {
    HNumber numerator = 0;
    HNumber denominator = 1;
};

const HNumber& exactLimit()
{
    static const HNumber limit = HMath::raise(HNumber(10), ExactDigits);
    return limit;
}

std::optional<ExactNumber> fraction(HNumber numerator, HNumber denominator)
{
    if (denominator.isZero())
        return std::nullopt;
    if (denominator.isNegative()) {
        numerator = -numerator;
        denominator = -denominator;
    }
    if (denominator != 1) {
        const HNumber gcd = HMath::gcd(HMath::abs(numerator), denominator);
        numerator = HMath::idiv(numerator, gcd);
        denominator = HMath::idiv(denominator, gcd);
    }
    if (HMath::abs(numerator) >= exactLimit() || denominator >= exactLimit())
        return std::nullopt;
    return ExactNumber{numerator, denominator};
}

std::optional<ExactNumber> exactLiteral(const QString& text)
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
    if (scale <= -ExactDigits || length + qMax(qint64(0), scale) > ExactDigits)
        return std::nullopt;

    digits = digits.mid(first, length);
    if (scale > 0)
        digits += QString(scale, QLatin1Char('0'));
    const QString denominator = scale < 0
        ? QStringLiteral("1") + QString(-scale, QLatin1Char('0')) : QStringLiteral("1");
    return fraction(HNumber(digits.toLatin1().constData()), HNumber(denominator.toLatin1().constData()));
}

using Expr = int;
using IntegralResult = std::variant<Expr, Quantity>;
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
    }

    Quantity evaluate(int first)
    {
        if (m_opcodes.at(first).type == Opcode::Integrate) {
            const IntegralResult result = integrate(first, 0);
            if (const auto* numeric = std::get_if<Quantity>(&result))
                return *numeric;
            return resultValue(std::get<Expr>(result));
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

private:
    Quantity resultValue(Expr result)
    {
        const auto& exact = node(result).number;
        if (exact && exact->denominator == 1)
            return Quantity(exact->numerator);
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

    IntegralResult integrate(int first, int nesting);
    Bound readBound(Span span, int nesting);
    Expr antiderivative(Expr expression, const QString& variable, int depth = 0);
    std::optional<PowerKernel> powerKernel(Expr expression);
    std::optional<PowerKernel> scaledPower(Expr expression, Expr& coefficient);
    std::optional<SquareOffset> squareWithPositiveOffset(Expr expression);
    std::optional<QVector<Expr>> polynomial(Expr expression, const QString& variable);
    Expr integratePolynomial(const QVector<Expr>& coefficients, Expr variable);
    Expr powerPrimitive(const PowerKernel& kernel);
    Expr kernelPrimitive(Expr expression, Expr& argument);
    bool dependsOn(Expr expression, const QString& variable);
    Expr substitute(Expr expression, Expr variable, Expr value);
    HNumber magnitude(Expr expression, const QString& variable = {}, const HNumber& x = 0);
    std::optional<Interval> enclosure(Expr expression, const QString& variable,
                                      const std::optional<Interval>& interval, HNumber* phaseWidth);
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
    Expr m_zero;
    Expr m_one;

    [[noreturn]] static void limitExceeded()
    {
        throw Failure{tr("symbolic calculus limit exceeded")};
    }

    void spend(int work = 1)
    {
        m_remainingWork -= work;
        if (m_remainingWork < 0)
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
        for (Expr child : children) {
            const Node& operand = node(child);
            size += operand.size;
            depth = qMax(depth, operand.depth + 1);
            containsUnit = containsUnit || operand.containsUnit;
        }
        if (size > MaxNodes || depth > MaxDepth)
            limitExceeded();

        const Expr expression = static_cast<int>(m_nodes.size());
        m_nodes.push_back({key, exact, size, depth, containsUnit});
        m_interned.insert(key, expression);
        return expression;
    }

    Expr number(const ExactNumber& value)
    {
        const auto format = HNumber::Format::Fixed() + HNumber::Format::Point()
            + HNumber::Format::Precision(0);
        QString text = HMath::format(value.numerator, format);
        if (value.denominator != 1)
            text += QLatin1Char('/') + HMath::format(value.denominator, format);
        return make(Opcode::Load, {}, text, value);
    }

    Expr number(int value, int denominator = 1)
    {
        return number(ExactNumber{HNumber(value), HNumber(denominator)});
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
        if (count < 1 || count > (m_opcodes.at(first).type == Opcode::Diff ? 3u : 4u))
            throw Failure{tr("wrong number of arguments")};
        if (end - first > MaxNodes)
            limitExceeded();

        QVector<Span> arguments;
        for (int pc = first + 1; pc < end; ++pc) {
            spend();
            const Opcode& opcode = m_opcodes.at(pc);
            if (opcode.type == Opcode::Nop)
                continue;
            if (opcode.type == Opcode::Diff || opcode.type == Opcode::Integrate) {
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
        if (arguments.size() != count)
            throw Failure{tr("invalid expression")};

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

    Expr readExpression(Span span, int nesting)
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
                const int end = callEnd(pc, span.end);
                const Call inner = readCall(pc);
                stack.append(inner.order.isZero() ? readExpression(inner.expression, nesting + 1)
                                                  : differentiate(inner, nesting + 1));
                pc = end;
                break;
            }
            case Opcode::Integrate: {
                const int end = callEnd(pc, span.end);
                const IntegralResult inner = integrate(pc, nesting + 1);
                const auto* expression = std::get_if<Expr>(&inner);
                if (!expression)
                    throw Failure{tr("the nested integral needs a symbolic result")};
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
                auto exact = exactLiteral(text);
                static const QRegularExpression integerWithBase(
                    QStringLiteral(R"(^0[boxd][0-9A-Fa-f]+$)"),
                    QRegularExpression::CaseInsensitiveOption);
                if (!exact && text.size() <= ExactDigits && value.isInteger()
                    && integerWithBase.match(text).hasMatch()) {
                    exact = fraction(value.numericValue().real, HNumber(1));
                }
                stack.append(exact ? number(*exact) : make(Opcode::Load, {}, text));
                break;
            }
            case Opcode::Ref: {
                QString name = m_identifiers.value(opcode.index);
                if (name == QLatin1String("ans")) {
                    stack.append(lastAnswer());
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
                const auto rule = functionRules().constFind(identifier);
                if (rule == functionRules().constEnd())
                    throw Failure{tr("unsupported function %1").arg(name.toHtmlEscaped())};
                if (rule->arity != count - 1)
                    throw Failure{tr("wrong number of arguments to %1").arg(name.toHtmlEscaped())};
                stack[first] = make(Opcode::Function, stack.sliced(first + 1, count - 1), identifier);
                stack.resize(first + 1);
                break;
            }
            default:
                throw Failure{tr("unsupported expression in symbolic differentiation")};
            }
        }
        if (stack.size() != 1)
            throw Failure{tr("invalid expression")};
        return stack.constLast();
    }

    void infer(Expr expression, QSet<QString>& variables)
    {
        spend();
        const Key& key = node(expression).key;
        if (key.type == Opcode::Ref) {
            if (key.text == QString(UnicodeChars::Infinity))
                throw Failure{tr("infinity is only supported as an integration bound")};
            if (isVariable(key.text))
                variables.insert(key.text);
            else if (!isConstant(key.text) || m_evaluator.getVariable(key.text).value().isSymbolic())
                throw Failure{tr("unsupported symbol %1").arg(key.text.toHtmlEscaped())};
        }
        for (Expr child : key.children)
            infer(child, variables);
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
        return expression;
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

        ExactNumber coefficient{1, 1};
        QVector<Expr> product;
        QMap<Expr, int> powers;
        for (Expr factor : terms) {
            const Node& value = node(factor);
            if (factor == m_zero)
                return m_zero;
            if (value.number) {
                const auto combined = fraction(coefficient.numerator * value.number->numerator,
                                               coefficient.denominator * value.number->denominator);
                if (combined)
                    coefficient = *combined;
                else
                    product.append(factor);
                continue;
            }
            const auto [base, exponent] = factorPower(factor);
            powers[base] += exponent;
        }
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
            Expr base = term;
            ExactNumber coefficient{1, 1};
            const Node& value = node(term);
            if (value.number) {
                coefficient = *value.number;
                base = m_one;
            } else if (value.key.type == Opcode::Mul) {
                const auto& scalar = node(value.key.children.at(0)).number;
                if (scalar) {
                    coefficient = *scalar;
                    const auto rest = value.key.children.sliced(1);
                    base = rest.size() == 1 ? rest.at(0) : make(Opcode::Mul, rest);
                }
            }
            auto existing = coefficients.find(base);
            if (existing == coefficients.end()) {
                bases.append(base);
                coefficients.insert(base, coefficient);
            } else {
                const auto combined = fraction(
                    existing->numerator * coefficient.denominator + coefficient.numerator * existing->denominator,
                    existing->denominator * coefficient.denominator);
                if (combined)
                    existing.value() = *combined;
                else
                    sum.append(term);
            }
        }
        for (Expr base : bases) {
            const Expr coefficient = number(coefficients.value(base));
            if (coefficient != m_zero)
                sum.append(multiply({coefficient, base}));
        }
        if (sum.isEmpty())
            return m_zero;
        return sum.size() == 1 ? sum.at(0) : make(Opcode::Add, sum);
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
        bool cancelled = false;
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
        if (value && exp && exp->denominator == 1 && HMath::abs(exp->numerator) <= MaxOrderSteps) {
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
            return number(ExactNumber{HMath::abs(exact->numerator), exact->denominator});
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
        if (name == QLatin1String("arctan") && exact) {
            if (argument == m_zero)
                return m_zero;
            if (exact->denominator == 1 && HMath::abs(exact->numerator) == 1) {
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
                const HNumber quarterTurns = multiplier.numerator * HNumber(2) / multiplier.denominator;
                if (quarterTurns.isInteger()) {
                    const int quadrant = ((quarterTurns % 4).toInt() + 4) % 4;
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
            if (key.text == QLatin1String("log"))
                return divide(function(QStringLiteral("ln"), second), function(QStringLiteral("ln"), first));
            if (children.size() == 1)
                return function(key.text, first);
            return make(key.type, children, key.text);
        default: return make(key.type, children, key.text);
        }
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
            const Expr dv = derivative(v, variable);
            if (dv == m_zero)
                return divide(du, v);
            return divide(subtract(multiply({du, v}), multiply({u, dv})), power(v, number(2)));
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
        Arcsin, Arccos, Arctan, Arctan2, Sinh, Cosh, Tanh, Arsinh, Arcosh, Artanh
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
            {QStringLiteral("artanh"), {Rule::Artanh}}
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
            const bool leadingMinus = coefficient && coefficient->numerator == -1 && coefficient->denominator == 1;
            if (leadingMinus)
                text += QLatin1Char('-');
            const int start = leadingMinus ? 1 : 0;
            for (int i = start; i < children.size(); ++i) {
                if (i > start)
                    text += QLatin1Char('*');
                render(children.at(i), i > start && negative(children.at(i)) ? 4 : 2, text);
            }
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
    spend();
    const Key& key = node(expression).key;
    if (key.type == Opcode::Ref)
        return key.text == variable;
    for (Expr child : key.children) {
        if (dependsOn(child, variable))
            return true;
    }
    return false;
}

Expr Calculus::substitute(Expr expression, Expr variable, Expr value)
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
        children.append(substitute(child, variable, value));
    return rebuild(key, children);
}

std::optional<Calculus::PowerKernel> Calculus::powerKernel(Expr expression)
{
    const Key& key = node(expression).key;
    if (key.type == Opcode::Pow) {
        const Expr exponent = simplify(key.children.at(1));
        if (node(exponent).number)
            return PowerKernel{key.children.at(0), exponent};
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
    constexpr int maxDegree = 24;
    spend();
    if (!dependsOn(expression, variable))
        return QVector<Expr>{expression};

    const Key& key = node(expression).key;
    if (key.type == Opcode::Ref)
        return QVector<Expr>{m_zero, m_one};

    const auto product = [&](const QVector<Expr>& left, const QVector<Expr>& right) {
        const int count = left.size() + right.size() - 1;
        if (count > maxDegree + 1)
            return QVector<Expr>{};
        QVector<Expr> coefficients(count, m_zero);
        for (int i = 0; i < left.size(); ++i) {
            for (int j = 0; j < right.size(); ++j)
                coefficients[i + j] = add({coefficients.at(i + j), multiply({left.at(i), right.at(j)})});
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
                    coefficients[i] = add({coefficients.at(i), part->at(i)});
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
        QVector<Expr> coefficients{m_one};
        for (int i = 0; i < exponent->numerator.toInt(); ++i) {
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
    default: return NoExpression;
    }
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
        return value.number ? value.number->numerator / value.number->denominator
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
    HNumber* phaseWidth)
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
    const auto at = [&](int i) { return enclosure(key.children.at(i), variable, interval, phaseWidth); };
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
    case Rule::Tanh: case Rule::Arsinh: case Rule::Arctan:
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

Calculus::Bound Calculus::readBound(Span span, int nesting)
{
    const int length = span.end - span.first;
    if (length > MaxScalarOpcodes)
        limitExceeded();
    const Opcode& first = m_opcodes.at(span.first);
    if (first.type == Opcode::Ref && (length == 1
        || (length == 2 && m_opcodes.at(span.first + 1).type == Opcode::Neg))) {
        const QString name = m_identifiers.value(first.index);
        if (name == QString(UnicodeChars::Infinity)
            || (SymbolicCalculus::isNamedInfinity(name) && !m_lookupVariable(name))) {
            return {NoExpression, DMath::nan(), length == 1 ? 1 : -1};
        }
    }

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
        const HNumber denominator = node(kernel->exponent).number->denominator;
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
    const Bound& finite = lower.infinity ? upper : lower;
    const int direction = lower.infinity ? -1 : 1;
    const Expr x = reference(variable);
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

IntegralResult Calculus::integrate(int first, int nesting)
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
        quadrature.lower, quadrature.upper, tolerance / HNumber(2), qMax(8, panels.toInt()),
        [&](const HNumber& point) {
            sampleWork -= source.end - source.first;
            if (sampleWork < 0)
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
    if (result.status != NumericalIntegration::Result::Converged || tailError.isNan()
        || (!tailError.isZero() && tailError + result.error > tolerance * HMath::abs(result.value))) {
        throw Failure{tr("numerical integration did not converge within the calculation limits")};
    }
    Quantity value = withUnits(reversed ? -result.value : result.value, model);
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
