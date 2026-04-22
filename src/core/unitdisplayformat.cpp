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

#include "core/unitdisplayformat.h"

#include "core/mathdsl.h"
#include "core/unicodechars.h"
#include "core/units.h"
#include <QHash>

namespace UnitDisplayFormat {

namespace {

// Returns the index where a trailing superscript exponent suffix starts.
// Supports ¹²³ and U+2070..U+2079 digits plus superscript plus/minus.
int superscriptSuffixStart(const QString& text)
{
    int suffixStart = text.size();
    while (suffixStart > 0) {
        const QChar ch = text.at(suffixStart - 1);
        if (!UnicodeChars::isSuperscriptDigit(ch) && !UnicodeChars::isSuperscriptSign(ch))
            break;
        --suffixStart;
    }
    return suffixStart;
}

// Builds an identifier->symbol cache from unit metadata, used to collapse
// long names (and aliases) to short display symbols.
const QHash<QString, QString>& shortNamesByIdentifier()
{
    static const QHash<QString, QString> shortNames = [] {
        QHash<QString, QString> map;
        const auto& values = Units::builtInUnitValues();
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        const QString& identifier = it.key();
        UnitId id = unitId(normalizeUnitName(identifier));
        if (id == UnitId::Unknown)
            continue;
            const QString symbol = unitSymbol(id);
            if (!symbol.isEmpty())
                map.insert(identifier, symbol);
        }
        return map;
    }();
    return shortNames;
}

} // namespace

// Collapses a unit token to a compact display form:
// canonical symbol when known, preserving superscript exponent suffixes.
// Examples:
// - "metre" -> "m"
// - "kilometre" -> "km"
// - "quebibyte" -> "QiB"
// - "metre⁻²" -> "m⁻²"
QString shortDisplayName(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return trimmed;

    const int suffixStart = superscriptSuffixStart(trimmed);
    const QString baseName = trimmed.left(suffixStart);
    const QString exponentSuffix = trimmed.mid(suffixStart);

    const auto& shortNames = shortNamesByIdentifier();
    if (shortNames.contains(baseName))
        return shortNames.value(baseName) + exponentSuffix;

    for (const PrefixId id : siPrefixIds()) {
        const QString prefixLong = prefixName(id);
        if (!baseName.startsWith(prefixLong))
            continue;
        const QString base = baseName.mid(prefixLong.size());
        if (shortNames.contains(base))
            return prefixSymbol(id) + shortNames.value(base) + exponentSuffix;
    }

    for (const PrefixId id : binaryPrefixIds()) {
        const QString prefixLong = prefixName(id);
        if (!baseName.startsWith(prefixLong))
            continue;
        const QString base = baseName.mid(prefixLong.size());
        if (shortNames.contains(base))
            return prefixSymbol(id) + shortNames.value(base) + exponentSuffix;
    }

    return trimmed;
}

// Normalizes a full unit-expression string for display by tokenizing around
// separators/operators and applying token-level formatting.
// Examples:
// - "metre second⁻¹" -> "m s⁻¹"
// - "joule/(mole kelvin)" -> "J/(mol K)"
// - "kilogram·metre/second²" -> "kg·m/s²"
QString normalizeUnitTextForDisplay(const QString& text)
{
    if (text.isEmpty())
        return text;

    const bool hasCompositeUnitOperators =
        text.contains(MathDsl::MulOpAl1)
        || text.contains(MathDsl::MulDotOp)
        || text.contains(MathDsl::MulCrossOp)
        || text.contains(MathDsl::DivOp);
    const QString degreeAlias = Units::degreeAliasSymbol();

    QString normalized;
    normalized.reserve(text.size());

    for (int i = 0; i < text.size();) {
        const QChar ch = text.at(i);
        if (!UnicodeChars::isUnitIdentifierChar(ch)) {
            normalized.append(ch);
            ++i;
            continue;
        }

        int j = i + 1;
        while (j < text.size() && UnicodeChars::isUnitIdentifierChar(text.at(j)))
            ++j;

        const QString token = text.mid(i, j - i);
        QString displayToken = shortDisplayName(token);
        if (hasCompositeUnitOperators
            && unitId(normalizeUnitName(token)) == UnitId::Degree)
        {
            QString suffix;
            if (!displayToken.isEmpty()
                && displayToken.at(0) == UnicodeChars::DegreeSign)
            {
                suffix = displayToken.mid(1);
            } else if (displayToken.startsWith(degreeAlias)) {
                suffix = displayToken.mid(degreeAlias.size());
            }
            displayToken = degreeAlias + suffix;
        }
        normalized.append(displayToken);
        i = j;
    }
    return normalized;
}

} // namespace UnitDisplayFormat
