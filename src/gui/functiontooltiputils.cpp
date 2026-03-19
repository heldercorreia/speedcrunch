// This file is part of the SpeedCrunch project
// Copyright (C) 2026 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "gui/functiontooltiputils.h"
#include "core/evaluator.h"
#include "core/functions.h"
#include "core/userfunction.h"

#include <QVector>

namespace {

struct ActiveFunctionCallContext {
    QString functionName;
    int argumentIndex;
};

bool activeFunctionCallContext(const Evaluator* evaluator,
                               const QString& expression,
                               int cursorPosition,
                               ActiveFunctionCallContext* context)
{
    if (!context)
        return false;

    const int safeCursorPosition = qBound(0, cursorPosition, expression.length());
    const Tokens tokens = evaluator->scan(expression.left(safeCursorPosition));
    if (!tokens.valid() || tokens.isEmpty())
        return false;

    struct Scope {
        QString functionName;
        int argumentIndex;
    };
    QVector<Scope> scopes;

    Token previous = Token::null;
    bool hasPrevious = false;

    for (int i = 0; i < tokens.count(); ++i) {
        const Token token = tokens.at(i);
        const Token::Operator op = token.asOperator();

        if (op == Token::AssociationStart) {
            Scope scope;
            scope.argumentIndex = 0;
            if (hasPrevious && previous.isIdentifier())
                scope.functionName = previous.text();
            scopes.append(scope);
        } else if (op == Token::AssociationEnd) {
            if (!scopes.isEmpty())
                scopes.removeLast();
        } else if (op == Token::ListSeparator) {
            if (!scopes.isEmpty() && !scopes.last().functionName.isEmpty())
                ++scopes.last().argumentIndex;
        }

        previous = token;
        hasPrevious = true;
    }

    for (int i = scopes.count() - 1; i >= 0; --i) {
        if (!scopes.at(i).functionName.isEmpty()) {
            context->functionName = scopes.at(i).functionName;
            context->argumentIndex = scopes.at(i).argumentIndex;
            return true;
        }
    }

    return false;
}

QString formatFunctionUsageTooltip(const QString& functionName,
                                   const QStringList& parameters,
                                   int argumentIndex,
                                   bool escapeParameters)
{
    QStringList displayParameters;
    displayParameters.reserve(parameters.count());

    for (int i = 0; i < parameters.count(); ++i) {
        QString parameter = escapeParameters
            ? parameters.at(i).toHtmlEscaped()
            : parameters.at(i);
        if (i == argumentIndex) {
            int leading = 0;
            while (leading < parameter.size() && parameter.at(leading).isSpace())
                ++leading;
            int trailing = 0;
            while (trailing < parameter.size() - leading
                   && parameter.at(parameter.size() - 1 - trailing).isSpace()) {
                ++trailing;
            }
            const QString prefix = parameter.left(leading);
            const QString core = parameter.mid(
                leading, parameter.size() - leading - trailing);
            const QString suffix = parameter.right(trailing);
            parameter = prefix + QStringLiteral("<b>%1</b>").arg(core) + suffix;
        }
        displayParameters.append(parameter);
    }

    const QString escapedFunctionName = functionName.toHtmlEscaped();
    return QStringLiteral("<b>%1</b>(%2)")
        .arg(escapedFunctionName, displayParameters.join(";"));
}

} // namespace

namespace FunctionTooltipUtils {

QString activeFunctionUsageTooltip(const Evaluator* evaluator,
                                   const QString& expression,
                                   int cursorPosition)
{
    ActiveFunctionCallContext context;
    if (!activeFunctionCallContext(evaluator, expression, cursorPosition, &context))
        return QString();

    if (Function* function = FunctionRepo::instance()->find(context.functionName)) {
        const QString usage = function->usage();
        if (usage.isEmpty())
            return QStringLiteral("<b>%1</b>()").arg(context.functionName.toHtmlEscaped());

        return formatFunctionUsageTooltip(
            context.functionName,
            usage.split(';'),
            context.argumentIndex,
            false
        );
    }

    const auto userFunctions = evaluator->getUserFunctions();
    for (const UserFunction& userFunction : userFunctions) {
        if (userFunction.name().compare(context.functionName, Qt::CaseInsensitive) != 0)
            continue;

        return formatFunctionUsageTooltip(
            userFunction.name(),
            userFunction.arguments(),
            context.argumentIndex,
            true
        );
    }

    return QString();
}

} // namespace FunctionTooltipUtils
