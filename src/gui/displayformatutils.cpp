// This file is part of the SpeedCrunch project
// Copyright (C) 2007 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2004, 2005 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2005, 2006 Johan Thelin <e8johan@gmail.com>
// Copyright (C) 2007-2026 @heldercorreia
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

#include "gui/displayformatutils.h"

#include "core/numberformatter.h"
#include "core/regexpatterns.h"
#include "core/unicodechars.h"

namespace DisplayFormatUtils {

QString applyDigitGroupingForDisplay(const QString& input)
{
    // Even when grouping is disabled we still pass numeric tokens through
    // NumberFormatter::formatNumericLiteralForDisplay(), because that helper
    // also normalizes decimal separator display according to the selected
    // number-format style (for example dot-internal -> comma-display).
    // Grouping can be a no-op there while decimal-separator conversion still
    // needs to happen.
    QString output;
    int lastPos = 0;
    auto it = RegExpPatterns::numericToken().globalMatch(input);
    while (it.hasNext()) {
        const auto match = it.next();
        output += input.mid(lastPos, match.capturedStart() - lastPos);
        output += NumberFormatter::formatNumericLiteralForDisplay(match.captured(0));
        lastPos = match.capturedEnd();
    }
    output += input.mid(lastPos);
    output = UnicodeChars::normalizePiForDisplay(output);
    return UnicodeChars::normalizeRootFunctionAliasesForDisplay(output);
}

} // namespace DisplayFormatUtils
