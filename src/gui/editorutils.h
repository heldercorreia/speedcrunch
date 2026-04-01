// This file is part of the SpeedCrunch project
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef GUI_EDITORUTILS_H
#define GUI_EDITORUTILS_H

#include "core/unicodechars.h"

#include <QString>
#include <QStringList>

namespace EditorUtils {

inline bool isMultiplicationOperatorAlias(const QChar& ch, bool keepDotOperator = false)
{
    switch (ch.unicode()) {
    case UnicodeChars::Asterisk.unicode():
    case UnicodeChars::MiddleDot.unicode():
    case UnicodeChars::AsteriskOperator.unicode():
    case UnicodeChars::BulletOperator.unicode():
    case UnicodeChars::MultiplicationX.unicode():
    case UnicodeChars::HeavyMultiplicationX.unicode():
    case UnicodeChars::NAryTimesOperator.unicode():
    case UnicodeChars::VectorOrCrossProduct.unicode():
        return true;
    case UnicodeChars::DotOperator.unicode():
        return !keepDotOperator;
    default:
        return false;
    }
}

inline QString normalizeMultiplicationOperators(QString text, bool keepDotOperator = false)
{
    for (QChar& ch : text) {
        if (isMultiplicationOperatorAlias(ch, keepDotOperator))
            ch = UnicodeChars::MultiplicationSign;
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
    text = normalizeMultiplicationOperators(text);
    text = normalizeDivisionOperators(text);
    text = normalizeAdditionOperators(text);
    text = normalizeSubtractionOperators(text);
    return text;
}

inline QString normalizeExpressionOperatorsForEditorInput(QString text)
{
    text = UnicodeChars::normalizePiAliasesToPi(text);
    text = UnicodeChars::normalizeRootFunctionAliasesForDisplay(text);
    text = normalizeMultiplicationOperators(text, true);
    text = normalizeDivisionOperatorsForEditorInput(text);
    text = normalizeAdditionOperators(text);
    text = normalizeSubtractionOperators(text);
    return text;
}

inline bool isExpressionOperatorOrSeparator(const QChar& ch)
{
    return ch == QLatin1Char('+')
           || ch == UnicodeChars::MinusSign
           || ch == UnicodeChars::MultiplicationSign
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
        (last == UnicodeChars::MultiplicationSign
         || last == UnicodeChars::DotOperator
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
                (prev == UnicodeChars::MultiplicationSign
                 || prev == UnicodeChars::DotOperator
                 || isMultiplicationOperatorAlias(prev, true));
            if (prevIsMultiplication) {
                --i;
                if (i >= 0) {
                    const QChar prevPrev = trimmed.at(i);
                    const bool prevPrevIsMultiplication =
                        (prevPrev == UnicodeChars::MultiplicationSign
                         || prevPrev == UnicodeChars::DotOperator
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

}

#endif
