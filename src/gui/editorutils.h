// This file is part of the SpeedCrunch project
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef GUI_EDITORUTILS_H
#define GUI_EDITORUTILS_H

#include <QString>
#include <QStringList>

namespace EditorUtils {

inline bool isMultiplicationOperatorAlias(const QChar& ch)
{
    switch (ch.unicode()) {
    case 0x002A: // * ASTERISK
    case 0x00B7: // · MIDDLE DOT
    case 0x2217: // ∗ ASTERISK OPERATOR
    case 0x2219: // ∙ BULLET OPERATOR
    case 0x22C5: // ⋅ DOT OPERATOR
    case 0x2715: // ✕ MULTIPLICATION X
    case 0x2716: // ✖ HEAVY MULTIPLICATION X
    case 0x2A09: // ⨉ N-ARY TIMES OPERATOR
    case 0x2A2F: // ⨯ VECTOR OR CROSS PRODUCT
        return true;
    default:
        return false;
    }
}

inline QString normalizeMultiplicationOperators(QString text)
{
    for (QChar& ch : text) {
        if (isMultiplicationOperatorAlias(ch))
            ch = QChar(0x00D7); // × MULTIPLICATION SIGN
    }
    return text;
}

inline QStringList parsePastedExpressions(const QString& text)
{
    QStringList expressions;
    const QStringList candidates =
        text.split('\n', Qt::KeepEmptyParts, Qt::CaseSensitive);
    expressions.reserve(candidates.size());

    for (const auto& candidate : candidates) {
        const auto expression =
            normalizeMultiplicationOperators(candidate.trimmed());
        if (!expression.isEmpty())
            expressions.append(expression);
    }

    return expressions;
}

}

#endif
