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

#ifndef CORE_REGEXPATTERNS_H
#define CORE_REGEXPATTERNS_H

#include <QRegularExpression>

namespace RegExpPatterns {

inline const QRegularExpression& numericToken()
{
    static const QRegularExpression pattern(
        QStringLiteral("(?<![\\p{L}\\p{N}])(?:0[xX][0-9A-Fa-f]+(?:[\\.,][0-9A-Fa-f]+)?|0[oO][0-7]+(?:[\\.,][0-7]+)?|0[bB][01]+(?:[\\.,][01]+)?|\\d+(?:[\\.,]\\d+)?(?:[eE][+\\-]?\\d+)?)(?![\\p{L}\\p{N}])"));
    return pattern;
}

inline const QRegularExpression& signedNumericLiteralWithSpaces()
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*[+\\-−]?\\s*(?:0[xX][0-9A-Fa-f]+(?:[\\.,][0-9A-Fa-f]+)?|0[oO][0-7]+(?:[\\.,][0-7]+)?|0[bB][01]+(?:[\\.,][01]+)?|\\d+(?:[\\.,]\\d+)?(?:[eE][+\\-]?\\d+)?)\\s*$"));
    return pattern;
}

inline const QRegularExpression& decimalExponentSuffix()
{
    static const QRegularExpression pattern(QStringLiteral("[eE][+\\-]?\\d+$"));
    return pattern;
}

inline const QRegularExpression& radixSeparator()
{
    static const QRegularExpression pattern(QStringLiteral("[\\.,]"));
    return pattern;
}

inline const QRegularExpression& lineBreak()
{
    static const QRegularExpression pattern(
        QStringLiteral("[\\n\\r\\x{0085}\\x{2028}\\x{2029}]")
    );
    return pattern;
}

inline const QRegularExpression& separatorToken()
{
    static const QRegularExpression pattern(
        // Match everything that is not alphanumeric or an expression operator.
        QStringLiteral(R"([^a-zA-Z0-9\+\-\−\*\×·÷\/⧸\^;\(\)\[\]%!=\\&\|<>\?#→\x0000])")
    );
    return pattern;
}

inline const QRegularExpression& trigFunctionCall()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(\b(?:sin|cos|tan|cot|sec|csc|arcsin|arccos|arctan|arctan2|radians|degrees|gradians)\s*\()"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

inline const QRegularExpression& trivialSingleFunctionCall()
{
    static const QRegularExpression pattern(
        // Keep this intentionally strict (ASCII identifier only) so we don't
        // hide meaningful simplifications like "f²(x)".
        QStringLiteral("^\\s*[+\\-−]?\\s*[A-Za-z_][A-Za-z0-9_]*\\s*\\(.*\\)\\s*$"));
    return pattern;
}

inline const QRegularExpression& unsignedDecimalInteger()
{
    static const QRegularExpression pattern(QStringLiteral(R"(^\d+$)"));
    return pattern;
}

inline const QRegularExpression& unsignedDecimalNumber()
{
    static const QRegularExpression pattern(QStringLiteral(R"(^\d+(?:\.\d+)?$)"));
    return pattern;
}

inline const QRegularExpression& signedDecimalNumber()
{
    static const QRegularExpression pattern(QStringLiteral(R"(^[+\-]?\d+(?:\.\d+)?$)"));
    return pattern;
}

inline const QRegularExpression& unsignedIntegerEquivalentDecimal()
{
    static const QRegularExpression pattern(QStringLiteral(R"(^(\d+)\.(\d*)$)"));
    return pattern;
}

inline const QRegularExpression& simpleUnitIdentifier()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z_µμΩ][A-Za-z0-9_µμΩ]*$"));
    return pattern;
}

inline const QRegularExpression& simpleParenthesizedDenominatorQuotient()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^\s*[+\-−]?\d+(?:\.\d+)?\s*/\s*\([^()]+\)\s*$)"));
    return pattern;
}

} // namespace RegExpPatterns

#endif
