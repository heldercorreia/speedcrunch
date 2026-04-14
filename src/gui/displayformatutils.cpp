// This file is part of the SpeedCrunch project
// Copyright (C) 2007 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2004, 2005 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2005, 2006 Johan Thelin <e8johan@gmail.com>
// Copyright (C) 2007-2026 @heldercorreia
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

#include "gui/displayformatutils.h"

#include "core/evaluator.h"
#include "core/numberformatter.h"
#include "core/regexpatterns.h"
#include "core/unicodechars.h"
#include "core/mathdsl.h"

namespace DisplayFormatUtils {

namespace {

struct ArrowRange
{
    int pos = -1;
    int size = 0;
};

ArrowRange findConversionArrow(const QString& text)
{
    const int unicodeArrowPos = text.indexOf(MathDsl::TransOp);
    if (unicodeArrowPos >= 0)
        return {unicodeArrowPos, 1};

    const int asciiArrowPos = text.indexOf(QStringLiteral("->"));
    if (asciiArrowPos >= 0)
        return {asciiArrowPos, 2};

    return {};
}

bool sourceHasBracketedConversionTarget(const QString& sourceExpression)
{
    if (sourceExpression.isEmpty())
        return false;

    const ArrowRange arrow = findConversionArrow(sourceExpression);
    if (arrow.pos < 0)
        return false;

    int sourceTargetStart = arrow.pos + arrow.size;
    while (sourceTargetStart < sourceExpression.size()
           && sourceExpression.at(sourceTargetStart).isSpace()) {
        ++sourceTargetStart;
    }

    return sourceTargetStart < sourceExpression.size()
        && sourceExpression.at(sourceTargetStart) == QLatin1Char('[');
}

bool tokenCanEndOperandForSpacing(const Token& token)
{
    return token.isOperand()
        || token.asOperator() == Token::AssociationEnd
        || token.asOperator() == Token::Factorial
        || token.asOperator() == Token::Percent;
}

bool tokenCanStartOperandForSpacing(const Token& token)
{
    return token.isOperand()
        || token.asOperator() == Token::AssociationStart
        || token.asOperator() == Token::Addition
        || token.asOperator() == Token::Subtraction
        || token.asOperator() == Token::BitwiseLogicalNOT;
}

QString spacingForBinaryOperator(const Token& token, const QString& operatorText)
{
    const Token::Operator op = token.asOperator();
    switch (op) {
    case Token::Addition:
        return QString(MathDsl::AddWrap)
            + MathDsl::AddOp
            + MathDsl::AddWrap;
    case Token::Subtraction:
        return QString(MathDsl::SubWrap)
            + MathDsl::SubOp
            + MathDsl::SubWrap;
    case Token::Division:
        return QString(MathDsl::DivWrap)
            + MathDsl::DivOp
            + MathDsl::DivWrap;
    case Token::Multiplication: {
        const bool useCrossSign = operatorText == QString(MathDsl::MulCrossOp)
            || operatorText == QStringLiteral("*");
        const QChar sign = useCrossSign ? MathDsl::MulCrossOp : MathDsl::MulDotOp;
        const QChar space = useCrossSign ? MathDsl::MulCrossWrap : MathDsl::MulDotWrap;
        return QString(space) + sign + space;
    }
    default:
        return token.text();
    }
}

QString signForUnitOperator(const Token& token, const QString& operatorText)
{
    const Token::Operator op = token.asOperator();
    switch (op) {
    case Token::Addition:
        return QString(MathDsl::AddOp);
    case Token::Subtraction:
        return QString(MathDsl::SubOp);
    case Token::Division:
        return QString(MathDsl::DivOp);
    case Token::Multiplication:
        return operatorText == QString(MathDsl::MulCrossOp)
            ? QString(MathDsl::MulCrossOp)
            : QString(MathDsl::MulDotOp);
    default:
        return token.text();
    }
}

QString compactCompositeUnitSpacing(QString text)
{
    static const QRegularExpression compactUnitOps(
        QStringLiteral(R"(\s*([·×/])\s*)"));

    int pos = 0;
    while (true) {
        const int open = text.indexOf(QLatin1Char('['), pos);
        if (open < 0)
            break;
        const int close = text.indexOf(QLatin1Char(']'), open + 1);
        if (close < 0)
            break;

        QString segment = text.mid(open + 1, close - open - 1);
        segment.replace(compactUnitOps, QStringLiteral("\\1"));
        text.replace(open + 1, close - open - 1, segment);
        pos = open + segment.size() + 2;
    }

    const int arrow = text.indexOf(MathDsl::TransOp);
    if (arrow >= 0) {
        int rhsStart = arrow + 1;
        while (rhsStart < text.size() && text.at(rhsStart).isSpace())
            ++rhsStart;

        int rhsEnd = text.indexOf(QLatin1Char('?'), rhsStart);
        if (rhsEnd < 0)
            rhsEnd = text.size();

        if (rhsStart < rhsEnd) {
            QString rhs = text.mid(rhsStart, rhsEnd - rhsStart);
            rhs.replace(compactUnitOps, QStringLiteral("\\1"));
            text.replace(rhsStart, rhsEnd - rhsStart, rhs);
        }
    }

    return text;
}

} // namespace

QString applyOperatorSpacingForDisplay(const QString& input)
{
    const Tokens tokens = Evaluator::instance()->scan(input);
    if (!tokens.valid() || tokens.isEmpty())
        return input;

    QString output;
    output.reserve(input.size() + 16);

    int lastPos = 0;
    int unitBracketDepth = 0;
    for (int i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens.at(i);
        if (token.pos() < lastPos || token.pos() < 0 || token.size() < 0
            || token.pos() + token.size() > input.size()) {
            return input;
        }
        const QString originalTokenText = input.mid(token.pos(), token.size());
        output += input.mid(lastPos, token.pos() - lastPos);
        lastPos = token.pos() + token.size();

        const Token::Operator op = token.asOperator();
        if (op == Token::AssociationStart && originalTokenText == QLatin1String("[")) {
            ++unitBracketDepth;
            output += originalTokenText;
            continue;
        }
        if (op == Token::AssociationEnd && originalTokenText == QLatin1String("]")) {
            if (unitBracketDepth > 0)
                --unitBracketDepth;
            output += originalTokenText;
            continue;
        }

        const bool isSpacingOperator = op == Token::Addition
            || op == Token::Subtraction
            || op == Token::Multiplication
            || op == Token::Division;
        const bool hasLeftOperand = i > 0 && tokenCanEndOperandForSpacing(tokens.at(i - 1));
        const bool hasRightOperand = i + 1 < tokens.size()
            && tokenCanStartOperandForSpacing(tokens.at(i + 1));
        const bool isBinaryOperator = hasLeftOperand && hasRightOperand;
        const bool isUnitCompositeOperator =
            (op == Token::Multiplication || op == Token::Division)
            && i > 0
            && i + 1 < tokens.size()
            && tokens.at(i - 1).isUnitIdentifier()
            && tokens.at(i + 1).isUnitIdentifier();

        if (!isSpacingOperator || !isBinaryOperator) {
            output += originalTokenText;
            continue;
        }

        while (!output.isEmpty() && output.back().isSpace())
            output.chop(1);

        output += (unitBracketDepth > 0 || isUnitCompositeOperator)
            ? signForUnitOperator(token, originalTokenText)
            : spacingForBinaryOperator(token, originalTokenText);

        while (lastPos < input.size() && input.at(lastPos).isSpace())
            ++lastPos;
    }

    output += input.mid(lastPos);

    auto isUnitOp = [](QChar ch) {
        return ch == MathDsl::MulDotOp
            || ch == MathDsl::MulCrossOp
            || ch == MathDsl::DivOp;
    };
    auto isUnitLike = [](QChar ch) {
        return ch.isLetter()
            || ch == QLatin1Char('_')
            || ch == UnicodeChars::MicroSign
            || ch == UnicodeChars::GreekCapitalOmega
            || ch == UnicodeChars::DegreeSign // °
            || ch == MathDsl::PowNeg // ⁻
            || MathDsl::isSuperscriptDigit(ch)
            || ch == QLatin1Char('(')
            || ch == QLatin1Char(')')
            || ch == QLatin1Char('[')
            || ch == QLatin1Char(']');
    };

    int i = 0;
    while (i < output.size()) {
        if (!isUnitOp(output.at(i))) {
            ++i;
            continue;
        }

        int left = i - 1;
        while (left >= 0 && output.at(left).isSpace())
            --left;
        int right = i + 1;
        while (right < output.size() && output.at(right).isSpace())
            ++right;

        if (left < 0 || right >= output.size()
            || !isUnitLike(output.at(left))
            || !isUnitLike(output.at(right))) {
            ++i;
            continue;
        }

        int removeStart = left + 1;
        int removeCount = i - removeStart;
        if (removeCount > 0) {
            output.remove(removeStart, removeCount);
            i = removeStart;
        }

        const int opPos = i;
        int next = opPos + 1;
        while (next < output.size() && output.at(next).isSpace())
            ++next;
        const int rightSpaces = next - (opPos + 1);
        if (rightSpaces > 0)
            output.remove(opPos + 1, rightSpaces);

        ++i;
    }
    return output;
}

QString applyValueUnitSpacingForDisplay(const QString& input)
{
    QString output;
    output.reserve(input.size() + 8);
    for (const QChar ch : input) {
        if (ch == QLatin1Char('[') && !output.isEmpty()) {
            if (output.back().isSpace()) {
                int i = output.size() - 1;
                while (i >= 0 && output.at(i).isSpace())
                    --i;
                if (i >= 0 && output.at(i).isDigit()) {
                    output.truncate(i + 1);
                    output += MathDsl::QuantitySpace;
                }
            } else if (output.back().isDigit()) {
                output += MathDsl::QuantitySpace;
            }
        }
        output += ch;
    }
    return output;
}

QString applyDigitGroupingForDisplay(const QString& input)
{
    // Even when grouping is disabled we still pass numeric tokens through
    // NumberFormatter::formatNumericLiteralForDisplay(), because that helper
    // also normalizes decimal separator display according to the selected
    // number-format style (for example dot-internal -> comma-display).
    // Grouping can be a no-op there while decimal-separator conversion still
    // needs to happen.
    QString output;
    int lastPos = 0;
    auto it = RegExpPatterns::numericToken().globalMatch(input);
    while (it.hasNext()) {
        const auto match = it.next();
        output += input.mid(lastPos, match.capturedStart() - lastPos);
        output += NumberFormatter::formatNumericLiteralForDisplay(match.captured(0));
        lastPos = match.capturedEnd();
    }
    output += input.mid(lastPos);
    output = UnicodeChars::normalizePiForDisplay(output);
    output = UnicodeChars::normalizeRootFunctionAliasesForDisplay(output);
    output = applyOperatorSpacingForDisplay(output);
    output = compactCompositeUnitSpacing(output);
    return applyValueUnitSpacingForDisplay(output);
}

QString preserveConversionTargetBracketsForDisplay(const QString& displayedInterpreted,
                                                   const QString& sourceExpression)
{
    if (displayedInterpreted.isEmpty())
        return displayedInterpreted;

    const bool hasSourceBracketedTarget = sourceHasBracketedConversionTarget(sourceExpression);

    const ArrowRange arrow = findConversionArrow(displayedInterpreted);
    if (arrow.pos < 0)
        return displayedInterpreted;

    int targetStart = arrow.pos + arrow.size;
    while (targetStart < displayedInterpreted.size() && displayedInterpreted.at(targetStart).isSpace())
        ++targetStart;
    if (targetStart >= displayedInterpreted.size())
        return displayedInterpreted;
    if (displayedInterpreted.at(targetStart) == QLatin1Char('['))
        return displayedInterpreted;

    bool shouldWrapTarget = hasSourceBracketedTarget;
    if (!shouldWrapTarget) {
        const QString lhs = displayedInterpreted.left(arrow.pos);
        const bool hasBracketedSourceUnit = lhs.contains(QLatin1Char('['))
            && lhs.contains(QLatin1Char(']'));
        shouldWrapTarget = hasBracketedSourceUnit;
    }
    if (!shouldWrapTarget)
        return displayedInterpreted;

    int targetEnd = displayedInterpreted.indexOf(QStringLiteral(" ?"), targetStart);
    if (targetEnd < 0)
        targetEnd = displayedInterpreted.size();

    const QString target = displayedInterpreted.mid(targetStart, targetEnd - targetStart).trimmed();
    if (target.isEmpty())
        return displayedInterpreted;

    return displayedInterpreted.left(targetStart)
        + QLatin1Char('[')
        + target
        + QLatin1Char(']')
        + displayedInterpreted.mid(targetEnd);
}

} // namespace DisplayFormatUtils
