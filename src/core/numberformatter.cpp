// This file is part of the SpeedCrunch project
// Copyright (C) 2013 @heldercorreia
// Copyright (C) 2015 Pol Welter <polwelter@gmail.com>
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

#include "core/numberformatter.h"

#include "core/regexpatterns.h"
#include "core/settings.h"
#include "core/unicodechars.h"
#include "math/quantity.h"
#include "math/rational.h"
#include "math/units.h"


static const QChar g_dotChar = UnicodeChars::DotOperator;
static const QChar g_minusChar = QString::fromUtf8("−")[0];

namespace {

constexpr int RationalFormatMaxDenominator = 1000000;
constexpr int TrigPiFormatMaxDenominator = 12;
HNumber rationalFormatRelativeTolerance()
{
    return HNumber("1e-60");
}

HNumber trigSymbolicRelativeTolerance()
{
    return HNumber("1e-30");
}

bool isCloseTo(const HNumber& value, const HNumber& reference)
{
    const HNumber diff = HMath::abs(value - reference);
    const HNumber scale = HMath::max(HMath::abs(value), HMath::abs(reference));
    const HNumber one(1);
    return diff <= trigSymbolicRelativeTolerance() * (scale < one ? one : scale);
}

QString formatFraction(const QString& numerator, int denominator)
{
    const QChar slash = QChar('/');
    const QString operatorSpace(UnicodeChars::MediumMathematicalSpace);
    return numerator + operatorSpace + slash + operatorSpace + QString::number(denominator);
}

QString formatPiMultiple(const HNumber& value)
{
    const HNumber pi = HMath::pi();
    if (HMath::abs(value) > HNumber(2) * pi)
        return QString();

    Rational ratio;
    if (!Rational::approximate(value / pi, TrigPiFormatMaxDenominator,
            trigSymbolicRelativeTolerance(), &ratio)) {
        return QString();
    }

    const int numerator = ratio.numerator();
    const int denominator = ratio.denominator();
    if (numerator == 0)
        return QString();
    if (qAbs(numerator) > TrigPiFormatMaxDenominator)
        return QString();

    if (denominator != 1
            && denominator != 2
            && denominator != 3
            && denominator != 4
            && denominator != 6
            && denominator != 8
            && denominator != 12) {
        return QString();
    }

    const HNumber expected = pi * HNumber(numerator) / HNumber(denominator);
    if (!isCloseTo(value, expected))
        return QString();

    if (denominator == 1) {
        if (numerator == 1)
            return QStringLiteral("pi");
        if (numerator == -1)
            return QStringLiteral("-pi");
        return QString::number(numerator) + UnicodeChars::DotOperator + QStringLiteral("pi");
    }

    QString numeratorText;
    if (numerator == 1)
        numeratorText = QStringLiteral("pi");
    else if (numerator == -1)
        numeratorText = QStringLiteral("-pi");
    else
        numeratorText = QString::number(numerator) + UnicodeChars::DotOperator + QStringLiteral("pi");

    return formatFraction(numeratorText, denominator);
}

QString formatCommonTrigRadical(const HNumber& value)
{
    const bool negative = value.isNegative();
    const HNumber absValue = negative ? -value : value;

    const HNumber sqrt2 = HMath::sqrt(HNumber(2));
    const HNumber sqrt3 = HMath::sqrt(HNumber(3));

    QString symbol;
    if (isCloseTo(absValue, sqrt2 / HNumber(2)))
        symbol = formatFraction(QStringLiteral("sqrt(2)"), 2);
    else if (isCloseTo(absValue, sqrt3 / HNumber(2)))
        symbol = formatFraction(QStringLiteral("sqrt(3)"), 2);
    else if (isCloseTo(absValue, sqrt3 / HNumber(3)))
        symbol = formatFraction(QStringLiteral("sqrt(3)"), 3);
    else if (isCloseTo(absValue, HNumber(2) * sqrt3 / HNumber(3)))
        symbol = formatFraction(QStringLiteral("2 sqrt(3)"), 3);
    else if (isCloseTo(absValue, sqrt2))
        symbol = QStringLiteral("sqrt(2)");
    else if (isCloseTo(absValue, sqrt3))
        symbol = QStringLiteral("sqrt(3)");

    if (symbol.isEmpty())
        return QString();
    return negative ? QStringLiteral("-") + symbol : symbol;
}

QString formatTrigSymbolicDisplay(const Quantity& q, const HNumber& value)
{
    if (!q.isDimensionless() || !q.unitName().isEmpty())
        return QString();

    QString symbolic = formatPiMultiple(value);
    if (!symbolic.isEmpty())
        return symbolic;

    return formatCommonTrigRadical(value);
}

QString formatRationalDisplay(Quantity q)
{
    if (q.isNan())
        return QString();

    if (!q.hasUnit() && !q.isDimensionless()) {
        q.cleanDimension();
        Units::findUnit(q);
    }

    CNumber number = q.numericValue();
    number /= q.unit();
    if (!number.isNearReal())
        return QString();

    QString symbolicTrig = formatTrigSymbolicDisplay(q, number.real);
    if (!symbolicTrig.isEmpty())
        return symbolicTrig;

    Rational rational;
    if (!Rational::approximate(number.real, RationalFormatMaxDenominator,
            rationalFormatRelativeTolerance(), &rational)) {
        return QString();
    }

    QString result;
    if (rational.denominator() == 1) {
        result = QString::number(rational.numerator());
    } else {
        result = formatFraction(QString::number(rational.numerator()), rational.denominator());
    }

    const QString unitName = q.unitName();
    if (!unitName.isEmpty())
        result += QStringLiteral(" ") + unitName;
    return result;
}

} // namespace

QString NumberFormatter::format(Quantity q)
{
    return format(q, '\0');
}

QString NumberFormatter::formatTrigSymbolic(Quantity q)
{
    if (q.isNan())
        return QString();

    CNumber number = q.numericValue();
    number /= q.unit();
    if (!number.isNearReal())
        return QString();

    return formatTrigSymbolicDisplay(q, number.real);
}

QString NumberFormatter::format(Quantity q, char resultFormatOverride)
{
    Settings* settings = Settings::instance();
    const char activeResultFormat = resultFormatOverride == '\0' ? settings->resultFormat : resultFormatOverride;
    QString result;

    Quantity::Format format = q.format();
    if (activeResultFormat == 'r'
            && format.base != Quantity::Format::Base::Binary
            && format.base != Quantity::Format::Base::Octal
            && format.base != Quantity::Format::Base::Hexadecimal
            && format.mode != Quantity::Format::Mode::Sexagesimal) {
        result = formatRationalDisplay(q);
    }

    if (format.base == Quantity::Format::Base::Null) {
        switch (activeResultFormat) {
        case 'b':
            format.base = Quantity::Format::Base::Binary;
            break;
        case 'o':
            format.base = Quantity::Format::Base::Octal;
            break;
        case 'h':
            format.base = Quantity::Format::Base::Hexadecimal;
            break;
        case 'n':
        case 'f':
        case 'e':
        case 'r':
        case 's':
        case 'g':
        default:
            format.base = Quantity::Format::Base::Decimal;
            break;
        }
    }

    if (format.mode == Quantity::Format::Mode::Null) {
        if (format.base == Quantity::Format::Base::Decimal) {
          switch (activeResultFormat) {
          case 'n':
              format.mode = Quantity::Format::Mode::Engineering;
              break;
          case 'f':
              format.mode = Quantity::Format::Mode::Fixed;
              break;
          case 'e':
              format.mode = Quantity::Format::Mode::Scientific;
              break;
          case 's':
              format.mode = Quantity::Format::Mode::Sexagesimal;
              break;
          case 'r':
          case 'g':
          default:
              format.mode = Quantity::Format::Mode::General;
              break;
          }
        } else {
            format.mode = Quantity::Format::Mode::Fixed;
        }
    }

    if (format.precision == Quantity::Format::PrecisionNull)
        format.precision = settings->resultPrecision;
    if (format.notation == Quantity::Format::Notation::Null) {
        if (settings->resultFormatComplex == 'c')
            format.notation = Quantity::Format::Notation::Cartesian;
        else if (settings->resultFormatComplex == 'p')
            format.notation = Quantity::Format::Notation::Polar;
    }

    bool time = false, arc = q.isDimensionless();
    if (activeResultFormat == 's' && q.hasDimension()) {
        auto dimension = q.getDimension();
        if (dimension.count() == 1 && dimension.firstKey() == "time") {
            auto iterator = dimension.begin();
            if ( iterator->numerator() == 1 && iterator->denominator() == 1) {
                q.clearDimension(); // remove unit, formatting itself is unit
                time = true;
            }
        }
    }

    if (arc && activeResultFormat == 's') {     // convert to arcseconds
        if (settings->angleUnit == 'r')
            q /= Quantity(HMath::pi() / HNumber(180));
        else if (settings->angleUnit == 'g')
            q /= Quantity(HNumber(200) / HNumber(180));
        else if (settings->angleUnit == 't')
            q *= Quantity(360);
        q *= Quantity(3600);
    }

    bool negative = false;
    if (activeResultFormat == 's' && q.isNegative()) {
        q *= Quantity(HNumber(-1));
        negative = true;
    }

    if (result.isEmpty())
        result = DMath::format(q, format);

    if (activeResultFormat == 's' && (arc || time)) {   // sexagesimal
        int dotPos = result.indexOf('.');
        HNumber seconds(dotPos > 0 ? result.left(dotPos).toStdString().c_str() : result.toStdString().c_str());
        HNumber mains = HMath::floor(seconds / HNumber(3600));
        seconds -= (mains * HNumber(3600));
        HNumber minutes = HMath::floor(seconds / HNumber(60));
        seconds -= (minutes * HNumber(60));
        HNumber::Format fixed = HNumber::Format::Fixed();
        QString sexa = HMath::format(mains, fixed);
        sexa.append(time ? QChar(':') : QChar(0xB0)).append(minutes < 10 ? "0" : "").append(HMath::format(minutes, fixed));
        sexa.append(time ? ':' : '\'').append(seconds < 10 ? "0" : "").append(HMath::format(seconds, fixed));
        if (dotPos > 0)     // append decimals
            sexa.append(result.mid(dotPos));
        result = sexa;
    }

    if (negative)
        result.insert(0, '-');

    if (settings->radixCharacter() == ',')
        result.replace('.', ',');

    result.replace('-', g_minusChar);

    // Replace all spaces between units with dot operator.
    int emptySpaces = 0;
    for (auto& ch : result) {
        if (ch.isSpace()) {
            ++emptySpaces;
            if (emptySpaces > 1)
                ch = g_dotChar;
        } else {
            emptySpaces = 0;
        }
    }

    return result;
}

QString NumberFormatter::formatNumericLiteralForDisplay(const QString& input)
{
    const Settings* settings = Settings::instance();
    if (settings->digitGrouping <= 0)
        return input;

    // Digit Grouping menu mapping:
    // Small Space  -> U+0020 once
    // Medium Space -> U+0020 twice
    // Large Space  -> U+0020 three times
    const QString separator = QStringLiteral(" ").repeated(settings->digitGrouping);
    auto groupPart = [&separator](const QString& digits, int groupSize, bool fromRight) {
        if (digits.size() <= groupSize)
            return digits;

        QString result;
        if (fromRight) {
            int first = digits.size() % groupSize;
            if (first == 0)
                first = groupSize;
            result = digits.left(first);
            for (int i = first; i < digits.size(); i += groupSize) {
                result += separator;
                result += digits.mid(i, groupSize);
            }
            return result;
        }

        for (int i = 0; i < digits.size(); i += groupSize) {
            if (i > 0)
                result += separator;
            result += digits.mid(i, groupSize);
        }
        return result;
    };

    QString output;
    int lastPos = 0;
    auto it = RegExpPatterns::numericToken().globalMatch(input);
    while (it.hasNext()) {
        const auto match = it.next();
        output += input.mid(lastPos, match.capturedStart() - lastPos);

        QString token = match.captured(0);
        int prefixLength = 0;
        int groupSize = 3;
        if (token.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
            prefixLength = 2;
            groupSize = 4;
        } else if (token.startsWith(QLatin1String("0o"), Qt::CaseInsensitive)) {
            prefixLength = 2;
            groupSize = 3;
        } else if (token.startsWith(QLatin1String("0b"), Qt::CaseInsensitive)) {
            prefixLength = 2;
            groupSize = 4;
        }

        const QString prefix = token.left(prefixLength);
        QString body = token.mid(prefixLength);
        QString exponent;
        if (prefixLength == 0) {
            const int exponentPos = body.indexOf(RegExpPatterns::decimalExponentSuffix());
            if (exponentPos >= 0) {
                exponent = body.mid(exponentPos);
                body = body.left(exponentPos);
            }
        }

        const int radixPos = body.indexOf(RegExpPatterns::radixSeparator());
        if (radixPos >= 0) {
            const QString integral = body.left(radixPos);
            const QString fractional = body.mid(radixPos + 1);
            const QString groupedFractional = settings->digitGroupingIntegerPartOnly
                ? fractional
                : groupPart(fractional, groupSize, false);
            token = prefix
                + groupPart(integral, groupSize, true)
                + body.at(radixPos)
                + groupedFractional
                + exponent;
        } else {
            token = prefix + groupPart(body, groupSize, true) + exponent;
        }

        output += token;
        lastPos = match.capturedEnd();
    }
    output += input.mid(lastPos);
    return output;
}
