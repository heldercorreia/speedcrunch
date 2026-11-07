// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_TOOLTIPSTYLEUTILS_H
#define GUI_TOOLTIPSTYLEUTILS_H

#include <QColor>
#include <QMargins>
#include <QPalette>
#include <QPoint>
#include <QString>
#include <Qt>

class QLabel;
class QFrame;
class QSize;
class QTreeWidget;
class QWidget;

namespace ToolTipStyleUtils {

struct PopupTheme {
    QColor background;
    QColor foreground;
    QColor outline;
    int cornerRadius = 0;
};

struct TreePopupTheme {
    QColor background;
    QColor foreground;
    QColor scrollbarThumb;
    QColor scrollbarThumbForeground;
    QColor selectedRow;
    QColor selectedRowForeground;
    QColor outline;
    int cornerRadius = 0;
};

// A theme with an invalid background color means "use the system style".
void applyRoundedPopupMask(QWidget* popup, int cornerRadius);
QPalette nativeToolTipPalette();
void applyNativeToolTipMask(QWidget* widget);
void paintNativeToolTipPanel(QWidget* widget);
// Draws smooth rounded corners instead of cutting them with a mask. This
// needs a see-through popup (Qt::WA_TranslucentBackground set before it is
// first shown). A radius of 0 turns it off.
void setSmoothPopupCorners(QWidget* popup, int cornerRadius);
void applyPopupTheme(QFrame* popup,
                     QLabel* label,
                     QWidget* paletteSource,
                     const PopupTheme& theme);
void applyTreePopupTheme(QTreeWidget* popup, const TreePopupTheme& theme);
QPoint constrainedPopupPosition(QWidget* anchor,
                                const QPoint& globalPos,
                                const QSize& popupSize);
QFrame* createPopup(QWidget* parent,
                    const QString& popupObjectName,
                    const QString& labelObjectName,
                    Qt::TextFormat textFormat,
                    QLabel** label,
                    bool transparentForMouseEvents = false,
                    const QMargins& contentsMargins = QMargins(6, 4, 6, 4));
QString labelToolTipStyleSheet(const QString& selector,
                               const QColor& background,
                               const QColor& foreground,
                               const QColor& outline,
                               int cornerRadius);
void showPopup(QFrame* popup,
               QLabel* label,
               const QString& text,
               QWidget* anchor,
               const QPoint& globalPos,
               int cornerRadius);

}

#endif
