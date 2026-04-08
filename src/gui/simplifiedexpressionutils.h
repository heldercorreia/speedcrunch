// This file is part of the SpeedCrunch project
// Copyright (C) 2026 @heldercorreia
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

#ifndef GUI_SIMPLIFIEDEXPRESSIONUTILS_H
#define GUI_SIMPLIFIEDEXPRESSIONUTILS_H

#include "core/regexpatterns.h"
#include "core/unicodechars.h"
#include "math/operatorchars.h"

#include <QString>

namespace SimplifiedExpressionUtils {

inline bool isNumericSimplifiedExpression(const QString& expression)
{
    return RegExpPatterns::signedNumericLiteralWithSpaces().match(expression).hasMatch();
}

inline bool isPlainNumericArithmeticExpression(const QString& expression)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[\\s\\p{Zs}]*[0-9\\.,\\s\\p{Zs}+\\-−*/%()]+[\\s\\p{Zs}]*$"));
    return pattern.match(expression).hasMatch();
}

inline QString normalizedTokenForCommutativeComparison(const QString& token)
{
    QString normalized = token;
    normalized.remove(QRegularExpression(QStringLiteral("[\\s\\p{Zs}]")));
    return normalized;
}

inline bool splitSingleTopLevelBinary(const QString& expression, QChar op,
                                      QString* leftOut, QString* rightOut)
{
    int depth = 0;
    int splitPos = -1;
    int splitCount = 0;
    for (int i = 0; i < expression.size(); ++i) {
        const QChar ch = expression.at(i);
        if (ch == QLatin1Char('('))
            ++depth;
        else if (ch == QLatin1Char(')'))
            --depth;
        else if (depth == 0 && ch == op) {
            // Keep only a single top-level operator (e.g. a+b, not a+b+c).
            splitPos = i;
            ++splitCount;
            if (splitCount > 1)
                return false;
        }
    }

    if (splitCount != 1 || splitPos <= 0 || splitPos >= expression.size() - 1)
        return false;

    const QString left = expression.left(splitPos).trimmed();
    const QString right = expression.mid(splitPos + 1).trimmed();
    if (left.isEmpty() || right.isEmpty())
        return false;

    *leftOut = left;
    *rightOut = right;
    return true;
}

inline bool isCommutativeTopLevelSwap(const QString& interpretedDisplay,
                                      const QString& simplifiedDisplay)
{
    const QChar commutativeOps[] = {
        QLatin1Char('+'),
        QLatin1Char('*'),
        QChar(OperatorChars::MulCrossSign),
        QChar(OperatorChars::MulDotSign)
    };
    for (const QChar op : commutativeOps) {
        QString leftA, rightA, leftB, rightB;
        if (!splitSingleTopLevelBinary(interpretedDisplay, op, &leftA, &rightA))
            continue;
        if (!splitSingleTopLevelBinary(simplifiedDisplay, op, &leftB, &rightB))
            continue;

        if (normalizedTokenForCommutativeComparison(leftA)
                == normalizedTokenForCommutativeComparison(rightB)
            && normalizedTokenForCommutativeComparison(rightA)
                == normalizedTokenForCommutativeComparison(leftB)) {
            return true;
        }
    }
    return false;
}

inline bool shouldSuppressSimplifiedExpressionLine(const QString& interpretedDisplay,
                                                   const QString& simplifiedDisplay)
{
    return isCommutativeTopLevelSwap(interpretedDisplay, simplifiedDisplay)
        || (isPlainNumericArithmeticExpression(interpretedDisplay)
        && (isPlainNumericArithmeticExpression(simplifiedDisplay)
            || isNumericSimplifiedExpression(simplifiedDisplay)));
}

} // namespace SimplifiedExpressionUtils

#endif // GUI_SIMPLIFIEDEXPRESSIONUTILS_H
