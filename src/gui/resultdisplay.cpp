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
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QToolTip>

namespace {
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
}

ResultDisplay::ResultDisplay(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_highlighter(new SyntaxHighlighter(this))
    , m_scrolledLines(0)
    , m_scrollDirection(0)
    , m_isScrollingPageOnly(false)
    , m_hoverHighlightEnabled(true)
    , m_hoveredHistoryIndex(-1)
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

void ResultDisplay::append(const QString& expression, Quantity& value)
{
    ++m_count;

    appendPlainText(expression);
    if (!value.isNan())
        appendPlainText(QLatin1String("= ") + NumberFormatter::format(value));
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
    setExtraSelections(QList<QTextEdit::ExtraSelection>());
    if (previousHoveredHistoryIndex >= 0)
        viewport()->update(removeGlyphRectForHistoryIndex(previousHoveredHistoryIndex));
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
        if (!value.isNan())
            appendPlainText(QLatin1String("= ") + NumberFormatter::format(value));
        appendPlainText(QLatin1String(""));
    }

}

void ResultDisplay::refreshLastHistoryEntry()
{
    clearHoverFeedback();
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

    int lastExprBlock = 0;
    for (int i = 0; i < historyCount - 1; ++i) {
        ++lastExprBlock;
        if (!history.at(i).result().isNan())
            ++lastExprBlock;
        ++lastExprBlock;
    }

    QTextBlock exprBlock = document()->findBlockByNumber(lastExprBlock);
    if (!exprBlock.isValid()) {
        refresh();
        return;
    }

    const HistoryEntry& lastEntry = history.last();
    if (lastEntry.result().isNan())
        return;

    QTextBlock resultBlock = exprBlock.next();
    if (!resultBlock.isValid()) {
        refresh();
        return;
    }

    QTextCursor cursor(resultBlock);
    cursor.setPosition(resultBlock.position());
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(QLatin1String("= ") + NumberFormatter::format(lastEntry.result()));
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
    if (event->button() == Qt::LeftButton && m_hoverHighlightEnabled && m_hoveredHistoryIndex >= 0) {
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
        QAction* removeAboveAction = menu->addAction(tr("Remove All Calculations Above"));
        connect(removeAboveAction, &QAction::triggered, this, [this, historyIndex]() {
            emit removeHistoryEntriesAboveRequested(historyIndex);
        });
        QAction* removeAction = menu->addAction(tr("Remove This Calculation"));
        connect(removeAction, &QAction::triggered, this, [this, historyIndex]() {
            emit removeHistoryEntryRequested(historyIndex);
        });
        QAction* removeBelowAction = menu->addAction(tr("Remove All Calculations Below"));
        connect(removeBelowAction, &QAction::triggered, this, [this, historyIndex]() {
            emit removeHistoryEntriesBelowRequested(historyIndex);
        });
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
        viewport()->update(removeGlyphRectForHistoryIndex(previousHoveredHistoryIndex));
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

    if (!m_hoverHighlightEnabled)
        return;

    const int hoveredHistoryIndex = historyIndexAtPosition(event->pos());
    if (hoveredHistoryIndex != m_hoveredHistoryIndex) {
        const int previousHoveredHistoryIndex = m_hoveredHistoryIndex;
        m_hoveredHistoryIndex = hoveredHistoryIndex;
        updateHoverHighlightSelection();
        if (previousHoveredHistoryIndex >= 0)
            viewport()->update(removeGlyphRectForHistoryIndex(previousHoveredHistoryIndex));
        if (m_hoveredHistoryIndex >= 0)
            viewport()->update(removeGlyphRectForHistoryIndex(m_hoveredHistoryIndex));
    }

    const bool overRemoveGlyph = m_hoveredHistoryIndex >= 0
        && removeGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex).contains(event->pos());
    if (overRemoveGlyph)
        viewport()->setCursor(Qt::PointingHandCursor);
    else
        viewport()->unsetCursor();
}

void ResultDisplay::paintEvent(QPaintEvent* event)
{
    QPlainTextEdit::paintEvent(event);

    if (!m_hoverHighlightEnabled || m_hoveredHistoryIndex < 0)
        return;

    const QRect removeRect = removeGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
    if (!removeRect.isValid())
        return;

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(removeRect);
    painter.setPen(QPen(QColor(220, 0, 0, 255), 1.8, Qt::SolidLine, Qt::RoundCap));
    const QPointF center = removeRect.center();
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
    int currentBlock = 0;
    for (int i = 0; i < history.count(); ++i) {
        if (currentBlock == blockNumber)
            return i;
        ++currentBlock;

        if (!history.at(i).result().isNan()) {
            if (currentBlock == blockNumber)
                return i;
            ++currentBlock;
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
    if (historyIndex >= history.count())
        return false;

    int currentBlock = 0;
    for (int i = 0; i < history.count(); ++i) {
        const int entryStartBlock = currentBlock;
        ++currentBlock; // expression/comment block

        int entryEndBlock = entryStartBlock;
        if (!history.at(i).result().isNan()) {
            entryEndBlock = currentBlock; // result block
            ++currentBlock;
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
    const int rightPadding = 6;
    const int left = viewport()->width() - side - rightPadding;
    if (left < 0)
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

void ResultDisplay::updateHoverHighlightSelection()
{
    QList<QTextEdit::ExtraSelection> selections;
    if (!m_hoverHighlightEnabled || m_hoveredHistoryIndex < 0) {
        setExtraSelections(selections);
        return;
    }

    int startBlock = -1;
    int endBlock = -1;
    if (!blockRangeForHistoryIndex(m_hoveredHistoryIndex, startBlock, endBlock)) {
        setExtraSelections(selections);
        return;
    }

    const QColor hoverColor = hoverColorForBackground(
        m_highlighter->colorForRole(ColorScheme::Background));

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

    setExtraSelections(selections);
}
