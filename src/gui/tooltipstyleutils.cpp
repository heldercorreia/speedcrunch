// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/tooltipstyleutils.h"

#include "gui/menustyleutils.h"
#include "gui/uiconfig.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QBitmap>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProxyStyle>
#include <QScreen>
#include <QStyleOption>
#include <QStylePainter>
#include <QToolTip>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

namespace {

const char* const NativeToolTipProperty = "speedcrunchNativeToolTip";
const char* const PopupMarginsProperty = "speedcrunchPopupMargins";
const char* const SmoothCornerRadiusProperty = "speedcrunchSmoothCornerRadius";
const char* const SmoothCornerPainterName = "speedcrunchSmoothCornerPainter";

bool usesNativeToolTipStyle(const QWidget* widget)
{
    return widget->property(NativeToolTipProperty).toBool();
}

int smoothCornerRadius(const QWidget* widget)
{
    return widget->property(SmoothCornerRadiusProperty).toInt();
}

// Paints the popup background as a rounded rectangle with smooth edges. The
// window is see-through, so the corners blend with what is behind it.
class SmoothCornerPainter : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() == QEvent::Paint) {
            auto* widget = static_cast<QWidget*>(object);
            const int radius = smoothCornerRadius(widget);
            if (radius > 0) {
                QPainter painter(widget);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(Qt::NoPen);
                painter.setBrush(widget->palette().brush(QPalette::Base));
                painter.drawRoundedRect(QRectF(widget->rect()), radius, radius);
            }
        }
        return false;
    }
};

// Draws like QToolTip when the system style is selected.
class PopupFrame : public QFrame {
public:
    PopupFrame(QWidget* parent, Qt::WindowFlags flags) : QFrame(parent, flags) {}

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if (usesNativeToolTipStyle(this))
            ToolTipStyleUtils::paintNativeToolTipPanel(this);
        else
            QFrame::paintEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QFrame::resizeEvent(event);
        if (usesNativeToolTipStyle(this))
            ToolTipStyleUtils::applyNativeToolTipMask(this);
    }
};

#ifdef Q_OS_MACOS
// Qt's macOS style fills a selected row from edge to edge. Native macOS
// popups show a rounded, inset selection in the accent color instead.
class MacPopupListStyle : public QProxyStyle {
public:
    // Like native menus, show the selection in the accent color while the
    // popup is open, with the matching text color.
    static void useAccentSelection(QPalette& palette)
    {
        for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive}) {
            palette.setColor(group, QPalette::Highlight,
                             palette.color(QPalette::Active, QPalette::Accent));
            palette.setColor(group, QPalette::HighlightedText,
                             palette.color(QPalette::Active, QPalette::HighlightedText));
        }
    }

    void polish(QWidget* widget) override
    {
        QProxyStyle::polish(widget);
        // A parent's stylesheet can put back an older palette when it styles
        // the list again. Set the colors each time the list is styled.
        if (qobject_cast<QAbstractItemView*>(widget) != nullptr) {
            QPalette palette = widget->palette();
            useAccentSelection(palette);
            if (palette != widget->palette())
                widget->setPalette(palette);
        }
    }
    using QProxyStyle::polish;

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                       QPainter* painter, const QWidget* widget) const override
    {
        const auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option);
        if (item != nullptr && (item->state & State_Selected)
            && (element == PE_PanelItemViewItem || element == PE_PanelItemViewRow)) {
            // The row is drawn one cell at a time, so draw each part of the bar.
            if (element == PE_PanelItemViewItem)
                drawSelection(*item, painter, widget);
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    QRect subElementRect(SubElement element, const QStyleOption* option,
                         const QWidget* widget) const override
    {
        // Move the icon and text inside the selection bar.
        const auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option);
        if (item != nullptr
            && (element == SE_ItemViewItemText || element == SE_ItemViewItemDecoration
                || element == SE_ItemViewItemCheckIndicator
                || element == SE_ItemViewItemFocusRect)) {
            QStyleOptionViewItem padded(*item);
            const Ends ends = outerEnds(*item);
            padded.rect.adjust(ends.left ? ContentPadding : 0, 0,
                               ends.right ? -ContentPadding : 0, 0);
            return QProxyStyle::subElementRect(element, &padded, widget);
        }
        return QProxyStyle::subElementRect(element, option, widget);
    }

    QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size,
                           const QWidget* widget) const override
    {
        QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type == CT_ItemViewItem) {
            if (const auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option)) {
                const Ends ends = outerEnds(*item);
                result.rwidth() += (ends.left ? ContentPadding : 0)
                    + (ends.right ? ContentPadding : 0);
            }
        }
        return result;
    }

private:
    // Space between the list edge and the bar, and between the bar and its text.
    static constexpr int BarInset = 5;
    static constexpr int ContentPadding = BarInset + 6;
    static constexpr qreal BarRadius = 5;

    struct Ends {
        bool left;
        bool right;
    };

    // Says which cells hold the left and right ends of the row.
    static Ends outerEnds(const QStyleOptionViewItem& item)
    {
        bool first = item.viewItemPosition != QStyleOptionViewItem::Middle
            && item.viewItemPosition != QStyleOptionViewItem::End;
        bool last = item.viewItemPosition != QStyleOptionViewItem::Middle
            && item.viewItemPosition != QStyleOptionViewItem::Beginning;
        if (item.direction == Qt::RightToLeft)
            std::swap(first, last);
        return {first, last};
    }

    static void drawSelection(const QStyleOptionViewItem& item, QPainter* painter,
                              const QWidget* widget)
    {
        const bool enabled = widget != nullptr ? widget->isEnabled()
                                               : bool(item.state & State_Enabled);
        const QPalette::ColorGroup group = !enabled ? QPalette::Disabled
            : (item.state & State_Active) ? QPalette::Active : QPalette::Inactive;
        const Ends ends = outerEnds(item);
        const QRectF cell = QRectF(item.rect).adjusted(ends.left ? BarInset : 0, 1,
                                                       ends.right ? -BarInset : 0, -1);
        // Round only the outer ends. Inner edges meet the next cell.
        QPainterPath path;
        path.addRoundedRect(cell.adjusted(ends.left ? 0 : -2 * BarRadius, 0,
                                          ends.right ? 0 : 2 * BarRadius, 0),
                            BarRadius, BarRadius);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setClipRect(cell, Qt::IntersectClip);
        painter->setPen(Qt::NoPen);
        painter->setBrush(item.palette.brush(group, QPalette::Highlight));
        painter->drawPath(path);
        painter->restore();
    }
};
#endif

}

namespace ToolTipStyleUtils {

QPalette nativeToolTipPalette()
{
    QPalette palette = QToolTip::palette();
    // In dark mode macOS gives a see-through white tooltip base with black
    // text, and a forced color mode keeps the platform's tooltip colors. When
    // the tooltip colors do not fit the system palette, use its text area
    // colors, as the bitfield's bit tooltips already showed.
    const QPalette system = QApplication::palette();
    const auto isDark = [](const QColor& background, const QColor& foreground) {
        return background.lightness() < foreground.lightness();
    };
    const QColor base = palette.color(QPalette::ToolTipBase);
    if (base.alpha() < 255
        || isDark(base, palette.color(QPalette::ToolTipText))
            != isDark(system.color(QPalette::Window), system.color(QPalette::WindowText))) {
        for (const QPalette::ColorGroup group : {QPalette::Active,
                                                 QPalette::Inactive,
                                                 QPalette::Disabled}) {
            palette.setColor(group, QPalette::ToolTipBase, system.color(group, QPalette::Base));
            palette.setColor(group, QPalette::ToolTipText, system.color(group, QPalette::Text));
        }
    }
    for (const QPalette::ColorGroup group : {QPalette::Active,
                                             QPalette::Inactive,
                                             QPalette::Disabled}) {
        palette.setColor(group, QPalette::Window, palette.color(group, QPalette::ToolTipBase));
        palette.setColor(group, QPalette::WindowText, palette.color(group, QPalette::ToolTipText));
        palette.setColor(group, QPalette::Text, palette.color(group, QPalette::ToolTipText));
        // Set every role, so a themed parent cannot pass its colors down.
        for (int role = 0; role < QPalette::NColorRoles; ++role) {
            const auto colorRole = static_cast<QPalette::ColorRole>(role);
            if (colorRole != QPalette::NoRole)
                palette.setBrush(group, colorRole, palette.brush(group, colorRole));
        }
    }
    return palette;
}

void applyNativeToolTipMask(QWidget* widget)
{
    if (widget == nullptr)
        return;
    QStyleHintReturnMask frameMask;
    QStyleOption option;
    option.initFrom(widget);
    if (widget->style()->styleHint(QStyle::SH_ToolTip_Mask, &option, widget, &frameMask))
        widget->setMask(frameMask.region);
    else
        widget->clearMask();
}

void paintNativeToolTipPanel(QWidget* widget)
{
    QStylePainter painter(widget);
    QStyleOptionFrame option;
    option.initFrom(widget);
    painter.drawPrimitive(QStyle::PE_PanelTipLabel, option);
}

void applyRoundedPopupMask(QWidget* popup, int cornerRadius)
{
    if (popup == nullptr)
        return;

    if (smoothCornerRadius(popup) > 0) {
        popup->clearMask();
        return;
    }

    if (usesNativeToolTipStyle(popup)) {
        applyNativeToolTipMask(popup);
        return;
    }

    if (cornerRadius > 0) {
        QBitmap mask(popup->size());
        mask.fill(Qt::color0);
        QPainter painter(&mask);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::color1);
        painter.drawRoundedRect(QRectF(mask.rect()).adjusted(0, 0, -1, -1),
                                cornerRadius,
                                cornerRadius);
        popup->setMask(mask);
    } else {
        popup->clearMask();
    }
}

void setSmoothPopupCorners(QWidget* popup, int cornerRadius)
{
    if (popup == nullptr)
        return;

    const bool smooth = cornerRadius > 0 && popup->testAttribute(Qt::WA_TranslucentBackground);
    popup->setProperty(SmoothCornerRadiusProperty, smooth ? cornerRadius : 0);
    if (smooth && popup->findChild<QObject*>(QLatin1String(SmoothCornerPainterName),
                                             Qt::FindDirectChildrenOnly) == nullptr) {
        auto* painter = new SmoothCornerPainter(popup);
        painter->setObjectName(QLatin1String(SmoothCornerPainterName));
        popup->installEventFilter(painter);
    }
    // The painter draws the background. A filled background would cover
    // the see-through corners.
    popup->setAutoFillBackground(!smooth);
    // A see-through window also skips its stylesheet background and border.
    // Paint them whenever the painter does not.
    if (popup->testAttribute(Qt::WA_TranslucentBackground))
        popup->setAttribute(Qt::WA_NoSystemBackground, smooth);
    if (auto* area = qobject_cast<QAbstractScrollArea*>(popup))
        area->viewport()->setAutoFillBackground(!smooth);
    if (smooth)
        popup->clearMask();
    popup->update();
}

QPoint constrainedPopupPosition(QWidget* anchor,
                                const QPoint& globalPos,
                                const QSize& popupSize)
{
    constexpr int kPopupOffsetX = 12;
    constexpr int kPopupOffsetY = 18;

    QPoint pos = globalPos + QPoint(kPopupOffsetX, kPopupOffsetY);
    QScreen* screen = anchor != nullptr ? anchor->screen() : QGuiApplication::primaryScreen();
    if (screen == nullptr)
        return pos;

    const QRect available = screen->availableGeometry();
    if (pos.x() + popupSize.width() > available.right())
        pos.setX(globalPos.x() - popupSize.width() - kPopupOffsetX);
    if (pos.y() + popupSize.height() > available.bottom())
        pos.setY(globalPos.y() - popupSize.height() - kPopupOffsetY);

    pos.setX(qBound(available.left(), pos.x(),
                    qMax(available.left(), available.right() - popupSize.width())));
    pos.setY(qBound(available.top(), pos.y(),
                    qMax(available.top(), available.bottom() - popupSize.height())));
    return pos;
}

QFrame* createPopup(QWidget* parent,
                    const QString& popupObjectName,
                    const QString& labelObjectName,
                    Qt::TextFormat textFormat,
                    QLabel** label,
                    bool transparentForMouseEvents,
                    const QMargins& contentsMargins)
{
    QFrame* popup = new PopupFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint);
    popup->setObjectName(popupObjectName);
    popup->setProperty(PopupMarginsProperty, QVariant::fromValue(contentsMargins));
    popup->setAutoFillBackground(true);
    popup->setAttribute(Qt::WA_ShowWithoutActivating, true);
    popup->setAttribute(Qt::WA_StyledBackground, true);
    popup->setAttribute(Qt::WA_TransparentForMouseEvents, transparentForMouseEvents);
    popup->setFocusPolicy(Qt::NoFocus);
    popup->setFrameStyle(QFrame::NoFrame);

    QVBoxLayout* layout = new QVBoxLayout(popup);
    layout->setContentsMargins(contentsMargins);
    layout->setSpacing(0);

    QLabel* popupLabel = new QLabel(popup);
    popupLabel->setObjectName(labelObjectName);
    popupLabel->setTextFormat(textFormat);
    popupLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    popupLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(popupLabel);

    if (label != nullptr)
        *label = popupLabel;
    return popup;
}

void applyPopupTheme(QFrame* popup,
                     QLabel* label,
                     QWidget* paletteSource,
                     const PopupTheme& theme)
{
    if (popup == nullptr || label == nullptr)
        return;

    if (!theme.background.isValid()) {
        // Use the same palette, font, frame, mask and opacity as QToolTip.
        popup->setProperty(NativeToolTipProperty, true);
        popup->setStyleSheet(QString());
        const QPalette palette = nativeToolTipPalette();
        popup->setPalette(palette);
        label->setPalette(palette);
        popup->setFont(QToolTip::font());
        if (QLayout* layout = popup->layout()) {
            const int margin = 1 + popup->style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth,
                                                               nullptr, popup);
            layout->setContentsMargins(margin + 1, margin, margin, margin);
        }
        popup->setWindowOpacity(
            popup->style()->styleHint(QStyle::SH_ToolTipLabel_Opacity, nullptr, popup) / 255.0);
        applyNativeToolTipMask(popup);
        popup->update();
        return;
    }

    popup->setProperty(NativeToolTipProperty, QVariant());
    popup->setFont(QFont());
    if (QLayout* layout = popup->layout()) {
        const QVariant margins = popup->property(PopupMarginsProperty);
        if (margins.isValid())
            layout->setContentsMargins(margins.value<QMargins>());
    }
    popup->setWindowOpacity(1.0);

    const QPalette sourcePalette = paletteSource != nullptr ? paletteSource->palette() : popup->palette();
    const QColor background = theme.background;
    const QColor foreground = theme.foreground.isValid()
        ? theme.foreground
        : sourcePalette.color(QPalette::ToolTipText);
    const QColor outline = theme.outline.isValid()
        ? theme.outline
        : background;
    const int cornerRadius = qMax(0, theme.cornerRadius);

    QPalette popupPalette = popup->palette();
    for (const QPalette::ColorGroup group : {QPalette::Active,
                                             QPalette::Inactive,
                                             QPalette::Disabled}) {
        popupPalette.setColor(group, QPalette::Window, background);
        popupPalette.setColor(group, QPalette::WindowText, foreground);
    }
    popup->setPalette(popupPalette);

    QPalette labelPalette = label->palette();
    for (const QPalette::ColorGroup group : {QPalette::Active,
                                             QPalette::Inactive,
                                             QPalette::Disabled}) {
        labelPalette.setColor(group, QPalette::WindowText, foreground);
        labelPalette.setColor(group, QPalette::Text, foreground);
    }
    label->setPalette(labelPalette);

    popup->setStyleSheet(QStringLiteral(
        "QFrame#%1 {"
        " background: %3; color: %4;"
        " border: %6px solid %5;"
        " border-radius: %7px;"
        "}"
        "QLabel#%2 {"
        " background: transparent; color: %4;"
        "}")
                             .arg(popup->objectName(),
                                  label->objectName(),
                                  background.name(),
                                  foreground.name(),
                                  outline.name())
                             .arg(UiConfig::PopupOutlineStrokeWidth)
                             .arg(cornerRadius));
    applyRoundedPopupMask(popup, cornerRadius);
}

void applyTreePopupTheme(QTreeWidget* popup, const TreePopupTheme& theme)
{
    if (popup == nullptr)
        return;

    if (!theme.background.isValid() || !theme.foreground.isValid()) {
        // Leave the list to the system style.
        popup->setStyleSheet(QString());
#ifdef Q_OS_MACOS
        if (dynamic_cast<MacPopupListStyle*>(popup->style()) == nullptr) {
            auto* style = new MacPopupListStyle;
            style->setParent(popup);
            popup->setStyle(style);
        }
#endif
        QPalette palette = MenuStyle::systemPalette(popup);
#ifdef Q_OS_MACOS
        MacPopupListStyle::useAccentSelection(palette);
#endif
        popup->setPalette(palette);
        popup->viewport()->setPalette(palette);
        popup->viewport()->setAutoFillBackground(true);
        popup->setCursor(Qt::PointingHandCursor);
        popup->viewport()->setCursor(Qt::PointingHandCursor);
        return;
    }

#ifdef Q_OS_MACOS
    if (auto* style = dynamic_cast<MacPopupListStyle*>(popup->style())) {
        popup->setStyle(nullptr);
        delete style;
    }
#endif

    const QColor scrollbarThumb = theme.scrollbarThumb.isValid()
        ? theme.scrollbarThumb
        : theme.background;
    const QColor scrollbarThumbForeground = theme.scrollbarThumbForeground.isValid()
        ? theme.scrollbarThumbForeground
        : theme.foreground;
    const QColor selectedRow = theme.selectedRow.isValid()
        ? theme.selectedRow
        : scrollbarThumb;
    const QColor selectedRowForeground = theme.selectedRowForeground.isValid()
        ? theme.selectedRowForeground
        : scrollbarThumbForeground;
    const QColor outline = theme.outline.isValid()
        ? theme.outline
        : theme.background;
    const int cornerRadius = qMax(0, theme.cornerRadius);

    QPalette palette = popup->palette();
    for (const QPalette::ColorGroup group : {QPalette::Active,
                                             QPalette::Inactive,
                                             QPalette::Disabled}) {
        palette.setColor(group, QPalette::Base, theme.background);
        palette.setColor(group, QPalette::Window, theme.background);
        palette.setColor(group, QPalette::Text, theme.foreground);
        palette.setColor(group, QPalette::WindowText, theme.foreground);
        palette.setColor(group, QPalette::Highlight, selectedRow);
        palette.setColor(group, QPalette::HighlightedText, selectedRowForeground);
    }
    popup->setPalette(palette);
    popup->viewport()->setPalette(palette);
    popup->viewport()->setAutoFillBackground(true);
    popup->setCursor(Qt::PointingHandCursor);
    popup->viewport()->setCursor(Qt::PointingHandCursor);

    popup->setStyleSheet(QStringLiteral(
        "QTreeWidget {"
        " background: %1; color: %2;"
        " selection-background-color: %3; selection-color: %4;"
        " border: %8px solid %6;"
        " border-radius: %7px;"
        "}"
        "QTreeWidget::item:selected {"
        " background: %3; color: %4;"
        "}"
        "QScrollBar:vertical {"
        " background: %1; border: 0; margin: 0; width: 10px;"
        "}"
        "QScrollBar:horizontal {"
        " background: %1; border: 0; margin: 0; height: 10px;"
        "}"
        "QScrollBar::handle:vertical {"
        " background: %5; border: 0; border-radius: 4px; min-height: 20px;"
        "}"
        "QScrollBar::handle:horizontal {"
        " background: %5; border: 0; border-radius: 4px; min-width: 20px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        " background: %1; border: 0; width: 0; height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical,"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        " background: %1;"
        "}")
        .arg(theme.background.name(),
             theme.foreground.name(),
             selectedRow.name(),
             selectedRowForeground.name(),
             scrollbarThumb.name(),
             outline.name())
        .arg(cornerRadius)
        .arg(UiConfig::PopupOutlineStrokeWidth));
}

QString labelToolTipStyleSheet(const QString& selector,
                               const QColor& background,
                               const QColor& foreground,
                               const QColor& outline,
                               int cornerRadius)
{
    return QStringLiteral(
        "%1 {"
        " background-color: %2;"
        " color: %3;"
        " border: %6px solid %4;"
        " border-radius: %5px;"
        "}")
        .arg(selector,
             background.name(),
             foreground.name(),
             outline.name())
        .arg(qMax(0, cornerRadius))
        .arg(UiConfig::PopupOutlineStrokeWidth);
}

void showPopup(QFrame* popup,
               QLabel* label,
               const QString& text,
               QWidget* anchor,
               const QPoint& globalPos,
               int cornerRadius)
{
    if (popup == nullptr || label == nullptr || text.isEmpty() || anchor == nullptr)
        return;

    label->setText(text);
    popup->adjustSize();
    popup->resize(popup->sizeHint());
    applyRoundedPopupMask(popup, qMax(0, cornerRadius));
    popup->move(constrainedPopupPosition(anchor, globalPos, popup->size()));
    popup->show();
}

}
