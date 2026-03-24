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

#include <QRegularExpression>
#include <QString>

namespace SimplifiedExpressionUtils {

inline bool isNumericSimplifiedExpression(const QString& expression)
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*[+\\-−]?\\s*(?:0[xX][0-9A-Fa-f]+(?:[\\.,][0-9A-Fa-f]+)?|0[oO][0-7]+(?:[\\.,][0-7]+)?|0[bB][01]+(?:[\\.,][01]+)?|\\d+(?:[\\.,]\\d+)?(?:[eE][+\\-]?\\d+)?)\\s*$"));
    return pattern.match(expression).hasMatch();
}

inline bool isPlainNumericArithmeticExpression(const QString& expression)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[\\s\\p{Zs}]*[0-9\\.,\\s\\p{Zs}+\\-−*/%()]+[\\s\\p{Zs}]*$"));
    return pattern.match(expression).hasMatch();
}

inline bool shouldSuppressSimplifiedExpressionLine(const QString& interpretedDisplay,
                                                   const QString& simplifiedDisplay)
{
    return isPlainNumericArithmeticExpression(interpretedDisplay)
        && (isPlainNumericArithmeticExpression(simplifiedDisplay)
            || isNumericSimplifiedExpression(simplifiedDisplay));
}

} // namespace SimplifiedExpressionUtils

#endif // GUI_SIMPLIFIEDEXPRESSIONUTILS_H
