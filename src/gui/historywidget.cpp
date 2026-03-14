// This file is part of the SpeedCrunch project
// Copyright (C) 2007 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2008, 2009, 2011 @heldercorreia
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

#include "gui/historywidget.h"
#include "core/sessionhistory.h"
#include "core/evaluator.h"
#include "core/session.h"
#include "core/settings.h"

#include <QAction>
#include <QEvent>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace {
QString groupedExpressionForHistory(const QString& input)
{
    const Settings* settings = Settings::instance();
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
}
}

HistoryWidget::HistoryWidget(QWidget *parent)
    : QWidget(parent)
    , m_list(new QListWidget(this))
{
    m_list->setAlternatingRowColors(true);
    m_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setCursor(QCursor(Qt::PointingHandCursor));
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(3, 3, 3, 3);
    layout->addWidget(m_list);
    setLayout(layout);

    connect(m_list, SIGNAL(itemActivated(QListWidgetItem *)), SLOT(handleItem(QListWidgetItem *)));
    connect(m_list, SIGNAL(customContextMenuRequested(const QPoint &)),
            SLOT(handleContextMenuRequested(const QPoint &)));

    updateHistory();
}

void HistoryWidget::updateHistory()
{
    QList<HistoryEntry> hist = Evaluator::instance()->session()->historyToList();
    m_list->clear();
    m_list->clearSelection();

    for (int i = 0; i < hist.size(); ++i) {
        const QString expression = hist[i].expr();
        QListWidgetItem* item = new QListWidgetItem(groupedExpressionForHistory(expression));
        item->setData(Qt::UserRole, expression);
        m_list->addItem(item);
    }
    m_list->scrollToBottom();
}

void HistoryWidget::handleItem(QListWidgetItem *item)
{
    m_list->clearSelection();
    emit expressionSelected(item->data(Qt::UserRole).toString());
}

void HistoryWidget::handleContextMenuRequested(const QPoint &position)
{
    const int historyIndex = m_list->indexAt(position).row();
    if (historyIndex < 0)
        return;

    QMenu menu(m_list);
    QAction *removeAboveAction = menu.addAction(tr("Remove All Calculations Above"));
    connect(removeAboveAction, &QAction::triggered, this, [this, historyIndex]() {
        emit removeHistoryEntriesAboveRequested(historyIndex);
    });
    QAction *removeAction = menu.addAction(tr("Remove This Calculation"));
    connect(removeAction, &QAction::triggered, this, [this, historyIndex]() {
        emit removeHistoryEntryRequested(historyIndex);
    });
    QAction *removeBelowAction = menu.addAction(tr("Remove All Calculations Below"));
    connect(removeBelowAction, &QAction::triggered, this, [this, historyIndex]() {
        emit removeHistoryEntriesBelowRequested(historyIndex);
    });
    menu.exec(m_list->viewport()->mapToGlobal(position));
}

void HistoryWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        setLayoutDirection(Qt::LeftToRight);
    else
        QWidget::changeEvent(e);
}
