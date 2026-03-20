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

#ifndef CORE_UNICODECHARS_H
#define CORE_UNICODECHARS_H

#include <QChar>

namespace UnicodeChars {

inline constexpr QChar MediumMathematicalSpace(0x205F);
inline constexpr QChar MultiplicationSign(0x00D7);
inline constexpr QChar MinusSign(0x2212);
inline constexpr QChar DotOperator(0x22C5);
inline constexpr QChar Asterisk(0x002A);
inline constexpr QChar MiddleDot(0x00B7);
inline constexpr QChar AsteriskOperator(0x2217);
inline constexpr QChar BulletOperator(0x2219);
inline constexpr QChar MultiplicationX(0x2715);
inline constexpr QChar HeavyMultiplicationX(0x2716);
inline constexpr QChar NAryTimesOperator(0x2A09);
inline constexpr QChar VectorOrCrossProduct(0x2A2F);

} // namespace UnicodeChars

#endif
