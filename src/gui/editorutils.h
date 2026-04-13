// This file is part of the SpeedCrunch project
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef GUI_EDITORUTILS_H
#define GUI_EDITORUTILS_H

#include "core/unicodechars.h"
#include "math/operatorchars.h"

#include <QString>
#include <QStringList>

namespace EditorUtils {

enum AutoAnsRewriteMode {
    AutoAnsNoRewrite = 0,
    AutoAnsPrependAns = 1,
    AutoAnsAppendAns = 2
};

inline bool isExpressionOperatorOrSeparator(const QChar& ch);

inline bool isMultiplicationOperatorAlias(const QChar& ch, bool keepDotOperator = false)
{
    switch (ch.unicode()) {
    case UnicodeChars::Asterisk.unicode():
    case UnicodeChars::AsteriskOperator.unicode():
    case UnicodeChars::BulletOperator.unicode():
    case UnicodeChars::MultiplicationX.unicode():
    case UnicodeChars::HeavyMultiplicationX.unicode():
    case UnicodeChars::NAryTimesOperator.unicode():
    case UnicodeChars::VectorOrCrossProduct.unicode():
        return true;
    case OperatorChars::MulDotSign.unicode():
        return !keepDotOperator;
    default:
        return false;
    }
}

inline QString normalizeMultiplicationOperators(QString text, bool keepDotOperator = false)
{
    for (QChar& ch : text) {
        if (isMultiplicationOperatorAlias(ch, keepDotOperator))
            ch = OperatorChars::MulCrossSign;
    }
    return text;
}

inline bool isDivisionOperatorAlias(const QChar& ch)
{
    switch (ch.unicode()) {
    case 0x002F: // / SOLIDUS
    case 0x00F7: // ÷ DIVISION SIGN
        return true;
    default:
        return false;
    }
}

inline QString normalizeDivisionOperators(QString text)
{
    for (QChar& ch : text) {
        if (isDivisionOperatorAlias(ch))
            ch = QChar(0x29F8); // ⧸ BIG SOLIDUS
    }
    return text;
}

inline QString normalizeDivisionOperatorsForEditorInput(QString text)
{
    for (QChar& ch : text) {
        if (isDivisionOperatorAlias(ch) || ch.unicode() == 0x29F8)
            ch = QChar(0x002F); // / SOLIDUS
    }
    return text;
}

inline bool isAdditionOperatorAlias(const QChar& ch)
{
    switch (ch.unicode()) {
    case 0xFF0B: // ＋ FULLWIDTH PLUS SIGN
        return true;
    default:
        return false;
    }
}

inline QString normalizeAdditionOperators(QString text)
{
    for (QChar& ch : text) {
        if (isAdditionOperatorAlias(ch))
            ch = QChar(0x002B); // + PLUS SIGN
    }
    return text;
}

inline bool isSubtractionOperatorAlias(const QChar& ch)
{
    switch (ch.unicode()) {
    case UnicodeChars::MinusSign.unicode():
    case 0x002D: // - HYPHEN-MINUS
    case 0x2010: // ‐ HYPHEN
    case 0x2011: // ‑ NON-BREAKING HYPHEN
    case 0x2013: // – EN DASH
    case 0x2014: // — EM DASH
    case 0x2015: // ― HORIZONTAL BAR
    case 0x2043: // ⁃ HYPHEN BULLET
    case 0xFE63: // ﹣ SMALL HYPHEN-MINUS
    case 0xFF0D: // － FULLWIDTH HYPHEN-MINUS
        return true;
    default:
        return false;
    }
}

inline QString normalizeSubtractionOperators(QString text)
{
    for (QChar& ch : text) {
        if (isSubtractionOperatorAlias(ch))
            ch = UnicodeChars::MinusSign;
    }
    return text;
}

inline QString normalizeExpressionOperators(QString text)
{
    text = UnicodeChars::normalizeUnitSymbolAliases(text);
    text = UnicodeChars::normalizeSexagesimalSymbolAliases(text);
    text = normalizeMultiplicationOperators(text);
    text = normalizeDivisionOperators(text);
    text = normalizeAdditionOperators(text);
    text = normalizeSubtractionOperators(text);
    return text;
}

inline QString normalizeExpressionOperatorsForEditorInput(QString text)
{
    text = UnicodeChars::normalizeUnitSymbolAliases(text);
    text = UnicodeChars::normalizeSexagesimalSymbolAliases(text);
    text = UnicodeChars::normalizePiAliasesToPi(text);
    text = UnicodeChars::normalizeRootFunctionAliasesForDisplay(text);
    text = normalizeMultiplicationOperators(text, true);
    text = normalizeDivisionOperatorsForEditorInput(text);
    text = normalizeAdditionOperators(text);
    text = normalizeSubtractionOperators(text);
    return text;
}

inline bool shouldIgnoreTypedSpaceAfterDigit(const QString& text, int cursorPosition)
{
    if (cursorPosition <= 0 || cursorPosition > text.size())
        return false;

    int i = cursorPosition - 1;
    while (i >= 0 && text.at(i).isSpace())
        --i;
    if (i < 0)
        return false;

    const QChar prevNonSpace = text.at(i);
    return prevNonSpace.isDigit()
           || prevNonSpace.isLetter()
           || OperatorChars::isSuperscriptDigit(prevNonSpace)
           || prevNonSpace == QLatin1Char(')')
           || prevNonSpace == QLatin1Char(']');
}

inline QString adjustedTypedTextForImplicitMultiplicationAfterDigit(
    const QString& text, int cursorPosition, const QString& typedText)
{
    if (typedText.size() != 1)
        return typedText;
    if (cursorPosition <= 0 || cursorPosition > text.size())
        return typedText;

    const QChar typed = typedText.at(0);
    const auto isSingleZeroTermToLeft = [&]() {
        int i = cursorPosition - 1;
        while (i >= 0 && text.at(i).isSpace())
            --i;
        if (i < 0 || text.at(i) != QLatin1Char('0'))
            return false;
        --i;
        while (i >= 0 && text.at(i).isSpace())
            --i;
        if (i < 0)
            return true;
        const QChar left = text.at(i);
        return isExpressionOperatorOrSeparator(left)
               || left == QLatin1Char('(')
               || left == QLatin1Char('[');
    };
    QString operatorPrefix;
    const auto leftNonSpaceSupportsOperatorInsertion = [&]() {
        int i = cursorPosition - 1;
        while (i >= 0 && text.at(i).isSpace())
            --i;
        if (i < 0)
            return false;
        const QChar prevNonSpace = text.at(i);
        return prevNonSpace.isDigit()
            || prevNonSpace.isLetter()
            || OperatorChars::isSuperscriptDigit(prevNonSpace)
            || prevNonSpace == QLatin1Char(')')
            || prevNonSpace == QLatin1Char(']');
    };
    if (typed.isLetter()) {
        if (typed == QLatin1Char('e') || typed == QLatin1Char('E'))
            return typedText;
        if (typed == QLatin1Char('i') || typed == QLatin1Char('I')) {
            int i = cursorPosition - 1;
            while (i >= 0 && text.at(i).isSpace())
                --i;
            if (i < 0)
                return typedText;
            const QChar prevNonSpace = text.at(i);
            const bool shouldExpandToIn =
                prevNonSpace.isDigit()
                || OperatorChars::isSuperscriptDigit(prevNonSpace)
                || prevNonSpace == QLatin1Char(')')
                || prevNonSpace == QLatin1Char(']');
            if (!shouldExpandToIn)
                return typedText;
            return (i == cursorPosition - 1) ? (QStringLiteral(" ") + typedText) : typedText;
        }
        if ((typed == QLatin1Char('b') || typed == QLatin1Char('B')
             || typed == QLatin1Char('o') || typed == QLatin1Char('O')
             || typed == QLatin1Char('x') || typed == QLatin1Char('X'))
            && isSingleZeroTermToLeft()) {
            return typedText;
        }
        const QChar prev = text.at(cursorPosition - 1);
        if (!prev.isDigit() && !OperatorChars::isSuperscriptDigit(prev))
            return typedText;
    } else if (typed == QLatin1Char('(') || typed == QLatin1Char('[')) {
        int i = cursorPosition - 1;
        while (i >= 0 && text.at(i).isSpace())
            --i;
        if (i < 0)
            return typedText;

        const QChar prevNonSpace = text.at(i);
        if (!prevNonSpace.isDigit()
            && !OperatorChars::isSuperscriptDigit(prevNonSpace)
            && !prevNonSpace.isLetter()) {
            return typedText;
        }
    } else if (typed == OperatorChars::AdditionSign || isAdditionOperatorAlias(typed)) {
        if (!leftNonSpaceSupportsOperatorInsertion())
            return typedText;
        operatorPrefix = QString(OperatorChars::AdditionSpace)
            + QString(OperatorChars::AdditionSign)
            + QString(OperatorChars::AdditionSpace);
    } else if (isSubtractionOperatorAlias(typed)) {
        if (!leftNonSpaceSupportsOperatorInsertion())
            return typedText;
        operatorPrefix = QString(OperatorChars::SubtractionSpace)
            + QString(OperatorChars::SubtractionSign)
            + QString(OperatorChars::SubtractionSpace);
    } else if (typed == OperatorChars::DivisionSign || isDivisionOperatorAlias(typed)) {
        if (!leftNonSpaceSupportsOperatorInsertion())
            return typedText;
        operatorPrefix = QString(OperatorChars::DivisionSpace)
            + QString(OperatorChars::DivisionSign)
            + QString(OperatorChars::DivisionSpace);
    } else if (typed == OperatorChars::MulCrossSign
               || typed == OperatorChars::MulDotSign
               || isMultiplicationOperatorAlias(typed, true)) {
        if (!leftNonSpaceSupportsOperatorInsertion())
            return typedText;
        const bool useDotSign = typed == OperatorChars::MulDotSign;
        const QChar sign = useDotSign ? OperatorChars::MulDotSign : OperatorChars::MulCrossSign;
        const QChar space = useDotSign ? OperatorChars::MulDotSpace : OperatorChars::MulCrossSpace;
        operatorPrefix = QString(space) + QString(sign) + QString(space);
    } else if (typed == QLatin1Char('=')) {
        if (!leftNonSpaceSupportsOperatorInsertion())
            return typedText;
        operatorPrefix = QStringLiteral(" = ");
    } else {
        return typedText;
    }

    if (!operatorPrefix.isEmpty()) {
        // Avoid producing double spaces when the cursor is already after a
        // space and we are about to insert a grouped " <op> " sequence.
        if (cursorPosition > 0 && text.at(cursorPosition - 1).isSpace()
            && operatorPrefix.size() >= 3
            && operatorPrefix.at(0).isSpace()) {
            return operatorPrefix.mid(1);
        }
        return operatorPrefix;
    }

    if (typed == QLatin1Char('(')) {
        int i = cursorPosition - 1;
        while (i >= 0 && text.at(i).isSpace())
            --i;
        if (i >= 0) {
            const QChar prevNonSpace = text.at(i);
            bool numericLeftTerm = prevNonSpace.isDigit()
                                   && !OperatorChars::isSuperscriptDigit(prevNonSpace);
            if (!numericLeftTerm && OperatorChars::isSuperscriptDigit(prevNonSpace)) {
                int baseIndex = i;
                while (baseIndex >= 0 && OperatorChars::isSuperscriptPowerChar(text.at(baseIndex)))
                    --baseIndex;
                numericLeftTerm = baseIndex >= 0 && text.at(baseIndex).isDigit();
            }
            if (numericLeftTerm) {
                return QString(OperatorChars::MulCrossSpace)
                       + QString(OperatorChars::MulCrossSign)
                       + QString(OperatorChars::MulCrossSpace)
                       + typedText;
            }
        }
    }

    return QString(OperatorChars::MulDotSpace)
           + QString(OperatorChars::MulDotSign)
           + QString(OperatorChars::MulDotSpace)
           + typedText;
}

inline bool isExpressionOperatorOrSeparator(const QChar& ch)
{
    return ch == QLatin1Char('+')
           || ch == UnicodeChars::MinusSign
           || ch == OperatorChars::MulCrossSign
           || ch == OperatorChars::MulDotSign
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

inline bool isAnyOperator(const QChar& ch)
{
    return ch == QLatin1Char('+')
           || ch == QLatin1Char('-')
           || ch == QLatin1Char('*')
           || ch == QLatin1Char('/')
           || ch == QLatin1Char('%')
           || ch == QLatin1Char('^')
           || ch == QLatin1Char('&')
           || ch == QLatin1Char('|')
           || ch == QLatin1Char('=')
           || ch == QLatin1Char('<')
           || ch == QLatin1Char('>')
           || isAdditionOperatorAlias(ch)
           || isSubtractionOperatorAlias(ch)
           || isDivisionOperatorAlias(ch)
           || isMultiplicationOperatorAlias(ch, true);
}

inline bool endsWithIncompleteExpressionToken(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return false;

    const QChar last = trimmed.at(trimmed.size() - 1);
    return last == QLatin1Char('(') || isExpressionOperatorOrSeparator(last);
}

inline bool expressionWithoutIgnorableTrailingToken(const QString& text, QString* out)
{
    if (!out)
        return false;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return false;

    int i = trimmed.size() - 1;
    const QChar last = trimmed.at(i);
    const bool isPlusMinusTail =
        (last == QLatin1Char('+') || isSubtractionOperatorAlias(last));
    const bool isMultiplicationTail =
        (last == OperatorChars::MulCrossSign
         || last == OperatorChars::MulDotSign
         || isMultiplicationOperatorAlias(last, true));

    if (last != QLatin1Char('(')
        && !isPlusMinusTail
        && !isMultiplicationTail
        && last != QLatin1Char(';')
        && last != QLatin1Char('/')
        && last != QLatin1Char('^')
        && last != QLatin1Char('\\'))
        return false;

    // Allow safe trailing suffixes:
    // - one or more '+'
    // - one or more '−'
    // - one or more '('
    // - one or more '\'
    // - one '/' only
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
                          || isSubtractionOperatorAlias(trimmed.at(i))))
            --i;
    } else if (isMultiplicationTail) {
        --i;
        if (i >= 0) {
            const QChar prev = trimmed.at(i);
            const bool prevIsMultiplication =
                (prev == OperatorChars::MulCrossSign
                 || prev == OperatorChars::MulDotSign
                 || isMultiplicationOperatorAlias(prev, true));
            if (prevIsMultiplication) {
                --i;
                if (i >= 0) {
                    const QChar prevPrev = trimmed.at(i);
                    const bool prevPrevIsMultiplication =
                        (prevPrev == OperatorChars::MulCrossSign
                         || prevPrev == OperatorChars::MulDotSign
                         || isMultiplicationOperatorAlias(prevPrev, true));
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
        || isExpressionOperatorOrSeparator(prefixLast)
        || prefixLast == QLatin1Char('\\'))
        return false;

    *out = prefix;
    return true;
}

template <typename NormalizeExpression>
inline QStringList parsePastedExpressionsImpl(const QString& text, NormalizeExpression normalizeExpression)
{
    QStringList expressions;
    const QStringList candidates =
        text.split('\n', Qt::KeepEmptyParts, Qt::CaseSensitive);
    expressions.reserve(candidates.size());

    for (const auto& candidate : candidates) {
        const auto expression = normalizeExpression(candidate.trimmed());
        if (!expression.isEmpty())
            expressions.append(expression);
    }

    return expressions;
}

inline QStringList parsePastedExpressions(const QString& text)
{
    return parsePastedExpressionsImpl(text, normalizeExpressionOperators);
}

inline QStringList parsePastedExpressionsForEditorInput(const QString& text)
{
    return parsePastedExpressionsImpl(text, normalizeExpressionOperatorsForEditorInput);
}

inline AutoAnsRewriteMode autoAnsRewriteModeForLeadingOperator(const QString& operatorText)
{
    if (operatorText.size() != 1)
        return AutoAnsNoRewrite;

    const QChar op = normalizeExpressionOperatorsForEditorInput(operatorText).at(0);
    if (op == QLatin1Char('~'))
        return AutoAnsAppendAns;
    if (op == OperatorChars::AdditionSign
        || isSubtractionOperatorAlias(op)
        || op == OperatorChars::MulCrossSign
        || op == OperatorChars::MulDotSign
        || op == OperatorChars::DivisionSign
        || op == QLatin1Char('^')
        || op == QLatin1Char('!')) {
        return AutoAnsPrependAns;
    }
    return AutoAnsNoRewrite;
}

inline QString applyAutoAnsRewrite(const QString& expression, AutoAnsRewriteMode mode)
{
    if (mode == AutoAnsAppendAns)
        return expression + QStringLiteral("ans");
    if (mode == AutoAnsPrependAns)
        return QStringLiteral("ans") + expression;
    return expression;
}

inline bool isAllowedLeadingCharAtExpressionStart(const QChar& ch, bool autoAnsEnabled)
{
    if (ch.isDigit())
        return true;
    if (ch.isLetter()) {
        // Exclude ordinal indicators that behave like symbols on PT/US layouts
        // and are not intended to be valid leading tokens.
        if (ch == QChar(0x00AA) || ch == QChar(0x00BA))
            return false;
        return true;
    }
    if (ch.category() == QChar::Symbol_Currency)
        return true;
    if (isSubtractionOperatorAlias(ch))
        return true;
    if (ch == QLatin1Char('~')
        || ch == QLatin1Char('(')
        || ch == QLatin1Char('#')
        || ch == QLatin1Char('?')) {
        return true;
    }
    if (!autoAnsEnabled)
        return false;

    // When auto-ans is enabled, allow leading operator starts that can be
    // rewritten as "ans <op> ..." or "~ans".
    return ch == OperatorChars::AdditionSign
           || ch == OperatorChars::DivisionSign
           || ch == OperatorChars::MulCrossSign
           || ch == OperatorChars::MulDotSign
           || ch == QLatin1Char('!')
           || isAdditionOperatorAlias(ch)
           || isDivisionOperatorAlias(ch)
           || isMultiplicationOperatorAlias(ch, true)
           || ch == QLatin1Char('^');
}

}

#endif
