// This file is part of the SpeedCrunch project
// Copyright (C) 2004 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2005, 2006 Johan Thelin <e8johan@gmail.com>
// Copyright (C) 2007-2016 @heldercorreia
// Copyright (C) 2009 Wolf Lammen <ookami1@gmx.de>
// Copyright (C) 2014 Tey <teyut@free.fr>
// Copyright (C) 2015 Pol Welter <polwelter@gmail.com>
// Copyright (C) 2015 Hadrien Theveneau <theveneau@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; see the file COPYING.  If not, write to
// the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
// Boston, MA 02110-1301, USA.

#include "core/evaluator.h"
#include "core/constants.h"
#include "core/regexpatterns.h"
#include "core/session.h"
#include "core/settings.h"
#include "core/unicodechars.h"
#include "math/rational.h"
#include "math/units.h"

#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>
#include <QStack>
#include <cmath>
#include <limits>

#define ALLOW_IMPLICIT_MULT

static constexpr int MAX_PRECEDENCE = INT_MAX;
static constexpr int INVALID_PRECEDENCE = INT_MIN;

static bool isSubscriptDigit(QChar ch);
static bool isSubscriptLetter(QChar ch);
static bool isIdentifierStart(QChar ch);
static bool isIdentifierContinue(QChar ch);
static bool isSuperscriptDigit(QChar ch);
static QString superscriptDigitsToAscii(const QString& text);

#ifdef EVALUATOR_DEBUG
#include <QDebug>
#include <QFile>
#include <QTextStream>

QTextStream& operator<<(QTextStream& s, Quantity num)
{
    s << DMath::format(num, Quantity::Format::Fixed());
    return s;
}
#endif // EVALUATOR_DEBUG

static Evaluator* s_evaluatorInstance = 0;

static void s_deleteEvaluator()
{
    delete s_evaluatorInstance;
}

static bool s_isSigmaFunctionIdentifier(const QString& identifier)
{
    return FunctionRepo::instance()->isIdentifierAliasOf(identifier, QStringLiteral("sigma"));
}

static QString s_toSuperscriptExponent(int exponent)
{
    static const QHash<QChar, QChar> superscriptDigits = {
        { QLatin1Char('0'), QChar(0x2070) }, // ⁰
        { QLatin1Char('1'), QChar(0x00B9) }, // ¹
        { QLatin1Char('2'), QChar(0x00B2) }, // ²
        { QLatin1Char('3'), QChar(0x00B3) }, // ³
        { QLatin1Char('4'), QChar(0x2074) }, // ⁴
        { QLatin1Char('5'), QChar(0x2075) }, // ⁵
        { QLatin1Char('6'), QChar(0x2076) }, // ⁶
        { QLatin1Char('7'), QChar(0x2077) }, // ⁷
        { QLatin1Char('8'), QChar(0x2078) }, // ⁸
        { QLatin1Char('9'), QChar(0x2079) }  // ⁹
    };

    QString result;
    if (exponent < 0)
        result += QChar(0x207B); // ⁻
    for (const QChar& digit : QString::number(qAbs(exponent)))
        result += superscriptDigits.value(digit, digit);
    return result;
}

static QString s_renderUnitExponentsAsSuperscripts(QString text)
{
    static const QRegularExpression caretExponentRe(
        QStringLiteral(R"(\^\(([−-]?\d+)\)|\^([−-]?\d+))"));

    int offset = 0;
    while (true) {
        const QRegularExpressionMatch match = caretExponentRe.match(text, offset);
        if (!match.hasMatch())
            break;

        QString expText = match.captured(1);
        if (expText.isEmpty())
            expText = match.captured(2);
        expText.replace(QChar(0x2212), QLatin1Char('-')); // Unicode minus.
        bool ok = false;
        const int exponent = expText.toInt(&ok);
        if (!ok) {
            offset = match.capturedEnd();
            continue;
        }

        text.replace(match.capturedStart(), match.capturedLength(),
                     s_toSuperscriptExponent(exponent));
        offset = match.capturedStart() + 1;
    }
    return text;
}

static bool splitUserFunctionDescription(const QString& expression,
                                         QString* expressionWithoutDescription,
                                         QString* description)
{
    const int equalsPos = expression.indexOf('=');
    if (equalsPos < 0)
        return false;

    const QString leftSide = expression.left(equalsPos);
    if (!leftSide.contains('(') || !leftSide.contains(')'))
        return false;

    int depth = 0;
    const int n = expression.size();
    for (int i = equalsPos + 1; i < n; ++i) {
        const QChar ch = expression.at(i);
        if (ch == '(') {
            ++depth;
        } else if (ch == ')' && depth > 0) {
            --depth;
        } else if (ch == '?' && depth == 0) {
            *expressionWithoutDescription = expression.left(i).trimmed();
            *description = expression.mid(i + 1).trimmed();
            return true;
        }
    }

    return false;
}

static bool splitVariableDescription(const QString& expression,
                                     QString* expressionWithoutDescription,
                                     QString* description)
{
    const int equalsPos = expression.indexOf('=');
    if (equalsPos < 0)
        return false;

    const QString leftSide = expression.left(equalsPos).trimmed();
    if (leftSide.contains('(') || leftSide.contains(')'))
        return false;

    int depth = 0;
    const int n = expression.size();
    for (int i = equalsPos + 1; i < n; ++i) {
        const QChar ch = expression.at(i);
        if (ch == '(') {
            ++depth;
        } else if (ch == ')' && depth > 0) {
            --depth;
        } else if (ch == '?' && depth == 0) {
            *expressionWithoutDescription = expression.left(i).trimmed();
            *description = expression.mid(i + 1).trimmed();
            return true;
        }
    }

    return false;
}

static int findTopLevelCommentDelimiterPos(const QString& text)
{
    int depth = 0;
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('(')) {
            ++depth;
            continue;
        }
        if (ch == QLatin1Char(')')) {
            if (depth > 0)
                --depth;
            continue;
        }
        if (ch != QLatin1Char('?') || depth != 0)
            continue;

        const bool hasLeftWhitespace = i > 0 && text.at(i - 1).isSpace();
        const bool hasRightWhitespace = i + 1 >= text.size() || text.at(i + 1).isSpace();
        if (hasLeftWhitespace && hasRightWhitespace)
            return i;
    }
    return -1;
}

static bool s_isSubtractionOperatorAlias(const QChar& ch)
{
    return ch == QLatin1Char('-') || ch == UnicodeChars::MinusSign;
}

static bool s_isMultiplicationOperatorAlias(const QChar& ch, bool keepDotOperator = false)
{
    switch (ch.unicode()) {
    case '*':
    case 0x00D7: // × MULTIPLICATION SIGN
        return true;
    case UnicodeChars::DotOperator.unicode():
        return !keepDotOperator;
    default:
        return false;
    }
}

static bool s_isExpressionOperatorOrSeparator(const QChar& ch)
{
    return ch == QLatin1Char('+')
           || ch == UnicodeChars::MinusSign
           || ch == UnicodeChars::DotOperator
           || ch == QLatin1Char('/')
           || ch == QLatin1Char('%')
           || ch == QLatin1Char('^')
           || ch == QLatin1Char('&')
           || ch == QLatin1Char('|')
           || ch == QLatin1Char('=')
           || ch == QLatin1Char('>')
           || ch == QLatin1Char('<')
           || ch == QLatin1Char(';')
           || ch == QLatin1Char(',');
}

static bool s_expressionWithoutIgnorableTrailingToken(const QString& text, QString* out)
{
    if (!out)
        return false;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return false;

    int i = trimmed.size() - 1;
    const QChar last = trimmed.at(i);
    const bool isPlusMinusTail =
        (last == QLatin1Char('+') || s_isSubtractionOperatorAlias(last));
    const bool isMultiplicationTail =
        (last == UnicodeChars::DotOperator || s_isMultiplicationOperatorAlias(last, true));

    if (last != QLatin1Char('(')
        && !isPlusMinusTail
        && !isMultiplicationTail
        && last != QLatin1Char(';')
        && last != QLatin1Char('/')
        && last != QLatin1Char('^')
        && last != QLatin1Char('\\'))
        return false;

    if (last == QLatin1Char('/')) {
        --i;
        if (i >= 0 && trimmed.at(i) == QLatin1Char('/'))
            return false;
    } else if (last == QLatin1Char('^')) {
        --i;
        if (i >= 0 && trimmed.at(i) == QLatin1Char('^'))
            return false;
    } else if (isPlusMinusTail) {
        while (i >= 0 && (trimmed.at(i) == QLatin1Char('+')
                          || s_isSubtractionOperatorAlias(trimmed.at(i))))
            --i;
    } else if (isMultiplicationTail) {
        --i;
        if (i >= 0) {
            const QChar prev = trimmed.at(i);
            const bool prevIsMultiplication =
                (prev == UnicodeChars::DotOperator
                 || s_isMultiplicationOperatorAlias(prev, true));
            if (prevIsMultiplication) {
                --i;
                if (i >= 0) {
                    const QChar prevPrev = trimmed.at(i);
                    const bool prevPrevIsMultiplication =
                        (prevPrev == UnicodeChars::DotOperator
                         || s_isMultiplicationOperatorAlias(prevPrev, true));
                    if (prevPrevIsMultiplication)
                        return false;
                }
            }
        }
    } else {
        while (i >= 0 && trimmed.at(i) == last)
            --i;
    }

    const QString prefix = trimmed.left(i + 1).trimmed();
    if (prefix.isEmpty())
        return false;

    const QChar prefixLast = prefix.at(prefix.size() - 1);
    if (prefixLast == QLatin1Char('(')
        || s_isExpressionOperatorOrSeparator(prefixLast)
        || prefixLast == QLatin1Char('\\'))
        return false;

    *out = prefix;
    return true;
}

bool isMinus(const QChar& ch)
{
    return ch == QLatin1Char('-') || ch == UnicodeChars::MinusSign;
}

static bool isDegreeSign(const QChar& ch)
{
    // Accept both DEGREE SIGN (U+00B0) and MASCULINE ORDINAL INDICATOR
    // (U+00BA), plus other degree-like symbols emitted by some layouts/IMEs.
    return ch == UnicodeChars::DegreeSign  // ° DEGREE SIGN
           || ch == QChar(0x00BA) // º MASCULINE ORDINAL INDICATOR
           || ch == QChar(0x02DA) // ˚ RING ABOVE
           || ch == QChar(0x2218); // ∘ RING OPERATOR
}

bool isExponent(const QChar& ch, int base)
{
    switch (base) {
        case 2:
            return ch == QLatin1Char('b') || ch == QLatin1Char('B');
        case 8:
            return ch == QLatin1Char('o') || ch == QLatin1Char('O') || ch == QLatin1Char('C');
        case 10:
            return ch == QLatin1Char('e') || ch == QLatin1Char('E');
        case 16:
            return ch == QLatin1Char('h') || ch == QLatin1Char('H');
        default:
            return false;
    }
}

const Quantity& Evaluator::checkOperatorResult(const Quantity& n)
{
    switch (n.error()) {
    case Success: break;
    case NoOperand:
        if (!m_assignFunc)
            // The arguments are still NaN, so ignore this error.
            m_error = Evaluator::tr("cannot operate on a NaN");
        break;
    case Underflow:
        m_error = Evaluator::tr("underflow - tiny result is out "
                                "of SpeedCrunch's number range");
        break;
    case Overflow:
        m_error = Evaluator::tr("overflow - huge result is out of "
                                "SpeedCrunch's number range");
        break;
    case ZeroDivide:
        m_error = Evaluator::tr("division by zero");
        break;
    case OutOfLogicRange:
        if (!(m_assignFunc && !m_assignArg.isEmpty())) {
            m_error = Evaluator::tr("overflow - logic result exceeds "
                                    "maximum of 256 bits");
        }
        break;
    case OutOfIntegerRange:
        if (!(m_assignFunc && !m_assignArg.isEmpty())) {
            m_error = Evaluator::tr("overflow - integer result exceeds "
                                    "maximum limit for integers");
        }
        break;
    case TooExpensive:
        m_error = Evaluator::tr("too time consuming - "
                                "computation was rejected");
        break;
    case DimensionMismatch:
        // We cannot make any assumptions about the dimension of the arguments,
        // so ignore this error when assigning a function.
        if (!m_assignFunc)
            m_error = Evaluator::tr("dimension mismatch - quantities with "
                                    "different dimensions cannot be "
                                    "compared, added, etc.");
        break;
    case InvalidDimension:
        m_error = Evaluator::tr("invalid dimension - operation might "
                                "require dimensionless arguments");
        break;
    case EvalUnstable:
        m_error = Evaluator::tr("Computation aborted - encountered "
                                "numerical instability");
        break;
    default:;
    }

    return n;
}

QString Evaluator::stringFromFunctionError(Function* function)
{
    if (!function->error())
        return QString();

    QString result = QString::fromLatin1("<b>%1</b>: ");

    switch (function->error()) {
    case Success: break;
    case InvalidParamCount:
        result += Evaluator::tr("wrong number of arguments");
        break;
    case NoOperand:
        result += Evaluator::tr("does not take NaN as an argument");
        break;
    case Overflow:
        result += Evaluator::tr("overflow - huge result is out of "
                                "SpeedCrunch's number range");
        break;
    case Underflow:
        result += Evaluator::tr("underflow - tiny result is out of "
                                "SpeedCrunch's number range");
        break;
    case OutOfLogicRange:
        result += Evaluator::tr("overflow - logic result exceeds "
                                "maximum of 256 bits");
        break;
    case OutOfIntegerRange:
        result += Evaluator::tr("result out of range");
        break;
    case ZeroDivide:
        result += Evaluator::tr("division by zero");
        break;
    case EvalUnstable:
        result += Evaluator::tr("Computation aborted - "
                                "encountered numerical instability");
        break;
    case OutOfDomain:
        result += Evaluator::tr("undefined for argument domain");
        break;
    case TooExpensive:
        result += Evaluator::tr("computation too expensive");
        break;
    case InvalidDimension:
        result += Evaluator::tr("invalid dimension - function "
                                "might require dimensionless arguments");
        break;
    case DimensionMismatch:
        result += Evaluator::tr("dimension mismatch - quantities with "
                                "different dimensions cannot be compared, "
                                "added, etc.");
        break;
    case IONoBase:
    case BadLiteral:
    case InvalidPrecision:
    case InvalidParam:
        result += Evaluator::tr("internal error, please report a bug");
        break;
    default:
        result += Evaluator::tr("error");
        break;
    };

    return result.arg(function->identifier());
}

class TokenStack : public QVector<Token> {
public:
    TokenStack();
    bool isEmpty() const;
    unsigned itemCount() const;
    Token pop();
    void push(const Token& token);
    const Token& top();
    const Token& top(unsigned index);
    bool hasError() const { return !m_error.isEmpty(); }
    QString error() const { return m_error; }
    void reduce(int count, int minPrecedence = INVALID_PRECEDENCE);
    void reduce(int count, Token&& top, int minPrecedence = INVALID_PRECEDENCE);
    void reduce(QList<Token> tokens, Token&& top,
                int minPrecedence = INVALID_PRECEDENCE);
private:
    void ensureSpace();
    int topIndex;
    QString m_error;
};

const Token Token::null;

static Token::Operator matchOperator(const QString& text)
{
    Token::Operator result = Token::Invalid;

    if (text.length() == 1) {
        QChar p = text.at(0);
        switch(p.unicode()) {
        case '+':
            result = Token::Addition;
            break;
        case 0x2212: // − MINUS SIGN
        case '-':
            result = Token::Subtraction;
            break;
        case 0x00D7: // × MULTIPLICATION SIGN
        case UnicodeChars::DotOperator.unicode():
        case '*':
            result = Token::Multiplication;
            break;
        case 0x00F7: // ÷ DIVISION SIGN
        case 0x29F8: // ⧸ BIG SOLIDUS
        case '/':
            result = Token::Division;
            break;
        case '^':
            result = Token::Exponentiation;
            break;
        case ';':
            result = Token::ListSeparator;
            break;
        case '(':
            result = Token::AssociationStart;
            break;
        case ')':
            result = Token::AssociationEnd;
            break;
        case '!':
            result = Token::Factorial;
            break;
        case '%':
            result = Token::Percent;
            break;
        case '=':
            result = Token::Assignment;
            break;
        case '\\':
            result = Token::IntegerDivision;
            break;
        case '~':
            result = Token::BitwiseLogicalNOT;
            break;
        case '&':
            result = Token::BitwiseLogicalAND;
            break;
        case '|':
            result = Token::BitwiseLogicalOR;
            break;
        case UnicodeChars::RightwardsArrow.unicode():
            result = Token::UnitConversion;
            break;
        default:
            result = Token::Invalid;
        }

    } else if (text.length() == 2) {
        if (text == "**")
            result = Token::Exponentiation;
        else if (text == "<<")
          result = Token::ArithmeticLeftShift;
        else if (text == ">>")
          result = Token::ArithmeticRightShift;
        else if (text == "->"
                 || (text.at(0).unicode() == 0x2212 && text.at(1) == QLatin1Char('>'))
                 || text.compare(QStringLiteral("in"), Qt::CaseInsensitive) == 0)
            result = Token::UnitConversion;
    }

   return result;
}

// Helper function: give operator precedence e.g. "+" is 300 while "*" is 500.
static int opPrecedence(Token::Operator op)
{
    int prec;
    switch (op) {
    case Token::Factorial:
    case Token::Percent:
        prec = 800;
        break;
    case Token::Exponentiation:
        prec = 700;
        break;
    case Token::Function:
        // Not really operator but needed for managing shift/reduce conflicts.
        prec = 600;
        break;
    case Token::Multiplication:
    case Token::Division:
        prec = 500;
        break;
    case Token::Modulo:
    case Token::IntegerDivision:
        prec = 600;
        break;
    case Token::Addition:
    case Token::Subtraction:
        prec = 300;
        break;
    case Token::ArithmeticLeftShift:
    case Token::ArithmeticRightShift:
        prec = 200;
        break;
    case Token::BitwiseLogicalNOT:
        prec = 150;
        break;
    case Token::BitwiseLogicalAND:
        prec = 100;
        break;
    case Token::BitwiseLogicalOR:
        prec = 50;
        break;
    case Token::UnitConversion:
        prec = 0;
        break;
    case Token::AssociationEnd:
    case Token::ListSeparator:
        prec = -100;
        break;
    case Token::AssociationStart:
        prec = -200;
        break;
    default:
        prec = -200;
        break;
    }
    return prec;
}

static int opcodePrecedence(Opcode::Type opcodeType)
{
    switch (opcodeType) {
    case Opcode::Add:
    case Opcode::Sub:
        return opPrecedence(Token::Addition);
    case Opcode::Mul:
    case Opcode::Div:
        return opPrecedence(Token::Multiplication);
    case Opcode::Pow:
        return opPrecedence(Token::Exponentiation);
    case Opcode::Fact:
        return opPrecedence(Token::Factorial);
    case Opcode::Percent:
        return opPrecedence(Token::Percent);
    case Opcode::Modulo:
    case Opcode::IntDiv:
        return opPrecedence(Token::Modulo);
    case Opcode::LSh:
    case Opcode::RSh:
        return opPrecedence(Token::ArithmeticLeftShift);
    case Opcode::BAnd:
        return opPrecedence(Token::BitwiseLogicalAND);
    case Opcode::BOr:
        return opPrecedence(Token::BitwiseLogicalOR);
    case Opcode::Conv:
        return opPrecedence(Token::UnitConversion);
    case Opcode::Neg:
    case Opcode::BNot:
        return opPrecedence(Token::Multiplication);
    default:
        return MAX_PRECEDENCE;
    }
}

static bool isRightAssociativeOpcode(Opcode::Type opcodeType)
{
    return opcodeType == Opcode::Pow;
}

static QString opcodeToInfixSymbol(Opcode::Type opcodeType, bool implicitMultiplication = false)
{
    switch (opcodeType) {
    case Opcode::Add: return "+";
    case Opcode::Sub: return "-";
    case Opcode::Mul:
        return implicitMultiplication
            ? QString(UnicodeChars::DotOperator)
            : QString(UnicodeChars::MultiplicationSign);
    case Opcode::Div: return "/";
    case Opcode::Pow: return "^";
    case Opcode::Percent: return "%";
    case Opcode::Modulo: return "mod";
    case Opcode::IntDiv: return "\\";
    case Opcode::LSh: return "<<";
    case Opcode::RSh: return ">>";
    case Opcode::BAnd: return "&";
    case Opcode::BOr: return "|";
    case Opcode::Conv: return QString(UnicodeChars::RightwardsArrow);
    default: return QString();
    }
}

Token::Token(Type type, const QString& text, int pos, int size)
{
    m_type = type;
    m_text = text;
    m_pos = pos;
    m_size = size;
    m_minPrecedence = MAX_PRECEDENCE;
}

Token::Token(const Token& token)
{
    m_type = token.m_type;
    m_text = token.m_text;
    m_pos = token.m_pos;
    m_size = token.m_size;
    m_minPrecedence = token.m_minPrecedence;
}

Token& Token::operator=(const Token& token)
{
    m_type = token.m_type;
    m_text = token.m_text;
    m_pos = token.m_pos;
    m_size = token.m_size;
    m_minPrecedence = token.m_minPrecedence;
    return*this;
}

Quantity Token::asNumber() const
{
    QString text = m_text;
    return isNumber() ? Quantity(CNumber((const char*)text.toLatin1()))
                      : Quantity(0);
}

Token::Operator Token::asOperator() const
{
    return isOperator() ? matchOperator(m_text) : Invalid;
}

QString Token::description() const
{
    QString desc;

    switch (m_type) {
    case stxNumber:
        desc = "Number";
        break;
    case stxIdentifier:
        desc = "Identifier";
        break;
    case stxOpenPar:
    case stxClosePar:
    case stxSep:
    case stxOperator:
        desc = "Operator";
        break;
    default:
        desc = "Unknown";
        break;
    }

    while (desc.length() < 10)
        desc.prepend(' ');

    QString header;
    header.append(QString::number(m_pos) + ","
                  + QString::number(m_pos + m_size - 1));
    header.append("," + (m_minPrecedence == MAX_PRECEDENCE ?
                             "MAX" : QString::number(m_minPrecedence)));
    header.append("  ");

    while (header.length() < 10)
        header.append(' ');

    desc.prepend(header);
    desc.append(" : ").append(m_text);

    return desc;
}

static bool tokenPositionCompare(const Token& a, const Token& b)
{
    return (a.pos() < b.pos());
}

static bool tokenCanEndOperandForDisplaySpacing(const Token& token)
{
    return token.isOperand()
        || token.asOperator() == Token::AssociationEnd
        || token.asOperator() == Token::Factorial
        || token.asOperator() == Token::Percent;
}

static bool tokenCanStartOperandForDisplaySpacing(const Token& token)
{
    return token.isOperand()
        || token.asOperator() == Token::AssociationStart
        || token.asOperator() == Token::Addition
        || token.asOperator() == Token::Subtraction
        || token.asOperator() == Token::BitwiseLogicalNOT;
}

static bool isImaginaryUnitTokenForDisplay(const Token& token)
{
    if (!token.isIdentifier())
        return false;

    const QString text = token.text();
    if (text == QLatin1String("i") || text == QLatin1String("j"))
        return true;

    const QChar configuredImaginaryUnit(Settings::instance()->imaginaryUnit);
    return text.size() == 1 && text.at(0) == configuredImaginaryUnit;
}

static bool startsWithImaginaryLiteralForDisplay(const Tokens& tokens, int startIndex)
{
    if (startIndex < 0 || startIndex >= tokens.size())
        return false;

    if (isImaginaryUnitTokenForDisplay(tokens.at(startIndex)))
        return true;

    if (startIndex + 2 >= tokens.size())
        return false;

    const Token::Operator mulOp = tokens.at(startIndex + 1).asOperator();
    return tokens.at(startIndex).isNumber()
        && mulOp == Token::Multiplication
        && isImaginaryUnitTokenForDisplay(tokens.at(startIndex + 2));
}

static bool isUnsignedDecimalIntegerText(const QString& text)
{
    return RegExpPatterns::unsignedDecimalInteger().match(text).hasMatch();
}

static bool isUnsignedDecimalNumberText(const QString& text)
{
    return RegExpPatterns::unsignedDecimalNumber().match(text).hasMatch();
}

static bool isSignedDecimalNumberText(const QString& text)
{
    return RegExpPatterns::signedDecimalNumber().match(text).hasMatch();
}

static bool isSafeForDoubleArithmetic(const QString& text)
{
    if (!isSignedDecimalNumberText(text))
        return false;

    int digitCount = 0;
    for (const QChar& ch : text) {
        if (ch.isDigit())
            ++digitCount;
    }

    // Keep folding conservative to avoid precision regressions when using
    // temporary C++ doubles in the display simplifier.
    return digitCount <= 15;
}

static QString formatSimplifiedDecimal(double value)
{
    const double rounded = std::round(value);
    if (std::abs(value - rounded) < 1e-12)
        return QString::number(static_cast<long long>(rounded));

    QString text = QString::number(value, 'g', 15);
    if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0')))
            text.chop(1);
        if (text.endsWith(QLatin1Char('.')))
            text.chop(1);
    }
    return text;
}

static bool normalizeUnsignedIntegerEquivalentDecimalText(QString& text)
{
    const QRegularExpressionMatch match = RegExpPatterns::unsignedIntegerEquivalentDecimal().match(text);
    if (!match.hasMatch())
        return false;

    const QString fractionalPart = match.captured(2);
    for (const QChar& ch : fractionalPart) {
        if (ch != QLatin1Char('0'))
            return false;
    }

    text = match.captured(1);
    return true;
}

static QString renderIntegerPowersAsSuperscriptsForDisplay(
    const QString& expression,
    bool collapseRightAssociativeIntegerPowerChains = false)
{
    auto collapseRightAssociativeIntegerPowerChainsFn = [](QString text) {
        auto checkedPow = [](long long base, long long exponent, long long* out) {
            if (exponent < 0)
                return false;
            long long result = 1;
            for (long long i = 0; i < exponent; ++i) {
                if (base != 0 && result > std::numeric_limits<long long>::max() / base)
                    return false;
                result *= base;
                if (result > 1000000)
                    return false;
            }
            *out = result;
            return true;
        };

        auto collapseIntegerExponentChainText = [&checkedPow](const QString& chain, long long* out) {
            QVector<long long> exponents;
            int pos = 0;
            while (pos < chain.size()) {
                bool negative = false;
                if (chain.at(pos) == QLatin1Char('-') || chain.at(pos) == QChar(0x2212)) {
                    negative = true;
                    ++pos;
                }
                if (negative || pos >= chain.size() || !chain.at(pos).isDigit())
                    return false;

                long long value = 0;
                while (pos < chain.size() && chain.at(pos).isDigit()) {
                    const int digit = chain.at(pos).digitValue();
                    if (value > 1000000)
                        return false;
                    value = value * 10 + digit;
                    ++pos;
                }
                exponents.append(value);

                if (pos >= chain.size())
                    break;
                if (chain.at(pos) != QLatin1Char('^'))
                    return false;
                ++pos;
            }

            if (exponents.size() < 2)
                return false;

            long long collapsed = exponents.last();
            for (int idx = exponents.size() - 2; idx >= 0; --idx) {
                if (!checkedPow(exponents.at(idx), collapsed, &collapsed))
                    return false;
            }
            *out = collapsed;
            return true;
        };

        auto parsePositiveIntegerToken = [](const QString& input, int start, long long* valueOut, int* endOut) {
            int pos = start;
            bool parenthesized = false;
            if (pos < input.size() && input.at(pos) == QLatin1Char('(')) {
                parenthesized = true;
                ++pos;
            }
            if (pos >= input.size() || !input.at(pos).isDigit())
                return false;
            long long value = 0;
            while (pos < input.size() && input.at(pos).isDigit()) {
                const int digit = input.at(pos).digitValue();
                if (value > 1000000)
                    return false;
                value = value * 10 + digit;
                ++pos;
            }
            if (parenthesized) {
                if (pos >= input.size() || input.at(pos) != QLatin1Char(')'))
                    return false;
                ++pos;
            }
            *valueOut = value;
            *endOut = pos;
            return true;
        };

        // Fold nested integer powers: (a^m)^n -> a^(m*n).
        while (true) {
            bool changed = false;
            for (int i = 1; i < text.size(); ++i) {
                if (text.at(i) != QLatin1Char('^') || text.at(i - 1) != QLatin1Char(')'))
                    continue;

                long long outerExp = 0;
                int outerEnd = i + 1;
                if (!parsePositiveIntegerToken(text, i + 1, &outerExp, &outerEnd))
                    continue;

                int depth = 0;
                int open = -1;
                for (int j = i - 1; j >= 0; --j) {
                    const QChar ch = text.at(j);
                    if (ch == QLatin1Char(')'))
                        ++depth;
                    else if (ch == QLatin1Char('(')) {
                        --depth;
                        if (depth == 0) {
                            open = j;
                            break;
                        }
                    }
                }
                if (open < 0)
                    continue;

                const QString inner = text.mid(open + 1, i - open - 2);
                int innerDepth = 0;
                int innerPowPos = -1;
                for (int j = 0; j < inner.size(); ++j) {
                    const QChar ch = inner.at(j);
                    if (ch == QLatin1Char('('))
                        ++innerDepth;
                    else if (ch == QLatin1Char(')')) {
                        if (innerDepth > 0)
                            --innerDepth;
                    } else if (ch == QLatin1Char('^') && innerDepth == 0) {
                        innerPowPos = j;
                    }
                }
                if (innerPowPos <= 0)
                    continue;

                long long innerExp = 0;
                int innerExpEnd = innerPowPos + 1;
                if (!parsePositiveIntegerToken(inner, innerPowPos + 1, &innerExp, &innerExpEnd))
                    continue;
                if (innerExpEnd != inner.size())
                    continue;

                const QString innerBase = inner.left(innerPowPos);
                long long mergedExp = 0;
                if (innerExp != 0 && outerExp > 1000000 / innerExp)
                    continue;
                mergedExp = innerExp * outerExp;
                if (mergedExp > 1000000)
                    continue;

                text.replace(open, outerEnd - open, innerBase + QLatin1Char('^') + QString::number(mergedExp));
                changed = true;
                break;
            }
            if (!changed)
                break;
        }

        auto parseIntegerExponentToken = [&text, &collapseIntegerExponentChainText](int start, long long* valueOut, int* endOut) {
            int pos = start;
            if (pos < text.size() && text.at(pos) == QLatin1Char('(')) {
                int depth = 0;
                int close = -1;
                for (int i = pos; i < text.size(); ++i) {
                    const QChar ch = text.at(i);
                    if (ch == QLatin1Char('('))
                        ++depth;
                    else if (ch == QLatin1Char(')')) {
                        --depth;
                        if (depth == 0) {
                            close = i;
                            break;
                        }
                    }
                }
                if (close < 0)
                    return false;
                const QString inner = text.mid(pos + 1, close - pos - 1).trimmed();
                long long collapsed = 0;
                if (!collapseIntegerExponentChainText(inner, &collapsed))
                    return false;
                *valueOut = collapsed;
                *endOut = close + 1;
                return true;
            }

            bool negative = false;
            if (pos < text.size() && (text.at(pos) == QLatin1Char('-') || text.at(pos) == QChar(0x2212))) {
                negative = true;
                ++pos;
            }

            if (pos >= text.size() || !text.at(pos).isDigit())
                return false;

            long long value = 0;
            while (pos < text.size() && text.at(pos).isDigit()) {
                const int digit = text.at(pos).digitValue();
                if (value > 1000000) // avoid overflow-heavy chains in display-only rewrite
                    return false;
                value = value * 10 + digit;
                ++pos;
            }

            *valueOut = negative ? -value : value;
            *endOut = pos;
            return true;
        };

        for (int i = 0; i < text.size(); ++i) {
            if (text.at(i) != QLatin1Char('^'))
                continue;

            QVector<long long> exponents;
            const bool startsWithParenthesizedExponentChain =
                (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('('));
            int tokenPos = i + 1;
            while (tokenPos < text.size()) {
                long long value = 0;
                int tokenEnd = tokenPos;
                if (!parseIntegerExponentToken(tokenPos, &value, &tokenEnd))
                    break;
                exponents.append(value);
                tokenPos = tokenEnd;
                if (tokenPos >= text.size() || text.at(tokenPos) != QLatin1Char('^'))
                    break;
                ++tokenPos;
            }

            if (exponents.isEmpty())
                continue;
            if (exponents.size() < 2 && !startsWithParenthesizedExponentChain)
                continue;
            if (std::any_of(exponents.constBegin(), exponents.constEnd(),
                    [](long long v) { return v < 0; })) {
                continue;
            }

            long long collapsedExponent = exponents.last();
            bool ok = true;
            for (int idx = exponents.size() - 2; idx >= 0; --idx) {
                if (!checkedPow(exponents.at(idx), collapsedExponent, &collapsedExponent)) {
                    ok = false;
                    break;
                }
            }
            if (!ok)
                continue;

            text.replace(i + 1, tokenPos - (i + 1), QString::number(collapsedExponent));
        }
        return text;
    };

    QString normalizedExpression = expression;
    if (collapseRightAssociativeIntegerPowerChains)
        normalizedExpression = collapseRightAssociativeIntegerPowerChainsFn(expression);

    static const QRegularExpression powerRE(
        QStringLiteral(R"(\^(?:\(([−-]?)(\d+)\)|(\d+))(?![\p{L}\p{N}\.,_!]))"));
    static const QHash<QChar, QChar> superscriptDigits = {
        {QChar('0'), QChar(0x2070)},
        {QChar('1'), QChar(0x00B9)},
        {QChar('2'), QChar(0x00B2)},
        {QChar('3'), QChar(0x00B3)},
        {QChar('4'), QChar(0x2074)},
        {QChar('5'), QChar(0x2075)},
        {QChar('6'), QChar(0x2076)},
        {QChar('7'), QChar(0x2077)},
        {QChar('8'), QChar(0x2078)},
        {QChar('9'), QChar(0x2079)},
    };

    QString rendered;
    rendered.reserve(normalizedExpression.size());
    int lastPos = 0;
    auto it = powerRE.globalMatch(normalizedExpression);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        rendered += normalizedExpression.mid(lastPos, match.capturedStart() - lastPos);

        const QString sign = match.captured(1);
        QString digits = match.captured(2);
        if (digits.isEmpty())
            digits = match.captured(3);

        QString superscript;
        superscript.reserve(digits.size() + 1);
        if (!sign.isEmpty())
            superscript += QChar(0x207B); // ⁻ SUPERSCRIPT MINUS.
        for (const QChar& digit : digits)
            superscript += superscriptDigits.value(digit, digit);
        rendered += superscript;

        lastPos = match.capturedEnd();
    }
    rendered += normalizedExpression.mid(lastPos);

    auto isIdentifierChar = [](const QChar& ch) {
        return isIdentifierContinue(ch);
    };
    auto isIdentifierStartChar = [](const QChar& ch) {
        return isIdentifierStart(ch);
    };
    auto isSuperscriptPowerChar = [](const QChar& ch) {
        switch (ch.unicode()) {
            case 0x207B: // ⁻
            case 0x2070: // ⁰
            case 0x00B9: // ¹
            case 0x00B2: // ²
            case 0x00B3: // ³
            case 0x2074: // ⁴
            case 0x2075: // ⁵
            case 0x2076: // ⁶
            case 0x2077: // ⁷
            case 0x2078: // ⁸
            case 0x2079: // ⁹
                return true;
            default:
                return false;
        }
    };

    // Convert function-call powers from "f(x)²" to "f²(x)".
    int i = 0;
    while (i < rendered.size()) {
        if (rendered.at(i) != QLatin1Char(')')) {
            ++i;
            continue;
        }

        int superscriptEnd = i + 1;
        while (superscriptEnd < rendered.size() && isSuperscriptPowerChar(rendered.at(superscriptEnd)))
            ++superscriptEnd;
        if (superscriptEnd == i + 1) {
            ++i;
            continue;
        }

        int depth = 0;
        int openParen = -1;
        for (int j = i; j >= 0; --j) {
            const QChar ch = rendered.at(j);
            if (ch == QLatin1Char(')')) {
                ++depth;
            } else if (ch == QLatin1Char('(')) {
                --depth;
                if (depth == 0) {
                    openParen = j;
                    break;
                }
            }
        }
        if (openParen <= 0) {
            i = superscriptEnd;
            continue;
        }

        int nameStart = openParen - 1;
        while (nameStart >= 0 && isIdentifierChar(rendered.at(nameStart)))
            --nameStart;
        ++nameStart;
        if (nameStart >= openParen || !isIdentifierStartChar(rendered.at(nameStart))) {
            i = superscriptEnd;
            continue;
        }

        if (nameStart > 0 && isIdentifierChar(rendered.at(nameStart - 1))) {
            i = superscriptEnd;
            continue;
        }

        const QString funcName = rendered.mid(nameStart, openParen - nameStart);
        const QString callArgs = rendered.mid(openParen, i - openParen + 1);
        const QString superscript = rendered.mid(i + 1, superscriptEnd - (i + 1));
        const QString replacement = funcName + superscript + callArgs;
        rendered.replace(nameStart, superscriptEnd - nameStart, replacement);
        i = nameStart + replacement.size();
    }

    return rendered;
}

static bool isWrappedInOuterParentheses(const QString& text)
{
    if (text.size() < 2 || text.front() != QLatin1Char('(') || text.back() != QLatin1Char(')'))
        return false;

    int depth = 0;
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('(')) {
            ++depth;
        } else if (ch == QLatin1Char(')')) {
            --depth;
            if (depth < 0)
                return false;
            if (depth == 0 && i != text.size() - 1)
                return false;
        }
    }

    return depth == 0;
}

static bool termNeedsGroupingForDisplay(const Tokens& tokens, int firstIndex, int lastIndex)
{
    if (firstIndex < 0 || lastIndex >= tokens.size() || firstIndex >= lastIndex)
        return false;

    const int additivePrecedence = opPrecedence(Token::Addition);
    int depth = 0;
    for (int i = firstIndex; i <= lastIndex; ++i) {
        const Token& token = tokens.at(i);
        const Token::Operator op = token.asOperator();
        if (op == Token::AssociationStart) {
            ++depth;
            continue;
        }
        if (op == Token::AssociationEnd) {
            if (depth > 0)
                --depth;
            continue;
        }
        if (depth != 0 || !token.isOperator())
            continue;

        const bool hasLeftOperand = i > firstIndex
            && tokenCanEndOperandForDisplaySpacing(tokens.at(i - 1));
        const bool hasRightOperand = i < lastIndex
            && tokenCanStartOperandForDisplaySpacing(tokens.at(i + 1));
        if (!hasLeftOperand || !hasRightOperand)
            continue;

        if (opPrecedence(op) > additivePrecedence) {
            // Additive precedence already disambiguates top-level arithmetic
            // terms, so avoid injecting grouping parentheses for them.
            if (op == Token::Exponentiation
                || op == Token::Multiplication
                || op == Token::Division
                || op == Token::IntegerDivision
                || op == Token::Modulo)
            {
                continue;
            }
            return true;
        }
    }

    return false;
}

static QString groupHighPrecedenceAdditiveTermsForDisplay(const QString& expression,
                                                          const Tokens& tokens)
{
    if (tokens.isEmpty())
        return expression;

    QVector<int> topLevelBinaryAddSubTokenIndices;
    int depth = 0;
    for (int i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens.at(i);
        const Token::Operator op = token.asOperator();
        if (op == Token::AssociationStart) {
            ++depth;
            continue;
        }
        if (op == Token::AssociationEnd) {
            if (depth > 0)
                --depth;
            continue;
        }
        if (depth != 0 || (op != Token::Addition && op != Token::Subtraction))
            continue;

        const bool hasLeftOperand = i > 0
            && tokenCanEndOperandForDisplaySpacing(tokens.at(i - 1));
        const bool hasRightOperand = i + 1 < tokens.size()
            && tokenCanStartOperandForDisplaySpacing(tokens.at(i + 1));
        if (hasLeftOperand && hasRightOperand)
            topLevelBinaryAddSubTokenIndices.append(i);
    }

    if (topLevelBinaryAddSubTokenIndices.isEmpty())
        return expression;

    auto tokenRangeText = [&expression, &tokens](int startIndex, int endIndex) {
        const int startPos = tokens.at(startIndex).pos();
        const int endPos = tokens.at(endIndex).pos() + tokens.at(endIndex).size();
        return expression.mid(startPos, endPos - startPos);
    };

    QString grouped;
    grouped.reserve(expression.size() + topLevelBinaryAddSubTokenIndices.size() * 2);

    int termStartIndex = 0;
    for (int splitPos = 0; splitPos < topLevelBinaryAddSubTokenIndices.size(); ++splitPos) {
        const int operatorIndex = topLevelBinaryAddSubTokenIndices.at(splitPos);
        const int termEndIndex = operatorIndex - 1;
        if (termEndIndex >= termStartIndex) {
            QString termText = tokenRangeText(termStartIndex, termEndIndex);
            if (termNeedsGroupingForDisplay(tokens, termStartIndex, termEndIndex)
                && !isWrappedInOuterParentheses(termText))
            {
                termText = QStringLiteral("(%1)").arg(termText);
            }
            grouped += termText;
        }

        grouped += tokenRangeText(operatorIndex, operatorIndex);
        termStartIndex = operatorIndex + 1;
    }

    if (termStartIndex < tokens.size()) {
        QString lastTermText = tokenRangeText(termStartIndex, tokens.size() - 1);
        if (termNeedsGroupingForDisplay(tokens, termStartIndex, tokens.size() - 1)
            && !isWrappedInOuterParentheses(lastTermText))
        {
            lastTermText = QStringLiteral("(%1)").arg(lastTermText);
        }
        grouped += lastTermText;
    }

    return grouped;
}

static QString simplifyRepeatedMultiplicativeBasesForDisplay(const QString& expression,
                                                             const Tokens& tokens);

static bool parseSimplifiablePowerFactor(const QString& factorText,
                                         QString* baseOut,
                                         int* exponentOut)
{
    static const QRegularExpression identifierRE(
        QStringLiteral(R"(^(?:[\p{L}_][\p{L}\p{N}_]*)$)"));
    static const QRegularExpression numberRE(
        QStringLiteral(R"(^(?:\d+(?:\.\d+)?)$)"));

    auto isFunctionCallBase = [](const QString& text) {
        const int firstParen = text.indexOf(QLatin1Char('('));
        if (firstParen <= 0 || !text.endsWith(QLatin1Char(')')))
            return false;

        const QString functionName = text.left(firstParen);
        if (!identifierRE.match(functionName).hasMatch())
            return false;

        int depth = 0;
        for (int i = firstParen; i < text.size(); ++i) {
            const QChar ch = text.at(i);
            if (ch == QLatin1Char('(')) {
                ++depth;
            } else if (ch == QLatin1Char(')')) {
                --depth;
                if (depth < 0)
                    return false;
                if (depth == 0 && i != text.size() - 1)
                    return false;
            }
        }
        return depth == 0;
    };

    auto isSimplifiableBase = [&isFunctionCallBase](const QString& text) {
        return identifierRE.match(text).hasMatch()
            || numberRE.match(text).hasMatch()
            || isFunctionCallBase(text);
    };

    const QString factor = factorText.trimmed();
    QString base = factor;
    int exponent = 1;

    int depth = 0;
    int topLevelPowerPos = -1;
    for (int i = 0; i < factor.size(); ++i) {
        const QChar ch = factor.at(i);
        if (ch == QLatin1Char('(')) {
            ++depth;
        } else if (ch == QLatin1Char(')')) {
            if (depth > 0)
                --depth;
        } else if (ch == QLatin1Char('^') && depth == 0) {
            topLevelPowerPos = i;
            break;
        }
    }

    if (topLevelPowerPos >= 0) {
        base = factor.left(topLevelPowerPos);
        QString exponentText = factor.mid(topLevelPowerPos + 1);
        if (base.isEmpty() || exponentText.isEmpty())
            return false;

        if (exponentText.startsWith(QLatin1Char('('))
            && exponentText.endsWith(QLatin1Char(')')))
        {
            exponentText = exponentText.mid(1, exponentText.size() - 2);
        }

        if (!normalizeUnsignedIntegerEquivalentDecimalText(exponentText)
            && !isUnsignedDecimalIntegerText(exponentText))
        {
            return false;
        }

        bool ok = false;
        exponent = exponentText.toInt(&ok);
        if (!ok || exponent <= 0)
            return false;
    }

    base = base.trimmed();
    if (!isSimplifiableBase(base))
        return false;

    *baseOut = base;
    *exponentOut = exponent;
    return true;
}

static QString simplifyRepeatedBasesInMultiplicativeTermForDisplay(const QString& termText)
{
    if (termText.isEmpty())
        return termText;

    QString workingTerm = termText;
    int negativeUnitFactorCount = 0;
    {
        static const QRegularExpression s_leadingNegativeUnitMultiplier(
            QStringLiteral(R"(^\s*[−-]1\s*(?:\*|⋅|×)\s*(.+)$)"));
        const QRegularExpressionMatch match = s_leadingNegativeUnitMultiplier.match(workingTerm);
        if (match.hasMatch()) {
            workingTerm = match.captured(1);
            ++negativeUnitFactorCount;
        }

        static const QRegularExpression s_trailingDivisionNegativeOne(
            QStringLiteral(R"(\s*(?:/|÷|⧸)\s*(?:[−-]1|\([−-]1\))\s*$)"));
        static const QRegularExpression s_trailingMultiplicationNegativeOne(
            QStringLiteral(R"(\s*(?:\*|⋅|×)\s*(?:[−-]1|\([−-]1\))\s*$)"));
        static const QRegularExpression s_trailingNeutralOne(
            QStringLiteral(R"(\s*(?:/|÷|⧸|\*|⋅|×)\s*(?:\+?1|\(\+?1\))\s*$)"));
        while (true) {
            QString updated = workingTerm;
            updated.replace(s_trailingDivisionNegativeOne, QString());
            if (updated != workingTerm) {
                workingTerm = updated;
                ++negativeUnitFactorCount;
                continue;
            }

            updated = workingTerm;
            updated.replace(s_trailingMultiplicationNegativeOne, QString());
            if (updated != workingTerm) {
                workingTerm = updated;
                ++negativeUnitFactorCount;
                continue;
            }

            updated = workingTerm;
            updated.replace(s_trailingNeutralOne, QString());
            if (updated == workingTerm)
                break;
            workingTerm = updated;
        }
    }

    auto simplifyMultiplicationOnlySegment = [](const QString& segmentText) {
        if (segmentText.isEmpty())
            return segmentText;

        QVector<QString> factors;
        QVector<QString> operators;
        int depth = 0;
        int partStart = 0;
        for (int i = 0; i < segmentText.size(); ++i) {
            const QChar ch = segmentText.at(i);
            if (ch == QLatin1Char('(')) {
                ++depth;
                continue;
            }
            if (ch == QLatin1Char(')')) {
                if (depth > 0)
                    --depth;
                continue;
            }
            if (depth == 0
                && (ch == QLatin1Char('*')
                    || ch == UnicodeChars::DotOperator
                    || ch == UnicodeChars::MultiplicationSign))
            {
                factors.append(segmentText.mid(partStart, i - partStart));
                operators.append(QString(ch));
                partStart = i + 1;
            }
        }
        factors.append(segmentText.mid(partStart));

        if (factors.size() < 2)
            return segmentText;

        QHash<QString, int> totalExponentsByBase;
        for (int i = 0; i < factors.size(); ++i) {
            QString base;
            int exponent = 0;
            if (!parseSimplifiablePowerFactor(factors.at(i), &base, &exponent)) {
                continue;
            }
            totalExponentsByBase[base] += exponent;
        }

        QString simplified;
        QSet<QString> emittedBases;
        bool hasOutput = false;
        int i = 0;
        while (i < factors.size()) {
            QString base;
            int exponent = 0;
            const bool isSimplifiableFactor =
                parseSimplifiablePowerFactor(factors.at(i), &base, &exponent);

            if (!isSimplifiableFactor) {
                if (hasOutput && i > 0)
                    simplified += operators.at(i - 1);
                simplified += factors.at(i);
                hasOutput = true;
                ++i;
                continue;
            }

            if (emittedBases.contains(base)) {
                ++i;
                continue;
            }
            emittedBases.insert(base);

            if (hasOutput && i > 0)
                simplified += operators.at(i - 1);
            const int totalExponent = totalExponentsByBase.value(base, 0);
            if (totalExponent > 1) {
                simplified += base;
                simplified += QStringLiteral("^%1").arg(totalExponent);
            } else {
                simplified += factors.at(i);
            }
            hasOutput = true;
            ++i;
        }

        QVector<QString> finalFactors;
        QVector<QString> finalOperators;
        int finalDepth = 0;
        int finalStart = 0;
        for (int i = 0; i < simplified.size(); ++i) {
            const QChar ch = simplified.at(i);
            if (ch == QLatin1Char('(')) {
                ++finalDepth;
                continue;
            }
            if (ch == QLatin1Char(')')) {
                if (finalDepth > 0)
                    --finalDepth;
                continue;
            }
            if (finalDepth == 0
                && (ch == QLatin1Char('*')
                    || ch == UnicodeChars::DotOperator
                    || ch == UnicodeChars::MultiplicationSign))
            {
                finalFactors.append(simplified.mid(finalStart, i - finalStart));
                finalOperators.append(QString(ch));
                finalStart = i + 1;
            }
        }
        finalFactors.append(simplified.mid(finalStart));

        int firstNumericIndex = -1;
        double numericProduct = 1.0;
        int numericCount = 0;
        bool hasSignedNumericFactor = false;
        auto isParenthesizedSignedDecimalFactor = [](const QString& factorText) {
            QString text = factorText.trimmed();
            if (!(text.startsWith(QLatin1Char('(')) && text.endsWith(QLatin1Char(')'))))
                return false;
            text = text.mid(1, text.size() - 2).trimmed();
            return isSignedDecimalNumberText(text) && !isUnsignedDecimalNumberText(text);
        };
        for (int i = 0; i < finalFactors.size(); ++i) {
            const QString factor = finalFactors.at(i);
            if (isSignedDecimalNumberText(factor) && !isUnsignedDecimalNumberText(factor))
                hasSignedNumericFactor = true;
            if (isParenthesizedSignedDecimalFactor(factor))
                hasSignedNumericFactor = true;
            if (!isUnsignedDecimalNumberText(finalFactors.at(i))
                || !isSafeForDoubleArithmetic(finalFactors.at(i)))
                continue;
            if (firstNumericIndex < 0)
                firstNumericIndex = i;
            bool ok = false;
            const double value = finalFactors.at(i).toDouble(&ok);
            if (!ok)
                continue;
            numericProduct *= value;
            ++numericCount;
        }
        if (hasSignedNumericFactor)
            return simplified;
        if (numericCount > 1) {
            finalFactors[firstNumericIndex] = formatSimplifiedDecimal(numericProduct);
            for (int i = finalFactors.size() - 1; i >= 0; --i) {
                if (i == firstNumericIndex
                    || !isUnsignedDecimalNumberText(finalFactors.at(i))
                    || !isSafeForDoubleArithmetic(finalFactors.at(i)))
                    continue;
                finalFactors.removeAt(i);
                finalOperators.removeAt(i - 1);
            }
            QString rebuiltNumeric = finalFactors.at(0);
            for (int i = 1; i < finalFactors.size(); ++i) {
                rebuiltNumeric += finalOperators.at(i - 1);
                rebuiltNumeric += finalFactors.at(i);
            }
            return rebuiltNumeric;
        }

        return simplified;
    };
    auto simplifyPostDivisionSegment = [&simplifyMultiplicationOnlySegment](const QString& segmentText) {
        if (segmentText.isEmpty())
            return segmentText;

        QVector<QString> factors;
        QVector<QString> operators;
        int depth = 0;
        int partStart = 0;
        for (int i = 0; i < segmentText.size(); ++i) {
            const QChar ch = segmentText.at(i);
            if (ch == QLatin1Char('(')) {
                ++depth;
                continue;
            }
            if (ch == QLatin1Char(')')) {
                if (depth > 0)
                    --depth;
                continue;
            }
            if (depth == 0
                && (ch == QLatin1Char('*')
                    || ch == UnicodeChars::DotOperator
                    || ch == UnicodeChars::MultiplicationSign))
            {
                factors.append(segmentText.mid(partStart, i - partStart));
                operators.append(QString(ch));
                partStart = i + 1;
            }
        }
        factors.append(segmentText.mid(partStart));

        if (factors.size() <= 1)
            return segmentText;

        QString tail;
        tail += factors.at(1);
        for (int i = 2; i < factors.size(); ++i) {
            tail += operators.at(i - 1);
            tail += factors.at(i);
        }
        tail = simplifyMultiplicationOnlySegment(tail);

        QString rebuilt = factors.at(0);
        rebuilt += operators.at(0);
        rebuilt += tail;
        return rebuilt;
    };

    QVector<QString> divisionSegments;
    QVector<QString> divisionOperators;
    int depth = 0;
    int segmentStart = 0;
    for (int i = 0; i < workingTerm.size(); ++i) {
        const QChar ch = workingTerm.at(i);
        if (ch == QLatin1Char('(')) {
            ++depth;
            continue;
        }
        if (ch == QLatin1Char(')')) {
            if (depth > 0)
                --depth;
            continue;
        }
        if (depth == 0 && (ch == QLatin1Char('/') || ch == QLatin1Char('\\'))) {
            divisionSegments.append(workingTerm.mid(segmentStart, i - segmentStart));
            divisionOperators.append(QString(ch));
            segmentStart = i + 1;
        }
    }
    divisionSegments.append(workingTerm.mid(segmentStart));

    auto foldNumericDenominatorIntoPreviousSegment =
        [](QString* previousSegment, const QString& denominatorText) {
            if (!isUnsignedDecimalNumberText(denominatorText)
                || !isSafeForDoubleArithmetic(denominatorText))
                return false;

            bool okDen = false;
            const double den = denominatorText.toDouble(&okDen);
            if (!okDen || den == 0.0)
                return false;

            QString prev = *previousSegment;
            QVector<QString> factors;
            QVector<QString> operators;
            int depth = 0;
            int partStart = 0;
            for (int i = 0; i < prev.size(); ++i) {
                const QChar ch = prev.at(i);
                if (ch == QLatin1Char('(')) {
                    ++depth;
                    continue;
                }
                if (ch == QLatin1Char(')')) {
                    if (depth > 0)
                        --depth;
                    continue;
                }
                if (depth == 0
                    && (ch == QLatin1Char('*')
                        || ch == UnicodeChars::DotOperator
                        || ch == UnicodeChars::MultiplicationSign))
                {
                    factors.append(prev.mid(partStart, i - partStart));
                    operators.append(QString(ch));
                    partStart = i + 1;
                }
            }
            factors.append(prev.mid(partStart));

            if (factors.size() <= 1)
                return false;

            for (int i = 1; i < factors.size(); ++i) {
                if (!isUnsignedDecimalNumberText(factors.at(i))
                    || !isSafeForDoubleArithmetic(factors.at(i)))
                    continue;

                bool okNum = false;
                const double num = factors.at(i).toDouble(&okNum);
                if (!okNum)
                    continue;

                factors[i] = formatSimplifiedDecimal(num / den);
                QString rebuilt = factors.at(0);
                for (int j = 1; j < factors.size(); ++j) {
                    rebuilt += operators.at(j - 1);
                    rebuilt += factors.at(j);
                }
                *previousSegment = rebuilt;
                return true;
            }

            return false;
        };

    QString rebuilt;
    if (divisionOperators.isEmpty()) {
        QString simplified = simplifyMultiplicationOnlySegment(workingTerm);
        if ((negativeUnitFactorCount % 2) != 0 && !simplified.isEmpty()) {
            if (simplified == QLatin1String("1"))
                return QStringLiteral("-1");
            return QStringLiteral("-") + simplified;
        }
        return simplified;
    } else {
        QVector<QString> simplifiedSegments;
        simplifiedSegments.reserve(divisionSegments.size());
        for (int i = 0; i < divisionSegments.size(); ++i) {
            const bool followsDivisionOperator =
                i > 0 && (divisionOperators.at(i - 1) == QLatin1String("/")
                          || divisionOperators.at(i - 1) == QLatin1String("\\"));
            simplifiedSegments.append(followsDivisionOperator
                ? simplifyPostDivisionSegment(divisionSegments.at(i))
                : simplifyMultiplicationOnlySegment(divisionSegments.at(i)));
        }

        for (int i = 1; i < simplifiedSegments.size(); ++i) {
            if (divisionOperators.at(i - 1) != QLatin1String("/"))
                continue;
            if (i <= 1 || divisionOperators.at(i - 2) != QLatin1String("/"))
                continue;
            QString previous = simplifiedSegments.at(i - 1);
            if (!foldNumericDenominatorIntoPreviousSegment(&previous, simplifiedSegments.at(i)))
                continue;
            simplifiedSegments[i - 1] = previous;
            simplifiedSegments.removeAt(i);
            divisionOperators.removeAt(i - 1);
            --i;
        }

        for (int i = 0; i < simplifiedSegments.size(); ++i) {
            rebuilt += simplifiedSegments.at(i);
            if (i < divisionOperators.size())
                rebuilt += divisionOperators.at(i);
        }
    }

    auto simplifyParenthesizedDenominators = [&simplifyMultiplicationOnlySegment](const QString& text) {
        QString out;
        out.reserve(text.size());

        for (int i = 0; i < text.size(); ++i) {
            const QChar ch = text.at(i);
            if (ch != QLatin1Char('/') || i + 1 >= text.size() || text.at(i + 1) != QLatin1Char('(')) {
                out += ch;
                continue;
            }

            int depth = 0;
            int closePos = -1;
            for (int j = i + 1; j < text.size(); ++j) {
                const QChar inner = text.at(j);
                if (inner == QLatin1Char('(')) {
                    ++depth;
                } else if (inner == QLatin1Char(')')) {
                    --depth;
                    if (depth == 0) {
                        closePos = j;
                        break;
                    }
                }
            }

            if (closePos < 0) {
                out += ch;
                continue;
            }

            const QString inside = text.mid(i + 2, closePos - (i + 2));
            const Tokens insideTokens = Evaluator::instance()->scan(inside);
            QString simplifiedInside = inside;
            if (!insideTokens.isEmpty())
                simplifiedInside = simplifyRepeatedMultiplicativeBasesForDisplay(inside, insideTokens);
            else
                simplifiedInside = simplifyMultiplicationOnlySegment(inside);

            QString base;
            int exponent = 0;
            if (parseSimplifiablePowerFactor(simplifiedInside, &base, &exponent)) {
                out += QLatin1Char('/');
                out += simplifiedInside;
            } else {
                out += QLatin1Char('/');
                out += QLatin1Char('(');
                out += simplifiedInside;
                out += QLatin1Char(')');
            }

            i = closePos;
        }

        return out;
    };
    rebuilt = simplifyParenthesizedDenominators(rebuilt);

    auto simplifyAlgebraicMulDivChain = [&negativeUnitFactorCount](const QString& text) {
        auto isFullyWrappedByOuterParens = [](const QString& expr) {
            if (expr.size() < 2 || expr.at(0) != QLatin1Char('(') || expr.at(expr.size() - 1) != QLatin1Char(')'))
                return false;
            int depth = 0;
            for (int i = 0; i < expr.size(); ++i) {
                const QChar ch = expr.at(i);
                if (ch == QLatin1Char('('))
                    ++depth;
                else if (ch == QLatin1Char(')'))
                    --depth;
                if (depth == 0 && i < expr.size() - 1)
                    return false;
            }
            return depth == 0;
        };

        auto normalizeSimplifiableSignedFactor = [&isFullyWrappedByOuterParens](const QString& factor,
                                                                                 QString* unsignedFactor,
                                                                                 int* signOut) {
            QString normalized = factor.trimmed();
            while (isFullyWrappedByOuterParens(normalized))
                normalized = normalized.mid(1, normalized.size() - 2).trimmed();

            int sign = 1;
            bool consumedUnarySign = false;
            while (!normalized.isEmpty()) {
                const QChar ch = normalized.at(0);
                if (ch == QLatin1Char('+')) {
                    consumedUnarySign = true;
                    normalized = normalized.mid(1).trimmed();
                    continue;
                }
                if (isMinus(ch)) {
                    consumedUnarySign = true;
                    sign = -sign;
                    normalized = normalized.mid(1).trimmed();
                    continue;
                }
                break;
            }

            while (isFullyWrappedByOuterParens(normalized))
                normalized = normalized.mid(1, normalized.size() - 2).trimmed();
            if (normalized.isEmpty())
                return false;
            if (consumedUnarySign && isUnsignedDecimalNumberText(normalized))
                return false;

            *unsignedFactor = normalized;
            *signOut = sign;
            return true;
        };

        QVector<QString> factors;
        QVector<QString> operators;
        int depth = 0;
        int partStart = 0;
        for (int i = 0; i < text.size(); ++i) {
            const QChar ch = text.at(i);
            if (ch == QLatin1Char('(')) {
                ++depth;
                continue;
            }
            if (ch == QLatin1Char(')')) {
                if (depth > 0)
                    --depth;
                continue;
            }
            if (depth == 0
                && (ch == QLatin1Char('*')
                    || ch == UnicodeChars::DotOperator
                    || ch == UnicodeChars::MultiplicationSign
                    || ch == QLatin1Char('/')))
            {
                factors.append(text.mid(partStart, i - partStart));
                operators.append(QString(ch));
                partStart = i + 1;
            } else if (depth == 0 && ch == QLatin1Char('\\')) {
                return text;
            }
        }
        factors.append(text.mid(partStart));
        if (factors.size() < 2)
            return text;

        QHash<QString, int> exponentByBase;
        QVector<QString> baseOrder;
        double numericCoefficient = 1.0;
        bool hasNonNumericBase = false;

        for (int i = 0; i < factors.size(); ++i) {
            QString unsignedFactor;
            int factorSign = 1;
            if (!normalizeSimplifiableSignedFactor(factors.at(i), &unsignedFactor, &factorSign))
                return text;

            if (factorSign < 0)
                ++negativeUnitFactorCount;

            QString base;
            int exponent = 0;
            if (!parseSimplifiablePowerFactor(unsignedFactor, &base, &exponent))
                return text;

            const QString op = (i == 0) ? QStringLiteral("*") : operators.at(i - 1);
            const int sign = (op == QLatin1String("/")) ? -1 : 1;
            if (isUnsignedDecimalNumberText(base) && isSafeForDoubleArithmetic(base)) {
                bool ok = false;
                const double value = base.toDouble(&ok);
                if (!ok)
                    return text;
                numericCoefficient *= std::pow(value, sign * exponent);
            } else if (!isUnsignedDecimalNumberText(base)) {
                if (!exponentByBase.contains(base))
                    baseOrder.append(base);
                exponentByBase[base] += sign * exponent;
                hasNonNumericBase = true;
            } else {
                return text;
            }
        }

        if (!hasNonNumericBase)
            return formatSimplifiedDecimal(numericCoefficient);

        QStringList positiveFactors;
        QStringList negativeFactors;
        for (const QString& base : baseOrder) {
            const int exponent = exponentByBase.value(base, 0);
            if (exponent > 0) {
                positiveFactors.append(exponent == 1
                    ? base
                    : QStringLiteral("%1^%2").arg(base).arg(exponent));
            } else if (exponent < 0) {
                const int denExp = -exponent;
                negativeFactors.append(denExp == 1
                    ? base
                    : QStringLiteral("%1^%2").arg(base).arg(denExp));
            }
        }

        QString result;
        const bool coefficientIsOne = std::abs(numericCoefficient - 1.0) < 1e-12;
        if (!coefficientIsOne || (positiveFactors.isEmpty() && !negativeFactors.isEmpty()))
            result += formatSimplifiedDecimal(numericCoefficient);
        if (!positiveFactors.isEmpty()) {
            if (!result.isEmpty())
                result += UnicodeChars::DotOperator;
            result += positiveFactors.join(QString(UnicodeChars::DotOperator));
        }
        for (const QString& factor : negativeFactors) {
            if (result.isEmpty())
                result = QStringLiteral("1");
            result += QLatin1Char('/');
            result += factor;
        }

        return result.isEmpty() ? text : result;
    };

    QString simplified = simplifyAlgebraicMulDivChain(rebuilt);
    if ((negativeUnitFactorCount % 2) != 0 && !simplified.isEmpty()) {
        if (simplified == QLatin1String("1"))
            return QStringLiteral("-1");
        return QStringLiteral("-") + simplified;
    }
    return simplified;

}

static QString simplifyRepeatedMultiplicativeBasesForDisplay(const QString& expression,
                                                             const Tokens& tokens)
{
    if (tokens.isEmpty())
        return expression;

    QVector<int> topLevelBinaryAddSubTokenIndices;
    int depth = 0;
    for (int i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens.at(i);
        const Token::Operator op = token.asOperator();
        if (op == Token::AssociationStart) {
            ++depth;
            continue;
        }
        if (op == Token::AssociationEnd) {
            if (depth > 0)
                --depth;
            continue;
        }
        if (depth != 0 || (op != Token::Addition && op != Token::Subtraction))
            continue;

        const bool hasLeftOperand = i > 0
            && tokenCanEndOperandForDisplaySpacing(tokens.at(i - 1));
        const bool hasRightOperand = i + 1 < tokens.size()
            && tokenCanStartOperandForDisplaySpacing(tokens.at(i + 1));
        if (hasLeftOperand && hasRightOperand)
            topLevelBinaryAddSubTokenIndices.append(i);
    }

    auto tokenRangeText = [&expression, &tokens](int startIndex, int endIndex) {
        const int startPos = tokens.at(startIndex).pos();
        const int endPos = tokens.at(endIndex).pos() + tokens.at(endIndex).size();
        return expression.mid(startPos, endPos - startPos);
    };

    if (topLevelBinaryAddSubTokenIndices.isEmpty())
        return simplifyRepeatedBasesInMultiplicativeTermForDisplay(expression);

    QVector<QString> simplifiedTerms;
    QVector<QString> additiveOperators;
    int termStartIndex = 0;
    for (int splitPos = 0; splitPos < topLevelBinaryAddSubTokenIndices.size(); ++splitPos) {
        const int operatorIndex = topLevelBinaryAddSubTokenIndices.at(splitPos);
        const int termEndIndex = operatorIndex - 1;
        if (termEndIndex >= termStartIndex) {
            const QString termText = tokenRangeText(termStartIndex, termEndIndex);
            simplifiedTerms.append(simplifyRepeatedBasesInMultiplicativeTermForDisplay(termText));
        }
        additiveOperators.append(tokenRangeText(operatorIndex, operatorIndex));
        termStartIndex = operatorIndex + 1;
    }

    if (termStartIndex < tokens.size()) {
        const QString lastTermText = tokenRangeText(termStartIndex, tokens.size() - 1);
        simplifiedTerms.append(simplifyRepeatedBasesInMultiplicativeTermForDisplay(lastTermText));
    }

    const auto isMinusOperator = [](const QString& op) {
        return op.size() == 1 && isMinus(op.at(0));
    };
    const auto normalizeFoldableNumericTerm = [&isMinusOperator](const QString& termText, double* out) {
        QString term = termText.trimmed();
        if (isSignedDecimalNumberText(term) && isSafeForDoubleArithmetic(term)) {
            bool ok = false;
            const double value = term.toDouble(&ok);
            if (ok) {
                *out = value;
                return true;
            }
        }
        if (!(term.startsWith(QLatin1Char('(')) && term.endsWith(QLatin1Char(')'))))
            return false;

        const QString inner = term.mid(1, term.size() - 2).trimmed();
        const Tokens innerTokens = Evaluator::instance()->scan(inner);
        if (innerTokens.isEmpty())
            return false;

        double total = 0.0;
        QString pendingOp = QStringLiteral("+");
        bool expectValue = true;
        for (const Token& token : innerTokens) {
            if (expectValue) {
                if (!token.isNumber())
                    return false;
                const QString numberText = token.text().trimmed();
                if (!isSignedDecimalNumberText(numberText)
                    || !isSafeForDoubleArithmetic(numberText))
                {
                    return false;
                }
                bool ok = false;
                const double value = numberText.toDouble(&ok);
                if (!ok)
                    return false;
                total += isMinusOperator(pendingOp) ? -value : value;
                expectValue = false;
                continue;
            }

            const Token::Operator op = token.asOperator();
            if (op != Token::Addition && op != Token::Subtraction)
                return false;
            pendingOp = token.text();
            expectValue = true;
        }

        if (expectValue)
            return false;

        *out = total;
        return true;
    };
    const bool hasPercentTerm = std::any_of(
        simplifiedTerms.constBegin(),
        simplifiedTerms.constEnd(),
        [](const QString& term) { return term.contains(QLatin1Char('%')); });
    if (hasPercentTerm) {
        if (simplifiedTerms.isEmpty())
            return expression;

        QVector<QString> normalizedTerms = simplifiedTerms;
        for (int i = 0; i < normalizedTerms.size(); ++i) {
            double value = 0.0;
            QString term = normalizedTerms.at(i).trimmed();
            if (term.contains(QLatin1Char('%')))
                continue;
            if (isWrappedInOuterParentheses(term)) {
                const QString inner = term.mid(1, term.size() - 2);
                const Tokens innerTokens = Evaluator::instance()->scan(inner);
                if (!innerTokens.isEmpty()) {
                    const QString simplifiedInner =
                        simplifyRepeatedMultiplicativeBasesForDisplay(inner, innerTokens);
                    term = QStringLiteral("(%1)").arg(simplifiedInner);
                }
            }
            if (normalizeFoldableNumericTerm(term, &value))
                term = formatSimplifiedDecimal(value);
            normalizedTerms[i] = term;
        }

        QVector<QString> foldedTerms;
        QVector<QString> foldedOperators;
        QString currentTerm = normalizedTerms.at(0).trimmed();

        for (int i = 1; i < normalizedTerms.size(); ++i) {
            const QString op = additiveOperators.at(i - 1);
            const QString nextTerm = normalizedTerms.at(i).trimmed();

            double currentValue = 0.0;
            double nextValue = 0.0;
            const bool canFoldCurrent =
                !currentTerm.contains(QLatin1Char('%'))
                && normalizeFoldableNumericTerm(currentTerm, &currentValue);
            const bool canFoldNext =
                !nextTerm.contains(QLatin1Char('%'))
                && normalizeFoldableNumericTerm(nextTerm, &nextValue);

            if ((op == QLatin1String("+") || isMinusOperator(op))
                && canFoldCurrent
                && canFoldNext)
            {
                const double result = currentValue + (isMinusOperator(op) ? -nextValue : nextValue);
                currentTerm = formatSimplifiedDecimal(result);
                continue;
            }

            foldedTerms.append(currentTerm);
            foldedOperators.append(op);
            currentTerm = nextTerm;
        }
        foldedTerms.append(currentTerm);

        QString simplifiedPercentAware;
        simplifiedPercentAware += foldedTerms.at(0);
        for (int i = 1; i < foldedTerms.size(); ++i) {
            simplifiedPercentAware += foldedOperators.at(i - 1);
            simplifiedPercentAware += foldedTerms.at(i);
        }
        return simplifiedPercentAware;
    }

    QVector<double> constantValues;
    QVector<QString> constantSigns;
    QVector<QString> keptTerms;
    QVector<QString> keptOperators;
    for (int i = 0; i < simplifiedTerms.size(); ++i) {
        QString term = simplifiedTerms.at(i).trimmed();
        const QString op = (i == 0) ? QStringLiteral("+") : additiveOperators.at(i - 1);
        if (isSignedDecimalNumberText(term) && isSafeForDoubleArithmetic(term)) {
            bool ok = false;
            const double value = term.toDouble(&ok);
            if (ok) {
                constantValues.append(value);
                constantSigns.append(op);
                continue;
            }
        }

        if (keptTerms.isEmpty()) {
            if (isMinusOperator(op))
                keptTerms.append(QStringLiteral("-") + term);
            else
                keptTerms.append(term);
        } else {
            keptOperators.append(op);
            keptTerms.append(term);
        }
    }

    double foldedConstant = 0.0;
    for (int i = 0; i < constantValues.size(); ++i) {
        const QString op = constantSigns.at(i);
        foldedConstant += isMinusOperator(op) ? -constantValues.at(i) : constantValues.at(i);
    }

    if (!constantValues.isEmpty()) {
        if (keptTerms.isEmpty()) {
            keptTerms.append(formatSimplifiedDecimal(foldedConstant));
        } else if (foldedConstant > 0.0) {
            keptOperators.append(QStringLiteral("+"));
            keptTerms.append(formatSimplifiedDecimal(foldedConstant));
        } else if (foldedConstant < 0.0) {
            keptOperators.append(QStringLiteral("-"));
            keptTerms.append(formatSimplifiedDecimal(-foldedConstant));
        }
    }

    if (!keptTerms.isEmpty()) {
        QHash<QString, double> termCoefficients;
        QVector<QString> termOrder;
        for (int i = 0; i < keptTerms.size(); ++i) {
            QString term = keptTerms.at(i).trimmed();
            QString op = (i == 0) ? QStringLiteral("+") : keptOperators.at(i - 1);

            if (!term.isEmpty()) {
                bool termNegative = false;
                int signPos = 0;
                while (signPos < term.size()) {
                    const QChar signCh = term.at(signPos);
                    if (signCh == QLatin1Char('+')) {
                        ++signPos;
                        continue;
                    }
                    if (signCh == QLatin1Char('-') || signCh == UnicodeChars::MinusSign) {
                        termNegative = !termNegative;
                        ++signPos;
                        continue;
                    }
                    break;
                }
                if (signPos > 0) {
                    term = term.mid(signPos).trimmed();
                    if (termNegative)
                        op = isMinusOperator(op) ? QStringLiteral("+") : QStringLiteral("-");
                }
            }

            if (term.isEmpty())
                continue;

            const double signedCoefficient = isMinusOperator(op) ? -1.0 : 1.0;
            if (!termCoefficients.contains(term))
                termOrder.append(term);
            termCoefficients[term] += signedCoefficient;
        }

        QVector<QString> mergedTerms;
        QVector<QString> mergedOperators;
        for (const QString& term : termOrder) {
            const double coefficient = termCoefficients.value(term, 0.0);
            if (std::abs(coefficient) < 1e-12)
                continue;

            const bool isNegative = coefficient < 0.0;
            const double magnitude = std::abs(coefficient);

            QString mergedTerm;
            if (std::abs(magnitude - 1.0) < 1e-12) {
                mergedTerm = term;
            } else {
                mergedTerm = formatSimplifiedDecimal(magnitude)
                    + QString(UnicodeChars::DotOperator)
                    + term;
            }

            if (mergedTerms.isEmpty()) {
                mergedTerms.append(isNegative ? QStringLiteral("-") + mergedTerm : mergedTerm);
            } else {
                mergedOperators.append(isNegative ? QStringLiteral("-") : QStringLiteral("+"));
                mergedTerms.append(mergedTerm);
            }
        }

        if (!mergedTerms.isEmpty()) {
            keptTerms = mergedTerms;
            keptOperators = mergedOperators;
        }
    }

    QString simplified;
    if (!keptTerms.isEmpty()) {
        simplified += keptTerms.at(0);
        for (int i = 1; i < keptTerms.size(); ++i) {
            simplified += keptOperators.at(i - 1);
            simplified += keptTerms.at(i);
        }
    }

    return simplified.isEmpty() ? expression : simplified;
}

static QString formatInterpretedExpressionForDisplayImpl(const QString& expression,
                                                         bool simplifyRepeatedMultiplicativeBases)
{
    if (expression.isEmpty())
        return expression;

    const int commentPos = findTopLevelCommentDelimiterPos(expression);
    const QString expressionPrefix = (commentPos >= 0)
        ? expression.left(commentPos).trimmed()
        : expression;
    QString commentSuffix;
    if (commentPos >= 0) {
        const QString commentText = expression.mid(commentPos + 1).trimmed();
        commentSuffix = QLatin1String(" ?");
        if (!commentText.isEmpty())
            commentSuffix += QLatin1String(" ") + commentText;
    }

    auto scanForDisplay = [](const QString& text) {
        // Interpreted expressions are internally dot-based. For comma-based
        // display styles, scanning with a strict single-radix setting can
        // misclassify '.' as grouping separator and corrupt token structure.
        // For display-only tokenization we temporarily enable "both radix
        // chars accepted" mode, scan, then restore the previous setting.
        // This keeps display formatting stable without changing evaluation
        // behavior or persisted style.
        Settings* settings = Settings::instance();
        char previousRadix = 0;
        if (settings->isRadixCharacterBoth()) {
            previousRadix = '*';
        } else if (!settings->isRadixCharacterAuto()) {
            previousRadix = settings->radixCharacter();
        }

        settings->setRadixCharacter('*');
        const Tokens tokens = Evaluator::instance()->scan(text);
        settings->setRadixCharacter(previousRadix);
        return tokens;
    };

    const Tokens scannedTokens = scanForDisplay(expressionPrefix);
    if (scannedTokens.isEmpty())
        return expressionPrefix + commentSuffix;

    const QString groupedExpression =
        groupHighPrecedenceAdditiveTermsForDisplay(expressionPrefix, scannedTokens);
    const Tokens groupedTokens = scanForDisplay(groupedExpression);
    if (groupedTokens.isEmpty())
        return groupedExpression + commentSuffix;

    auto normalizeNeutralMultiplicativeOnes = [](const QString& text) {
        static const QRegularExpression s_trailingTimesOne(
            QStringLiteral(R"(\s*(?:\*|⋅|×)\s*1(?:[.,]0+)?\s*$)"));
        static const QRegularExpression s_leadingOneTimes(
            QStringLiteral(R"(^\s*1(?:[.,]0+)?\s*(?:\*|⋅|×)\s*)"));
        static const QRegularExpression s_trailingDivOne(
            QStringLiteral(R"(\s*(?:/|÷|⧸)\s*1(?:[.,]0+)?\s*$)"));

        QString normalized = text;
        bool changed = false;
        do {
            changed = false;
            QString updated = normalized;
            updated.replace(s_trailingTimesOne, QString());
            if (updated != normalized) {
                normalized = updated;
                changed = true;
            }

            updated = normalized;
            updated.replace(s_leadingOneTimes, QString());
            if (updated != normalized) {
                normalized = updated;
                changed = true;
            }

            updated = normalized;
            updated.replace(s_trailingDivOne, QString());
            if (updated != normalized) {
                normalized = updated;
                changed = true;
            }
        } while (changed);

        auto isFullyWrappedByOuterParens = [](const QString& expr) {
            if (expr.size() < 2 || expr.at(0) != QLatin1Char('(') || expr.at(expr.size() - 1) != QLatin1Char(')'))
                return false;
            int depth = 0;
            for (int i = 0; i < expr.size(); ++i) {
                const QChar ch = expr.at(i);
                if (ch == QLatin1Char('('))
                    ++depth;
                else if (ch == QLatin1Char(')'))
                    --depth;
                if (depth == 0 && i < expr.size() - 1)
                    return false;
            }
            return depth == 0;
        };

        while (isFullyWrappedByOuterParens(normalized))
            normalized = normalized.mid(1, normalized.size() - 2).trimmed();

        auto hasTopLevelLowPrecedenceOperator = [](const QString& expr) {
            int depth = 0;
            for (int i = 0; i < expr.size(); ++i) {
                const QChar ch = expr.at(i);
                if (ch == QLatin1Char('(')) {
                    ++depth;
                    continue;
                }
                if (ch == QLatin1Char(')')) {
                    if (depth > 0)
                        --depth;
                    continue;
                }
                if (depth != 0)
                    continue;

                if (ch == QLatin1Char('+')
                        || ch == QLatin1Char('&')
                        || ch == QLatin1Char('|')
                        || ch == QLatin1Char(';')
                        || ch == QLatin1Char('=')
                        || ch == QLatin1Char('?')
                        || ch == QLatin1Char('<')
                        || ch == QLatin1Char('>')) {
                    return true;
                }
                if ((ch == QLatin1Char('-') || ch == QChar(0x2212)) && i > 0)
                    return true;
            }
            return false;
        };

        auto isMulOp = [](QChar ch) {
            return ch == QLatin1Char('*')
                || ch == QChar(0x22C5) // ⋅
                || ch == QChar(0x00D7); // ×
        };

        auto unwrapEdgeParenthesizedMulDivFactor = [&](QString* expr) {
            QString t = expr->trimmed();
            if (t.size() < 3)
                return false;

            // Unwrap "(factor) * rest" when factor has no top-level low-precedence ops.
            if (t.at(0) == QLatin1Char('(')) {
                int depth = 0;
                int close = -1;
                for (int i = 0; i < t.size(); ++i) {
                    const QChar ch = t.at(i);
                    if (ch == QLatin1Char('('))
                        ++depth;
                    else if (ch == QLatin1Char(')')) {
                        --depth;
                        if (depth == 0) {
                            close = i;
                            break;
                        }
                    }
                }
                if (close > 0 && close < t.size() - 1) {
                    int next = close + 1;
                    while (next < t.size() && t.at(next).isSpace())
                        ++next;
                    if (next < t.size() && isMulOp(t.at(next))) {
                        const QString inner = t.mid(1, close - 1).trimmed();
                        const bool startsWithUnarySign =
                            !inner.isEmpty()
                            && (inner.at(0) == QLatin1Char('-')
                                || inner.at(0) == QChar(0x2212)
                                || inner.at(0) == QLatin1Char('+'));
                        if (!inner.isEmpty()
                                && !startsWithUnarySign
                                && !hasTopLevelLowPrecedenceOperator(inner)) {
                            t = inner + t.mid(close + 1);
                            *expr = t.trimmed();
                            return true;
                        }
                    }
                }
            }

            // Unwrap "left * (factor)" when factor has no top-level low-precedence ops.
            if (t.at(t.size() - 1) == QLatin1Char(')')) {
                int depth = 0;
                int open = -1;
                for (int i = t.size() - 1; i >= 0; --i) {
                    const QChar ch = t.at(i);
                    if (ch == QLatin1Char(')'))
                        ++depth;
                    else if (ch == QLatin1Char('(')) {
                        --depth;
                        if (depth == 0) {
                            open = i;
                            break;
                        }
                    }
                }
                if (open > 0) {
                    int prev = open - 1;
                    while (prev >= 0 && t.at(prev).isSpace())
                        --prev;
                    if (prev >= 0 && isMulOp(t.at(prev))) {
                        const QString inner = t.mid(open + 1, t.size() - open - 2).trimmed();
                        const bool startsWithUnarySign =
                            !inner.isEmpty()
                            && (inner.at(0) == QLatin1Char('-')
                                || inner.at(0) == QChar(0x2212)
                                || inner.at(0) == QLatin1Char('+'));
                        if (!inner.isEmpty()
                                && !startsWithUnarySign
                                && !hasTopLevelLowPrecedenceOperator(inner)) {
                            t = t.left(open) + inner;
                            *expr = t.trimmed();
                            return true;
                        }
                    }
                }
            }
            return false;
        };

        while (unwrapEdgeParenthesizedMulDivFactor(&normalized)) {
            // Keep unwrapping edge multiplicative wrappers until stable.
        }

        return normalized;
    };

    QString displayExpression;
    if (simplifyRepeatedMultiplicativeBases) {
        displayExpression =
            simplifyRepeatedMultiplicativeBasesForDisplay(groupedExpression, groupedTokens);
        const QString normalized = normalizeNeutralMultiplicativeOnes(displayExpression);
        if (normalized != displayExpression) {
            const Tokens normalizedTokens = Evaluator::instance()->scan(normalized);
            displayExpression = normalizedTokens.isEmpty()
                ? normalized
                : simplifyRepeatedMultiplicativeBasesForDisplay(normalized, normalizedTokens);
        }
    } else {
        displayExpression = groupedExpression;
    }
    const Tokens tokens = scanForDisplay(displayExpression);
    if (tokens.isEmpty())
        return displayExpression + commentSuffix;

    const QString operatorSpace(UnicodeChars::MediumMathematicalSpace);
    const QString unicodeMinusSign(UnicodeChars::MinusSign);
    static const QSet<QString> s_unitIdentifiers = []() {
        QSet<QString> names;
        const QList<Unit> units = Units::getList();
        for (const Unit& unit : units)
            names.insert(unit.name);
        return names;
    }();
    QString formatted;
    formatted.reserve(displayExpression.size() + tokens.size() * 2);

    for (int i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens.at(i);
        const Token::Operator op = token.asOperator();
        const bool isSpacingOperator =
            op == Token::Addition
            || op == Token::Subtraction
            || op == Token::Multiplication
            || op == Token::Division
            || op == Token::IntegerDivision
            || op == Token::UnitConversion
            || op == Token::Assignment
            || op == Token::ArithmeticLeftShift
            || op == Token::ArithmeticRightShift;
        const QString operatorText =
            op == Token::Subtraction ? unicodeMinusSign : token.text();
        const QString displayTokenText = (token.isIdentifier() && s_unitIdentifiers.contains(operatorText))
            ? Units::formatUnitTokenForDisplay(operatorText)
            : operatorText;

        if (!isSpacingOperator) {
            formatted += displayTokenText;
            continue;
        }

        const bool hasLeftOperand = i > 0
            && tokenCanEndOperandForDisplaySpacing(tokens.at(i - 1));
        const bool hasRightOperand = i + 1 < tokens.size()
            && tokenCanStartOperandForDisplaySpacing(tokens.at(i + 1));
        const bool isBinaryOperator = hasLeftOperand && hasRightOperand;
        if (!isBinaryOperator) {
            // Keep unary operators untouched (for example "−pi").
            formatted += displayTokenText;
            continue;
        }

        if ((op == Token::Addition || op == Token::Subtraction)
            && startsWithImaginaryLiteralForDisplay(tokens, i + 1)) {
            // Keep "a+bj"/"a-bj" compact in complex-mode interpreted output.
            formatted += operatorText;
            continue;
        }

        if (op == Token::Multiplication
            && i > 0
            && i + 1 < tokens.size()
            && tokens.at(i - 1).isNumber()
            && isImaginaryUnitTokenForDisplay(tokens.at(i + 1))) {
            // Render numeric imaginary literals as compact suffixes: "3j".
            continue;
        }

        formatted += operatorSpace;
        formatted += displayTokenText;
        formatted += operatorSpace;
    }

    return renderIntegerPowersAsSuperscriptsForDisplay(
        formatted,
        simplifyRepeatedMultiplicativeBases) + commentSuffix;
}

QString Evaluator::simplifyInterpretedExpression(const QString& expression)
{
    if (expression.isEmpty())
        return expression;

    const int commentPos = findTopLevelCommentDelimiterPos(expression);
    const QString expressionPrefix = (commentPos >= 0)
        ? expression.left(commentPos).trimmed()
        : expression;
    QString commentSuffix;
    if (commentPos >= 0) {
        const QString commentText = expression.mid(commentPos + 1).trimmed();
        commentSuffix = QLatin1String(" ?");
        if (!commentText.isEmpty())
            commentSuffix += QLatin1String(" ") + commentText;
    }

    auto scanForDisplay = [](const QString& text) {
        // Same display-tokenization strategy as above:
        // force temporary dual-radix scan mode so internal dot-decimal
        // interpreted text is tokenized correctly under comma-style displays.
        Settings* settings = Settings::instance();
        char previousRadix = 0;
        if (settings->isRadixCharacterBoth()) {
            previousRadix = '*';
        } else if (!settings->isRadixCharacterAuto()) {
            previousRadix = settings->radixCharacter();
        }

        settings->setRadixCharacter('*');
        const Tokens tokens = Evaluator::instance()->scan(text);
        settings->setRadixCharacter(previousRadix);
        return tokens;
    };

    const Tokens scannedTokens = scanForDisplay(expressionPrefix);
    if (scannedTokens.isEmpty())
        return expressionPrefix + commentSuffix;

    const QString groupedExpression =
        groupHighPrecedenceAdditiveTermsForDisplay(expressionPrefix, scannedTokens);
    const Tokens groupedTokens = scanForDisplay(groupedExpression);
    if (groupedTokens.isEmpty())
        return groupedExpression + commentSuffix;

    QString simplifiedExpression =
        simplifyRepeatedMultiplicativeBasesForDisplay(groupedExpression, groupedTokens);

    // Normalize neutral multiplicative factors that can block symbolic
    // cancellations in the display simplifier (for example "(a/b)*1" -> "a/b").
    static const QRegularExpression s_trailingTimesOne(
        QStringLiteral(R"(\s*(?:\*|⋅|×)\s*1(?:[.,]0+)?\s*$)"));
    static const QRegularExpression s_leadingOneTimes(
        QStringLiteral(R"(^\s*1(?:[.,]0+)?\s*(?:\*|⋅|×)\s*)"));
    static const QRegularExpression s_trailingDivOne(
        QStringLiteral(R"(\s*(?:/|÷|⧸)\s*1(?:[.,]0+)?\s*$)"));

    QString normalized = simplifiedExpression;
    bool changed = false;
    do {
        changed = false;
        QString updated = normalized;
        updated.replace(s_trailingTimesOne, QString());
        if (updated != normalized) {
            normalized = updated;
            changed = true;
        }

        updated = normalized;
        updated.replace(s_leadingOneTimes, QString());
        if (updated != normalized) {
            normalized = updated;
            changed = true;
        }

        updated = normalized;
        updated.replace(s_trailingDivOne, QString());
        if (updated != normalized) {
            normalized = updated;
            changed = true;
        }
    } while (changed);

    if (normalized != simplifiedExpression) {
        const Tokens normalizedTokens = scanForDisplay(normalized);
        if (!normalizedTokens.isEmpty())
            simplifiedExpression = simplifyRepeatedMultiplicativeBasesForDisplay(normalized, normalizedTokens);
        else
            simplifiedExpression = normalized;
    }

    return simplifiedExpression + commentSuffix;
}

QString Evaluator::formatInterpretedExpressionForDisplay(const QString& expression)
{
    return formatInterpretedExpressionForDisplayImpl(expression, false);
}

QString Evaluator::formatInterpretedExpressionSimplifiedForDisplay(const QString& expression)
{
    return formatInterpretedExpressionForDisplayImpl(expression, true);
}

static QStringList splitTopLevelFunctionArguments(const QString& text)
{
    QStringList arguments;
    int parenDepth = 0;
    int argumentStart = 0;

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == '(') {
            ++parenDepth;
        } else if (ch == ')') {
            if (parenDepth > 0)
                --parenDepth;
        } else if (ch == ';' && parenDepth == 0) {
            arguments.append(text.mid(argumentStart, i - argumentStart).trimmed());
            argumentStart = i + 1;
        }
    }

    arguments.append(text.mid(argumentStart).trimmed());
    return arguments;
}

TokenStack::TokenStack() : QVector<Token>()
{
    topIndex = 0;
    m_error = "";
    ensureSpace();
}

bool TokenStack::isEmpty() const
{
    return topIndex == 0;
}

unsigned TokenStack::itemCount() const
{
    return topIndex;
}

void TokenStack::push(const Token& token)
{
    ensureSpace();
    (*this)[topIndex++] = token;
}

Token TokenStack::pop()
{
    if (topIndex > 0)
        return Token(at(--topIndex));

    m_error = "token stack is empty (BUG)";
    return Token();
}

const Token& TokenStack::top()
{
    return top(0);
}

const Token& TokenStack::top(unsigned index)
{
    return (topIndex > (int)index) ? at(topIndex - index - 1) : Token::null;
}

void TokenStack::ensureSpace()
{
    int length = size();
    while (topIndex >= length) {
        length += 10;
        resize(length);
    }
}

/** Remove \a count tokens from the top of the stack, add a stxAbstract token
 * to the top and adjust its text position and minimum precedence.
 *
 * \param minPrecedence minimum precedence to set the top token, or
 * \c INVALID_PRECEDENCE if this method should use the minimum value from
 * the removed tokens.
 */
void TokenStack::reduce(int count, int minPrecedence)
{
    // assert(itemCount() > count);

    QList<Token> tokens;
    for (int i = 0 ; i < count ; ++i)
        tokens.append(pop());

    reduce(tokens, Token(Token::stxAbstract), minPrecedence);
}

/** Remove \a count tokens from the top of the stack, push \a top to the top
 * and adjust its text position and minimum precedence.
 *
 * \param minPrecedence minimum precedence to set the top token, or
 * \c INVALID_PRECEDENCE if this method should use the minimum value
 * from the removed tokens.
 */
void TokenStack::reduce(int count, Token&& top, int minPrecedence)
{
    // assert(itemCount() >= count);

    QList<Token> tokens;
    for (int i = 0 ; i < count ; ++i)
        tokens.append(pop());

    reduce(tokens, std::forward<Token>(top), minPrecedence);
}

/** Push \a top to the top and adjust its text position and minimum precedence
 * using \a tokens.
 *
 * \param minPrecedence minimum precedence to set the top token, or
 * \c INVALID_PRECEDENCE if this method should use the minimum value from the
 * removed tokens.
 */
void TokenStack::reduce(QList<Token> tokens, Token&& top, int minPrecedence)
{
#ifdef EVALUATOR_DEBUG
    {
        const auto& _tokens = tokens;
        qDebug() << "reduce(" << _tokens.size() << ", " << top.description()
                 << ", " << minPrecedence << ")";
        for (const auto& t : _tokens)
            qDebug() << t.description();
    }
#endif // EVALUATOR_DEBUG

    std::sort(tokens.begin(), tokens.end(), tokenPositionCompare);

    bool computeMinPrec = (minPrecedence == INVALID_PRECEDENCE);
    int min_prec = computeMinPrec ? MAX_PRECEDENCE : minPrecedence;
    int start = -1, end = -1;
    const auto& _tokens = tokens;
    for (auto& token : _tokens) {
        if (computeMinPrec) {
            Token::Operator op = token.asOperator();
            if (op != Token::Invalid) {
                int prec = opPrecedence(op);
                if (prec < min_prec)
                    min_prec = prec;
            }
        }

        if (token.pos() == -1 && token.size() == -1)
            continue;

        if (token.pos() == -1 || token.size() == -1) {

#ifdef EVALUATOR_DEBUG
            qDebug() << "BUG: found token with either pos or size not set, "
                        "but not both.";
#endif  // EVALUATOR_DEBUG
            continue;
        }

        if (start == -1) {
            start = token.pos();
        } else {

#ifdef EVALUATOR_DEBUG
            if (token.pos() != end)
                qDebug() << "BUG: tokens expressions are not successive.";
#endif  // EVALUATOR_DEBUG

        }

        end = token.pos() + token.size();
    }

    if (start != -1) {
        top.setPos(start);
        top.setSize(end - start);
    }

    top.setMinPrecedence(min_prec);

#ifdef EVALUATOR_DEBUG
    qDebug() << "=> " << top.description();
#endif  // EVALUATOR_DEBUG

    push(top);
}

#ifdef EVALUATOR_DEBUG
void Tokens::append(const Token& token)
{
    qDebug() << QString("tokens.append: type=%1 text=%2")
                    .arg(token.type())
                    .arg(token.text());
    QVector<Token>::append(token);
}
#endif // EVALUATOR_DEBUG

static bool isSubscriptDigit(QChar ch)
{
    return ch.unicode() >= 0x2080 && ch.unicode() <= 0x2089;
}

static bool isSuperscriptDigit(QChar ch)
{
    switch (ch.unicode()) {
    case 0x2070: // ⁰
    case 0x00B9: // ¹
    case 0x00B2: // ²
    case 0x00B3: // ³
    case 0x2074: // ⁴
    case 0x2075: // ⁵
    case 0x2076: // ⁶
    case 0x2077: // ⁷
    case 0x2078: // ⁸
    case 0x2079: // ⁹
        return true;
    default:
        return false;
    }
}

static QString superscriptDigitsToAscii(const QString& text)
{
    QString converted;
    converted.reserve(text.size());
    for (const QChar& ch : text) {
        switch (ch.unicode()) {
        case 0x2070: converted.append(QLatin1Char('0')); break; // ⁰
        case 0x00B9: converted.append(QLatin1Char('1')); break; // ¹
        case 0x00B2: converted.append(QLatin1Char('2')); break; // ²
        case 0x00B3: converted.append(QLatin1Char('3')); break; // ³
        case 0x2074: converted.append(QLatin1Char('4')); break; // ⁴
        case 0x2075: converted.append(QLatin1Char('5')); break; // ⁵
        case 0x2076: converted.append(QLatin1Char('6')); break; // ⁶
        case 0x2077: converted.append(QLatin1Char('7')); break; // ⁷
        case 0x2078: converted.append(QLatin1Char('8')); break; // ⁸
        case 0x2079: converted.append(QLatin1Char('9')); break; // ⁹
        default: converted.append(ch); break;
        }
    }
    return converted;
}

static bool isSubscriptLetter(QChar ch)
{
    switch (ch.unicode()) {
    case 0x2090: // ₐ
    case 0x2091: // ₑ
    case 0x2092: // ₒ
    case 0x2093: // ₓ
    case 0x2094: // ₔ
    case 0x2095: // ₕ
    case 0x2096: // ₖ
    case 0x2097: // ₗ
    case 0x2098: // ₘ
    case 0x2099: // ₙ
    case 0x209A: // ₚ
    case 0x209B: // ₛ
    case 0x209C: // ₜ
    case 0x1D62: // ᵢ
    case 0x1D63: // ᵣ
    case 0x1D64: // ᵤ
    case 0x1D65: // ᵥ
    case 0x1D66: // ᵦ
    case 0x1D67: // ᵧ
    case 0x1D68: // ᵨ
    case 0x1D69: // ᵩ
    case 0x1D6A: // ᵪ
        return true;
    default:
        return false;
    }
}

// Helper function: return true for valid identifier start character.
static bool isIdentifierStart(QChar ch)
{
    return ch.unicode() == '_'
           || ch.unicode() == '$'
           || ch.isLetter()
           || isSubscriptLetter(ch)
           || ch == UnicodeChars::SquareRoot
           || ch == UnicodeChars::CubeRoot;
}

// Helper function: return true for valid identifier continuation character.
static bool isIdentifierContinue(QChar ch)
{
    return isIdentifierStart(ch) || ch.isDigit() || isSubscriptDigit(ch);
}

// Helper function: return true for valid radix characters.
bool Evaluator::isRadixChar(const QChar& ch)
{
    // Accept both '.' and ',' as potential radix markers at tokenization time.
    // Final decimal/grouping disambiguation is done later in fixNumberRadix().
    if (ch.unicode() == '.' || ch.unicode() == ',')
        return true;

    if (Settings::instance()->isRadixCharacterBoth())
        return ch.unicode() == '.' || ch.unicode() == ',';

    // There are more than 2 radix characters, actually:
    //     U+0027 ' apostrophe
    //     U+002C , comma
    //     U+002E . full stop
    //     U+00B7 · middle dot
    //     U+066B ٫ arabic decimal separator
    //     U+2396 ⎖ decimal separator key symbol

    return ch.unicode() == Settings::instance()->radixCharacter();
}

// Helper function: return true for valid thousand separator characters.
bool Evaluator::isSeparatorChar(const QChar& ch)
{
    // Never treat letters or digits from any script as grouping separators.
    // This avoids silently collapsing expressions like "12天2" into "122".
    if (ch.isLetterOrNumber())
        return false;

    if (ch == UnicodeChars::SquareRoot || ch == UnicodeChars::CubeRoot)
        return false;

    if (isRadixChar(ch))
        return false;

    return RegExpPatterns::separatorToken().match(ch).hasMatch();
}

bool Evaluator::isCommentOnlyExpression(const QString& expr)
{
    for (QChar ch : expr) {
        if (!ch.isSpace())
            return ch == '?';
    }

    return false;
}


QString Evaluator::fixNumberRadix(const QString& number)
{
    int dotCount = 0;
    int commaCount = 0;
    QChar lastRadixChar;

    // First pass: count dot/comma occurrences independently of the selected
    // radix setting. This keeps number parsing tolerant to copied values that
    // use a different but obvious decimal/grouping convention than the active
    // UI format (for example "1,56" while dot style is selected).
    for (int i = 0 ; i < number.size() ; ++i) {
        QChar c = number[i];
        if (c == '.' || c == ',') {
            lastRadixChar = c;
            if (c == '.')
                ++dotCount;
            else
                ++commaCount;
        }
    }

    // Decide which radix characters to ignore based on their occurence count.
    bool ignoreDot = dotCount != 1;
    bool ignoreComma = commaCount != 1;
    if (!ignoreDot && !ignoreComma) {
        // If both radix characters are present once,
        // consider the last one as the radix point.
        ignoreDot = lastRadixChar != '.';
        ignoreComma = lastRadixChar != ',';
    }

    QChar radixChar; // Null character by default.
    if (!ignoreDot)
        radixChar = '.';
    else if (!ignoreComma)
        radixChar = ',';

    // Second pass: write the result.
    QString result = "";
    for (int i = 0 ; i < number.size() ; ++i) {
        QChar c = number[i];
        if (c == '.' || c == ',') {
            if (c == radixChar)
                result.append('.');
        } else
          result.append(c);
    }

    return result;
}

HNumber getNumber(const QString& number) {
    if (number.isEmpty())
        return HNumber(0);
    const QString sexagesimalStopChars =
        QStringLiteral("['\":") + UnicodeChars::Prime + UnicodeChars::DoublePrime + QLatin1Char(']');
    int endPos = number.indexOf(QRegularExpression(sexagesimalStopChars)); // Stop at sexagesimal separators/markers.
    if (endPos == 0)
        return HNumber(0);
    if (endPos > 0)
        return HNumber(Evaluator::fixNumberRadix(number.left(endPos)).toStdString().c_str());
    return HNumber(Evaluator::fixNumberRadix(number).toStdString().c_str());
}

QString Evaluator::fixSexagesimal(const QString& number, QString& unit)
{
    unit.clear();
    QString bad, result = number;
    const QString minuteMarkClass = QStringLiteral("['") + UnicodeChars::Prime + QLatin1Char(']');
    const QString secondMarkClass = QStringLiteral("[\"") + UnicodeChars::DoublePrime + QLatin1Char(']');
    const auto isMinuteMark = [](QChar c) {
        return c == QChar('\'') || c == UnicodeChars::Prime;
    };
    const auto isSecondMark = [](QChar c) {
        return c == QChar('"') || c == UnicodeChars::DoublePrime;
    };

    bool arc = false;
    int colonCount = 0, degreeCount = 0, minuteCount = 0, secondCount = 0, digitCount = 0;
    for (int i = 0 ; i < number.size() ; ++i) { // check order and amount
        QChar c = number[i];
        if (isDegreeSign(c)) {
            if (colonCount || minuteCount || secondCount)
                return bad;
            if (++degreeCount > 1)
                return bad;
            arc = true;
        }
        else if (c == ':') {
            if (minuteCount || degreeCount || secondCount)
                return bad;
            if (++colonCount > 2)
                return bad;
        }
        else if (isMinuteMark(c)) {
            if (secondCount)
                return bad;
            if (++minuteCount > 1)
                return bad;
            arc = true;
        }
        else if (isSecondMark(c)) {
            if (degreeCount && !minuteCount)
                return bad;
            if (++secondCount > 1)
                return bad;
            arc = true;
        }
        else if (c.isDigit())
            ++digitCount;
    }

    if (colonCount || degreeCount || minuteCount || secondCount) {  // sexagesimal value
        if (digitCount == 0)
            return bad;
        HNumber::Format fixed = HNumber::Format::Fixed();
        HNumber mains(0), minutes(0), seconds(0), sign(1);
        int minPos = number.indexOf(arc ? QRegularExpression("[\\x{00B0}\\x{00BA}\\x{02DA}\\x{2218}]")
                                        : QRegularExpression(":"));
        if (minPos >= 0) {  // degree sign or first colon -> minutes
            mains = getNumber(number.left(minPos));    // hours or degrees
            if (mains.isNegative())
                sign = HNumber(-1);
            int secPos = number.indexOf(arc ? QRegularExpression(minuteMarkClass)
                                            : QRegularExpression(":"),
                                        minPos + 1);
            if (secPos >= 0) {  // single quote or second colon -> seconds
                minutes = getNumber(number.mid(minPos + 1));
                seconds = getNumber(number.mid(secPos + 1));
                if (seconds.isZero()) {  // postfix minutes
                    result = HMath::format(mains * HNumber(60) + minutes * sign, fixed);
                    unit = arc ? "arcminute" : "minute";
                }
                else {  // minutes and seconds
                    result = HMath::format(mains * HNumber(3600) + minutes * HNumber(60) * sign + seconds * sign, fixed);
                    unit = arc ? "arcsecond" : "second";
                }
            }
            else {  // just minutes
                minutes = getNumber(number.mid(minPos + 1));
                result = HMath::format(mains * HNumber(60) + minutes * sign, fixed);
                unit = arc ? "arcminute" : "minute";
            }
            if (seconds.isZero() && minutes.isZero()) {  // postfix mains
                result = HMath::format(mains, fixed);
                unit = arc ? "degree" : "hour";
            }
        }
        else if ( arc ) {
            int secPos = number.indexOf(QRegularExpression(minuteMarkClass));
            if (secPos >= 0) {  // single quote -> seconds
                minutes = getNumber(number.left(secPos));
                if (minutes.isNegative())
                    sign = HNumber(-1);
                seconds = getNumber(number.mid(secPos + 1));
                if (seconds.isZero()) {  // postfix minutes
                    result = HMath::format(minutes, fixed);
                    unit = "arcminute";
                }
                else {
                    result = HMath::format(minutes * HNumber(60) + seconds * sign, fixed);
                    unit = "arcsecond";
                }
            }
            else {
                int unitPos = number.indexOf(QRegularExpression(secondMarkClass));
                if (unitPos >= 0) {  // postfix seconds
                    seconds = getNumber(number.left(unitPos));
                    result = HMath::format(seconds, fixed);
                    unit = "arcsecond";
                }
            }
        }
        int unitPos = number.indexOf(QRegularExpression(secondMarkClass));  // check digits after seconds unit
        if (unitPos >= 0 && !getNumber(number.mid(unitPos + 1)).isZero())
            return bad;
        if (!seconds.isZero() && (!minutes.isInteger() || !mains.isInteger()))
            return bad;
        if (!minutes.isZero() && !mains.isInteger())
            return bad;
        const bool hasScientificExponent =
            number.contains(QLatin1Char('e'), Qt::CaseInsensitive);
        int dotNumber = number.lastIndexOf(QRegularExpression("[.,]"));
        if (dotNumber >= 0 && !hasScientificExponent) {  // append decimals, remove possible postfix units
            int minPos = number.indexOf(QRegularExpression(minuteMarkClass));
            int secPos = number.indexOf(QRegularExpression(secondMarkClass));
            int unitPos = (secPos >= 0 && secPos < minPos) ? secPos : minPos;
            int dotResult = result.lastIndexOf('.');
            if (dotResult >= 0)  // replace decimals with original ones for accuracy
                result.resize(dotResult);
            result += number.mid(dotNumber, unitPos < 0 ? -1 : unitPos - dotNumber);
        }
    }
    else
        result = fixNumberRadix(result);

    return result;
}

Evaluator* Evaluator::instance()
{
    if (!s_evaluatorInstance) {
        s_evaluatorInstance = new Evaluator;
        qAddPostRoutine(s_deleteEvaluator);
    }
    return s_evaluatorInstance;
}

Evaluator::Evaluator()
{
    reset();
}

#define ADD_UNIT(name) \
    setVariable(QString::fromUtf8(#name), Units::name(), Variable::BuiltIn)

void Evaluator::initializeBuiltInVariables()
{
    setVariable(QLatin1String("e"), DMath::e(), Variable::BuiltIn);
    setVariable(QString::fromUtf8("ℯ"), DMath::e(), Variable::BuiltIn);

    setVariable(QLatin1String("pi"), DMath::pi(), Variable::BuiltIn);
    setVariable(QString::fromUtf8("π"), DMath::pi(), Variable::BuiltIn);

    if (Settings::instance()->complexNumbers) {
        setVariable(QLatin1String("i"), DMath::i(), Variable::BuiltIn);
        setVariable(QLatin1String("j"), DMath::i(), Variable::BuiltIn);
    } else {
        if (hasVariable("i"))
            unsetVariable("i", ForceBuiltinVariableErasure(true));
        if (hasVariable("j"))
            unsetVariable("j", ForceBuiltinVariableErasure(true));
    }

    QList<Unit> unitList(Units::getList());
    for (Unit& u : unitList) {
        Quantity unitValue = u.value;
        // Preserve display names for units where alias identity or semantic
        // context matters during expression propagation/canonicalization.
        const bool isInformationUnit =
            unitValue.getDimension().contains(QLatin1String("information"));
        const bool keepDisplayUnit =
            u.name == QLatin1String("hertz")
            || u.name == QLatin1String("becquerel")
            || u.name == QLatin1String("gray")
            || u.name == QLatin1String("sievert")
            || u.name == QLatin1String("steradian")
            || u.name == QLatin1String("sr")
            || u.name == QLatin1String("pascal")
            || u.name == QLatin1String("newton")
            || u.name == QLatin1String("meter")
            || isInformationUnit;
        if (keepDisplayUnit)
            unitValue.setDisplayUnit(u.value.numericValue(), u.name);
        setVariable(u.name, unitValue, Variable::BuiltIn);
    }
    {
        Quantity hz = Units::hertz();
        hz.setDisplayUnit(Units::hertz().numericValue(), "hertz");
        setVariable("Hz", hz, Variable::BuiltIn);
    }
    {
        Quantity bq = Units::becquerel();
        bq.setDisplayUnit(Units::becquerel().numericValue(), "becquerel");
        setVariable("Bq", bq, Variable::BuiltIn);
    }

    initializeAngleUnits();
}

void Evaluator::initializeAngleUnits()
{
    // Explicit radian units are absolute and independent of the selected
    // global angle mode.
    {
        Quantity displayRad(1);
        displayRad.setDisplayUnit(Quantity(1).numericValue(), "rad");
        setVariable("radian", displayRad, Variable::BuiltIn);
        setVariable("rad", displayRad, Variable::BuiltIn);
    }

    if (Settings::instance()->angleUnit == 'r') {
        setVariable("degree", HMath::pi() / HNumber(180), Variable::BuiltIn);
        setVariable("deg", HMath::pi() / HNumber(180), Variable::BuiltIn);
        setVariable("gradian", HMath::pi() / HNumber(200), Variable::BuiltIn);
        setVariable("gon", HMath::pi() / HNumber(200), Variable::BuiltIn);
        setVariable("turn", HNumber(2) * HMath::pi(), Variable::BuiltIn);
        setVariable("arcminute", HMath::pi() / HNumber(180) / HNumber(60), Variable::BuiltIn);
        setVariable("arcsecond", HMath::pi() / HNumber(180) / HNumber(3600), Variable::BuiltIn);
        setVariable("arcmin", HMath::pi() / HNumber(180) / HNumber(60), Variable::BuiltIn);
        setVariable("arcsec", HMath::pi() / HNumber(180) / HNumber(3600), Variable::BuiltIn);
    } else if (Settings::instance()->angleUnit == 'g') {
        setVariable("degree", HNumber(200) / HNumber(180), Variable::BuiltIn);
        setVariable("deg", HNumber(200) / HNumber(180), Variable::BuiltIn);
        setVariable("gradian", 1, Variable::BuiltIn);
        setVariable("gon", 1, Variable::BuiltIn);
        setVariable("turn", HNumber(400), Variable::BuiltIn);
        setVariable("arcminute", HNumber(200) / HNumber(180) / HNumber(60), Variable::BuiltIn);
        setVariable("arcsecond", HNumber(200) / HNumber(180) / HNumber(3600), Variable::BuiltIn);
        setVariable("arcmin", HNumber(200) / HNumber(180) / HNumber(60), Variable::BuiltIn);
        setVariable("arcsec", HNumber(200) / HNumber(180) / HNumber(3600), Variable::BuiltIn);
    } else if (Settings::instance()->angleUnit == 't') {
        setVariable("degree", HNumber(1) / HNumber(360), Variable::BuiltIn);
        setVariable("deg", HNumber(1) / HNumber(360), Variable::BuiltIn);
        setVariable("gradian", HNumber(1) / HNumber(400), Variable::BuiltIn);
        setVariable("gon", HNumber(1) / HNumber(400), Variable::BuiltIn);
        setVariable("turn", 1, Variable::BuiltIn);
        setVariable("arcminute", HNumber(1) / HNumber(21600), Variable::BuiltIn);
        setVariable("arcsecond", HNumber(1) / HNumber(1296000), Variable::BuiltIn);
        setVariable("arcmin", HNumber(1) / HNumber(21600), Variable::BuiltIn);
        setVariable("arcsec", HNumber(1) / HNumber(1296000), Variable::BuiltIn);
    } else {    // d
        setVariable("degree", 1, Variable::BuiltIn);
        setVariable("deg", 1, Variable::BuiltIn);
        setVariable("gradian", HNumber(180) / HNumber(200), Variable::BuiltIn);
        setVariable("gon", HNumber(180) / HNumber(200), Variable::BuiltIn);
        setVariable("turn", HNumber(360), Variable::BuiltIn);
        setVariable("arcminute", HNumber(1) / HNumber(60), Variable::BuiltIn);
        setVariable("arcsecond", HNumber(1) / HNumber(3600), Variable::BuiltIn);
        setVariable("arcmin", HNumber(1) / HNumber(60), Variable::BuiltIn);
        setVariable("arcsec", HNumber(1) / HNumber(3600), Variable::BuiltIn);
    }
}

void Evaluator::setExpression(const QString& expr)
{
    m_expression = expr;
    m_dirty = true;
    m_valid = false;
    m_error = QString();
    m_interpretedExpression = QString();
    m_hasImplicitMultiplication = false;
}

QString Evaluator::expression() const
{
    return m_expression;
}

// Returns the validity. Note: empty expression is always invalid.
bool Evaluator::isValid()
{
    if (m_dirty) {
        Tokens tokens = scan(m_expression);
        if (!tokens.valid())
            compile(tokens);
        else
            m_valid = false;
    }
    return m_valid;
}

void Evaluator::reset()
{
    m_expression = QString();
    m_dirty = true;
    m_valid = false;
    m_error = QString();
    m_interpretedExpression = QString();
    m_constants.clear();
    m_constantTexts.clear();
    m_codes.clear();
    m_implicitMultiplicationOpcodeIndices.clear();
    m_assignId = QString();
    m_assignFunc = false;
    m_assignArg.clear();
    m_assignVarDescription = QString();
    m_assignFuncExpr = QString();
    m_assignFuncDescription = QString();
    m_session = nullptr;
    m_functionsInUse.clear();
    m_hasImplicitMultiplication = false;

    initializeBuiltInVariables();
}

void Evaluator::setSession(Session* s)
{
    m_session = s;
}

const Session* Evaluator::session()
{
    return m_session;
}

QString Evaluator::error() const
{
    return m_error;
}

QString Evaluator::interpretedExpression() const
{
    return m_interpretedExpression;
}

bool Evaluator::hasImplicitMultiplication() const
{
    return m_hasImplicitMultiplication;
}

// Returns list of token for the expression.
// This triggers again the lexical analysis step. It is however preferable
// (even when there's small performance penalty) because otherwise we need to
// store parsed tokens all the time which serves no good purpose.
Tokens Evaluator::tokens() const
{
    return scan(m_expression);
}

bool isArcTime(QChar ch) {
    return (isDegreeSign(ch)
            || ch == '\''
            || ch == '"'
            || ch == UnicodeChars::Prime
            || ch == UnicodeChars::DoublePrime
            || ch == ':');
}

Tokens Evaluator::scan(const QString& expr) const
{
    // Associate character codes with the highest number base
    // they might belong to.
    constexpr unsigned DIGIT_MAP_COUNT = 128;
    static unsigned char s_digitMap[DIGIT_MAP_COUNT] = { 0 };

    if (s_digitMap[0] == 0) {
        // Initialize the digits map.
        std::fill_n(s_digitMap, DIGIT_MAP_COUNT, 255);
        for (int i = '0' ; i <= '9' ; ++i)
            s_digitMap[i] = i - '0' + 1;
        for (int i = 'a' ; i <= 'z' ; ++i)
            s_digitMap[i] = i - 'a' + 11;
        for (int i = 'A' ; i <= 'Z' ; ++i)
            s_digitMap[i] = i - 'A' + 11;
    }

    // Result.
    Tokens tokens;

    // Parsing state.
    enum {
        Init, Start, Finish, Bad, InNumberPrefix, InNumber, InExpIndicator,
        InExponentBase, InExponent, InIdentifier, InNumberEnd
    } state;

    // Initialize variables.
    state = Init;
    int i = 0;
    QString ex = UnicodeChars::normalizeUnitSymbolAliases(expr);
    ex.replace(UnicodeChars::Prime, QChar('\''));
    ex.replace(UnicodeChars::DoublePrime, QChar('"'));
    // Accept superscript shorthand aliases for unit symbols (m², mm³, etc.)
    // by rewriting to ASCII aliases (m2, mm3) when the target exists as a
    // built-in unit variable.
    for (int p = 0; p < ex.size();) {
        if (!isIdentifierStart(ex.at(p))) {
            ++p;
            continue;
        }

        int identEnd = p + 1;
        while (identEnd < ex.size() && isIdentifierContinue(ex.at(identEnd)))
            ++identEnd;

        int superscriptEnd = identEnd;
        while (superscriptEnd < ex.size() && isSuperscriptDigit(ex.at(superscriptEnd)))
            ++superscriptEnd;

        if (superscriptEnd == identEnd) {
            p = identEnd;
            continue;
        }

        const QString identifier = ex.mid(p, identEnd - p);
        const QString superscript = ex.mid(identEnd, superscriptEnd - identEnd);
        const QString candidate = identifier + superscriptDigitsToAscii(superscript);
        if (isBuiltInVariable(candidate)) {
            ex.replace(p, superscriptEnd - p, candidate);
            p += candidate.size();
        } else {
            p = superscriptEnd;
        }
    }
    QString tokenText, tokenUnit;
    int tokenStart = 0; // Includes leading spaces.
    Token::Type type;
    int numberBase = 10;
    int expBase = 0;
    int expStart = -1;  // Index of the exponent part in the expression.
    QString expText;    // Start of the exponent text matching /E[\+\-]*/

    // Force a terminator.
    ex.append(QChar());

#ifdef EVALUATOR_DEBUG
    qDebug() << "Scanning" << ex;
#endif // EVALUATOR_DEBUG

    // Main loop.
    while (state != Bad && state != Finish && i < ex.length()) {
        QChar ch = ex.at(i);

#ifdef EVALUATOR_DEBUG
        qDebug() << QString("state=%1 ch=%2 i=%3 tokenText=%4")
                            .arg(state).arg(ch).arg(i).arg(tokenText);
#endif // EVALUATOR_DEBUG

        switch (state) {
        case Init:
            tokenStart = i;
            tokenText = "";
            tokenUnit = "";
            state = Start;

            // State variables reset
            expStart = -1;
            expText = "";
            numberBase = 10;
            expBase = 0;

            // C++17: [[fallthrough]];
            // fall through

        case Start:
            // Skip any whitespaces.
            if (ch.isSpace())
                ++i;
            else if (ch == '?') // Comment.
                state = Finish;
            else if (ch.isDigit()) {
                // Check for number
                state = InNumberPrefix;
            } else if (ch == '#') {
                // Simple hexadecimal notation
                tokenText.append("0x");
                numberBase = 16;
                state = InNumber;
                ++i;
            } else if (isRadixChar(ch)) {
                // Radix character?
                tokenText.append(ch);
                numberBase = 10;
                state = InNumber;
                ++i;
            } else if (ch == '~') {
                int tokenSize = ++i - tokenStart;
                tokens.append(Token(Token::stxOperator, "~", tokenStart, tokenSize));
                state = Init;
            } else if (isSeparatorChar(ch)) {
                // Leading separator, probably a number
                state = InNumberPrefix;
            } else if (ch.isNull()) // Terminator character.
                state = Finish;
            else if (isIdentifierStart(ch)) // Identifier or alphanumeric operator
                state = InIdentifier;
            else { // Look for operator match.
                int op;
                QString s;
                s = QString(ch).append(ex.at(i+1));
                op = matchOperator(s);
                // Check for one-char operator.
                if (op == Token::Invalid) {
                    s = QString(ch);
                    op = matchOperator(s);
                }
                // Any matched operator?
                if (op != Token::Invalid) {
                    switch(op) {
                        case Token::AssociationStart:
                            type = Token::stxOpenPar;
                            break;
                        case Token::AssociationEnd:
                            type = Token::stxClosePar;
                            break;
                        case Token::ListSeparator:
                            type = Token::stxSep;
                            break;
                        default: type = Token::stxOperator;
                    }
                    int len = s.length();
                    i += len;
                    int tokenSize = i - tokenStart;
                    tokens.append(Token(type, s.left(len),
                                        tokenStart, tokenSize));
                    state = Init;
                }
                else
                    state = Bad;
            }
            break;

        // Manage both identifier and alphanumeric operators.
        case InIdentifier:
            // Consume as long as alpha, dollar sign, underscore, or digit.
            if (isIdentifierContinue(ch))
                tokenText.append(ex.at(i++));
            else { // We're done with identifier.
                QString identifier = tokenText;
                if (identifier.startsWith(QLatin1Char('u')) && identifier.size() > 1) {
                    QString microIdentifier(identifier);
                    microIdentifier[0] = UnicodeChars::MicroSign;
                    if (isBuiltInVariable(microIdentifier))
                        identifier = microIdentifier;
                }
                int tokenSize = i - tokenStart;
                if (matchOperator(identifier)) {
                    tokens.append(Token(Token::stxOperator, identifier,
                                        tokenStart, tokenSize));
                } else {
                    // Normal identifier.
                    tokens.append(Token(Token::stxIdentifier, identifier,
                                        tokenStart, tokenSize));
                }
                state = Init;
            }
            break;

        // Find out the number base.
        case InNumberPrefix:
            if (ch.isDigit()) {
                // Only consume the first digit and the second digit
                // if the first was 0.
                tokenText.append(ex.at(i++));
                if (tokenText != "0") {
                    numberBase = 10;
                    state = InNumber;
                }
            } else if (isArcTime(ch)) {
                // Time/Degree notation
                numberBase = 10;
                tokenText.append(ex.at(i++));
                state = InNumber;
            } else if (isExponent(ch, numberBase)) {
                if (tokenText.endsWith("0")) {
                    // Maybe exponent (tokenText is "0" or "-0").
                    numberBase = 10;
                    expText = ch.toUpper();
                    expStart = i;
                    ++i;
                    state = InExpIndicator;
                } else {
                    // Only leading separators.
                    state = Bad;
                }
            } else if (isRadixChar(ch)) {
                // Might be a radix point or a separator.
                // Collect it and decide later.
                tokenText.append(ch);
                numberBase = 10;
                state = InNumber;
                ++i;
            } else if (ch.toUpper() == 'X' && tokenText == "0") {
                // Hexadecimal number.
                numberBase = 16;
                tokenText.append('x');
                ++i;
                state = InNumber;
            } else if (ch.toUpper() == 'B' && tokenText == "0") {
                // Binary number.
                numberBase = 2;
                tokenText.append('b');
                ++i;
                state = InNumber;
            } else if (ch.toUpper() == 'O' && tokenText == "0") {
                // Octal number.
                numberBase = 8;
                tokenText.append('o');
                ++i;
                state = InNumber;
            } else if (ch.toUpper() == 'D' && tokenText == "0") {
                // Decimal number (with prefix).
                numberBase = 10;
                tokenText.append('d');
                ++i;
                state = InNumber;
            } else if (isSeparatorChar(ch)) {
                // Ignore thousand separators.
                ++i;
            } else if (tokenText.isEmpty() && (ch == '+' || isMinus(ch))) {
                // Allow expressions like "$-10" or "$+10".
                if (isMinus(ch))
                    tokenText.append('-');
                ++i;
            } else {
                if (tokenText.endsWith("0")) {
                    // Done with integer number (tokenText is "0" or "-0").
                    numberBase = 10;
                    state = InNumberEnd;
                } else {
                    // Only leading separators.
                    state = Bad;
                }
            }
            break;

        // Parse the number digits.
        case InNumber: {
            ushort c = ch.unicode();
            bool isDigit = c < DIGIT_MAP_COUNT && (s_digitMap[c] <= numberBase);

            if (isDigit) {
                // Consume as long as it's a digit
                tokenText.append(ex.at(i++).toUpper());
            } else if (isArcTime(ch)) {
                // Time/Degree notation
                tokenText.append(ex.at(i++));
            } else if (isExponent(ch, numberBase)) {
                // Maybe exponent
                expText = ch.toUpper();
                expStart = i;
                ++i;
                tokenText = fixNumberRadix(tokenText);
                if (!tokenText.isNull())
                    state = InExpIndicator;
                else
                    state = Bad;
            } else if (isRadixChar(ch)) {
                // Might be a radix point or a separator.
                // Collect it and decide later.
                tokenText.append(ch);
                ++i;
            } else if (isSeparatorChar(ch)) {
                // Ignore thousand separators.
                ++i;
            } else {
                // We're done with number.
                if (numberBase == 10)
                    tokenText = fixSexagesimal(tokenText, tokenUnit);
                else
                    tokenText = fixNumberRadix(tokenText);
                if (!tokenText.isNull())
                    state = InNumberEnd;
                else
                    state = Bad;
            }

            break;
        }

        // Validate exponent start.
        case InExpIndicator: {
            ushort c = ch.unicode();
            bool isDigit = c < DIGIT_MAP_COUNT && (s_digitMap[c] <= numberBase);

            if (expBase == 0) {
                // Set the default exponent base (same as number)
                expBase = numberBase;
            }

            if (expText.length() == 1 && (ch == '+' || isMinus(ch))) {
                // Possible + or - right after E.
                expText.append(ch == UnicodeChars::MinusSign ? '-' : ch);
                ++i;
            } else if (isDigit) {
                if (ch == '0') {
                    // Might be a base prefix
                    expText.append(ch);
                    ++i;
                    state = InExponentBase;
                } else {
                    // Parse the exponent absolute value.
                    tokenText.append(expText);
                    state = InExponent;
                }
            } else if (isSeparatorChar(ch)) {
                // Ignore thousand separators.
                ++i;
            } else {
                // Invalid thing here. Rollback: might be an identifier
                // used in implicit multiplication.
                i = expStart;
                state = InNumberEnd;
            }

            break;
        }

        // Detect exponent base.
        case InExponentBase: {
            int base = -1;
            switch (ch.toUpper().unicode()) {
                case 'B':
                    base = 2;
                    break;
                case 'O':
                    base = 8;
                    break;
                case 'D':
                    base = 10;
                    break;
                case 'X':
                    base = 16;
                    break;
            }

            if (base != -1) {
                // Specific exponent base found
                expBase = base;
                tokenText.append(expText);
                tokenText.append(ch.toLower());
                ++i;
            } else {
                // No exponent base specified, use the default one
                tokenText.append(expText);
            }

            state = InExponent;

            break;
        }

        // Parse exponent.
        case InExponent: {
            ushort c = ch.unicode();
            bool isDigit = c < DIGIT_MAP_COUNT && (s_digitMap[c] <= expBase);

            if (isDigit) {
                // Consume as long as it's a digit.
                tokenText.append(ex.at(i++));
            } else if (isArcTime(ch)) {
                // Allow sexagesimal markers right after scientific notation,
                // e.g. "8.4381406e4\"" or "8.4381406e4″".
                tokenText.append(ex.at(i++));
                state = InNumber;
            } else if (isSeparatorChar(ch)) {
                // Ignore thousand separators.
                ++i;
            } else {
                // We're done with floating-point number.
                state = InNumberEnd;
            };

            break;
        }

        case InNumberEnd: {
            int tokenSize = i - tokenStart;
            if (!tokenUnit.isEmpty()) {
                // Keep the sexagesimal number and its generated unit together:
                // e.g. "1:30 / 4:00" should parse as
                // "(90 minute) / (4 hour)", not "(90 minute / 4) hour".
                tokens.append(Token(Token::stxOpenPar, "(", tokenStart, 0));
                tokens.append(Token(Token::stxNumber, tokenText,
                                    tokenStart, tokenSize));
                tokens.append(Token(Token::stxIdentifier, tokenUnit,
                    tokenStart + tokenSize, 0));
                tokens.append(Token(Token::stxClosePar, ")", tokenStart + tokenSize, 0));
            } else {
                tokens.append(Token(Token::stxNumber, tokenText,
                                    tokenStart, tokenSize));
            }

            // Make sure a number cannot be followed by another number.
            if (ch.isDigit() || isRadixChar(ch) || ch == '#')
                state = Bad;
            else
                state = Init;
            break;
        }

        case Bad:
            tokens.setValid(false);
            break;

        default:
            break;
        };
    }

    if (state == Bad)
        // Invalidating here too, because usually when we set state to Bad,
        // the case Bad won't be run.
        tokens.setValid(false);

    return tokens;
}

void Evaluator::compile(const Tokens& tokens)
{
#ifdef EVALUATOR_DEBUG
    QFile debugFile("eval.log");
    if (!debugFile.open(QIODevice::WriteOnly))
        qWarning() << "Could not open eval.log for writing:" << debugFile.errorString();
    QTextStream dbg(&debugFile);
#endif

    // Initialize variables.
    m_dirty = false;
    auto isGeneratedSexagesimalUnitToken = [](const Token& token) {
        if (token.type() != Token::stxIdentifier || token.size() != 0)
            return false;
        const QString text = token.text();
        return text == QLatin1String("hour")
            || text == QLatin1String("minute")
            || text == QLatin1String("second")
            || text == QLatin1String("degree")
            || text == QLatin1String("arcminute")
            || text == QLatin1String("arcsecond");
    };

    bool hasGeneratedSexagesimalUnit = false;
    for (int idx = 0; idx < tokens.size(); ++idx) {
        if (isGeneratedSexagesimalUnitToken(tokens.at(idx))) {
            hasGeneratedSexagesimalUnit = true;
            break;
        }
    }

    m_valid = false;
    m_codes.clear();
    m_implicitMultiplicationOpcodeIndices.clear();
    m_constants.clear();
    m_constantTexts.clear();
    m_identifiers.clear();
    m_error = QString();
    m_interpretedExpression = QString();
    m_hasImplicitMultiplication = false;

    // Sanity check.
    if (tokens.count() == 0)
        return;

    TokenStack syntaxStack;
    QStack<int> argStack;
    unsigned argCount = 1;

    for (int i = 0; i <= tokens.count() && !syntaxStack.hasError(); ++i) {
        // Helper token: Invalid is end-of-expression.
        auto token = (i < tokens.count()) ? tokens.at(i)
                                          : Token(Token::stxOperator);
        auto tokenType = token.type();
        if (tokenType >= Token::stxOperator)
            tokenType = Token::stxOperator;

#ifdef EVALUATOR_DEBUG
        dbg << "\nToken: " << token.description() << "\n";
#endif

        // Unknown token is invalid.
        if (tokenType == Token::stxUnknown)
            break;

        // Try to apply all parsing rules.
#ifdef EVALUATOR_DEBUG
        dbg << "\tChecking rules..." << "\n";
#endif
        // Repeat until no more rule applies.
        bool argHandled = false;
        while (!syntaxStack.hasError()) {
            bool ruleFound = false;

            // Rule for function last argument: id (arg) -> arg.
            if (!ruleFound && syntaxStack.itemCount() >= 4) {
                Token par2 = syntaxStack.top();
                Token arg = syntaxStack.top(1);
                Token par1 = syntaxStack.top(2);
                Token id = syntaxStack.top(3);
                if (par2.asOperator() == Token::AssociationEnd
                    && arg.isOperand()
                    && par1.asOperator() == Token::AssociationStart
                    && id.isIdentifier())
                {
                    ruleFound = true;
                    syntaxStack.reduce(4, MAX_PRECEDENCE);
                    Opcode functionCall(Opcode::Function, argCount);
                    if (s_isSigmaFunctionIdentifier(id.text()) && argCount == 3)
                    {
                        const QString argText = m_expression.mid(arg.pos(), arg.size());
                        const QStringList argList = splitTopLevelFunctionArguments(argText);
                        if (argList.count() == 3)
                            functionCall.text = argList.at(2);
                    }
                    m_codes.append(functionCall);
#ifdef EVALUATOR_DEBUG
                        dbg << "\tRule for function last argument "
                            << argCount << " \n";
#endif
                    argCount = argStack.empty() ? 0 : argStack.pop();
                }
            }

            // Are we entering a function? If token is operator,
            // and stack already has: id (arg.
            if (!ruleFound && !argHandled && tokenType == Token::stxOperator
                 && syntaxStack.itemCount() >= 3)
            {
                Token arg = syntaxStack.top();
                Token par = syntaxStack.top(1);
                Token id = syntaxStack.top(2);
                if (arg.isOperand()
                    && par.asOperator() == Token::AssociationStart
                    && id.isIdentifier())
                {
                    ruleFound = true;
                    argStack.push(argCount);
#ifdef EVALUATOR_DEBUG
                    dbg << "\tEntering new function, pushing argcount="
                        << argCount << " of parent function\n";
#endif
                    argCount = 1;
                    break;
                }
           }

           // Rule for postfix operators: Y POSTFIX -> Y.
           // Condition: Y is not an operator, POSTFIX is a postfix op.
           // Since we evaluate from left to right,
           // we need not check precedence at this point.
           if (!ruleFound && syntaxStack.itemCount() >= 2) {
               Token postfix = syntaxStack.top();
               Token y = syntaxStack.top(1);
               if (postfix.isOperator() && y.isOperand()) {
                   switch (postfix.asOperator()) {
                   case Token::Factorial:
                       ruleFound = true;
                       syntaxStack.reduce(2);
                       m_codes.append(Opcode(Opcode::Fact));
                       break;
                   case Token::Percent:
                       ruleFound = true;
                       syntaxStack.reduce(2);
                       m_codes.append(Opcode(Opcode::Percent));
                       break;
                   default:;
                   }
               }
#ifdef EVALUATOR_DEBUG
               if (ruleFound) {
                   dbg << "\tRule for postfix operator "
                       << postfix.text() << "\n";
               }
#endif
           }

           // Rule for parenthesis: (Y) -> Y.
           if (!ruleFound && syntaxStack.itemCount() >= 3) {
               Token right = syntaxStack.top();
               Token y = syntaxStack.top(1);
               Token left = syntaxStack.top(2);
               if (y.isOperand()
                   && right.asOperator() == Token::AssociationEnd
                   && left.asOperator() == Token::AssociationStart)
               {
                   ruleFound = true;
                   syntaxStack.reduce(3, MAX_PRECEDENCE);
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for (Y) -> Y" << "\n";
#endif
               }
           }

           // Rule for simplified syntax for function,
           // e.g. "sin pi" or "cos 1.2". Conditions:
           // *precedence of function reduction >= precedence of next token.
           // *or next token is not an operator.
           if (!ruleFound && syntaxStack.itemCount() >= 2) {
               Token arg = syntaxStack.top();
               Token id = syntaxStack.top(1);
               if (arg.isOperand() && isFunction(id)
                   && (!token.isOperator()
                       || opPrecedence(Token::Function) >=
                              opPrecedence(token.asOperator())))
               {
                   ruleFound = true;
                   m_codes.append(Opcode(Opcode::Function, 1));
                   syntaxStack.reduce(2);
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for simplified function syntax; function "
                       << id.text() << "\n";
#endif
               }
           }

           // Rule for unary operator in simplified function syntax.
           // This handles cases like "sin -90".
           if (!ruleFound && syntaxStack.itemCount() >= 3) {
               Token x = syntaxStack.top();
               Token op = syntaxStack.top(1);
               Token id = syntaxStack.top(2);
               if (x.isOperand() && isFunction(id)
                   && (op.asOperator() == Token::Addition
                   || op.asOperator() == Token::Subtraction
                   || op.asOperator() == Token::BitwiseLogicalNOT))
               {
                   ruleFound = true;
                   syntaxStack.reduce(2);
                   if (op.asOperator() == Token::Subtraction)
                        m_codes.append(Opcode(Opcode::Neg));
                    else if (op.asOperator() == Token::BitwiseLogicalNOT)
                        m_codes.append(Opcode(Opcode::BNot));
#ifdef EVALUATOR_DEBUG
                     dbg << "\tRule for unary operator in simplified "
                            "function syntax; function " << id.text() << "\n";
#endif
               }
           }

           // Rule for function arguments. If token is ; or ):
           // id (arg1 ; arg2 -> id (arg.
           // Must come before binary op rule, special case of the latter.
           if (!ruleFound && syntaxStack.itemCount() >= 5
               && token.isOperator()
               && (token.asOperator() == Token::AssociationEnd
                   || token.asOperator() == Token::ListSeparator))
           {
               Token arg2 = syntaxStack.top();
               Token sep = syntaxStack.top(1);
               Token arg1 = syntaxStack.top(2);
               Token par = syntaxStack.top(3);
               Token id = syntaxStack.top(4);
               if (arg2.isOperand()
                   && sep.asOperator() == Token::ListSeparator
                   && arg1.isOperand()
                   && par.asOperator() == Token::AssociationStart
                   && id.isIdentifier())
               {
                   ruleFound = true;
                   argHandled = true;
                   syntaxStack.reduce(3, MAX_PRECEDENCE);
                   ++argCount;
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for function argument "
                       << argCount << " \n";
#endif
               }
           }

           // Rule for trailing function argument separator:
           // id (arg ; ) -> id (arg )
           // id (arg ;   -> id (arg   (end-of-expression)
           // This keeps the already parsed argument list and discards
           // an empty trailing argument.
           if (!ruleFound && syntaxStack.itemCount() >= 4
               && token.isOperator()
               && (token.asOperator() == Token::AssociationEnd
                   || token.asOperator() == Token::Invalid))
           {
               Token sep = syntaxStack.top();
               Token arg = syntaxStack.top(1);
               Token par = syntaxStack.top(2);
               Token id = syntaxStack.top(3);
               if (sep.asOperator() == Token::ListSeparator
                   && arg.isOperand()
                   && par.asOperator() == Token::AssociationStart
                   && id.isIdentifier())
               {
                   ruleFound = true;
                   argHandled = true;
                   syntaxStack.reduce(2, std::move(arg), MAX_PRECEDENCE);
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for trailing function argument separator\n";
#endif
               }
           }

           // Rule for function call with parentheses,
           // but without argument, e.g. "2*PI()".
           if (!ruleFound && syntaxStack.itemCount() >= 3) {
               Token par2 = syntaxStack.top();
               Token par1 = syntaxStack.top(1);
               Token id = syntaxStack.top(2);
               if (par2.asOperator() == Token::AssociationEnd
                   && par1.asOperator() == Token::AssociationStart
                   && id.isIdentifier())
               {
                   ruleFound = true;
                   syntaxStack.reduce(3, MAX_PRECEDENCE);
                   m_codes.append(Opcode(Opcode::Function, 0));
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for function call with parentheses, "
                          "but without argument\n";
#endif
               }
           }

           // Rule for binary operator:  A (op) B -> A.
           // Conditions: precedence of op >= precedence of token.
           // Action: push (op) to result e.g.
           // "A * B" becomes "A" if token is operator "+".
           // Exception: for caret (power operator), if op is another caret
           // then it doesn't apply, e.g. "2^3^2" is evaluated as "2^(3^2)".
           // Exception: doesn't apply if B is a function name (to manage
           // shift/reduce conflict with simplified function syntax
           // (issue #600).
           if (!ruleFound && syntaxStack.itemCount() >= 3) {
               Token b = syntaxStack.top();
               Token op = syntaxStack.top(1);
               Token a = syntaxStack.top(2);
               if (a.isOperand() && b.isOperand() && op.isOperator()
                   && ( // Normal operator.
                       (token.isOperator()
                           && opPrecedence(op.asOperator()) >=
                               opPrecedence(token.asOperator())
#ifdef ALLOW_IMPLICIT_MULT
                           && (token.asOperator() != Token::AssociationStart
                               || opPrecedence(op.asOperator()) >= opPrecedence(Token::Multiplication))
#endif
                           && !(b.isIdentifier() && token.asOperator() == Token::AssociationStart)
                           && token.asOperator() != Token::Exponentiation)

                       || ( // May represent implicit multiplication.
                           token.isOperand()
                           && opPrecedence(op.asOperator()) >=
                               opPrecedence(Token::Multiplication)))
                   && !(isFunction(b)))
               {
                   ruleFound = true;
                   switch (op.asOperator()) {
                   // Simple binary operations.
                   case Token::Addition:
                       m_codes.append(Opcode::Add);
                       break;
                   case Token::Subtraction:
                       m_codes.append(Opcode::Sub);
                       break;
                   case Token::Multiplication:
                       m_codes.append(Opcode::Mul);
                       break;
                   case Token::Division:
                       m_codes.append(Opcode::Div);
                       break;
                   case Token::Exponentiation:
                       m_codes.append(Opcode::Pow);
                       break;
                   case Token::Modulo:
                       m_codes.append(Opcode::Modulo);
                       break;
                   case Token::IntegerDivision:
                       m_codes.append(Opcode::IntDiv);
                       break;
                   case Token::ArithmeticLeftShift:
                       m_codes.append(Opcode::LSh);
                       break;
                   case Token::ArithmeticRightShift:
                       m_codes.append(Opcode::RSh);
                       break;
                   case Token::BitwiseLogicalAND:
                       m_codes.append(Opcode::BAnd);
                       break;
                   case Token::BitwiseLogicalOR:
                       m_codes.append(Opcode::BOr);
                       break;
                   case Token::UnitConversion: {
                       static const QRegularExpression unitNameNumberRE(
                           "(^[0-9e\\+\\-\\.,]|[0-9e\\.,]$)",
                           QRegularExpression::CaseInsensitiveOption
                       );
                       QString unitName =
                           m_expression.mid(b.pos(), b.size()).simplified();
                       // Make sure the whole unit name can be used
                       // as a single operand in multiplications.
                       if (b.minPrecedence() <
                               opPrecedence(Token::Multiplication))
                       {
                           unitName = "(" + unitName + ")";
                       }
                       // Protect the unit name
                       // if it starts or ends with a number.
                       else {
                           QRegularExpressionMatch match = unitNameNumberRE.match(unitName);
                           if (match.hasMatch())
                               unitName = "(" + unitName + ")";
                       }
                       m_codes.append(Opcode(Opcode::Conv, unitName));
                       break;
                   }
                   default: break;
                   };
                   syntaxStack.reduce(3);
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for binary operator" << "\n";
#endif
               }
           }

#ifdef ALLOW_IMPLICIT_MULT
           // Rule for implicit multiplication.
           // Action: Treat as A * B.
           // Exception: doesn't apply if B is a function name
           // (to manage shift/reduce conflict with simplified
           // function syntax (fixes issue #600).
           if (!ruleFound && syntaxStack.itemCount() >= 2) {
               Token b = syntaxStack.top();
               Token a = syntaxStack.top(1);

               if (a.isOperand() && b.isOperand()
                   && token.asOperator() != Token::AssociationStart
                   && ( // Token is normal operator.
                        (token.isOperator()
                            && opPrecedence(Token::Multiplication) >=
                                   opPrecedence(token.asOperator()))
                        || token.isOperand()) // Implicit multiplication.
                   && !isFunction(b))
               {
                   ruleFound = true;
                   syntaxStack.reduce(2, opPrecedence(Token::Multiplication));
                   m_implicitMultiplicationOpcodeIndices.insert(m_codes.count());
                   m_codes.append(Opcode::Mul);
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for implicit multiplication" << "\n";
#endif
               }

           }
#endif

           // Rule for unary operator:  (op1) (op2) X -> (op1) X.
           // Conditions: op2 is unary.
           // Current token has lower precedence than multiplication.
           if (!ruleFound
               && token.asOperator() != Token::AssociationStart
               && syntaxStack.itemCount() >= 3)
           {
               Token x = syntaxStack.top();
               Token op2 = syntaxStack.top(1);
               Token op1 = syntaxStack.top(2);
               if (x.isOperand() && op1.isOperator()
                   && (op2.asOperator() == Token::Addition
                       || op2.asOperator() == Token::Subtraction
                       || op2.asOperator() == Token::BitwiseLogicalNOT)
                   && !(token.isOperand() && isFunction(x))
                   && (token.isOperand()
                       || opPrecedence(token.asOperator()) <=
                              opPrecedence(Token::Multiplication)))
               {
                   ruleFound = true;
                   if (op2.asOperator() == Token::Subtraction)
                       m_codes.append(Opcode(Opcode::Neg));
                   else if (op2.asOperator() == Token::BitwiseLogicalNOT)
                       m_codes.append(Opcode(Opcode::BNot));

                   syntaxStack.reduce(2);
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for unary operator" << op2.text() << "\n";
#endif
               }
           }

           // Auxiliary rule for unary prefix operator:  (op) X -> X.
           // Conditions: op is unary, op is first in syntax stack.
           // Action: create code for (op). Unary MINUS or PLUS are
           // treated with the precedence of multiplication.
           if (!ruleFound
               && token.asOperator() != Token::AssociationStart
               && syntaxStack.itemCount() == 2)
           {
               Token x = syntaxStack.top();
               Token op = syntaxStack.top(1);
               if (x.isOperand()
                   && (op.asOperator() == Token::Addition
                       || op.asOperator() == Token::Subtraction
                       || op.asOperator() == Token::BitwiseLogicalNOT)
                   && !(token.isOperand() && isFunction(x))
                   && ((token.isOperator()
                           && opPrecedence(token.asOperator()) <=
                                  opPrecedence(Token::Multiplication))
                       || token.isOperand()))
               {
                   ruleFound = true;
                   if (op.asOperator() == Token::Subtraction)
                       m_codes.append(Opcode(Opcode::Neg));
                   else if (op.asOperator() == Token::BitwiseLogicalNOT)
                       m_codes.append(Opcode(Opcode::BNot));
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for unary operator (auxiliary)" << "\n";
#endif
                   syntaxStack.reduce(2);
               }
           }

           if (!ruleFound)
               break;
        }

        // Can't apply rules anymore, push the token.
        syntaxStack.push(token);

        // For identifier, generate code to load from reference.
        if (tokenType == Token::stxIdentifier) {
            m_identifiers.append(token.text());
            m_codes.append(Opcode(Opcode::Ref, m_identifiers.count() - 1));
#ifdef EVALUATOR_DEBUG
            dbg << "\tPush " << token.text() << " to identifier pools" << "\n";
#endif
        }

        // For constants, generate code to load from a constant.
        if (tokenType == Token::stxNumber) {
            m_constants.append(token.asNumber());
            m_constantTexts.append(token.text());
            m_codes.append(Opcode(Opcode::Load, m_constants.count() - 1));
#ifdef EVALUATOR_DEBUG
            dbg << "\tPush " << token.asNumber()
                << " to constant pools" << "\n";
#endif
        }
    }

    m_valid = false;
    if (syntaxStack.hasError())
        m_error = syntaxStack.error();
    // syntaxStack must left only one operand
    // and end-of-expression (i.e. Invalid).
    else if (syntaxStack.itemCount() == 2
             && syntaxStack.top().isOperator()
             && syntaxStack.top().asOperator() == Token::Invalid
             && !syntaxStack.top(1).isOperator())
    {
        m_valid = true;

        m_hasImplicitMultiplication = !m_implicitMultiplicationOpcodeIndices.isEmpty();

        m_interpretedExpression = hasGeneratedSexagesimalUnit
            ? QString()
            : buildInterpretedExpressionFromOpcodes();
    }

#ifdef EVALUATOR_DEBUG
    dbg << "Dump: " << dump() << "\n";
    debugFile.close();
#endif

    // Bad parsing? Clean-up everything.
    if (!m_valid) {
        m_constants.clear();
        m_constantTexts.clear();
        m_codes.clear();
        m_identifiers.clear();
        m_implicitMultiplicationOpcodeIndices.clear();
        m_interpretedExpression = QString();
        m_hasImplicitMultiplication = false;
    }
}

QString Evaluator::buildInterpretedExpressionFromOpcodes() const
{
    struct RenderNode {
        QString text;
        int precedence;
        Opcode::Type rootOpcode;
        bool isLiteralSymbol;
        bool isNumericOnly;
        bool isUnaryNegation;
    };

    QVector<RenderNode> stack;
    QHash<int, QString> refs;

    auto wrapInParentheses = [](const QString& text) {
        return QStringLiteral("(%1)").arg(text);
    };
    auto makeUnaryNode = [&wrapInParentheses](const QString& symbol,
                                              const RenderNode& operand,
                                              int precedence) {
        QString operandText = operand.text;
        if (operand.precedence <= precedence)
            operandText = wrapInParentheses(operandText);
        return RenderNode{
            symbol + operandText,
            precedence,
            Opcode::Nop,
            operand.isLiteralSymbol,
            operand.isNumericOnly,
            symbol == QLatin1String("-")
        };
    };
    auto makeBinaryNode = [&wrapInParentheses, this](const RenderNode& left,
                                                     const RenderNode& right,
                                                     Opcode::Type opcodeType,
                                                     int opcodeIndex) {
        Opcode::Type renderedOpcodeType = opcodeType;
        QString rightText = right.text;
        if (opcodeType == Opcode::Sub && right.isUnaryNegation) {
            // Prefer "a+b" over "a-(-b)" / "a--b" in interpreted output.
            renderedOpcodeType = Opcode::Add;
            if (rightText.startsWith(QLatin1Char('-')))
                rightText.remove(0, 1);
        }

        const int precedence = opcodePrecedence(renderedOpcodeType);
        const bool isRightAssociative = isRightAssociativeOpcode(opcodeType);
        const bool isImplicitMultiplication =
            opcodeType == Opcode::Mul
            && m_implicitMultiplicationOpcodeIndices.contains(opcodeIndex);
        QString leftText = left.text;
        if (left.precedence < precedence
            || (isRightAssociative && left.precedence == precedence))
        {
            leftText = wrapInParentheses(leftText);
        }
        if (right.precedence < precedence
            || (!isRightAssociative && right.precedence == precedence))
        {
            rightText = wrapInParentheses(rightText);
        }

        // In contextual percent expressions, make the additive base explicit:
        // render "1+99+10%" as "(1+99)+10%".
        if ((opcodeType == Opcode::Add || opcodeType == Opcode::Sub)
            && right.rootOpcode == Opcode::Percent
            && (left.rootOpcode == Opcode::Add || left.rootOpcode == Opcode::Sub)
            && !isWrappedInOuterParentheses(leftText))
        {
            leftText = wrapInParentheses(leftText);
        }

        const bool leftIsAtomicNumericForGrouping =
            left.isNumericOnly
            && left.rootOpcode == Opcode::Nop
            && !left.isLiteralSymbol;
        const bool rightIsAtomicNumericForGrouping =
            right.isNumericOnly
            && right.rootOpcode == Opcode::Nop
            && !right.isLiteralSymbol;

        // Make "a / b c" style parses explicit as "(a / b) * c", except
        // for simple numeric factors where a flat mul/div chain is clearer.
        if (opcodeType == Opcode::Mul
            && isImplicitMultiplication
            && (left.rootOpcode == Opcode::Div
                || left.rootOpcode == Opcode::IntDiv
                || left.rootOpcode == Opcode::Modulo))
        {
            const bool allowFlatChainForSimpleDivisorFactor =
                left.rootOpcode == Opcode::Div && rightIsAtomicNumericForGrouping;
            if (!allowFlatChainForSimpleDivisorFactor)
                leftText = wrapInParentheses(leftText);
        }
        if (opcodeType == Opcode::Mul
            && isImplicitMultiplication
            && (right.rootOpcode == Opcode::Div
                || right.rootOpcode == Opcode::IntDiv
                || right.rootOpcode == Opcode::Modulo))
        {
            const bool allowFlatChainForSimpleDividendFactor =
                right.rootOpcode == Opcode::Div && leftIsAtomicNumericForGrouping;
            if (!allowFlatChainForSimpleDividendFactor)
                rightText = wrapInParentheses(rightText);
        }

        // For explicit multiplications, isolate power terms to improve
        // readability in compound products (for example "1⋅(2^3)⋅3").
        if (opcodeType == Opcode::Mul && !isImplicitMultiplication) {
            if (left.rootOpcode == Opcode::Pow
                && !left.isLiteralSymbol
                && !isWrappedInOuterParentheses(leftText))
            {
                leftText = wrapInParentheses(leftText);
            }
            if (right.rootOpcode == Opcode::Pow
                && !right.isLiteralSymbol
                && !isWrappedInOuterParentheses(rightText))
            {
                rightText = wrapInParentheses(rightText);
            }
        }

        // Keep explicit grouping for factorial denominators
        // (for example "1/(2!)").
        if (opcodeType == Opcode::Div
            && right.rootOpcode == Opcode::Fact
            && !isWrappedInOuterParentheses(rightText))
        {
            rightText = wrapInParentheses(rightText);
        }

        // Keep non-integer numeric exponents explicit (for example
        // "2^(12.5)" instead of "2^12.5").
        if (opcodeType == Opcode::Pow
            && !isWrappedInOuterParentheses(rightText))
        {
            normalizeUnsignedIntegerEquivalentDecimalText(rightText);
            const bool isNonIntegerNumericExponent =
                right.isNumericOnly
                && right.rootOpcode == Opcode::Nop
                && !isUnsignedDecimalIntegerText(rightText);
            const bool isFactorialExponent =
                right.rootOpcode == Opcode::Fact;
            const bool isChainedExponent =
                right.rootOpcode == Opcode::Pow;
            if (isNonIntegerNumericExponent || isFactorialExponent || isChainedExponent)
                rightText = wrapInParentheses(rightText);
        }

        const bool leftIsAtomicNumeric =
            left.isNumericOnly
            && left.rootOpcode == Opcode::Nop
            && !left.isLiteralSymbol;
        const bool rightIsAtomicNumeric =
            right.isNumericOnly
            && right.rootOpcode == Opcode::Nop
            && !right.isLiteralSymbol;
        const bool isNumberTimesNumber =
            opcodeType == Opcode::Mul
            && leftIsAtomicNumeric
            && rightIsAtomicNumeric;
        static const QRegularExpression s_endsWithDigitRE(QStringLiteral(R"([0-9]$)"));
        const bool isNumericBoundaryTimesNumber =
            opcodeType == Opcode::Mul
            && !isImplicitMultiplication
            && rightIsAtomicNumeric
            && s_endsWithDigitRE.match(leftText).hasMatch();
        const bool isLiteralSymbolMultiplication =
            opcodeType == Opcode::Mul
            && !isNumberTimesNumber
            && !isNumericBoundaryTimesNumber;

        const QString infixSymbol =
            opcodeToInfixSymbol(
                renderedOpcodeType,
                isImplicitMultiplication || isLiteralSymbolMultiplication
            );
        return RenderNode{
            leftText + infixSymbol + rightText,
            precedence,
            renderedOpcodeType,
            left.isLiteralSymbol || right.isLiteralSymbol,
            left.isNumericOnly && right.isNumericOnly,
            false
        };
    };

    for (int opcodeIndex = 0; opcodeIndex < m_codes.count(); ++opcodeIndex) {
        const Opcode& opcode = m_codes.at(opcodeIndex);
        switch (opcode.type) {
        case Opcode::Nop:
            break;
        case Opcode::Load: {
            QString constantText = m_constantTexts.value(opcode.index);
            if (constantText.isEmpty()) {
                constantText = DMath::format(m_constants.value(opcode.index),
                                             Quantity::Format::Fixed());
            }
            stack.append({
                constantText,
                MAX_PRECEDENCE,
                Opcode::Nop,
                false,
                true,
                false
            });
            break;
        }
        case Opcode::Ref: {
            const QString identifier = m_identifiers.value(opcode.index);
            stack.append({
                identifier,
                MAX_PRECEDENCE,
                Opcode::Nop,
                true,
                false,
                false
            });

            if (m_assignArg.contains(identifier) || hasVariable(identifier))
                break;

            if (FunctionRepo::instance()->find(identifier)
                || hasUserFunction(identifier)
                || m_assignFunc)
            {
                refs.insert(stack.count(), identifier);
            }
            break;
        }
        case Opcode::Neg:
        case Opcode::BNot: {
            if (stack.isEmpty())
                return QString();
            const RenderNode operand = stack.takeLast();
            if (opcode.type == Opcode::Neg && operand.isUnaryNegation) {
                QString text = operand.text;
                if (text.startsWith(QLatin1Char('-')))
                    text.remove(0, 1);
                stack.append({
                    text,
                    MAX_PRECEDENCE,
                    Opcode::Nop,
                    operand.isLiteralSymbol,
                    operand.isNumericOnly,
                    false
                });
                break;
            }
            const QString symbol = opcode.type == Opcode::Neg ? "-" : "~";
            stack.append(makeUnaryNode(symbol, operand,
                                       opcodePrecedence(opcode.type)));
            break;
        }
        case Opcode::Fact:
        case Opcode::Percent: {
            if (stack.isEmpty())
                return QString();
            const int precedence = opcodePrecedence(opcode.type);
            RenderNode operand = stack.takeLast();
            if (operand.precedence < precedence)
                operand.text = wrapInParentheses(operand.text);
            const QString symbol = opcode.type == Opcode::Fact
                ? QStringLiteral("!")
                : QStringLiteral("%");
            stack.append({
                operand.text + symbol,
                precedence,
                opcode.type,
                operand.isLiteralSymbol,
                operand.isNumericOnly,
                false
            });
            break;
        }
        case Opcode::Add:
        case Opcode::Sub:
        case Opcode::Mul:
        case Opcode::Div:
        case Opcode::Pow:
        case Opcode::Modulo:
        case Opcode::IntDiv:
        case Opcode::LSh:
        case Opcode::RSh:
        case Opcode::BAnd:
        case Opcode::BOr:
        case Opcode::Conv: {
            if (stack.size() < 2)
                return QString();
            const RenderNode right = stack.takeLast();
            const RenderNode left = stack.takeLast();
            stack.append(makeBinaryNode(left, right, opcode.type, opcodeIndex));
            break;
        }
        case Opcode::Function: {
            const int argumentCount = static_cast<int>(opcode.index);
            const int functionRefKey = stack.count() - argumentCount;
            QString functionName = refs.take(functionRefKey);
            if (functionName.isEmpty()
                && functionRefKey > 0
                && functionRefKey <= stack.count())
            {
                functionName = stack.at(functionRefKey - 1).text;
            }

            if (stack.count() < argumentCount + 1)
                return QString();

            QStringList arguments;
            for (int i = 0; i < argumentCount; ++i)
                arguments.prepend(stack.takeLast().text);

            // Discard function reference placeholder.
            stack.takeLast();

            stack.append({
                QStringLiteral("%1(%2)").arg(
                    UnicodeChars::normalizeRootFunctionAliasesForDisplay(functionName),
                    arguments.join("; ")),
                MAX_PRECEDENCE,
                Opcode::Nop,
                true,
                false,
                false
            });
            break;
        }
        }
    }

    if (stack.count() != 1)
        return QString();

    return stack.constLast().text;
}

Quantity Evaluator::evalNoAssign()
{
    Quantity result;

    if (m_dirty) {
        // Reset.
        m_assignId = QString();
        m_assignFunc = false;
        m_assignArg.clear();
        m_assignVarDescription = QString();
        m_assignFuncExpr = QString();
        m_assignFuncDescription = QString();
        m_interpretedExpression = QString();
        m_hasImplicitMultiplication = false;
        QString expressionToParse = m_expression;
        QString trailingComment;
        const int commentPos = m_expression.indexOf('?');
        if (commentPos >= 0)
            trailingComment = m_expression.mid(commentPos + 1).trimmed();
        if (m_expression.contains('?')) {
            QString expressionWithoutDescription;
            QString description;
            if (splitUserFunctionDescription(m_expression,
                                             &expressionWithoutDescription,
                                             &description)) {
                expressionToParse = expressionWithoutDescription;
                m_assignFuncDescription = description;
            } else if (splitVariableDescription(m_expression,
                                                &expressionWithoutDescription,
                                                &description))
            {
                expressionToParse = expressionWithoutDescription;
                m_assignVarDescription = description;
            }
        }
        Tokens tokens = scan(expressionToParse);

        // Invalid expression?
        if (!tokens.valid()) {
            m_error = tr("invalid expression");
            m_interpretedExpression = QString();
            m_hasImplicitMultiplication = false;
            return Quantity(0);
        }

        if (tokens.count() == 0 && isCommentOnlyExpression(m_expression)) {
            m_valid = true;
            m_constants.clear();
            m_constantTexts.clear();
            m_codes.clear();
            m_identifiers.clear();
            m_interpretedExpression = QString();
            m_hasImplicitMultiplication = false;
            return CMath::nan();
        }

        // Variable assignment?
        if (tokens.count() > 2
            && tokens.at(0).isIdentifier()
            && tokens.at(1).asOperator() == Token::Assignment)
        {
            m_assignId = tokens.at(0).text();
            tokens.erase(tokens.begin());
            tokens.erase(tokens.begin());
        } else if (tokens.count() > 2
                   && tokens.at(0).isIdentifier()
                   && tokens.at(1).asOperator() == Token::AssociationStart)
        {
            // Check for function assignment.
            // Syntax:
            // ident opLeftPar (ident (opSemiColon ident)*)? opRightPar opEqual
            int t;
            if (tokens.count() > 4
                && tokens.at(2).asOperator() == Token::AssociationEnd)
            {
                // Functions with no argument.
                t = 3;
                if (tokens.at(3).asOperator() == Token::Assignment)
                    m_assignFunc = true;
            } else {
                for (t = 2; t + 1 < tokens.count(); t += 2)  {
                    if (!tokens.at(t).isIdentifier())
                        break;

                    m_assignArg.append(tokens.at(t).text());

                    if (tokens.at(t+1).asOperator() == Token::AssociationEnd) {
                        t += 2;
                        if (t < tokens.count()
                            && tokens.at(t).asOperator() == Token::Assignment)
                        {
                            m_assignFunc = true;
                        }

                        break;
                    } else if (tokens.at(t + 1)
                               .asOperator() != Token::ListSeparator)
                        break;
                }
            }

            if (m_assignFunc) {
                m_assignId = tokens.at(0).text();
                m_assignFuncExpr = expressionToParse.section("=", 1, 1).trimmed();
                for (; t >= 0; --t)
                    tokens.erase(tokens.begin());
            } else
                m_assignArg.clear();
        }

        compile(tokens);
        if (!m_valid) {
            if (m_error.isEmpty())
                m_error = tr("syntax error");
            m_interpretedExpression = QString();
            m_hasImplicitMultiplication = false;
            return CNumber(0);
        }

        if (!m_assignId.isEmpty() && !m_interpretedExpression.isEmpty()) {
            if (m_assignFunc) {
                const QString leftSide = QStringLiteral("%1(%2)").arg(
                    m_assignId,
                    m_assignArg.join(";")
                );
                m_interpretedExpression =
                    leftSide + QStringLiteral("=") + m_interpretedExpression;
            } else {
                m_interpretedExpression =
                    m_assignId + QStringLiteral("=") + m_interpretedExpression;
            }
        }

        if (!m_interpretedExpression.isEmpty() && !trailingComment.isEmpty())
            m_interpretedExpression += QStringLiteral(" ? ") + trailingComment;
    }

    result = exec(m_codes, m_constants, m_identifiers);
    return result;
}

Quantity Evaluator::exec(const QVector<Opcode>& opcodes,
                         const QVector<Quantity>& constants,
                         const QStringList& identifiers)
{
    struct StackEntry {
        Quantity value;
        bool isPercentValue;
    };
    QStack<StackEntry> stack;
    QHash<int, QString> refs;
    int index;
    Quantity val1, val2;
    bool val1IsPercent = false;
    bool rhsIsPercent = false;
    QVector<Quantity> args;
    QString fname;
    Function* function;
    const UserFunction* userFunction = nullptr;
    auto pushStackValue = [&stack](const Quantity& value, bool isPercentValue = false) {
        stack.push(StackEntry{value, isPercentValue});
    };
    auto popStackValue = [&stack](Quantity& value, bool* isPercentValue = nullptr) {
        const StackEntry entry = stack.pop();
        value = entry.value;
        if (isPercentValue)
            *isPercentValue = entry.isPercentValue;
    };
    auto hasPendingDeferredOperandContext = [&refs](int stackSize) {
        QHash<int, QString>::const_iterator it = refs.constBegin();
        for (; it != refs.constEnd(); ++it) {
            if (it.key() <= stackSize
                && s_isSigmaFunctionIdentifier(it.value()))
            {
                return true;
            }
        }
        return false;
    };
    auto checkOperatorResultWithDeferredNoOperand = [this, &hasPendingDeferredOperandContext, &stack](const Quantity& result) {
        if (result.error() == NoOperand
            && hasPendingDeferredOperandContext(stack.count()))
        {
            return result;
        }
        return checkOperatorResult(result);
    };

    for (int pc = 0; pc < opcodes.count(); ++pc) {
        const Opcode& opcode = opcodes.at(pc);
        index = opcode.index;
        switch (opcode.type) {
            // No operation.
            case Opcode::Nop:
                break;

            // Load a constant, push to stack.
            case Opcode::Load:
                val1 = constants.at(index);
                pushStackValue(val1);
                break;

            // Unary operation.
            case Opcode::Neg:
                if (stack.count() < 1) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                val1IsPercent = false;
                popStackValue(val1, &val1IsPercent);
                val1 = checkOperatorResultWithDeferredNoOperand(-val1);
                pushStackValue(val1, val1IsPercent);
                break;

            case Opcode::BNot:
                if (stack.count() < 1) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                val1 = checkOperatorResultWithDeferredNoOperand(~val1);
                pushStackValue(val1);
                break;

            // Binary operation: take two values from stack,
            // do the operation, push the result to stack.
            case Opcode::Add:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                rhsIsPercent = false;
                popStackValue(val1, &rhsIsPercent);
                popStackValue(val2);
                if (rhsIsPercent)
                    val1 = checkOperatorResultWithDeferredNoOperand(val2 * val1);
                val2 = checkOperatorResultWithDeferredNoOperand(val2 + val1);
                pushStackValue(val2);
                break;

            case Opcode::Sub:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                rhsIsPercent = false;
                popStackValue(val1, &rhsIsPercent);
                popStackValue(val2);
                if (rhsIsPercent)
                    val1 = checkOperatorResultWithDeferredNoOperand(val2 * val1);
                val2 = checkOperatorResultWithDeferredNoOperand(val2 - val1);
                pushStackValue(val2);
                break;

            case Opcode::Mul:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = checkOperatorResultWithDeferredNoOperand(val2 * val1);
                pushStackValue(val2);
                break;

            case Opcode::Div:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = checkOperatorResultWithDeferredNoOperand(val2 / val1);
                pushStackValue(val2);
                break;

            case Opcode::Pow:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = checkOperatorResultWithDeferredNoOperand(DMath::raise(val2, val1));
                pushStackValue(val2);
                break;

            case Opcode::Fact:
                if (stack.count() < 1) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                val1 = checkOperatorResultWithDeferredNoOperand(DMath::factorial(val1));
                pushStackValue(val1);
                break;

            case Opcode::Percent:
                if (stack.count() < 1) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                val1 = checkOperatorResultWithDeferredNoOperand(val1 / Quantity(100));
                pushStackValue(val1, true);
                break;

            case Opcode::Modulo:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = checkOperatorResultWithDeferredNoOperand(val2 % val1);
                pushStackValue(val2);
                break;

            case Opcode::IntDiv:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = checkOperatorResultWithDeferredNoOperand(val2 / val1);
                pushStackValue(DMath::integer(val2));
                break;

            case Opcode::LSh:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return DMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = val2 << val1;
                pushStackValue(val2);
                break;

            case Opcode::RSh:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return DMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = val2 >> val1;
                pushStackValue(val2);
                break;

            case Opcode::BAnd:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return DMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 &= val1;
                pushStackValue(val2);
                break;

            case Opcode::BOr:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return DMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 |= val1;
                pushStackValue(val2);
                break;

            case Opcode::Conv: {
                auto isFullyWrappedByOuterParens = [](const QString& expr) {
                    if (expr.size() < 2 || expr.at(0) != QLatin1Char('(') || expr.at(expr.size() - 1) != QLatin1Char(')'))
                        return false;
                    int depth = 0;
                    for (int i = 0; i < expr.size(); ++i) {
                        const QChar ch = expr.at(i);
                        if (ch == QLatin1Char('('))
                            ++depth;
                        else if (ch == QLatin1Char(')'))
                            --depth;
                        if (depth == 0 && i < expr.size() - 1)
                            return false;
                    }
                    return depth == 0;
                };
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return HMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                if (val1.isZero()) {
                    m_error = tr("unit must not be zero");
                    return HMath::nan();
                }
                if (!val1.sameDimension(val2)) {
                    m_error = tr("Conversion failed - dimension mismatch");
                    return HMath::nan();
                }
                CNumber displayUnitValue = val1.numericValue();
                QString displayUnitName = opcode.text.trimmed();
                if (displayUnitName.startsWith(QStringLiteral("(("))
                    && isFullyWrappedByOuterParens(displayUnitName))
                {
                    const QString onceUnwrapped = displayUnitName.mid(1, displayUnitName.size() - 2).trimmed();
                    displayUnitName = onceUnwrapped;
                }
                if (isFullyWrappedByOuterParens(displayUnitName)) {
                    const QString inner = displayUnitName.mid(1, displayUnitName.size() - 2).trimmed();
                    if (RegExpPatterns::simpleUnitIdentifier().match(inner).hasMatch())
                        displayUnitName = inner;
                }
                const bool nestedConversionTarget =
                    opcode.text.contains(QLatin1String("->"))
                    || opcode.text.contains(QChar(0x2192));
                if (nestedConversionTarget) {
                    Quantity canonicalTarget(val1);
                    canonicalTarget.stripUnits();
                    if (!canonicalTarget.isDimensionless())
                        Units::findUnit(canonicalTarget);
                    if (canonicalTarget.hasUnit()) {
                        displayUnitValue = canonicalTarget.unit();
                        displayUnitName = canonicalTarget.unitName();
                    }
                }
                displayUnitName = s_renderUnitExponentsAsSuperscripts(displayUnitName);
                val2.setDisplayUnit(displayUnitValue, displayUnitName);
                pushStackValue(val2);
                break;
            }

            // Reference.
            case Opcode::Ref:
                fname = identifiers.at(index);
                if (m_assignArg.contains(fname)) {
                    // Argument.
                    pushStackValue(CMath::nan());
                } else if (hasVariable(fname)) {
                    // Variable.
                    pushStackValue(getVariable(fname).value());
                } else {
                    // Function.
                    function = FunctionRepo::instance()->find(fname);
                    if (function) {
                        pushStackValue(CMath::nan());
                        refs.insert(stack.count(), fname);
                    } else if (m_assignFunc) {
                        // Allow arbitrary identifiers
                        // when declaring user functions.
                        pushStackValue(CMath::nan());
                        refs.insert(stack.count(), fname);
                    } else if (hasUserFunction(fname)) {
                        pushStackValue(CMath::nan());
                        refs.insert(stack.count(), fname);
                    } else if (fname.compare("n", Qt::CaseInsensitive) == 0
                               && hasPendingDeferredOperandContext(stack.count()))
                    {
                        pushStackValue(CMath::nan());
                    } else {
                        m_error = "<b>" + fname + "</b>: "
                                  + tr("unknown function or variable");
                        return CMath::nan();
                    }
                }
                break;

            // Calling function.
            case Opcode::Function:
                fname = refs.take(stack.count() - index);
                // Identifiers followed by () must resolve to a callable.
                // Do not silently accept constants/variables as nullary calls.
                if (fname.isEmpty()) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                function = FunctionRepo::instance()->find(fname);

                userFunction = nullptr;
                if (!function) {
                    // Check if this is a valid user function call.
                    userFunction = getUserFunction(fname);
                }

                if (!function && !userFunction && !m_assignFunc) {
                    m_error = "<b>" + fname + "</b>: "
                              + tr("unknown function or variable");
                    return CMath::nan();
                }

                if (stack.count() < index) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }

                args.clear();
                for(; index; --index)
                    args.insert(args.begin(), stack.pop().value);

                // Remove the NaN we put on the stack (needed to make the user
                // functions declaration work with arbitrary identifiers).
                stack.pop();

                // Show function signature if user has given no argument (yet).
                if (userFunction) {
                    if (!args.count()
                        && userFunction->arguments().count() != 0)
                    {
                        m_error = QString::fromLatin1("<b>%1</b>(%2)").arg(
                            userFunction->name(),
                            userFunction->arguments().join(";")
                        );
                        return CMath::nan();
                    }
                }

                if (m_assignFunc) {
                    // Allow arbitrary identifiers for declaring user functions.
                    pushStackValue(CMath::nan());
                } else if (userFunction) {
                    pushStackValue(execUserFunction(userFunction, args));
                    if (!m_error.isEmpty())
                        return CMath::nan();
                } else if (s_isSigmaFunctionIdentifier(fname)) {
                    if (args.count() != 3) {
                        m_error = QString::fromLatin1("<b>%1</b>: ").arg(fname)
                                + tr("wrong number of arguments");
                        return CMath::nan();
                    }
                    if (!args.at(0).isInteger() || !args.at(1).isInteger()) {
                        m_error = QString::fromLatin1("<b>%1</b>: ").arg(fname)
                                + tr("undefined for argument domain");
                        return CMath::nan();
                    }

                    const bool hadN = hasVariable("n");
                    const Variable oldN = hadN ? getVariable("n")
                                               : Variable(QString(), Quantity(0));
                    auto restoreN = [this, hadN, oldN]() {
                        if (hadN) {
                            setVariable("n", oldN.value(), oldN.type(),
                                        oldN.description());
                        } else {
                            unsetVariable("n");
                        }
                    };

                    const Quantity start = args.at(0);
                    const Quantity end = args.at(1);
                    const Quantity step = (start <= end) ? Quantity(1) : Quantity(-1);
                    const bool hasExpression = !opcode.text.isEmpty();
                    Quantity sigmaResult(0);

                    for (Quantity n = start; step > 0 ? (n <= end) : (n >= end); n += step) {
                        setVariable("n", n);

                        Quantity term = args.at(2);
                        if (hasExpression) {
                            const bool savedDirty = m_dirty;
                            const QString savedExpression = m_expression;
                            const bool savedValid = m_valid;
                            const QString savedInterpretedExpression = m_interpretedExpression;
                            const QString savedAssignId = m_assignId;
                            const bool savedAssignFunc = m_assignFunc;
                            const QStringList savedAssignArg = m_assignArg;
                            const QVector<Opcode> savedCodes = m_codes;
                            const QVector<Quantity> savedConstants = m_constants;
                            const QStringList savedIdentifiers = m_identifiers;
                            const QString savedError = m_error;

                            setExpression(opcode.text);
                            term = evalNoAssign();
                            const QString termError = m_error;

                            m_dirty = savedDirty;
                            m_expression = savedExpression;
                            m_valid = savedValid;
                            m_interpretedExpression = savedInterpretedExpression;
                            m_assignId = savedAssignId;
                            m_assignFunc = savedAssignFunc;
                            m_assignArg = savedAssignArg;
                            m_codes = savedCodes;
                            m_constants = savedConstants;
                            m_identifiers = savedIdentifiers;
                            m_error = savedError;

                            if (!termError.isEmpty()) {
                                restoreN();
                                m_error = QString::fromLatin1("<b>%1</b>: ").arg(fname)
                                        + termError;
                                return CMath::nan();
                            }
                        }

                        sigmaResult += term;
                    }

                    restoreN();
                    pushStackValue(sigmaResult);
                } else {
                    pushStackValue(function->exec(args));
                    if (function->error()) {
                        if (!args.count() && function->error() == InvalidParamCount) {
                            m_error = QString::fromLatin1("<b>%1</b>(%2)").arg(
                                fname,
                                function->usage()
                            );
                        } else {
                            m_error = stringFromFunctionError(function);
                        }
                        return CMath::nan();
                    }
                }
                break;

            default:
                break;
        }
    }

    // More than one value in stack? Unsuccessful execution.
    if (stack.count() != 1) {
        m_error = tr("invalid expression");
        return CMath::nan();
    }
    return stack.pop().value;
}

Quantity Evaluator::execUserFunction(const UserFunction* function,
                                     QVector<Quantity>& arguments)
{
    // TODO: Replace user variables by user functions (with no argument)?
    if (arguments.count() != function->arguments().count()) {
        m_error = "<b>" + function->name() + "</b>: "
                  + tr("wrong number of arguments");
        return CMath::nan();
    }

    if (m_functionsInUse.contains(function->name())) {
           m_error = "<b>" + function->name() + "</b>: "
                     + tr("recursion not supported");
           return CMath::nan();
    }
    m_functionsInUse.insert(function->name());

    QVector<Opcode> newOpcodes;
    auto newConstants = function->constants; // Copy.

    // Replace references to function arguments by constants.
    for (int i = 0; i < function->opcodes.count(); ++i) {
        Opcode opcode = function->opcodes.at(i);

        if (opcode.type == Opcode::Ref) {
            // Check if the identifier is an argument name.
            QString name = function->identifiers.at(opcode.index);
            int argIdx = function->arguments().indexOf(name);
            if (argIdx >= 0) {
                // Replace the reference by a constant value.
                opcode = Opcode(Opcode::Load, newConstants.count());
                newConstants.append(arguments.at(argIdx));
            }
        }

        newOpcodes.append(opcode);
    }

    auto result = exec(newOpcodes, newConstants, function->identifiers);
    if (!m_error.isEmpty()) {
        // Tell the user where the error happened.
        m_error = "<b>" + function->name() + "</b>: " + m_error;
    }

    m_functionsInUse.remove(function->name());
    return result;
}

bool Evaluator::isUserFunctionAssign() const
{
    return m_assignFunc;
}

bool Evaluator::isBuiltInVariable(const QString& id) const
{
    // Defining variables with the same name as existing functions
    // is not supported for now.
    if (FunctionRepo::instance()->find(id))
        return true;

    if (!m_session || !m_session->hasVariable(id))
        return false;

    return m_session->getVariable(id).type() == Variable::BuiltIn;
}

Quantity Evaluator::eval()
{
    Quantity result = evalNoAssign(); // This sets m_assignId.

    if (!m_error.isEmpty())
        return result;

    if (isBuiltInVariable(m_assignId)) {
        m_error = tr("%1 is a reserved name, "
                     "please choose another").arg(m_assignId);
        return CMath::nan();
    }
    // Handle user variable or function assignment.
    if (!m_assignId.isEmpty()) {
        if (m_assignFunc) {
            if (hasVariable(m_assignId)) {
                m_error = tr("%1 is a variable name, please choose another "
                             "or delete the variable").arg(m_assignId);
                return CMath::nan();
            }

            // Check that each argument is unique and not a reserved identifier.
            for (int i = 0; i < m_assignArg.count() - 1; ++i) {
                const QString& argName = m_assignArg.at(i);

                if (m_assignArg.indexOf(argName, i + 1) != -1) {
                    m_error = tr("argument %1 is used "
                                 "more than once").arg(argName);
                    return CMath::nan();
                }

                if (isBuiltInVariable(argName)) {
                    m_error = tr("%1 is a reserved name, "
                                 "please choose another").arg(argName);
                    return CMath::nan();
                }
            }

            if (m_codes.isEmpty())
                return CMath::nan();
            UserFunction userFunction(m_assignId, m_assignArg,
                                      m_assignFuncExpr, m_assignFuncDescription);
            if (!m_interpretedExpression.isEmpty()) {
                const QString leftSide = QStringLiteral("%1(%2)=").arg(
                    m_assignId,
                    m_assignArg.join(";")
                );
                QString interpretedBody = m_interpretedExpression;
                if (interpretedBody.startsWith(leftSide))
                    interpretedBody = interpretedBody.mid(leftSide.size());
                else {
                    const int assignmentPos = interpretedBody.indexOf('=');
                    if (assignmentPos >= 0)
                        interpretedBody = interpretedBody.mid(assignmentPos + 1);
                }
                const int commentPos = findTopLevelCommentDelimiterPos(interpretedBody);
                if (commentPos >= 0)
                    interpretedBody = interpretedBody.left(commentPos).trimmed();
                userFunction.setInterpretedExpression(interpretedBody);
            }
            userFunction.constants = m_constants;
            userFunction.identifiers = m_identifiers;
            userFunction.opcodes = m_codes;

            setUserFunction(userFunction);

        } else {
            if (hasUserFunction(m_assignId)) {
                m_error = tr("%1 is a user function name, please choose "
                             "another or delete the function").arg(m_assignId);
                return CMath::nan();
            }

            setVariable(m_assignId, result, Variable::UserDefined,
                        m_assignVarDescription);
        }
    }

    return result;
}

Quantity Evaluator::evalUpdateAns()
{
    auto result = eval();
    if (m_error.isEmpty() && !m_assignFunc
        && !isCommentOnlyExpression(m_expression))
        setVariable(QLatin1String("ans"), result, Variable::BuiltIn);
    return result;
}

void Evaluator::setVariable(const QString& id, Quantity value,
                            Variable::Type type,
                            const QString& description)
{
    if (!m_session)
        m_session = new Session;
    m_session->addVariable(Variable(id, value, type, description));
}

Variable Evaluator::getVariable(const QString& id) const
{
    if (id.isEmpty() || !m_session)
        return Variable(QLatin1String(""), Quantity(0));

    return m_session->getVariable(id);
}

bool Evaluator::hasVariable(const QString& id) const
{
    if (id.isEmpty() || !m_session)
        return false;
    else
        return m_session->hasVariable(id);
}

void Evaluator::unsetVariable(const QString& id,
                              ForceBuiltinVariableErasure force)
{
    if (!m_session || (m_session->isBuiltInVariable(id) && !force))
        return;
    m_session->removeVariable(id);
}

QList<Variable> Evaluator::getVariables() const
{
    return m_session ? m_session->variablesToList() : QList<Variable>();
}

QList<Variable> Evaluator::getUserDefinedVariables() const
{
    auto result = getVariables();
    auto iter = result.begin();
    while (iter != result.end()) {
        if ((*iter).type() == Variable::BuiltIn)
            iter = result.erase(iter);
        else
            ++iter;
    }
    return result;
}

QList<Variable> Evaluator::getUserDefinedVariablesPlusAns() const
{
    auto result = getUserDefinedVariables();
    auto ans = getVariable(QLatin1String("ans"));
    if (!ans.identifier().isEmpty())
        result.append(ans);
    return result;
}

void Evaluator::unsetAllUserDefinedVariables()
{
    if (!m_session)
        return;
    auto ansBackup = getVariable(QLatin1String("ans")).value();
    m_session->clearVariables();
    setVariable(QLatin1String("ans"), ansBackup, Variable::BuiltIn);
    initializeBuiltInVariables();
}

static void replaceSuperscriptPowersWithCaretEquivalent(QString& expr)
{
    auto isIdentifierChar = [](const QChar& ch) {
        return isIdentifierContinue(ch);
    };
    auto isIdentifierStartChar = [](const QChar& ch) {
        return isIdentifierStart(ch);
    };
    auto isSuperscriptPowerChar = [](const QChar& ch) {
        switch (ch.unicode()) {
            case 0x207B: // ⁻
            case 0x2070: // ⁰
            case 0x00B9: // ¹
            case 0x00B2: // ²
            case 0x00B3: // ³
            case 0x2074: // ⁴
            case 0x2075: // ⁵
            case 0x2076: // ⁶
            case 0x2077: // ⁷
            case 0x2078: // ⁸
            case 0x2079: // ⁹
                return true;
            default:
                return false;
        }
    };

    // Convert function-call powers from "f²(x)" to "f(x)²" so later conversion
    // can produce the parser-friendly "f(x)^2" form.
    int i = 0;
    while (i < expr.size()) {
        if (!isSuperscriptPowerChar(expr.at(i))) {
            ++i;
            continue;
        }

        int superscriptEnd = i + 1;
        while (superscriptEnd < expr.size() && isSuperscriptPowerChar(expr.at(superscriptEnd)))
            ++superscriptEnd;

        int nameStart = i - 1;
        while (nameStart >= 0 && isIdentifierChar(expr.at(nameStart)))
            --nameStart;
        ++nameStart;
        while (nameStart < i && !isIdentifierStartChar(expr.at(nameStart)))
            ++nameStart;
        if (nameStart >= i || !isIdentifierStartChar(expr.at(nameStart))) {
            i = superscriptEnd;
            continue;
        }

        int argsStart = superscriptEnd;
        while (argsStart < expr.size() && expr.at(argsStart).isSpace())
            ++argsStart;
        if (argsStart >= expr.size() || expr.at(argsStart) != QLatin1Char('(')) {
            i = superscriptEnd;
            continue;
        }

        int depth = 0;
        int argsEnd = -1;
        for (int j = argsStart; j < expr.size(); ++j) {
            const QChar ch = expr.at(j);
            if (ch == QLatin1Char('(')) {
                ++depth;
            } else if (ch == QLatin1Char(')')) {
                --depth;
                if (depth == 0) {
                    argsEnd = j;
                    break;
                }
            }
        }
        if (argsEnd < 0) {
            i = superscriptEnd;
            continue;
        }

        const QString superscript = expr.mid(i, superscriptEnd - i);
        const QString callArgs = expr.mid(argsStart, argsEnd - argsStart + 1);
        int trailingPos = argsEnd + 1;
        while (trailingPos < expr.size() && expr.at(trailingPos).isSpace())
            ++trailingPos;
        const bool followedByAnotherSuperscript =
            trailingPos < expr.size() && isSuperscriptPowerChar(expr.at(trailingPos));

        if (followedByAnotherSuperscript) {
            const QString functionName = expr.mid(nameStart, i - nameStart);
            const QString replacement = QStringLiteral("(%1%2%3)")
                .arg(functionName, callArgs, superscript);
            expr.replace(nameStart, argsEnd - nameStart + 1, replacement);
            i = nameStart + replacement.size();
        } else {
            const QString replacement = callArgs + superscript;
            expr.replace(i, argsEnd - i + 1, replacement);
            i += replacement.size();
        }
    }

    static const QRegularExpression s_superscriptPowersRE(
        "(\\x{207B})?[\\x{2070}¹²³\\x{2074}-\\x{2079}]+"
    );
    static const QHash<QChar, QChar> s_superscriptPowersHash {
        {QChar(0x207B), '-'},
        {QChar(0x2070), '0'},
        {QChar(0x00B9), '1'},
        {QChar(0x00B2), '2'},
        {QChar(0x00B3), '3'},
        {QChar(0x2074), '4'},
        {QChar(0x2075), '5'},
        {QChar(0x2076), '6'},
        {QChar(0x2077), '7'},
        {QChar(0x2078), '8'},
        {QChar(0x2079), '9'},
    };

    int offset = 0;
    while (true) {
      QRegularExpressionMatch match = s_superscriptPowersRE.match(expr, offset);
      if (!match.hasMatch())
          break;

      QString power = match.captured();
      for (int pos = power.size() - 1; pos >= 0; --pos) {
        QChar c = power.at(pos);
        power.replace(pos, 1, s_superscriptPowersHash.value(c, c));
      }

      bool isNegative = match.capturedStart(1) != -1;
      if (isNegative)
          power = "^(" + power + ")";
      else
          power = "^" + power;

      expr.replace(match.capturedStart(), match.capturedLength(), power);
      offset = match.capturedStart() + power.size();
    }
}

static void normalizeFunctionCaretPowers(QString& expr)
{
    auto isIdentifierChar = [](const QChar& ch) {
        return isIdentifierContinue(ch);
    };
    auto isIdentifierStartChar = [](const QChar& ch) {
        return isIdentifierStart(ch);
    };
    auto parseIntegerExponentToken = [](const QString& input, int start, int* endOut) {
        int pos = start;
        bool parenthesized = false;
        if (pos < input.size() && input.at(pos) == QLatin1Char('(')) {
            parenthesized = true;
            ++pos;
        }
        if (pos < input.size() && (input.at(pos) == QLatin1Char('+') || input.at(pos) == QLatin1Char('-')))
            ++pos;
        const int digitsStart = pos;
        while (pos < input.size() && input.at(pos).isDigit())
            ++pos;
        if (pos == digitsStart)
            return false;
        if (parenthesized) {
            if (pos >= input.size() || input.at(pos) != QLatin1Char(')'))
                return false;
            ++pos;
        }
        *endOut = pos;
        return true;
    };

    int i = 0;
    while (i < expr.size()) {
        if (!isIdentifierStartChar(expr.at(i))) {
            ++i;
            continue;
        }

        int nameEnd = i + 1;
        while (nameEnd < expr.size() && isIdentifierChar(expr.at(nameEnd)))
            ++nameEnd;

        int afterName = nameEnd;
        while (afterName < expr.size() && expr.at(afterName).isSpace())
            ++afterName;
        if (afterName >= expr.size() || expr.at(afterName) != QLatin1Char('^')) {
            i = nameEnd;
            continue;
        }

        int expStart = afterName + 1;
        while (expStart < expr.size() && expr.at(expStart).isSpace())
            ++expStart;

        int expEnd = expStart;
        if (!parseIntegerExponentToken(expr, expStart, &expEnd)) {
            i = nameEnd;
            continue;
        }

        int argsStart = expEnd;
        while (argsStart < expr.size() && expr.at(argsStart).isSpace())
            ++argsStart;
        if (argsStart >= expr.size() || expr.at(argsStart) != QLatin1Char('(')) {
            i = nameEnd;
            continue;
        }

        int depth = 0;
        int argsEnd = -1;
        for (int j = argsStart; j < expr.size(); ++j) {
            const QChar ch = expr.at(j);
            if (ch == QLatin1Char('('))
                ++depth;
            else if (ch == QLatin1Char(')')) {
                --depth;
                if (depth == 0) {
                    argsEnd = j;
                    break;
                }
            }
        }
        if (argsEnd < 0) {
            i = nameEnd;
            continue;
        }

        const QString callArgs = expr.mid(argsStart, argsEnd - argsStart + 1);
        const QString exponentToken = expr.mid(expStart, expEnd - expStart);
        const QString replacement = callArgs + QLatin1Char('^') + exponentToken;
        expr.replace(afterName, argsEnd - afterName + 1, replacement);
        i = i + replacement.size();
    }
}

QList<UserFunction> Evaluator::getUserFunctions() const
{
        return m_session ? m_session->UserFunctionsToList()
                         : QList<UserFunction>();
}

void Evaluator::setUserFunction(const UserFunction& f)
{
    if (!m_session)
        m_session = new Session;
    m_session->addUserFunction(f);
}

void Evaluator::unsetUserFunction(const QString& fname)
{
    m_session->removeUserFunction(fname);
}

void Evaluator::unsetAllUserFunctions()
{
    m_session->clearUserFunctions();
}

bool Evaluator::hasUserFunction(const QString& fname) const
{
    bool invalid = fname.isEmpty() || !m_session;
    return (invalid) ? false : m_session->hasUserFunction(fname);
}

const UserFunction* Evaluator::getUserFunction(const QString& fname) const
{
    if (hasUserFunction(fname))
        return m_session->getUserFunction(fname);
    else
        return nullptr;
}

QString Evaluator::autoFix(const QString& expr)
{
    int par = 0;
    QString result;

    // Strip off all funny characters.
    for (int c = 0; c < expr.length(); ++c) {
        const QChar ch = expr.at(c);
        if (ch.isSpace()) {
            result.append(QLatin1Char(' '));
            continue;
        }
        if (ch >= QChar(32))
            result.append(ch);
    }

    // No extra whitespaces at the beginning and at the end.
    result = result.trimmed();

    // Keep comments untouched by expression normalization.
    QString commentText;
    const int commentPos = result.indexOf('?');
    if (commentPos >= 0) {
        commentText = result.mid(commentPos + 1).trimmed();
        result = result.left(commentPos).trimmed();
    }

    // Strip trailing equal sign (=).
    while (result.endsWith("="))
        result = result.left(result.length() - 1);

    // Make committed evaluation consistent with live/selection previews by
    // swallowing safe trailing incomplete tokens (e.g. "1+2+").
    QString withoutTrailingIncompleteToken;
    if (s_expressionWithoutIgnorableTrailingToken(result, &withoutTrailingIncompleteToken))
        result = withoutTrailingIncompleteToken;

    normalizeFunctionCaretPowers(result);
    replaceSuperscriptPowersWithCaretEquivalent(result);
    result = UnicodeChars::normalizeUnitSymbolAliases(result);
    result = UnicodeChars::normalizeRootFunctionAliasesForDisplay(result);

    // Normalize symbolic multiplication to a compact "⋅" form.
    // Examples:
    //   "2×pi" -> "2⋅pi"
    //   "2   ×    pi   pi" -> "2⋅pi⋅pi"
    {
        const QString dotOperator(UnicodeChars::DotOperator);
        const Tokens tokens = Evaluator::scan(result);
        for (int i = tokens.count() - 2; i >= 1; --i) {
            const Token& op = tokens.at(i);
            if (op.asOperator() != Token::Multiplication)
                continue;

            const Token& left = tokens.at(i - 1);
            const Token& right = tokens.at(i + 1);
            const bool leftLooksMultiplicativeOperand =
                left.isIdentifier()
                || left.isNumber()
                || left.asOperator() == Token::AssociationEnd
                || left.asOperator() == Token::Factorial
                || left.asOperator() == Token::Percent;
            const bool rightLooksMultiplicativeOperand =
                right.isIdentifier()
                || right.isNumber()
                || right.asOperator() == Token::AssociationStart;
            if (!leftLooksMultiplicativeOperand || !rightLooksMultiplicativeOperand)
                continue;

            // Keep explicit multiplication sign for pure number×number.
            if (left.isNumber() && right.isNumber())
                continue;

            result.replace(op.pos(), op.size(), dotOperator);
        }

        // Accept ASCII 'u' as a typing-friendly micro prefix for units:
        // normalize identifiers to 'µ*' only when such builtin unit exists.
        for (int i = tokens.count() - 1; i >= 0; --i) {
            const Token& token = tokens.at(i);
            if (!token.isIdentifier())
                continue;

            const QString sourceText = result.mid(token.pos(), token.size()).trimmed();
            if (!sourceText.startsWith(QLatin1Char('u')) || sourceText.size() <= 1)
                continue;

            const QString normalizedText = token.text();
            if (!normalizedText.startsWith(UnicodeChars::MicroSign))
                continue;

            if (!isBuiltInVariable(normalizedText))
                continue;

            result.replace(token.pos(), token.size(), normalizedText);
        }

        // Remove extra spaces around explicit dot multiplications.
        result.replace(QRegularExpression(QStringLiteral("\\s*%1\\s*").arg(dotOperator)),
                       dotOperator);

        // Rewrite identifier adjacency to dot multiplication, except when
        // the left identifier is a known function (e.g. keep "sin pi").
        static const QRegularExpression s_adjacentIdentifiersRE(
            QStringLiteral(R"(([A-Za-z_][A-Za-z_0-9]*)\s+([A-Za-z_][A-Za-z_0-9]*))")
        );
        int offset = 0;
        while (true) {
            const QRegularExpressionMatch match =
                s_adjacentIdentifiersRE.match(result, offset);
            if (!match.hasMatch())
                break;

            const QString left = match.captured(1);
            const QString right = match.captured(2);
            const bool keepAsUnitConversionAlias =
                left.compare(QStringLiteral("in"), Qt::CaseInsensitive) == 0
                || right.compare(QStringLiteral("in"), Qt::CaseInsensitive) == 0;
            const bool keepAsFunctionCall =
                FunctionRepo::instance()->find(left) || hasUserFunction(left);
            if (keepAsUnitConversionAlias || keepAsFunctionCall) {
                offset = match.capturedStart(2);
                continue;
            }

            const int start = match.capturedStart();
            result.replace(start, match.capturedLength(),
                           left + dotOperator + right);
            offset = start + left.length() + 1 + right.length();
        }
    }

    // Rewrite common shift-vs-add/sub ambiguity:
    // "a<<b+c" -> "(a << b) + c" and "a>>b-c" -> "(a >> b) - c".
    // This targets simple three-operand forms only.
    while (true) {
        const Tokens tokens = Evaluator::scan(result);
        bool updated = false;

        for (int i = 1; i + 3 < tokens.count(); ++i) {
            const Token& left = tokens.at(i - 1);
            const Token& shift = tokens.at(i);
            const Token& right = tokens.at(i + 1);
            const Token& addSub = tokens.at(i + 2);
            const Token& tail = tokens.at(i + 3);

            const Token::Operator shiftOp = shift.asOperator();
            if (shiftOp != Token::ArithmeticLeftShift
                && shiftOp != Token::ArithmeticRightShift)
            {
                continue;
            }
            if (!left.isOperand() || !right.isOperand() || !tail.isOperand())
                continue;
            if (addSub.asOperator() != Token::Addition
                && addSub.asOperator() != Token::Subtraction)
            {
                continue;
            }

            const int startPos = left.pos();
            const int endPos = tail.pos() + tail.size();
            const QString leftText = result.mid(left.pos(), left.size()).trimmed();
            const QString shiftText =
                shiftOp == Token::ArithmeticLeftShift ? QStringLiteral("<<")
                                                      : QStringLiteral(">>");
            const QString rightText = result.mid(right.pos(), right.size()).trimmed();
            const QString addSubText = result.mid(addSub.pos(), addSub.size()).trimmed();
            const QString tailText = result.mid(tail.pos(), tail.size()).trimmed();
            const QString replacement = QStringLiteral("(%1 %2 %3) %4 %5")
                .arg(leftText, shiftText, rightText, addSubText, tailText);
            result.replace(startPos, endPos - startPos, replacement);
            updated = true;
            break;
        }

        if (!updated)
            break;
    }

    // Automagically close all parenthesis.
    Tokens tokens = Evaluator::scan(result);
    if (tokens.count()) {
        for (int i = 0; i < tokens.count(); ++i)
            if (tokens.at(i).asOperator() == Token::AssociationStart)
                ++par;
            else if (tokens.at(i).asOperator() == Token::AssociationEnd)
                --par;

        if (par < 0)
            par = 0;

        // If the scanner stops in the middle, do not bother to apply fix.
        const Token& lastToken = tokens.at(tokens.count() - 1);
        if (lastToken.pos() + lastToken.size() >= result.length())
            while (par--)
                result.append(')');
    }

    // Special treatment for simple function
    // e.g. "cos" is regarded as "cos(ans)".
    if (!result.isEmpty()) {
        Tokens tokens = Evaluator::scan(result);

        if (tokens.count() == 1
            && tokens.at(0).isIdentifier()
            && FunctionRepo::instance()->find(tokens.at(0).text()))
        {
            result.append("(ans)");
        }
    }

    if (commentPos >= 0) {
        result += QLatin1String(" ?");
        if (!commentText.isEmpty())
            result += QLatin1String(" ") + commentText;
    }

    return result;
}

QString Evaluator::dump()
{
    QString result;
    int c;

    if (m_dirty) {
        Tokens tokens = scan(m_expression);
        compile(tokens);
    }

    result = QString("Expression: [%1]\n").arg(m_expression);

    result.append("  Constants:\n");
    for (c = 0; c < m_constants.count(); ++c) {
        auto val = m_constants.at(c);
        result += QString("    #%1 = %2\n").arg(c).arg(
            DMath::format(val, Quantity::Format::Fixed())
        );
    }

    result.append("\n");
    result.append("  Identifiers:\n");
    for (c = 0; c < m_identifiers.count(); ++c) {
        result += QString("    #%1 = %2\n").arg(c).arg(m_identifiers.at(c));
    }

    result.append("\n");
    result.append("  Code:\n");
    for (int i = 0; i < m_codes.count(); ++i) {
        QString code;
        switch (m_codes.at(i).type) {
            case Opcode::Load:
                code = QString("Load #%1").arg(m_codes.at(i).index);
                break;
            case Opcode::Ref:
                code = QString("Ref #%1").arg(m_codes.at(i).index);
                break;
            case Opcode::Function:
                code = QString("Function (%1)").arg(m_codes.at(i).index);
                break;
            case Opcode::Add:
                code = "Add";
                break;
            case Opcode::Sub:
                code = "Sub";
                break;
            case Opcode::Mul:
                code = "Mul";
                break;
            case Opcode::Div:
                code = "Div";
                break;
            case Opcode::Neg:
                code = "Neg";
                break;
            case Opcode::BNot:
                code = "BNot";
                break;
            case Opcode::Pow:
                code = "Pow";
                break;
            case Opcode::Fact:
                code = "Fact";
                break;
            case Opcode::Percent:
                code = "Percent";
                break;
            case Opcode::Modulo:
                code = "Modulo";
                break;
            case Opcode::IntDiv:
                code = "IntDiv";
                break;
            case Opcode::LSh:
                code = "LSh";
                break;
            case Opcode::RSh:
                code = "RSh";
                break;
            case Opcode::BAnd:
                code = "BAnd";
                break;
            case Opcode::BOr:
                code = "BOr";
                break;
            case Opcode::Conv:
                code = "Conv";
                break;
            default:
                code = "Unknown";
                break;
        }
        result.append("   ").append(code).append("\n");
    }

    return result;
}
