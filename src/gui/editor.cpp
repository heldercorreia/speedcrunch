// This file is part of the SpeedCrunch project
// Copyright (C) 2007 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2004, 2005 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2005, 2006 Johan Thelin <e8johan@gmail.com>
// Copyright (C) 2007-2016 @heldercorreia
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

#include "gui/editor.h"
#include "gui/displayformatutils.h"
#include "gui/editorutils.h"
#include "gui/functiontooltiputils.h"
#include "gui/simplifiedexpressionutils.h"
#include "gui/syntaxhighlighter.h"
#include "core/constants.h"
#include "core/evaluator.h"
#include "core/functions.h"
#include "core/numberformatter.h"
#include "core/regexpatterns.h"
#include "core/settings.h"
#include "core/session.h"
#include "core/unicodechars.h"
#include "math/units.h"

#include <QApplication>
#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScreen>
#include <QScrollBar>
#include <QRegularExpression>
#include <QStyle>
#include <QResizeEvent>
#include <QTimeLine>
#include <QTimer>
#include <QTextBlock>
#include <QTextLayout>
#include <QTreeWidget>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

static void moveCursorToEnd(Editor* editor)
{
    QTextCursor cursor = editor->textCursor();
    cursor.movePosition(QTextCursor::EndOfBlock);
    editor->setTextCursor(cursor);
}

static bool isOperatorOnlyIncompleteInput(const QString& expression)
{
    const QString trimmed = expression.trimmed();
    if (trimmed.isEmpty())
        return false;

    bool sawOperator = false;
    for (int i = 0; i < trimmed.size(); ++i) {
        const QChar ch = trimmed.at(i);
        if (ch.isSpace())
            continue;
        if (ch == QLatin1Char('+') || EditorUtils::isSubtractionOperatorAlias(ch)) {
            sawOperator = true;
            continue;
        }
        return false;
    }

    return sawOperator;
}

static QString normalizeExpressionTypedInEditor(QString text)
{
    return EditorUtils::normalizeExpressionOperatorsForEditorInput(text);
}

static QString formattedLiveResult(const Quantity& quantity, char resultFormat = '\0')
{
    return DisplayFormatUtils::applyDigitGroupingForDisplay(
        NumberFormatter::format(quantity, resultFormat));
}

static QString formattedLiveResultForSlot(const Quantity& quantity, char resultFormat,
                                          int precision, bool complexEnabled, char complexForm)
{
    return DisplayFormatUtils::applyDigitGroupingForDisplay(
        NumberFormatter::format(quantity, resultFormat, precision, complexEnabled, complexForm));
}

static bool shouldShowAdditionalRationalForTrig(const QString& expression,
                                                const QString& interpretedExpression,
                                                const Quantity& quantity)
{
    const bool usesTrigInExpression = RegExpPatterns::trigFunctionCall().match(expression).hasMatch();
    const bool usesTrigInInterpreted =
        RegExpPatterns::trigFunctionCall().match(interpretedExpression).hasMatch();
    if (!usesTrigInExpression && !usesTrigInInterpreted)
        return false;

    const Settings* settings = Settings::instance();
    const bool useExtraResultLines = settings->multipleResultLinesEnabled;
    const bool hasRationalAlready =
        settings->resultFormat == 'r'
        || (useExtraResultLines && settings->secondaryResultEnabled && settings->alternativeResultFormat == 'r')
        || (useExtraResultLines && settings->tertiaryResultEnabled && settings->tertiaryResultFormat == 'r')
        || (useExtraResultLines && settings->quaternaryResultEnabled && settings->quaternaryResultFormat == 'r')
        || (useExtraResultLines && settings->quinaryResultEnabled && settings->quinaryResultFormat == 'r');
    if (hasRationalAlready)
        return false;

    return !NumberFormatter::formatTrigSymbolic(quantity).isEmpty();
}

static QString formattedLiveResultWithAlternatives(const Quantity& quantity,
                                                   const QString& expression,
                                                   const QString& interpretedExpression,
                                                   const QString& simplifiedExpression = QString())
{
    const Settings* settings = Settings::instance();
    const bool useExtraResultLines = settings->multipleResultLinesEnabled;
    const QString primaryFormattedResult = formattedLiveResult(quantity);
    QStringList lines;
    auto appendUniqueLine = [&lines](const QString& line) {
        if (line.isEmpty() || lines.contains(line))
            return;
        lines.append(line);
    };
    QString interpretedForDisplay;
    if (!interpretedExpression.isEmpty()) {
        interpretedForDisplay = DisplayFormatUtils::applyDigitGroupingForDisplay(
            Evaluator::formatInterpretedExpressionForDisplay(interpretedExpression));
        appendUniqueLine(interpretedForDisplay);
    }
    if (!simplifiedExpression.isEmpty() && simplifiedExpression != primaryFormattedResult) {
        appendUniqueLine(QStringLiteral("= %1").arg(simplifiedExpression));
    }
    appendUniqueLine(QStringLiteral("= %1").arg(primaryFormattedResult));
    if (useExtraResultLines && settings->secondaryResultEnabled && settings->alternativeResultFormat != '\0') {
        appendUniqueLine(QStringLiteral("= %1").arg(
            formattedLiveResultForSlot(quantity,
                                       settings->alternativeResultFormat,
                                       settings->secondaryResultPrecision,
                                       settings->complexNumbers && settings->secondaryComplexNumbers,
                                       settings->secondaryResultFormatComplex)));
    }
    if (useExtraResultLines && settings->tertiaryResultEnabled && settings->tertiaryResultFormat != '\0') {
        appendUniqueLine(QStringLiteral("= %1").arg(
            formattedLiveResultForSlot(quantity,
                                       settings->tertiaryResultFormat,
                                       settings->tertiaryResultPrecision,
                                       settings->complexNumbers && settings->tertiaryComplexNumbers,
                                       settings->tertiaryResultFormatComplex)));
    }
    if (useExtraResultLines && settings->quaternaryResultEnabled && settings->quaternaryResultFormat != '\0') {
        appendUniqueLine(QStringLiteral("= %1").arg(
            formattedLiveResultForSlot(quantity,
                                       settings->quaternaryResultFormat,
                                       settings->quaternaryResultPrecision,
                                       settings->complexNumbers && settings->quaternaryComplexNumbers,
                                       settings->quaternaryResultFormatComplex)));
    }
    if (useExtraResultLines && settings->quinaryResultEnabled && settings->quinaryResultFormat != '\0') {
        appendUniqueLine(QStringLiteral("= %1").arg(
            formattedLiveResultForSlot(quantity,
                                       settings->quinaryResultFormat,
                                       settings->quinaryResultPrecision,
                                       settings->complexNumbers && settings->quinaryComplexNumbers,
                                       settings->quinaryResultFormatComplex)));
    }
    const QString symbolicTrig = NumberFormatter::formatTrigSymbolic(quantity);
    if (shouldShowAdditionalRationalForTrig(expression, interpretedExpression, quantity)) {
        appendUniqueLine(QStringLiteral("= %1").arg(
            DisplayFormatUtils::applyDigitGroupingForDisplay(symbolicTrig)));
    }
    QStringList escapedLines;
    for (const QString& line : lines)
        escapedLines.append(line.toHtmlEscaped());
    return escapedLines.join(QStringLiteral("<br/>"));
}

static QString simplifiedExpressionLineForTooltip(const QString& interpretedExpression)
{
    const Settings* settings = Settings::instance();
    if (!settings->simplifyResultExpressions || interpretedExpression.isEmpty())
        return QString();

    const QString interpretedDisplay = UnicodeChars::normalizePiForDisplay(
        Evaluator::formatInterpretedExpressionForDisplay(interpretedExpression));
    const QString simplifiedDisplay = UnicodeChars::normalizePiForDisplay(
        Evaluator::formatInterpretedExpressionSimplifiedForDisplay(interpretedExpression));
    if (simplifiedDisplay.isEmpty()
        || simplifiedDisplay == interpretedDisplay) {
        return QString();
    }

    if (SimplifiedExpressionUtils::shouldSuppressSimplifiedExpressionLine(
            interpretedDisplay, simplifiedDisplay)) {
        return QString();
    }

    return DisplayFormatUtils::applyDigitGroupingForDisplay(simplifiedDisplay);
}

Editor::Editor(QWidget* parent)
    : QPlainTextEdit(parent)
{
    m_evaluator = Evaluator::instance();
    m_currentHistoryIndex = 0;
    m_isAutoCompletionEnabled = true;
    m_completion = new EditorCompletion(this);
    m_constantCompletion = 0;
    m_completionTimer = new QTimer(this);
    m_isAutoCalcEnabled = true;
    m_highlighter = new SyntaxHighlighter(this);
    m_matchingTimer = new QTimer(this);
    m_shouldPaintCustomCursor = true;
    m_historyArrowNavigationEnabled = true;

    setViewportMargins(0, 0, 0, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    setTabChangesFocus(true);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setWordWrapMode(QTextOption::WrapAnywhere);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setCursorWidth(0);

    connect(m_completion, &EditorCompletion::selectedCompletion,
            this, &Editor::autoComplete);
    connect(m_completionTimer, SIGNAL(timeout()), SLOT(triggerAutoComplete()));
    connect(m_matchingTimer, SIGNAL(timeout()), SLOT(doMatchingPar()));
    connect(this, &Editor::selectionChanged, this, &Editor::checkSelectionAutoCalc);
    connect(this, &Editor::textChanged, this, &Editor::checkAutoCalc);
    connect(this, &Editor::textChanged, this, &Editor::checkAutoComplete);
    connect(this, &Editor::textChanged, this, &Editor::checkMatching);
    connect(document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
            this, [this](const QSizeF&) { updateHeightForWrappedText(); });

    adjustSize();
    updateHeightForWrappedText();
}

void Editor::refreshAutoCalc()
{
    if (m_isAutoCalcEnabled) {
      if (!textCursor().selectedText().isEmpty())
          checkSelectionAutoCalc();
      else
          checkAutoCalc();
    }
}

QString Editor::text() const
{
    return toPlainText();
}

void Editor::setText(const QString& text)
{
    setPlainText(normalizeExpressionTypedInEditor(text));
}

void Editor::insert(const QString& text)
{
    insertPlainText(normalizeExpressionTypedInEditor(text));
}

void Editor::doBackspace()
{
    QTextCursor cursor = textCursor();
    cursor.deletePreviousChar();
    setTextCursor(cursor);
}

char Editor::radixChar() const
{
    return Settings::instance()->radixCharacter();
}

int Editor::cursorPosition() const
{
    return textCursor().position();
}

void Editor::setCursorPosition(int position)
{
    QTextCursor cursor = textCursor();
    cursor.setPosition(position);
    setTextCursor(cursor);
}

QSize Editor::sizeHint() const
{
    ensurePolished();
    const QFontMetrics metrics = fontMetrics();
    const int width = metrics.horizontalAdvance('x') * 10;
    const int height = metrics.lineSpacing() + 6;
    return QSize(width, height);
}

void Editor::clearHistory()
{
    m_history.clear();
    m_currentHistoryIndex = 0;
}

bool Editor::isAutoCompletionEnabled() const
{
    return m_isAutoCompletionEnabled;
}

void Editor::setAutoCompletionEnabled(bool enable)
{
    m_isAutoCompletionEnabled = enable;
}

bool Editor::isAutoCalcEnabled() const
{
    return m_isAutoCalcEnabled;
}

void Editor::setAutoCalcEnabled(bool enable)
{
    m_isAutoCalcEnabled = enable;
}

void Editor::setHistoryArrowNavigationEnabled(bool enabled)
{
    m_historyArrowNavigationEnabled = enabled;
}

void Editor::checkAutoComplete()
{
    if (!m_isAutoCompletionEnabled)
        return;

    m_completionTimer->stop();
    m_completionTimer->setSingleShot(true);
    m_completionTimer->start();
}


void Editor::checkMatching()
{
    if (!Settings::instance()->syntaxHighlighting)
        return;

    m_matchingTimer->stop();
    m_matchingTimer->setSingleShot(true);
    m_matchingTimer->start();
}

void Editor::checkAutoCalc()
{
    if (m_isAutoCalcEnabled)
        autoCalc();
}

void Editor::doMatchingPar()
{
    // Clear previous.
    setExtraSelections(QList<QTextEdit::ExtraSelection>());

    if (!Settings::instance()->syntaxHighlighting)
        return;

    doMatchingLeft();
    doMatchingRight();
}

void Editor::checkSelectionAutoCalc()
{
    if (m_isAutoCalcEnabled)
        autoCalcSelection();
}

void Editor::doMatchingLeft()
{
    // Tokenize the expression.
    const int currentPosition = textCursor().position();

    // Check for right par.
    QString subtext = text().left(currentPosition);
    Tokens tokens = m_evaluator->scan(subtext);
    if (!tokens.valid() || tokens.count() < 1)
        return;
    Token lastToken = tokens.at(tokens.count() - 1);

    // Right par?
    if (lastToken.type() == Token::stxClosePar
        && lastToken.size() > 0
        && (lastToken.pos() + lastToken.size()) == currentPosition) {
        // Find the matching left par.
        unsigned par = 1;
        int matchPosition = -1;
        int closeParPos = lastToken.pos() + lastToken.size() - 1;

        for (int i = tokens.count() - 2; i >= 0 && par > 0; --i) {
            Token matchToken = tokens.at(i);
            switch (matchToken.type()) {
                case Token::stxOpenPar : --par; break;
                case Token::stxClosePar: ++par; break;
                default:;
            }
            if (matchToken.size() > 0)
                matchPosition = matchToken.pos() + matchToken.size() - 1;
        }

        if (par == 0 && matchPosition >= 0 && closeParPos >= 0) {
            QTextEdit::ExtraSelection hilite1;
            hilite1.cursor = textCursor();
            hilite1.cursor.setPosition(matchPosition);
            hilite1.cursor.setPosition(matchPosition + 1,
                                       QTextCursor::KeepAnchor);
            hilite1.format.setBackground(
                m_highlighter->colorForRole(ColorScheme::Matched));

            QTextEdit::ExtraSelection hilite2;
            hilite2.cursor = textCursor();
            hilite2.cursor.setPosition(closeParPos);
            hilite2.cursor.setPosition(closeParPos + 1,
                                       QTextCursor::KeepAnchor);
            hilite2.format.setBackground(
                m_highlighter->colorForRole(ColorScheme::Matched));

            QList<QTextEdit::ExtraSelection> extras;
            extras << hilite1;
            extras << hilite2;
            setExtraSelections(extras);
        }
    }
}

void Editor::doMatchingRight()
{
    // Tokenize the expression.
    const int currentPosition = textCursor().position();

    // Check for left par.
    auto subtext = text().right(text().length() - currentPosition);
    auto tokens = m_evaluator->scan(subtext);
    if (!tokens.valid() || tokens.count() < 1)
        return;
    auto firstToken = tokens.at(0);

    // Left par?
    if (firstToken.type() == Token::stxOpenPar
        && firstToken.size() > 0
        && (firstToken.pos() + firstToken.size()) == 1)
    {
        // Find the matching right par.
        unsigned par = 1;
        int k = 0;
        Token matchToken;
        int matchPosition = -1;
        int openParPos = firstToken.pos() + firstToken.size() - 1;

        for (k = 1; k < tokens.count() && par > 0; ++k) {
            const Token matchToken = tokens.at(k);
            switch (matchToken.type()) {
            case Token::stxOpenPar:
                ++par;
                break;
            case Token::stxClosePar:
                --par;
                break;
            default:;
            }
            if (matchToken.size() > 0)
                matchPosition = matchToken.pos() + matchToken.size() - 1;
        }

        if (par == 0 && matchPosition >= 0 && openParPos >= 0) {
            QTextEdit::ExtraSelection hilite1;
            hilite1.cursor = textCursor();
            hilite1.cursor.setPosition(currentPosition+matchPosition);
            hilite1.cursor.setPosition(currentPosition+matchPosition + 1,
                                       QTextCursor::KeepAnchor);
            hilite1.format.setBackground(
                m_highlighter->colorForRole(ColorScheme::Matched));

            QTextEdit::ExtraSelection hilite2;
            hilite2.cursor = textCursor();
            hilite2.cursor.setPosition(currentPosition+openParPos);
            hilite2.cursor.setPosition(currentPosition+openParPos + 1,
                                       QTextCursor::KeepAnchor);
            hilite2.format.setBackground(
                m_highlighter->colorForRole(ColorScheme::Matched));

            QList<QTextEdit::ExtraSelection> extras;
            extras << hilite1;
            extras << hilite2;
            setExtraSelections(extras);
        }
    }
}


// Matches a list of built-in functions and variables to a fragment of the name.
QStringList Editor::matchFragment(const QString& id) const
{
    const Settings* settings = Settings::instance();

    // Find matches in function names.
    QStringList choices;
    if (settings->autoCompletionBuiltInFunctions) {
        const auto fnames = FunctionRepo::instance()->getIdentifiers();
        for (int i = 0; i < fnames.count(); ++i) {
            if (fnames.at(i).startsWith(id, Qt::CaseInsensitive)) {
                QString str = fnames.at(i);
                Function* f = FunctionRepo::instance()->find(str);
                if (f)
                    str.append(':').append(f->name());
                choices.append(str);
            }
        }
        choices.sort();
    }

    // Find matches in variables names.
    QStringList vchoices;
    const QList<Unit> allUnits = Units::getList();
    QSet<QString> unitNames;
    for (const Unit& unit : allUnits)
        unitNames.insert(unit.name);
    QList<Variable> variables = m_evaluator->getVariables();
    for (int i = 0; i < variables.count(); ++i) {
        const Variable variable = variables.at(i);
        const bool isBuiltIn = variable.type() == Variable::BuiltIn;
        const bool isUnit = isBuiltIn && unitNames.contains(variable.identifier());
        const bool includeVariable =
            (isUnit && settings->autoCompletionUnits)
            || (!isUnit && isBuiltIn && settings->autoCompletionBuiltInVariables)
            || (!isBuiltIn && settings->autoCompletionUserVariables);
        if (!includeVariable)
            continue;

        if (variable.identifier().startsWith(id, Qt::CaseInsensitive)) {
            const QString variableDescription = variable.description().trimmed().isEmpty()
                ? NumberFormatter::format(variable.value())
                : variable.description().trimmed();
            vchoices.append(QString("%1:%2").arg(
                variable.identifier(),
                variableDescription));
        }
    }
    vchoices.sort();
    choices += vchoices;

    // Find matches in user functions.
    if (settings->autoCompletionUserFunctions) {
        QStringList ufchoices;
        auto userFunctions = m_evaluator->getUserFunctions();
        for (int i = 0; i < userFunctions.count(); ++i) {
            if (userFunctions.at(i).name().startsWith(id, Qt::CaseInsensitive)) {
                const QString description = userFunctions.at(i).description().trimmed().isEmpty()
                    ? tr("User function")
                    : userFunctions.at(i).description().trimmed();
                ufchoices.append(QString("%1:%2").arg(
                    userFunctions.at(i).name(), description));
            }
        }
        ufchoices.sort();
        choices += ufchoices;
    }

    return choices;
}

QString Editor::getKeyword() const
{
    // Tokenize the expression.
    const int currentPosition = textCursor().position();
    const Tokens tokens = m_evaluator->scan(text());

    // Find the token at the cursor.
    for (int i = tokens.size() - 1; i >= 0; --i) {
        const auto& token = tokens[i];
        if (token.pos() > currentPosition)
            continue;
        if (token.isIdentifier()) {
            const QString tokenText = token.text();
            const auto matches = matchFragment(tokenText);

            // Prefer an exact identifier match under cursor; prefix matches
            // are only a fallback for partial identifiers.
            for (const auto& match : matches) {
                const QString identifier = match.split(":").first();
                if (identifier.compare(tokenText, Qt::CaseInsensitive) == 0)
                    return identifier;
            }
            if (!matches.empty())
                return matches.first().split(":").first();
        }

        // Try further to the left.
        continue;
    }
    return "";
}

void Editor::triggerAutoComplete()
{
    if (m_shouldBlockAutoCompletionOnce) {
        m_shouldBlockAutoCompletionOnce = false;
        return;
    }
    if (!m_isAutoCompletionEnabled)
        return;

    // Tokenize the expression (this is very fast).
    const int currentPosition = textCursor().position();
    auto subtext = text().left(currentPosition);
    const auto tokens = m_evaluator->scan(subtext);
    if (!tokens.valid() || tokens.count() < 1)
        return;

    auto lastToken = tokens.at(tokens.count()-1);

    // Last token must be an identifier.
    if (!lastToken.isIdentifier())
        return;
    if (!lastToken.size())  // Invisible unit token
        return;
    const auto id = lastToken.text();
    if (id.length() < 1)
        return;

    // No space after identifier.
    if (lastToken.pos() + lastToken.size() < subtext.length())
        return;

    QStringList choices(matchFragment(id));

    // If we are assigning a user function, find matches in its arguments names
    // and replace variables names that collide.
    if (Evaluator::instance()->isUserFunctionAssign()) {
        for (int i=2; i<tokens.size(); ++i) {
            if (tokens[i].asOperator() == Token::ListSeparator)
                continue;
            if (tokens[i].asOperator() == Token::AssociationEnd)
                break;
            if (tokens[i].isIdentifier()) {
                auto arg = tokens[i].text();
                if (!arg.startsWith(id, Qt::CaseInsensitive))
                    continue;
                for (int j = 0; j < choices.size(); ++j) {
                    if (choices[j].split(":")[0] == arg) {
                        choices.removeAt(j);
                        j--;
                    }
                }
                choices.append(arg + ": " + tr("Argument"));
            }
        }
    }

    // No match, don't bother with completion.
    if (!choices.count())
        return;

    // Single perfect match, no need to give choices.
    if (choices.count() == 1)
        if (choices.at(0).toLower() == id.toLower())
            return;

    // Present the user with completion choices.
    m_completion->showCompletion(choices);
}

void Editor::autoComplete(const QString& item)
{
    if (!m_isAutoCompletionEnabled || item.isEmpty())
        return;

    const int currentPosition = textCursor().position();
    const auto subtext = text().left(currentPosition);
    const auto tokens = m_evaluator->scan(subtext);
    if (!tokens.valid() || tokens.count() < 1)
        return;

    const auto lastToken = tokens.at(tokens.count() - 1);
    if (!lastToken.isIdentifier())
        return;

    const auto str = item.split(':');

    // Add leading space characters if any.
    auto newTokenText = str.at(0);
    const int leadingSpaces = lastToken.size() - lastToken.text().length();
    if (leadingSpaces > 0)
        newTokenText = newTokenText.rightJustified(
            leadingSpaces + newTokenText.length(), ' ');

    blockSignals(true);
    QTextCursor cursor = textCursor();
    cursor.setPosition(lastToken.pos());
    cursor.setPosition(lastToken.pos() + lastToken.size(),
                       QTextCursor::KeepAnchor);
    setTextCursor(cursor);
    insert(newTokenText);
    blockSignals(false);

    cursor = textCursor();
    bool hasParensAlready = cursor.movePosition(QTextCursor::NextCharacter,
                                                QTextCursor::KeepAnchor);
    if (hasParensAlready) {
        auto nextChar = cursor.selectedText();
        hasParensAlready = (nextChar == "(");
    }
    bool isFunction = FunctionRepo::instance()->find(str.at(0))
                      || m_evaluator->hasUserFunction(str.at(0));
    bool shouldAutoInsertParens = isFunction && !hasParensAlready;
    if (shouldAutoInsertParens) {
        insert(QString::fromLatin1("()"));
        cursor = textCursor();
        cursor.movePosition(QTextCursor::PreviousCharacter);
        setTextCursor(cursor);
    }

    checkAutoCalc();
}

void Editor::insertFromMimeData(const QMimeData* source)
{
    const QStringList expressions = EditorUtils::parsePastedExpressionsForEditorInput(source->text());

    if (expressions.isEmpty())
        return;

    if (expressions.size() == 1) {
        // Insert text manually to make sure expression does not contain new line characters
        insert(expressions.at(0));
        return;
    }
    for (int i = 0; i < expressions.size(); ++i) {
        insert(expressions.at(i));
        evaluate();
    }
}

void Editor::autoCalc()
{
    if (!m_isAutoCalcEnabled)
        return;

    const auto str = m_evaluator->autoFix(text());
    if (str.isEmpty())
        return;

    // Same reason as above, do not update "ans".
    m_evaluator->setExpression(str);
    auto quantity = m_evaluator->evalNoAssign();

    if (m_evaluator->error().isEmpty()) {
        QString interpretedExpr = m_evaluator->interpretedExpression();
        QString simplifiedLine;
        if (!quantity.isNan() && !m_evaluator->isUserFunctionAssign()
            && !Evaluator::isCommentOnlyExpression(str)) {
            simplifiedLine = simplifiedExpressionLineForTooltip(interpretedExpr);

            const Settings* settings = Settings::instance();
            if (settings->simplifyResultExpressions
                && !simplifiedLine.isEmpty()
                && !interpretedExpr.contains(QLatin1Char('='))) {
                const QString simplifiedInterpretedExpr =
                    Evaluator::simplifyInterpretedExpression(interpretedExpr);
                m_evaluator->setExpression(simplifiedInterpretedExpr);
                const Quantity simplifiedQuantity = m_evaluator->evalNoAssign();
                if (m_evaluator->error().isEmpty() && !simplifiedQuantity.isNan()) {
                    quantity = simplifiedQuantity;
                } else {
                    m_evaluator->setExpression(str);
                    quantity = m_evaluator->evalNoAssign();
                    interpretedExpr = m_evaluator->interpretedExpression();
                }
            }
        }

        if (quantity.isNan() && (m_evaluator->isUserFunctionAssign()
            || Evaluator::isCommentOnlyExpression(str))) {
            // Result is not available for user function assignment and
            // comment-only expressions.
            emit autoCalcDisabled();
        } else {
            const auto formatted =
                formattedLiveResultWithAlternatives(quantity, str, interpretedExpr, simplifiedLine);
            auto message = tr("Current result:<br/>%1").arg(formatted);
            emit autoCalcMessageAvailable(message);
            emit autoCalcQuantityAvailable(quantity);
        }
    } else {
        if (isOperatorOnlyIncompleteInput(str)) {
            emit autoCalcDisabled();
            return;
        }

        const QString usageTooltip = FunctionTooltipUtils::activeFunctionUsageTooltip(
            m_evaluator,
            text(),
            textCursor().position()
        );
        if (usageTooltip.isEmpty()) {
            QString baseExpression;
            if (EditorUtils::expressionWithoutIgnorableTrailingToken(str, &baseExpression)) {
                m_evaluator->setExpression(baseExpression);
                auto baseQuantity = m_evaluator->evalNoAssign();
                if (m_evaluator->error().isEmpty()) {
                    const QString interpretedExpr = m_evaluator->interpretedExpression();
                    const QString simplifiedLine = (!baseQuantity.isNan()
                        && !m_evaluator->isUserFunctionAssign()
                        && !Evaluator::isCommentOnlyExpression(baseExpression))
                        ? simplifiedExpressionLineForTooltip(interpretedExpr)
                        : QString();
                    if (baseQuantity.isNan() && (m_evaluator->isUserFunctionAssign()
                        || Evaluator::isCommentOnlyExpression(baseExpression))) {
                        emit autoCalcDisabled();
                    } else {
                        const auto formatted =
                            formattedLiveResultWithAlternatives(
                                baseQuantity, baseExpression, interpretedExpr, simplifiedLine);
                        auto message = tr("Current result:<br/>%1").arg(formatted);
                        emit autoCalcMessageAvailable(message);
                        emit autoCalcQuantityAvailable(baseQuantity);
                    }
                    return;
                }
            }
        }
        emit autoCalcMessageAvailable(
            usageTooltip.isEmpty() ? m_evaluator->error() : usageTooltip
        );
    }
}

void Editor::increaseFontPointSize()
{
    QFont newFont = font();
    const int newSize = newFont.pointSize() + 1;
    if (newSize > 96)
        return;
    newFont.setPointSize(newSize);
    setFont(newFont);
}

void Editor::decreaseFontPointSize()
{
    QFont newFont = font();
    const int newSize = newFont.pointSize() - 1;
    if (newSize < 8)
        return;
    newFont.setPointSize(newSize);
    setFont(newFont);
}

void Editor::autoCalcSelection(const QString& custom)
{
    if (!m_isAutoCalcEnabled)
        return;

    const QString rawSelection = custom.isNull() ? textCursor().selectedText() : custom;
    if (rawSelection.contains(RegExpPatterns::lineBreak())) {
        emit autoCalcDisabled();
        return;
    }

    auto str = rawSelection;
    str = m_evaluator->autoFix(str);
    if (str.isEmpty()) {
        emit autoCalcDisabled();
        return;
    }

    // Same reason as above, do not update "ans".
    m_evaluator->setExpression(str);
    auto quantity = m_evaluator->evalNoAssign();

    if (m_evaluator->error().isEmpty()) {
        const QString interpretedExpr = m_evaluator->interpretedExpression();
        const QString simplifiedLine = (!quantity.isNan() && !m_evaluator->isUserFunctionAssign()
            && !Evaluator::isCommentOnlyExpression(str))
            ? simplifiedExpressionLineForTooltip(interpretedExpr)
            : QString();
        if (quantity.isNan() && (m_evaluator->isUserFunctionAssign()
            || Evaluator::isCommentOnlyExpression(str))) {
            // Result is not available for user function assignment and
            // comment-only expressions.
            auto message = tr("Selection result: n/a");
            emit autoCalcMessageAvailable(message);
        } else {
            const auto formatted =
                formattedLiveResultWithAlternatives(quantity, str, interpretedExpr, simplifiedLine);
            auto message = tr("Selection result:<br/>%1").arg(formatted);
            emit autoCalcMessageAvailable(message);
            emit autoCalcQuantityAvailable(quantity);
        }
    } else {
        if (isOperatorOnlyIncompleteInput(str)) {
            emit autoCalcDisabled();
            return;
        }

        QString baseExpression;
        if (EditorUtils::expressionWithoutIgnorableTrailingToken(str, &baseExpression)) {
            m_evaluator->setExpression(baseExpression);
            const auto baseQuantity = m_evaluator->evalNoAssign();
            if (m_evaluator->error().isEmpty()) {
                const QString interpretedExpr = m_evaluator->interpretedExpression();
                const QString simplifiedLine = (!baseQuantity.isNan() && !m_evaluator->isUserFunctionAssign()
                    && !Evaluator::isCommentOnlyExpression(baseExpression))
                    ? simplifiedExpressionLineForTooltip(interpretedExpr)
                    : QString();
                if (baseQuantity.isNan() && (m_evaluator->isUserFunctionAssign()
                    || Evaluator::isCommentOnlyExpression(baseExpression))) {
                    auto message = tr("Selection result: n/a");
                    emit autoCalcMessageAvailable(message);
                } else {
                    const auto formatted =
                        formattedLiveResultWithAlternatives(
                            baseQuantity, baseExpression, interpretedExpr, simplifiedLine);
                    auto message = tr("Selection result:<br/>%1").arg(formatted);
                    emit autoCalcMessageAvailable(message);
                    emit autoCalcQuantityAvailable(baseQuantity);
                }
                return;
            }
        }
        auto message = tr("Selection result: %1").arg(m_evaluator->error());
        emit autoCalcMessageAvailable(message);
    }
}

void Editor::insertConstant(const QString& constant)
{
    auto formattedConstant = constant;
    if (radixChar() == ',')
        formattedConstant.replace('.', ',');
    if (!constant.isNull())
        insert(formattedConstant);
    if (m_constantCompletion) {
        disconnect(m_constantCompletion);
        m_constantCompletion->deleteLater();
        m_constantCompletion = 0;
    }
}

void Editor::cancelConstantCompletion()
{
    if (m_constantCompletion) {
        disconnect(m_constantCompletion);
        m_constantCompletion->deleteLater();
        m_constantCompletion = 0;
    }
}

void Editor::evaluate()
{
    triggerEnter();
}

void Editor::paintEvent(QPaintEvent* event)
{
    QPlainTextEdit::paintEvent(event);

    if (!m_shouldPaintCustomCursor) {
        m_shouldPaintCustomCursor = true;
        return;
    }
    m_shouldPaintCustomCursor = false;

    QRect cursor = cursorRect();
    cursor.setLeft(cursor.left() - 1);
    cursor.setRight(cursor.right() + 1);

    QPainter painter(viewport());
    painter.fillRect(cursor, m_highlighter->colorForRole(ColorScheme::Cursor));
}

void Editor::historyBack()
{
    if (!m_history.count())
        return;
    if (!m_currentHistoryIndex)
        return;

    m_shouldBlockAutoCompletionOnce = true;
    if (m_currentHistoryIndex == m_history.count())
        m_savedCurrentEditor = toPlainText();
    --m_currentHistoryIndex;
    setText(m_history.at(m_currentHistoryIndex).expr());
    moveCursorToEnd(this);
    ensureCursorVisible();
}

void Editor::historyForward()
{
    if (!m_history.count())
        return;
    if (m_currentHistoryIndex == m_history.count())
        return;

    m_shouldBlockAutoCompletionOnce = true;
    m_currentHistoryIndex++;
    if (m_currentHistoryIndex == m_history.count())
        setText(m_savedCurrentEditor);
    else
        setText(m_history.at(m_currentHistoryIndex).expr());
    moveCursorToEnd(this);
    ensureCursorVisible();
}

void Editor::triggerEnter()
{
    m_completionTimer->stop();
    m_matchingTimer->stop();
    m_currentHistoryIndex = m_history.count();
    emit returnPressed();
}

void Editor::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::FontChange)
        updateHeightForWrappedText();
    QPlainTextEdit::changeEvent(event);
}

void Editor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    updateHeightForWrappedText();
}

void Editor::focusOutEvent(QFocusEvent* event)
{
    m_shouldPaintCustomCursor = false;
    QPlainTextEdit::focusOutEvent(event);
}

void Editor::inputMethodEvent(QInputMethodEvent* event)
{
    const QString normalizedCommit = normalizeExpressionTypedInEditor(event->commitString());
    if (normalizedCommit == event->commitString()) {
        QPlainTextEdit::inputMethodEvent(event);
        return;
    }

    QInputMethodEvent normalizedEvent(event->preeditString(), event->attributes());
    normalizedEvent.setCommitString(normalizedCommit, event->replacementStart(), event->replacementLength());
    QPlainTextEdit::inputMethodEvent(&normalizedEvent);
}

void Editor::keyPressEvent(QKeyEvent* event)
{
    int key = event->key();

    switch (key) {
    case Qt::Key_Tab:
        // setTabChangesFocus() still allows entering a Tab character when
        // there's no other widgets to change focus to. To avoid that,
        // explicitly consume any Tabs that make it here.
        event->accept();
        return;

    case Qt::Key_Enter:
    case Qt::Key_Return:
        QTimer::singleShot(0, this, SLOT(triggerEnter()));
        event->accept();
        return;

    case Qt::Key_Escape:
        emit escapePressed();
        event->accept();
        return;

    case Qt::Key_Up:
        if (!m_historyArrowNavigationEnabled) {
            QPlainTextEdit::keyPressEvent(event);
            event->accept();
            return;
        }
        if (event->modifiers() & Qt::ShiftModifier)
            emit shiftUpPressed();
        else if (Settings::instance()->upDownArrowBehavior == Settings::UpDownArrowBehaviorAlways
                 || (Settings::instance()->upDownArrowBehavior == Settings::UpDownArrowBehaviorSingleLineOnly
                     && height() <= sizeHint().height()))
            historyBack();
        else
            QPlainTextEdit::keyPressEvent(event);
        event->accept();
        return;

    case Qt::Key_Down:
        if (!m_historyArrowNavigationEnabled) {
            QPlainTextEdit::keyPressEvent(event);
            event->accept();
            return;
        }
        if (event->modifiers() & Qt::ShiftModifier)
            emit shiftDownPressed();
        else if (Settings::instance()->upDownArrowBehavior == Settings::UpDownArrowBehaviorAlways
                 || (Settings::instance()->upDownArrowBehavior == Settings::UpDownArrowBehaviorSingleLineOnly
                     && height() <= sizeHint().height()))
            historyForward();
        else
            QPlainTextEdit::keyPressEvent(event);
        event->accept();
        return;

    case Qt::Key_PageUp:
        if (event->modifiers() & Qt::ShiftModifier)
            emit shiftPageUpPressed();
        else if (event->modifiers() & Qt::ControlModifier)
            emit controlPageUpPressed();
        else
            emit pageUpPressed();
        event->accept();
        return;

    case Qt::Key_PageDown:
        if (event->modifiers() & Qt::ShiftModifier)
            emit shiftPageDownPressed();
        else if (event->modifiers() & Qt::ControlModifier)
            emit controlPageDownPressed();
        else
            emit pageDownPressed();
        event->accept();
        return;

    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Home:
    case Qt::Key_End:
        checkMatching();
        checkAutoCalc();
        QPlainTextEdit::keyPressEvent(event);
        event->accept();
        return;

    case Qt::Key_Space:
        if (event->modifiers() == Qt::ControlModifier
            && !m_constantCompletion)
        {
            m_constantCompletion = new ConstantCompletion(this);
            connect(m_constantCompletion,
                    SIGNAL(selectedCompletion(const QString&)),
                    SLOT(insertConstant(const QString&)));
            connect(m_constantCompletion,
                    &ConstantCompletion::canceledCompletion,
                    this, &Editor::cancelConstantCompletion);
            m_constantCompletion->showCompletion();
            event->accept();
            return;
        }
        break;

    case Qt::Key_Period:
    case Qt::Key_Comma:
        if (event->modifiers() == Qt::KeypadModifier) {
            insert(QChar(this->radixChar()));
            event->accept();
            return;
        }
        break;

    case Qt::Key_Asterisk: {
        auto position = textCursor().position();
        if (position > 0 && QString("*×").contains(text().at(position - 1))) {
          // Replace ×* by ^ operator
          auto cursor = textCursor();
          cursor.removeSelectedText();  // just in case some text is selected
          cursor.deletePreviousChar();
          insert(QString::fromUtf8("^"));
        } else {
          insert(QString(UnicodeChars::MultiplicationSign));
        }
        event->accept();
        return;
    }

    case Qt::Key_Minus:
        insert(QString(UnicodeChars::MinusSign));
        event->accept();
        return;
    case Qt::Key_At:
        insert(QString::fromUtf8("°")); // U+00B0 ° DEGREE SIGN
        event->accept();
        return;
    case Qt::Key_ParenLeft:
        if (!textCursor().hasSelection())
        {
            QTextCursor cursor = textCursor();
            const int position = cursor.position();
            const QString rightText = text().mid(position);
            const bool isAtEndOfExpression = rightText.trimmed().isEmpty();

            bool isBeforeOperator = false;
            bool isBeforeClosingPar = false;
            if (!isAtEndOfExpression) {
                const int firstNonSpace = rightText.indexOf(QRegularExpression(QStringLiteral("\\S")));
                if (firstNonSpace >= 0)
                    isBeforeClosingPar = rightText.at(firstNonSpace) == QLatin1Char(')');

                const Tokens rightTokens = m_evaluator->scan(rightText);
                if (rightTokens.valid() && !rightTokens.isEmpty())
                    isBeforeOperator = rightTokens.at(0).type() == Token::stxOperator;
            }

            if (isAtEndOfExpression || isBeforeOperator || isBeforeClosingPar) {
                cursor.insertText(QStringLiteral("()"));
                cursor.setPosition(position + 1);
                setTextCursor(cursor);
                event->accept();
                return;
            }
        }
        break;
    default:;
    }

    if (event->matches(QKeySequence::Copy)) {
        emit copySequencePressed();
        event->accept();
        return;
    }

    const QString normalizedText = normalizeExpressionTypedInEditor(event->text());
    if (!normalizedText.isEmpty() && normalizedText != event->text()) {
        insert(normalizedText);
        event->accept();
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void Editor::scrollContentsBy(int dx, int dy)
{
    if (dy)
        return;
    QPlainTextEdit::scrollContentsBy(dx, dy);
    verticalScrollBar()->setMaximum(0);
    verticalScrollBar()->setMinimum(0);
}

void Editor::updateHeightForWrappedText()
{
    const int baseHeight = sizeHint().height();
    int visualLineCount = 0;

    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        const QTextLayout* layout = block.layout();
        if (!layout) {
            visualLineCount += 1;
            continue;
        }

        visualLineCount += std::max(1, layout->lineCount());
    }

    if (visualLineCount <= 0)
        visualLineCount = 1;

    const int clampedLines = std::max(1, std::min(5, visualLineCount));
    setFixedHeight(baseHeight * clampedLines);
}

void Editor::wheelEvent(QWheelEvent* event)
{
    if (event->angleDelta().y() > 0)
        historyBack();
    else if (event->angleDelta().y() < 0)
        historyForward();
    event->accept();
}

void Editor::rehighlight()
{
    m_highlighter->update();
    auto color = m_highlighter->colorForRole(ColorScheme::EditorBackground);
    auto colorName = color.name();
    setStyleSheet(QString("QPlainTextEdit { background: %1; }").arg(colorName));
}

void Editor::updateHistory()
{
    const Session* session = Evaluator::instance()->session();
    const int sessionHistoryCount = session->historySize();

    // Fast path for appending one new history entry.
    if (sessionHistoryCount == m_history.count() + 1) {
        m_history.append(session->historyEntryAtRef(sessionHistoryCount - 1));
        m_currentHistoryIndex = m_history.count();
        return;
    }

    // Fast path for history cap behavior: oldest entry dropped, newest appended.
    if (sessionHistoryCount == m_history.count() && sessionHistoryCount > 0) {
        const int count = sessionHistoryCount;
        if (count == 1) {
            m_history[0] = session->historyEntryAtRef(count - 1);
            m_currentHistoryIndex = m_history.count();
            return;
        }
        if (m_history.count() > 1
            && session->historyEntryAtRef(0).expr() == m_history.at(1).expr())
        {
            m_history.removeFirst();
            m_history.append(session->historyEntryAtRef(count - 1));
            m_currentHistoryIndex = m_history.count();
            return;
        }
    }

    m_history.clear();
    m_history.reserve(sessionHistoryCount);
    for (int i = 0; i < sessionHistoryCount; ++i)
        m_history.append(session->historyEntryAtRef(i));
    m_currentHistoryIndex = m_history.count();
}

void Editor::stopAutoCalc()
{
    emit autoCalcDisabled();
}

void Editor::stopAutoComplete()
{
    m_completionTimer->stop();
    m_completion->selectItem(QString());
    m_completion->doneCompletion();
    setFocus();
}

void Editor::wrapSelection()
{
    auto cursor = textCursor();
    if (cursor.hasSelection()) {
        const int selectionStart = cursor.selectionStart();
        const int selectionEnd = cursor.selectionEnd();
        cursor.setPosition(selectionStart);
        cursor.insertText("(");
        cursor.setPosition(selectionEnd + 1);
        cursor.insertText(")");
    } else {
        cursor.movePosition(QTextCursor::Start);
        cursor.insertText("(");
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(")");
    }
    setTextCursor(cursor);
}

EditorCompletion::EditorCompletion(Editor* editor)
    : QObject(editor)
{
    m_editor = editor;

    m_popup = new QTreeWidget();
    m_popup->setFrameShape(QFrame::NoFrame);
    m_popup->setColumnCount(3);
    m_popup->setRootIsDecorated(false);
    m_popup->header()->hide();
    m_popup->header()->setStretchLastSection(false);
    m_popup->setEditTriggers(QTreeWidget::NoEditTriggers);
    m_popup->setSelectionBehavior(QTreeWidget::SelectRows);
    m_popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_popup->setMouseTracking(true);
    m_popup->installEventFilter(this);

    connect(m_popup, SIGNAL(itemClicked(QTreeWidgetItem*, int)),
            SLOT(doneCompletion()));

    m_popup->hide();
    m_popup->setParent(editor->window(), Qt::Popup);
    m_popup->setFocusPolicy(Qt::NoFocus);
    m_popup->setFocusProxy(editor);
    m_popup->setFrameStyle(QFrame::Box | QFrame::Plain);
}

EditorCompletion::~EditorCompletion()
{
    // Popup ownership is handled by Qt parent-child deletion (its parent is
    // the window). Deleting it manually here can double-delete during shutdown.
}

bool EditorCompletion::eventFilter(QObject* object, QEvent* event)
{
    if (object != m_popup)
        return false;

    if (event->type() == QEvent::KeyPress) {
        int key = static_cast<QKeyEvent*>(event)->key();

        switch (key) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Tab:
            doneCompletion();
            return true;

        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Home:
        case Qt::Key_End:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            return false;

        default:
            m_popup->hide();
            m_editor->setFocus();
            if (key != Qt::Key_Escape)
                QApplication::sendEvent(m_editor, event);
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        m_popup->hide();
        m_editor->setFocus();
        return true;
    }

    return false;
}

void EditorCompletion::doneCompletion()
{
    m_popup->hide();
    m_editor->setFocus();
    QTreeWidgetItem* item = m_popup->currentItem();
    emit selectedCompletion(item ? item->text(1) : QString());
}

void EditorCompletion::showCompletion(const QStringList& choices)
{
    if (!choices.count())
        return;

    QFontMetrics metrics(m_editor->font());

    m_popup->setUpdatesEnabled(false);
    m_popup->clear();

    for (int i = 0; i < choices.count(); ++i) {
        const auto pair = choices.at(i).split(':');
        if (pair.count() < 2)
            continue;

        const auto identifier = pair.at(0);
        const auto description = pair.mid(1).join(":");
        QString typeSymbol = QString::fromUtf8("👤 𝑥");

        if (FunctionRepo::instance()->find(identifier)) {
            typeSymbol = QString::fromUtf8("📚 ƒ");
        } else if (Evaluator::instance()->hasUserFunction(identifier)) {
            typeSymbol = QString::fromUtf8("👤 ƒ");
        } else if (Evaluator::instance()->hasVariable(identifier)) {
            const auto variable = Evaluator::instance()->getVariable(identifier);
            if (variable.type() == Variable::BuiltIn)
                typeSymbol = QString::fromUtf8("📏 𝑘");
        }

        QStringList columns;
        columns << typeSymbol << identifier << description;
        QTreeWidgetItem* item = new QTreeWidgetItem(m_popup, columns);

        if (item && m_editor->layoutDirection() == Qt::RightToLeft)
            item->setTextAlignment(1, Qt::AlignRight);

        item->setTextAlignment(0, Qt::AlignCenter);
    }

    m_popup->sortItems(2, Qt::AscendingOrder);
    m_popup->sortItems(1, Qt::AscendingOrder);
    m_popup->setCurrentItem(m_popup->topLevelItem(0));

    // Size of the pop-up.
    m_popup->resizeColumnToContents(0);
    m_popup->setColumnWidth(0, m_popup->columnWidth(0) + 18);
    m_popup->resizeColumnToContents(1);
    m_popup->setColumnWidth(1, m_popup->columnWidth(1) + 25);
    m_popup->resizeColumnToContents(2);
    m_popup->setColumnWidth(2, m_popup->columnWidth(2) + 25);

    const int maxVisibleItems = 8;
    const int height =
        m_popup->sizeHintForRow(0) * qMin(maxVisibleItems, choices.count()) + 3;
    const int width = m_popup->columnWidth(0)
                      + m_popup->columnWidth(1)
                      + m_popup->columnWidth(2) + 1;

    // Position, reference is editor's cursor position in global coord.
    auto cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::StartOfWord);
    const int pixelsOffset = metrics.horizontalAdvance(m_editor->text(), cursor.position());
    auto point = QPoint(pixelsOffset, m_editor->height());
    QPoint position = m_editor->mapToGlobal(point);

    // If popup is partially invisible, move to other position.
    auto screen = m_editor->screen()->availableGeometry();
    if (position.y() + height > screen.y() + screen.height())
        position.setY(position.y() - height - m_editor->height());
    if (position.x() + width > screen.x() + screen.width())
        position.setX(screen.x() + screen.width() - width);

    m_popup->setUpdatesEnabled(true);
    m_popup->setGeometry(QRect(position, QSize(width, height)));
    m_popup->show();
    m_popup->setFocus();
}

void EditorCompletion::selectItem(const QString& item)
{
    if (item.isNull()) {
        m_popup->setCurrentItem(0);
        return;
    }

    auto targets = m_popup->findItems(item, Qt::MatchExactly, 1);
    if (targets.count() > 0)
        m_popup->setCurrentItem(targets.at(0));
}

ConstantCompletion::ConstantCompletion(Editor* editor)
    : QObject(editor)
{
    m_editor = editor;

    m_popup = new QFrame;
    m_popup->setParent(editor->window(), Qt::Popup);
    m_popup->setFocusPolicy(Qt::NoFocus);
    m_popup->setFocusProxy(editor);
    m_popup->setFrameStyle(QFrame::Box | QFrame::Plain);

    m_categoryWidget = new QTreeWidget(m_popup);
    m_categoryWidget->setFrameShape(QFrame::NoFrame);
    m_categoryWidget->setColumnCount(1);
    m_categoryWidget->setRootIsDecorated(false);
    m_categoryWidget->header()->hide();
    m_categoryWidget->setEditTriggers(QTreeWidget::NoEditTriggers);
    m_categoryWidget->setSelectionBehavior(QTreeWidget::SelectRows);
    m_categoryWidget->setMouseTracking(true);
    m_categoryWidget->installEventFilter(this);
    m_categoryWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    connect(m_categoryWidget, SIGNAL(itemClicked(QTreeWidgetItem*, int)),
                              SLOT(showConstants()));

    m_constantWidget = new QTreeWidget(m_popup);
    m_constantWidget->setFrameShape(QFrame::NoFrame);
    m_constantWidget->setColumnCount(2);
    m_constantWidget->setColumnHidden(1, true);
    m_constantWidget->setRootIsDecorated(false);
    m_constantWidget->header()->hide();
    m_constantWidget->setEditTriggers(QTreeWidget::NoEditTriggers);
    m_constantWidget->setSelectionBehavior(QTreeWidget::SelectRows);
    m_constantWidget->setMouseTracking(true);
    m_constantWidget->installEventFilter(this);
    m_constantWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    connect(m_constantWidget, SIGNAL(itemClicked(QTreeWidgetItem*, int)),
                              SLOT(doneCompletion()));

    m_slider = new QTimeLine(100, m_popup);
    m_slider->setEasingCurve(QEasingCurve(QEasingCurve::Linear));
    connect(m_slider, SIGNAL(frameChanged(int)),
                      SLOT(setHorizontalPosition(int)));

    const Constants* constants = Constants::instance();
    m_constantList = constants->list();

    // Populate categories.
    QStringList categoryList;
    categoryList << tr("All");
    QTreeWidgetItem* all = new QTreeWidgetItem(m_categoryWidget, categoryList);
    for (int i = 0; i < constants->categories().count(); ++i) {
        categoryList.clear();
        categoryList << constants->categories().at(i);
        new QTreeWidgetItem(m_categoryWidget, categoryList);
    }
    m_categoryWidget->setCurrentItem(all);

    // Populate constants.
    m_lastCategory = tr("All");
    for (int i = 0; i < constants->list().count(); ++i) {
        QStringList names;
        names << constants->list().at(i).name;
        names << constants->list().at(i).name.toUpper();
        new QTreeWidgetItem(m_constantWidget, names);
    }
    m_constantWidget->sortItems(0, Qt::AscendingOrder);

    // Find size, the biggest between both.
    m_constantWidget->resizeColumnToContents(0);
    m_categoryWidget->resizeColumnToContents(0);
    int width = qMax(m_constantWidget->width(), m_categoryWidget->width());
    const int constantsHeight =
        m_constantWidget->sizeHintForRow(0)
            * qMin(7, m_constantList.count()) + 3;
    const int categoriesHeight =
        m_categoryWidget->sizeHintForRow(0)
            * qMin(7, constants->categories().count()) + 3;
    const int height = qMax(constantsHeight, categoriesHeight);
    width += 200; // Extra space (FIXME: scrollbar size?).

    // Adjust dimensions.
    m_popup->resize(width, height);
    m_constantWidget->resize(width, height);
    m_categoryWidget->resize(width, height);
}

ConstantCompletion::~ConstantCompletion()
{
    // Popup ownership is handled by Qt parent-child deletion (its parent is
    // the window). Deleting it manually here can double-delete during shutdown.
    m_editor->setFocus();
}

void ConstantCompletion::showCategory()
{
    m_slider->setFrameRange(m_popup->width(), 0);
    m_slider->stop();
    m_slider->start();
    m_categoryWidget->setFocus();
}

void ConstantCompletion::showConstants()
{
    m_slider->setFrameRange(0, m_popup->width());
    m_slider->stop();
    m_slider->start();
    m_constantWidget->setFocus();

    QString chosenCategory;
    if (m_categoryWidget->currentItem())
        chosenCategory = m_categoryWidget->currentItem()->text(0);

    if (m_lastCategory == chosenCategory)
        return;

    m_constantWidget->clear();

    for (int i = 0; i < m_constantList.count(); ++i) {
        QStringList names;
        names << m_constantList.at(i).name;
        names << m_constantList.at(i).name.toUpper();

        const bool include = (chosenCategory == tr("All")) ?
            true : (m_constantList.at(i).category == chosenCategory);

        if (!include)
            continue;

        new QTreeWidgetItem(m_constantWidget, names);
    }

    m_constantWidget->sortItems(0, Qt::AscendingOrder);
    m_constantWidget->setCurrentItem(m_constantWidget->itemAt(0, 0));
    m_lastCategory = chosenCategory;
}

bool ConstantCompletion::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::Hide) {
        emit canceledCompletion();
        return true;
    }

    if (object == m_constantWidget) {

        if (event->type() == QEvent::KeyPress) {
            int key = static_cast<QKeyEvent*>(event)->key();

            switch (key) {
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Tab:
                doneCompletion();
                return true;

            case Qt::Key_Left:
                showCategory();
                return true;

            case Qt::Key_Right:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Home:
            case Qt::Key_End:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
                return false;
            }

            if (key != Qt::Key_Escape)
                QApplication::sendEvent(m_editor, event);
            m_popup->hide();
            emit canceledCompletion();
            return true;
        }
    }

    if (object == m_categoryWidget) {

        if (event->type() == QEvent::KeyPress) {
            int key = static_cast<QKeyEvent*>(event)->key();

            switch (key) {
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Right:
                showConstants();
                return true;

            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Home:
            case Qt::Key_End:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
                return false;
            }

            if (key != Qt::Key_Escape)
                QApplication::sendEvent(m_editor, event);
            m_popup->hide();
            emit canceledCompletion();
            return true;
        }
    }

    return false;
}

void ConstantCompletion::doneCompletion()
{
    m_popup->hide();
    m_editor->setFocus();
    const auto* item = m_constantWidget->currentItem();
    auto found = std::find_if(m_constantList.begin(), m_constantList.end(),
        [&](const Constant& c) { return item->text(0) == c.name; }
    );
    emit selectedCompletion(
        (item && found != m_constantList.end()) ?
            found->value : QString()
    );
}

void ConstantCompletion::showCompletion()
{
    // Position, reference is editor's cursor position in global coord.
    QFontMetrics metrics(m_editor->font());
    const int currentPosition = m_editor->textCursor().position();
    const int pixelsOffset = metrics.horizontalAdvance(m_editor->text(), currentPosition);
    auto pos = m_editor->mapToGlobal(QPoint(pixelsOffset, m_editor->height()));

    const int height = m_popup->height();
    const int width = m_popup->width();

    // If popup is partially invisible, move to other position.
    const QRect screen = m_editor->screen()->availableGeometry();
    if (pos.y() + height > screen.y() + screen.height())
        pos.setY(pos.y() - height - m_editor->height());
    if (pos.x() + width > screen.x() + screen.width())
        pos.setX(screen.x() + screen.width() - width);

    // Start with category.
    m_categoryWidget->setFocus();
    setHorizontalPosition(0);

    m_popup->move(pos);
    m_popup->show();
}

void ConstantCompletion::setHorizontalPosition(int x)
{
    m_categoryWidget->move(-x, 0);
    m_constantWidget->move(m_popup->width() - x, 0);
}
