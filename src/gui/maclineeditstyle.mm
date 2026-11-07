// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/maclineeditstyle.h"

#include <QColor>
#include <QFocusFrame>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QStyleHints>
#include <QStyleOptionFrame>

#import <AppKit/AppKit.h>

namespace MacLineEditStyle {

namespace {
QColor colorForAppAppearance(NSColor* color, NSColorSpace* space)
{
    __block QColor result;
    // Resolve the dynamic color for the appearance the app paints with.
    [NSApp.effectiveAppearance performAsCurrentDrawingAppearance:^{
        if (NSColor* converted = [color colorUsingColorSpace:space]) {
            result.setRgbF(converted.redComponent, converted.greenComponent,
                           converted.blueComponent, converted.alphaComponent);
        }
    }];
    return result;
}

bool isNativeLineEdit(const QLineEdit* edit)
{
    // Stylesheet frames draw their own outline and focus border.
    return edit != nullptr && edit->hasFrame() && edit->styleSheet().isEmpty();
}
} // namespace

bool runningWithLiquidGlass()
{
    // Same conditions as Qt's macOS style: built against and running on macOS 26
    // or later, without the compatibility opt-out that macOS 26 still honors.
    static const bool liquidGlass = [] {
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
        const NSInteger major = NSProcessInfo.processInfo.operatingSystemVersion.majorVersion;
        if (major < 26)
            return false;
        if (major < 27) {
            const id optOut = [NSBundle.mainBundle objectForInfoDictionaryKey:@"UIDesignRequiresCompatibility"];
            if (optOut != nil && [optOut boolValue])
                return false;
        }
        return true;
#else
        return false;
#endif
    }();
    return liquidGlass;
}

bool useStyleTextBackground(QPalette& palette)
{
    NSColor* system = NSColor.textBackgroundColor;
    // Same conversions as qt_mac_toQColor() and QMacStyle's PE_FrameLineEdit.
    const QColor paletteColor = colorForAppAppearance(system, NSColorSpace.genericRGBColorSpace);
    const QColor styleColor = colorForAppAppearance(system, NSColorSpace.sRGBColorSpace);
    if (!paletteColor.isValid() || !styleColor.isValid())
        return false;
    for (QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        if (palette.color(group, QPalette::Base) == paletteColor)
            palette.setColor(group, QPalette::Base, styleColor);
    }
    return palette.color(QPalette::Base) == styleColor;
}

QRectF fieldRect(const QWidget* lineEdit)
{
    // QMacStyle insets the Liquid Glass bezel by one point inside the widget.
    return QRectF(lineEdit->rect()).adjusted(1, 1, -1, -1);
}

qreal fieldCornerRadius(const QWidget* lineEdit)
{
    // Measured from the system bezel. QMacStyle picks the control size from
    // these attributes, independently of the line edit height.
    if (lineEdit->testAttribute(Qt::WA_MacMiniSize))
        return 4;
    if (lineEdit->testAttribute(Qt::WA_MacSmallSize))
        return 5;
    return 6;
}

bool drawPrimitive(const QStyle* baseStyle, QStyle::PrimitiveElement element,
                   const QStyleOption* option, QPainter* painter, const QWidget* widget)
{
    if (element != QStyle::PE_PanelLineEdit && element != QStyle::PE_FrameLineEdit)
        return false;
    if (qobject_cast<const QLineEdit*>(widget) == nullptr)
        return false;
    const auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option);
    if (frame == nullptr)
        return false;
    QStyleOptionFrame field(*frame);
    if (!useStyleTextBackground(field.palette))
        return false; // A custom background, which the style handles itself.
    // In Dark mode the style first fills the whole rectangle with Base, which
    // shows square corners around the rounded Liquid Glass bezel. The bezel
    // paints its own system background, as it already does in Light mode.
    if (element == QStyle::PE_PanelLineEdit && field.lineWidth > 0 && runningWithLiquidGlass())
        element = QStyle::PE_FrameLineEdit;
    baseStyle->drawPrimitive(element, &field, painter, widget);
    return true;
}

bool drawFocusFrame(const QStyleOption* option, QPainter* painter, const QWidget* widget)
{
    Q_UNUSED(option);
    // QMacStyle draws a line edit focus ring with square inner corners, made for
    // the square fields before Liquid Glass.
    const auto* focusFrame = qobject_cast<const QFocusFrame*>(widget);
    const auto* edit = focusFrame ? qobject_cast<const QLineEdit*>(focusFrame->widget()) : nullptr;
    if (!isNativeLineEdit(edit) || !runningWithLiquidGlass())
        return false;

    const QPoint origin = edit->mapTo(edit->window(), QPoint())
        - focusFrame->mapTo(focusFrame->window(), QPoint());
    const QRectF inner = fieldRect(edit).translated(origin);
    const qreal innerRadius = fieldCornerRadius(edit);
    const QRectF outer = inner.adjusted(-FocusRingWidth, -FocusRingWidth, FocusRingWidth, FocusRingWidth);
    const qreal outerRadius = innerRadius + FocusRingWidth;
    QPainterPath ring;
    ring.setFillRule(Qt::OddEvenFill);
    ring.addRoundedRect(outer, outerRadius, outerRadius);
    ring.addRoundedRect(inner, innerRadius, innerRadius);

    // Same color as QMacStyle's other focus rings.
    QColor color;
    const CGColorRef indicator = NSColor.keyboardFocusIndicatorColor.CGColor;
    const CGFloat* components = CGColorGetComponents(indicator);
    if (CGColorSpaceGetModel(CGColorGetColorSpace(indicator)) == kCGColorSpaceModelRGB)
        color.setRgbF(components[0], components[1], components[2], components[3]);
    else
        color = edit->palette().color(QPalette::Highlight);
    if (QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Dark)
        color.setAlphaF(0.39);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillPath(ring, color);
    painter->restore();
    return true;
}

} // namespace MacLineEditStyle
