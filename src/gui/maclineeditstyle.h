// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GUI_MACLINEEDITSTYLE_H
#define GUI_MACLINEEDITSTYLE_H

#include <QtGlobal>

#ifdef Q_OS_MACOS
#include <QStyle>

class QPainter;
class QPalette;
class QRectF;
class QStyleOption;
class QWidget;

// Corrects how the macOS style draws native line edits with Liquid Glass.
namespace MacLineEditStyle {

bool runningWithLiquidGlass();

// Qt's macOS palette converts the system text background through generic RGB,
// but the macOS style compares a line edit's Base color with the sRGB value of
// the same system color. In Dark mode those differ, and with Liquid Glass the
// style then paints a square, flat field. Gives every Base color that Qt took
// from the system text background the value the style expects. Returns whether
// the Base color of the current color group is the system text background.
bool useStyleTextBackground(QPalette& palette);

// The rounded field outline of a native line edit, in the line edit's coordinates.
QRectF fieldRect(const QWidget* lineEdit);
qreal fieldCornerRadius(const QWidget* lineEdit);
constexpr qreal FocusRingWidth = 3.5;

// Draws line edit panels and frames with the system text background.
// Returns false when the base style should draw the element unchanged.
bool drawPrimitive(const QStyle* baseStyle, QStyle::PrimitiveElement element,
                   const QStyleOption* option, QPainter* painter, const QWidget* widget);

// Draws the focus ring of a native line edit around its rounded field.
// Returns false when the base style should draw the focus frame.
bool drawFocusFrame(const QStyleOption* option, QPainter* painter, const QWidget* widget);

} // namespace MacLineEditStyle
#endif

#endif
