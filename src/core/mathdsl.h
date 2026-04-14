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

#ifndef CORE_MATHDSL_H
#define CORE_MATHDSL_H

#include "core/unicodechars.h"

#include <QChar>
#include <QString>

namespace MathDsl {

// Display/operator symbol constants consumed by DisplayFormatUtils.
// Spacing behavior is defined in gui/displayformatutils.h.
inline constexpr QChar MulDotOp = UnicodeChars::MiddleDot;
inline constexpr QChar MulCrossOp = UnicodeChars::MultiplicationSign;
inline constexpr QChar DivOp = UnicodeChars::Solidus;
inline constexpr QChar TransOp = UnicodeChars::RightwardsArrow;
inline constexpr QChar PowOp = UnicodeChars::CircumflexAccent;
inline constexpr QChar TimeSep = UnicodeChars::Colon;
inline constexpr QChar AddOp = UnicodeChars::PlusSign;
inline constexpr QChar Equals = UnicodeChars::EqualsSign;
inline constexpr QChar SubOp = UnicodeChars::MinusSign;
inline constexpr QChar SubOpAlt1 = UnicodeChars::HyphenMinus;
inline constexpr QChar Deg = UnicodeChars::DegreeSign;
inline constexpr QChar CommaSep = UnicodeChars::Comma;
inline constexpr QChar DotSep = UnicodeChars::FullStop;
inline constexpr QChar MinOp = UnicodeChars::Prime;
inline constexpr QChar MinOpAlt1 = UnicodeChars::Apostrophe;
inline constexpr QChar SecOp = UnicodeChars::DoublePrime;
inline constexpr QChar SecOpAlt1 = UnicodeChars::QuotationMark;
inline constexpr QChar IdivOp = UnicodeChars::ReverseSolidus;
inline constexpr QChar SubWrap = UnicodeChars::Space;
inline constexpr QChar AddWrap = UnicodeChars::Space;
inline constexpr QChar DivWrap = UnicodeChars::Space;
inline constexpr QChar MulDotWrap = UnicodeChars::Space;
inline constexpr QChar MulCrossWrap = UnicodeChars::Space;
inline constexpr QChar QuantitySpace = UnicodeChars::NoBreakSpace;
inline constexpr QChar Dig0 = UnicodeChars::DigitZero;
inline constexpr QChar Dig1 = UnicodeChars::DigitOne;
inline constexpr QChar Dig2 = UnicodeChars::DigitTwo;
inline constexpr QChar Dig3 = UnicodeChars::DigitThree;
inline constexpr QChar Dig4 = UnicodeChars::DigitFour;
inline constexpr QChar Dig5 = UnicodeChars::DigitFive;
inline constexpr QChar Dig6 = UnicodeChars::DigitSix;
inline constexpr QChar Dig7 = UnicodeChars::DigitSeven;
inline constexpr QChar Dig8 = UnicodeChars::DigitEight;
inline constexpr QChar Dig9 = UnicodeChars::DigitNine;
inline constexpr QChar PowPos = UnicodeChars::SuperscriptPlus;
inline constexpr QChar PowNeg = UnicodeChars::SuperscriptMinus;
inline constexpr QChar Pow0 = UnicodeChars::SuperscriptZero;
inline constexpr QChar Pow1 = UnicodeChars::SuperscriptOne;
inline constexpr QChar Pow2 = UnicodeChars::SuperscriptTwo;
inline constexpr QChar Pow3 = UnicodeChars::SuperscriptThree;
inline constexpr QChar Pow4 = UnicodeChars::SuperscriptFour;
inline constexpr QChar Pow5 = UnicodeChars::SuperscriptFive;
inline constexpr QChar Pow6 = UnicodeChars::SuperscriptSix;
inline constexpr QChar Pow7 = UnicodeChars::SuperscriptSeven;
inline constexpr QChar Pow8 = UnicodeChars::SuperscriptEight;
inline constexpr QChar Pow9 = UnicodeChars::SuperscriptNine;
inline constexpr QChar OpenParen = UnicodeChars::LeftParenthesis;
inline constexpr QChar CloseParen = UnicodeChars::RightParenthesis;
inline constexpr QChar OpenUnit = UnicodeChars::LeftSquareBracket;
inline constexpr QChar CloseUnit = UnicodeChars::RightSquareBracket;
inline const QString HexPrefix = QStringLiteral("0x");
inline const QString BinPrefix = QStringLiteral("0b");
inline const QString OctPrefix = QStringLiteral("0o");
inline constexpr QChar HexPrefixAlt1 = UnicodeChars::NumberSign;
inline const QString TransOpAlt1 = QStringLiteral("in");
inline constexpr QChar PowerDigits[] = { Pow0, Pow1, Pow2, Pow3, Pow4, Pow5, Pow6, Pow7, Pow8, Pow9 };
inline constexpr QChar Digits[] = { Dig0, Dig1, Dig2, Dig3, Dig4, Dig5, Dig6, Dig7, Dig8, Dig9 };

inline bool isSuperscriptDigit(const QChar& ch)
{
    for (int i = 0; i < 10; ++i) {
        if (ch == PowerDigits[i])
            return true;
    }
    return false;
}

inline bool isSuperscriptSign(const QChar& ch)
{
    return ch == PowNeg || ch == PowPos;
}

inline bool isSuperscriptPowerChar(const QChar& ch)
{
    return isSuperscriptDigit(ch) || isSuperscriptSign(ch);
}

inline QChar superscriptDigitToAscii(const QChar& ch)
{
    for (int i = 0; i < 10; ++i) {
        if (ch == PowerDigits[i])
            return Digits[i];
    }
    return QChar();
}

inline QChar asciiDigitToSuperscript(const QChar& ch)
{
    const ushort code = ch.unicode();
    if (code < Dig0.unicode() || code > Dig9.unicode())
        return QChar();
    return PowerDigits[code - Dig0.unicode()];
}

} // namespace MathDsl

#endif // CORE_MATHDSL_H
