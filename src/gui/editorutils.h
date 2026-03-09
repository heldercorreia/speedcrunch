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

inline QStringList parsePastedExpressions(const QString& text)
{
    QStringList expressions;
    const QStringList candidates =
        text.split('\n', Qt::KeepEmptyParts, Qt::CaseSensitive);
    expressions.reserve(candidates.size());

    for (const auto& candidate : candidates) {
        const auto expression = candidate.trimmed();
        if (!expression.isEmpty())
            expressions.append(expression);
    }

    return expressions;
}

}

#endif
