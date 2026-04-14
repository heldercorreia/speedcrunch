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

#ifndef DISPLAYFORMATUTILS_H
#define DISPLAYFORMATUTILS_H

#include <QString>

namespace DisplayFormatUtils {

/*
   Display spacing rules (single source of truth)
   ----------------------------------------------
   Operator sign constants come from MathDsl.

   1) Arithmetic expression operators
      - use MathDsl::AddWrap around AdditionSign
      - use MathDsl::SubtractionSpace around SubtractionSign
      - use MathDsl::DivWrap around DivisionSign
      - use MathDsl::MulDotWrap around MulDotSign
      - use MathDsl::MulCrossWrap around MulCrossSign

      Examples:
      - 2 + 3 + 4
      - 2 − 3 − 4
      - 2 / 3 / 4
      - 2 · cos(45) · sin(90)
      - 2 × 3 × 4

   2) Number + unit pair
      - Between numeric value and unit bracket use MathDsl::QuantitySpace.
      - Example: 2 [kg]

   3) Composite units
      - Inside unit expressions there are no surrounding spaces around unit
        multiplication/division operators, regardless of operator sign.
      - Example: [kg·m/s²]

   4) Conversion target display
      - Preserve explicit source RHS unit brackets for display, e.g.
        `3 [m] -> [km]` displays as `3 [m] → [km]`.
 */
QString applyDigitGroupingForDisplay(const QString& input);
QString applyOperatorSpacingForDisplay(const QString& input);
QString applyValueUnitSpacingForDisplay(const QString& input);
QString preserveConversionTargetBracketsForDisplay(const QString& displayedInterpreted,
                                                   const QString& sourceExpression);

}

#endif // DISPLAYFORMATUTILS_H
