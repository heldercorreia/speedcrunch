// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_SYMBOLICNUMBERFORMAT_H
#define CORE_SYMBOLICNUMBERFORMAT_H

#include "core/mathdsl.h"
#include "math/hmath.h"
#include "math/rational.h"

#include <QtGlobal>
#include <QString>

namespace SymbolicNumberFormat {

inline HNumber trigSymbolicRelativeTolerance()
{
    return HNumber("1e-30");
}

inline bool isCloseToTrigSymbolicValue(const HNumber& value, const HNumber& reference)
{
    const HNumber diff = HMath::abs(value - reference);
    const HNumber scale = HMath::max(HMath::abs(value), HMath::abs(reference));
    const HNumber one(1);
    return diff <= trigSymbolicRelativeTolerance() * (scale < one ? one : scale);
}

inline QString formatFraction(const QString& numerator, int denominator)
{
    const QString operatorSpace(MathDsl::DivWrap);
    return numerator + operatorSpace + MathDsl::DivOp
           + operatorSpace + QString::number(denominator);
}

inline QString formatConstantMultiple(const HNumber& value, const HNumber& factor,
                                      const QString& symbol, const HNumber& tolerance,
                                      int maxDenominator)
{
    Rational ratio;
    if (!Rational::approximate(value / factor, maxDenominator, tolerance, &ratio))
        return QString();

    const int numerator = ratio.numerator();
    const int denominator = ratio.denominator();
    if (numerator == 0 || denominator <= 0)
        return QString();

    const HNumber expected = factor * HNumber(numerator) / HNumber(denominator);
    const HNumber diff = HMath::abs(value - expected);
    const HNumber scale = HMath::max(HMath::max(HMath::abs(value), HMath::abs(expected)), HNumber(1));
    if (diff > tolerance * scale)
        return QString();

    const QString division = QString(MathDsl::DivWrap) + QString(MathDsl::DivOp) + QString(MathDsl::DivWrap);
    const QString multiplication = QString(MathDsl::MulDotWrapSp) + QString(MathDsl::MulDotOp)
        + QString(MathDsl::MulDotWrapSp);
    const QString sign = (numerator < 0) ? QString(MathDsl::SubOp) : QString();
    const int absNumerator = qAbs(numerator);

    if (denominator == 1) {
        if (absNumerator == 1)
            return sign + symbol;
        return sign + QString::number(absNumerator) + multiplication + symbol;
    }

    if (absNumerator == 1)
        return sign + symbol + division + QString::number(denominator);

    return sign
        + QString::number(absNumerator)
        + division
        + QString::number(denominator)
        + multiplication
        + symbol;
}

inline QString formatPiMultiple(const HNumber& value)
{
    constexpr int maxDenominator = 12;
    const HNumber pi = HMath::pi();
    if (HMath::abs(value) > HNumber(2) * pi)
        return QString();

    Rational ratio;
    if (!Rational::approximate(value / pi, maxDenominator,
            trigSymbolicRelativeTolerance(), &ratio)) {
        return QString();
    }

    const int numerator = ratio.numerator();
    const int denominator = ratio.denominator();
    if (numerator == 0)
        return QString();
    if (qAbs(numerator) > maxDenominator)
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
    if (!isCloseToTrigSymbolicValue(value, expected))
        return QString();

    const QString mulSpace(MathDsl::MulDotWrapSp);
    if (denominator == 1) {
        if (numerator == 1)
            return QStringLiteral("pi");
        if (numerator == -1)
            return QStringLiteral("-pi");
        return QString::number(numerator) + mulSpace + MathDsl::MulDotOp
               + mulSpace + QStringLiteral("pi");
    }

    QString numeratorText;
    if (numerator == 1)
        numeratorText = QStringLiteral("pi");
    else if (numerator == -1)
        numeratorText = QStringLiteral("-pi");
    else
        numeratorText = QString::number(numerator) + mulSpace + MathDsl::MulDotOp
                        + mulSpace + QStringLiteral("pi");

    return formatFraction(numeratorText, denominator);
}

}

#endif // CORE_SYMBOLICNUMBERFORMAT_H
