// This file is part of the SpeedCrunch project
// Copyright (C) 2009-2016 @heldercorreia
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

#include "gui/resultdisplay.h"

#include "core/functions.h"
#include "core/numberformatter.h"
#include "core/settings.h"
#include "gui/syntaxhighlighter.h"
#include "math/cmath.h"
#include "math/floatconfig.h"
#include "core/evaluator.h"
#include "core/session.h"
#include "core/sessionhistory.h"

#include <QLatin1String>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QToolTip>

namespace {
QPointF badgeCenter(const QRect& rect)
{
    return QPointF(rect.x() + rect.width() * 0.5, rect.y() + rect.height() * 0.5);
}

QColor hoverColorForBackground(const QColor& background)
{
    const bool isLightBackground = background.lightnessF() >= 0.5;
    const QColor target = isLightBackground ? QColor(Qt::black) : QColor(Qt::white);
    const qreal blendFactor = 0.12; // subtle, but always visible even on pure black/white backgrounds

    auto mixChannel = [blendFactor](int from, int to) {
        return qBound(0, static_cast<int>(from + (to - from) * blendFactor), 255);
    };

    return QColor(mixChannel(background.red(), target.red()),
                  mixChannel(background.green(), target.green()),
                  mixChannel(background.blue(), target.blue()));
}

void cloneMenuActions(const QMenu* sourceMenu, QMenu* targetMenu)
{
    const QList<QAction*> sourceActions = sourceMenu->actions();
    for (QAction* sourceAction : sourceActions) {
        if (sourceAction->isSeparator()) {
            targetMenu->addSeparator();
            continue;
        }

        QMenu* sourceSubmenu = sourceAction->menu();
        if (sourceSubmenu != 0) {
            QMenu* clonedSubmenu = targetMenu->addMenu(sourceSubmenu->title());
            clonedSubmenu->setEnabled(sourceAction->isEnabled());
            cloneMenuActions(sourceSubmenu, clonedSubmenu);
            continue;
        }

        targetMenu->addAction(sourceAction);
    }
}

QStringList formatResultLines(const Quantity& value)
{
    const Settings* settings = Settings::instance();
    auto groupedResultForDisplay = [settings](const QString& input) {
        if (settings->digitGrouping <= 0)
            return input;

        const QString separator = QStringLiteral(" ").repeated(settings->digitGrouping);
        static const QRegularExpression numberPattern(
            QStringLiteral("(?<![\\p{L}\\p{N}])(?:0[xX][0-9A-Fa-f]+(?:[\\.,][0-9A-Fa-f]+)?|0[oO][0-7]+(?:[\\.,][0-7]+)?|0[bB][01]+(?:[\\.,][01]+)?|\\d+(?:[\\.,]\\d+)?(?:[eE][+\\-]?\\d+)?)(?![\\p{L}\\p{N}])"));

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
        auto it = numberPattern.globalMatch(input);
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
                const int exponentPos = body.indexOf(QRegularExpression(QStringLiteral("[eE][+\\-]?\\d+$")));
                if (exponentPos >= 0) {
                    exponent = body.mid(exponentPos);
                    body = body.left(exponentPos);
                }
            }

            const int radixPos = body.indexOf(QRegularExpression(QStringLiteral("[\\.,]")));
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
    };

    QStringList lines;
    lines.append(QLatin1String("= ") + groupedResultForDisplay(NumberFormatter::format(value)));
    if (settings->alternativeResultFormat != '\0') {
        lines.append(QLatin1String("= ")
            + groupedResultForDisplay(NumberFormatter::format(value, settings->alternativeResultFormat)));
    }
    if (settings->tertiaryResultFormat != '\0') {
        lines.append(QLatin1String("= ")
            + groupedResultForDisplay(NumberFormatter::format(value, settings->tertiaryResultFormat)));
    }
    return lines;
}

int resultLineCountForSettings(const Settings* settings)
{
    int count = 1; // Primary result format is always shown for non-NaN results.
    if (settings->alternativeResultFormat != '\0')
        ++count;
    if (settings->tertiaryResultFormat != '\0')
        ++count;
    return count;
}
}

ResultDisplay::ResultDisplay(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_highlighter(new SyntaxHighlighter(this))
    , m_scrolledLines(0)
    , m_scrollDirection(0)
    , m_isScrollingPageOnly(false)
    , m_hoverHighlightEnabled(true)
    , m_hoveredHistoryIndex(-1)
    , m_editingHistoryIndex(-1)
    , m_count(0)
{
    setViewportMargins(0, 0, 0, 0);
    setBackgroundRole(QPalette::Base);
    setLayoutDirection(Qt::LeftToRight);
    setMinimumWidth(150);
    setReadOnly(true);
    setFocusPolicy(Qt::NoFocus);
    setWordWrapMode(QTextOption::WrapAnywhere);
    setMouseTracking(true);
}

void ResultDisplay::setHoverHighlightEnabled(bool enabled)
{
    if (m_hoverHighlightEnabled == enabled)
        return;

    m_hoverHighlightEnabled = enabled;
    if (!m_hoverHighlightEnabled)
        m_hoveredHistoryIndex = -1;
    updateHoverHighlightSelection();
}

void ResultDisplay::setEditingHistoryIndex(int index)
{
    if (m_editingHistoryIndex == index)
        return;

    const int previousEditingHistoryIndex = m_editingHistoryIndex;
    m_editingHistoryIndex = index;

    if (m_editingHistoryIndex >= 0 && m_hoveredHistoryIndex >= 0) {
        const int previousHoveredHistoryIndex = m_hoveredHistoryIndex;
        m_hoveredHistoryIndex = -1;
        updateHoverHighlightSelection();
        viewport()->update(hoverActionRectForHistoryIndex(previousHoveredHistoryIndex));
    }
    if (m_editingHistoryIndex >= 0)
        viewport()->unsetCursor();

    updateHoverHighlightSelection();

    if (previousEditingHistoryIndex >= 0)
        viewport()->update(viewport()->rect());
    if (m_editingHistoryIndex >= 0)
        viewport()->update(viewport()->rect());
}

void ResultDisplay::append(const QString& expression, Quantity& value)
{
    ++m_count;

    appendPlainText(expression);
    if (!value.isNan()) {
        const QStringList resultLines = formatResultLines(value);
        for (const QString& line : resultLines)
            appendPlainText(line);
    }
    appendPlainText(QLatin1String(""));
}

int ResultDisplay::count() const
{
    return m_count;
}

QString ResultDisplay::exportHtml() const
{
    QString str;
    m_highlighter->asHtml(str);
    return str;
}

void ResultDisplay::rehighlight()
{
    m_highlighter->update();
    updateScrollBarStyleSheet();
}


void ResultDisplay::clear()
{
    m_count = 0;
    setPlainText(QLatin1String(""));
    clearHoverFeedback();
}

void ResultDisplay::clearHoverFeedback()
{
    const int previousHoveredHistoryIndex = m_hoveredHistoryIndex;
    m_hoveredHistoryIndex = -1;
    QToolTip::hideText();
    updateHoverHighlightSelection();
    if (previousHoveredHistoryIndex >= 0)
        viewport()->update(hoverActionRectForHistoryIndex(previousHoveredHistoryIndex));
}


void ResultDisplay::refresh()
{
    clearHoverFeedback();
    clear();
    QList<HistoryEntry> history = Evaluator::instance()->session()->historyToList();
    m_count = history.count();

    for(int i=0; i<m_count; ++i) {
        QString expression = history[i].expr();
        Quantity value = history[i].result();
        appendPlainText(expression);
        if (!value.isNan()) {
            const QStringList resultLines = formatResultLines(value);
            for (const QString& line : resultLines)
                appendPlainText(line);
        }
        appendPlainText(QLatin1String(""));
    }

}

void ResultDisplay::refreshLastHistoryEntry()
{
    const QList<HistoryEntry> history = Evaluator::instance()->session()->historyToList();
    const int historyCount = history.count();
    if (historyCount == 0) {
        clear();
        return;
    }

    if (m_count != historyCount || blockCount() <= 0) {
        refresh();
        return;
    }

    QTextBlock endBlock = document()->lastBlock();
    while (endBlock.isValid() && endBlock.text().isEmpty() && endBlock.previous().isValid())
        endBlock = endBlock.previous();

    if (!endBlock.isValid()) {
        refresh();
        return;
    }

    QTextBlock startBlock = endBlock;
    while (startBlock.previous().isValid() && !startBlock.previous().text().isEmpty())
        startBlock = startBlock.previous();

    const HistoryEntry& lastEntry = history.last();
    QStringList updatedLines;
    updatedLines.append(lastEntry.expr());
    if (!lastEntry.result().isNan())
        updatedLines.append(formatResultLines(lastEntry.result()));
    updatedLines.append(QLatin1String(""));

    clearHoverFeedback();

    QTextCursor cursor(document());
    cursor.setPosition(startBlock.position());
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.insertText(updatedLines.join(QLatin1String("\n")));
}

void ResultDisplay::scrollLines(int numberOfLines)
{
    QScrollBar* bar = verticalScrollBar();
    bar->setValue(bar->value() + numberOfLines);
}

void ResultDisplay::scrollLineUp()
{
    if (m_scrollDirection != 0) {
        stopActiveScrollingAnimation();
        return;
    }

    scrollLines(-1);
}

void ResultDisplay::scrollLineDown()
{
    if (m_scrollDirection != 0) {
        stopActiveScrollingAnimation();
        return;
    }

    scrollLines(1);
}

void ResultDisplay::scrollToDirection(int direction)
{
    m_scrolledLines = 0;
    bool mustStartTimer = (m_scrollDirection == 0);
    m_scrollDirection = direction;
    if (mustStartTimer)
        m_scrollTimer.start(16, this);
}

void ResultDisplay::scrollPageUp()
{
    if (verticalScrollBar()->value() == 0)
        return;

    m_isScrollingPageOnly = true;
    scrollToDirection(-1);
}

void ResultDisplay::scrollPageDown()
{
    if (verticalScrollBar()->value() == verticalScrollBar()->maximum())
        return;

    m_isScrollingPageOnly = true;
    scrollToDirection(1);
}

void ResultDisplay::scrollToTop()
{
    if (verticalScrollBar()->value() == 0)
        return;

    m_isScrollingPageOnly = false;
    scrollToDirection(-1);
}


void ResultDisplay::scrollToBottom()
{
    if (verticalScrollBar()->value() == verticalScrollBar()->maximum())
        return;

    m_isScrollingPageOnly = false;
    scrollToDirection(1);
}

void ResultDisplay::increaseFontPointSize()
{
    QFont newFont = font();
    const int newSize = newFont.pointSize() + 1;
    if (newSize > 96)
        return;
    newFont.setPointSize(newSize);
    setFont(newFont);
}

void ResultDisplay::decreaseFontPointSize()
{
    QFont newFont = font();
    const int newSize = newFont.pointSize() - 1;
    if (newSize < 8)
        return;
    newFont.setPointSize(newSize);
    setFont(newFont);
}

void ResultDisplay::mouseDoubleClickEvent(QMouseEvent*)
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
    QString text = cursor.selectedText();
    QString resultMarker = QLatin1String("= ");
    if (text.startsWith(resultMarker))
        text.remove(resultMarker);
    emit expressionSelected(text);
}

void ResultDisplay::mousePressEvent(QMouseEvent* event)
{
    if (m_editingHistoryIndex >= 0) {
        const QRect cancelRect = cancelGlyphBadgeRectForEditingIndex();
        if (event->button() == Qt::LeftButton && cancelRect.isValid() && cancelRect.contains(event->pos())) {
            emit cancelHistoryEditRequested();
            event->accept();
            return;
        }
        QPlainTextEdit::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton && m_hoverHighlightEnabled && m_hoveredHistoryIndex >= 0) {
        const QRect editRect = editGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
        if (editRect.contains(event->pos())) {
            emit editHistoryEntryRequested(m_hoveredHistoryIndex);
            event->accept();
            return;
        }

        const QRect removeRect = removeGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
        if (removeRect.contains(event->pos())) {
            emit removeHistoryEntryRequested(m_hoveredHistoryIndex);
            event->accept();
            return;
        }
    }

    QPlainTextEdit::mousePressEvent(event);
}

void ResultDisplay::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu();
    const int historyIndex = historyIndexAtPosition(event->pos());
    if (historyIndex >= 0) {
        menu->addSeparator();
        QAction* editAction = menu->addAction(tr("Edit This Calculation"));
        connect(editAction, &QAction::triggered, this, [this, historyIndex]() {
            emit editHistoryEntryRequested(historyIndex);
        });
        menu->addSeparator();
        QAction* removeAction = menu->addAction(tr("Remove This Calculation"));
        connect(removeAction, &QAction::triggered, this, [this, historyIndex]() {
            emit removeHistoryEntryRequested(historyIndex);
        });
        menu->addSeparator();
        QAction* removeAboveAction = menu->addAction(tr("Remove All Calculations Above"));
        connect(removeAboveAction, &QAction::triggered, this, [this, historyIndex]() {
            emit removeHistoryEntriesAboveRequested(historyIndex);
        });
        QAction* removeBelowAction = menu->addAction(tr("Remove All Calculations Below"));
        connect(removeBelowAction, &QAction::triggered, this, [this, historyIndex]() {
            emit removeHistoryEntriesBelowRequested(historyIndex);
        });
    }

    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(window());
    if (mainWindow != 0 && mainWindow->menuBar() != 0) {
        menu->addSeparator();
        QMenu* mainMenu = menu->addMenu(tr("Main Menu"));
        const QList<QAction*> topLevelActions = mainWindow->menuBar()->actions();
        for (QAction* topLevelAction : topLevelActions) {
            if (topLevelAction->isSeparator()) {
                mainMenu->addSeparator();
                continue;
            }

            QMenu* sourceSubmenu = topLevelAction->menu();
            if (sourceSubmenu != 0) {
                QMenu* clonedTopLevelSubmenu = mainMenu->addMenu(sourceSubmenu->title());
                clonedTopLevelSubmenu->setEnabled(topLevelAction->isEnabled());
                cloneMenuActions(sourceSubmenu, clonedTopLevelSubmenu);
            }
        }
    }

    menu->exec(event->globalPos());
    delete menu;
}

void ResultDisplay::leaveEvent(QEvent* event)
{
    QPlainTextEdit::leaveEvent(event);
    viewport()->unsetCursor();
    if (m_hoveredHistoryIndex >= 0) {
        const int previousHoveredHistoryIndex = m_hoveredHistoryIndex;
        m_hoveredHistoryIndex = -1;
        updateHoverHighlightSelection();
        viewport()->update(hoverActionRectForHistoryIndex(previousHoveredHistoryIndex));
    }
}

void ResultDisplay::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != m_scrollTimer.timerId()) {
        QWidget::timerEvent(event);
        return;
    }

    if (m_isScrollingPageOnly)
        pageScrollEvent();
    else
        fullContentScrollEvent();
}

void ResultDisplay::pageScrollEvent()
{
    if (m_scrolledLines >= linesPerPage()) {
        stopActiveScrollingAnimation();
        return;
    }

    scrollLines(m_scrollDirection * 2);
    m_scrolledLines += 2;
}

void ResultDisplay::fullContentScrollEvent()
{
    QScrollBar* bar = verticalScrollBar();
    int value = bar->value();
    bool shouldStop = (m_scrollDirection == -1 && value <= 0) || (m_scrollDirection == 1 && value >= bar->maximum());

    if (shouldStop && m_scrollDirection != 0) {
        stopActiveScrollingAnimation();
        return;
    }

    scrollLines(m_scrollDirection * 10);
}

void ResultDisplay::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {
        if (event->angleDelta().y() > 0)
            emit shiftControlWheelUp();
        else
            emit shiftControlWheelDown();
    } else if (event->modifiers() == Qt::ShiftModifier) {
        if (event->angleDelta().y() > 0)
            emit shiftWheelUp();
        else
            emit shiftWheelDown();
    } else if (event->modifiers() == Qt::ControlModifier) {
        if (event->angleDelta().y() > 0)
            emit controlWheelUp();
        else
            emit controlWheelDown();
    } else {
        QPlainTextEdit::wheelEvent(event);
        return;
    }

    event->accept();
}

void ResultDisplay::mouseMoveEvent(QMouseEvent* event)
{
    QPlainTextEdit::mouseMoveEvent(event);

    if (m_editingHistoryIndex >= 0) {
        const QRect cancelRect = cancelGlyphBadgeRectForEditingIndex();
        const bool overCancelGlyph = cancelRect.isValid() && cancelRect.contains(event->pos());
        if (overCancelGlyph)
            viewport()->setCursor(Qt::PointingHandCursor);
        else
            viewport()->unsetCursor();

        if (overCancelGlyph)
            QToolTip::showText(QCursor::pos(), tr("Cancel editing"), this);
        else
            QToolTip::hideText();
        return;
    }

    if (!m_hoverHighlightEnabled) {
        if (m_hoveredHistoryIndex >= 0) {
            const int previousHoveredHistoryIndex = m_hoveredHistoryIndex;
            m_hoveredHistoryIndex = -1;
            updateHoverHighlightSelection();
            viewport()->update(hoverActionRectForHistoryIndex(previousHoveredHistoryIndex));
        }
        QToolTip::hideText();
        viewport()->unsetCursor();
        return;
    }

    const int hoveredHistoryIndex = historyIndexAtPosition(event->pos());
    if (hoveredHistoryIndex != m_hoveredHistoryIndex) {
        const int previousHoveredHistoryIndex = m_hoveredHistoryIndex;
        m_hoveredHistoryIndex = hoveredHistoryIndex;
        updateHoverHighlightSelection();
        if (previousHoveredHistoryIndex >= 0)
            viewport()->update(hoverActionRectForHistoryIndex(previousHoveredHistoryIndex));
        if (m_hoveredHistoryIndex >= 0)
            viewport()->update(hoverActionRectForHistoryIndex(m_hoveredHistoryIndex));
    }

    const bool overActionGlyph = m_hoveredHistoryIndex >= 0
        && (removeGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex).contains(event->pos())
            || editGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex).contains(event->pos()));
    if (overActionGlyph)
        viewport()->setCursor(Qt::PointingHandCursor);
    else
        viewport()->unsetCursor();

    if (m_hoveredHistoryIndex >= 0 && editGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex).contains(event->pos())) {
        QToolTip::showText(QCursor::pos(), tr("Edit this expression"), this);
    } else if (m_hoveredHistoryIndex >= 0 && removeGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex).contains(event->pos())) {
        QToolTip::showText(QCursor::pos(), tr("Remove this calculation"), this);
    } else {
        QToolTip::hideText();
    }
}

void ResultDisplay::paintEvent(QPaintEvent* event)
{
    QPlainTextEdit::paintEvent(event);

    if (m_editingHistoryIndex >= 0) {
        const QRect cancelRect = cancelGlyphBadgeRectForEditingIndex();
        if (!cancelRect.isValid())
            return;

        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        painter.drawEllipse(cancelRect);
        painter.setPen(QPen(QColor(200, 50, 0, 255), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const QPointF center = badgeCenter(cancelRect);
        const qreal side = cancelRect.width() * 0.22;
        painter.drawRect(QRectF(center.x() - side, center.y() - side, side * 2.0, side * 2.0));
        return;
    }

    if (!m_hoverHighlightEnabled || m_hoveredHistoryIndex < 0)
        return;

    const QRect removeRect = removeGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
    const QRect editRect = editGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
    if (!removeRect.isValid() || !editRect.isValid())
        return;

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(editRect);
    painter.setPen(QPen(QColor(40, 90, 180, 255), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF editCenter = badgeCenter(editRect);
    const qreal editHalf = editRect.width() * 0.20;
    painter.drawLine(QPointF(editCenter.x() - editHalf, editCenter.y() + editHalf),
                     QPointF(editCenter.x() + editHalf, editCenter.y() - editHalf));
    painter.setPen(QPen(QColor(40, 90, 180, 255), 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(editCenter.x() + editHalf * 0.62, editCenter.y() - editHalf * 0.62),
                     QPointF(editCenter.x() + editHalf * 1.08, editCenter.y() - editHalf * 1.08));

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(removeRect);
    painter.setPen(QPen(QColor(220, 0, 0, 255), 1.8, Qt::SolidLine, Qt::RoundCap));
    const QPointF center = badgeCenter(removeRect);
    const qreal half = removeRect.width() * 0.22;
    painter.drawLine(QPointF(center.x() - half, center.y() - half),
                     QPointF(center.x() + half, center.y() + half));
    painter.drawLine(QPointF(center.x() - half, center.y() + half),
                     QPointF(center.x() + half, center.y() - half));
}

void ResultDisplay::stopActiveScrollingAnimation()
{
    m_scrollTimer.stop();
    m_scrolledLines = 0;
    m_scrollDirection = 0;
}

void ResultDisplay::updateScrollBarStyleSheet()
{
    verticalScrollBar()->setStyleSheet(QString(
        "QScrollBar:vertical {"
        "   border: 0;"
        "   margin: 0 0 0 0;"
        "   background: %1;"
        "   width: 5px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: %2;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   border: 0;"
        "   width: 0;"
        "   height: 0;"
        "   background: %1;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "   background: %1;"
        "}"
    ).arg(m_highlighter->colorForRole(ColorScheme::Background).name(),
          m_highlighter->colorForRole(ColorScheme::ScrollBar).name()));
}

int ResultDisplay::historyIndexAtPosition(const QPoint& pos) const
{
    const QTextCursor cursor = cursorForPosition(pos);
    if (cursor.block().text().trimmed().isEmpty())
        return -1;

    const int blockNumber = cursor.blockNumber();
    if (blockNumber < 0)
        return -1;

    const QList<HistoryEntry> history = Evaluator::instance()->session()->historyToList();
    const Settings* settings = Settings::instance();
    const int resultLineCount = resultLineCountForSettings(settings);
    int currentBlock = 0;
    for (int i = 0; i < history.count(); ++i) {
        if (currentBlock == blockNumber)
            return i;
        ++currentBlock;

        if (!history.at(i).result().isNan()) {
            for (int line = 0; line < resultLineCount; ++line) {
                if (currentBlock == blockNumber)
                    return i;
                ++currentBlock;
            }
        }

        if (currentBlock == blockNumber)
            return i;
        ++currentBlock;
    }

    return -1;
}

bool ResultDisplay::blockRangeForHistoryIndex(int historyIndex, int& startBlock, int& endBlock) const
{
    startBlock = -1;
    endBlock = -1;
    if (historyIndex < 0)
        return false;

    const QList<HistoryEntry> history = Evaluator::instance()->session()->historyToList();
    const Settings* settings = Settings::instance();
    const int resultLineCount = resultLineCountForSettings(settings);
    if (historyIndex >= history.count())
        return false;

    int currentBlock = 0;
    for (int i = 0; i < history.count(); ++i) {
        const int entryStartBlock = currentBlock;
        ++currentBlock; // expression/comment block

        int entryEndBlock = entryStartBlock;
        if (!history.at(i).result().isNan()) {
            entryEndBlock = currentBlock + resultLineCount - 1; // result block(s)
            currentBlock += resultLineCount;
        }

        ++currentBlock; // separator block

        if (i == historyIndex) {
            startBlock = entryStartBlock;
            endBlock = entryEndBlock;
            return true;
        }
    }

    return false;
}

QRect ResultDisplay::removeGlyphRectForHistoryIndex(int historyIndex) const
{
    int startBlock = -1;
    int endBlock = -1;
    if (!blockRangeForHistoryIndex(historyIndex, startBlock, endBlock))
        return QRect();

    QTextBlock start = document()->findBlockByNumber(startBlock);
    QTextBlock end = document()->findBlockByNumber(endBlock);
    if (!start.isValid() || !end.isValid())
        return QRect();

    const QRectF startRect = blockBoundingGeometry(start).translated(contentOffset());
    const QRectF endRect = blockBoundingGeometry(end).translated(contentOffset());
    const int top = qRound(startRect.top());
    const int bottom = qRound(endRect.bottom());
    if (bottom <= top)
        return QRect();

    const int side = qMax(12, fontMetrics().height());
    const int spacing = 4;
    const int rightPadding = 6;
    const int left = viewport()->width() - side - rightPadding;
    const int minLeft = side + spacing;
    if (left < minLeft)
        return QRect();

    return QRect(left, top, side, bottom - top);
}

QRect ResultDisplay::removeGlyphBadgeRectForHistoryIndex(int historyIndex) const
{
    const QRect removeLaneRect = removeGlyphRectForHistoryIndex(historyIndex);
    if (!removeLaneRect.isValid())
        return QRect();

    const int diameter = qMin(removeLaneRect.width(), qMax(12, fontMetrics().height() - 2));
    const int left = removeLaneRect.left() + (removeLaneRect.width() - diameter) / 2;
    const int top = removeLaneRect.top() + (removeLaneRect.height() - diameter) / 2;
    return QRect(left, top, diameter, diameter);
}

QRect ResultDisplay::editGlyphRectForHistoryIndex(int historyIndex) const
{
    const QRect removeRect = removeGlyphRectForHistoryIndex(historyIndex);
    if (!removeRect.isValid())
        return QRect();

    const int spacing = 4;
    const int left = removeRect.left() - removeRect.width() - spacing;
    if (left < 0)
        return QRect();

    return QRect(left, removeRect.top(), removeRect.width(), removeRect.height());
}

QRect ResultDisplay::editGlyphBadgeRectForHistoryIndex(int historyIndex) const
{
    const QRect editLaneRect = editGlyphRectForHistoryIndex(historyIndex);
    if (!editLaneRect.isValid())
        return QRect();

    const int diameter = qMin(editLaneRect.width(), qMax(12, fontMetrics().height() - 2));
    const int left = editLaneRect.left() + (editLaneRect.width() - diameter) / 2;
    const int top = editLaneRect.top() + (editLaneRect.height() - diameter) / 2;
    return QRect(left, top, diameter, diameter);
}

QRect ResultDisplay::hoverActionRectForHistoryIndex(int historyIndex) const
{
    return removeGlyphRectForHistoryIndex(historyIndex).united(editGlyphRectForHistoryIndex(historyIndex));
}

QRect ResultDisplay::cancelGlyphBadgeRectForEditingIndex() const
{
    if (m_editingHistoryIndex < 0)
        return QRect();
    return removeGlyphBadgeRectForHistoryIndex(m_editingHistoryIndex);
}

void ResultDisplay::updateHoverHighlightSelection()
{
    QList<QTextEdit::ExtraSelection> selections;
    if (m_hoveredHistoryIndex < 0 && m_editingHistoryIndex < 0) {
        setExtraSelections(selections);
        return;
    }

    const QColor hoverColor = hoverColorForBackground(
        m_highlighter->colorForRole(ColorScheme::Background));

    auto appendSelectionForHistoryIndex = [this, &selections, &hoverColor](int historyIndex) {
        int startBlock = -1;
        int endBlock = -1;
        if (!blockRangeForHistoryIndex(historyIndex, startBlock, endBlock))
            return;

        for (int blockNumber = startBlock; blockNumber <= endBlock; ++blockNumber) {
            QTextBlock block = document()->findBlockByNumber(blockNumber);
            if (!block.isValid())
                continue;

            QTextLayout* layout = block.layout();
            if (layout == nullptr)
                continue;

            const int lineCount = layout->lineCount();
            if (lineCount <= 0)
                continue;

            const int blockPosition = block.position();
            for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
                const QTextLine line = layout->lineAt(lineIndex);
                QTextEdit::ExtraSelection selection;
                selection.cursor = QTextCursor(document());
                selection.cursor.setPosition(blockPosition + line.textStart());
                selection.format.setProperty(QTextFormat::FullWidthSelection, true);
                selection.format.setBackground(hoverColor);
                selections.append(selection);
            }
        }
    };

    if (m_hoverHighlightEnabled && m_hoveredHistoryIndex >= 0)
        appendSelectionForHistoryIndex(m_hoveredHistoryIndex);
    if (m_editingHistoryIndex >= 0 && (!m_hoverHighlightEnabled || m_editingHistoryIndex != m_hoveredHistoryIndex))
        appendSelectionForHistoryIndex(m_editingHistoryIndex);

    setExtraSelections(selections);
}
