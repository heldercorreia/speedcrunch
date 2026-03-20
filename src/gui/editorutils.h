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
    case 0x002A: // * ASTERISK
    case 0x00B7: // · MIDDLE DOT
    case 0x2217: // ∗ ASTERISK OPERATOR
    case 0x2219: // ∙ BULLET OPERATOR
    case 0x2715: // ✕ MULTIPLICATION X
    case 0x2716: // ✖ HEAVY MULTIPLICATION X
    case 0x2A09: // ⨉ N-ARY TIMES OPERATOR
    case 0x2A2F: // ⨯ VECTOR OR CROSS PRODUCT
        return true;
    case 0x22C5: // ⋅ DOT OPERATOR
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
    text = normalizeMultiplicationOperators(text, true);
    text = normalizeDivisionOperatorsForEditorInput(text);
    text = normalizeAdditionOperators(text);
    text = normalizeSubtractionOperators(text);
    return text;
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
