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

#ifndef MATH_OPERATORCHARS_H
#define MATH_OPERATORCHARS_H

#include "core/unicodechars.h"

#include <QChar>

namespace OperatorChars {

// Display/operator symbol constants consumed by DisplayFormatUtils.
// Spacing behavior is defined in gui/displayformatutils.h.
inline constexpr QChar MulDotSign = UnicodeChars::MiddleDot;
inline constexpr QChar MulCrossSign = UnicodeChars::MultiplicationSign;
inline constexpr QChar DivisionSign = UnicodeChars::Solidus;
inline constexpr QChar AdditionSign = UnicodeChars::PlusSign;
inline constexpr QChar SubtractionSign = UnicodeChars::MinusSign;
inline constexpr QChar SubtractionSpace = UnicodeChars::Space;
inline constexpr QChar AdditionSpace = UnicodeChars::Space;
inline constexpr QChar DivisionSpace = UnicodeChars::Space;
inline constexpr QChar MulDotSpace = UnicodeChars::Space;
inline constexpr QChar MulCrossSpace = UnicodeChars::Space;
inline constexpr QChar ValueUnitSpace = UnicodeChars::NoBreakSpace;
inline constexpr QChar SuperscriptPlus(0x207A);
inline constexpr QChar SuperscriptMinus(0x207B);

inline bool isSuperscriptDigit(const QChar& ch)
{
    switch (ch.unicode()) {
    case 0x00B9: // ¹
    case 0x00B2: // ²
    case 0x00B3: // ³
    case 0x2070: // ⁰
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

inline bool isSuperscriptSign(const QChar& ch)
{
    return ch == SuperscriptMinus || ch == SuperscriptPlus;
}

inline bool isSuperscriptPowerChar(const QChar& ch)
{
    return isSuperscriptDigit(ch) || isSuperscriptSign(ch);
}

inline QChar superscriptDigitToAscii(const QChar& ch)
{
    switch (ch.unicode()) {
    case 0x2070: return QLatin1Char('0'); // ⁰
    case 0x00B9: return QLatin1Char('1'); // ¹
    case 0x00B2: return QLatin1Char('2'); // ²
    case 0x00B3: return QLatin1Char('3'); // ³
    case 0x2074: return QLatin1Char('4'); // ⁴
    case 0x2075: return QLatin1Char('5'); // ⁵
    case 0x2076: return QLatin1Char('6'); // ⁶
    case 0x2077: return QLatin1Char('7'); // ⁷
    case 0x2078: return QLatin1Char('8'); // ⁸
    case 0x2079: return QLatin1Char('9'); // ⁹
    default: return QChar();
    }
}

inline QChar asciiDigitToSuperscript(const QChar& ch)
{
    switch (ch.unicode()) {
    case '0': return QChar(0x2070); // ⁰
    case '1': return QChar(0x00B9); // ¹
    case '2': return QChar(0x00B2); // ²
    case '3': return QChar(0x00B3); // ³
    case '4': return QChar(0x2074); // ⁴
    case '5': return QChar(0x2075); // ⁵
    case '6': return QChar(0x2076); // ⁶
    case '7': return QChar(0x2077); // ⁷
    case '8': return QChar(0x2078); // ⁸
    case '9': return QChar(0x2079); // ⁹
    default: return QChar();
    }
}

} // namespace OperatorChars

#endif // MATH_OPERATORCHARS_H
