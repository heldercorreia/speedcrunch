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
#include <QRegularExpression>
#include <QString>

namespace UnicodeChars {

inline constexpr QChar Space(0x0020);
inline constexpr QChar NoBreakSpace(0x00A0);
inline constexpr QChar MediumMathematicalSpace(0x205F);
inline constexpr QChar NarrowNoBreakSpace(0x202F);
inline constexpr QChar PlusSign(0x002B);
inline constexpr QChar Solidus(0x002F);
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
inline constexpr QChar GreekCapitalOmega(0x03A9);
inline constexpr QChar OhmSign(0x2126);
inline constexpr QChar MicroSign(0x00B5);
inline constexpr QChar GreekSmallLetterMu(0x03BC);
inline constexpr QChar Pi(0x03C0);
inline constexpr QChar Summation(0x2211);
inline constexpr QChar SquareRoot(0x221A);
inline constexpr QChar CubeRoot(0x221B);
inline constexpr QChar RightwardsArrow(0x2192);
inline constexpr QChar Prime(0x2032);
inline constexpr QChar DoublePrime(0x2033);
inline constexpr QChar DegreeSign(0x00B0);
inline constexpr char32_t MathematicalBoldSmallPiCodePoint = 0x1D6D1;
inline constexpr char32_t MathematicalItalicSmallPiCodePoint = 0x1D70B;
inline constexpr char32_t MathematicalSansSerifBoldSmallPiCodePoint = 0x1D745;
inline constexpr char32_t MathematicalSansSerifBoldItalicSmallPiCodePoint = 0x1D7B9;

inline QString normalizePiAliasesToPi(QString text)
{
    const QString piSymbol(Pi);
    text.replace(QString::fromUcs4(&MathematicalBoldSmallPiCodePoint, 1), piSymbol);
    text.replace(QString::fromUcs4(&MathematicalItalicSmallPiCodePoint, 1), piSymbol);
    text.replace(QString::fromUcs4(&MathematicalSansSerifBoldSmallPiCodePoint, 1), piSymbol);
    text.replace(QString::fromUcs4(&MathematicalSansSerifBoldItalicSmallPiCodePoint, 1), piSymbol);
    return text;
}

inline QString normalizeUnitSymbolAliases(QString text)
{
    text.replace(OhmSign, GreekCapitalOmega);
    text.replace(GreekSmallLetterMu, MicroSign);
    text.replace(QChar(0x00BA), DegreeSign); // º -> °
    return text;
}

inline QString normalizeSexagesimalSymbolAliases(QString text)
{
    text.replace(QChar('\''), Prime);
    text.replace(QChar('"'), DoublePrime);
    return text;
}

inline QString normalizePiForDisplay(QString text)
{
    text = normalizePiAliasesToPi(text);

    auto isIdentifierChar = [](QChar ch) {
        return ch.isLetter() || ch.isDigit() || ch == QChar('_');
    };

    for (int i = 0; i + 1 < text.size();) {
        if (text.at(i) == QChar('p') && text.at(i + 1) == QChar('i')) {
            const bool hasLeft = i > 0;
            const bool hasRight = i + 2 < text.size();
            const bool leftIsIdentifier = hasLeft && isIdentifierChar(text.at(i - 1));
            const bool rightIsIdentifier = hasRight && isIdentifierChar(text.at(i + 2));
            if (!leftIsIdentifier && !rightIsIdentifier) {
                text.replace(i, 2, QString(Pi));
                ++i;
                continue;
            }
        }
        ++i;
    }

    return text;
}

inline QString normalizeRootFunctionAliasesForDisplay(QString text)
{
    static const QRegularExpression summationRE(
        QStringLiteral(R"((?<![\p{L}\p{N}_$])summation(?![\p{L}\p{N}_$]))"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression sqrtRE(
        QStringLiteral(R"((?<![\p{L}\p{N}_$])sqrt(?![\p{L}\p{N}_$]))"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression cbrtRE(
        QStringLiteral(R"((?<![\p{L}\p{N}_$])cbrt(?![\p{L}\p{N}_$]))"),
        QRegularExpression::CaseInsensitiveOption);

    text.replace(summationRE, QString(Summation));
    text.replace(sqrtRE, QString(SquareRoot));
    text.replace(cbrtRE, QString(CubeRoot));
    return text;
}

} // namespace UnicodeChars

#endif
