// This file is part of the SpeedCrunch project
// Copyright (C) 2026 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef GUI_RESULTLINEFORMATUTILS_H
#define GUI_RESULTLINEFORMATUTILS_H

#include "core/evaluator.h"
#include "core/mathdsl.h"
#include "core/regexpatterns.h"
#include "core/settings.h"
#include "core/unicodechars.h"
#include "core/units.h"
#include "gui/displayformatutils.h"
#include "gui/simplifiedexpressionutils.h"

#include <QRegularExpression>
#include <QString>
#include <QVector>

namespace ResultLineFormatUtils {

inline bool containsExplicitBracketedAngleUnit(const QString& expression)
{
    QRegularExpressionMatchIterator matches =
        RegExpPatterns::unitBrackets().globalMatch(expression);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        if (Units::isExplicitAngleUnitName(match.captured(1)))
            return true;
    }
    return false;
}

inline bool containsExplicitSexagesimalAngleMarkers(const QString& expression)
{
    for (const QChar ch : expression) {
        if (ch == UnicodeChars::DegreeSign
            || ch == UnicodeChars::MasculineOrdinalIndicator
            || ch == UnicodeChars::RingOperator
            || ch == UnicodeChars::Prime
            || ch == UnicodeChars::DoublePrime
            || ch == UnicodeChars::Apostrophe
            || ch == UnicodeChars::QuotationMark) {
            return true;
        }
    }
    return false;
}

inline QString foldRepeatedAdditiveTermsForDisplay(const QString& text)
{
    if (text.isEmpty())
        return text;

    const Tokens tokens = Evaluator::instance()->scan(text);
    if (!tokens.valid() || tokens.isEmpty())
        return text;

    QVector<int> splitOps;
    int depth = 0;
    for (int i = 0; i < tokens.size(); ++i) {
        const Token::Operator op = tokens.at(i).asOperator();
        if (op == Token::AssociationStart) {
            ++depth;
            continue;
        }
        if (op == Token::AssociationEnd) {
            if (depth > 0)
                --depth;
            continue;
        }
        if (depth == 0 && (op == Token::Addition || op == Token::Subtraction))
            splitOps.append(i);
    }
    if (splitOps.isEmpty())
        return text;

    auto tokenRangeText = [&text, &tokens](int startIndex, int endIndex) {
        const int startPos = tokens.at(startIndex).pos();
        const int endPos = tokens.at(endIndex).pos() + tokens.at(endIndex).size();
        return text.mid(startPos, endPos - startPos);
    };
    auto isMinusChar = [](const QChar& ch) {
        return ch == MathDsl::SubOp || ch == MathDsl::SubOpAl1;
    };
    auto formatCoeff = [](double value) {
        return QString::number(value, 'g', 15);
    };
    auto normalizeKey = [](QString s) {
        s.remove(QRegularExpression(QStringLiteral("\\s+")));
        return s;
    };

    QHash<QString, double> coeffByKey;
    QHash<QString, QString> displayByKey;
    QVector<QString> keyOrder;

    int termStart = 0;
    for (int part = 0; part <= splitOps.size(); ++part) {
        const int opIndex = (part < splitOps.size()) ? splitOps.at(part) : -1;
        const int termEnd = (opIndex >= 0) ? (opIndex - 1) : (tokens.size() - 1);
        if (termEnd < termStart) {
            termStart = opIndex + 1;
            continue;
        }

        const QString opText = (part == 0) ? QString(MathDsl::AddOp)
            : tokenRangeText(splitOps.at(part - 1), splitOps.at(part - 1));
        QString termText = tokenRangeText(termStart, termEnd).trimmed();
        termStart = opIndex + 1;
        if (termText.isEmpty())
            continue;

        bool negFromTerm = false;
        while (!termText.isEmpty() && (termText.at(0) == MathDsl::AddOp || isMinusChar(termText.at(0)))) {
            if (isMinusChar(termText.at(0)))
                negFromTerm = !negFromTerm;
            termText.remove(0, 1);
            termText = termText.trimmed();
        }
        if (termText.isEmpty())
            continue;

        bool ok = false;
        double numericFactor = 1.0;
        QString base = termText;
        const Tokens termTokens = Evaluator::instance()->scan(termText);
        if (termTokens.valid()
            && termTokens.size() >= 3
            && termTokens.at(0).isNumber()
            && termTokens.at(1).asOperator() == Token::Multiplication) {
            numericFactor = termTokens.at(0).text().toDouble(&ok);
            if (ok && numericFactor != 0.0) {
                const int baseStart = termTokens.at(2).pos();
                const int baseEnd = termTokens.at(termTokens.size() - 1).pos()
                    + termTokens.at(termTokens.size() - 1).size();
                if (baseStart >= 0 && baseEnd > baseStart && baseEnd <= termText.size())
                    base = termText.mid(baseStart, baseEnd - baseStart).trimmed();
            } else {
                ok = false;
            }
        } else {
            ok = true;
        }
        if (!ok || base.isEmpty())
            continue;

        const bool minusOp = (opText.size() == 1 && isMinusChar(opText.at(0)));
        const double signedFactor = (minusOp ? -1.0 : 1.0) * (negFromTerm ? -numericFactor : numericFactor);
        const QString key = normalizeKey(base);
        if (!coeffByKey.contains(key)) {
            keyOrder.append(key);
            displayByKey.insert(key, base);
        }
        coeffByKey[key] += signedFactor;
    }

    QString rebuilt;
    for (const QString& key : keyOrder) {
        const double coeff = coeffByKey.value(key, 0.0);
        if (std::abs(coeff) < 1e-12)
            continue;
        const QString base = displayByKey.value(key, key);
        const bool negative = coeff < 0.0;
        const double magnitude = std::abs(coeff);

        QString term = base;
        if (std::abs(magnitude - 1.0) >= 1e-12)
            term = formatCoeff(magnitude) + QString(MathDsl::MulDotOp) + base;

        if (rebuilt.isEmpty()) {
            rebuilt = negative ? (QString(MathDsl::SubOpAl1) + term) : term;
        } else {
            rebuilt += negative ? QString(MathDsl::SubOpAl1) : QString(MathDsl::AddOp);
            rebuilt += term;
        }
    }

    return rebuilt.isEmpty() ? text : rebuilt;
}

inline QString simplifiedExpressionLineForDisplay(const QString& interpretedExpression,
                                                  const QString& sourceExpression,
                                                  bool simplifyResultExpressions)
{
    if (!simplifyResultExpressions)
        return QString();

    QString expressionForSimplification = interpretedExpression;
    if (expressionForSimplification.isEmpty()) {
        const bool sourceHasExplicitAngles =
            containsExplicitBracketedAngleUnit(sourceExpression)
            || containsExplicitSexagesimalAngleMarkers(sourceExpression);
        if (sourceHasExplicitAngles)
            expressionForSimplification = sourceExpression;
    }
    if (expressionForSimplification.isEmpty())
        return QString();

    QString interpretedDisplay = DisplayFormatUtils::applyDigitGroupingForDisplay(
        Evaluator::formatInterpretedExpressionForDisplay(expressionForSimplification));
    QString simplifiedDisplay = DisplayFormatUtils::applyDigitGroupingForDisplay(
        Evaluator::formatInterpretedExpressionSimplifiedForDisplay(expressionForSimplification));
    if (!sourceExpression.isEmpty()) {
        interpretedDisplay = DisplayFormatUtils::preserveConversionTargetBracketsForDisplay(
            interpretedDisplay, sourceExpression);
        simplifiedDisplay = DisplayFormatUtils::preserveConversionTargetBracketsForDisplay(
            simplifiedDisplay, sourceExpression);
    }
    if (simplifiedDisplay.isEmpty() || simplifiedDisplay == interpretedDisplay)
        return QString();

    if (SimplifiedExpressionUtils::shouldSuppressSimplifiedExpressionLine(
            interpretedDisplay, simplifiedDisplay))
        return QString();

    return foldRepeatedAdditiveTermsForDisplay(simplifiedDisplay);
}

} // namespace ResultLineFormatUtils

#endif
