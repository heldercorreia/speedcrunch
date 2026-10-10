// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/colorscheme.h"
#include "core/evaluator.h"
#include "core/session.h"
#include "core/sessionjsonkeys.h"
#include "core/settings.h"
#include "core/userdefinitions.h"
#include "gui/aboutbox.h"
#include "gui/bitfieldwidget.h"
#include "gui/bookdock.h"
#include "gui/constantswidget.h"
#include "gui/dockliststyle.h"
#include "gui/editor.h"
#include "gui/syntaxhighlighter.h"
#include "gui/functionswidget.h"
#include "gui/gtkmenupalette.h"
#include "gui/historywidget.h"
#include "gui/keypad.h"
#include "gui/mainwindow.h"
#include "gui/menustyleutils.h"
#include "gui/manualwindow.h"
#include "gui/notationandprecisiondialog.h"
#include "gui/oklchutils.h"
#include "gui/resultdisplay.h"
#include "gui/themedlineedit.h"
#include "gui/uiconfig.h"
#include "math/quantity.h"
#include "uitestfixture.h"

#include <QCoreApplication>
#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFocusEvent>
#include <QHeaderView>
#include <QGlyphRun>
#include <QGridLayout>
#include <QHelpEvent>
#include <QImage>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QMainWindow>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMargins>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPixmap>
#include <QProxyStyle>
#include <QPushButton>
#include <QScrollBar>
#include <QScopeGuard>
#include <QShortcut>
#include <QSignalSpy>
#include <QSplitter>
#include <QSplitterHandle>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QSysInfo>
#include <QTabBar>
#include <QTest>
#include <QTextBrowser>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>
#include <QToolButton>
#include <QTranslator>
#include <QTimer>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTreeWidget>
#include <QUrl>

#include <cmath>

namespace {
class TransparentMenuBarTestStyle : public QProxyStyle {
public:
    TransparentMenuBarTestStyle()
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
    {}

    void drawControl(ControlElement element, const QStyleOption* option,
                     QPainter* painter, const QWidget* widget = nullptr) const override
    {
        if (element != CE_MenuBarEmptyArea)
            QProxyStyle::drawControl(element, option, painter, widget);
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                       QPainter* painter, const QWidget* widget = nullptr) const override
    {
        if (element != PE_PanelMenuBar)
            QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

class MenuBarPaletteTestStyle : public QProxyStyle {
public:
    explicit MenuBarPaletteTestStyle(const QPalette& platformPalette)
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
        , m_platformPalette(platformPalette)
    {}

    void polish(QWidget* widget) override
    {
        QProxyStyle::polish(widget);
        // Breeze gives menu bars their own desktop header palette.
        if (qobject_cast<QMenuBar*>(widget))
            widget->setPalette(m_platformPalette);
    }

private:
    QPalette m_platformPalette;
};

class DockHeaderButtonTestStyle : public QProxyStyle {
public:
    explicit DockHeaderButtonTestStyle(bool framed)
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
        , m_framed(framed)
    {}

    int pixelMetric(PixelMetric metric, const QStyleOption* option = nullptr,
                    const QWidget* widget = nullptr) const override
    {
        if (metric == PM_SmallIconSize)
            return 16;
        return QProxyStyle::pixelMetric(metric, option, widget);
    }

    int styleHint(StyleHint hint, const QStyleOption* option = nullptr,
                  const QWidget* widget = nullptr,
                  QStyleHintReturn* returnData = nullptr) const override
    {
        if (hint == SH_DockWidget_ButtonsHaveFrame)
            return m_framed;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }

private:
    bool m_framed;
};

QJsonObject themeJson(QJsonObject colors)
{
    colors.insert(QStringLiteral("$schema"), QString::fromLatin1(ColorScheme::SchemaDraft));
    colors.insert(QStringLiteral("$id"), QString::fromLatin1(ColorScheme::SchemaId));
    return colors;
}

QString themeJsonString(QJsonObject colors)
{
    return QString::fromUtf8(QJsonDocument(themeJson(colors)).toJson(QJsonDocument::Compact));
}

QJsonObject sessionJson(const QString& name)
{
    QJsonObject historyEntry;
    HistoryEntry(QStringLiteral("6*7"), Quantity(42)).serialize(historyEntry);

    QJsonArray history;
    history.append(historyEntry);

    QJsonObject values;
    values.insert(QLatin1String(SessionJsonKeys::Schema), QLatin1String(SessionJsonKeys::SchemaDialect));
    values.insert(QLatin1String(SessionJsonKeys::Id), QLatin1String(SessionJsonKeys::SchemaId));
    values.insert(QLatin1String(SessionJsonKeys::Session), name);
    values.insert(QLatin1String(SessionJsonKeys::Limit), 1000);
    values.insert(QLatin1String(SessionJsonKeys::History), history);
    values.insert(QLatin1String(SessionJsonKeys::Variables), QJsonArray());
    values.insert(QLatin1String(SessionJsonKeys::Functions), QJsonArray());
    values.insert(QLatin1String(SessionJsonKeys::Units), QJsonArray());
    values.insert(QLatin1String(SessionJsonKeys::Globals), QJsonArray());
    return values;
}

void writeFile(const QString& filePath, const QByteArray& data)
{
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(data), qint64(data.size()));
}

QWidget* paneWidgetForDisplay(ResultDisplay* display)
{
    QWidget* widget = display;
    while (widget != nullptr && !qobject_cast<QSplitter*>(widget->parentWidget()))
        widget = widget->parentWidget();
    return widget;
}

void appendPaneScrollValues(const QJsonObject& node, QList<int>* values)
{
    const QString type = node.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("pane")) {
        const QJsonArray tabs = node.value(QStringLiteral("tabs")).toArray();
        for (const QJsonValue& tabValue : tabs) {
            const QJsonObject scroll = tabValue.toObject().value(QStringLiteral("scroll")).toObject();
            if (scroll.contains(QStringLiteral("value")))
                values->append(scroll.value(QStringLiteral("value")).toInt(-1));
        }
        return;
    }

    if (type != QLatin1String("split"))
        return;

    const QJsonArray children = node.value(QStringLiteral("children")).toArray();
    for (const QJsonValue& childValue : children) {
        if (childValue.isObject())
            appendPaneScrollValues(childValue.toObject(), values);
    }
}

void appendPaneEditorTexts(const QJsonObject& node, QStringList* texts)
{
    const QString type = node.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("pane")) {
        const QString active = node.value(QStringLiteral("active")).toString();
        const QJsonArray tabs = node.value(QStringLiteral("tabs")).toArray();
        for (const QJsonValue& tabValue : tabs) {
            const QJsonObject tab = tabValue.toObject();
            if (tab.value(QStringLiteral("name")).toString() != active)
                continue;
            const QJsonObject editor = tab.value(QStringLiteral("editor")).toObject();
            if (editor.contains(QStringLiteral("text")))
                texts->append(editor.value(QStringLiteral("text")).toString());
        }
        return;
    }

    if (type != QLatin1String("split"))
        return;

    const QJsonArray children = node.value(QStringLiteral("children")).toArray();
    for (const QJsonValue& childValue : children) {
        if (childValue.isObject())
            appendPaneEditorTexts(childValue.toObject(), texts);
    }
}

bool menuContainsActionText(const QMenu* menu, const QString& text)
{
    for (QAction* action : menu->actions()) {
        if (action->text() == text)
            return true;
        if (action->menu() != nullptr && menuContainsActionText(action->menu(), text))
            return true;
    }
    return false;
}

QMenu* menuWithTitle(const QMenuBar* menuBar, const QString& title)
{
    for (QAction* action : menuBar->actions()) {
        QMenu* menu = action->menu();
        if (menu != nullptr && menu->title() == title)
            return menu;
    }
    return nullptr;
}

QAction* directMenuActionWithText(const QMenu* menu, const QString& text)
{
    for (QAction* action : menu->actions()) {
        if (action->text() == text)
            return action;
    }
    return nullptr;
}

QMenu* directSubmenuWithTitle(const QMenu* menu, const QString& title)
{
    QAction* action = directMenuActionWithText(menu, title);
    return action != nullptr ? action->menu() : nullptr;
}

QList<MainWindow*> topLevelMainWindows()
{
    QList<MainWindow*> windows;
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (MainWindow* window = qobject_cast<MainWindow*>(widget))
            windows.append(window);
    }
    return windows;
}

bool rejectActiveDialogWithTitle(const QString& title)
{
    QDialog* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (dialog == nullptr)
        return false;

    const bool matches = dialog->windowTitle() == title;
    dialog->reject();
    return matches;
}

class MenuTestResultDisplay : public ResultDisplay {
public:
    explicit MenuTestResultDisplay(QWidget* parent = nullptr)
        : ResultDisplay(parent)
    {
    }

    using ResultDisplay::createContextMenu;
};

class BadgeTestResultDisplay : public ResultDisplay {
public:
    explicit BadgeTestResultDisplay(QWidget* parent = nullptr)
        : ResultDisplay(parent)
    {
    }

    QRect copyBadgeRect(int historyIndex) const
    {
        return copyGlyphBadgeRectForHistoryIndex(historyIndex);
    }

    QRect editBadgeRect(int historyIndex) const
    {
        return editGlyphBadgeRectForHistoryIndex(historyIndex);
    }

    QRect settingsBadgeRect(int historyIndex) const
    {
        return settingsGlyphBadgeRectForHistoryIndex(historyIndex);
    }

    QRect removeBadgeRect(int historyIndex) const
    {
        return removeGlyphBadgeRectForHistoryIndex(historyIndex);
    }
};

bool contextMenuContainsMainMenu(MenuTestResultDisplay* display)
{
    QMenu* menu = display->createContextMenu(display->rect().center());
    const bool mainMenuSeen = menuContainsActionText(menu, QStringLiteral("Main Menu"));
    delete menu;
    return mainMenuSeen;
}

bool editorHasPrimaryOutline(const Editor* editor, const QColor& primary)
{
    return editor != nullptr
        && editor->styleSheet().contains(
            QStringLiteral("border: %1px solid %2")
                .arg(UiConfig::OutlineStrokeWidth)
                .arg(primary.name()));
}

bool anyEditorHasPrimaryOutline(const QList<Editor*>& editors, const QColor& primary)
{
    for (const Editor* editor : editors) {
        if (editorHasPrimaryOutline(editor, primary))
            return true;
    }
    return false;
}

bool colorsAreClose(const QColor& actual, const QColor& expected, int tolerance = 2)
{
    return qAbs(actual.red() - expected.red()) <= tolerance
        && qAbs(actual.green() - expected.green()) <= tolerance
        && qAbs(actual.blue() - expected.blue()) <= tolerance;
}

QRect imageRectForWidgetRect(const QImage& image, const QRect& rect)
{
    const qreal scale = image.devicePixelRatio();
    return QRect(qRound(rect.x() * scale), qRound(rect.y() * scale),
                 qRound(rect.width() * scale), qRound(rect.height() * scale));
}

QPoint firstPixelMatchingColor(const QImage& image, const QRect& rect, const QColor& color, int tolerance)
{
    const QRect bounded = rect.intersected(image.rect());
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            if (colorsAreClose(image.pixelColor(x, y), color, tolerance))
                return QPoint(x, y);
        }
    }
    return QPoint(-1, -1);
}

QPoint firstPixelMatchingColorBlend(const QImage& image, const QRect& rect,
                                  const QColor& foreground, const QColor& background,
                                  int tolerance)
{
    const int redDelta = foreground.red() - background.red();
    const int greenDelta = foreground.green() - background.green();
    const int blueDelta = foreground.blue() - background.blue();
    const int squaredDistance = redDelta * redDelta + greenDelta * greenDelta
        + blueDelta * blueDelta;
    if (squaredDistance == 0)
        return QPoint(-1, -1);

    const QRect bounded = rect.intersected(image.rect());
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            // Antialiased glyphs can contain only partial foreground coverage.
            // Check the blend's hue as well as its contrast with the background.
            const qreal coverage = qreal((pixel.red() - background.red()) * redDelta
                + (pixel.green() - background.green()) * greenDelta
                + (pixel.blue() - background.blue()) * blueDelta) / squaredDistance;
            if (coverage < 0.25 || coverage > 1.0)
                continue;
            const QColor blended(qRound(background.red() + coverage * redDelta),
                                 qRound(background.green() + coverage * greenDelta),
                                 qRound(background.blue() + coverage * blueDelta));
            if (colorsAreClose(pixel, blended, tolerance))
                return QPoint(x, y);
        }
    }
    return QPoint(-1, -1);
}

QPoint firstPixelDistinctFromColor(const QImage& image,
                                   const QRect& rect,
                                   const QColor& color,
                                   int minimumChannelDistance)
{
    const QRect bounded = rect.intersected(image.rect());
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            const int distance = qMax(qAbs(pixel.red() - color.red()),
                                      qMax(qAbs(pixel.green() - color.green()),
                                           qAbs(pixel.blue() - color.blue())));
            if (distance >= minimumChannelDistance)
                return QPoint(x, y);
        }
    }
    return QPoint(-1, -1);
}

class FunctionsTestTranslator : public QTranslator {
public:
    QString translate(const char* context,
                      const char* sourceText,
                      const char* disambiguation = nullptr,
                      int n = -1) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);

        if (qstrcmp(context, "FunctionsWidget") == 0
            && qstrcmp(sourceText, "Domain") == 0) {
            return QStringLiteral("Translated Domain");
        }

        return QString();
    }
};

QPushButton* keypadButtonWithText(Keypad* keypad, const QString& text)
{
    if (keypad == nullptr)
        return nullptr;

    for (QPushButton* button : keypad->findChildren<QPushButton*>()) {
        if (button->text() == text)
            return button;
    }
    return nullptr;
}

QAction* keypadModeAction(MainWindow* window, Settings::KeypadMode mode)
{
    if (window == nullptr)
        return nullptr;

    for (QAction* action : window->findChildren<QAction*>()) {
        if (action->isCheckable()
                && action->data().isValid()
                && action->data().toInt() == static_cast<int>(mode)) {
            return action;
        }
    }
    return nullptr;
}

QColor keypadPrimaryStateBackgroundForTest(const QColor& primary,
                                           const QColor& stateBackground,
                                           const QColor& normalBackground)
{
    Oklch primaryOklch = qColorToOklch(primary);
    const Oklch stateOklch = qColorToOklch(stateBackground);
    const Oklch normalOklch = qColorToOklch(normalBackground);
    const double offset = stateOklch.l - normalOklch.l;
    if (qAbs(offset) < 1e-9)
        return primary;

    primaryOklch.l = qBound(0.0, primaryOklch.l + offset, 1.0);
    return oklchToValidSrgbQColor(primaryOklch);
}

QColor keypadPrimaryHueFillForTest(const QColor& primary,
                                   const QColor& stateBackground,
                                   const QColor& normalBackground,
                                   int primaryPercent)
{
    const double primaryRatio =
        double(qBound(0, primaryPercent, 100)) / 100.0;
    if (primaryRatio <= 0.0)
        return stateBackground;

    const QColor primaryStateBackground =
        keypadPrimaryStateBackgroundForTest(primary, stateBackground, normalBackground);
    if (primaryRatio >= 1.0)
        return primaryStateBackground;

    const Oklch stateOklch = qColorToOklch(stateBackground);
    const Oklch primaryStateOklch = qColorToOklch(primaryStateBackground);
    return oklchToValidSrgbQColor(Oklch {
        stateOklch.l + (primaryStateOklch.l - stateOklch.l) * primaryRatio,
        stateOklch.c + (primaryStateOklch.c - stateOklch.c) * primaryRatio,
        primaryStateOklch.h,
        stateOklch.alpha
    });
}

QImage dockSeparatorPrimitiveImage(QWidget* widget,
                                   QStyle::State state,
                                   const QRect& separatorRect,
                                   const QSize& imageSize)
{
    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QStyleOption option;
    option.rect = separatorRect;
    option.state = state;
    option.palette = widget->palette();

    QPainter painter(&image);
    QApplication::style()->drawPrimitive(QStyle::PE_IndicatorDockWidgetResizeHandle,
                                         &option,
                                         &painter,
                                         widget);
    return image;
}

QImage dockSeparatorPrimitiveImage(QWidget* widget, QStyle::State state, const QSize& size = QSize(32, 8))
{
    return dockSeparatorPrimitiveImage(widget, state, QRect(QPoint(0, 0), size), size);
}

QColor dockSeparatorPrimitiveColor(QWidget* widget, QStyle::State state)
{
    const QImage image = dockSeparatorPrimitiveImage(widget, state);
    return image.pixelColor(image.rect().center());
}

QRect sessionTabPillRect(const QTabBar* tabBar)
{
    return tabBar->tabRect(tabBar->currentIndex()).adjusted(2, 3, -2, 0);
}

QColor selectedSessionTabFillColor(QTabBar* tabBar)
{
    const QRect pill = sessionTabPillRect(tabBar);
    const QImage image = tabBar->grab().toImage();
    return image.pixelColor(pill.left() + 6, pill.center().y());
}

bool selectedSessionTabHasBottomIndicator(QTabBar* tabBar, const QColor& color)
{
    const QRect pill = sessionTabPillRect(tabBar);
    const QImage image = tabBar->grab().toImage();
    const int firstY = qMax(pill.top(), pill.bottom() - UiConfig::ActiveSessionTabIndicatorStrokeWidth);
    for (int y = firstY; y <= pill.bottom(); ++y) {
        for (int x = pill.left() + 4; x <= pill.right() - 4; ++x) {
            if (image.rect().contains(x, y) && colorsAreClose(image.pixelColor(x, y), color))
                return true;
        }
    }
    return false;
}

Editor* editorForDisplay(ResultDisplay* display)
{
    QWidget* page = display != nullptr ? display->parentWidget() : nullptr;
    return page ? page->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

QTabBar* tabBarForDisplay(ResultDisplay* display)
{
    QWidget* pane = paneWidgetForDisplay(display);
    return pane ? pane->findChild<QTabBar*>() : nullptr;
}

QString visibleResultPreviewText(const MainWindow& window)
{
    for (QLabel* label : window.findChildren<QLabel*>()) {
        if (!label->isVisible())
            continue;
        const QString text = label->text();
        if (text.contains(QStringLiteral("Current result:"))
            || text.contains(QStringLiteral("Selection result:"))) {
            return text;
        }
    }
    return QString();
}

struct MainWindowStateGuard {
    Settings* settings = Settings::instance();
    QString oldColorScheme = settings->colorScheme;
    QString oldCustomColorSchemeJson = settings->customColorSchemeJson;
    Settings::MenuAppearance oldMenuAppearance = settings->menuAppearance;
    QString oldSessionLayoutJson = settings->sessionLayoutJson;
    QString oldConstantsDockDomain = settings->constantsDockDomain;
    QString oldConstantsDockSubdomain = settings->constantsDockSubdomain;
    QString oldConstantsDockSearchText = settings->constantsDockSearchText;
    QByteArray oldWindowState = settings->windowState;
    QByteArray oldWindowGeometry = settings->windowGeometry;
    bool oldConstantsDockVisible = settings->constantsDockVisible;
    bool oldFunctionsDockVisible = settings->functionsDockVisible;
    bool oldHistoryDockVisible = settings->historyDockVisible;
    bool oldKeypadVisible = settings->keypadVisible;
    bool oldFormulaBookDockVisible = settings->formulaBookDockVisible;
    bool oldVariablesDockVisible = settings->variablesDockVisible;
    bool oldUserFunctionsDockVisible = settings->userFunctionsDockVisible;
    bool oldUserUnitsDockVisible = settings->userUnitsDockVisible;
    bool oldBitfieldVisible = settings->bitfieldVisible;
    Settings::KeypadMode oldKeypadMode = settings->keypadMode;
    int oldKeypadZoomPercent = settings->keypadZoomPercent;
    bool oldWindowPositionSave = settings->windowPositionSave;
    bool oldStatusBarVisible = settings->statusBarVisible;
    char oldAngleUnit = settings->angleUnit;
    char oldResultFormat = settings->resultFormat;
    int oldResultPrecision = settings->resultPrecision;
    bool oldHasNumberFormatStyleSetting = settings->hasNumberFormatStyleSetting;
    QByteArray oldSkipUpdateCheck = qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
    bool hadSkipUpdateCheck = qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");

    MainWindowStateGuard()
    {
        settings->windowPositionSave = false;
        qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    }

    ~MainWindowStateGuard()
    {
        settings->colorScheme = oldColorScheme;
        settings->customColorSchemeJson = oldCustomColorSchemeJson;
        settings->menuAppearance = oldMenuAppearance;
        settings->sessionLayoutJson = oldSessionLayoutJson;
        settings->constantsDockDomain = oldConstantsDockDomain;
        settings->constantsDockSubdomain = oldConstantsDockSubdomain;
        settings->constantsDockSearchText = oldConstantsDockSearchText;
        settings->windowState = oldWindowState;
        settings->windowGeometry = oldWindowGeometry;
        settings->constantsDockVisible = oldConstantsDockVisible;
        settings->functionsDockVisible = oldFunctionsDockVisible;
        settings->historyDockVisible = oldHistoryDockVisible;
        settings->keypadVisible = oldKeypadVisible;
        settings->formulaBookDockVisible = oldFormulaBookDockVisible;
        settings->variablesDockVisible = oldVariablesDockVisible;
        settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
        settings->userUnitsDockVisible = oldUserUnitsDockVisible;
        settings->bitfieldVisible = oldBitfieldVisible;
        settings->keypadMode = oldKeypadMode;
        settings->keypadZoomPercent = oldKeypadZoomPercent;
        settings->windowPositionSave = oldWindowPositionSave;
        settings->statusBarVisible = oldStatusBarVisible;
        settings->angleUnit = oldAngleUnit;
        settings->resultFormat = oldResultFormat;
        settings->resultPrecision = oldResultPrecision;
        settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
        if (hadSkipUpdateCheck)
            qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
        else
            qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
    }
};

void sendTabDragMouseEvent(QTabBar* tabBar, QEvent::Type type, const QPoint& pos,
                           Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type,
                      QPointF(pos),
                      QPointF(tabBar->mapToGlobal(pos)),
                      button,
                      buttons,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(tabBar, &event);
}

QKeyCombination restoreClosedTabShortcut()
{
    return QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_T);
}

QKeyCombination nextSessionTabShortcut()
{
#if defined(Q_OS_MACOS)
    return QKeyCombination(Qt::ControlModifier | Qt::AltModifier, Qt::Key_Right);
#else
    return QKeyCombination(Qt::ControlModifier, Qt::Key_PageDown);
#endif
}

QKeyCombination previousSessionTabShortcut()
{
#if defined(Q_OS_MACOS)
    return QKeyCombination(Qt::ControlModifier | Qt::AltModifier, Qt::Key_Left);
#else
    return QKeyCombination(Qt::ControlModifier, Qt::Key_PageUp);
#endif
}

bool focusIsWithin(QWidget* target)
{
    QWidget* focused = QApplication::focusWidget();
    if (target == nullptr || focused == nullptr)
        return false;
    if (focused == target || target->isAncestorOf(focused))
        return true;
    if (QAbstractItemView* view = qobject_cast<QAbstractItemView*>(target)) {
        QWidget* viewport = view->viewport();
        return focused == viewport
            || (viewport != nullptr && viewport->isAncestorOf(focused));
    }
    return false;
}

}

class TestDisplayUi : public QObject {
    Q_OBJECT

public slots:
    void captureOpenedUrl(const QUrl& url) { m_capturedUrl = url; }

private slots:
    void initTestCase();
    void init() { UiTestFixture::resetSettings(); }
    void about_box_shows_os_between_application_and_qt_versions();
    void update_checks_follow_build_option();
    void ui_test_fixture_resets_persisted_layout_and_first_run_preference();
    void editing_input_digit_grouping_updates_all_open_editors();
    void number_format_is_shared_and_refreshes_all_input_panes();
    void manual_preserves_text_weights_and_emphasis();
    void color_scheme_roles_exclude_obsolete_scrollbar();
    void color_scheme_reads_optional_display_name();
    void color_scheme_preserves_rose_pine_names_with_ascii_resources();
    void color_scheme_validates_schema_metadata();
    void theme_dialog_preserves_list_scroll_and_fills_role_color_buttons_data();
    void theme_dialog_preserves_list_scroll_and_fills_role_color_buttons();
    void theme_dialog_repeatedly_previews_same_light_and_dark_themes_data();
    void theme_dialog_repeatedly_previews_same_light_and_dark_themes();
    void result_display_insets_viewport_horizontally();
    void result_display_scrollbar_hover_keeps_viewport_width_stable();
    void result_display_hover_action_badges_use_hover_and_primary_colors_data();
    void result_display_hover_action_badges_use_hover_and_primary_colors();
    void result_display_hover_action_badges_trigger_when_clicked();
    void result_display_scroll_to_bottom_button_uses_custom_tooltip();
    void result_display_context_menu_hides_main_menu_when_menu_bar_visible();
    void result_display_copies_entire_calculation_data();
    void result_display_copies_entire_calculation();
    void result_display_copies_generated_simplification_and_formats();
    void result_display_copy_calculation_ignores_empty_areas();
    void bitfield_selected_bit_keeps_primary_fill_while_hovered();
    void bitfield_buttons_use_configured_generated_shades();
    void keypad_buttons_use_custom_themed_tooltips();
    void dock_list_selected_row_keeps_primary_fill_while_hovered();
    void custom_keypad_action_stays_checked_after_dialog_accepts();
    void keypad_power_button_uses_exponent_label_but_inserts_caret();
    void keypad_nth_root_button_inserts_root_data();
    void keypad_nth_root_button_inserts_root();
    void keypad_zoom_round_trip_restores_button_sizes_data();
    void keypad_zoom_round_trip_restores_button_sizes();
    void keypad_fifty_percent_zoom_scales_and_restores_data();
    void keypad_fifty_percent_zoom_scales_and_restores();
    void keypad_fifty_percent_zoom_survives_settings_reload();
    void keypad_input_stays_in_own_window_data();
    void keypad_input_stays_in_own_window();
    void functions_dock_retranslates_domain_label_after_language_change();
    void main_window_applies_primary_role_to_active_editor_and_dock_selection();
    void current_result_tooltip_stays_hidden_after_escape_and_arrow_caret_move();
    void current_result_tooltip_stays_hidden_after_escape_and_mouse_caret_move();
    void current_result_tooltip_hides_when_dragging_splitters();
    void calculation_settings_dialog_matches_notation_precision_layout();
    void main_window_uses_generated_theme_surface_for_chrome_and_editor();
    void menu_bar_keeps_themed_contrast_with_platform_palette_data();
    void menu_bar_keeps_themed_contrast_with_platform_palette();
    void menu_appearance_defaults_to_system_and_persists();
    void menu_bar_restores_native_painting_after_theme_switch_data();
    void menu_bar_restores_native_painting_after_theme_switch();
    void system_menus_restore_style_polished_foreground_and_hover_colors();
    void gtk_menus_use_hover_colors_instead_of_the_list_accent_data();
    void gtk_menus_use_hover_colors_instead_of_the_list_accent();
    void dock_dropdowns_follow_menu_appearance_data();
    void dock_dropdowns_follow_menu_appearance();
    void system_dropdowns_use_the_native_popup_selection_palette_data();
    void system_dropdowns_use_the_native_popup_selection_palette();
    void macos_system_menus_use_cocoa_style_and_selection_colors();
    void editor_context_menu_paints_system_background_data();
    void editor_context_menu_paints_system_background();
    void menu_appearance_switches_all_windows_without_changing_other_controls();
    void system_menus_preserve_platform_roles_and_follow_palette_changes_data();
    void system_menus_preserve_platform_roles_and_follow_palette_changes();
    void precision_menu_editor_uses_system_colors();
    void dock_context_menus_follow_menu_theme_data();
    void dock_context_menus_follow_menu_theme();
    void restored_session_layout_reapplies_generated_theme_surfaces();
    void saved_window_ui_state_overrides_defaults_before_show();
    void visible_window_applies_restored_dock_and_keypad_layout();
    void always_on_top_toggles_preserve_window_geometry();
    void dock_surfaces_use_successive_generated_shades();
    void dock_header_buttons_render_full_size_data();
    void dock_header_buttons_render_full_size();
    void dock_header_buttons_render_full_size_on_first_run_data();
    void dock_header_buttons_render_full_size_on_first_run();
    void dock_header_buttons_stay_centered_in_title_bar_data();
    void dock_header_buttons_stay_centered_in_title_bar();
    void dock_header_buttons_keep_theme_after_redocking_data();
    void dock_header_buttons_keep_theme_after_redocking();
    void formula_book_text_scales_with_zoom_data();
    void formula_book_text_scales_with_zoom();
    void restored_constants_dock_empty_filter_fills_header();
    void dock_scroll_corner_uses_scrollbar_track_fill();
    void dock_separator_style_uses_primary_while_hovered_or_dragged();
    void constants_dock_uses_configured_narrow_minimum_width();
    void f6_cycles_focus_between_editor_and_visible_dock_controls_data();
    void f6_cycles_focus_between_editor_and_visible_dock_controls();
    void f6_cycles_focus_with_another_main_window_visible();
    void f6_cycles_focus_with_another_main_window_visible_data();
    void dock_search_focus_suppresses_editor_primary_outline_across_panes();
    void dock_selection_inserts_into_active_session_pane_after_focus_transfer();
    void clicking_tab_activates_own_pane_in_nested_split_layout();
    void active_pane_survives_window_reactivation_focus_replay();
    void extra_window_activation_restores_own_active_tab_indicator();
    void session_tab_context_menu_opens_on_right_click();
    void focused_dock_search_survives_window_reactivation_focus_replay();
    void focusing_loaded_pane_preserves_its_current_scroll_position();
    void persisting_layout_captures_visible_scroll_positions_for_all_panes();
    void switching_session_tabs_preserves_each_editor_text();
    void session_tabs_show_full_name_tooltip_on_hover();
    void session_tab_navigation_shortcuts_switch_tabs();
    void new_tab_menu_action_and_shortcut_create_session_in_active_pane();
    void new_tab_menu_action_targets_focused_window_when_native_menu_uses_last_window_action();
    void view_dock_menu_tracks_and_changes_only_active_window();
    void keypad_view_menu_tracks_and_changes_only_active_window();
    void status_bar_menu_tracks_and_changes_only_active_window();
    void precision_menu_editor_uses_themed_colors_data();
    void precision_menu_editor_uses_themed_colors();
    void status_bar_visibility_persists_for_every_window_during_shutdown();
    void status_bar_setting_selectors_update_only_active_window();
    void new_session_window_menu_action_copies_layout_with_single_fresh_session();
    void session_open_menu_action_uses_open_dialog();
    void user_definitions_menu_opens_working_dialog_data();
    void user_definitions_menu_opens_working_dialog();
    void user_definitions_hint_follows_theme_contrast_data();
    void user_definitions_hint_follows_theme_contrast();
    void quit_shortcut_triggers_menu_action_from_editor_and_window();
    void clipboard_actions_follow_active_pane_data();
    void clipboard_actions_follow_active_pane();
    void copy_shortcut_preserves_display_selection_in_other_pane();
    void session_open_sessions_folder_menu_action_opens_session_storage();
    void session_import_dialog_opens_valid_json_as_new_tab();
    void session_import_rejects_invalid_json_without_new_tab();
    void session_export_menu_offers_json_without_save_action();
    void session_export_dialog_prefills_session_name_data();
    void session_export_dialog_prefills_session_name();
    void restore_closed_tab_shortcut_restores_last_closed_session_tab();
    void session_tabs_reorder_with_horizontal_drag();
    void closing_and_reopening_docks_keeps_attached_widgets();

private:
    QUrl m_capturedUrl;
};

void TestDisplayUi::initTestCase()
{
    // Fail before a file-operation test can open an unhandled error dialog.
    for (const QString& path : {Settings::getConfigPath(), Settings::getDataPath(),
                                Settings::getCachePath()}) {
        QVERIFY2(QDir().mkpath(path),
                 qPrintable(QStringLiteral("Cannot create test storage at %1").arg(path)));
        QTemporaryFile probe(QDir(path).filePath(QStringLiteral("storage-probe-XXXXXX")));
        QVERIFY2(probe.open(),
                 qPrintable(QStringLiteral("Cannot write test storage at %1: %2")
                                .arg(path, probe.errorString())));
    }
}

void TestDisplayUi::about_box_shows_os_between_application_and_qt_versions()
{
    AboutBox dialog;
    const QTextEdit* textEdit = dialog.findChild<QTextEdit*>();
    QVERIFY(textEdit);

    QString applicationVersion = QStringLiteral("SpeedCrunch " SPEEDCRUNCH_VERSION);
#ifdef SPEEDCRUNCH_PORTABLE
    applicationVersion += QStringLiteral(" Portable");
#else
    QVERIFY(!textEdit->toPlainText().contains(QStringLiteral("Portable")));
#endif
    const QStringList lines = textEdit->toPlainText().split(QLatin1Char('\n'));
    const int versionLine = lines.indexOf(applicationVersion);
    QVERIFY(versionLine >= 0);
    QVERIFY(lines.size() > versionLine + 2);
    const QString osName = QSysInfo::prettyProductName()
        .remove(QLatin1Char('(')).remove(QLatin1Char(')'));
    QCOMPARE(lines.at(versionLine + 1), osName);
    QCOMPARE(lines.at(versionLine + 2), QStringLiteral("Qt " QT_VERSION_STR));
}

void TestDisplayUi::update_checks_follow_build_option()
{
    MainWindowStateGuard guard;
#ifndef SPEEDCRUNCH_ENABLE_UPDATE_CHECKS
    // A disabled build must work without the test-only launch-check bypass.
    qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
#endif

    MainWindow window;
    QCoreApplication::processEvents();
    QVERIFY(QMetaObject::invokeMethod(&window, "retranslateText", Qt::DirectConnection));

    int checkerCount = 0;
    for (QObject* child : window.children()) {
        if (child->inherits("VersionCheck"))
            ++checkerCount;
    }

    QAction* updateAction = nullptr;
    const QString updateText = MainWindow::tr("Check for &Updates");
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->text() == updateText) {
            QVERIFY(!updateAction);
            updateAction = action;
        }
    }

#ifdef SPEEDCRUNCH_ENABLE_UPDATE_CHECKS
    QCOMPARE(checkerCount, 1);
    QVERIFY(updateAction);
    QVERIFY(updateAction->isEnabled());
    QVERIFY(window.metaObject()->indexOfSlot("checkForUpdates()") >= 0);

    bool inHelpMenu = false;
    for (QMenu* menu : window.findChildren<QMenu*>()) {
        if (menu->title() == MainWindow::tr("&Help")
            && menu->actions().contains(updateAction)) {
            inHelpMenu = true;
        }
    }
    QVERIFY(inHelpMenu);
#else
    QCOMPARE(checkerCount, 0);
    QVERIFY(!updateAction);
    QCOMPARE(window.metaObject()->indexOfSlot("checkForUpdates()"), -1);
#endif
}

void TestDisplayUi::editing_input_digit_grouping_updates_all_open_editors()
{
    Settings::instance()->numberFormatStyle = Settings::NumberFormatThreeDigitSpaceDot;
    Settings::instance()->applyNumberFormatStyle();
    MainWindow first;
    MainWindow second;
    QAction* firstAction = first.findChild<QAction*>(QStringLiteral("InputDigitGroupingAction"));
    QAction* secondAction = second.findChild<QAction*>(QStringLiteral("InputDigitGroupingAction"));
    QVERIFY(firstAction);
    QVERIFY(secondAction);
    QVERIFY(firstAction->isCheckable());
    QVERIFY(firstAction->isChecked());
    bool inEditingMenu = false;
    for (QMenu* menu : first.findChildren<QMenu*>()) {
        if (menu->title() == MainWindow::tr("&Editing") && menu->actions().contains(firstAction))
            inEditingMenu = true;
    }
    QVERIFY(inEditingMenu);
    QVERIFY(QMetaObject::invokeMethod(&first, "splitActivePaneRight", Qt::DirectConnection));
    const QList<Editor*> editors = first.findChildren<Editor*>() + second.findChildren<Editor*>();
    QVERIFY(editors.size() >= 3);
    for (Editor* editor : editors)
        editor->setText(QStringLiteral("1000"));
    for (bool enabled : {false, true}) {
        firstAction->trigger();
        QCOMPARE(Settings::instance()->inputDigitGrouping, enabled);
        QCOMPARE(secondAction->isChecked(), enabled);
        for (Editor* editor : editors) {
            bool hasGap = false;
            for (const auto& range : editor->document()->firstBlock().layout()->formats())
                hasGap |= range.format.hasProperty(SyntaxHighlighter::InputDigitSeparator);
            QCOMPARE(hasGap, enabled);
            QCOMPARE(editor->text(), QStringLiteral("1000"));
        }
    }
}

void TestDisplayUi::number_format_is_shared_and_refreshes_all_input_panes()
{
    Settings::instance()->inputDigitGrouping = true;
    MainWindow first;
    MainWindow second;
    QAction* numberFormatAction = nullptr;
    for (QMenu* menu : first.findChildren<QMenu*>()) {
        for (QAction* action : menu->actions()) {
            if (action->text() == MainWindow::tr("Number Format...")) {
                QCOMPARE(menu->title(), MainWindow::tr("Se&ttings"));
                QCOMPARE(menu->actions().first(), action);
                numberFormatAction = action;
            }
        }
    }
    QVERIFY(numberFormatAction);
    QVERIFY(QMetaObject::invokeMethod(&first, "splitActivePaneRight", Qt::DirectConnection));
    const QList<Editor*> editors = first.findChildren<Editor*>() + second.findChildren<Editor*>();
    QVERIFY(editors.size() >= 3);
    for (Editor* editor : editors)
        editor->setText(QStringLiteral("12345678.1234567"));
    for (auto style : {Settings::NumberFormatIndianCommaDot,
                       Settings::NumberFormatThreeDigitUnderscoreDotFraction,
                       Settings::NumberFormatNoGroupingDot}) {
        QTimer::singleShot(0, &first, [style]() {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto* combo = dialog->findChild<QComboBox*>();
            QVERIFY(combo);
            // Select by example because the dialog's ordering differs from the enum.
            const QString example = style == Settings::NumberFormatIndianCommaDot
                ? QStringLiteral("12,34,567.12345")
                : style == Settings::NumberFormatThreeDigitUnderscoreDotFraction
                    ? QStringLiteral("1_234_567.123_45") : QStringLiteral("1234567.12345");
            const int index = combo->findText(example);
            QVERIFY(index >= 0);
            combo->setCurrentIndex(index);
            dialog->accept();
        });
        numberFormatAction->trigger();
        QCOMPARE(Settings::instance()->numberFormatStyle, style);
        const QList<int> expected = style == Settings::NumberFormatIndianCommaDot
            ? QList<int>{0, 2, 4}
            : style == Settings::NumberFormatThreeDigitUnderscoreDotFraction
                ? QList<int>{1, 4, 11, 14} : QList<int>{};
        for (Editor* editor : editors) {
            QList<int> positions;
            for (const auto& range : editor->document()->firstBlock().layout()->formats()) {
                if (range.format.hasProperty(SyntaxHighlighter::InputDigitSeparator)) {
                    for (int pos = range.start; pos < range.start + range.length; ++pos)
                        positions.append(pos);
                }
            }
            QCOMPARE(positions, expected);
            QCOMPARE(editor->text(), QStringLiteral("12345678.1234567"));
        }
    }
}

void TestDisplayUi::ui_test_fixture_resets_persisted_layout_and_first_run_preference()
{
    Settings* settings = Settings::instance();
    settings->sessionLayoutJson = QStringLiteral("{\"windows\":[{\"id\":\"stale-window\"}]}");
    settings->windowState = QByteArray("stale dock state");
    settings->keypadMode = Settings::KeypadModeScientificNarrow;
    settings->hasNumberFormatStyleSetting = false;
    settings->save();

    const QString root = QString::fromUtf8(qgetenv("SPEEDCRUNCH_UI_TEST_STORAGE"));
    QVERIFY(!root.isEmpty());
    for (const QString& path : {Settings::getConfigPath(), Settings::getDataPath(),
                                Settings::getCachePath()})
        QVERIFY(path.startsWith(root + QLatin1Char('/')));

    UiTestFixture::resetSettings();
    QVERIFY(settings->sessionLayoutJson.isEmpty());
    QVERIFY(settings->windowState.isEmpty());
    QVERIFY(settings->hasNumberFormatStyleSetting);
    QVERIFY(!settings->windowPositionSave);
    QSettings persisted(Settings::getConfigPath() + QStringLiteral("/SpeedCrunch.ini"),
                        QSettings::IniFormat);
    QVERIFY(!persisted.contains(QStringLiteral("SpeedCrunch/General/SessionLayoutJson")));

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();
    QCOMPARE(topLevelMainWindows().size(), 1);
    QVERIFY(QApplication::activeModalWidget() == nullptr);
}

void TestDisplayUi::manual_preserves_text_weights_and_emphasis()
{
    const QFont originalFont = QApplication::font();
    const auto restoreFont = qScopeGuard([originalFont]() {
        QApplication::setFont(originalFont);
    });
    QFont boldApplicationFont = originalFont;
    boldApplicationFont.setWeight(QFont::Bold);
    QApplication::setFont(boldApplicationFont);

    ManualWindow manual;
    manual.setHtml(QStringLiteral(
        "<h1>Heading</h1><p>Body text <strong>Bold emphasis</strong> "
        "<em>Italic emphasis</em> <a href=\"qthelp://manual/doc/next.html\">Link</a></p>"));

    const auto textFont = [&manual](const QString& text) {
        return manual.document()->find(text).charFormat().font()
            .resolve(manual.document()->defaultFont());
    };
    QCOMPARE(textFont(QStringLiteral("Body text")).weight(), QFont::Normal);
    QCOMPARE(textFont(QStringLiteral("Link")).weight(), QFont::Normal);
    QCOMPARE(textFont(QStringLiteral("Heading")).weight(), QFont::Bold);
    QCOMPARE(textFont(QStringLiteral("Bold emphasis")).weight(), QFont::Bold);
    QVERIFY(textFont(QStringLiteral("Italic emphasis")).italic());
    QVERIFY(!textFont(QStringLiteral("Body text")).italic());
#ifdef Q_OS_MACOS
    QVERIFY(textFont(QStringLiteral("Body text")).styleStrategy() & QFont::NoSubpixelAntialias);
    QVERIFY(textFont(QStringLiteral("Bold emphasis")).styleStrategy() & QFont::NoSubpixelAntialias);
#endif
}

void TestDisplayUi::color_scheme_roles_exclude_obsolete_scrollbar()
{
    const auto roles = ColorScheme::roleNames();
    bool hasPrimaryRole = false;
    for (const auto& roleEntry : roles) {
        hasPrimaryRole = hasPrimaryRole || roleEntry.first == QStringLiteral("primary");
        QVERIFY(roleEntry.first != QStringLiteral("scrollbar"));
        QVERIFY(roleEntry.first != QStringLiteral("cursor"));
        QVERIFY(roleEntry.first != QStringLiteral("matched"));
    }
    QVERIFY(hasPrimaryRole);

    const ColorScheme scheme = ColorScheme::fromJsonObject(themeJson(QJsonObject{
        {QStringLiteral("background"), QStringLiteral("#1f3229")},
        {QStringLiteral("cursor"), QStringLiteral("#ffff00")},
        {QStringLiteral("scrollbar"), QStringLiteral("#ff00ff")},
        {QStringLiteral("matched"), QStringLiteral("#00ffff")}
    }));
    QVERIFY(scheme.isValid());
    const QJsonObject schemeJson = scheme.toJsonObject();
    QCOMPARE(schemeJson.value(QStringLiteral("$schema")).toString(),
             QString::fromLatin1(ColorScheme::SchemaDraft));
    QCOMPARE(schemeJson.value(QStringLiteral("$id")).toString(),
             QString::fromLatin1(ColorScheme::SchemaId));
    QVERIFY(!schemeJson.contains(QStringLiteral("version")));
    QVERIFY(!schemeJson.contains(QStringLiteral("scheme")));
    QVERIFY(!scheme.hasColorForRole(ColorScheme::Primary));
    QVERIFY(!schemeJson.contains(QStringLiteral("scrollbar")));
    QVERIFY(!schemeJson.contains(QStringLiteral("cursor")));
    QVERIFY(!schemeJson.contains(QStringLiteral("matched")));
    QVERIFY(!schemeJson.contains(QStringLiteral("primary")));

    const ColorScheme schemeWithPrimary = ColorScheme::fromJsonObject(themeJson(QJsonObject{
        {QStringLiteral("background"), QStringLiteral("#1f3229")},
        {QStringLiteral("primary"), QStringLiteral("#abcdef")}
    }));
    QVERIFY(schemeWithPrimary.isValid());
    QVERIFY(schemeWithPrimary.hasColorForRole(ColorScheme::Primary));
    QCOMPARE(schemeWithPrimary.colorForRole(ColorScheme::Primary).name(),
             QStringLiteral("#abcdef"));
    QCOMPARE(schemeWithPrimary.toJsonObject().value(QStringLiteral("primary")).toString(),
             QStringLiteral("#abcdef"));
}

void TestDisplayUi::color_scheme_reads_optional_display_name()
{
    const ColorScheme namedScheme = ColorScheme::fromJsonObject(themeJson(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("  Named Theme  ")},
        {QStringLiteral("background"), QStringLiteral("#1f3229")}
    }));
    QVERIFY(namedScheme.isValid());
    QCOMPARE(namedScheme.displayName(), QStringLiteral("Named Theme"));
    QCOMPARE(namedScheme.toJsonObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("Named Theme"));

    const ColorScheme blankNameScheme = ColorScheme::fromJsonObject(themeJson(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("   ")},
        {QStringLiteral("background"), QStringLiteral("#1f3229")}
    }));
    QVERIFY(blankNameScheme.isValid());
    QVERIFY(blankNameScheme.displayName().isEmpty());
    QVERIFY(!blankNameScheme.toJsonObject().contains(QStringLiteral("name")));

    const ColorScheme nonStringNameScheme = ColorScheme::fromJsonObject(themeJson(QJsonObject{
        {QStringLiteral("name"), 1},
        {QStringLiteral("background"), QStringLiteral("#1f3229")}
    }));
    QVERIFY(nonStringNameScheme.isValid());
    QVERIFY(nonStringNameScheme.displayName().isEmpty());
}

void TestDisplayUi::color_scheme_preserves_rose_pine_names_with_ascii_resources()
{
    const QStringList schemeNames = ColorScheme::enumerate();
    for (const QString& variant : {QStringLiteral("Moon"), QStringLiteral("Dawn")}) {
        const QString schemeName = QStringLiteral("Ros\u00e9 Pine %1").arg(variant);
        const QString resourceName = QStringLiteral("Rose Pine %1").arg(variant);
        const QString resourcePath = QStringLiteral(":/color-schemes/%1.json").arg(resourceName);
        QCOMPARE(schemeNames.count(schemeName), 1);
        QVERIFY(!schemeNames.contains(resourceName));
        QVERIFY(ColorScheme::isBuiltInName(schemeName));
        QVERIFY(!ColorScheme::isBuiltInName(resourceName));
        QCOMPARE(ColorScheme::filePathForName(schemeName), resourcePath);

        const ColorScheme selectedScheme = ColorScheme::loadByName(schemeName);
        const ColorScheme resourceScheme = ColorScheme::loadFromFile(resourcePath);
        QVERIFY(selectedScheme.isValid());
        QVERIFY(resourceScheme.isValid());
        QCOMPARE(selectedScheme.displayName(), schemeName);
        QCOMPARE(selectedScheme.toJsonObject(), resourceScheme.toJsonObject());
    }
}

void TestDisplayUi::color_scheme_validates_schema_metadata()
{
    const QJsonObject baseScheme{
        {QStringLiteral("background"), QStringLiteral("#1f3229")}
    };

    QJsonObject currentSchema = baseScheme;
    currentSchema.insert(QStringLiteral("$schema"), QString::fromLatin1(ColorScheme::SchemaDraft));
    currentSchema.insert(QStringLiteral("$id"), QString::fromLatin1(ColorScheme::SchemaId));
    currentSchema.insert(QStringLiteral("version"), QStringLiteral("1"));
    QVERIFY(ColorScheme::fromJsonObject(currentSchema).isValid());

    QJsonObject futureSchemaId = baseScheme;
    futureSchemaId.insert(QStringLiteral("$schema"), QString::fromLatin1(ColorScheme::SchemaDraft));
    futureSchemaId.insert(QStringLiteral("$id"),
                          QStringLiteral("https://speedcrunch.org/schemas/theme-v2.schema.json"));
    futureSchemaId.insert(QStringLiteral("version"), QStringLiteral("1"));
    QVERIFY(!ColorScheme::fromJsonObject(futureSchemaId).isValid());

    QVERIFY(!ColorScheme::fromJsonObject(baseScheme).isValid());

    QJsonObject missingVersion = baseScheme;
    missingVersion.insert(QStringLiteral("$schema"), QString::fromLatin1(ColorScheme::SchemaDraft));
    missingVersion.insert(QStringLiteral("$id"), QString::fromLatin1(ColorScheme::SchemaId));
    QVERIFY(ColorScheme::fromJsonObject(missingVersion).isValid());

    QJsonObject authorVersion = baseScheme;
    authorVersion.insert(QStringLiteral("$schema"), QString::fromLatin1(ColorScheme::SchemaDraft));
    authorVersion.insert(QStringLiteral("$id"), QString::fromLatin1(ColorScheme::SchemaId));
    authorVersion.insert(QStringLiteral("version"), QStringLiteral("2"));
    QVERIFY(ColorScheme::fromJsonObject(authorVersion).isValid());

    QJsonObject stringVersion = baseScheme;
    stringVersion.insert(QStringLiteral("$schema"), QString::fromLatin1(ColorScheme::SchemaDraft));
    stringVersion.insert(QStringLiteral("$id"), QString::fromLatin1(ColorScheme::SchemaId));
    stringVersion.insert(QStringLiteral("version"), QStringLiteral("1.2"));
    QVERIFY(ColorScheme::fromJsonObject(stringVersion).isValid());

    QJsonObject invalidVersion = baseScheme;
    invalidVersion.insert(QStringLiteral("$schema"), QString::fromLatin1(ColorScheme::SchemaDraft));
    invalidVersion.insert(QStringLiteral("$id"), QString::fromLatin1(ColorScheme::SchemaId));
    invalidVersion.insert(QStringLiteral("version"), 1);
    QVERIFY(!ColorScheme::fromJsonObject(invalidVersion).isValid());

    QJsonObject malformedSchema = baseScheme;
    malformedSchema.insert(QStringLiteral("$schema"), 1);
    malformedSchema.insert(QStringLiteral("$id"), QString::fromLatin1(ColorScheme::SchemaId));
    malformedSchema.insert(QStringLiteral("version"), QStringLiteral("1"));
    QVERIFY(!ColorScheme::fromJsonObject(malformedSchema).isValid());

    QJsonObject missingId = baseScheme;
    missingId.insert(QStringLiteral("$schema"), QString::fromLatin1(ColorScheme::SchemaDraft));
    QVERIFY(!ColorScheme::fromJsonObject(missingId).isValid());
}

void TestDisplayUi::theme_dialog_preserves_list_scroll_and_fills_role_color_buttons_data()
{
    QTest::addColumn<bool>("hasNumberFormatPreference");
    QTest::newRow("first-run") << false;
    QTest::newRow("saved-preference") << true;
}

void TestDisplayUi::theme_dialog_preserves_list_scroll_and_fills_role_color_buttons()
{
    QFETCH(bool, hasNumberFormatPreference);
    Settings* settings = Settings::instance();
    const bool originalPreference = settings->hasNumberFormatStyleSetting;
    const auto restorePreference = qScopeGuard([settings, originalPreference]() {
        settings->hasNumberFormatStyleSetting = originalPreference;
    });
    settings->hasNumberFormatStyleSetting = hasNumberFormatPreference;
    UiTestFixture::resetSettings();
    QVERIFY(settings->hasNumberFormatStyleSetting);
    MainWindowStateGuard guard;

    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{
        {QStringLiteral("background"), QStringLiteral("#123456")}
    });
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QString failure;
    int lightScrollBefore = -1;
    int lightScrollAfter = -1;
    int darkScrollBefore = -1;
    int darkScrollAfter = -1;
    QColor backgroundButtonColor;
    QColor backgroundButtonPixel;
    QColor primaryButtonColor;
    QColor expectedPrimaryButtonColor;

    const auto recordFailure = [&failure](const QString& message) {
        if (failure.isEmpty())
            failure = message;
    };

    QTimer::singleShot(0, &window, [&]() {
        QDialog* dialog = window.findChild<QDialog*>(QStringLiteral("ThemeDialog"));
        if (dialog == nullptr) {
            recordFailure(QStringLiteral("Theme dialog was not found."));
            return;
        }

        const auto finishDialog = qScopeGuard([dialog]() {
            dialog->reject();
        });
        Q_UNUSED(finishDialog);

        QListWidget* lightList = dialog->findChild<QListWidget*>(QStringLiteral("LightThemeList"));
        QListWidget* darkList = dialog->findChild<QListWidget*>(QStringLiteral("DarkThemeList"));
        QPushButton* backgroundButton = dialog->findChild<QPushButton*>(
            QStringLiteral("ThemeColorButton_background"));
        QPushButton* primaryButton = dialog->findChild<QPushButton*>(
            QStringLiteral("ThemeColorButton_primary"));

        if (lightList == nullptr || darkList == nullptr || backgroundButton == nullptr
                || primaryButton == nullptr) {
            recordFailure(QStringLiteral("Theme dialog controls were not found."));
            return;
        }
        const auto constrainListHeight = [](QListWidget* list) {
            const int rowHeight = list->sizeHintForRow(0) > 0
                ? list->sizeHintForRow(0)
                : list->fontMetrics().height() + 6;
            list->setFixedHeight(rowHeight * 3 + list->frameWidth() * 2);
        };
        constrainListHeight(lightList);
        constrainListHeight(darkList);
        if (dialog->layout() != nullptr)
            dialog->layout()->activate();
        QCoreApplication::processEvents();

        const QColor initialBackgroundButtonColor(backgroundButton->text());
        primaryButtonColor = QColor(primaryButton->text());
        expectedPrimaryButtonColor = generatePrimaryFromBackground(initialBackgroundButtonColor);

        if (lightList->verticalScrollBar()->maximum() <= 0
                || darkList->verticalScrollBar()->maximum() <= 0) {
            recordFailure(QStringLiteral("Theme lists are not scrollable: light count %1 max %2, dark count %3 max %4.")
                              .arg(lightList->count())
                              .arg(lightList->verticalScrollBar()->maximum())
                              .arg(darkList->count())
                              .arg(darkList->verticalScrollBar()->maximum()));
            return;
        }

        const auto clickVisibleTheme = [&recordFailure](QListWidget* list) {
            const QModelIndex index = list->indexAt(QPoint(list->viewport()->width() / 2,
                                                           list->viewport()->height() / 2));
            if (!index.isValid()) {
                recordFailure(QStringLiteral("No visible theme item was found for clicking."));
                return false;
            }

            QTest::mouseClick(list->viewport(),
                              Qt::LeftButton,
                              Qt::NoModifier,
                              list->visualRect(index).center());
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            return true;
        };

        lightList->verticalScrollBar()->setValue(qMin(2, lightList->verticalScrollBar()->maximum()));
        QCoreApplication::processEvents();
        lightScrollBefore = lightList->verticalScrollBar()->value();
        if (!clickVisibleTheme(lightList))
            return;
        lightScrollAfter = lightList->verticalScrollBar()->value();

        darkList->verticalScrollBar()->setValue(qMin(2, darkList->verticalScrollBar()->maximum()));
        QCoreApplication::processEvents();
        darkScrollBefore = darkList->verticalScrollBar()->value();
        if (!clickVisibleTheme(darkList))
            return;
        darkScrollAfter = darkList->verticalScrollBar()->value();

        backgroundButtonColor = QColor(backgroundButton->text());
        const QImage buttonImage = backgroundButton->grab().toImage();
        backgroundButtonPixel = buttonImage.pixelColor(buttonImage.width() - 6,
                                                       buttonImage.height() / 2);
    });

    QVERIFY(QMetaObject::invokeMethod(&window, "showCustomThemeDialog", Qt::DirectConnection));
    QVERIFY2(failure.isEmpty(), qPrintable(failure));
    QCOMPARE(lightScrollAfter, lightScrollBefore);
    QCOMPARE(darkScrollAfter, darkScrollBefore);
    QVERIFY(colorsAreClose(backgroundButtonPixel, backgroundButtonColor, 3));
    QCOMPARE(primaryButtonColor.name(), expectedPrimaryButtonColor.name());
}

void TestDisplayUi::theme_dialog_repeatedly_previews_same_light_and_dark_themes_data()
{
    QTest::addColumn<bool>("useKeyboard");
    QTest::addColumn<bool>("acceptTheme");
    QTest::newRow("mouse-accept") << false << true;
    QTest::newRow("mouse-cancel") << false << false;
    QTest::newRow("keyboard-accept") << true << true;
    QTest::newRow("keyboard-cancel") << true << false;
}

void TestDisplayUi::theme_dialog_repeatedly_previews_same_light_and_dark_themes()
{
    QFETCH(bool, useKeyboard);
    QFETCH(bool, acceptTheme);
    MainWindowStateGuard guard;
    Settings* settings = Settings::instance();
    const QString initialSchemeName = QStringLiteral("Duskfox");
    settings->colorScheme = initialSchemeName;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    int previewsChecked = 0;
    QString lastSchemeName;
    QTimer::singleShot(0, &window, [&]() {
        QDialog* dialog = window.findChild<QDialog*>(QStringLiteral("ThemeDialog"));
        QVERIFY(dialog != nullptr);
        auto finishDialog = qScopeGuard([dialog]() { dialog->reject(); });
        Q_UNUSED(finishDialog);

        QListWidget* lightList = dialog->findChild<QListWidget*>(QStringLiteral("LightThemeList"));
        QListWidget* darkList = dialog->findChild<QListWidget*>(QStringLiteral("DarkThemeList"));
        QWidget* previewWidget = dialog->findChild<QWidget*>(QStringLiteral("ThemePreview"));
        QVERIFY(lightList != nullptr);
        QVERIFY(darkList != nullptr);
        QVERIFY(previewWidget != nullptr);
        QPlainTextEdit* resultPreview = nullptr;
        for (QPlainTextEdit* textEdit : previewWidget->findChildren<QPlainTextEdit*>()) {
            if (qobject_cast<Editor*>(textEdit) == nullptr)
                resultPreview = textEdit;
        }
        Editor* editorPreview = previewWidget->findChild<Editor*>();
        QPushButton* backgroundButton = dialog->findChild<QPushButton*>(
            QStringLiteral("ThemeColorButton_background"));
        QVERIFY(resultPreview != nullptr);
        QVERIFY(editorPreview != nullptr);
        QVERIFY(backgroundButton != nullptr);
        QVERIFY(lightList->count() > 0);
        QVERIFY(darkList->count() > 0);
        QCoreApplication::processEvents();

        for (int cycle = 0; cycle < 4; ++cycle) {
            for (QListWidget* list : {lightList, darkList}) {
                QListWidgetItem* item = list->item(0);
                list->scrollToItem(item);
                if (useKeyboard) {
                    list->setFocus();
                    QTest::keyClick(list, Qt::Key_Home);
                    QTest::keyClick(list, Qt::Key_Space);
                } else {
                    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                                      list->visualItemRect(item).center());
                }
                QCoreApplication::processEvents();

                QCOMPARE(list->selectedItems(), QList<QListWidgetItem*>{item});
                QListWidget* otherList = list == lightList ? darkList : lightList;
                QVERIFY(otherList->selectedItems().isEmpty());
                lastSchemeName = item->data(Qt::UserRole).toString();
                const ColorScheme scheme = ColorScheme::loadByName(lastSchemeName);
                QVERIFY(scheme.isValid());
                const QColor background = scheme.colorForRole(ColorScheme::Background);
                QCOMPARE(QColor(backgroundButton->text()), background);
                QCOMPARE(resultPreview->palette().color(QPalette::Base), background);
                const QVector<QColor> shades = generateOklchShades(
                    background, 6, themePolarityForBackground(background));
                QCOMPARE(editorPreview->palette().color(QPalette::Base),
                         shades.at(UiConfig::DockBackgroundShade));
                ++previewsChecked;
            }
        }
        if (acceptTheme) {
            dialog->accept();
            finishDialog.dismiss();
        }
    });

    QVERIFY(QMetaObject::invokeMethod(&window, "showCustomThemeDialog", Qt::DirectConnection));
    QCOMPARE(previewsChecked, 8);
    QCOMPARE(settings->colorScheme, acceptTheme ? lastSchemeName : initialSchemeName);
}

void TestDisplayUi::result_display_insets_viewport_horizontally()
{
    ResultDisplay display;
    display.resize(320, 200);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));

    const QRect viewportGeometry = display.viewport()->geometry();
    QVERIFY(viewportGeometry.left() > 0);
    QVERIFY(display.width() - viewportGeometry.right() - 1 > 0);
}

void TestDisplayUi::result_display_scrollbar_hover_keeps_viewport_width_stable()
{
    ResultDisplay display;
    display.resize(360, 160);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));

    Quantity value;
    for (int i = 0; i < 40; ++i)
        display.append(QStringLiteral("123456789012345678901234567890"), value);

    QScrollBar* scrollBar = display.verticalScrollBar();
    QVERIFY(scrollBar->maximum() > scrollBar->minimum());

    const QRect initialViewportGeometry = display.viewport()->geometry();
    QEvent enterEvent(QEvent::Enter);
    QCoreApplication::sendEvent(scrollBar, &enterEvent);
    QCOMPARE(display.viewport()->geometry(), initialViewportGeometry);

    QEvent leaveEvent(QEvent::Leave);
    QCoreApplication::sendEvent(scrollBar, &leaveEvent);
    QCOMPARE(display.viewport()->geometry(), initialViewportGeometry);
}

void TestDisplayUi::result_display_hover_action_badges_use_hover_and_primary_colors_data()
{
    QTest::addColumn<int>("fontPixelSize");
    QTest::newRow("default-font") << 0;
    QTest::newRow("small-badges") << 12;
    QTest::newRow("even-badge-diameter") << 13;
}

void TestDisplayUi::result_display_hover_action_badges_use_hover_and_primary_colors()
{
    QFETCH(int, fontPixelSize);
    BadgeTestResultDisplay display;
    if (fontPixelSize > 0) {
        QFont font = display.font();
        font.setPixelSize(fontPixelSize);
        display.setFont(font);
    }
    const QColor resultBackground(QStringLiteral("#101820"));
    const QColor hoverColor(QStringLiteral("#2a3038"));
    const QColor primaryColor(QStringLiteral("#79b8ff"));
    const QColor popupBackground(QStringLiteral("#45465f"));
    const QColor popupForeground(QStringLiteral("#f0ecff"));
    const QColor popupOutline(QStringLiteral("#696a80"));
    const QColor expectedBadgeFill = aaForegroundForBackground(hoverColor, 7.0);
    display.setThemeSurfaceColor(resultBackground);
    display.setThemeToolTipColors(popupBackground, popupForeground, popupOutline);
    display.setThemeInteractionColors(hoverColor,
                                      primaryColor,
                                      QColor(QStringLiteral("#111111")),
                                      QColor(QStringLiteral("#eeeeee")),
                                      QColor(QStringLiteral("#222222")),
                                      QColor(QStringLiteral("#ffffff")));
    Session session;
    session.addHistoryEntry(HistoryEntry(QStringLiteral("120 / 8"), Quantity(15)));
    display.setSession(&session);
    display.resize(420, 120);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));

    const QRect copyRect = display.copyBadgeRect(0);
    const QRect editRect = display.editBadgeRect(0);
    QVERIFY(copyRect.isValid());
    QVERIFY(editRect.isValid());

    // A preceding data row can leave the cursor at the target position.
    QTest::mouseMove(display.viewport(), QPoint(1, 1));
    QTest::mouseMove(display.viewport(), QPoint(18, copyRect.center().y()));
    QTRY_VERIFY(display.viewport()->cursor().shape() != Qt::PointingHandCursor);
    QCOMPARE(display.viewport()->toolTip(), QString());
    QImage rowHoverImage = display.viewport()->grab().toImage();
    // The center can lie on a copy-glyph stroke at some font sizes.
    QVERIFY2(firstPixelMatchingColor(rowHoverImage,
                                     imageRectForWidgetRect(rowHoverImage, copyRect),
                                     expectedBadgeFill, 3) != QPoint(-1, -1),
             qPrintable(QStringLiteral("badge fill did not use %1")
                            .arg(expectedBadgeFill.name())));
    const QPoint defaultIconPixel =
        firstPixelMatchingColorBlend(rowHoverImage,
                                     imageRectForWidgetRect(rowHoverImage, copyRect),
                                     hoverColor, expectedBadgeFill, 10);
    QVERIFY2(defaultIconPixel.x() >= 0,
             qPrintable(QStringLiteral("copy glyph did not use hover color %1")
                            .arg(hoverColor.name())));

    QTest::mouseMove(display.viewport(), copyRect.center());
    QTRY_COMPARE(display.viewport()->cursor().shape(), Qt::PointingHandCursor);
    QCOMPARE(display.viewport()->toolTip(), QString());
    QFrame* actionPopup = display.findChild<QFrame*>(QStringLiteral("resultActionPopup"));
    QTRY_VERIFY(actionPopup != nullptr && actionPopup->isVisible());
    QLabel* actionPopupLabel =
        actionPopup->findChild<QLabel*>(QStringLiteral("resultActionPopupLabel"));
    QVERIFY(actionPopupLabel != nullptr);
    QCOMPARE(actionPopupLabel->text(), QStringLiteral("Copy result"));
    QVERIFY(actionPopup->styleSheet().contains(popupBackground.name()));
    QVERIFY(actionPopup->styleSheet().contains(popupForeground.name()));
    QVERIFY(actionPopup->styleSheet().contains(QStringLiteral("border: %1px solid %2")
                                                   .arg(UiConfig::PopupOutlineStrokeWidth)
                                                   .arg(popupOutline.name())));
    QVERIFY(!actionPopup->mask().isEmpty());
    QVERIFY(actionPopup->testAttribute(Qt::WA_TransparentForMouseEvents));
    QImage copyHoverImage = display.viewport()->grab().toImage();
    const QPoint primaryIconPixel =
        firstPixelMatchingColorBlend(copyHoverImage,
                                     imageRectForWidgetRect(copyHoverImage, copyRect),
                                     primaryColor, expectedBadgeFill, 10);
    QVERIFY2(primaryIconPixel.x() >= 0,
             qPrintable(QStringLiteral("hovered copy glyph did not use primary color %1")
                            .arg(primaryColor.name())));
    const QPoint editHoverPixel =
        firstPixelMatchingColorBlend(copyHoverImage,
                                     imageRectForWidgetRect(copyHoverImage, editRect),
                                     hoverColor, expectedBadgeFill, 10);
    QVERIFY2(editHoverPixel.x() >= 0,
             qPrintable(QStringLiteral("non-hovered edit glyph did not keep hover color %1")
                            .arg(hoverColor.name())));

    QTest::mouseMove(display.viewport(), QPoint(18, copyRect.center().y()));
    QTRY_VERIFY(display.viewport()->cursor().shape() != Qt::PointingHandCursor);
    QCOMPARE(display.viewport()->toolTip(), QString());
    QTRY_VERIFY(actionPopup == nullptr || !actionPopup->isVisible());
}

void TestDisplayUi::result_display_copies_entire_calculation_data()
{
    QTest::addColumn<QStringList>("lines");
    QTest::addColumn<bool>("failed");
    QTest::addColumn<int>("precedingEntries");

    QTest::newRow("simple")
        << QStringList({QStringLiteral("120 / 8"), QStringLiteral("= 15")})
        << false << 1;
    QTest::newRow("simplification-and-formats")
        << QStringList({QStringLiteral("1:120:3600"), QStringLiteral("= 4:00:00"),
                        QStringLiteral("= 14400 s"), QStringLiteral("= 1.44 × 10⁴ s")})
        << false << 1;
    QTest::newRow("wrapped-unicode-and-units")
        << QStringList({QStringLiteral("123456789 [m] − 98765432 [m] + 1 [m] + 2 [m]"),
                        QStringLiteral("= 24\u2009567\u2009360 m"),
                        QStringLiteral("= 2.456736 × 10⁷ m")})
        << false << 1;
    QTest::newRow("failed")
        << QStringList({QStringLiteral("1 / 0")}) << true << 1;
    QTest::newRow("displayed-history-limit")
        << QStringList({QStringLiteral("120 / 8"), QStringLiteral("= 15")})
        << false << 801;
}

void TestDisplayUi::result_display_copies_entire_calculation()
{
    QFETCH(QStringList, lines);
    QFETCH(bool, failed);
    QFETCH(int, precedingEntries);
    const QString oldClipboard = QApplication::clipboard()->text();
    const auto restoreClipboard = qScopeGuard([oldClipboard]() {
        QApplication::clipboard()->setText(oldClipboard);
    });

    Session session;
    session.setHistoryLimit(0);
    for (int i = 0; i < precedingEntries; ++i)
        session.addHistoryEntry(HistoryEntry(QStringLiteral("2 + 3"), Quantity(5)));
    HistoryEntry entry(lines.first(), failed ? DMath::nan() : Quantity(15));
    entry.setRenderedLines(lines);
    session.addHistoryEntry(entry);
    session.addHistoryEntry(HistoryEntry(QStringLiteral("7 + 8"), Quantity(15)));

    MenuTestResultDisplay display;
    display.resize(250, 350);
    display.setSession(&session);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));
    QVERIFY(display.toPlainText().contains(lines.join(QLatin1Char('\n'))));

    // Keep a selection in another calculation while copying the clicked one.
    QTextCursor selection(display.document()->firstBlock());
    selection.select(QTextCursor::BlockUnderCursor);
    display.setTextCursor(selection);
    const int selectionStart = display.textCursor().selectionStart();
    const int selectionEnd = display.textCursor().selectionEnd();

    if (qstrcmp(QTest::currentDataTag(), "wrapped-unicode-and-units") == 0) {
        const QTextBlock expressionBlock = display.document()->find(lines.first()).block();
        QVERIFY(expressionBlock.layout()->lineCount() > 1);
    }

    for (const QString& line : lines) {
        QTextCursor clickedLine = display.document()->find(line);
        QVERIFY(!clickedLine.isNull());
        clickedLine.movePosition(QTextCursor::StartOfBlock);
        display.verticalScrollBar()->setValue(clickedLine.blockNumber());
        QCoreApplication::processEvents();
        QScopedPointer<QMenu> menu(display.createContextMenu(display.cursorRect(clickedLine).center()));
        QAction* action = directMenuActionWithText(menu.data(), QStringLiteral("Copy Calculation"));
        QVERIFY(action != nullptr);
        QVERIFY(action->isEnabled());
        QApplication::clipboard()->setText(QStringLiteral("unchanged"));
        action->trigger();
        QCOMPARE(QApplication::clipboard()->text(), lines.join(QLatin1Char('\n')));
        QCOMPARE(display.textCursor().selectionStart(), selectionStart);
        QCOMPARE(display.textCursor().selectionEnd(), selectionEnd);
    }
}

void TestDisplayUi::result_display_copies_generated_simplification_and_formats()
{
    const QString oldClipboard = QApplication::clipboard()->text();
    const auto restoreClipboard = qScopeGuard([oldClipboard]() {
        QApplication::clipboard()->setText(oldClipboard);
    });
    Settings::instance()->simplifyResultExpressions = true;
    Session session;
    Evaluator* evaluator = session.evaluator();
    const QString expression = QStringLiteral("1:120:3600");
    evaluator->setExpression(expression);
    const Quantity value = evaluator->eval();
    QVERIFY2(evaluator->error().isEmpty(), qPrintable(evaluator->error()));
    EvaluationContext context;
    context.main.fmt = 'f';
    context.main.prec = 2;
    context.extras = {{'e', 2}, {'r', -1}};
    session.addHistoryEntry(HistoryEntry(expression, value, evaluator->interpretedExpression(), context));

    MenuTestResultDisplay display;
    display.resize(600, 350);
    display.setSession(&session);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));
    const QString expected = display.toPlainText().trimmed();
    QVERIFY(expected.contains(QStringLiteral("= 4:00:00\n")));
    QCOMPARE(expected.count(QLatin1Char('\n')), 4);

    // A later format change must not change the text copied from older lines.
    Settings::instance()->resultFormat = 'h';
    Settings::instance()->simplifyResultExpressions = false;
    QScopedPointer<QMenu> menu(display.createContextMenu(display.cursorRect(QTextCursor(display.document())).center()));
    QAction* action = directMenuActionWithText(menu.data(), QStringLiteral("Copy Calculation"));
    QVERIFY(action != nullptr);
    action->trigger();
    QCOMPARE(QApplication::clipboard()->text(), expected);
}

void TestDisplayUi::result_display_copy_calculation_ignores_empty_areas()
{
    Session session;
    MenuTestResultDisplay display;
    display.resize(400, 200);
    display.setSession(&session);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));
    {
        QScopedPointer<QMenu> menu(display.createContextMenu(QPoint(20, 20)));
        QVERIFY(directMenuActionWithText(menu.data(), QStringLiteral("Copy Calculation")) == nullptr);
    }

    session.addHistoryEntry(HistoryEntry(QStringLiteral("2 + 3"), Quantity(5)));
    display.refresh();
    QTextCursor separator(display.document()->lastBlock());
    QScopedPointer<QMenu> menu(display.createContextMenu(display.cursorRect(separator).center()));
    QVERIFY(directMenuActionWithText(menu.data(), QStringLiteral("Copy Calculation")) == nullptr);
}

void TestDisplayUi::result_display_hover_action_badges_trigger_when_clicked()
{
    BadgeTestResultDisplay display;
    Session session;
    session.addHistoryEntry(HistoryEntry(QStringLiteral("120 / 8"), Quantity(15)));
    display.setSession(&session);
    display.resize(420, 120);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));

    const QRect copyRect = display.copyBadgeRect(0);
    const QRect editRect = display.editBadgeRect(0);
    const QRect settingsRect = display.settingsBadgeRect(0);
    const QRect removeRect = display.removeBadgeRect(0);
    QVERIFY(copyRect.isValid());
    QVERIFY(editRect.isValid());
    QVERIFY(settingsRect.isValid());
    QVERIFY(removeRect.isValid());

    QApplication::clipboard()->clear();
    QTest::mouseMove(display.viewport(), copyRect.center());
    QTest::mouseClick(display.viewport(), Qt::LeftButton, Qt::NoModifier, copyRect.center());
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("15"));

    QSignalSpy editSpy(&display, &ResultDisplay::editHistoryEntryRequested);
    QTest::mouseMove(display.viewport(), editRect.center());
    QTest::mouseClick(display.viewport(), Qt::LeftButton, Qt::NoModifier, editRect.center());
    QCOMPARE(editSpy.count(), 1);
    QCOMPARE(editSpy.takeFirst().at(0).toInt(), 0);

    QSignalSpy settingsSpy(&display, &ResultDisplay::editHistoryEntryContextRequested);
    QTest::mouseMove(display.viewport(), settingsRect.center());
    QTest::mouseClick(display.viewport(), Qt::LeftButton, Qt::NoModifier, settingsRect.center());
    QCOMPARE(settingsSpy.count(), 1);
    QCOMPARE(settingsSpy.takeFirst().at(0).toInt(), 0);

    QSignalSpy removeSpy(&display, &ResultDisplay::removeHistoryEntryRequested);
    QTest::mouseMove(display.viewport(), removeRect.center());
    QTest::mouseClick(display.viewport(), Qt::LeftButton, Qt::NoModifier, removeRect.center());
    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(removeSpy.takeFirst().at(0).toInt(), 0);
}

void TestDisplayUi::result_display_scroll_to_bottom_button_uses_custom_tooltip()
{
    ResultDisplay display;
    const QColor popupBackground(QStringLiteral("#45465f"));
    const QColor popupForeground(QStringLiteral("#f0ecff"));
    const QColor popupOutline(QStringLiteral("#696a80"));
    display.setThemeToolTipColors(popupBackground, popupForeground, popupOutline);
    display.resize(360, 160);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));

    Quantity value;
    for (int i = 0; i < 40; ++i)
        display.append(QStringLiteral("123456789012345678901234567890"), value);

    QScrollBar* scrollBar = display.verticalScrollBar();
    QVERIFY(scrollBar->maximum() > scrollBar->minimum());
    scrollBar->setValue(scrollBar->minimum());
    QCoreApplication::processEvents();

    QToolButton* scrollToBottomButton =
        display.findChild<QToolButton*>(QStringLiteral("ScrollToBottomButton"));
    QVERIFY(scrollToBottomButton != nullptr);
    QTRY_VERIFY(scrollToBottomButton->isVisible());
    QCOMPARE(scrollToBottomButton->toolTip(), QString());

    const QPoint buttonCenter = scrollToBottomButton->rect().center();
    QMouseEvent moveEvent(QEvent::MouseMove,
                          QPointF(buttonCenter),
                          QPointF(scrollToBottomButton->mapToGlobal(buttonCenter)),
                          Qt::NoButton,
                          Qt::NoButton,
                          Qt::NoModifier);
    QCoreApplication::sendEvent(scrollToBottomButton, &moveEvent);

    QFrame* actionPopup = display.findChild<QFrame*>(QStringLiteral("resultActionPopup"));
    QTRY_VERIFY(actionPopup != nullptr && actionPopup->isVisible());
    QLabel* actionPopupLabel =
        actionPopup->findChild<QLabel*>(QStringLiteral("resultActionPopupLabel"));
    QVERIFY(actionPopupLabel != nullptr);
    QCOMPARE(actionPopupLabel->text(), QStringLiteral("Scroll to bottom"));
    QVERIFY(actionPopup->styleSheet().contains(popupBackground.name()));
    QVERIFY(actionPopup->styleSheet().contains(popupForeground.name()));
    QVERIFY(actionPopup->styleSheet().contains(QStringLiteral("border: %1px solid %2")
                                                   .arg(UiConfig::PopupOutlineStrokeWidth)
                                                   .arg(popupOutline.name())));
    QVERIFY(!actionPopup->mask().isEmpty());
    QVERIFY(actionPopup->testAttribute(Qt::WA_TransparentForMouseEvents));

    QEvent leaveEvent(QEvent::Leave);
    QCoreApplication::sendEvent(scrollToBottomButton, &leaveEvent);
    QTRY_VERIFY(actionPopup == nullptr || !actionPopup->isVisible());
}

void TestDisplayUi::result_display_context_menu_hides_main_menu_when_menu_bar_visible()
{
    MainWindowStateGuard guard;
    guard.settings->menuAppearance = Settings::MenuAppearanceSpeedCrunch;
    QMainWindow window;
    window.menuBar()->addMenu(QStringLiteral("File"))->addAction(QStringLiteral("Dummy"));
    MenuTestResultDisplay* display = new MenuTestResultDisplay(&window);
    display->setThemeInteractionColors(QColor(QStringLiteral("#333333")),
                                       QColor(QStringLiteral("#5588ff")),
                                       QColor(QStringLiteral("#111111")),
                                       QColor(QStringLiteral("#eeeeee")),
                                       QColor(QStringLiteral("#222222")),
                                       QColor(QStringLiteral("#ffffff")));
    window.setCentralWidget(display);
    window.resize(360, 180);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(window.menuBar()->isVisible());
    QVERIFY(!contextMenuContainsMainMenu(display));
    QSignalSpy importSessionSpy(display, &ResultDisplay::importSessionRequested);
    QSignalSpy exportJsonSpy(display, &ResultDisplay::exportSessionJsonRequested);
    QSignalSpy exportPlainTextSpy(display, &ResultDisplay::exportSessionPlainTextRequested);
    QSignalSpy exportHtmlSpy(display, &ResultDisplay::exportSessionHtmlRequested);

    QMenu* menu = display->createContextMenu(display->rect().center());
    QVERIFY(directMenuActionWithText(menu, QStringLiteral("New Tab")) != nullptr);
    QVERIFY(directMenuActionWithText(menu, QStringLiteral("New Session")) == nullptr);
    QAction* importAction = directMenuActionWithText(menu, QStringLiteral("&Import..."));
    QVERIFY(importAction != nullptr);
    QMenu* exportMenu = directSubmenuWithTitle(menu, QStringLiteral("&Export"));
    QVERIFY(exportMenu != nullptr);
    QVERIFY(directMenuActionWithText(menu, QStringLiteral("Import Session")) == nullptr);
    QVERIFY(directMenuActionWithText(menu, QStringLiteral("Export Session")) == nullptr);
    QVERIFY(directMenuActionWithText(exportMenu, QStringLiteral("JSON")) != nullptr);
    QVERIFY(directMenuActionWithText(exportMenu, QStringLiteral("Plain &text")) != nullptr);
    QVERIFY(directMenuActionWithText(exportMenu, QStringLiteral("&HTML")) != nullptr);

    importAction->trigger();
    directMenuActionWithText(exportMenu, QStringLiteral("JSON"))->trigger();
    directMenuActionWithText(exportMenu, QStringLiteral("Plain &text"))->trigger();
    directMenuActionWithText(exportMenu, QStringLiteral("&HTML"))->trigger();
    QCOMPARE(importSessionSpy.size(), 1);
    QCOMPARE(exportJsonSpy.size(), 1);
    QCOMPARE(exportPlainTextSpy.size(), 1);
    QCOMPARE(exportHtmlSpy.size(), 1);

    QVERIFY(menu->styleSheet().contains(QStringLiteral("#111111")));
    QVERIFY(menu->styleSheet().contains(QStringLiteral("#eeeeee")));
    QVERIFY(menu->styleSheet().contains(QStringLiteral("#222222")));
    QVERIFY(menu->styleSheet().contains(QStringLiteral("#ffffff")));
    QVERIFY(menu->styleSheet().contains(QStringLiteral("border-radius: 8px")));
    delete menu;

    window.menuBar()->hide();
    QVERIFY(!window.menuBar()->isVisible());
    QVERIFY(contextMenuContainsMainMenu(display));
}

void TestDisplayUi::bitfield_selected_bit_keeps_primary_fill_while_hovered()
{
    const QColor background(QStringLiteral("#202124"));
    const QColor foreground(QStringLiteral("#d6d8dc"));
    const QColor hoverBackground(QStringLiteral("#4a5568"));
    const QColor hoverForeground(QStringLiteral("#f8fafc"));
    const QColor primaryBackground(QStringLiteral("#2f80ed"));
    const QColor primaryForeground(QStringLiteral("#ffffff"));
    const QColor popupBackground(QStringLiteral("#30384a"));
    const QColor popupForeground(QStringLiteral("#f4f7ff"));
    const QColor popupOutline(QStringLiteral("#596274"));

    BitWidget bit(3);
    bit.setThemeColors(background,
                       foreground,
                       hoverBackground,
                       hoverForeground,
                       primaryBackground,
                       primaryForeground);
    bit.setToolTipThemeColors(popupBackground, popupForeground, popupOutline, 8);
    bit.resize(48, 48);
    bit.show();
    QVERIFY(QTest::qWaitForWindowExposed(&bit));

    const QPoint sampledFill(4, 4);
    QTest::mouseMove(&bit, bit.rect().center());
    QTRY_VERIFY(bit.underMouse());
    QCOMPARE(bit.toolTip(), QString());
    QFrame* summaryPopup = bit.findChild<QFrame*>(QStringLiteral("bitSummaryPopup"));
    QTRY_VERIFY(summaryPopup != nullptr && summaryPopup->isVisible());
    QLabel* summaryPopupLabel =
        summaryPopup->findChild<QLabel*>(QStringLiteral("bitSummaryPopupLabel"));
    QVERIFY(summaryPopupLabel != nullptr);
    QCOMPARE(summaryPopupLabel->text(), QStringLiteral("2<sup>3</sup> = 8"));
    QVERIFY(summaryPopup->styleSheet().contains(popupBackground.name()));
    QVERIFY(summaryPopup->styleSheet().contains(popupForeground.name()));
    QVERIFY(summaryPopup->styleSheet().contains(QStringLiteral("border: %1px solid %2")
                                                    .arg(UiConfig::PopupOutlineStrokeWidth)
                                                    .arg(popupOutline.name())));
    QVERIFY(!summaryPopup->mask().isEmpty());
    QTRY_COMPARE(bit.grab().toImage().pixelColor(sampledFill).name(),
                 hoverBackground.name());

    bit.setState(true);
    QTRY_COMPARE(bit.grab().toImage().pixelColor(sampledFill).name(),
                 primaryBackground.name());
}

void TestDisplayUi::bitfield_buttons_use_configured_generated_shades()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{{QStringLiteral("background"), QStringLiteral("#e5eee8")}});
    settings->bitfieldVisible = true;
    settings->keypadVisible = false;

    const QVector<QColor> shades =
        generateOklchShades(QColor(QStringLiteral("#e5eee8")), 6, ThemePolarity::Light);
    const QVector<QColor> foregrounds = aaForegroundsForBackgrounds(shades);
    const QColor buttonFill = shades.at(UiConfig::BitfieldButtonFillShade);
    const QColor buttonForeground = foregrounds.at(UiConfig::BitfieldButtonFillShade);
    const QColor buttonHoverFill = shades.at(UiConfig::BitfieldButtonHoverFillShade);
    const QColor buttonHoverForeground =
        foregrounds.at(UiConfig::BitfieldButtonHoverFillShade);
    const QColor buttonPressedFill = shades.at(UiConfig::BitfieldButtonPressedFillShade);
    const QColor buttonPressedForeground =
        foregrounds.at(UiConfig::BitfieldButtonPressedFillShade);
    const QColor popupBackground = shades.at(UiConfig::CompletionPopupBackgroundShade);
    const QColor popupForeground = foregrounds.at(UiConfig::CompletionPopupBackgroundShade);
    const QColor popupOutline = shades.at(UiConfig::CompletionPopupOutlineShade);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    BitFieldWidget* bitfield = window.findChild<BitFieldWidget*>();
    QVERIFY(bitfield != nullptr);
    const QList<QPushButton*> buttons = bitfield->findChildren<QPushButton*>();
    QCOMPARE(buttons.size(), 4);
    QPushButton* shiftLeftButton = nullptr;

    for (QPushButton* button : buttons) {
        if (button->text() == QLatin1String("<<"))
            shiftLeftButton = button;

        const QString style = button->styleSheet();
        QCOMPARE(button->toolTip(), QString());
        QCOMPARE(button->palette().color(QPalette::Button).name(), buttonFill.name());
        QCOMPARE(button->palette().color(QPalette::ButtonText).name(),
                 buttonForeground.name());
        QVERIFY(style.contains(QStringLiteral("background-color: %1")
                                   .arg(buttonFill.name())));
        QVERIFY(style.contains(QStringLiteral("color: %1").arg(buttonForeground.name())));
        QVERIFY(style.contains(QStringLiteral("background-color: %1")
                                   .arg(buttonHoverFill.name())));
        QVERIFY(style.contains(QStringLiteral("color: %1").arg(buttonHoverForeground.name())));
        QVERIFY(style.contains(QStringLiteral("background-color: %1")
                                   .arg(buttonPressedFill.name())));
        QVERIFY(style.contains(QStringLiteral("color: %1")
                                   .arg(buttonPressedForeground.name())));
    }

    QVERIFY(shiftLeftButton != nullptr);
    const QPoint buttonCenter = shiftLeftButton->rect().center();
    QMouseEvent moveEvent(QEvent::MouseMove,
                          QPointF(buttonCenter),
                          QPointF(shiftLeftButton->mapToGlobal(buttonCenter)),
                          Qt::NoButton,
                          Qt::NoButton,
                          Qt::NoModifier);
    QCoreApplication::sendEvent(shiftLeftButton, &moveEvent);
    QFrame* summaryPopup =
        bitfield->findChild<QFrame*>(QStringLiteral("bitfieldButtonSummaryPopup"));
    QTRY_VERIFY(summaryPopup != nullptr && summaryPopup->isVisible());
    QLabel* summaryPopupLabel =
        summaryPopup->findChild<QLabel*>(QStringLiteral("bitfieldButtonSummaryPopupLabel"));
    QVERIFY(summaryPopupLabel != nullptr);
    QCOMPARE(summaryPopupLabel->text(), QStringLiteral("Shift bits left"));
    QVERIFY(summaryPopup->styleSheet().contains(popupBackground.name()));
    QVERIFY(summaryPopup->styleSheet().contains(popupForeground.name()));
    QVERIFY(summaryPopup->styleSheet().contains(QStringLiteral("border: %1px solid %2")
                                                    .arg(UiConfig::PopupOutlineStrokeWidth)
                                                    .arg(popupOutline.name())));
    QVERIFY(!summaryPopup->mask().isEmpty());
}

void TestDisplayUi::keypad_buttons_use_custom_themed_tooltips()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{{QStringLiteral("background"), QStringLiteral("#e5eee8")}});
    settings->keypadMode = Settings::KeypadModeBasicWide;
    settings->keypadVisible = true;
    settings->bitfieldVisible = false;

    const QVector<QColor> shades =
        generateOklchShades(QColor(QStringLiteral("#e5eee8")), 6, ThemePolarity::Light);
    const QVector<QColor> foregrounds = aaForegroundsForBackgrounds(shades);
    const QColor popupBackground = shades.at(UiConfig::CompletionPopupBackgroundShade);
    const QColor popupForeground = foregrounds.at(UiConfig::CompletionPopupBackgroundShade);
    const QColor popupOutline = shades.at(UiConfig::CompletionPopupOutlineShade);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    Keypad* keypad = window.findChild<Keypad*>();
    QVERIFY(keypad != nullptr);
    QPushButton* plusButton = keypadButtonWithText(keypad, QStringLiteral("+"));
    QVERIFY(plusButton != nullptr);
    QCOMPARE(plusButton->toolTip(), QString());

    const QPoint buttonCenter = plusButton->rect().center();
    QMouseEvent moveEvent(QEvent::MouseMove,
                          QPointF(buttonCenter),
                          QPointF(plusButton->mapToGlobal(buttonCenter)),
                          Qt::NoButton,
                          Qt::NoButton,
                          Qt::NoModifier);
    QCoreApplication::sendEvent(plusButton, &moveEvent);

    QFrame* summaryPopup = keypad->findChild<QFrame*>(QStringLiteral("keypadSummaryPopup"));
    QTRY_VERIFY(summaryPopup != nullptr && summaryPopup->isVisible());
    QLabel* summaryPopupLabel =
        summaryPopup->findChild<QLabel*>(QStringLiteral("keypadSummaryPopupLabel"));
    QVERIFY(summaryPopupLabel != nullptr);
    QCOMPARE(summaryPopupLabel->text(), QStringLiteral("Addition"));
    QVERIFY(summaryPopup->styleSheet().contains(popupBackground.name()));
    QVERIFY(summaryPopup->styleSheet().contains(popupForeground.name()));
    QVERIFY(summaryPopup->styleSheet().contains(QStringLiteral("border: %1px solid %2")
                                                    .arg(UiConfig::PopupOutlineStrokeWidth)
                                                    .arg(popupOutline.name())));
    QVERIFY(!summaryPopup->mask().isEmpty());
}

void TestDisplayUi::dock_list_selected_row_keeps_primary_fill_while_hovered()
{
    const QColor background(QStringLiteral("#202124"));
    const QColor foreground(QStringLiteral("#d6d8dc"));
    const QColor hoverBackground(QStringLiteral("#4a5568"));
    const QColor hoverForeground(QStringLiteral("#f8fafc"));
    const QColor primaryBackground(QStringLiteral("#2f80ed"));
    const QColor primaryForeground(QStringLiteral("#ffffff"));
    const QColor inactiveBackground(QStringLiteral("#3b4252"));
    const QColor inactiveForeground(QStringLiteral("#eceff4"));

    QTreeWidget table;
    table.setColumnCount(3);
    table.setRootIsDecorated(false);
    table.setSelectionBehavior(QAbstractItemView::SelectRows);
    table.header()->hide();
    DockListStyle::apply(&table);
    table.setProperty("dockListHoverBackground", hoverBackground);
    table.setProperty("dockListHoverForeground", hoverForeground);
    table.setProperty("dockListActiveSelectionBackground", primaryBackground);
    table.setProperty("dockListActiveSelectionForeground", primaryForeground);
    table.setProperty("dockListInactiveSelectionBackground", inactiveBackground);
    table.setProperty("dockListInactiveSelectionForeground", inactiveForeground);
    table.setStyleSheet(QStringLiteral(
        "QAbstractItemView { background-color: %1; color: %2; border: 0; }")
                            .arg(background.name(),
                                 foreground.name()));

    auto* item = new QTreeWidgetItem(&table, QStringList{
        QStringLiteral("x"),
        QStringLiteral("42"),
        QStringLiteral("m")
    });
    table.setColumnWidth(0, 70);
    table.setColumnWidth(1, 70);
    table.setColumnWidth(2, 70);
    table.resize(260, 80);
    table.show();
    QVERIFY(QTest::qWaitForWindowExposed(&table));

    const QModelIndex index = table.indexFromItem(item, 0);
    const QModelIndex secondColumnIndex = table.indexFromItem(item, 1);
    const QRect itemRect = table.visualRect(index);
    const QRect secondColumnRect = table.visualRect(secondColumnIndex);
    QVERIFY(itemRect.isValid());
    QVERIFY(secondColumnRect.isValid());
    const QPoint sampledFill(itemRect.right() - 4, itemRect.center().y());
    const QPoint sampledColumnBoundary(secondColumnRect.left() + 1, itemRect.top() + 2);
    const QPoint sampledRoundedCorner(itemRect.left() + 1, itemRect.top() + 1);

    QTest::mouseMove(table.viewport(), itemRect.center());
    QTRY_COMPARE(table.property("dockListHoveredRow").toInt(), 0);
    QImage hoveredImage = table.viewport()->grab().toImage();
    QVERIFY2(colorsAreClose(hoveredImage.pixelColor(sampledFill), hoverBackground, 24),
             qPrintable(QStringLiteral("hover sample is %1, expected %2")
                            .arg(hoveredImage.pixelColor(sampledFill).name(),
                                 hoverBackground.name())));
    QVERIFY2(colorsAreClose(hoveredImage.pixelColor(sampledColumnBoundary), hoverBackground, 32),
             qPrintable(QStringLiteral("column-boundary hover sample is %1, expected %2")
                            .arg(hoveredImage.pixelColor(sampledColumnBoundary).name(),
                                 hoverBackground.name())));
    QVERIFY(!colorsAreClose(hoveredImage.pixelColor(sampledRoundedCorner), hoverBackground, 8));
    QVERIFY(hoveredImage.pixelColor(sampledFill).name() != primaryBackground.name());

    table.setCurrentItem(item);
    item->setSelected(true);
    table.setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(table.hasFocus());
    QTRY_COMPARE(table.viewport()->grab().toImage().pixelColor(sampledFill).name(),
                 primaryBackground.name());
}

void TestDisplayUi::custom_keypad_action_stays_checked_after_dialog_accepts()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        Settings::KeypadMode oldKeypadMode;
        bool oldKeypadVisible;
        Settings::CustomKeypad oldCustomKeypad;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->keypadMode = oldKeypadMode;
            settings->keypadVisible = oldKeypadVisible;
            settings->customKeypad = oldCustomKeypad;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->keypadMode,
        settings->keypadVisible,
        settings->customKeypad,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->keypadMode = Settings::KeypadModeBasicWide;
    settings->keypadVisible = true;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QAction* basicAction = keypadModeAction(&window, Settings::KeypadModeBasicWide);
    QAction* customAction = keypadModeAction(&window, Settings::KeypadModeCustom);
    QVERIFY(basicAction != nullptr);
    QVERIFY(customAction != nullptr);
    QVERIFY(basicAction->isChecked());

    QTimer::singleShot(0, &window, []() {
        QDialog* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (dialog != nullptr)
            dialog->accept();
    });
    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "setKeypadMode",
                                      Qt::DirectConnection,
                                      Q_ARG(QAction*, customAction)));
    QCoreApplication::processEvents();

    QCOMPARE(settings->keypadMode, Settings::KeypadModeCustom);
    QVERIFY(customAction->isChecked());
    QVERIFY(!basicAction->isChecked());
}

void TestDisplayUi::keypad_power_button_uses_exponent_label_but_inserts_caret()
{
    const QString powerLabel = QString::fromUtf8("xʸ");

    for (const Keypad::LayoutMode layoutMode : {
             Keypad::LayoutModeScientificWide,
             Keypad::LayoutModeScientificNarrow
         }) {
        const QList<Keypad::CustomButtonDescription> presetButtons =
            Keypad::presetCustomButtons(layoutMode, QLatin1Char('.'));
        bool foundPowerButton = false;
        for (const auto& button : presetButtons) {
            QVERIFY(button.label != QStringLiteral("^"));
            if (button.label != powerLabel)
                continue;

            foundPowerButton = true;
            QCOMPARE(button.action, int(Settings::CustomKeypadActionInsertText));
            QCOMPARE(button.text, QStringLiteral("^"));
        }
        QVERIFY(foundPowerButton);
    }

    MainWindowStateGuard guard;
    Settings* settings = Settings::instance();
    settings->keypadMode = Settings::KeypadModeScientificWide;
    settings->keypadVisible = true;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    Keypad* keypad = window.findChild<Keypad*>();
    QVERIFY(keypad != nullptr);
    QPushButton* powerButton = keypadButtonWithText(keypad, powerLabel);
    QVERIFY(powerButton != nullptr);
    QCOMPARE(keypadButtonWithText(keypad, QStringLiteral("^")), nullptr);

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    editor->setText(QStringLiteral("2"));
    editor->setCursorPosition(editor->text().size());

    QTest::mouseClick(powerButton, Qt::LeftButton);
    QTRY_COMPARE(editor->text(), QStringLiteral("2^"));
}

void TestDisplayUi::keypad_nth_root_button_inserts_root_data()
{
    QTest::addColumn<int>("layoutMode");
    QTest::addColumn<bool>("custom");
    QTest::newRow("scientific-wide") << int(Keypad::LayoutModeScientificWide) << false;
    QTest::newRow("scientific-narrow") << int(Keypad::LayoutModeScientificNarrow) << false;
    QTest::newRow("custom-wide") << int(Keypad::LayoutModeScientificWide) << true;
    QTest::newRow("custom-narrow") << int(Keypad::LayoutModeScientificNarrow) << true;
}

void TestDisplayUi::keypad_nth_root_button_inserts_root()
{
    QFETCH(int, layoutMode);
    QFETCH(bool, custom);
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    const auto oldCustomKeypad = settings->customKeypad;
    const auto customGuard = qScopeGuard([&]() { settings->customKeypad = oldCustomKeypad; });
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->keypadVisible = true;
    settings->hasNumberFormatStyleSetting = true;

    const auto layout = static_cast<Keypad::LayoutMode>(layoutMode);
    const bool wide = layout == Keypad::LayoutModeScientificWide;
    const QString rootLabel = QString::fromUtf8("ⁿ√");
    int rows = 0;
    int columns = 0;
    const auto preset = Keypad::presetCustomButtons(layout, QLatin1Char('.'), &rows, &columns);
    int rootButtonCount = 0;
    int squareButtonCount = 0;
    int separatorButtonCount = 0;
    for (const auto& button : preset) {
        QVERIFY(button.label != QString::fromUtf8("∛"));
        QVERIFY(button.text != QStringLiteral("cbrt("));
        QVERIFY(button.label != QStringLiteral("x"));
        QVERIFY(button.label != QStringLiteral("x="));
        if (button.label == QString::fromUtf8("x²")) {
            ++squareButtonCount;
            QCOMPARE(button.action, int(Settings::CustomKeypadActionInsertText));
            QCOMPARE(button.text, QStringLiteral("^2"));
        }
        if (button.label == QStringLiteral(";")) {
            ++separatorButtonCount;
            QCOMPARE(button.action, int(Settings::CustomKeypadActionInsertText));
            QCOMPARE(button.text, QStringLiteral(";"));
        }
        if (button.label != rootLabel)
            continue;
        ++rootButtonCount;
        QCOMPARE(button.action, int(Settings::CustomKeypadActionInsertText));
        QCOMPARE(button.text, QStringLiteral("root("));
        QCOMPARE(button.row, wide ? 1 : 5);
        QCOMPARE(button.column, wide ? 5 : 1);
    }
    QCOMPARE(rootButtonCount, 1);
    QCOMPARE(squareButtonCount, 1);
    QCOMPARE(separatorButtonCount, 1);

    if (custom) {
        settings->keypadMode = Settings::KeypadModeCustom;
        settings->customKeypad.rows = rows;
        settings->customKeypad.columns = columns;
        settings->customKeypad.buttons.clear();
        for (const auto& button : preset) {
            settings->customKeypad.buttons.append({button.row, button.column, button.label,
                button.text, static_cast<Settings::CustomKeypadButtonAction>(button.action)});
        }
    } else {
        settings->keypadMode = wide ? Settings::KeypadModeScientificWide
                                   : Settings::KeypadModeScientificNarrow;
    }

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    Keypad* keypad = window.findChild<Keypad*>();
    QVERIFY(keypad != nullptr);
    const QStringList expectedRows = wide
        ? QStringList{
            QString::fromUtf8("7 8 9 ÷ √ x² E exp ⌧ ⌫"),
            QString::fromUtf8("4 5 6 × xʸ ⁿ√ ln log10 π ans"),
            QString::fromUtf8("1 2 3 − sin arcsin cos arccos ( )"),
            QString::fromUtf8("0 . = + tan arctan mod ; % !")}
        : QStringList{
            QString::fromUtf8("7 8 9 ÷ ⌧"),
            QString::fromUtf8("4 5 6 × ⌫"),
            QString::fromUtf8("1 2 3 − ("),
            QStringLiteral("0 . = + )"),
            QString::fromUtf8("√ x² E exp π"),
            QString::fromUtf8("xʸ ⁿ√ ln log10 ans"),
            QStringLiteral("sin arcsin cos arccos mod"),
            QStringLiteral("tan arctan % ! ;")};
    QGridLayout* grid = qobject_cast<QGridLayout*>(keypad->layout());
    QVERIFY(grid != nullptr);
    QCOMPARE(rows, expectedRows.size());
    QCOMPARE(columns, wide ? 10 : 5);
    QCOMPARE(grid->count(), 40);
    for (int row = 0; row < expectedRows.size(); ++row) {
        const QStringList labels = expectedRows.at(row).split(QLatin1Char(' '));
        QCOMPARE(labels.size(), columns);
        for (int column = 0; column < columns; ++column) {
            QLayoutItem* item = grid->itemAtPosition(row, column);
            QVERIFY(item != nullptr);
            QPushButton* button = qobject_cast<QPushButton*>(item->widget());
            QVERIFY(button != nullptr);
            const QString expectedLabel = labels.at(column) == QStringLiteral(".")
                ? QString(settings->radixCharacter()) : labels.at(column);
            QCOMPARE(button->text(), expectedLabel);
            QVERIFY(button->isVisible());
        }
    }
    QPushButton* rootButton = keypadButtonWithText(keypad, rootLabel);
    QVERIFY(rootButton != nullptr);
    QCOMPARE(keypadButtonWithText(keypad, QString::fromUtf8("∛")), nullptr);

    if (!custom) {
        const QPoint center = rootButton->rect().center();
        QHelpEvent tooltipEvent(QEvent::ToolTip, center, rootButton->mapToGlobal(center));
        QCoreApplication::sendEvent(rootButton, &tooltipEvent);
        QFrame* popup = keypad->findChild<QFrame*>(QStringLiteral("keypadSummaryPopup"));
        QTRY_VERIFY(popup != nullptr && popup->isVisible());
        QLabel* label = popup->findChild<QLabel*>(QStringLiteral("keypadSummaryPopupLabel"));
        QVERIFY(label != nullptr);
        QCOMPARE(label->text(), Keypad::tr("Nth root"));
    }

    Editor* editor = window.findChild<Editor*>();
    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(editor != nullptr);
    QVERIFY(display != nullptr);
    editor->setText(QString());
    editor->setAutoCompletionEnabled(false);
    QTest::mouseClick(rootButton, Qt::LeftButton);
    QCOMPARE(editor->text(), custom ? QStringLiteral("root()") : QStringLiteral("root("));
    QCOMPARE(editor->cursorPosition(), 5);

    const auto clickButton = [keypad](const QString& label) {
        QTest::mouseClick(keypadButtonWithText(keypad, label), Qt::LeftButton);
    };
    clickButton(QStringLiteral("3"));
    clickButton(QStringLiteral("2"));
    clickButton(QStringLiteral(";"));
    clickButton(QStringLiteral("5"));
    if (!custom)
        clickButton(QStringLiteral(")"));
    QCOMPARE(editor->text(), QStringLiteral("root(32;5)"));
    QPushButton* equalsButton = keypadButtonWithText(keypad, QStringLiteral("="));
    QVERIFY(equalsButton != nullptr);
    const int historySize = display->session()->historySize();
    QTest::mouseClick(equalsButton, Qt::LeftButton);
    QCOMPARE(display->session()->historySize(), historySize + 1);
    const Quantity result = display->session()->historyEntryAtRef(historySize).result();
    QVERIFY(result.numericValue().real == HNumber(2));
    QVERIFY(result.numericValue().imag.isZero());

    editor->setText(QString());
    clickButton(QStringLiteral("7"));
    clickButton(QString::fromUtf8("x²"));
    QCOMPARE(editor->text(), QString::fromUtf8("7²"));
    clickButton(QStringLiteral("="));
    QCOMPARE(display->session()->historySize(), historySize + 2);
    const Quantity squared = display->session()->historyEntryAtRef(historySize + 1).result();
    // Complex-mode powers can leave rounding noise beyond the displayed digits.
    QVERIFY2(HMath::abs(squared.numericValue().real - HNumber(49)) < HNumber("1e-50"),
             qPrintable(HMath::format(squared.numericValue().real,
                                     HNumber::Format::Fixed() + HNumber::Format::Precision(70))));
    QVERIFY(squared.numericValue().imag.isZero());
}

void TestDisplayUi::keypad_input_stays_in_own_window_data()
{
    QTest::addColumn<int>("mode");
    QTest::newRow("basic") << int(Settings::KeypadModeBasicWide);
    QTest::newRow("scientific-wide") << int(Settings::KeypadModeScientificWide);
    QTest::newRow("scientific-narrow") << int(Settings::KeypadModeScientificNarrow);
    QTest::newRow("custom") << int(Settings::KeypadModeCustom);
}

void TestDisplayUi::keypad_input_stays_in_own_window()
{
    QFETCH(int, mode);
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    const auto oldCustomKeypad = settings->customKeypad;
    struct CustomKeypadGuard {
        Settings* settings;
        Settings::CustomKeypad keypad;
        ~CustomKeypadGuard() { settings->customKeypad = keypad; }
    } customGuard { settings, oldCustomKeypad };
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->keypadMode = static_cast<Settings::KeypadMode>(mode);
    settings->keypadVisible = true;
    settings->hasNumberFormatStyleSetting = true;
    if (mode == Settings::KeypadModeCustom) {
        settings->customKeypad.rows = 1;
        settings->customKeypad.columns = 3;
        settings->customKeypad.buttons.clear();
        settings->customKeypad.buttons.append(
            { 0, 0, QStringLiteral("7"), QStringLiteral("7"), Settings::CustomKeypadActionInsertText });
        settings->customKeypad.buttons.append(
            { 0, 1, QString::fromUtf8("⌫"), QString(), Settings::CustomKeypadActionBackspace });
        settings->customKeypad.buttons.append(
            { 0, 2, QStringLiteral("="), QString(), Settings::CustomKeypadActionEvaluateExpression });
    }

    MainWindow firstWindow;
    firstWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&firstWindow));
    MainWindow secondWindow;
    secondWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&secondWindow));
    Editor* firstEditor = firstWindow.findChild<Editor*>();
    Editor* secondEditor = secondWindow.findChild<Editor*>();
    ResultDisplay* firstDisplay = firstWindow.findChild<ResultDisplay*>();
    ResultDisplay* secondDisplay = secondWindow.findChild<ResultDisplay*>();
    QVERIFY(firstEditor != nullptr);
    QVERIFY(secondEditor != nullptr);
    QVERIFY(firstDisplay != nullptr);
    QVERIFY(secondDisplay != nullptr);
    Keypad* firstKeypad = firstWindow.findChild<Keypad*>();
    Keypad* secondKeypad = secondWindow.findChild<Keypad*>();
    QPushButton* firstDigit = keypadButtonWithText(firstKeypad, QStringLiteral("7"));
    QPushButton* secondDigit = keypadButtonWithText(secondKeypad, QStringLiteral("7"));
    QPushButton* firstBackspace = keypadButtonWithText(firstKeypad, QString::fromUtf8("⌫"));
    QPushButton* firstEquals = keypadButtonWithText(firstKeypad, QStringLiteral("="));
    QVERIFY(firstDigit != nullptr);
    QVERIFY(secondDigit != nullptr);
    QVERIFY(firstBackspace != nullptr);
    QVERIFY(firstEquals != nullptr);

    secondWindow.activateWindow();
    secondEditor->setFocus();
    QTRY_VERIFY(secondEditor->hasFocus());
    // Let the activation focus replay finish before exercising the keypad.
    QTest::qWait(400);
    firstEditor->setText(QString());
    secondEditor->setText(QString());
    QTest::mouseClick(secondDigit, Qt::LeftButton);
    QCOMPARE(secondEditor->text(), QStringLiteral("7"));
    QCOMPARE(firstEditor->text(), QString());

    // No explicit window activation or editor focus before this click.
    QTest::mouseClick(firstDigit, Qt::LeftButton);
    QCOMPARE(firstEditor->text(), QStringLiteral("7"));
    QCOMPARE(secondEditor->text(), QStringLiteral("7"));
    QTest::mouseClick(firstBackspace, Qt::LeftButton);
    QCOMPARE(firstEditor->text(), QString());
    QCOMPARE(secondEditor->text(), QStringLiteral("7"));
    QTest::mouseClick(firstDigit, Qt::LeftButton);
    const int firstHistorySize = firstDisplay->session()->historySize();
    const int secondHistorySize = secondDisplay->session()->historySize();
    QTest::mouseClick(firstEquals, Qt::LeftButton);
    QCOMPARE(firstDisplay->session()->historySize(), firstHistorySize + 1);
    QCOMPARE(secondDisplay->session()->historySize(), secondHistorySize);
    QCOMPARE(secondEditor->text(), QStringLiteral("7"));

    QTest::mouseClick(secondDigit, Qt::LeftButton);
    QCOMPARE(secondEditor->text(), QStringLiteral("77"));
}

void TestDisplayUi::functions_dock_retranslates_domain_label_after_language_change()
{
    FunctionsWidget widget;
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    QLabel* domainLabel = nullptr;
    for (QLabel* label : widget.findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("Domain")) {
            domainLabel = label;
            break;
        }
    }
    QVERIFY(domainLabel != nullptr);

    FunctionsTestTranslator translator;
    QCoreApplication::installTranslator(&translator);
    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&widget, &languageChange);

    QCOMPARE(domainLabel->text(), QStringLiteral("Translated Domain"));

    QCoreApplication::removeTranslator(&translator);
}

void TestDisplayUi::main_window_applies_primary_role_to_active_editor_and_dock_selection()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QString oldColorScheme;
        QString oldCustomColorSchemeJson;
        bool oldConstantsDockVisible;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->colorScheme = oldColorScheme;
            settings->customColorSchemeJson = oldCustomColorSchemeJson;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->colorScheme,
        settings->customColorSchemeJson,
        settings->constantsDockVisible,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    const QColor base(QStringLiteral("#1f3229"));
    const QColor generatedPrimary = generatePrimaryFromBackground(base);
    const QColor configuredPrimary(QStringLiteral("#d94f8c"));
    const QVector<QColor> shades = generateOklchShades(base, 6, ThemePolarity::Dark);
    const QVector<QColor> foregrounds = aaForegroundsForBackgrounds(shades);
    QVERIFY(generatedPrimary.isValid());
    QVERIFY(configuredPrimary.isValid());
    QVERIFY(generatedPrimary.name() != configuredPrimary.name());

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{
        {QStringLiteral("background"), base.name()},
        {QStringLiteral("primary"), configuredPrimary.name()}
    });
    settings->constantsDockVisible = true;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    QCOMPARE(editor->palette().color(QPalette::Text).name(), configuredPrimary.name());
    QVERIFY(editor->palette().color(QPalette::Base).name() != configuredPrimary.name());
    QTRY_VERIFY(editor->styleSheet().contains(QStringLiteral("color: %1;").arg(configuredPrimary.name())));
    QTRY_VERIFY(editorHasPrimaryOutline(editor, configuredPrimary));
    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_VERIFY(editorHasPrimaryOutline(editor, configuredPrimary));

    QDockWidget* constantsDock = window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QTreeWidget* table = constantsDock->findChild<QTreeWidget*>();
    QVERIFY(table != nullptr);
    QCOMPARE(table->property("dockListActiveSelectionBackground").value<QColor>().name(),
             configuredPrimary.name());
    QCOMPARE(table->property("dockListActiveSelectionForeground").value<QColor>().name(),
             aaForegroundForBackground(configuredPrimary).name());
    QCOMPARE(table->property("dockListInactiveSelectionBackground").value<QColor>().name(),
             shades.at(4).name());
    QCOMPARE(table->property("dockListInactiveSelectionForeground").value<QColor>().name(),
             foregrounds.at(4).name());

    settings->customColorSchemeJson = themeJsonString(QJsonObject{
        {QStringLiteral("background"), base.name()}
    });
    window.colorSchemeChanged();
    QCoreApplication::processEvents();

    QCOMPARE(editor->palette().color(QPalette::Text).name(), generatedPrimary.name());
    QVERIFY(editor->styleSheet().contains(QStringLiteral("color: %1;").arg(generatedPrimary.name())));
    QCOMPARE(table->property("dockListActiveSelectionBackground").value<QColor>().name(),
             generatedPrimary.name());
    QCOMPARE(table->property("dockListInactiveSelectionBackground").value<QColor>().name(),
             shades.at(4).name());
}

void TestDisplayUi::current_result_tooltip_stays_hidden_after_escape_and_arrow_caret_move()
{
    MainWindowStateGuard guard;
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    editor->setFocus();
    editor->setText(QStringLiteral("1+24"));
    editor->setCursorPosition(editor->text().size());
    editor->refreshAutoCalc();
    QTRY_VERIFY(visibleResultPreviewText(window).contains(QStringLiteral("Current result:")));

    QTest::keyClick(editor, Qt::Key_Escape);
    QTRY_VERIFY(visibleResultPreviewText(window).isEmpty());

    QTest::keyClick(editor, Qt::Key_Left);

    QTRY_VERIFY2(visibleResultPreviewText(window).isEmpty(),
                 qPrintable(QStringLiteral("Caret movement should not reopen the result tooltip, got: %1")
                                .arg(visibleResultPreviewText(window))));
}

void TestDisplayUi::current_result_tooltip_stays_hidden_after_escape_and_mouse_caret_move()
{
    MainWindowStateGuard guard;
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    editor->setFocus();
    editor->setText(QStringLiteral("1+24"));
    editor->setCursorPosition(editor->text().size());
    editor->refreshAutoCalc();
    QTRY_VERIFY(visibleResultPreviewText(window).contains(QStringLiteral("Current result:")));

    QTest::keyClick(editor, Qt::Key_Escape);
    QTRY_VERIFY(visibleResultPreviewText(window).isEmpty());

    QTextCursor cursor = editor->textCursor();
    cursor.setPosition(1);
    const QPoint clickPosition = editor->cursorRect(cursor).center();
    QTest::mouseClick(editor->viewport(), Qt::LeftButton, Qt::NoModifier, clickPosition);

    QTRY_VERIFY2(visibleResultPreviewText(window).isEmpty(),
                 qPrintable(QStringLiteral("Mouse caret movement should not reopen the result tooltip, got: %1")
                                .arg(visibleResultPreviewText(window))));
}

void TestDisplayUi::current_result_tooltip_hides_when_dragging_splitters()
{
    MainWindowStateGuard guard;
    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    editor->setFocus();
    editor->setText(QStringLiteral("1+24"));
    editor->setCursorPosition(editor->text().size());
    editor->refreshAutoCalc();
    QTRY_VERIFY(visibleResultPreviewText(window).contains(QStringLiteral("Current result:")));

    QSplitter* splitContainer =
        window.findChild<QSplitter*>(QStringLiteral("MainSplitContainer"));
    QVERIFY(splitContainer != nullptr);
    QVERIFY(splitContainer->count() > 1);
    QSplitterHandle* handle = splitContainer->handle(1);
    QVERIFY(handle != nullptr);
    const QPoint handleCenter = handle->rect().center();
    QTest::mousePress(handle, Qt::LeftButton, Qt::NoModifier, handleCenter);
    QTest::mouseMove(handle, handleCenter + QPoint(8, 0));
    QTest::mouseRelease(handle, Qt::LeftButton, Qt::NoModifier, handleCenter + QPoint(8, 0));
    QTRY_VERIFY(visibleResultPreviewText(window).isEmpty());

    editor->setText(QStringLiteral("1+25"));
    editor->setCursorPosition(editor->text().size());
    editor->refreshAutoCalc();
    QTRY_VERIFY(visibleResultPreviewText(window).contains(QStringLiteral("Current result:")));

    window.setCursor(Qt::SplitHCursor);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, window.rect().center());
    QTest::mouseMove(&window, window.rect().center() + QPoint(8, 0));
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier,
                        window.rect().center() + QPoint(8, 0));
    window.unsetCursor();
    QTRY_VERIFY(visibleResultPreviewText(window).isEmpty());
}

void TestDisplayUi::calculation_settings_dialog_matches_notation_precision_layout()
{
    EvaluationContext context;
    context.main.fmt = 'f';
    context.main.prec = 3;
    context.angle = 'd';
    context.extras.append(ResultLineContext{'e', 5, ComplexForm::Default});

    ResultSlotsDialog dialog(QStringLiteral("Calculation Settings"), context);
    QCOMPARE(dialog.windowTitle(), QStringLiteral("Calculation Settings"));

    const QList<QLabel*> labels = dialog.findChildren<QLabel*>();
    for (QLabel* label : labels)
        QVERIFY(label->text() != QStringLiteral("Angle Mode"));

    const EvaluationContext updated = dialog.evaluationContext(context);
    QCOMPARE(updated.angle, 'd');
    QCOMPARE(updated.main.fmt, 'f');
    QCOMPARE(updated.main.prec, 3);
    QCOMPARE(updated.extras.size(), 1);
    QCOMPARE(updated.extras.at(0).fmt, 'e');
    QCOMPARE(updated.extras.at(0).prec, 5);
}

void TestDisplayUi::main_window_uses_generated_theme_surface_for_chrome_and_editor()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QString oldColorScheme;
        QString oldCustomColorSchemeJson;
        Settings::KeypadMode oldKeypadMode;
        bool oldKeypadVisible;
        bool oldStatusBarVisible;
        bool oldBitfieldVisible;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->colorScheme = oldColorScheme;
            settings->customColorSchemeJson = oldCustomColorSchemeJson;
            settings->keypadMode = oldKeypadMode;
            settings->keypadVisible = oldKeypadVisible;
            settings->statusBarVisible = oldStatusBarVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->colorScheme,
        settings->customColorSchemeJson,
        settings->keypadMode,
        settings->keypadVisible,
        settings->statusBarVisible,
        settings->bitfieldVisible,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->keypadMode = Settings::KeypadModeBasicWide;
    settings->statusBarVisible = true;
    settings->bitfieldVisible = true;
    settings->hasNumberFormatStyleSetting = true;

    const auto verifyTheme = [settings](const QString& baseName, ThemePolarity polarity) {
        QJsonObject colors;
        colors.insert(QStringLiteral("background"), baseName);
        settings->customColorSchemeJson = themeJsonString(colors);
        settings->keypadVisible = true;
        settings->bitfieldVisible = true;

        const QColor base(baseName);
        const QVector<QColor> shades = generateOklchShades(base, 6, polarity);
        const QVector<QColor> foregrounds = aaForegroundsForBackgrounds(shades);
        const QColor expectedResultSurface = shades.at(UiConfig::ResultDisplayShade);
        const QColor expectedWindowSurface = shades.at(UiConfig::WindowBackgroundShade);
        const QColor expectedKeypadSurface = shades.at(UiConfig::KeypadBackgroundShade);
        const QColor expectedEditorSurface = shades.at(UiConfig::DockBackgroundShade);
        const QColor expectedHeaderSurface = shades.at(UiConfig::DockHeaderShade);
        const QColor expectedInputSurface = shades.at(UiConfig::DockUnfocusedSelectedItemShade);
        const QColor expectedKeypadButtonSurface = shades.at(UiConfig::KeypadButtonShade);
        const QColor expectedBitfieldButtonSurface =
            shades.at(UiConfig::BitfieldButtonFillShade);
        const QColor expectedBitfieldButtonHoverSurface =
            shades.at(UiConfig::BitfieldButtonHoverFillShade);
        const QColor expectedBitfieldButtonPressedSurface =
            shades.at(UiConfig::BitfieldButtonPressedFillShade);
        const QColor expectedStatusBarSurface = shades.at(UiConfig::StatusBarBackgroundShade);
        const QColor expectedPrimary = generatePrimaryFromBackground(base);
        const QColor expectedWindowForeground = foregrounds.at(UiConfig::WindowBackgroundShade);
        const QColor expectedKeypadForeground = foregrounds.at(UiConfig::KeypadBackgroundShade);
        const QColor expectedEditorForeground = foregrounds.at(UiConfig::DockBackgroundShade);
        const QColor expectedHeaderForeground = foregrounds.at(UiConfig::DockHeaderShade);
        const QColor expectedInputForeground =
            foregrounds.at(UiConfig::DockUnfocusedSelectedItemShade);
        const QColor expectedKeypadButtonForeground = foregrounds.at(UiConfig::KeypadButtonShade);
        const QColor expectedBitfieldButtonForeground =
            foregrounds.at(UiConfig::BitfieldButtonFillShade);
        const QColor expectedBitfieldButtonHoverForeground =
            foregrounds.at(UiConfig::BitfieldButtonHoverFillShade);
        const QColor expectedBitfieldButtonPressedForeground =
            foregrounds.at(UiConfig::BitfieldButtonPressedFillShade);
        const QColor expectedStatusBarForeground =
            foregrounds.at(UiConfig::StatusBarBackgroundShade);

        MainWindow window;
        window.show();
        QCoreApplication::processEvents();

        QCOMPARE(window.palette().color(QPalette::Window).name(), expectedWindowSurface.name());
        QCOMPARE(window.palette().color(QPalette::WindowText).name(),
                 expectedWindowForeground.name());
        QCOMPARE(window.palette().color(QPalette::ButtonText).name(),
                 expectedWindowForeground.name());

        Editor* editor = window.findChild<Editor*>();
        ResultDisplay* display = window.findChild<ResultDisplay*>();
        QSplitter* splitContainer =
            window.findChild<QSplitter*>(QStringLiteral("MainSplitContainer"));
        Keypad* keypad = window.findChild<Keypad*>();
        BitFieldWidget* bitfield = window.findChild<BitFieldWidget*>();
        BitWidget* bit = bitfield ? bitfield->findChild<BitWidget*>() : nullptr;
        QStatusBar* statusBar =
            window.findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly);
        QVERIFY(editor != nullptr);
        QVERIFY(display != nullptr);
        QVERIFY(splitContainer != nullptr);
        QVERIFY(keypad != nullptr);
        QVERIFY(bitfield != nullptr);
        QVERIFY(bit != nullptr);
        QVERIFY(statusBar != nullptr);
        QCOMPARE(statusBar->findChildren<QPushButton*>().size(), 3);
        int pipeSeparators = 0;
        for (QLabel* label : statusBar->findChildren<QLabel*>()) {
            if (label->text() == QStringLiteral("|"))
                ++pipeSeparators;
        }
        QCOMPARE(pipeSeparators, 2);
        for (QPushButton* button : statusBar->findChildren<QPushButton*>()) {
            QCOMPARE(button->cursor().shape(), Qt::PointingHandCursor);
            const QImage buttonImage = button->grab().toImage();
            QVERIFY(!buttonImage.isNull());
            QVERIFY2(firstPixelMatchingColor(buttonImage,
                                             buttonImage.rect(),
                                             expectedStatusBarForeground,
                                             64) != QPoint(-1, -1),
                     qPrintable(QStringLiteral("status button %1 has no %2 text pixel")
                                    .arg(button->text(), expectedStatusBarForeground.name())));
        }
        QVERIFY(QMetaObject::invokeMethod(&window,
                                          "setStatusBarVisible",
                                          Qt::DirectConnection,
                                          Q_ARG(bool, true)));
        QCOMPARE(statusBar->findChildren<QPushButton*>().size(), 3);
        QWidget* pane = paneWidgetForDisplay(display);
        QWidget* page = display->parentWidget();
        QTabBar* sessionTabBar = pane ? pane->findChild<QTabBar*>() : nullptr;
        QVERIFY(pane != nullptr);
        QVERIFY(page != nullptr);
        QVERIFY(sessionTabBar != nullptr);
        QCOMPARE(splitContainer->palette().color(QPalette::Window).name(),
                 expectedWindowSurface.name());
        QCOMPARE(pane->palette().color(QPalette::Window).name(),
                 expectedResultSurface.name());
        QCOMPARE(page->palette().color(QPalette::Window).name(),
                 expectedResultSurface.name());
        QCOMPARE(page->parentWidget()->palette().color(QPalette::Window).name(),
                 expectedResultSurface.name());
        QCOMPARE(display->palette().color(QPalette::Base).name(), expectedResultSurface.name());
        QCOMPARE(display->viewport()->palette().color(QPalette::Base).name(),
                 expectedResultSurface.name());
        QVERIFY(display->styleSheet().contains(expectedResultSurface.name()));
        QVERIFY(display->viewport()->styleSheet().contains(expectedResultSurface.name()));
        const QImage displayImage = display->viewport()->grab().toImage();
        QVERIFY(!displayImage.isNull());
        QCOMPARE(displayImage.pixelColor(displayImage.width() / 2,
                                         displayImage.height() / 2).name(),
                 expectedResultSurface.name());
        sessionTabBar->show();
        QCoreApplication::processEvents();
        QVERIFY(sessionTabBar->isVisible());
        QToolButton* sessionCloseButton = qobject_cast<QToolButton*>(
            sessionTabBar->tabButton(0, QTabBar::RightSide));
        QVERIFY(sessionCloseButton != nullptr);
        QVERIFY(sessionCloseButton->toolTip().isEmpty());
        QEvent closeButtonEnterEvent(QEvent::Enter);
        QCoreApplication::sendEvent(sessionCloseButton, &closeButtonEnterEvent);
        QCoreApplication::processEvents();
        QFrame* closeButtonToolTip =
            sessionTabBar->findChild<QFrame*>(QStringLiteral("sessionTabToolTipPopup"));
        QVERIFY(closeButtonToolTip != nullptr);
        QVERIFY(closeButtonToolTip->isVisible());
        QVERIFY(closeButtonToolTip->styleSheet().contains(
            shades.at(UiConfig::CompletionPopupBackgroundShade).name()));
        QLabel* closeButtonToolTipLabel = closeButtonToolTip->findChild<QLabel*>(
            QStringLiteral("sessionTabToolTipPopupLabel"));
        QVERIFY(closeButtonToolTipLabel != nullptr);
        QCOMPARE(closeButtonToolTipLabel->text(), QStringLiteral("Close Session"));
        QEvent closeButtonLeaveEvent(QEvent::Leave);
        QCoreApplication::sendEvent(sessionCloseButton, &closeButtonLeaveEvent);
        QCoreApplication::processEvents();
        QVERIFY(!closeButtonToolTip->isVisible());
        const QImage tabBarImage = sessionTabBar->grab().toImage();
        QVERIFY(!tabBarImage.isNull());
        QCOMPARE(tabBarImage.pixelColor(tabBarImage.width() - 1,
                                        tabBarImage.height() / 2).name(),
                 expectedWindowSurface.name());
        QWidget* tabBarRow = sessionTabBar->parentWidget();
        QVERIFY(tabBarRow != nullptr);
        QCOMPARE(tabBarRow->palette().color(QPalette::Window).name(),
                 expectedWindowSurface.name());
        QVERIFY(tabBarRow->styleSheet().contains(expectedWindowSurface.name()));
        QCOMPARE(editor->palette().color(QPalette::Base).name(),
                 expectedEditorSurface.name());
        QVERIFY(editor->styleSheet().contains(expectedEditorSurface.name()));
        QVERIFY(editor->styleSheet().contains(QStringLiteral("border-radius: 13px")));
        QVERIFY(editor->styleSheet().contains(QStringLiteral("padding: 10px 18px")));
        QVERIFY(editor->viewport()->styleSheet().contains(QStringLiteral("background: transparent")));
        QTRY_COMPARE(editor->cursorWidth(), 2);
        QCOMPARE(editor->graphicsEffect(), nullptr);
        QVERIFY(editor->mask().isEmpty());
        QCOMPARE(keypad->palette().color(QPalette::Window).name(), expectedKeypadSurface.name());
        QTRY_COMPARE(keypad->palette().color(QPalette::WindowText).name(),
                     expectedKeypadForeground.name());
        QPushButton* keypadButton = keypadButtonWithText(keypad, QStringLiteral("%"));
        QPushButton* keypadDigitButton = keypadButtonWithText(keypad, QStringLiteral("7"));
        QPushButton* keypadDecimalButton =
            keypadButtonWithText(keypad, QString(QChar(settings->radixCharacter())));
        QPushButton* keypadEvaluateButton = keypadButtonWithText(keypad, QStringLiteral("="));
        QVERIFY(keypadButton != nullptr);
        QVERIFY(keypadDigitButton != nullptr);
        QVERIFY(keypadDecimalButton != nullptr);
        QVERIFY(keypadEvaluateButton != nullptr);
        QWidget* keypadContainer = keypad->parentWidget();
        QVERIFY(keypadContainer != nullptr);
        QCOMPARE(keypadContainer->palette().color(QPalette::Window).name(),
                 expectedKeypadSurface.name());
        QVERIFY(keypadContainer->styleSheet().contains(expectedKeypadSurface.name()));
        const QImage keypadContainerImage = keypadContainer->grab().toImage();
        QVERIFY(!keypadContainerImage.isNull());
        QCOMPARE(keypadContainerImage.pixelColor(0, keypadContainerImage.height() / 2).name(),
                 expectedKeypadSurface.name());
        QCOMPARE(keypadContainerImage.pixelColor(keypadContainerImage.width() - 1,
                                                 keypadContainerImage.height() / 2).name(),
                 expectedKeypadSurface.name());
        const QString keypadButtonStyle = keypadButton->styleSheet();
        QCOMPARE(keypadButton->palette().color(QPalette::Button).name(),
                 expectedKeypadButtonSurface.name());
        QCOMPARE(keypadButton->palette().color(QPalette::ButtonText).name(),
                 expectedKeypadButtonForeground.name());
        QVERIFY(keypadButtonStyle.contains(expectedKeypadButtonSurface.name()));
        QVERIFY(keypadButtonStyle.contains(expectedKeypadButtonForeground.name()));
        QVERIFY(keypadButtonStyle.contains(expectedHeaderSurface.name()));
        QVERIFY(keypadButtonStyle.contains(expectedHeaderForeground.name()));
        QVERIFY(keypadButtonStyle.contains(expectedInputSurface.name()));
        QVERIFY(keypadButtonStyle.contains(expectedInputForeground.name()));
        QVERIFY(keypadButtonStyle.contains(QStringLiteral("border: none")));
        QVERIFY(keypadButtonStyle.contains(QStringLiteral("qlineargradient")));
        QVERIFY(keypadButtonStyle.contains(QStringLiteral("border-radius: %1px")
                                               .arg(UiConfig::KeypadButtonCornerRadius)));
        QVERIFY(keypadButtonStyle.contains(QStringLiteral("margin: %1px")
                                               .arg(UiConfig::KeypadButtonMargin)));
        QVERIFY2(keypadButtonStyle.contains(QStringLiteral("padding: %1px")
                                                .arg(UiConfig::KeypadButtonPadding)),
                 qPrintable(keypadButtonStyle));
        QVERIFY(keypad->layout() != nullptr);
        QCOMPARE(keypad->layout()->contentsMargins(),
                 QMargins(UiConfig::KeypadButtonMargin,
                          UiConfig::KeypadButtonMargin,
                          UiConfig::KeypadButtonMargin,
                          UiConfig::KeypadButtonMargin));
        const QColor expectedDigitSurface =
            keypadPrimaryHueFillForTest(
                expectedPrimary,
                expectedKeypadButtonSurface,
                expectedKeypadButtonSurface,
                UiConfig::KeypadDigitPrimaryHueChromaPercent);
        const QColor expectedDigitForeground = aaForegroundForBackground(expectedDigitSurface);
        for (QPushButton* digitButton : {keypadDigitButton, keypadDecimalButton}) {
            const QString digitStyle = digitButton->styleSheet();
            QCOMPARE(digitButton->palette().color(QPalette::Button).name(),
                     expectedDigitSurface.name());
            QCOMPARE(digitButton->palette().color(QPalette::ButtonText).name(),
                     expectedDigitForeground.name());
            QVERIFY(digitStyle.contains(expectedDigitSurface.name()));
            QVERIFY(digitStyle.contains(expectedDigitForeground.name()));
            QVERIFY(digitStyle.contains(QStringLiteral("qlineargradient")));
        }
        const QColor expectedOperatorSurface =
            keypadPrimaryHueFillForTest(
                expectedPrimary,
                expectedKeypadButtonSurface,
                expectedKeypadButtonSurface,
                UiConfig::KeypadOperatorPrimaryHueChromaPercent);
        const QColor expectedOperatorHoverSurface =
            keypadPrimaryHueFillForTest(
                expectedPrimary,
                expectedHeaderSurface,
                expectedKeypadButtonSurface,
                UiConfig::KeypadOperatorPrimaryHueChromaPercent);
        const QColor expectedOperatorPressedSurface =
            keypadPrimaryHueFillForTest(
                expectedPrimary,
                expectedInputSurface,
                expectedKeypadButtonSurface,
                UiConfig::KeypadOperatorPrimaryHueChromaPercent);
        const QColor expectedOperatorForeground = aaForegroundForBackground(expectedOperatorSurface);
        const QColor expectedOperatorHoverForeground =
            aaForegroundForBackground(expectedOperatorHoverSurface);
        const QColor expectedOperatorPressedForeground =
            aaForegroundForBackground(expectedOperatorPressedSurface);
        const QStringList keypadOperatorLabels = {
            QStringLiteral("+"),
            QString::fromUtf8("−"),
            QString::fromUtf8("×"),
            QString::fromUtf8("÷")
        };
        for (const QString& label : keypadOperatorLabels) {
            QPushButton* keypadOperatorButton = keypadButtonWithText(keypad, label);
            QVERIFY2(keypadOperatorButton != nullptr, qPrintable(label));
            const QString keypadOperatorStyle = keypadOperatorButton->styleSheet();
            QCOMPARE(keypadOperatorButton->palette().color(QPalette::Button).name(),
                     expectedOperatorSurface.name());
            QCOMPARE(keypadOperatorButton->palette().color(QPalette::ButtonText).name(),
                     expectedOperatorForeground.name());
            QVERIFY(keypadOperatorStyle.contains(expectedOperatorSurface.name()));
            QVERIFY(keypadOperatorStyle.contains(expectedOperatorForeground.name()));
            QVERIFY(keypadOperatorStyle.contains(expectedOperatorHoverSurface.name()));
            QVERIFY(keypadOperatorStyle.contains(expectedOperatorHoverForeground.name()));
            QVERIFY(keypadOperatorStyle.contains(expectedOperatorPressedSurface.name()));
            QVERIFY(keypadOperatorStyle.contains(expectedOperatorPressedForeground.name()));
            QVERIFY(keypadOperatorStyle.contains(QStringLiteral("qlineargradient")));
        }
        const QColor expectedEvaluateSurface =
            keypadPrimaryHueFillForTest(
                expectedPrimary,
                expectedKeypadButtonSurface,
                expectedKeypadButtonSurface,
                UiConfig::KeypadEvaluatePrimaryHueChromaPercent);
        const QColor expectedEvaluateForeground =
            aaForegroundForBackground(expectedEvaluateSurface);
        QCOMPARE(keypadEvaluateButton->palette().color(QPalette::Button).name(),
                 expectedEvaluateSurface.name());
        QCOMPARE(keypadEvaluateButton->palette().color(QPalette::ButtonText).name(),
                 expectedEvaluateForeground.name());
        QVERIFY(keypadEvaluateButton->styleSheet().contains(expectedEvaluateSurface.name()));
        QVERIFY(keypadEvaluateButton->styleSheet().contains(expectedEvaluateForeground.name()));
        QVERIFY(keypadEvaluateButton->styleSheet().contains(QStringLiteral("qlineargradient")));
        QCOMPARE(bitfield->palette().color(QPalette::Window).name(), expectedEditorSurface.name());
        QCOMPARE(bitfield->palette().color(QPalette::Button).name(), expectedEditorSurface.name());
        QVERIFY(bit->styleSheet().contains(expectedEditorForeground.name()));
        QVERIFY(bit->styleSheet().contains(expectedEditorSurface.name()));
        QVERIFY(bit->styleSheet().contains(expectedHeaderSurface.name()));
        QVERIFY(bit->styleSheet().contains(expectedHeaderForeground.name()));
        QVERIFY(bit->styleSheet().contains(expectedPrimary.name()));
        QVERIFY(bit->styleSheet().contains(aaForegroundForBackground(expectedPrimary).name()));
        QVERIFY(bitfield->styleSheet().contains(expectedEditorForeground.name()));
        QVERIFY(bitfield->styleSheet().contains(expectedEditorSurface.name()));
        QPushButton* bitfieldButton = bitfield->findChild<QPushButton*>();
        QVERIFY(bitfieldButton != nullptr);
        QCOMPARE(bitfieldButton->palette().color(QPalette::Button).name(),
                 expectedBitfieldButtonSurface.name());
        QCOMPARE(bitfieldButton->palette().color(QPalette::ButtonText).name(),
                 expectedBitfieldButtonForeground.name());
        QVERIFY(bitfieldButton->styleSheet().contains(expectedBitfieldButtonSurface.name()));
        QVERIFY(bitfieldButton->styleSheet().contains(expectedBitfieldButtonForeground.name()));
        QVERIFY(bitfieldButton->styleSheet().contains(expectedBitfieldButtonHoverSurface.name()));
        QVERIFY(bitfieldButton->styleSheet().contains(
            expectedBitfieldButtonHoverForeground.name()));
        QVERIFY(bitfieldButton->styleSheet().contains(expectedBitfieldButtonPressedSurface.name()));
        QVERIFY(bitfieldButton->styleSheet().contains(
            expectedBitfieldButtonPressedForeground.name()));
        QCOMPARE(statusBar->palette().color(QPalette::Window).name(),
                 expectedStatusBarSurface.name());
        QCOMPARE(statusBar->palette().color(QPalette::WindowText).name(),
                 expectedStatusBarForeground.name());
        QVERIFY(statusBar->styleSheet().contains(expectedStatusBarSurface.name()));
        QVERIFY(statusBar->styleSheet().contains(expectedStatusBarForeground.name()));
        QVERIFY(statusBar->styleSheet().contains(QStringLiteral("QStatusBar QPushButton")));
        QVERIFY(statusBar->styleSheet().contains(
            QStringLiteral("padding: 0px 4px")));
    };

    verifyTheme(QStringLiteral("#300a24"), ThemePolarity::Dark);
    verifyTheme(QStringLiteral("#1f3229"), ThemePolarity::Dark);
    verifyTheme(QStringLiteral("#e5eee8"), ThemePolarity::Light);

    settings->customColorSchemeJson = themeJsonString(QJsonObject{{QStringLiteral("background"), QStringLiteral("#e5eee8")}});
    MainWindow changedWindow;
    changedWindow.show();
    QCoreApplication::processEvents();

    settings->customColorSchemeJson = themeJsonString(QJsonObject{{QStringLiteral("background"), QStringLiteral("#300a24")}});
    changedWindow.colorSchemeChanged();
    QCoreApplication::processEvents();

    const QVector<QColor> changedShades =
        generateOklchShades(QColor(QStringLiteral("#300a24")), 6, ThemePolarity::Dark);
    const QColor changedPrimary = generatePrimaryFromBackground(QColor(QStringLiteral("#300a24")));
    ResultDisplay* changedDisplay = changedWindow.findChild<ResultDisplay*>();
    Editor* changedEditor = changedWindow.findChild<Editor*>();
    BitFieldWidget* changedBitfield = changedWindow.findChild<BitFieldWidget*>();
    Keypad* changedKeypad = changedWindow.findChild<Keypad*>();
    QPushButton* changedKeypadButton =
        changedKeypad ? keypadButtonWithText(changedKeypad, QStringLiteral("%")) : nullptr;
    QPushButton* changedDigitButton =
        changedKeypad ? keypadButtonWithText(changedKeypad, QStringLiteral("7")) : nullptr;
    QPushButton* changedOperatorButton =
        changedKeypad ? keypadButtonWithText(changedKeypad, QStringLiteral("+")) : nullptr;
    QPushButton* changedEvaluateButton =
        changedKeypad ? keypadButtonWithText(changedKeypad, QStringLiteral("=")) : nullptr;
    QStatusBar* changedStatusBar =
        changedWindow.findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly);
    QVERIFY(changedDisplay != nullptr);
    QVERIFY(changedEditor != nullptr);
    QVERIFY(changedBitfield != nullptr);
    QVERIFY(changedKeypad != nullptr);
    QVERIFY(changedKeypadButton != nullptr);
    QVERIFY(changedDigitButton != nullptr);
    QVERIFY(changedOperatorButton != nullptr);
    QVERIFY(changedEvaluateButton != nullptr);
    QVERIFY(changedStatusBar != nullptr);
    QCOMPARE(changedWindow.palette().color(QPalette::Window).name(),
             changedShades.at(UiConfig::WindowBackgroundShade).name());
    QCOMPARE(changedDisplay->palette().color(QPalette::Base).name(),
             changedShades.at(UiConfig::ResultDisplayShade).name());
    QCOMPARE(changedEditor->viewport()->palette().color(QPalette::Base).name(),
             changedShades.at(UiConfig::DockBackgroundShade).name());
    QCOMPARE(changedEditor->parentWidget()->palette().color(QPalette::Window).name(),
             changedShades.at(UiConfig::ResultDisplayShade).name());
    QCOMPARE(changedBitfield->palette().color(QPalette::Window).name(),
             changedShades.at(UiConfig::DockBackgroundShade).name());
    QCOMPARE(changedKeypadButton->palette().color(QPalette::Button).name(),
             changedShades.at(UiConfig::KeypadButtonShade).name());
    QVERIFY(changedKeypadButton->styleSheet().contains(
        changedShades.at(UiConfig::KeypadButtonShade).name()));
    QCOMPARE(changedStatusBar->palette().color(QPalette::Window).name(),
             changedShades.at(UiConfig::StatusBarBackgroundShade).name());
    QVERIFY(changedStatusBar->styleSheet().contains(
        changedShades.at(UiConfig::StatusBarBackgroundShade).name()));
    const QColor changedDigitSurface = keypadPrimaryHueFillForTest(
        changedPrimary,
        changedShades.at(UiConfig::KeypadButtonShade),
        changedShades.at(UiConfig::KeypadButtonShade),
        UiConfig::KeypadDigitPrimaryHueChromaPercent);
    QCOMPARE(changedDigitButton->palette().color(QPalette::Button).name(),
             changedDigitSurface.name());
    QCOMPARE(changedDigitButton->palette().color(QPalette::ButtonText).name(),
             aaForegroundForBackground(changedDigitSurface).name());
    QVERIFY(changedDigitButton->styleSheet().contains(changedDigitSurface.name()));
    const QColor changedOperatorSurface = keypadPrimaryHueFillForTest(
        changedPrimary,
        changedShades.at(UiConfig::KeypadButtonShade),
        changedShades.at(UiConfig::KeypadButtonShade),
        UiConfig::KeypadOperatorPrimaryHueChromaPercent);
    QCOMPARE(changedOperatorButton->palette().color(QPalette::Button).name(),
             changedOperatorSurface.name());
    QCOMPARE(changedOperatorButton->palette().color(QPalette::ButtonText).name(),
             aaForegroundForBackground(changedOperatorSurface).name());
    QVERIFY(changedOperatorButton->styleSheet().contains(changedOperatorSurface.name()));
    const QColor changedEvaluateSurface = keypadPrimaryHueFillForTest(
        changedPrimary,
        changedShades.at(UiConfig::KeypadButtonShade),
        changedShades.at(UiConfig::KeypadButtonShade),
        UiConfig::KeypadEvaluatePrimaryHueChromaPercent);
    QCOMPARE(changedEvaluateButton->palette().color(QPalette::Button).name(),
             changedEvaluateSurface.name());
    QCOMPARE(changedEvaluateButton->palette().color(QPalette::ButtonText).name(),
             aaForegroundForBackground(changedEvaluateSurface).name());
    QVERIFY(changedEvaluateButton->styleSheet().contains(changedEvaluateSurface.name()));

    QAction* scientificNarrowAction =
        keypadModeAction(&changedWindow, Settings::KeypadModeScientificNarrow);
    QVERIFY(scientificNarrowAction != nullptr);
    QVERIFY(QMetaObject::invokeMethod(&changedWindow,
                                      "setKeypadMode",
                                      Qt::DirectConnection,
                                      Q_ARG(QAction*, scientificNarrowAction)));
    QCoreApplication::processEvents();

    Keypad* switchedKeypad = changedWindow.findChild<Keypad*>();
    QPushButton* switchedDigitButton =
        switchedKeypad ? keypadButtonWithText(switchedKeypad, QStringLiteral("7")) : nullptr;
    QPushButton* switchedOperatorButton =
        switchedKeypad ? keypadButtonWithText(switchedKeypad, QStringLiteral("+")) : nullptr;
    QPushButton* switchedEvaluateButton =
        switchedKeypad ? keypadButtonWithText(switchedKeypad, QStringLiteral("=")) : nullptr;
    QVERIFY(switchedKeypad != nullptr);
    QVERIFY(switchedDigitButton != nullptr);
    QVERIFY(switchedOperatorButton != nullptr);
    QVERIFY(switchedEvaluateButton != nullptr);
    QCOMPARE(switchedDigitButton->palette().color(QPalette::Button).name(),
             changedDigitSurface.name());
    QCOMPARE(switchedDigitButton->palette().color(QPalette::ButtonText).name(),
             aaForegroundForBackground(changedDigitSurface).name());
    QVERIFY(switchedDigitButton->styleSheet().contains(changedDigitSurface.name()));
    QCOMPARE(switchedOperatorButton->palette().color(QPalette::Button).name(),
             changedOperatorSurface.name());
    QCOMPARE(switchedOperatorButton->palette().color(QPalette::ButtonText).name(),
             aaForegroundForBackground(changedOperatorSurface).name());
    QVERIFY(switchedOperatorButton->styleSheet().contains(changedOperatorSurface.name()));
    QCOMPARE(switchedEvaluateButton->palette().color(QPalette::Button).name(),
             changedEvaluateSurface.name());
    QCOMPARE(switchedEvaluateButton->palette().color(QPalette::ButtonText).name(),
             aaForegroundForBackground(changedEvaluateSurface).name());
    QVERIFY(switchedEvaluateButton->styleSheet().contains(changedEvaluateSurface.name()));

    QVERIFY(!changedKeypadButton->styleSheet().contains(
        generateOklchShades(QColor(QStringLiteral("#e5eee8")), 6, ThemePolarity::Light)
            .at(UiConfig::KeypadButtonShade)
            .name()));

    QVERIFY(QMetaObject::invokeMethod(&changedWindow,
                                      "setStatusBarVisible",
                                      Qt::DirectConnection,
                                      Q_ARG(bool, false)));
    QCoreApplication::processEvents();
    QVERIFY(QMetaObject::invokeMethod(&changedWindow,
                                      "setStatusBarVisible",
                                      Qt::DirectConnection,
                                      Q_ARG(bool, true)));
    QCoreApplication::processEvents();

    QStatusBar* recreatedStatusBar =
        changedWindow.findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly);
    QVERIFY(recreatedStatusBar != nullptr);
    QCOMPARE(recreatedStatusBar->palette().color(QPalette::Window).name(),
             changedShades.at(UiConfig::StatusBarBackgroundShade).name());
    QVERIFY(recreatedStatusBar->styleSheet().contains(
        changedShades.at(UiConfig::StatusBarBackgroundShade).name()));
    QVERIFY(recreatedStatusBar->styleSheet().contains(QStringLiteral("QStatusBar QPushButton")));
    QCOMPARE(recreatedStatusBar->findChildren<QPushButton*>().size(), 3);
    int recreatedPipeSeparators = 0;
    for (QLabel* label : recreatedStatusBar->findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("|"))
            ++recreatedPipeSeparators;
    }
    QCOMPARE(recreatedPipeSeparators, 2);
    for (QPushButton* button : recreatedStatusBar->findChildren<QPushButton*>())
        QCOMPARE(button->cursor().shape(), Qt::PointingHandCursor);

    const QImage changedDisplayImage = changedDisplay->viewport()->grab().toImage();
    QVERIFY(!changedDisplayImage.isNull());
    QCOMPARE(changedDisplayImage.pixelColor(changedDisplayImage.width() / 2,
                                            changedDisplayImage.height() / 2).name(),
             QStringLiteral("#300a24"));

    if (!UiConfig::OklchThemeDebugReportEnabled)
        return;

    QFile report(QDir(QDir::tempPath()).absoluteFilePath(
        QStringLiteral("speedcrunch-oklch-theme-report.html")));
    QVERIFY(report.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString reportHtml = QString::fromUtf8(report.readAll());
    QVERIFY(reportHtml.contains(QStringLiteral("Runtime widget samples")));
    QVERIFY(reportHtml.contains(QStringLiteral("<code>1=#300A24</code>")));
    QVERIFY(reportHtml.contains(QStringLiteral("ResultDisplay 1")));
    QVERIFY(reportHtml.contains(QStringLiteral("<td><code>#300A24</code></td>")));
}

void TestDisplayUi::menu_bar_keeps_themed_contrast_with_platform_palette_data()
{
    QTest::addColumn<bool>("darkPlatform");
    QTest::addColumn<QString>("background");
    QTest::newRow("dark-desktop-dark-theme") << true << QStringLiteral("#232136");
    QTest::newRow("light-desktop-dark-theme") << false << QStringLiteral("#232136");
    QTest::newRow("dark-desktop-light-theme") << true << QStringLiteral("#e5eee8");
    QTest::newRow("light-desktop-light-theme") << false << QStringLiteral("#e5eee8");
}

void TestDisplayUi::menu_bar_keeps_themed_contrast_with_platform_palette()
{
    QFETCH(bool, darkPlatform);
    QFETCH(QString, background);
    MainWindowStateGuard guard;
    guard.settings->menuAppearance = Settings::MenuAppearanceSpeedCrunch;
    guard.settings->colorScheme = QStringLiteral("Custom");
    guard.settings->customColorSchemeJson = themeJsonString(
        QJsonObject{{QStringLiteral("background"), background}});

    MainWindow window;
    QMenuBar* bar = window.menuBar();
    bar->setNativeMenuBar(false);
    QPalette platformPalette = QApplication::palette();
    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive,
                                            QPalette::Disabled}) {
        platformPalette.setColor(group, QPalette::Window,
                                 darkPlatform ? QColor("#1e1e1e") : QColor("#efefef"));
        platformPalette.setColor(group, QPalette::WindowText,
                                 darkPlatform ? QColor("#eff0f1") : QColor("#232629"));
        platformPalette.setColor(group, QPalette::ButtonText,
                                 platformPalette.color(group, QPalette::WindowText));
    }
    auto* platformStyle = new MenuBarPaletteTestStyle(platformPalette);
    platformStyle->setParent(bar);
    bar->setStyle(platformStyle);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.colorSchemeChanged();
    QCoreApplication::processEvents();

    const auto verifyTheme = [bar](const QColor& base) {
        const QVector<QColor> shades = generateOklchShades(base, 6, themePolarityForBackground(base));
        const QColor fill = shades.at(UiConfig::WindowBackgroundShade);
        const QColor text = aaForegroundForBackground(fill);
        const QColor selectedFill = shades.at(UiConfig::DockUnfocusedSelectedItemShade);
        const QColor selectedText = aaForegroundForBackground(selectedFill);
        for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive,
                                                QPalette::Disabled}) {
            QCOMPARE(bar->palette().color(group, QPalette::Window), fill);
            QCOMPARE(bar->palette().color(group, QPalette::WindowText), text);
            QCOMPARE(bar->palette().color(group, QPalette::ButtonText), text);
            QCOMPARE(bar->palette().color(group, QPalette::Highlight), selectedFill);
            QCOMPARE(bar->palette().color(group, QPalette::HighlightedText), selectedText);
        }

        const QImage rendered = bar->grab().toImage();
        const qreal dpr = rendered.devicePixelRatio();
        QCOMPARE(rendered.pixelColor(qRound((bar->width() - 4) * dpr),
                                     qRound(bar->height() / 2.0 * dpr)).name(), fill.name());

        // Check painted glyphs as well as palette roles. Native styles may
        // choose a different text role or selection color when drawing items.
        for (const QStyle::State state : {QStyle::State(QStyle::State_Enabled | QStyle::State_Active),
                                          QStyle::State(QStyle::State_Enabled),
                                          QStyle::State(QStyle::State_None),
                                          QStyle::State(QStyle::State_Enabled | QStyle::State_Selected),
                                          QStyle::State(QStyle::State_Enabled | QStyle::State_Selected
                                                        | QStyle::State_Sunken)}) {
            QStyleOptionMenuItem option;
            option.initFrom(bar);
            option.state = state;
            option.rect = QRect(0, 0, 100, bar->height());
            option.text = QStringLiteral("Session");
            const bool selected = state.testFlag(QStyle::State_Selected);
            const QColor expectedFill = selected ? selectedFill : fill;
            const QColor expectedText = selected ? selectedText : text;
            QImage item(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
            item.fill(fill);
            QPainter painter(&item);
            painter.setFont(bar->font());
            bar->style()->drawControl(QStyle::CE_MenuBarItem, &option, &painter, bar);
            painter.end();
            QCOMPARE(item.pixelColor(1, 1).name(), expectedFill.name());
            int textPixels = 0;
            for (int y = 0; y < item.height(); ++y) {
                for (int x = 0; x < item.width(); ++x) {
                    const QColor pixel = item.pixelColor(x, y);
                    if (qAbs(pixel.red() - expectedText.red()) < 24
                        && qAbs(pixel.green() - expectedText.green()) < 24
                        && qAbs(pixel.blue() - expectedText.blue()) < 24)
                        ++textPixels;
                }
            }
            QVERIFY2(textPixels > 10, "Menu bar text does not use the contrasting theme color.");
        }
    };
    verifyTheme(QColor(background));

    const QColor switchedBackground(background == QLatin1String("#232136")
                                    ? QStringLiteral("#e5eee8") : QStringLiteral("#232136"));
    guard.settings->customColorSchemeJson = themeJsonString(
        QJsonObject{{QStringLiteral("background"), switchedBackground.name()}});
    window.colorSchemeChanged();
    QCoreApplication::processEvents();
    verifyTheme(switchedBackground);
}

void TestDisplayUi::menu_bar_restores_native_painting_after_theme_switch_data()
{
    QTest::addColumn<QString>("styleName");
    QTest::newRow("fusion") << QStringLiteral("Fusion");
    QTest::newRow("windows") << QStringLiteral("Windows");
}

void TestDisplayUi::menu_bar_restores_native_painting_after_theme_switch()
{
    QFETCH(QString, styleName);
    MainWindowStateGuard guard;
    guard.settings->menuAppearance = Settings::MenuAppearanceSystem;
    MainWindow window(false);
    QMenuBar* bar = window.menuBar();
    bar->setNativeMenuBar(false);
    QScopedPointer<QStyle> style(QStyleFactory::create(styleName));
    QVERIFY(style != nullptr);
    bar->setStyle(style.data());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    MenuStyle::refresh();
    QCoreApplication::processEvents();
    const bool nativeStyledBackground = bar->testAttribute(Qt::WA_StyledBackground);
    const QImage nativeRendering = bar->grab().toImage();
    QMenu* appearance = window.findChild<QMenu*>(QStringLiteral("MenuAppearanceMenu"));
    QVERIFY(appearance != nullptr);
    QCOMPARE(appearance->title(), QStringLiteral("&Menus"));
    for (int cycle = 0; cycle < 2; ++cycle) {
        appearance->actions().at(1)->trigger();
        QCoreApplication::processEvents();
        QVERIFY(bar->testAttribute(Qt::WA_StyledBackground));
        // Switch from an open menu, as users do, then let the style finish.
        bool switched = false;
        QTimer::singleShot(0, &window, [&]() {
            appearance->actions().at(0)->trigger();
            appearance->close();
            switched = true;
        });
        appearance->exec(bar->mapToGlobal(QPoint(0, bar->height())));
        QVERIFY(switched);
        QCoreApplication::processEvents();
        QVERIFY(bar->styleSheet().isEmpty());
        QCOMPARE(bar->testAttribute(Qt::WA_StyledBackground), nativeStyledBackground);
        QCOMPARE(bar->grab().toImage(), nativeRendering);
    }
    bar->setStyle(nullptr);
}

void TestDisplayUi::dock_dropdowns_follow_menu_appearance_data()
{
    QTest::addColumn<bool>("darkSystem");
    QTest::addColumn<bool>("floating");
    QTest::newRow("dark-attached") << true << false;
    QTest::newRow("light-attached") << false << false;
    QTest::newRow("dark-floating") << true << true;
    QTest::newRow("light-floating") << false << true;
}

void TestDisplayUi::dock_dropdowns_follow_menu_appearance()
{
    QFETCH(bool, darkSystem);
    QFETCH(bool, floating);
    MainWindowStateGuard guard;
    const QPalette original = QApplication::palette();
    const auto restorePalette = qScopeGuard([&]() {
        QApplication::setPalette(original);
        MenuStyle::refresh();
    });
    QPalette native = original;
    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        for (const QPalette::ColorRole role : {QPalette::Window, QPalette::Base, QPalette::Button})
            native.setColor(group, role, QColor(darkSystem ? "#202428" : "#eeeeee"));
        for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
            native.setColor(group, role, QColor(group == QPalette::Disabled ? "#808890"
                                                                         : (darkSystem ? "#f0f0f0" : "#202020")));
        native.setColor(group, QPalette::Highlight, QColor("#354faf"));
        native.setColor(group, QPalette::HighlightedText, QColor("#ffffff"));
    }
    QApplication::setPalette(native);
    guard.settings->menuAppearance = Settings::MenuAppearanceSystem;
    guard.settings->colorScheme = QStringLiteral("Custom");
    guard.settings->customColorSchemeJson = themeJsonString(
        QJsonObject{{QStringLiteral("background"), darkSystem ? "#e5eee8" : "#232136"}});
    guard.settings->constantsDockVisible = true;
    guard.settings->functionsDockVisible = true;
    MainWindow window(false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QMenu* appearance = window.findChild<QMenu*>(QStringLiteral("MenuAppearanceMenu"));
    QVERIFY(appearance != nullptr);
    for (const char* dockName : {"ConstantsDock", "FunctionsDock"}) {
        appearance->actions().at(0)->trigger();
        QDockWidget* dock = window.findChild<QDockWidget*>(QString::fromLatin1(dockName));
        QVERIFY(dock != nullptr);
        dock->setFloating(floating);
        dock->show();
        QComboBox* combo = dock->findChild<QComboBox*>();
        QVERIFY(combo != nullptr);
        QCoreApplication::processEvents();
        QAbstractItemView* view = combo->view();
        const auto nativeFrame = view->frameShape();
        const bool nativeViewportFill = view->viewport()->autoFillBackground();
        const QPalette closedPalette = combo->palette();
        for (const int mode : {0, 1, 0, 1, 0}) {
            appearance->actions().at(mode)->trigger();
            combo->showPopup();
            const auto closePopup = qScopeGuard([combo]() { combo->hidePopup(); });
            QCoreApplication::processEvents();
            QVERIFY(view->isVisible());
            QWidget* popup = view->window();
            QCOMPARE(combo->palette(), closedPalette);
            if (mode == 0) {
                QVERIFY(view->styleSheet().isEmpty());
                QVERIFY(view->verticalScrollBar()->styleSheet().isEmpty());
                QVERIFY(!combo->styleSheet().contains(QStringLiteral("QComboBox QAbstractItemView")));
                QCOMPARE(view->frameShape(), nativeFrame);
                QCOMPARE(view->viewport()->autoFillBackground(), nativeViewportFill);
                QVERIFY(popup->mask().isEmpty());
                for (QWidget* surface : {static_cast<QWidget*>(view), view->viewport(), popup}) {
                    const QPalette expected = MenuStyle::systemPalette(surface);
                    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive,
                                                            QPalette::Disabled}) {
                        for (const QPalette::ColorRole role : {QPalette::Base, QPalette::Text,
                                                               QPalette::Highlight, QPalette::HighlightedText})
                            QCOMPARE(surface->palette().color(group, role), expected.color(group, role));
                    }
                }
                const QImage image = view->viewport()->grab().toImage();
                int backgroundPixels = 0;
                for (int y = 0; y < image.height(); ++y) {
                    for (int x = 0; x < image.width(); ++x)
                        backgroundPixels += image.pixelColor(x, y).name() == native.color(QPalette::Base).name();
                }
                QVERIFY2(backgroundPixels > image.width() * image.height() / 3,
                         qPrintable(QStringLiteral("%1: expected %2, view %3, viewport %4, painted %5")
                             .arg(QString::fromLatin1(dockName), native.color(QPalette::Base).name(),
                                  view->palette().color(QPalette::Base).name(),
                                  view->viewport()->palette().color(QPalette::Base).name(),
                                  image.pixelColor(5, 5).name())));
            } else {
                QVERIFY(!view->styleSheet().isEmpty());
                QVERIFY(!view->verticalScrollBar()->styleSheet().isEmpty());
                QVERIFY(!popup->mask().isEmpty());
                QCOMPARE(view->frameShape(), QFrame::NoFrame);
                QVERIFY(view->palette().color(QPalette::Base) != native.color(QPalette::Base));
            }
            // Refresh an already-open dropdown without changing other controls.
            appearance->actions().at(1 - mode)->trigger();
            QCoreApplication::processEvents();
            QCOMPARE(view->styleSheet().isEmpty(), mode == 1);
        }
        appearance->actions().at(0)->trigger();
        combo->showPopup();
        const auto closePopup = qScopeGuard([combo]() { combo->hidePopup(); });
        for (const QPalette::ColorRole role : {QPalette::Window, QPalette::Base, QPalette::Button})
            native.setColor(role, QColor(darkSystem ? "#292e36" : "#e1f0fd"));
        native.setColor(QPalette::Highlight, QColor("#6850aa"));
        QApplication::setPalette(native);
        QCoreApplication::processEvents();
        QCOMPARE(view->palette().color(QPalette::Base), native.color(QPalette::Base));
        QCOMPARE(view->palette().color(QPalette::Highlight), native.color(QPalette::Highlight));
        guard.settings->customColorSchemeJson = themeJsonString(
            QJsonObject{{QStringLiteral("background"), QStringLiteral("#433229")}});
        window.colorSchemeChanged();
        QCoreApplication::processEvents();
        QVERIFY(view->styleSheet().isEmpty());
        QCOMPARE(view->palette().color(QPalette::Base), native.color(QPalette::Base));
        QCOMPARE(view->viewport()->palette().color(QPalette::Base), native.color(QPalette::Base));
    }
}

void TestDisplayUi::system_dropdowns_use_the_native_popup_selection_palette_data()
{
    QTest::addColumn<bool>("editable");
    QTest::newRow("menu-popup") << false;
    QTest::newRow("list-popup") << true;
}

void TestDisplayUi::system_dropdowns_use_the_native_popup_selection_palette()
{
    QFETCH(bool, editable);
    MainWindowStateGuard guard;
    const QPalette originalMenu = QApplication::palette("QMenu");
    const QPalette originalList = QApplication::palette("QListView");
    const auto restorePalettes = qScopeGuard([&]() {
        QApplication::setPalette(originalMenu, "QMenu");
        QApplication::setPalette(originalList, "QListView");
        MenuStyle::refresh();
    });
    QPalette nativeMenu = originalMenu;
    QPalette nativeList = originalList;
    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        nativeMenu.setColor(group, QPalette::Highlight, QColor("#427dc0"));
        nativeMenu.setColor(group, QPalette::HighlightedText, QColor("#fefefe"));
        nativeList.setColor(group, QPalette::Highlight, QColor("#1a4266"));
        nativeList.setColor(group, QPalette::HighlightedText, QColor("#eeeeee"));
    }
    QApplication::setPalette(nativeMenu, "QMenu");
    QApplication::setPalette(nativeList, "QListView");
    guard.settings->menuAppearance = Settings::MenuAppearanceSystem;
    guard.settings->constantsDockVisible = true;
    MainWindow window(false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* dock = window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(dock != nullptr);
    auto* combo = dock->findChild<QComboBox*>();
    QVERIFY(combo != nullptr);
    combo->setEditable(editable);
    QStyleOptionComboBox option;
    option.initFrom(combo);
    option.editable = editable;
    const bool menuPopup = QApplication::style()->styleHint(QStyle::SH_ComboBox_Popup, &option, combo);
    const QPalette expected = menuPopup ? nativeMenu : nativeList;
    QMenu* appearance = window.findChild<QMenu*>(QStringLiteral("MenuAppearanceMenu"));
    QVERIFY(appearance != nullptr);
    for (const int mode : {0, 1, 0}) {
        appearance->actions().at(mode)->trigger();
        combo->showPopup();
        const auto closePopup = qScopeGuard([combo]() { combo->hidePopup(); });
        QCoreApplication::processEvents();
        if (mode != 0)
            continue;
        for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
            QCOMPARE(combo->view()->palette().color(group, QPalette::Highlight),
                     expected.color(group, QPalette::Highlight));
            QCOMPARE(combo->view()->palette().color(group, QPalette::HighlightedText),
                     expected.color(group, QPalette::HighlightedText));
        }
        QStyleOptionViewItem selected;
        selected.initFrom(combo->view());
        selected.rect = QRect(0, 0, 240, 40);
        selected.state = QStyle::State_Active | QStyle::State_Enabled | QStyle::State_Selected;
        selected.palette.setCurrentColorGroup(QPalette::Active);
        QImage image(selected.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        combo->itemDelegate()->paint(&painter, selected, combo->model()->index(0, combo->modelColumn()));
        painter.end();
        QCOMPARE(image.pixelColor(220, 20).name(), expected.color(QPalette::Active, QPalette::Highlight).name());
    }
}

void TestDisplayUi::macos_system_menus_use_cocoa_style_and_selection_colors()
{
#if defined(Q_OS_MACOS)
    if (QApplication::platformName() != QLatin1String("cocoa")) {
        QTest::qSkip("Native macOS theme verification requires the Cocoa backend.", __FILE__, __LINE__);
        return;
    }
    MainWindowStateGuard guard;
    guard.settings->menuAppearance = Settings::MenuAppearanceSystem;
    guard.settings->constantsDockVisible = true;
    MainWindow window(false);
    QStyle* nativeStyle = QApplication::style();
    while (auto* proxy = qobject_cast<QProxyStyle*>(nativeStyle))
        nativeStyle = proxy->baseStyle();
    QCOMPARE(nativeStyle->objectName(), QStringLiteral("macos"));
    const QPalette nativeMenu = QApplication::palette("QMenu");
    QScopedPointer<QMenu> menu(window.findChild<Editor*>()->createStandardContextMenu());
    menu->ensurePolished();
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* dock = window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(dock != nullptr);
    auto* combo = dock->findChild<QComboBox*>();
    QVERIFY(combo != nullptr);
    combo->showPopup();
    const auto closePopup = qScopeGuard([combo]() { combo->hidePopup(); });
    QCoreApplication::processEvents();
    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        QCOMPARE(menu->palette().color(group, QPalette::Highlight), nativeMenu.color(group, QPalette::Highlight));
        QCOMPARE(combo->view()->palette().color(group, QPalette::Highlight),
                 nativeMenu.color(group, QPalette::Highlight));
        QCOMPARE(combo->view()->palette().color(group, QPalette::HighlightedText),
                 nativeMenu.color(group, QPalette::HighlightedText));
    }
#else
    QTest::qSkip("Native macOS theme verification requires macOS.", __FILE__, __LINE__);
#endif
}

void TestDisplayUi::editor_context_menu_paints_system_background_data()
{
    QTest::addColumn<QString>("editorBackground");
    QTest::newRow("dark-editor") << QStringLiteral("#232136");
    QTest::newRow("light-editor") << QStringLiteral("#e5eee8");
}

void TestDisplayUi::editor_context_menu_paints_system_background()
{
    QFETCH(QString, editorBackground);
    MainWindowStateGuard guard;
    guard.settings->menuAppearance = Settings::MenuAppearanceSystem;
    guard.settings->colorScheme = QStringLiteral("Custom");
    guard.settings->customColorSchemeJson = themeJsonString(
        QJsonObject{{QStringLiteral("background"), editorBackground}});
    MainWindow window(false);
    auto* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    const auto openContextMenu = [&]() {
        const QPoint pos(8, 8);
        QContextMenuEvent event(QContextMenuEvent::Mouse, pos,
                                editor->viewport()->mapToGlobal(pos));
        QCoreApplication::sendEvent(editor->viewport(), &event);
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu != nullptr)
            menu->setAttribute(Qt::WA_DeleteOnClose, false);
        return menu;
    };
    QScopedPointer<QMenu> context(openContextMenu());
    QVERIFY(context != nullptr);
    QCOMPARE(context->parentWidget(), editor->viewport());

    // Palette checks alone miss inherited stylesheets that suppress painting.
    const auto panelImage = [](QMenu* menu) {
        menu->ensurePolished();
        QImage image(180, 120, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QStyleOption option;
        option.initFrom(menu);
        option.rect = image.rect();
        option.state = QStyle::State_Enabled | QStyle::State_Active;
        QPainter painter(&image);
        menu->style()->drawPrimitive(QStyle::PE_PanelMenu, &option, &painter, menu);
        painter.end();
        return image;
    };
    const auto verifyBackground = [&](QMenu* menu) {
        QMenu reference;
        const QImage expected = panelImage(&reference);
        const QPoint center = expected.rect().center();
        QVERIFY(expected.pixelColor(center).alpha() > 0);
        const QImage actual = panelImage(menu);
        QCOMPARE(actual.pixelColor(center), expected.pixelColor(center));
    };
    verifyBackground(context.data());
    context->close();

    // Check both a retained popup and a newly created one after switching back.
    guard.settings->menuAppearance = Settings::MenuAppearanceSpeedCrunch;
    MenuStyle::refresh();
    context->ensurePolished();
    const QImage themed = panelImage(context.data());
    QCOMPARE(themed.pixelColor(themed.rect().center()).rgba(),
             context->palette().color(QPalette::Window).rgba());
    guard.settings->menuAppearance = Settings::MenuAppearanceSystem;
    MenuStyle::refresh();
    QCoreApplication::processEvents();
    verifyBackground(context.data());
    QScopedPointer<QMenu> fresh(openContextMenu());
    QVERIFY(fresh != nullptr);
    verifyBackground(fresh.data());
}

void TestDisplayUi::menu_appearance_defaults_to_system_and_persists()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    QCOMPARE(settings->menuAppearance, Settings::MenuAppearanceSystem);
    const QString scheme = settings->colorScheme;
    for (const Settings::MenuAppearance appearance : {Settings::MenuAppearanceSpeedCrunch,
                                                     Settings::MenuAppearanceSystem}) {
        settings->menuAppearance = appearance;
        settings->save();
        settings->menuAppearance = appearance == Settings::MenuAppearanceSystem
            ? Settings::MenuAppearanceSpeedCrunch : Settings::MenuAppearanceSystem;
        settings->load();
        QCOMPARE(settings->menuAppearance, appearance);
        QCOMPARE(settings->colorScheme, scheme);
    }
    QSettings persisted(Settings::getConfigPath() + QStringLiteral("/SpeedCrunch.ini"),
                        QSettings::IniFormat);
    const QString key = QStringLiteral("SpeedCrunch/Display/MenuAppearance");
    persisted.remove(key);
    persisted.sync();
    settings->load();
    QCOMPARE(settings->menuAppearance, Settings::MenuAppearanceSystem);
    persisted.setValue(key, 99);
    persisted.sync();
    settings->load();
    QCOMPARE(settings->menuAppearance, Settings::MenuAppearanceSystem);
}

void TestDisplayUi::menu_appearance_switches_all_windows_without_changing_other_controls()
{
    MainWindowStateGuard guard;
    guard.settings->colorScheme = QStringLiteral("Custom");
    guard.settings->customColorSchemeJson = themeJsonString(
        QJsonObject{{QStringLiteral("background"), QStringLiteral("#232136")}});
    guard.settings->constantsDockVisible = true;
    MainWindow first(false);
    MainWindow second(false);
    first.menuBar()->setNativeMenuBar(false);
    second.menuBar()->setNativeMenuBar(false);
    first.show();
    second.show();
    QVERIFY(QTest::qWaitForWindowExposed(&first));
    QVERIFY(QTest::qWaitForWindowExposed(&second));
    MenuStyle::refresh();
    QMenu* firstAppearance = first.findChild<QMenu*>(QStringLiteral("MenuAppearanceMenu"));
    QMenu* secondAppearance = second.findChild<QMenu*>(QStringLiteral("MenuAppearanceMenu"));
    QVERIFY(firstAppearance != nullptr);
    QVERIFY(secondAppearance != nullptr);
    QCOMPARE(firstAppearance->actions().size(), 2);
    QVERIFY(firstAppearance->actions().at(0)->isChecked());
    QVERIFY(secondAppearance->actions().at(0)->isChecked());
    Editor* editor = first.findChild<Editor*>();
    ResultDisplay* display = first.findChild<ResultDisplay*>();
    QDockWidget* constants = first.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(editor != nullptr);
    QVERIFY(display != nullptr);
    QVERIFY(constants != nullptr);
    const QPalette editorPalette = editor->palette();
    const QPalette displayPalette = display->palette();
    const QPalette dockPalette = constants->palette();
    const QString editorStyle = editor->styleSheet();

    // Keep a context menu alive across both switches to catch stale palettes
    // and styles that would otherwise survive until the next popup.
    QScopedPointer<QMenu> context(editor->createStandardContextMenu());
    context->ensurePolished();
    QVERIFY(context->styleSheet().isEmpty());
    firstAppearance->actions().at(1)->trigger();
    QCOMPARE(guard.settings->menuAppearance, Settings::MenuAppearanceSpeedCrunch);
    QVERIFY(firstAppearance->actions().at(1)->isChecked());
    QVERIFY(secondAppearance->actions().at(1)->isChecked());
    for (QMenuBar* bar : {first.menuBar(), second.menuBar()})
        QVERIFY(!bar->styleSheet().isEmpty());
    QVERIFY(!context->styleSheet().isEmpty());
    QMenu* child = context->addMenu(QStringLiteral("Nested"));
    child->addAction(QStringLiteral("Disabled"))->setEnabled(false);
    child->ensurePolished();
    QVERIFY(!child->styleSheet().isEmpty());
    QCOMPARE(child->palette().color(QPalette::Window).name(), context->palette().color(QPalette::Window).name());

    secondAppearance->actions().at(0)->trigger();
    QCOMPARE(guard.settings->menuAppearance, Settings::MenuAppearanceSystem);
    QVERIFY(firstAppearance->actions().at(0)->isChecked());
    QVERIFY(secondAppearance->actions().at(0)->isChecked());
    for (QWidget* menu : {static_cast<QWidget*>(first.menuBar()),
                          static_cast<QWidget*>(second.menuBar()),
                          static_cast<QWidget*>(context.data()), static_cast<QWidget*>(child)}) {
        QVERIFY(menu->styleSheet().isEmpty());
        QCOMPARE(menu->palette().color(QPalette::Window), QApplication::palette(menu).color(QPalette::Window));
        QCOMPARE(menu->palette().color(QPalette::Disabled, QPalette::WindowText),
                 QApplication::palette(menu).color(QPalette::Disabled, QPalette::WindowText));
    }
    QCOMPARE(editor->palette(), editorPalette);
    QCOMPARE(display->palette(), displayPalette);
    QCOMPARE(constants->palette(), dockPalette);
    QCOMPARE(editor->styleSheet(), editorStyle);
}

void TestDisplayUi::system_menus_restore_style_polished_foreground_and_hover_colors()
{
    MainWindowStateGuard guard;
    guard.settings->menuAppearance = Settings::MenuAppearanceSpeedCrunch;
    guard.settings->colorScheme = QStringLiteral("Custom");
    guard.settings->customColorSchemeJson = themeJsonString(
        QJsonObject{{QStringLiteral("background"), QStringLiteral("#232136")}});
    MainWindow window(false);
    QMenuBar* bar = window.menuBar();
    bar->setNativeMenuBar(false);
    QPalette native = QApplication::palette(bar);
    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        native.setColor(group, QPalette::Window, QColor("#eeeeee"));
        native.setColor(group, QPalette::Button, QColor("#eeeeee"));
        native.setColor(group, QPalette::ButtonText,
                        group == QPalette::Disabled ? QColor("#777777") : QColor("#242424"));
        native.setColor(group, QPalette::Highlight, QColor("#cccccc"));
        native.setColor(group, QPalette::HighlightedText, QColor("#242424"));
    }
    auto* style = new MenuBarPaletteTestStyle(native);
    style->setParent(bar);
    bar->setStyle(style);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QMenu* appearance = window.findChild<QMenu*>(QStringLiteral("MenuAppearanceMenu"));
    QVERIFY(appearance != nullptr);
    for (int cycle = 0; cycle < 3; ++cycle) {
        appearance->actions().at(1)->trigger();
        QCoreApplication::processEvents();
        appearance->actions().at(0)->trigger();
        QCoreApplication::processEvents();
        QVERIFY(bar->styleSheet().isEmpty());
        for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
            for (const QPalette::ColorRole role : {QPalette::ButtonText, QPalette::Highlight,
                                                   QPalette::HighlightedText})
                QCOMPARE(bar->palette().color(group, role), native.color(group, role));
        }
        for (const QStyle::State state : {QStyle::State(QStyle::State_Enabled | QStyle::State_Active),
                                          QStyle::State(QStyle::State_Enabled),
                                          QStyle::State(QStyle::State_None)}) {
            QStyleOptionMenuItem option;
            option.initFrom(bar);
            option.state = state;
            option.palette.setCurrentColorGroup(state & QStyle::State_Enabled
                ? state & QStyle::State_Active ? QPalette::Active : QPalette::Inactive : QPalette::Disabled);
            option.rect = QRect(0, 0, 200, 32);
            option.text = QStringLiteral("Menu foreground");
            option.font = bar->font();
            QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(native.color(QPalette::Window));
            QPainter painter(&image);
            bar->style()->drawControl(QStyle::CE_MenuBarItem, &option, &painter, bar);
            painter.end();
            QVERIFY(firstPixelMatchingColor(image, image.rect(),
                native.color(option.palette.currentColorGroup(), QPalette::ButtonText), 12) != QPoint(-1, -1));
        }
    }
}

void TestDisplayUi::gtk_menus_use_hover_colors_instead_of_the_list_accent_data()
{
    QTest::addColumn<bool>("dark");
    QTest::newRow("light-menu") << false;
    QTest::newRow("dark-menu") << true;
}

void TestDisplayUi::gtk_menus_use_hover_colors_instead_of_the_list_accent()
{
    QFETCH(bool, dark);
    // Model GTK's CSS states and Cairo's pixel buffer without requiring a
    // GNOME session. This also checks translucent disabled text and cleanup.
    struct Widget {
        unsigned state = 0;
        Widget* child = nullptr;
        bool destroyed = false;
    };
    static bool darkTheme;
    static int widgetCount;
    static int surfaceCount;
    darkTheme = dark;
    widgetCount = surfaceCount = 0;
    GtkMenuPalette::Api api{};
    api.popoverNew = [](void*) -> void* { ++widgetCount; return new Widget; };
    api.modelButtonNew = []() -> void* { ++widgetCount; return new Widget; };
    api.add = [](void* menu, void* item) { static_cast<Widget*>(menu)->child = static_cast<Widget*>(item); };
    api.context = [](void* widget) { return widget; };
    api.setState = [](void* widget, unsigned state) { static_cast<Widget*>(widget)->state = state; };
    api.color = [](void*, unsigned state, GtkMenuPalette::Rgba* color) {
        const double component = state & (1 << 3) ? 0 : darkTheme ? 238.0 / 255 : 36.0 / 255;
        *color = {component, component, component, state & (1 << 3) ? 0.5 : 1};
    };
    api.background = [](void* context, void* painter, double, double, double, double) {
        auto* widget = static_cast<Widget*>(context);
        auto* image = static_cast<QImage*>(painter);
        if (widget->child) {
            image->fill(QColor(darkTheme ? "#202020" : "#f0f0f0"));
        } else if (widget->state & (1 << 1)) {
            // GTK themes can express hover colors with translucent CSS.
            QPainter paint(image);
            paint.fillRect(image->rect(), darkTheme ? QColor(255, 255, 255, 32) : QColor(0, 0, 0, 32));
        }
    };
    api.destroyWidget = [](void* widget) {
        auto* menu = static_cast<Widget*>(widget);
        delete menu->child;
        menu->child = nullptr;
        menu->destroyed = true;
        --widgetCount;
    };
    api.refSink = [](void* widget) { return widget; };
    api.unref = [](void* widget) {
        auto* menu = static_cast<Widget*>(widget);
        Q_ASSERT(menu->destroyed);
        delete menu;
        --widgetCount;
    };
    api.surfaceForData = [](unsigned char* data, int format, int width, int height, int stride) -> void* {
        Q_ASSERT(format == 0);
        ++surfaceCount;
        return new QImage(data, width, height, stride, QImage::Format_ARGB32_Premultiplied);
    };
    api.createPainter = [](void* surface) { return surface; };
    api.destroyPainter = [](void*) {};
    api.flushSurface = [](void*) {};
    api.destroySurface = [](void* surface) { delete static_cast<QImage*>(surface); --surfaceCount; };
    QPalette fallback;
    fallback.setColor(QPalette::Highlight, QColor("#e95420"));
    const QPalette palette = GtkMenuPalette::read(api, fallback);
    const QColor fill(dark ? "#202020" : "#f0f0f0");
    const QColor selected(dark ? "#3c3c3c" : "#d2d2d2");
    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        QCOMPARE(palette.color(group, QPalette::Window), fill);
        QCOMPARE(palette.color(group, QPalette::Highlight), selected);
        if (group != QPalette::Disabled) {
            QCOMPARE(palette.color(group, QPalette::ButtonText), QColor(dark ? "#eeeeee" : "#242424"));
            QCOMPARE(palette.color(group, QPalette::HighlightedText), palette.color(group, QPalette::ButtonText));
        }
    }
    QCOMPARE(palette.color(QPalette::Disabled, QPalette::ButtonText),
             QColor(dark ? "#101010" : "#787878"));
    QCOMPARE(palette.color(QPalette::Link), fallback.color(QPalette::Link));
    QCOMPARE(fallback.color(QPalette::Highlight), QColor("#e95420"));
    QCOMPARE(widgetCount, 0);
    QCOMPARE(surfaceCount, 0);
}

void TestDisplayUi::system_menus_preserve_platform_roles_and_follow_palette_changes_data()
{
    QTest::addColumn<bool>("darkPlatform");
    QTest::newRow("dark-system-light-calculator") << true;
    QTest::newRow("light-system-dark-calculator") << false;
}

void TestDisplayUi::system_menus_preserve_platform_roles_and_follow_palette_changes()
{
    QFETCH(bool, darkPlatform);
    MainWindowStateGuard guard;
    const QPalette original = QApplication::palette();
    const QPalette originalMenu = QApplication::palette("QMenu");
    const QPalette originalBar = QApplication::palette("QMenuBar");
    const auto restorePalette = qScopeGuard([&]() {
        QApplication::setPalette(original);
        QApplication::setPalette(originalMenu, "QMenu");
        QApplication::setPalette(originalBar, "QMenuBar");
        MenuStyle::refresh();
    });
    const auto platformPalette = [&original](bool dark) {
        QPalette palette = original;
        for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive,
                                                QPalette::Disabled}) {
            const QColor fill(dark ? "#202428" : "#eeeeee");
            const QColor text(group == QPalette::Disabled ? "#808890" : (dark ? "#f0f0f0" : "#202020"));
            for (const QPalette::ColorRole role : {QPalette::Window, QPalette::Base, QPalette::Button})
                palette.setColor(group, role, fill);
            for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
                palette.setColor(group, role, text);
            palette.setColor(group, QPalette::Highlight, QColor("#354faf"));
            palette.setColor(group, QPalette::HighlightedText, QColor("#ffffff"));
        }
        return palette;
    };
    QPalette menuPalette = platformPalette(darkPlatform);
    QPalette barPalette = menuPalette;
    barPalette.setColor(QPalette::Highlight, QColor("#246044"));
    QApplication::setPalette(menuPalette, "QMenu");
    QApplication::setPalette(barPalette, "QMenuBar");
    guard.settings->colorScheme = QStringLiteral("Custom");
    guard.settings->customColorSchemeJson = themeJsonString(QJsonObject{
        {QStringLiteral("background"), darkPlatform ? QStringLiteral("#e5eee8") : QStringLiteral("#232136")}
    });
    MainWindow window(false);
    window.menuBar()->setNativeMenuBar(false);
    auto* transparentStyle = new TransparentMenuBarTestStyle;
    transparentStyle->setParent(window.menuBar());
    window.menuBar()->setStyle(transparentStyle);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    MenuStyle::refresh();
    QScopedPointer<QMenu> editorMenu(window.findChild<Editor*>()->createStandardContextMenu());
    editorMenu->ensurePolished();
    MenuTestResultDisplay result;
    QScopedPointer<QMenu> resultMenu(result.createContextMenu(QPoint()));
    resultMenu->ensurePolished();
    const auto verifyPalette = [](QWidget* widget, const QPalette& expected) {
        widget->ensurePolished();
        QVERIFY(widget->styleSheet().isEmpty());
        for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive,
                                                QPalette::Disabled}) {
            for (const QPalette::ColorRole role : {QPalette::Window, QPalette::Text,
                                                   QPalette::WindowText, QPalette::ButtonText,
                                                   QPalette::Highlight, QPalette::HighlightedText}) {
                QCOMPARE(widget->palette().color(group, role), expected.color(group, role));
            }
        }
    };
    verifyPalette(window.menuBar(), barPalette);
    const QImage barImage = window.menuBar()->grab().toImage();
    const QPoint backgroundPixel(barImage.width() - 4, barImage.height() / 2);
    QCOMPARE(barImage.pixelColor(backgroundPixel), barPalette.color(QPalette::Button));
    for (QMenu* menu : window.findChildren<QMenu*>())
        verifyPalette(menu, menuPalette);
    verifyPalette(editorMenu.data(), menuPalette);
    verifyPalette(resultMenu.data(), menuPalette);
    for (QMenu* submenu : resultMenu->findChildren<QMenu*>())
        verifyPalette(submenu, menuPalette);

    for (bool enabled : {true, false}) {
        QStyleOptionMenuItem option;
        option.initFrom(editorMenu.data());
        option.menuItemType = QStyleOptionMenuItem::Normal;
        option.rect = QRect(0, 0, 160, 28);
        option.text = QStringLiteral("Menu item");
        option.state = enabled ? QStyle::State(QStyle::State_Enabled | QStyle::State_Selected)
                               : QStyle::State(QStyle::State_None);
        option.palette.setCurrentColorGroup(enabled ? QPalette::Active : QPalette::Disabled);
        option.font = editorMenu->font();
        const QColor fill = menuPalette.color(enabled ? QPalette::Highlight : QPalette::Window);
        const QColor text = enabled ? menuPalette.color(QPalette::HighlightedText)
                                   : menuPalette.color(QPalette::Disabled, QPalette::Text);
        QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(fill);
        QPainter painter(&image);
        painter.setFont(option.font);
        editorMenu->style()->drawControl(QStyle::CE_MenuItem, &option, &painter, editorMenu.data());
        painter.end();
        QVERIFY(firstPixelMatchingColor(image, image.rect(), text, 12) != QPoint(-1, -1));
    }

    menuPalette = platformPalette(!darkPlatform);
    barPalette = menuPalette;
    barPalette.setColor(QPalette::Highlight, QColor("#246044"));
    QApplication::setPalette(menuPalette, "QMenu");
    QApplication::setPalette(barPalette, "QMenuBar");
    QCoreApplication::processEvents();
    verifyPalette(window.menuBar(), barPalette);
    verifyPalette(editorMenu.data(), menuPalette);
    verifyPalette(resultMenu.data(), menuPalette);
}

void TestDisplayUi::precision_menu_editor_uses_system_colors()
{
    MainWindowStateGuard guard;
    guard.settings->statusBarVisible = true;
    guard.settings->resultPrecision = -1;
    MainWindow window(false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    bool inspected = false;
    QTimer::singleShot(0, &window, [&]() {
        QMenu* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu == nullptr)
            return;
        const auto closeMenu = qScopeGuard([menu]() { menu->close(); });
        QSpinBox* spin = menu->findChild<QSpinBox*>();
        QVERIFY(spin != nullptr);
        QVERIFY(!spin->isEnabled());
        QVERIFY(spin->styleSheet().isEmpty());
        QVERIFY(menu->styleSheet().isEmpty());
        QCOMPARE(spin->palette().color(QPalette::Disabled, QPalette::Base),
                 QApplication::palette(spin).color(QPalette::Disabled, QPalette::Base));
        QCOMPARE(spin->palette().color(QPalette::Disabled, QPalette::Text),
                 QApplication::palette(spin).color(QPalette::Disabled, QPalette::Text));
        inspected = true;
    });
    QVERIFY(QMetaObject::invokeMethod(&window, "showPrecisionContextMenu", Qt::DirectConnection,
                                      Q_ARG(QPoint, QPoint())));
    QVERIFY(inspected);
}

void TestDisplayUi::dock_context_menus_follow_menu_theme_data()
{
    QTest::addColumn<bool>("darkSystem");
    QTest::addColumn<bool>("floating");
    QTest::newRow("dark-system-attached") << true << false;
    QTest::newRow("light-system-attached") << false << false;
    QTest::newRow("dark-system-floating") << true << true;
    QTest::newRow("light-system-floating") << false << true;
}

void TestDisplayUi::dock_context_menus_follow_menu_theme()
{
    QFETCH(bool, darkSystem);
    QFETCH(bool, floating);
    MainWindowStateGuard guard;
    const QPalette originalMenuPalette = QApplication::palette("QMenu");
    const auto restorePalette = qScopeGuard([&]() {
        QApplication::setPalette(originalMenuPalette, "QMenu");
        MenuStyle::refresh();
    });
    QPalette native = originalMenuPalette;
    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        const QColor background(darkSystem ? "#202428" : "#eeeeee");
        const QColor foreground(group == QPalette::Disabled ? "#808890"
                                                            : (darkSystem ? "#f0f0f0" : "#202020"));
        for (const QPalette::ColorRole role : {QPalette::Window, QPalette::Base, QPalette::Button})
            native.setColor(group, role, background);
        for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
            native.setColor(group, role, foreground);
        native.setColor(group, QPalette::Highlight, QColor("#354faf"));
        native.setColor(group, QPalette::HighlightedText, QColor("#ffffff"));
    }
    QApplication::setPalette(native, "QMenu");
    const QColor calculatorBackground(darkSystem ? "#e5eee8" : "#232136");
    guard.settings->colorScheme = QStringLiteral("Custom");
    guard.settings->customColorSchemeJson = themeJsonString(
        QJsonObject{{QStringLiteral("background"), calculatorBackground.name()}});
    guard.settings->historyDockVisible = true;
    guard.settings->variablesDockVisible = true;
    guard.settings->userFunctionsDockVisible = true;
    guard.settings->userUnitsDockVisible = true;
    Session historySession;
    historySession.addHistoryEntry(HistoryEntry(QStringLiteral("1+1"), Quantity(2)));
    MainWindow window(false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QMenu* appearance = window.findChild<QMenu*>(QStringLiteral("MenuAppearanceMenu"));
    QVERIFY(appearance != nullptr);
    HistoryWidget* history = window.findChild<HistoryWidget*>();
    QVERIFY(history != nullptr);
    history->setSession(&historySession);

    const QVector<QColor> shades = generateOklchShades(
        calculatorBackground, 6, themePolarityForBackground(calculatorBackground));
    for (const char* dockName : {"HistoryDock", "VariablesDock", "UserFunctionsDock", "UserUnitsDock"}) {
        QDockWidget* dock = window.findChild<QDockWidget*>(QString::fromLatin1(dockName));
        QVERIFY(dock != nullptr);
        dock->setFloating(floating);
        dock->show();
        dock->raise();
        QCoreApplication::processEvents();
        QAbstractItemView* list = dock->findChild<QAbstractItemView*>();
        QVERIFY(list != nullptr);
        const QPalette listPalette = list->palette();
        const QString listStyle = list->styleSheet();
        for (const int mode : {0, 1, 0}) {
            appearance->actions().at(mode)->trigger();
            const QColor expectedBackground = mode == 0 ? native.color(QPalette::Window)
                                                        : shades.at(UiConfig::DockHeaderShade);
            const auto inspectPopup = [&](QWidget* target, const QPoint& pos) {
                bool inspected = false;
                QTimer inspector;
                inspector.setSingleShot(true);
                connect(&inspector, &QTimer::timeout, &window, [&]() {
                    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                    QVERIFY2(menu != nullptr, dockName);
                    const auto closeMenu = qScopeGuard([menu]() { menu->close(); });
                    QVERIFY(!menu->actions().isEmpty());
                    QCOMPARE(menu->palette().color(QPalette::Window).name(), expectedBackground.name());
                    if (mode == 0) {
                        QVERIFY(menu->styleSheet().isEmpty());
                        for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive,
                                                                QPalette::Disabled}) {
                            for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text,
                                                                   QPalette::Highlight, QPalette::HighlightedText})
                                QCOMPARE(menu->palette().color(group, role), native.color(group, role));
                        }
                    } else {
                        QVERIFY(!menu->styleSheet().isEmpty());
                    }
                    // Check the pixels too. An inherited stylesheet can replace
                    // a correct menu palette when the popup is painted.
                    const QImage image = menu->grab().toImage();
                    QColor paintedBackground = expectedBackground;
                    if (mode == 0) {
                        // Native styles may shade the palette's background.
                        // Compare with the same style without a themed parent.
                        QMenu reference;
                        reference.ensurePolished();
                        QStyleOption option;
                        option.initFrom(&reference);
                        option.rect = image.rect();
                        QImage nativeBackground(image.size(), QImage::Format_ARGB32_Premultiplied);
                        nativeBackground.fill(native.color(QPalette::Window));
                        QPainter painter(&nativeBackground);
                        reference.style()->drawPrimitive(QStyle::PE_PanelMenu, &option, &painter, &reference);
                        painter.end();
                        paintedBackground = nativeBackground.pixelColor(nativeBackground.rect().center());
                    }
                    const QRect interior = image.rect().adjusted(5, 5, -5, -5);
                    int matchingPixels = 0;
                    for (int y = interior.top(); y <= interior.bottom(); ++y) {
                        for (int x = interior.left(); x <= interior.right(); ++x) {
                            if (image.pixelColor(x, y).name() == paintedBackground.name())
                                ++matchingPixels;
                        }
                    }
                    QVERIFY2(matchingPixels > interior.width() * interior.height() / 2, dockName);
                    inspected = true;
                });
                inspector.start(0);
                QContextMenuEvent event(QContextMenuEvent::Mouse, pos, target->mapToGlobal(pos));
                QCoreApplication::sendEvent(target, &event);
                QCoreApplication::processEvents();
                inspector.stop();
                QVERIFY2(inspected, dockName);
            };
            const QPoint listPos = list->model()->rowCount() > 0
                ? list->visualRect(list->model()->index(0, 0)).center() : QPoint(8, 8);
            inspectPopup(list->viewport(), listPos);
            if (QLineEdit* search = dock->findChild<QLineEdit*>())
                inspectPopup(search, QPoint(8, 8));
            QCOMPARE(list->palette(), listPalette);
            QCOMPARE(list->styleSheet(), listStyle);
        }
    }
}

void TestDisplayUi::restored_session_layout_reapplies_generated_theme_surfaces()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;
        QString oldColorScheme;
        QString oldCustomColorSchemeJson;
        QString oldSessionLayoutJson;
        Settings::KeypadMode oldKeypadMode;
        bool oldStatusBarVisible;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldWindowPositionSave;
        bool oldHasNumberFormatStyleSetting;

        ~SettingsGuard()
        {
            settings->colorScheme = oldColorScheme;
            settings->customColorSchemeJson = oldCustomColorSchemeJson;
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->keypadMode = oldKeypadMode;
            settings->statusBarVisible = oldStatusBarVisible;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->windowPositionSave = oldWindowPositionSave;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        settings->colorScheme,
        settings->customColorSchemeJson,
        settings->sessionLayoutJson,
        settings->keypadMode,
        settings->statusBarVisible,
        settings->constantsDockVisible,
        settings->functionsDockVisible,
        settings->historyDockVisible,
        settings->keypadVisible,
        settings->formulaBookDockVisible,
        settings->variablesDockVisible,
        settings->userFunctionsDockVisible,
        settings->userUnitsDockVisible,
        settings->bitfieldVisible,
        settings->windowPositionSave,
        settings->hasNumberFormatStyleSetting
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{{QStringLiteral("background"), QStringLiteral("#300a24")}});
    settings->sessionLayoutJson.clear();
    settings->keypadMode = Settings::KeypadModeBasicWide;
    settings->statusBarVisible = true;
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = true;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    {
        MainWindow sourceWindow;
        sourceWindow.show();
        QCoreApplication::processEvents();
        QVERIFY(QMetaObject::invokeMethod(&sourceWindow, "splitActivePaneRight",
                                          Qt::DirectConnection));
        QCoreApplication::processEvents();
        QTRY_COMPARE(sourceWindow.findChildren<ResultDisplay*>().size(), 2);
        sourceWindow.persistSessionAndSettingsForShutdown();
    }

    const QString restoredLayout = settings->sessionLayoutJson;
    QVERIFY(!restoredLayout.isEmpty());

    MainWindow restoredWindow;
    restoredWindow.show();
    QCoreApplication::processEvents();
    QTRY_COMPARE(restoredWindow.findChildren<ResultDisplay*>().size(), 2);

    const QVector<QColor> shades =
        generateOklchShades(QColor(QStringLiteral("#300a24")), 6, ThemePolarity::Dark);
    const QColor paneFill = shades.at(UiConfig::ResultDisplayShade);
    const QColor chromeFill = shades.at(UiConfig::WindowBackgroundShade);
    const QColor keypadFill = shades.at(UiConfig::KeypadBackgroundShade);
    const QColor editorFill = shades.at(UiConfig::DockBackgroundShade);

    QSplitter* splitContainer =
        restoredWindow.findChild<QSplitter*>(QStringLiteral("MainSplitContainer"));
    QVERIFY(splitContainer != nullptr);
    QCOMPARE(splitContainer->palette().color(QPalette::Window).name(), chromeFill.name());

    for (ResultDisplay* display : restoredWindow.findChildren<ResultDisplay*>()) {
        QCOMPARE(display->palette().color(QPalette::Base).name(), paneFill.name());
        QCOMPARE(display->viewport()->palette().color(QPalette::Base).name(),
                 paneFill.name());
        const QImage displayImage = display->viewport()->grab().toImage();
        QVERIFY(!displayImage.isNull());
        QCOMPARE(displayImage.pixelColor(displayImage.width() / 2,
                                         displayImage.height() / 2).name(),
                 paneFill.name());
        QWidget* pane = paneWidgetForDisplay(display);
        QVERIFY(pane != nullptr);
        QCOMPARE(pane->palette().color(QPalette::Window).name(), paneFill.name());
    }

    for (Editor* editor : restoredWindow.findChildren<Editor*>())
        QCOMPARE(editor->palette().color(QPalette::Base).name(), editorFill.name());

    Keypad* keypad = restoredWindow.findChild<Keypad*>();
    QVERIFY(keypad != nullptr);
    QCOMPARE(keypad->palette().color(QPalette::Window).name(), keypadFill.name());
    QWidget* keypadContainer = keypad->parentWidget();
    QVERIFY(keypadContainer != nullptr);
    QCOMPARE(keypadContainer->palette().color(QPalette::Window).name(), keypadFill.name());
}

void TestDisplayUi::always_on_top_toggles_preserve_window_geometry()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    const bool oldAlwaysOnTop = settings->windowAlwaysOnTop;
    const bool oldFullScreen = settings->windowOnfullScreen;
    const auto restoreWindowSettings = qScopeGuard([settings, oldAlwaysOnTop, oldFullScreen]() {
        settings->windowAlwaysOnTop = oldAlwaysOnTop;
        settings->windowOnfullScreen = oldFullScreen;
    });
    settings->windowAlwaysOnTop = false;
    settings->windowOnfullScreen = false;
    settings->hasNumberFormatStyleSetting = true;
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();

    MainWindow window(false);
    window.resize(640, 480);
    window.move(100, 100);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    QAction* alwaysOnTop = nullptr;
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->text() == QStringLiteral("Always on &Top")) {
            alwaysOnTop = action;
            break;
        }
    }
    QVERIFY(alwaysOnTop != nullptr);
    QVERIFY(!alwaysOnTop->isChecked());
    const QRect originalFrame = window.frameGeometry();
    const QRect originalGeometry = window.geometry();
    const Qt::WindowFlags originalFlags = window.windowFlags();

    for (int toggle = 0; toggle < 10; ++toggle) {
        const bool enabled = toggle % 2 == 0;
        alwaysOnTop->trigger();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCoreApplication::processEvents();

        QCOMPARE(alwaysOnTop->isChecked(), enabled);
        QCOMPARE(settings->windowAlwaysOnTop, enabled);
        QCOMPARE(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint), enabled);
        QCOMPARE(window.windowFlags() & ~Qt::WindowStaysOnTopHint, originalFlags);
        QCOMPARE(window.frameGeometry(), originalFrame);
        QCOMPARE(window.geometry(), originalGeometry);
    }
}

void TestDisplayUi::saved_window_ui_state_overrides_defaults_before_show()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->statusBarVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    QByteArray docklessWindowState;
    {
        MainWindow sourceWindow(false);
        docklessWindowState = sourceWindow.saveState(1);
    }
    QVERIFY(!docklessWindowState.isEmpty());

    QMainWindow compactGeometrySource;
    compactGeometrySource.resize(560, 380);
    const QSize compactSize = compactGeometrySource.size();
    const QByteArray compactGeometry = compactGeometrySource.saveGeometry();
    QVERIFY(!compactGeometry.isEmpty());

    QJsonObject tab {
        { QStringLiteral("name"), QStringLiteral("Missing session") },
        { QStringLiteral("file"), QStringLiteral("missing-window-ui-state.json") }
    };
    QJsonObject root {
        { QStringLiteral("type"), QStringLiteral("tabs") },
        { QStringLiteral("active"), QStringLiteral("Missing session") },
        { QStringLiteral("tabs"), QJsonArray({ tab }) }
    };
    QJsonObject savedWindow {
        { QStringLiteral("id"), QStringLiteral("window-0") },
        { QStringLiteral("active"), true },
        { QStringLiteral("root"), root },
        { QStringLiteral("statusBarVisible"), false },
        { QStringLiteral("keypadVisible"), false },
        { QStringLiteral("bitfieldVisible"), false },
        { QStringLiteral("windowState"),
          QString::fromLatin1(docklessWindowState.toBase64()) },
        { QStringLiteral("geometry"),
          QString::fromLatin1(compactGeometry.toBase64()) }
    };
    QJsonObject layout {
        { QStringLiteral("scheme"), 1 },
        { QStringLiteral("kind"), QStringLiteral("session-layout") },
        { QStringLiteral("activeWindow"), QStringLiteral("window-0") },
        { QStringLiteral("windows"), QJsonArray({ savedWindow }) }
    };
    settings->sessionLayoutJson = QString::fromUtf8(
        QJsonDocument(layout).toJson(QJsonDocument::Compact));

    // Deliberately conflicting defaults must never become the visible state of
    // a window that has its own saved UI record.
    settings->constantsDockVisible = true;
    settings->keypadMode = Settings::KeypadModeBasicWide;
    settings->keypadVisible = true;
    settings->statusBarVisible = true;

    MainWindow restoredWindow;

    QStatusBar* statusBar =
        restoredWindow.findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly);
    QDockWidget* constantsDock =
        restoredWindow.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(statusBar == nullptr || statusBar->isHidden());
    QVERIFY(restoredWindow.findChild<Keypad*>() == nullptr);
    QVERIFY(constantsDock != nullptr);
    QVERIFY(constantsDock->isHidden());
    QCOMPARE(restoredWindow.size(), compactSize);
}

void TestDisplayUi::visible_window_applies_restored_dock_and_keypad_layout()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadMode = Settings::KeypadModeBasicWide;
    settings->keypadVisible = true;
    settings->keypadZoomPercent = 100;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    QByteArray docklessWindowState;
    {
        MainWindow sourceWindow(false);
        sourceWindow.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sourceWindow));
        docklessWindowState = sourceWindow.saveState(1);
    }
    QVERIFY(!docklessWindowState.isEmpty());

    // Global settings describe the primary window. A secondary window can
    // restore after it is shown, and must replace that inherited dock state.
    settings->constantsDockVisible = true;

    MainWindow restoredWindow(false);
    restoredWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&restoredWindow));

    QDockWidget* constantsDock =
        restoredWindow.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QVERIFY(constantsDock->isVisible());
    QVERIFY(QMetaObject::invokeMethod(&restoredWindow,
                                      "restoreWindowLayoutState",
                                      Qt::DirectConnection,
                                      Q_ARG(QByteArray, docklessWindowState)));
    QVERIFY(!constantsDock->isVisible());

    QAction* restoredBasicAction =
        keypadModeAction(&restoredWindow, Settings::KeypadModeBasicWide);
    QVERIFY(restoredBasicAction != nullptr);
    QVERIFY(restoredBasicAction->isChecked());
    QVERIFY(restoredWindow.findChild<Keypad*>() != nullptr);
    QVERIFY(QMetaObject::invokeMethod(&restoredWindow,
                                      "restoreWindowKeypadZoom",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 150)));

    MainWindow secondaryWindow(false);
    secondaryWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&secondaryWindow));
    QVERIFY(QMetaObject::invokeMethod(
        &secondaryWindow,
        "restoreWindowKeypadLayout",
        Qt::DirectConnection,
        Q_ARG(bool, true),
        Q_ARG(int, static_cast<int>(Settings::KeypadModeScientificNarrow))));
    QVERIFY(QMetaObject::invokeMethod(&secondaryWindow,
                                      "restoreWindowKeypadZoom",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 200)));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    QAction* secondaryScientificNarrowAction =
        keypadModeAction(&secondaryWindow, Settings::KeypadModeScientificNarrow);
    QVERIFY(secondaryScientificNarrowAction != nullptr);
    QVERIFY(secondaryScientificNarrowAction->isChecked());
    QVERIFY(secondaryWindow.findChild<Keypad*>() != nullptr);
    QVERIFY(restoredBasicAction->isChecked());
    QVERIFY(restoredWindow.findChild<Keypad*>() != nullptr);

    restoredWindow.persistSessionAndSettingsForShutdown();
    const QJsonDocument savedLayout =
        QJsonDocument::fromJson(settings->sessionLayoutJson.toUtf8());
    QVERIFY(savedLayout.isObject());
    const QJsonArray savedWindows =
        savedLayout.object().value(QStringLiteral("windows")).toArray();
    QCOMPARE(savedWindows.size(), 2);
    bool savedBasicKeypad = false;
    bool savedScientificNarrowKeypad = false;
    for (const QJsonValue& value : savedWindows) {
        const QJsonObject window = value.toObject();
        QVERIFY(window.value(QStringLiteral("keypadVisible")).toBool(false));
        const int mode = window.value(QStringLiteral("keypadMode")).toInt(-1);
        const int zoomPercent =
            window.value(QStringLiteral("keypadZoomPercent")).toInt(-1);
        savedBasicKeypad = savedBasicKeypad
            || (mode == static_cast<int>(Settings::KeypadModeBasicWide)
                && zoomPercent == 150);
        savedScientificNarrowKeypad = savedScientificNarrowKeypad
            || (mode == static_cast<int>(Settings::KeypadModeScientificNarrow)
                && zoomPercent == 200);
    }
    QVERIFY(savedBasicKeypad);
    QVERIFY(savedScientificNarrowKeypad);

    QVERIFY(QMetaObject::invokeMethod(
        &secondaryWindow,
        "restoreWindowKeypadLayout",
        Qt::DirectConnection,
        Q_ARG(bool, false),
        Q_ARG(int, static_cast<int>(Settings::KeypadModeScientificNarrow))));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    QVERIFY(secondaryWindow.findChild<Keypad*>() == nullptr);
    QAction* secondaryDisabledAction =
        keypadModeAction(&secondaryWindow, Settings::KeypadModeDisabled);
    QVERIFY(secondaryDisabledAction != nullptr);
    QVERIFY(secondaryDisabledAction->isChecked());
    QVERIFY(restoredBasicAction->isChecked());
    QVERIFY(restoredWindow.findChild<Keypad*>() != nullptr);

    QMainWindow compactGeometrySource;
    compactGeometrySource.resize(560, 380);
    compactGeometrySource.show();
    QVERIFY(QTest::qWaitForWindowExposed(&compactGeometrySource));
    const QSize compactSize = compactGeometrySource.size();
    const QByteArray compactGeometry = compactGeometrySource.saveGeometry();
    QVERIFY(!compactGeometry.isEmpty());

    settings->constantsDockVisible = true;
    settings->keypadMode = Settings::KeypadModeBasicWide;
    settings->windowState.clear();
    MainWindow compactWindow(false);
    compactWindow.resize(1000, 700);
    QDockWidget* compactConstantsDock =
        compactWindow.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(compactConstantsDock != nullptr);
    QVERIFY(!compactConstantsDock->isHidden());
    QVERIFY(compactWindow.findChild<Keypad*>() != nullptr);

    QVERIFY(QMetaObject::invokeMethod(&compactWindow,
                                      "restoreWindowLayoutState",
                                      Qt::DirectConnection,
                                      Q_ARG(QByteArray, docklessWindowState)));
    QVERIFY(QMetaObject::invokeMethod(
        &compactWindow,
        "restoreWindowKeypadLayout",
        Qt::DirectConnection,
        Q_ARG(bool, false),
        Q_ARG(int, static_cast<int>(Settings::KeypadModeBasicWide))));
    QVERIFY(QMetaObject::invokeMethod(&compactWindow,
                                      "showRestoredWindow",
                                      Qt::DirectConnection,
                                      Q_ARG(QByteArray, compactGeometry)));

    QVERIFY(!compactWindow.isVisible());
    QTRY_VERIFY(compactWindow.isVisible());
    QVERIFY(QTest::qWaitForWindowExposed(&compactWindow));
    QTRY_VERIFY(!compactConstantsDock->isVisible());
    QTRY_VERIFY(compactWindow.findChild<Keypad*>() == nullptr);
    QTRY_COMPARE(compactWindow.size(), compactSize);
}

void TestDisplayUi::dock_surfaces_use_successive_generated_shades()
{
    Settings* settings = Settings::instance();
    const Settings::MenuAppearance oldMenuAppearance = settings->menuAppearance;
    const auto restoreMenuAppearance = qScopeGuard([settings, oldMenuAppearance]() {
        settings->menuAppearance = oldMenuAppearance;
    });
    settings->menuAppearance = Settings::MenuAppearanceSpeedCrunch;
    struct SettingsGuard {
        Settings* settings;
        QString oldColorScheme;
        QString oldCustomColorSchemeJson;
        QByteArray oldWindowState;
        bool oldConstantsDockVisible;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->colorScheme = oldColorScheme;
            settings->customColorSchemeJson = oldCustomColorSchemeJson;
            settings->windowState = oldWindowState;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->colorScheme,
        settings->customColorSchemeJson,
        settings->windowState,
        settings->constantsDockVisible,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{{QStringLiteral("background"), QStringLiteral("#1f3229")}});
    settings->windowState.clear();
    settings->constantsDockVisible = true;
    settings->hasNumberFormatStyleSetting = true;

    const QVector<QColor> shades =
        generateOklchShades(QColor(QStringLiteral("#1f3229")), 6, ThemePolarity::Dark);
    const QVector<QColor> foregrounds = aaForegroundsForBackgrounds(shades);
    const QColor primary = generatePrimaryFromBackground(QColor(QStringLiteral("#1f3229")));

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    const QColor titleFill = shades.at(UiConfig::DockHeaderShade);
    const QColor titleText = foregrounds.at(UiConfig::DockHeaderShade);
    const QColor headerButtonFill = shades.at(UiConfig::DockHeaderButtonFillShade);
    const QColor headerButtonText = foregrounds.at(UiConfig::DockHeaderButtonFillShade);
    const QColor headerButtonHoverFill =
        shades.at(UiConfig::DockHeaderButtonHoverFillShade);
    const QColor headerButtonHoverText =
        foregrounds.at(UiConfig::DockHeaderButtonHoverFillShade);
    const QColor controlFill = shades.at(4);
    const QColor controlText = foregrounds.at(4);
    const QColor contentFill = shades.at(2);
    const QColor contentText = foregrounds.at(2);
    const QColor hoveredItemFill = shades.at(UiConfig::DockHoveredItemShade);
    const QColor hoveredItemText = foregrounds.at(UiConfig::DockHoveredItemShade);
    const int comboPopupShade = qMin(UiConfig::DockBackgroundShade + 1, UiConfig::Shade600);
    const QColor comboPopupFill = shades.at(comboPopupShade);
    const QColor comboPopupText = foregrounds.at(comboPopupShade);
    const QColor completionPopupFill = shades.at(UiConfig::CompletionPopupBackgroundShade);
    const QColor completionPopupText = foregrounds.at(UiConfig::CompletionPopupBackgroundShade);
    const QColor completionPopupOutlineFill = shades.at(UiConfig::CompletionPopupOutlineShade);
    const QColor chromeFill = shades.at(UiConfig::WindowBackgroundShade);
    const QColor chromeText = foregrounds.at(UiConfig::WindowBackgroundShade);
    const QColor resultFill = shades.at(UiConfig::ResultDisplayShade);
    const QColor resultText = foregrounds.at(UiConfig::ResultDisplayShade);
    const QColor hoverFill = shades.at(5);
    const QColor textInputOutlineFill = shades.at(UiConfig::DockTextInputOutlineShade);
    const QColor scrollToBottomOutlineFill =
        shades.at(UiConfig::ScrollToBottomButtonOutlineShade);
    const QColor splitterFill = shades.at(UiConfig::SplitterShade);

    const QList<QDockWidget*> docks = window.findChildren<QDockWidget*>();
    QVERIFY(!docks.isEmpty());
    QSplitter* splitContainer =
        window.findChild<QSplitter*>(QStringLiteral("MainSplitContainer"));
    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(splitContainer != nullptr);
    QVERIFY(display != nullptr);
    QWidget* pane = display->parentWidget();
    Editor* editor = pane ? pane->findChild<Editor*>() : nullptr;
    QVERIFY(editor != nullptr);
    QVERIFY(splitContainer->styleSheet().contains(splitterFill.name()));
    QVERIFY(splitContainer->styleSheet().contains(primary.name()));
    QVERIFY(splitContainer->styleSheet().contains(QStringLiteral("QSplitter::handle:hover")));
    QVERIFY(splitContainer->styleSheet().contains(QStringLiteral("QSplitter::handle:pressed")));
    const QList<QSplitterHandle*> splitterHandles = window.findChildren<QSplitterHandle*>();
    QVERIFY(!splitterHandles.isEmpty());
    for (QSplitterHandle* splitterHandle : splitterHandles) {
        QVERIFY(splitterHandle->styleSheet().contains(splitterFill.name()));
        QVERIFY(splitterHandle->styleSheet().contains(primary.name()));
        QVERIFY(splitterHandle->styleSheet().contains(QStringLiteral("QSplitterHandle:hover")));
        QVERIFY(splitterHandle->styleSheet().contains(QStringLiteral("QSplitterHandle:pressed")));
    }
    QCOMPARE(window.property("speedcrunchDockSeparatorNormalColor").value<QColor>(), splitterFill);
    QCOMPARE(window.property("speedcrunchDockSeparatorActiveColor").value<QColor>(), primary);
    const QString resultScrollBarStyle = display->verticalScrollBar()->styleSheet();
    QVERIFY(resultScrollBarStyle.contains(shades.at(1).name()));
    QVERIFY(resultScrollBarStyle.contains(contentFill.name()));
    QVERIFY(resultScrollBarStyle.contains(titleFill.name()));
    QVERIFY(resultScrollBarStyle.contains(controlFill.name()));
    QToolButton* scrollToBottomButton =
        display->findChild<QToolButton*>(QStringLiteral("ScrollToBottomButton"));
    QVERIFY(scrollToBottomButton != nullptr);
    QVERIFY(scrollToBottomButton->styleSheet().contains(contentFill.name()));
    QVERIFY(scrollToBottomButton->styleSheet().contains(titleFill.name()));
    QVERIFY(scrollToBottomButton->styleSheet().contains(QStringLiteral("border: %1px solid %2")
                                                            .arg(UiConfig::OutlineStrokeWidth)
                                                            .arg(scrollToBottomOutlineFill.name())));
    QVERIFY(!scrollToBottomButton->icon().isNull());
    for (QDockWidget* dock : docks) {
        QCOMPARE(dock->palette().color(QPalette::Window).name(), titleFill.name());
        QCOMPARE(dock->palette().color(QPalette::WindowText).name(), titleText.name());
        QVERIFY(dock->styleSheet().contains(titleFill.name()));
        QVERIFY(dock->styleSheet().contains(titleText.name()));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("padding: 5px 4px")));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("QDockWidget::close-button")));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("QDockWidget::float-button")));
        QVERIFY(dock->styleSheet().contains(headerButtonFill.name()));
        QVERIFY(dock->styleSheet().contains(headerButtonText.name()));
        QVERIFY(dock->styleSheet().contains(headerButtonHoverFill.name()));
        QVERIFY(dock->styleSheet().contains(headerButtonHoverText.name()));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("border: none")));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("border-radius: 9px")));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("width: 18px")));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("height: 18px")));
    }

    for (QDockWidget* dock : docks) {
        QWidget* dockContent = dock->widget();
        QVERIFY(dockContent != nullptr);
        const QList<QComboBox*> comboBoxes = dockContent->findChildren<QComboBox*>();
        for (QAbstractItemView* view : dockContent->findChildren<QAbstractItemView*>()) {
            if (qobject_cast<QHeaderView*>(view))
                continue;

            bool comboPopup = false;
            for (const QComboBox* comboBox : comboBoxes)
                comboPopup = comboPopup || comboBox->view() == view || comboBox->isAncestorOf(view);
            if (comboPopup)
                continue;

            QVERIFY(view->parentWidget() != nullptr);
            QVERIFY(view->parentWidget()->layout() != nullptr);
            const QMargins viewMargins = view->parentWidget()->layout()->contentsMargins();
            if (viewMargins != QMargins(0, 0, 0, 0)) {
                const QString message = QStringLiteral("%1 in %2 has list/table margins %3,%4,%5,%6")
                    .arg(QString::fromLatin1(view->metaObject()->className()),
                         dock->objectName())
                    .arg(viewMargins.left())
                    .arg(viewMargins.top())
                    .arg(viewMargins.right())
                    .arg(viewMargins.bottom());
                QFAIL(qPrintable(message));
            }
        }
    }

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QDockWidget* functionsDock =
        window.findChild<QDockWidget*>(QStringLiteral("FunctionsDock"));
    QVERIFY(constantsDock != nullptr);
    QVERIFY(functionsDock != nullptr);
    ConstantsWidget* constantsWidget = qobject_cast<ConstantsWidget*>(constantsDock->widget());
    QVERIFY(constantsWidget != nullptr);
    bool foundCloseButton = false;
    bool foundFloatButton = false;
    for (QAbstractButton* button : constantsDock->findChildren<QAbstractButton*>()) {
        const QString name = button->objectName();
        if (name != QStringLiteral("qt_dockwidget_closebutton")
            && name != QStringLiteral("qt_dockwidget_floatbutton")) {
            continue;
        }
        foundCloseButton |= name == QStringLiteral("qt_dockwidget_closebutton");
        foundFloatButton |= name == QStringLiteral("qt_dockwidget_floatbutton");
        QCOMPARE(button->palette().color(QPalette::Button).name(), headerButtonFill.name());
        QCOMPARE(button->palette().color(QPalette::ButtonText).name(), headerButtonText.name());
        QCOMPARE(button->cursor().shape(), Qt::PointingHandCursor);
        QVERIFY(button->hasMouseTracking());
        QCOMPARE(button->minimumSize(), QSize(18, 18));
        QCOMPARE(button->maximumSize(), QSize(18, 18));
        QCOMPARE(button->iconSize(), QSize(18, 18));
        QVERIFY(button->styleSheet().contains(headerButtonFill.name()));
        QVERIFY(button->styleSheet().contains(headerButtonText.name()));
        QVERIFY(button->styleSheet().contains(headerButtonHoverFill.name()));
        QVERIFY(button->styleSheet().contains(headerButtonHoverText.name()));
        QVERIFY(button->styleSheet().contains(QStringLiteral("border-radius: 9px")));
        QVERIFY(!button->icon().isNull());
        const QImage iconImage = button->icon().pixmap(QSize(18, 18)).toImage();
        QVERIFY(!iconImage.isNull());
        QVERIFY(iconImage.pixelColor(0, 0).alpha() < 32);
        QVERIFY2(colorsAreClose(iconImage.pixelColor(2, 9), headerButtonFill, 3),
                 qPrintable(QStringLiteral("icon fill %1 expected %2")
                                .arg(iconImage.pixelColor(2, 9).name(),
                                     headerButtonFill.name())));
        const QPoint buttonCenter = button->rect().center();
        QMouseEvent buttonMoveEvent(QEvent::MouseMove,
                                    QPointF(buttonCenter),
                                    QPointF(button->mapToGlobal(buttonCenter)),
                                    Qt::NoButton,
                                    Qt::NoButton,
                                    Qt::NoModifier);
        QCoreApplication::sendEvent(button, &buttonMoveEvent);
        const QImage hoverIconImage = button->icon().pixmap(QSize(18, 18)).toImage();
        QVERIFY(!hoverIconImage.isNull());
        QVERIFY2(colorsAreClose(hoverIconImage.pixelColor(2, 9), headerButtonHoverFill, 3),
                 qPrintable(QStringLiteral("hover icon fill %1 expected %2")
                                .arg(hoverIconImage.pixelColor(2, 9).name(),
                                     headerButtonHoverFill.name())));
        QEvent leaveEvent(QEvent::Leave);
        QCoreApplication::sendEvent(button, &leaveEvent);
    }
    QVERIFY(foundCloseButton);
    QVERIFY(foundFloatButton);
    QComboBox* comboBox = constantsDock->findChild<QComboBox*>();
    QLineEdit* searchBox = constantsDock->findChild<QLineEdit*>();
    QTreeWidget* table = constantsDock->findChild<QTreeWidget*>();
    QLabel* searchLabel = nullptr;
    for (QLabel* label : constantsDock->findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("Search")) {
            searchLabel = label;
            break;
        }
    }
    QVERIFY(comboBox != nullptr);
    QVERIFY(searchBox != nullptr);
    QVERIFY(searchLabel != nullptr);
    QVERIFY(table != nullptr);
    QVERIFY(table->header() != nullptr);

    QCOMPARE(constantsDock->widget()->palette().color(QPalette::Window).name(), contentFill.name());
    QVERIFY(constantsDock->widget()->styleSheet().contains(contentFill.name()));
    QCOMPARE(comboBox->palette().color(QPalette::Button).name(), contentFill.name());
    QCOMPARE(comboBox->palette().color(QPalette::ButtonText).name(), contentText.name());
    QVERIFY(comboBox->styleSheet().contains(contentFill.name()));
    QVERIFY(comboBox->styleSheet().contains(contentText.name()));
    QVERIFY(comboBox->styleSheet().contains(QStringLiteral("padding: 4px 32px 4px 8px")));
    QVERIFY(comboBox->styleSheet().contains(QStringLiteral("width: 28px")));
    QVERIFY(comboBox->styleSheet().contains(comboPopupFill.name()));
    QVERIFY(comboBox->styleSheet().contains(comboPopupText.name()));
    QCOMPARE(comboBox->view()->palette().color(QPalette::Base).name(), comboPopupFill.name());
    QCOMPARE(comboBox->view()->palette().color(QPalette::Text).name(), comboPopupText.name());
    QVERIFY(comboBox->view()->styleSheet().contains(comboPopupFill.name()));
    QVERIFY(comboBox->view()->styleSheet().contains(comboPopupText.name()));
    QVERIFY(comboBox->styleSheet().contains(QStringLiteral("QComboBox QAbstractItemView")));
    QVERIFY(comboBox->styleSheet().contains(QStringLiteral("border: 0; outline: 0")));
    QVERIFY(comboBox->view()->styleSheet().contains(QStringLiteral("border: 0; border-radius: 8px; outline: 0")));
    QVERIFY(comboBox->view()->styleSheet().contains(QStringLiteral("QAbstractItemView::item")));
    QVERIFY(comboBox->view()->styleSheet().contains(QStringLiteral("border: 0; border-radius: 6px;")));
    QVERIFY(comboBox->view()->styleSheet().contains(hoveredItemFill.name()));
    QVERIFY(comboBox->view()->styleSheet().contains(hoveredItemText.name()));
    QVERIFY(comboBox->view()->verticalScrollBar()->styleSheet().contains(comboPopupFill.name()));
    QVERIFY(comboBox->view()->verticalScrollBar()->styleSheet().contains(controlFill.name()));
    QVERIFY(comboBox->view()->verticalScrollBar()->styleSheet().contains(hoverFill.name()));
    QVERIFY(table->header()->styleSheet().contains(contentFill.name()));
    QVERIFY(table->header()->styleSheet().contains(contentText.name()));
    QVERIFY(table->header()->styleSheet().contains(titleFill.name()));
    QVERIFY(table->header()->styleSheet().contains(QStringLiteral("border: 0")));
    QVERIFY(table->header()->styleSheet().contains(QStringLiteral("border-top: 1px solid")));
    QVERIFY(table->header()->styleSheet().contains(QStringLiteral("border-right: 1px solid")));
    QVERIFY(table->header()->styleSheet().contains(QStringLiteral("border-bottom: 1px solid")));
    QVERIFY(table->header()->styleSheet().contains(QStringLiteral("QHeaderView::section:last")));
    QVERIFY(table->header()->styleSheet().contains(QStringLiteral("border-right: 0")));
    QCOMPARE(searchBox->palette().color(QPalette::Base).name(), contentFill.name());
    QCOMPARE(searchBox->palette().color(QPalette::Text).name(), contentText.name());
    ThemedLineEdit* themedSearchBox = dynamic_cast<ThemedLineEdit*>(searchBox);
    QVERIFY(themedSearchBox != nullptr);
    QCOMPARE(themedSearchBox->cursorColor().name(), primary.name());
    const QString focusRingBorderTemplate = QStringLiteral("border: %1px solid %2");
    QVERIFY(searchBox->styleSheet().contains(focusRingBorderTemplate
                                                 .arg(UiConfig::DockTextInputUnfocusedOutlineStrokeWidth)
                                                 .arg(textInputOutlineFill.name())));
    QVERIFY(searchBox->styleSheet().contains(QStringLiteral("QLineEdit:focus")));
    QVERIFY(searchBox->styleSheet().contains(focusRingBorderTemplate
                                                 .arg(UiConfig::OutlineStrokeWidth)
                                                 .arg(primary.name())));
    QVERIFY(searchBox->styleSheet().contains(primary.name()));
    QVERIFY(searchBox->property("speedcrunchDockTextInput").toBool());
    QTRY_VERIFY(editorHasPrimaryOutline(editor, primary));
    Editor inactiveEditor;
    inactiveEditor.setThemePrimaryColor(primary, true);
    QVERIFY(inactiveEditor.styleSheet().contains(focusRingBorderTemplate
                                                     .arg(UiConfig::OutlineStrokeWidth)
                                                     .arg(primary.name())));
    inactiveEditor.setThemePrimaryColor(primary, false);
    QVERIFY(inactiveEditor.styleSheet().contains(QStringLiteral("color: %1;").arg(primary.name())));
    QVERIFY2(!inactiveEditor.styleSheet().contains(focusRingBorderTemplate
                                                       .arg(UiConfig::OutlineStrokeWidth)
                                                       .arg(primary.name())),
             qPrintable(inactiveEditor.styleSheet()));
    QCOMPARE(editor->cursorColor().name(), primary.name());
    editor->setText(QStringLiteral("123"));
    editor->setCursorPosition(editor->text().size());
    QCoreApplication::processEvents();
    const QRect editorNativeCursorRect = editor->cursorRect();
    QVERIFY(editorNativeCursorRect.isValid());
    QVERIFY(editorNativeCursorRect.height() > 0);
    const QRect editorCursorRect(
        editorNativeCursorRect.x() + (editorNativeCursorRect.width() - 2) / 2,
        editorNativeCursorRect.y(),
        2,
        editorNativeCursorRect.height());
    const QImage focusedEditorImage = editor->viewport()->grab().toImage();
    QVERIFY(focusedEditorImage.rect().contains(editorCursorRect.center()));
    for (int x = editorCursorRect.left(); x <= editorCursorRect.right(); ++x)
        QCOMPARE(focusedEditorImage.pixelColor(x, editorCursorRect.center().y()).name(), primary.name());
    const int editorBlinkTimeout = qMax(1000, QApplication::cursorFlashTime() + 250);
    const auto editorCursorPixelName = [&]() {
        return editor->viewport()->grab().toImage().pixelColor(editorCursorRect.center()).name();
    };
    QTRY_VERIFY_WITH_TIMEOUT(editorCursorPixelName() != primary.name(), editorBlinkTimeout);
    QTRY_COMPARE_WITH_TIMEOUT(editorCursorPixelName(), primary.name(), editorBlinkTimeout);
    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "handleApplicationFocusChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(QWidget*, editor),
                                      Q_ARG(QWidget*, searchBox)));
    constantsDock->show();
    constantsDock->raise();
    constantsDock->setMinimumWidth(UiConfig::ConstantsDockDefaultWidth);
    window.resizeDocks(QList<QDockWidget*> { constantsDock },
                       QList<int> { UiConfig::ConstantsDockDefaultWidth },
                       Qt::Horizontal);
    QCoreApplication::processEvents();
    const auto visibleComboTextPixelCount = [&]() {
        const QImage image = comboBox->grab().toImage();
        int textPixels = 0;
        for (int y = 4; y < image.height() - 4; ++y) {
            for (int x = 8; x < image.width() - 40; ++x) {
                if (colorsAreClose(image.pixelColor(x, y), contentText, 42))
                    ++textPixels;
            }
        }
        return textPixels;
    };
    QTRY_VERIFY2(visibleComboTextPixelCount() > 8,
                 qPrintable(QStringLiteral("visible text pixels=%1 width=%2")
                                .arg(visibleComboTextPixelCount())
                                .arg(comboBox->width())));
    searchBox->clear();
    QVERIFY2(searchBox->focusPolicy() != Qt::NoFocus,
             qPrintable(QStringLiteral("focusPolicy=%1").arg(int(searchBox->focusPolicy()))));
    editor->setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(editor->hasFocus());
    QTest::mouseClick(searchBox, Qt::LeftButton);
    QTRY_VERIFY(searchBox->hasFocus());
    QCOMPARE(themedSearchBox->cursorColor().name(), primary.name());
    QTest::keyClicks(searchBox, "2323123das23");
    QCOMPARE(searchBox->text(), QStringLiteral("2323123das23"));
    const QRect nativeCursorRect =
        themedSearchBox->inputMethodQuery(Qt::ImCursorRectangle).toRect();
    QVERIFY(nativeCursorRect.isValid());
    QVERIFY(nativeCursorRect.height() > 0);
    const QRect cursorRect(nativeCursorRect.x() + (nativeCursorRect.width() - 2) / 2 + 1,
                           nativeCursorRect.y(),
                           2,
                           nativeCursorRect.height());
    const QImage focusedSearchImage = themedSearchBox->grab().toImage();
    QVERIFY(focusedSearchImage.rect().contains(cursorRect.center()));
    for (int x = cursorRect.left(); x <= cursorRect.right(); ++x)
        QCOMPARE(focusedSearchImage.pixelColor(x, cursorRect.center().y()).name(), primary.name());
    const QPoint leftOfCursor(cursorRect.left() - 1, cursorRect.center().y());
    const QPoint rightOfCursor(cursorRect.right() + 1, cursorRect.center().y());
    if (focusedSearchImage.rect().contains(leftOfCursor))
        QVERIFY(focusedSearchImage.pixelColor(leftOfCursor).name() != primary.name());
    if (focusedSearchImage.rect().contains(rightOfCursor))
        QVERIFY(focusedSearchImage.pixelColor(rightOfCursor).name() != primary.name());
    const int blinkTimeout = qMax(1000, QApplication::cursorFlashTime() + 250);
    const auto cursorPixelName = [&]() {
        return themedSearchBox->grab().toImage().pixelColor(cursorRect.center()).name();
    };
    QTRY_VERIFY_WITH_TIMEOUT(cursorPixelName() != primary.name(), blinkTimeout);
    QTRY_COMPARE_WITH_TIMEOUT(cursorPixelName(), primary.name(), blinkTimeout);
    const QImage visibleAgainSearchImage = themedSearchBox->grab().toImage();
    if (visibleAgainSearchImage.rect().contains(leftOfCursor))
        QVERIFY(visibleAgainSearchImage.pixelColor(leftOfCursor).name() != primary.name());
    if (visibleAgainSearchImage.rect().contains(rightOfCursor))
        QVERIFY(visibleAgainSearchImage.pixelColor(rightOfCursor).name() != primary.name());
    searchBox->clear();
    QTest::keyClicks(searchBox, "mol");
    QCOMPARE(searchBox->text(), QStringLiteral("mol"));
    searchBox->clear();
    comboBox->showPopup();
    QTRY_VERIFY(comboBox->view()->isVisible());
    QCOMPARE(comboBox->view()->frameShape(), QFrame::NoFrame);
    QWidget* comboPopupChrome = comboBox->view()->window();
    if (comboPopupChrome == comboBox->window())
        comboPopupChrome = comboBox->view();
    QVERIFY(comboPopupChrome != nullptr);
    QTRY_VERIFY(!comboPopupChrome->mask().isEmpty());
    comboBox->hidePopup();
    QTRY_VERIFY(!comboBox->view()->isVisible());
    QCOMPARE(searchLabel->palette().color(QPalette::Window).name(), contentFill.name());
    QCOMPARE(searchLabel->palette().color(QPalette::WindowText).name(), contentText.name());
    QVERIFY(searchLabel->styleSheet().contains(contentFill.name()));
    QVERIFY(searchLabel->styleSheet().contains(contentText.name()));
    QCOMPARE(table->palette().color(QPalette::Base).name(), contentFill.name());
    QCOMPARE(table->palette().color(QPalette::Text).name(), contentText.name());
    QCOMPARE(table->property("dockListHoverBackground").value<QColor>().name(),
             hoveredItemFill.name());
    QCOMPARE(table->property("dockListHoverForeground").value<QColor>().name(),
             hoveredItemText.name());
    QCOMPARE(table->viewport()->palette().color(QPalette::Base).name(), contentFill.name());
    QVERIFY(table->styleSheet().contains(QStringLiteral("padding: 6px 8px")));
    QVERIFY(table->styleSheet().contains(QStringLiteral("border: 0")));
    QCOMPARE(table->frameShape(), QFrame::NoFrame);
    QCOMPARE(table->property("dockListInactiveSelectionBackground").value<QColor>().name(),
             controlFill.name());
    QCOMPARE(table->property("dockListInactiveSelectionForeground").value<QColor>().name(),
             controlText.name());
    const QString tableScrollBarStyle = table->verticalScrollBar()->styleSheet();
    QVERIFY(tableScrollBarStyle.contains(contentFill.name()));
    QVERIFY(tableScrollBarStyle.contains(titleFill.name()));
    QVERIFY(tableScrollBarStyle.contains(controlFill.name()));
    QVERIFY(tableScrollBarStyle.contains(hoverFill.name()));
    QTRY_VERIFY(table->topLevelItemCount() > 0);
    QTreeWidgetItem* tooltipItem = table->topLevelItem(0);
    QVERIFY(tooltipItem != nullptr);
    table->scrollToItem(tooltipItem);
    QCoreApplication::processEvents();
    const QRect tooltipRect = table->visualItemRect(tooltipItem);
    QVERIFY(tooltipRect.isValid());
    const QPoint tooltipPos = tooltipRect.center();
    QMouseEvent tooltipMoveEvent(QEvent::MouseMove,
                                 QPointF(tooltipPos),
                                 QPointF(table->viewport()->mapToGlobal(tooltipPos)),
                                 Qt::NoButton,
                                 Qt::NoButton,
                                 Qt::NoModifier);
    QCoreApplication::sendEvent(table->viewport(), &tooltipMoveEvent);
    QFrame* summaryPopup =
        constantsWidget->findChild<QFrame*>(QStringLiteral("constantsSummaryPopup"));
    QTRY_VERIFY(summaryPopup != nullptr && summaryPopup->isVisible());
    QLabel* summaryPopupLabel =
        summaryPopup->findChild<QLabel*>(QStringLiteral("constantsSummaryPopupLabel"));
    QVERIFY(summaryPopupLabel != nullptr);
    QCOMPARE(summaryPopup->palette().color(QPalette::Window).name(),
             completionPopupFill.name());
    QCOMPARE(summaryPopup->palette().color(QPalette::WindowText).name(),
             completionPopupText.name());
    QCOMPARE(summaryPopupLabel->palette().color(QPalette::WindowText).name(),
             completionPopupText.name());
    QVERIFY(summaryPopup->styleSheet().contains(completionPopupFill.name()));
    QVERIFY(summaryPopup->styleSheet().contains(completionPopupText.name()));
    QVERIFY(summaryPopup->styleSheet().contains(
        QStringLiteral("border: %1px solid %2")
            .arg(UiConfig::PopupOutlineStrokeWidth)
            .arg(completionPopupOutlineFill.name())));
    QVERIFY(summaryPopup->styleSheet().contains(
        QStringLiteral("border-radius: %1px")
            .arg(UiConfig::CompletionPopupCornerRadius)));
    QVERIFY(!summaryPopup->mask().isEmpty());
    summaryPopup->hide();
    const QMargins dockRootMargins = constantsDock->widget()->layout()->contentsMargins();
    QCOMPARE(dockRootMargins, QMargins(0, 0, 0, 0));
    const QMargins searchRowMargins =
        searchLabel->parentWidget()->layout()->contentsMargins();
    QCOMPARE(searchRowMargins, QMargins(8, 6, 8, 6));
    QVERIFY(searchLabel->parentWidget()->styleSheet().contains(contentFill.name()));
    bool foundStyledDockMenu = false;
    for (QMenu* menu : window.findChildren<QMenu*>()) {
        if (!menu->styleSheet().contains(titleFill.name()))
            continue;
        foundStyledDockMenu = true;
        QVERIFY(menu->styleSheet().contains(titleText.name()));
        QVERIFY(menu->styleSheet().contains(controlFill.name()));
        QVERIFY(menu->styleSheet().contains(controlText.name()));
        QVERIFY(menu->styleSheet().contains(QStringLiteral("border-radius: 8px")));
    }
    QVERIFY(foundStyledDockMenu);

    searchBox->setText(QStringLiteral("no-such-constant-filter-value"));
    QTest::qWait(650);
    QCoreApplication::processEvents();
    QLabel* noMatchLabel = nullptr;
    for (QLabel* label : constantsDock->findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("No match found")) {
            noMatchLabel = label;
            break;
        }
    }
    QVERIFY(noMatchLabel != nullptr);
    QVERIFY(!noMatchLabel->isHidden());
    QCOMPARE(noMatchLabel->palette().color(QPalette::WindowText).name(), contentText.name());
    QVERIFY(noMatchLabel->styleSheet().contains(contentText.name()));
    QCOMPARE(table->topLevelItemCount(), 0);
    QVERIFY2(table->header()->length() >= table->header()->width() - 1,
             qPrintable(QStringLiteral("length=%1 header=%2 sections=%3,%4,%5")
                            .arg(table->header()->length())
                            .arg(table->header()->width())
                            .arg(table->header()->sectionSize(0))
                            .arg(table->header()->sectionSize(1))
                            .arg(table->header()->sectionSize(2))));
    searchBox->clear();
    QTest::qWait(650);
    QCoreApplication::processEvents();
    QVERIFY(table->topLevelItemCount() > 0);
    QVERIFY(!table->header()->stretchLastSection());

    QLabel* domainLabel = nullptr;
    for (QLabel* label : functionsDock->findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("Domain")) {
            domainLabel = label;
            break;
        }
    }
    QVERIFY(domainLabel != nullptr);
    QCOMPARE(domainLabel->palette().color(QPalette::Window).name(), contentFill.name());
    QCOMPARE(domainLabel->palette().color(QPalette::WindowText).name(), contentText.name());
    QVERIFY(domainLabel->styleSheet().contains(contentFill.name()));
    QVERIFY(domainLabel->styleSheet().contains(contentText.name()));
    const QMargins domainRowMargins =
        domainLabel->parentWidget()->layout()->contentsMargins();
    QCOMPARE(domainRowMargins, QMargins(8, 6, 8, 6));
    QVERIFY(domainLabel->parentWidget()->styleSheet().contains(contentFill.name()));

    constantsDock->show();
    constantsDock->raise();
    QCoreApplication::processEvents();
    const QImage searchImage = searchBox->grab().toImage();
    QVERIFY(!searchImage.isNull());
    QCOMPARE(searchImage.pixelColor(searchImage.width() / 2, searchImage.height() / 2).name(),
             contentFill.name());

    QDockWidget* bookDock = window.findChild<QDockWidget*>(QStringLiteral("BookDock"));
    QVERIFY(bookDock != nullptr);
    QVERIFY(bookDock->widget() != nullptr);
    QVERIFY(bookDock->widget()->layout() != nullptr);
    QCOMPARE(bookDock->widget()->layout()->contentsMargins(), QMargins(0, 0, 0, 0));
    QTextBrowser* bookBrowser = bookDock->findChild<QTextBrowser*>();
    QVERIFY(bookBrowser != nullptr);
    QCOMPARE(bookBrowser->palette().color(QPalette::Base).name(), contentFill.name());
    QCOMPARE(bookBrowser->palette().color(QPalette::Text).name(), contentText.name());
    QCOMPARE(bookBrowser->viewport()->palette().color(QPalette::Base).name(), contentFill.name());
    QVERIFY(bookBrowser->toHtml().contains(contentFill.name()));
    const QColor expectedBookSectionLink = generateSecondaryLinkFromBackground(
        contentFill,
        aaForegroundForBackground(contentFill, bookBrowser->palette().color(QPalette::Link)));
    QVERIFY(bookBrowser->toHtml().contains(expectedBookSectionLink.name()));
    QVERIFY(!bookBrowser->toHtml().contains(QStringLiteral("#555555")));
    const QColor expectedBookFormulaLink = aaForegroundForBackground(
        contentFill, bookBrowser->palette().color(QPalette::Link));
    QVERIFY(QMetaObject::invokeMethod(bookDock,
                                      "openPage",
                                      Q_ARG(QUrl, QUrl(QStringLiteral("geometry/sector")))));
    QVERIFY(bookBrowser->toHtml().contains(expectedBookFormulaLink.name()));
    const QString bookScrollBarStyle = bookBrowser->verticalScrollBar()->styleSheet();
    QVERIFY(bookScrollBarStyle.contains(contentFill.name()));
    QVERIFY(bookScrollBarStyle.contains(titleFill.name()));
    QVERIFY(bookScrollBarStyle.contains(controlFill.name()));
    QVERIFY(bookScrollBarStyle.contains(hoverFill.name()));

    bool foundDockTabs = false;
    for (QTabBar* tabBar : window.findChildren<QTabBar*>()) {
        bool dockNavigationTabBar = false;
        for (int i = 0; i < tabBar->count(); ++i) {
            const QString text = tabBar->tabText(i);
            if (text == QStringLiteral("Constants") || text == QStringLiteral("Functions")) {
                dockNavigationTabBar = true;
                break;
            }
        }
        if (dockNavigationTabBar
            && tabBar->isVisible()
            && tabBar->styleSheet().contains(titleFill.name())) {
            foundDockTabs = true;
            QCOMPARE(tabBar->palette().color(QPalette::WindowText).name(), titleText.name());
            QVERIFY(tabBar->styleSheet().contains(QStringLiteral("background-color: transparent")));
            QVERIFY(tabBar->styleSheet().contains(chromeFill.name()));
            QVERIFY(tabBar->styleSheet().contains(chromeText.name()));
            QVERIFY(tabBar->styleSheet().contains(resultFill.name()));
            QVERIFY(tabBar->styleSheet().contains(resultText.name()));
            QVERIFY(tabBar->styleSheet().contains(titleFill.name()));
            QVERIFY(tabBar->styleSheet().contains(titleText.name()));
            QVERIFY(tabBar->styleSheet().contains(QStringLiteral("padding: 5px 14px")));
            QVERIFY(tabBar->styleSheet().contains(QStringLiteral("margin: 2px 1px")));
            QVERIFY(tabBar->property("speedcrunchDockSystemTabBar").toBool());
            QVERIFY(tabBar->hasMouseTracking());
            QVERIFY(!tabBar->drawBase());
            int hoveredTab = -1;
            for (int i = 0; i < tabBar->count(); ++i) {
                if (tabBar->tabText(i) == QStringLiteral("Constants")
                    || tabBar->tabText(i) == QStringLiteral("Functions")) {
                    hoveredTab = i;
                    break;
                }
            }
            QVERIFY(hoveredTab >= 0);
            const QPoint hoverPos = tabBar->tabRect(hoveredTab).center();
            QCOMPARE(tabBar->tabAt(hoverPos), hoveredTab);
            QMouseEvent moveEvent(QEvent::MouseMove,
                                  QPointF(hoverPos),
                                  QPointF(tabBar->mapToGlobal(hoverPos)),
                                  Qt::NoButton,
                                  Qt::NoButton,
                                  Qt::NoModifier);
            QCoreApplication::sendEvent(tabBar, &moveEvent);
            QTRY_COMPARE(tabBar->cursor().shape(), Qt::PointingHandCursor);
            QWidget* tabBarParent = tabBar->parentWidget();
            QVERIFY(tabBarParent != nullptr);
            QCOMPARE(tabBarParent->palette().color(QPalette::Window).name(), chromeFill.name());
            QVERIFY(tabBarParent->styleSheet().contains(chromeFill.name()));
            const QImage tabBarImage = tabBar->grab().toImage();
            QVERIFY(!tabBarImage.isNull());
            QCOMPARE(tabBarImage.pixelColor(tabBarImage.width() - 1,
                                            tabBarImage.height() / 2).name(),
                     chromeFill.name());
            QCOMPARE(tabBarImage.pixelColor(tabBarImage.width() - 1, 0).name(),
                     chromeFill.name());
        }
    }
    QVERIFY(foundDockTabs);

    settings->customColorSchemeJson = themeJsonString(QJsonObject{{QStringLiteral("background"), QStringLiteral("#300a24")}});
    window.colorSchemeChanged();
    QCoreApplication::processEvents();

    const QVector<QColor> changedShades =
        generateOklchShades(QColor(QStringLiteral("#300a24")), 6, ThemePolarity::Dark);
    const QVector<QColor> changedForegrounds = aaForegroundsForBackgrounds(changedShades);
    const QColor changedContentFill = changedShades.at(2);
    const QColor changedContentText = changedForegrounds.at(2);
    const QColor changedTitleFill = changedShades.at(3);
    const QColor changedTitleText = changedForegrounds.at(3);
    const QColor changedPrimary = generatePrimaryFromBackground(QColor(QStringLiteral("#300a24")));

    QTRY_VERIFY(constantsDock->styleSheet().contains(changedTitleFill.name()));
    QVERIFY(constantsDock->styleSheet().contains(changedTitleText.name()));
    QVERIFY(!constantsDock->styleSheet().contains(titleFill.name()));
    bool foundUpdatedDockTab = false;
    for (QTabBar* tabBar : window.findChildren<QTabBar*>()) {
        if (!tabBar->styleSheet().contains(changedTitleFill.name()))
            continue;
        foundUpdatedDockTab = true;
        QVERIFY(tabBar->styleSheet().contains(changedTitleText.name()));
    }
    QVERIFY(foundUpdatedDockTab);
    QCOMPARE(constantsDock->widget()->palette().color(QPalette::Window).name(),
             changedContentFill.name());
    QVERIFY(constantsDock->widget()->styleSheet().contains(changedContentFill.name()));
    QVERIFY(!constantsDock->widget()->styleSheet().contains(contentFill.name()));
    QCOMPARE(searchLabel->palette().color(QPalette::Window).name(),
             changedContentFill.name());
    QCOMPARE(searchLabel->palette().color(QPalette::WindowText).name(),
             changedContentText.name());
    QVERIFY(searchLabel->parentWidget()->styleSheet().contains(changedContentFill.name()));
    QCOMPARE(domainLabel->palette().color(QPalette::Window).name(),
             changedContentFill.name());
    QCOMPARE(domainLabel->palette().color(QPalette::WindowText).name(),
             changedContentText.name());
    QVERIFY(domainLabel->parentWidget()->styleSheet().contains(changedContentFill.name()));
    QCOMPARE(searchBox->palette().color(QPalette::Base).name(), changedContentFill.name());
    QCOMPARE(searchBox->palette().color(QPalette::Text).name(), changedContentText.name());
    QCOMPARE(themedSearchBox->cursorColor().name(), changedPrimary.name());
    QVERIFY(searchBox->styleSheet().contains(QStringLiteral("border: %1px solid %2")
                                                 .arg(UiConfig::OutlineStrokeWidth)
                                                 .arg(changedPrimary.name())));
    QCOMPARE(comboBox->palette().color(QPalette::Button).name(), changedContentFill.name());
    QCOMPARE(comboBox->palette().color(QPalette::ButtonText).name(), changedContentText.name());
    QCOMPARE(table->palette().color(QPalette::Base).name(), changedContentFill.name());
    QCOMPARE(table->viewport()->palette().color(QPalette::Base).name(),
             changedContentFill.name());
    QVERIFY(table->styleSheet().contains(changedContentFill.name()));
}

void TestDisplayUi::dock_header_buttons_render_full_size_data()
{
    QTest::addColumn<bool>("framed");
    QTest::addColumn<QString>("background");
    QTest::newRow("dark-framed") << true << QStringLiteral("#402034");
    QTest::newRow("dark-unframed") << false << QStringLiteral("#402034");
    QTest::newRow("light-framed") << true << QStringLiteral("#f4e8ee");
    QTest::newRow("light-unframed") << false << QStringLiteral("#f4e8ee");
}

void TestDisplayUi::dock_header_buttons_render_full_size()
{
    QFETCH(bool, framed);
    QFETCH(QString, background);
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(
        QJsonObject{{QStringLiteral("background"), background}});
    settings->windowState.clear();
    settings->constantsDockVisible = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QDockWidget* dock = window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(dock != nullptr);
    dock->setFloating(false);
    dock->show();
    dock->raise();
    QCoreApplication::processEvents();
    QAbstractButton* closeButton = dock->findChild<QAbstractButton*>(
        QStringLiteral("qt_dockwidget_closebutton"));
    QAbstractButton* floatButton = dock->findChild<QAbstractButton*>(
        QStringLiteral("qt_dockwidget_floatbutton"));
    QVERIFY(closeButton != nullptr);
    QVERIFY(floatButton != nullptr);

    for (QAbstractButton* button : {closeButton, floatButton}) {
        auto* style = new DockHeaderButtonTestStyle(framed);
        style->setParent(button);
        button->setStyle(style);
        QCOMPARE(button->style()->styleHint(QStyle::SH_DockWidget_ButtonsHaveFrame,
                                           nullptr, button), int(framed));
        QCOMPARE(button->style()->pixelMetric(QStyle::PM_SmallIconSize,
                                              nullptr, button), 16);
        QVERIFY(button->isVisible());

        for (bool hovered : {false, true}) {
            QEvent hoverEvent(hovered ? QEvent::Enter : QEvent::Leave);
            QCoreApplication::sendEvent(button, &hoverEvent);
            const QImage rendered = button->grab().toImage();
            const qreal dpr = rendered.devicePixelRatio();
            const QImage expected = button->icon().pixmap(button->iconSize(), dpr).toImage();
            QCOMPARE(rendered.size(), expected.size());

            // Compare the actual widget's symbol with the full-size source icon.
            // Checking iconSize() alone misses Qt's private dock-button scaling.
            const QRect symbolRect(qRound(4 * dpr), qRound(4 * dpr),
                                   qRound(10 * dpr), qRound(10 * dpr));
            for (int y = symbolRect.top(); y <= symbolRect.bottom(); ++y) {
                for (int x = symbolRect.left(); x <= symbolRect.right(); ++x) {
                    QVERIFY2(colorsAreClose(rendered.pixelColor(x, y),
                                            expected.pixelColor(x, y), 3),
                             qPrintable(QStringLiteral("%1 symbol differs at %2,%3")
                                            .arg(button->objectName()).arg(x).arg(y)));
                }
            }
        }
    }

    QTest::mouseClick(floatButton, Qt::LeftButton);
    QTRY_VERIFY(dock->isFloating());
    dock->setFloating(false);
    QTRY_VERIFY(closeButton->isVisible());
    QTest::mouseClick(closeButton, Qt::LeftButton);
    QTRY_VERIFY(!dock->isVisible());
}

void TestDisplayUi::dock_header_buttons_render_full_size_on_first_run_data()
{
    dock_header_buttons_render_full_size_data();
}

void TestDisplayUi::dock_header_buttons_render_full_size_on_first_run()
{
    Settings* settings = Settings::instance();
    const bool oldHasNumberFormatStyleSetting = settings->hasNumberFormatStyleSetting;
    const auto restoreSettings = qScopeGuard([&]() {
        settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
    });
    settings->hasNumberFormatStyleSetting = false;
    // Exercise the shared fixture with fresh first-run settings; no window
    // guard should need to suppress this dialog on its own.
    UiTestFixture::resetSettings();
    QVERIFY(settings->hasNumberFormatStyleSetting);
    dock_header_buttons_render_full_size();
    QVERIFY(QApplication::activeModalWidget() == nullptr);
    QVERIFY(settings->hasNumberFormatStyleSetting);
}

void TestDisplayUi::dock_header_buttons_stay_centered_in_title_bar_data()
{
    QTest::addColumn<int>("fontPixelSize");
    QTest::addColumn<bool>("verticalTitleBar");
    QTest::newRow("small-horizontal") << 9 << false;
    QTest::newRow("normal-horizontal") << 13 << false;
    QTest::newRow("large-horizontal") << 22 << false;
    QTest::newRow("small-vertical") << 9 << true;
    QTest::newRow("large-vertical") << 22 << true;
}

void TestDisplayUi::dock_header_buttons_stay_centered_in_title_bar()
{
    QFETCH(int, fontPixelSize);
    QFETCH(bool, verticalTitleBar);
    MainWindowStateGuard guard;
    guard.settings->windowState.clear();
    guard.settings->constantsDockVisible = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QDockWidget* dock = window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(dock != nullptr);
    dock->setFloating(false);
    dock->show();
    dock->raise();
    if (verticalTitleBar)
        dock->setFeatures(dock->features() | QDockWidget::DockWidgetVerticalTitleBar);
    QFont font = dock->font();
    font.setPixelSize(fontPixelSize);
    dock->setFont(font);

    for (const QSize size : {QSize(800, 600), QSize(1050, 750)}) {
        window.resize(size);
        QCoreApplication::processEvents();
        const int titleExtent = verticalTitleBar
            ? dock->widget()->geometry().left() : dock->widget()->geometry().top();
        QVERIFY(titleExtent >= 18);
        for (const QString& name : {QStringLiteral("qt_dockwidget_closebutton"),
                                    QStringLiteral("qt_dockwidget_floatbutton")}) {
            QAbstractButton* button = dock->findChild<QAbstractButton*>(name);
            QVERIFY(button != nullptr);
            QVERIFY(button->isVisible());
            const QRect rect = button->geometry();
            const int twiceButtonCenter = verticalTitleBar
                ? 2 * rect.x() + rect.width() : 2 * rect.y() + rect.height();
            QVERIFY2(qAbs(twiceButtonCenter - titleExtent) <= 1,
                     qPrintable(QStringLiteral("%1 center %2 differs from title center %3")
                                    .arg(name).arg(twiceButtonCenter / 2.0)
                                    .arg(titleExtent / 2.0)));
        }
    }
}

void TestDisplayUi::dock_header_buttons_keep_theme_after_redocking_data()
{
    QTest::addColumn<QString>("background");
    QTest::addColumn<QString>("dockName");
    QTest::newRow("dark-constants") << QStringLiteral("#402034") << QStringLiteral("ConstantsDock");
    QTest::newRow("light-constants") << QStringLiteral("#f4e8ee") << QStringLiteral("ConstantsDock");
    QTest::newRow("dark-functions") << QStringLiteral("#402034") << QStringLiteral("FunctionsDock");
    QTest::newRow("light-functions") << QStringLiteral("#f4e8ee") << QStringLiteral("FunctionsDock");
}

void TestDisplayUi::dock_header_buttons_keep_theme_after_redocking()
{
    QFETCH(QString, background);
    QFETCH(QString, dockName);
    MainWindowStateGuard guard;
    guard.settings->colorScheme = QStringLiteral("Custom");
    guard.settings->customColorSchemeJson = themeJsonString(
        QJsonObject{{QStringLiteral("background"), background}});
    guard.settings->windowState.clear();
    guard.settings->constantsDockVisible = true;
    guard.settings->functionsDockVisible = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QDockWidget* dock = window.findChild<QDockWidget*>(dockName);
    QVERIFY(dock != nullptr);
    dock->setFloating(false);
    dock->show();
    dock->raise();
    QCoreApplication::processEvents();

    const QStringList buttonNames {QStringLiteral("qt_dockwidget_closebutton"),
                                   QStringLiteral("qt_dockwidget_floatbutton")};
    QList<QAbstractButton*> buttons;
    QList<QImage> expectedIcons;
    for (const QString& name : buttonNames) {
        QAbstractButton* button = dock->findChild<QAbstractButton*>(name);
        QVERIFY(button != nullptr);
        button->setAttribute(Qt::WA_UnderMouse, false);
        QEvent leaveEvent(QEvent::Leave);
        QCoreApplication::sendEvent(button, &leaveEvent);
        const QImage icon = button->icon().pixmap(button->iconSize()).toImage();
        QVERIFY(!icon.isNull());
        QVERIFY(colorsAreClose(icon.pixelColor(2, 9),
                               button->property("speedcrunchDockHeaderButtonFill").value<QColor>()));
        buttons.append(button);
        expectedIcons.append(icon);
    }

    for (int cycle = 0; cycle < 2; ++cycle) {
        dock->setFloating(true);
        QVERIFY(dock->isFloating());
        dock->setFloating(false);
        QVERIFY(!dock->isFloating());
        // Check before a paint, queued theme update, or hover can repair an icon.
        for (bool processEvents : {false, true}) {
            if (processEvents)
                QCoreApplication::processEvents();
            for (int i = 0; i < buttons.size(); ++i) {
                QVERIFY(!buttons.at(i)->underMouse());
                const QImage icon = buttons.at(i)->icon().pixmap(buttons.at(i)->iconSize()).toImage();
                QVERIFY2(icon == expectedIcons.at(i), qPrintable(buttonNames.at(i)));
            }
        }
    }
}

void TestDisplayUi::formula_book_text_scales_with_zoom_data()
{
    QTest::addColumn<QString>("page");
    QTest::addColumn<QString>("formula");
    QTest::addColumn<QString>("caption");
    QTest::addColumn<QString>("variable");
    QTest::addColumn<QString>("unit");

    QTest::newRow("geometry")
        << QStringLiteral("geometry/circle") << QStringLiteral("A =")
        << QStringLiteral("radius") << QStringLiteral("r") << QString();
    QTest::newRow("electronics")
        << QStringLiteral("electronics/ohmslaw") << QStringLiteral("R =")
        << QStringLiteral("resistance") << QStringLiteral("R") << QStringLiteral("Ω");
    QTest::newRow("compound-unit")
        << QStringLiteral("rf/propagation") << QStringLiteral("= 3e8")
        << QStringLiteral("dielectric constant") << QStringLiteral("e") << QStringLiteral("m·s");
}

void TestDisplayUi::formula_book_text_scales_with_zoom()
{
    QFETCH(QString, page);
    QFETCH(QString, formula);
    QFETCH(QString, caption);
    QFETCH(QString, variable);
    QFETCH(QString, unit);

    BookDock dock;
    dock.resize(800, 600);
    dock.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dock));
    dock.openPage(QUrl(page));
    QTextBrowser* browser = dock.findChild<QTextBrowser*>();
    QVERIFY(browser != nullptr);

    // Inspect laid-out glyph fonts so relative sizes and the document's zoom
    // are resolved by Qt, including fonts inside caption table cells.
    const auto pixelSize = [&](const QString& text, bool backwards = false) -> qreal {
        QTextDocument* document = browser->document();
        document->documentLayout()->documentSize();
        const QTextCursor cursor = document->find(
            text, backwards ? document->characterCount() - 1 : 0,
            backwards ? QTextDocument::FindBackward | QTextDocument::FindWholeWords
                      : QTextDocument::FindFlags());
        if (cursor.isNull())
            return -1;
        const QTextBlock block = cursor.block();
        const auto runs = block.layout()->glyphRuns(cursor.selectionStart() - block.position(), 1);
        return runs.isEmpty() ? -1 : runs.first().rawFont().pixelSize();
    };
    const auto sizes = [&]() {
        return QList<qreal> { pixelSize(QStringLiteral("Index")), pixelSize(formula),
                             pixelSize(caption), pixelSize(variable, true) };
    };
    const QList<qreal> original = sizes();
    for (qreal size : original)
        QVERIFY(size > 0);
    const qreal originalBaseSize = browser->document()->defaultFont().pointSizeF();

    for (int steps : { 4, -4 }) {
        browser->zoomIn(steps);
        const qreal scale = browser->document()->defaultFont().pointSizeF() / originalBaseSize;
        const QList<qreal> zoomed = sizes();
        for (qsizetype i = 0; i < original.size(); ++i) {
            QVERIFY2(steps > 0 ? zoomed[i] > original[i] : zoomed[i] < original[i],
                     qPrintable(QStringLiteral("text sample %1 does not follow zoom").arg(i)));
            QVERIFY(qAbs(zoomed[i] / original[i] - scale) < 0.1);
        }
        if (!unit.isEmpty())
            QCOMPARE(pixelSize(unit), pixelSize(formula));

        // Reloading content must retain the same sizes at the current zoom.
        dock.openPage(QUrl(QStringLiteral("index")));
        dock.openPage(QUrl(page));
        QCOMPARE(sizes(), zoomed);
        dock.setContentSurfaceColors(QColor(steps > 0 ? "#202020" : "#303030"), QColor("#eeeeee"));
        QCOMPARE(sizes(), zoomed);
        dock.retranslateText();
        QCOMPARE(sizes(), zoomed);
        browser->zoomOut(steps);
        QCOMPARE(sizes(), original);
    }
    if (!unit.isEmpty())
        QCOMPARE(pixelSize(unit), pixelSize(formula));
}

void TestDisplayUi::restored_constants_dock_empty_filter_fills_header()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->statusBarVisible = false;
    settings->hasNumberFormatStyleSetting = true;
    settings->constantsDockDomain.clear();
    settings->constantsDockSubdomain.clear();
    settings->constantsDockSearchText = QStringLiteral("no-such-constant-filter-value");

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QLineEdit* searchBox = constantsDock->findChild<QLineEdit*>();
    QTreeWidget* table = constantsDock->findChild<QTreeWidget*>();
    QVERIFY(searchBox != nullptr);
    QVERIFY(table != nullptr);
    QVERIFY(table->header() != nullptr);
    QCOMPARE(searchBox->text(), QStringLiteral("no-such-constant-filter-value"));
    QCOMPARE(table->topLevelItemCount(), 0);
    QTRY_VERIFY2(table->header()->length() >= table->header()->width() - 1,
                 qPrintable(QStringLiteral("length=%1 header=%2 sections=%3,%4,%5")
                                .arg(table->header()->length())
                                .arg(table->header()->width())
                                .arg(table->header()->sectionSize(0))
                                .arg(table->header()->sectionSize(1))
                                .arg(table->header()->sectionSize(2))));
}

void TestDisplayUi::dock_scroll_corner_uses_scrollbar_track_fill()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{{QStringLiteral("background"), QStringLiteral("#1f3229")}});
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    const QColor base(QStringLiteral("#1f3229"));
    const QVector<QColor> shades = generateOklchShades(base, 6, ThemePolarity::Dark);
    const QColor expectedTrackFill = shades.at(UiConfig::DockBackgroundShade);

    MainWindow window;
    window.resize(700, 420);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QTreeWidget* table = constantsDock->findChild<QTreeWidget*>();
    QVERIFY(table != nullptr);

    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    for (int column = 0; column < table->columnCount(); ++column)
        table->setColumnWidth(column, 240);
    constantsDock->show();
    constantsDock->raise();
    window.resizeDocks(QList<QDockWidget*> { constantsDock },
                       QList<int> { UiConfig::ConstantsDockMinimumWidth },
                       Qt::Horizontal);
    QCoreApplication::processEvents();

    QWidget* corner = table->cornerWidget();
    QVERIFY(corner != nullptr);
    QCOMPARE(corner->palette().color(QPalette::Window).name(), expectedTrackFill.name());
    QVERIFY(corner->styleSheet().contains(expectedTrackFill.name()));
    QVERIFY(corner->styleSheet().contains(QStringLiteral("border: 0")));
    QTRY_VERIFY(table->horizontalScrollBar()->isVisible());
    QTRY_VERIFY(table->verticalScrollBar()->isVisible());
    QTRY_VERIFY(corner->isVisible());

    const QImage image = corner->grab().toImage();
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 1);
    QVERIFY(image.height() > 1);
    const QList<QPoint> samplePoints {
        QPoint(0, 0),
        QPoint(image.width() - 1, 0),
        QPoint(0, image.height() - 1),
        QPoint(image.width() - 1, image.height() - 1),
        image.rect().center()
    };
    for (const QPoint& point : samplePoints) {
        const QColor sampled = image.pixelColor(point);
        QVERIFY2(colorsAreClose(sampled, expectedTrackFill, 3),
                 qPrintable(QStringLiteral("corner sample %1,%2 is %3, expected %4")
                                .arg(point.x())
                                .arg(point.y())
                                .arg(sampled.name(), expectedTrackFill.name())));
    }
}

void TestDisplayUi::dock_separator_style_uses_primary_while_hovered_or_dragged()
{
    MainWindowStateGuard guard;
    guard.settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const QColor normal(QStringLiteral("#123456"));
    const QColor primary(QStringLiteral("#abcdef"));

    QWidget propertyOwner;
    propertyOwner.setProperty("speedcrunchDockSeparatorNormalColor", normal);
    propertyOwner.setProperty("speedcrunchDockSeparatorActiveColor", primary);
    propertyOwner.resize(96, 64);
    QWidget styleHost(&propertyOwner);
    styleHost.resize(64, 32);
    styleHost.move(0, 0);
    propertyOwner.show();
    QVERIFY(QTest::qWaitForWindowExposed(&propertyOwner));

    QCursor::setPos(styleHost.mapToGlobal(QPoint(50, 24)));
    QCoreApplication::processEvents();
    QCOMPARE(dockSeparatorPrimitiveColor(&styleHost, QStyle::State_None).name(),
             normal.name());

    QCursor::setPos(styleHost.mapToGlobal(QPoint(4, 4)));
    QCoreApplication::processEvents();
    QCOMPARE(dockSeparatorPrimitiveColor(&styleHost, QStyle::State_None).name(),
             primary.name());
    QImage horizontalSeparator =
        dockSeparatorPrimitiveImage(&styleHost, QStyle::State_None, QSize(32, 8));
    const int horizontalStrokeTop =
        (horizontalSeparator.height() - UiConfig::DockSplitterStrokeWidth) / 2;
    const int horizontalStrokeBottom =
        horizontalStrokeTop + UiConfig::DockSplitterStrokeWidth - 1;
    for (int y = horizontalStrokeTop; y <= horizontalStrokeBottom; ++y)
        QCOMPARE(horizontalSeparator.pixelColor(horizontalSeparator.width() / 2, y).name(),
                 primary.name());
    if (horizontalStrokeTop > 0)
        QCOMPARE(horizontalSeparator.pixelColor(horizontalSeparator.width() / 2,
                                                horizontalStrokeTop - 1).alpha(), 0);
    if (horizontalStrokeBottom + 1 < horizontalSeparator.height())
        QCOMPARE(horizontalSeparator.pixelColor(horizontalSeparator.width() / 2,
                                                horizontalStrokeBottom + 1).alpha(), 0);

    QImage verticalSeparator =
        dockSeparatorPrimitiveImage(&styleHost, QStyle::State_None, QSize(8, 32));
    const int verticalStrokeLeft =
        (verticalSeparator.width() - UiConfig::DockSplitterStrokeWidth) / 2;
    const int verticalStrokeRight =
        verticalStrokeLeft + UiConfig::DockSplitterStrokeWidth - 1;
    for (int x = verticalStrokeLeft; x <= verticalStrokeRight; ++x)
        QCOMPARE(verticalSeparator.pixelColor(x, verticalSeparator.height() / 2).name(),
                 primary.name());
    if (verticalStrokeLeft > 0)
        QCOMPARE(verticalSeparator.pixelColor(verticalStrokeLeft - 1,
                                              verticalSeparator.height() / 2).alpha(), 0);
    if (verticalStrokeRight + 1 < verticalSeparator.width())
        QCOMPARE(verticalSeparator.pixelColor(verticalStrokeRight + 1,
                                              verticalSeparator.height() / 2).alpha(), 0);

    QCursor::setPos(styleHost.mapToGlobal(QPoint(4, 1)));
    QCoreApplication::processEvents();
    QImage thinHorizontalSeparator = dockSeparatorPrimitiveImage(&styleHost,
                                                                QStyle::State_None,
                                                                QRect(0, 0, 32, 1),
                                                                QSize(32, 3));
    QCOMPARE(thinHorizontalSeparator.pixelColor(thinHorizontalSeparator.width() / 2, 1).name(),
             primary.name());
    QCOMPARE(thinHorizontalSeparator.pixelColor(thinHorizontalSeparator.width() / 2, 2).alpha(),
             0);

    QCursor::setPos(styleHost.mapToGlobal(QPoint(1, 4)));
    QCoreApplication::processEvents();
    QImage thinVerticalSeparator = dockSeparatorPrimitiveImage(&styleHost,
                                                              QStyle::State_None,
                                                              QRect(0, 0, 1, 32),
                                                              QSize(3, 32));
    QCOMPARE(thinVerticalSeparator.pixelColor(1, thinVerticalSeparator.height() / 2).name(),
             primary.name());
    QCOMPARE(thinVerticalSeparator.pixelColor(2, thinVerticalSeparator.height() / 2).alpha(),
             0);

    QCursor::setPos(styleHost.mapToGlobal(QPoint(50, 24)));
    QCoreApplication::processEvents();
    QCOMPARE(dockSeparatorPrimitiveColor(&styleHost, QStyle::State_Sunken).name(),
             primary.name());
}

void TestDisplayUi::constants_dock_uses_configured_narrow_minimum_width()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        Settings::KeypadMode oldKeypadMode;
        bool oldKeypadVisible;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->keypadMode = oldKeypadMode;
            settings->keypadVisible = oldKeypadVisible;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->constantsDockVisible,
        settings->functionsDockVisible,
        settings->historyDockVisible,
        settings->formulaBookDockVisible,
        settings->variablesDockVisible,
        settings->userFunctionsDockVisible,
        settings->userUnitsDockVisible,
        settings->bitfieldVisible,
        settings->keypadMode,
        settings->keypadVisible,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QCOMPARE(constantsDock->minimumWidth(), UiConfig::ConstantsDockMinimumWidth);

    QVERIFY(constantsDock->widget()->minimumSizeHint().width()
            <= UiConfig::ConstantsDockMinimumWidth);
}

void TestDisplayUi::f6_cycles_focus_between_editor_and_visible_dock_controls_data()
{
    QTest::addColumn<bool>("savedDockLayout");
    QTest::newRow("default-layout") << false;
    QTest::newRow("saved-visible-dock") << true;
}

void TestDisplayUi::f6_cycles_focus_between_editor_and_visible_dock_controls()
{
    QFETCH(bool, savedDockLayout);
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    if (savedDockLayout) {
        settings->sessionLayoutJson.clear();
        settings->windowState.clear();
        settings->constantsDockVisible = true;
        MainWindow previousWindow(false);
        settings->windowState = previousWindow.saveState(1);
    }
    // Saved layouts can re-show docks after the visibility settings below.
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    editor->setFocus();
    QTRY_VERIFY(focusIsWithin(editor));

    QTest::keyClick(editor, Qt::Key_F6);
    QCoreApplication::processEvents();
    QVERIFY(focusIsWithin(editor));

    QVERIFY(QMetaObject::invokeMethod(&window, "setConstantsDockVisible",
                                      Qt::DirectConnection,
                                      Q_ARG(bool, true),
                                      Q_ARG(bool, false)));
    QCoreApplication::processEvents();
    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QLineEdit* searchBox = constantsDock->findChild<QLineEdit*>();
    QTreeWidget* table = constantsDock->findChild<QTreeWidget*>();
    QVERIFY(searchBox != nullptr);
    QVERIFY(table != nullptr);
    QVERIFY(searchBox->isVisible());
    QVERIFY(table->isVisible());

    // F6 is explicit focus intent and must supersede delayed focus restoration
    // scheduled by a recent window activation.
    QEvent windowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&window, &windowActivate);
    QCoreApplication::processEvents();

    editor->setFocus();
    QTRY_VERIFY(focusIsWithin(editor));

    QTest::keyClick(editor, Qt::Key_F6);
    QCoreApplication::processEvents();
    QTRY_VERIFY(focusIsWithin(searchBox));

    QTest::keyClick(searchBox, Qt::Key_F6);
    QCoreApplication::processEvents();
    QTRY_VERIFY(focusIsWithin(table));
    QTest::qWait(175);
    QVERIFY(focusIsWithin(table));

    QTest::keyClick(table, Qt::Key_F6);
    QCoreApplication::processEvents();
    QTRY_VERIFY(focusIsWithin(editor));

    QVERIFY(QMetaObject::invokeMethod(&window, "cycleFocusBackward", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_VERIFY(focusIsWithin(table));
}

void TestDisplayUi::f6_cycles_focus_with_another_main_window_visible()
{
    QFETCH(bool, floatingDock);
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow firstWindow(false);
    MainWindow secondWindow(false);
    firstWindow.show();
    secondWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&firstWindow));
    QVERIFY(QTest::qWaitForWindowExposed(&secondWindow));

    for (MainWindow* window : {&firstWindow, &secondWindow}) {
        window->activateWindow();
        Editor* editor = window->findChild<Editor*>();
        QDockWidget* dock = window->findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
        QVERIFY(editor != nullptr);
        QVERIFY(dock != nullptr);
        dock->setFloating(floatingDock);
        window->activateWindow();
        QLineEdit* searchBox = dock->findChild<QLineEdit*>();
        QTreeWidget* table = dock->findChild<QTreeWidget*>();
        QVERIFY(searchBox != nullptr);
        QVERIFY(table != nullptr);
        editor->setFocus();
        QTRY_VERIFY(focusIsWithin(editor));

        QTest::keyClick(editor, Qt::Key_F6);
        QTRY_VERIFY(focusIsWithin(searchBox));
        if (floatingDock)
            dock->activateWindow();
        QTest::keyClick(searchBox, Qt::Key_F6);
        QTRY_VERIFY(focusIsWithin(table));
        QTest::keyClick(table, Qt::Key_F6);
        QTRY_VERIFY(focusIsWithin(editor));

        QTest::keyClick(editor, Qt::Key_F6, Qt::ShiftModifier);
        QTRY_VERIFY(focusIsWithin(table));
        QTest::keyClick(table, Qt::Key_F6, Qt::ShiftModifier);
        QTRY_VERIFY(focusIsWithin(searchBox));
        QTest::keyClick(searchBox, Qt::Key_F6, Qt::ShiftModifier);
        QTRY_VERIFY(focusIsWithin(editor));
    }
}

void TestDisplayUi::f6_cycles_focus_with_another_main_window_visible_data()
{
    QTest::addColumn<bool>("floatingDock");
    QTest::newRow("docked") << false;
    QTest::newRow("floating") << true;
}

void TestDisplayUi::dock_search_focus_suppresses_editor_primary_outline_across_panes()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QString oldColorScheme;
        QString oldCustomColorSchemeJson;
        QString oldSessionLayoutJson;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldWindowPositionSave;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->colorScheme = oldColorScheme;
            settings->customColorSchemeJson = oldCustomColorSchemeJson;
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->windowPositionSave = oldWindowPositionSave;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->colorScheme,
        settings->customColorSchemeJson,
        settings->sessionLayoutJson,
        settings->constantsDockVisible,
        settings->functionsDockVisible,
        settings->historyDockVisible,
        settings->keypadVisible,
        settings->formulaBookDockVisible,
        settings->variablesDockVisible,
        settings->userFunctionsDockVisible,
        settings->userUnitsDockVisible,
        settings->bitfieldVisible,
        settings->windowPositionSave,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{{QStringLiteral("background"), QStringLiteral("#1f3229")}});
    settings->sessionLayoutJson.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    const QColor primary = generatePrimaryFromBackground(QColor(QStringLiteral("#1f3229")));

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(window.findChildren<Editor*>().size(), 2);

    const QVector<QColor> shades =
        generateOklchShades(QColor(QStringLiteral("#1f3229")), 6, ThemePolarity::Dark);
    const QColor selectedTabFill = shades.at(UiConfig::SelectedSessionTabFillShade);
    QList<QTabBar*> tabBars;
    for (ResultDisplay* display : window.findChildren<ResultDisplay*>()) {
        QTabBar* tabBar = tabBarForDisplay(display);
        QVERIFY(tabBar != nullptr);
        QVERIFY(tabBar->isVisible());
        QVERIFY(tabBar->count() > 0);
        tabBars.append(tabBar);
    }
    QCOMPARE(tabBars.size(), 2);
    for (QTabBar* tabBar : tabBars) {
        QVERIFY2(colorsAreClose(selectedSessionTabFillColor(tabBar), selectedTabFill),
                 qPrintable(QStringLiteral("fill=%1 expected=%2")
                                .arg(selectedSessionTabFillColor(tabBar).name(),
                                     selectedTabFill.name())));
    }
    QCOMPARE(std::count_if(tabBars.cbegin(), tabBars.cend(), [&primary](QTabBar* tabBar) {
        return selectedSessionTabHasBottomIndicator(tabBar, primary);
    }), 1);

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QLineEdit* searchBox = constantsDock->findChild<QLineEdit*>();
    QVERIFY(searchBox != nullptr);

    QList<Editor*> editors = window.findChildren<Editor*>();
    QTRY_VERIFY(anyEditorHasPrimaryOutline(editors, primary));

    constantsDock->show();
    constantsDock->raise();
    QCoreApplication::processEvents();
    QTest::mouseClick(searchBox, Qt::LeftButton);
    QTRY_VERIFY(searchBox->hasFocus());
    QTRY_VERIFY(!anyEditorHasPrimaryOutline(window.findChildren<Editor*>(), primary));

    for (int i = 0; i < editors.size(); ++i) {
        editors.at(i)->setText(QStringLiteral("pane %1").arg(i + 1));
        QCoreApplication::processEvents();
        QVERIFY(searchBox->hasFocus());
        QVERIFY2(!anyEditorHasPrimaryOutline(window.findChildren<Editor*>(), primary),
                 qPrintable(editors.at(i)->styleSheet()));
    }
}

void TestDisplayUi::dock_selection_inserts_into_active_session_pane_after_focus_transfer()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QString oldSessionLayoutJson;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldWindowPositionSave;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->windowPositionSave = oldWindowPositionSave;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->sessionLayoutJson,
        settings->constantsDockVisible,
        settings->functionsDockVisible,
        settings->historyDockVisible,
        settings->keypadVisible,
        settings->formulaBookDockVisible,
        settings->variablesDockVisible,
        settings->userFunctionsDockVisible,
        settings->userUnitsDockVisible,
        settings->bitfieldVisible,
        settings->windowPositionSave,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->sessionLayoutJson.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    ResultDisplay* firstDisplay = window.findChild<ResultDisplay*>();
    QVERIFY(firstDisplay != nullptr);
    QWidget* firstPage = firstDisplay->parentWidget();
    Editor* firstEditor = firstPage ? firstPage->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    QVERIFY(firstEditor != nullptr);

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(window.findChildren<ResultDisplay*>().size(), 2);

    const QList<ResultDisplay*> displays = window.findChildren<ResultDisplay*>();
    ResultDisplay* secondDisplay = displays.first() == firstDisplay ? displays.last() : displays.first();
    QWidget* secondPage = secondDisplay->parentWidget();
    Editor* secondEditor = secondPage ? secondPage->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    QVERIFY(secondEditor != nullptr);

    secondEditor->setText(QString());
    firstEditor->setText(QStringLiteral("x"));
    firstEditor->setCursorPosition(firstEditor->text().size());
    QWidget* firstPane = paneWidgetForDisplay(firstDisplay);
    QWidget* secondPane = paneWidgetForDisplay(secondDisplay);
    QVERIFY(firstPane != nullptr);
    QVERIFY(secondPane != nullptr);
    QCOMPARE(firstPane->objectName(), QStringLiteral("SessionPane"));
    QCOMPARE(secondPane->objectName(), QStringLiteral("SessionPane"));
    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QTreeWidget* constantsList = constantsDock->findChild<QTreeWidget*>();
    QVERIFY(constantsList != nullptr);

    constantsDock->show();
    constantsDock->raise();
    QCoreApplication::processEvents();
    QTest::mouseClick(constantsList->viewport(), Qt::LeftButton, Qt::NoModifier,
                      constantsList->viewport()->rect().center());
    QTRY_VERIFY(constantsList->hasFocus());

    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "insertConstantIntoEditor",
                                      Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("pi"))));
    QVERIFY2(firstEditor->text() == QStringLiteral("x") && secondEditor->text() == QStringLiteral("pi"),
             qPrintable(QStringLiteral("first='%1' second='%2'")
                            .arg(firstEditor->text(), secondEditor->text())));
}

void TestDisplayUi::clicking_tab_activates_own_pane_in_nested_split_layout()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QString oldColorScheme;
        QString oldCustomColorSchemeJson;
        QString oldSessionLayoutJson;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldWindowPositionSave;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->colorScheme = oldColorScheme;
            settings->customColorSchemeJson = oldCustomColorSchemeJson;
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->windowPositionSave = oldWindowPositionSave;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->colorScheme,
        settings->customColorSchemeJson,
        settings->sessionLayoutJson,
        settings->constantsDockVisible,
        settings->functionsDockVisible,
        settings->historyDockVisible,
        settings->keypadVisible,
        settings->formulaBookDockVisible,
        settings->variablesDockVisible,
        settings->userFunctionsDockVisible,
        settings->userUnitsDockVisible,
        settings->bitfieldVisible,
        settings->windowPositionSave,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{{QStringLiteral("background"), QStringLiteral("#1f3229")}});
    settings->sessionLayoutJson.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    const QColor primary = generatePrimaryFromBackground(QColor(QStringLiteral("#1f3229")));

    MainWindow window;
    window.resize(900, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneDown", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(window.findChildren<ResultDisplay*>().size(), 3);

    QList<ResultDisplay*> displays = window.findChildren<ResultDisplay*>();
    std::sort(displays.begin(), displays.end(), [](ResultDisplay* lhs, ResultDisplay* rhs) {
        const QPoint lhsPos = lhs->mapToGlobal(QPoint(0, 0));
        const QPoint rhsPos = rhs->mapToGlobal(QPoint(0, 0));
        if (lhsPos.y() != rhsPos.y())
            return lhsPos.y() < rhsPos.y();
        return lhsPos.x() < rhsPos.x();
    });
    ResultDisplay* paneA = displays.at(0);
    ResultDisplay* paneB = displays.at(1);
    ResultDisplay* paneC = displays.at(2);
    QVERIFY(paneA->mapToGlobal(QPoint(0, 0)).x() < paneB->mapToGlobal(QPoint(0, 0)).x());
    QVERIFY(paneC->mapToGlobal(QPoint(0, 0)).y() > paneB->mapToGlobal(QPoint(0, 0)).y());

    Editor* editorA = editorForDisplay(paneA);
    Editor* editorB = editorForDisplay(paneB);
    Editor* editorC = editorForDisplay(paneC);
    QTabBar* tabA = tabBarForDisplay(paneA);
    QTabBar* tabB = tabBarForDisplay(paneB);
    QTabBar* tabC = tabBarForDisplay(paneC);
    QVERIFY(editorA != nullptr);
    QVERIFY(editorB != nullptr);
    QVERIFY(editorC != nullptr);
    QVERIFY(tabA != nullptr);
    QVERIFY(tabB != nullptr);
    QVERIFY(tabC != nullptr);
    QVERIFY(qAbs(tabA->mapToGlobal(QPoint(0, 0)).x() - paneA->mapToGlobal(QPoint(0, 0)).x()) < 8);
    QVERIFY(qAbs(tabB->mapToGlobal(QPoint(0, 0)).x() - paneB->mapToGlobal(QPoint(0, 0)).x()) < 8);
    QVERIFY(qAbs(tabC->mapToGlobal(QPoint(0, 0)).x() - paneC->mapToGlobal(QPoint(0, 0)).x()) < 8);

    QTest::mouseClick(paneB->viewport(), Qt::LeftButton, Qt::NoModifier,
                      paneB->viewport()->rect().center());
    QTRY_VERIFY(editorHasPrimaryOutline(editorB, primary));
    QVERIFY(selectedSessionTabHasBottomIndicator(tabB, primary));

    QVERIFY(tabA->count() > 0);
    QTest::mouseClick(tabA, Qt::LeftButton, Qt::NoModifier,
                      tabA->tabRect(0).center());
    QTRY_VERIFY2(editorHasPrimaryOutline(editorA, primary),
                 qPrintable(QStringLiteral("A=%1 B=%2 C=%3 tabA=%4 tabB=%5 tabC=%6 focus=%7")
                                .arg(editorHasPrimaryOutline(editorA, primary))
                                .arg(editorHasPrimaryOutline(editorB, primary))
                                .arg(editorHasPrimaryOutline(editorC, primary))
                                .arg(selectedSessionTabHasBottomIndicator(tabA, primary))
                                .arg(selectedSessionTabHasBottomIndicator(tabB, primary))
                                .arg(selectedSessionTabHasBottomIndicator(tabC, primary))
                                .arg(QApplication::focusWidget()
                                         ? QString::fromLatin1(QApplication::focusWidget()->metaObject()->className())
                                         : QStringLiteral("<none>"))));
    QVERIFY(selectedSessionTabHasBottomIndicator(tabA, primary));
    QVERIFY(!editorHasPrimaryOutline(editorC, primary));
    QVERIFY(!selectedSessionTabHasBottomIndicator(tabC, primary));
}

void TestDisplayUi::active_pane_survives_window_reactivation_focus_replay()
{
    MainWindowStateGuard guard;
    Settings* settings = Settings::instance();
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{{QStringLiteral("background"), QStringLiteral("#1f3229")}});
    settings->sessionLayoutJson.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    const QColor primary = generatePrimaryFromBackground(QColor(QStringLiteral("#1f3229")));

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(window.findChildren<ResultDisplay*>().size(), 3);

    QList<ResultDisplay*> displays = window.findChildren<ResultDisplay*>();
    std::sort(displays.begin(), displays.end(), [](ResultDisplay* lhs, ResultDisplay* rhs) {
        return lhs->mapToGlobal(QPoint(0, 0)).x() < rhs->mapToGlobal(QPoint(0, 0)).x();
    });

    ResultDisplay* firstDisplay = displays.at(0);
    Editor* firstEditor = editorForDisplay(firstDisplay);
    QVERIFY(firstEditor != nullptr);

    QTest::mouseClick(firstDisplay->viewport(), Qt::LeftButton, Qt::NoModifier,
                      firstDisplay->viewport()->rect().center());
    QTRY_VERIFY(editorHasPrimaryOutline(firstEditor, primary));

    for (ResultDisplay* display : displays) {
        Editor* editor = editorForDisplay(display);
        QVERIFY(editor != nullptr);
        QFocusEvent focusIn(QEvent::FocusIn, Qt::OtherFocusReason);
        QCoreApplication::sendEvent(editor->viewport(), &focusIn);
    }
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "insertTextIntoEditor",
                                      Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("p"))));
    QVERIFY2(firstEditor->text() == QStringLiteral("p"),
             qPrintable(QStringLiteral("passive replay pane0='%1' pane1='%2' pane2='%3'")
                            .arg(editorForDisplay(displays.at(0))->text(),
                                 editorForDisplay(displays.at(1))->text(),
                                 editorForDisplay(displays.at(2))->text())));
    firstEditor->clear();

    // Let pane-creation focus timers settle, then reselect the first pane so
    // WindowDeactivate captures the same stable state as a real app switch.
    QCoreApplication::processEvents();
    QTest::mouseClick(firstDisplay->viewport(), Qt::LeftButton, Qt::NoModifier,
                      firstDisplay->viewport()->rect().center());
    QTRY_VERIFY(editorHasPrimaryOutline(firstEditor, primary));

    QEvent windowDeactivate(QEvent::WindowDeactivate);
    QCoreApplication::sendEvent(&window, &windowDeactivate);
    QEvent windowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&window, &windowActivate);
    for (ResultDisplay* display : displays) {
        Editor* editor = editorForDisplay(display);
        QVERIFY(editor != nullptr);
        QFocusEvent focusIn(QEvent::FocusIn, Qt::OtherFocusReason);
        QCoreApplication::sendEvent(editor->viewport(), &focusIn);
    }
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "insertTextIntoEditor",
                                      Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("z"))));
    QVERIFY2(firstEditor->text() == QStringLiteral("z"),
             qPrintable(QStringLiteral("after activation pane0='%1' pane1='%2' pane2='%3'")
                            .arg(editorForDisplay(displays.at(0))->text(),
                                 editorForDisplay(displays.at(1))->text(),
                                 editorForDisplay(displays.at(2))->text())));

    QTRY_VERIFY2(editorHasPrimaryOutline(firstEditor, primary),
                 qPrintable(QStringLiteral("pane0=%1 pane1=%2 pane2=%3 focus=%4")
                                .arg(editorHasPrimaryOutline(editorForDisplay(displays.at(0)), primary))
                                .arg(editorHasPrimaryOutline(editorForDisplay(displays.at(1)), primary))
                                .arg(editorHasPrimaryOutline(editorForDisplay(displays.at(2)), primary))
                                .arg(QApplication::focusWidget()
                                         ? QString::fromLatin1(QApplication::focusWidget()->metaObject()->className())
                                         : QStringLiteral("<none>"))));
    for (int i = 1; i < displays.size(); ++i)
        QVERIFY(!editorHasPrimaryOutline(editorForDisplay(displays.at(i)), primary));
}

void TestDisplayUi::extra_window_activation_restores_own_active_tab_indicator()
{
    MainWindowStateGuard guard;
    Settings* settings = Settings::instance();
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{{QStringLiteral("background"), QStringLiteral("#1f3229")}});
    settings->sessionLayoutJson.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    const QColor primary = generatePrimaryFromBackground(QColor(QStringLiteral("#1f3229")));

    MainWindow primaryWindow;
    primaryWindow.resize(800, 500);
    primaryWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&primaryWindow));

    MainWindow extraWindow;
    extraWindow.resize(900, 500);
    extraWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&extraWindow));

    QList<ResultDisplay*> extraDisplays = extraWindow.findChildren<ResultDisplay*>();
    QCOMPARE(extraDisplays.size(), 1);
    ResultDisplay* extraDisplay = extraDisplays.constFirst();
    QTabBar* extraTabBar = tabBarForDisplay(extraDisplay);
    QVERIFY(extraTabBar != nullptr);

    QTest::mouseClick(extraDisplay->viewport(), Qt::LeftButton, Qt::NoModifier,
                      extraDisplay->viewport()->rect().center());
    QVERIFY(QMetaObject::invokeMethod(&extraWindow, "showNewSessionDialog", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(extraTabBar->count(), 2);
    QTRY_VERIFY(extraTabBar->isVisible());
    QTRY_VERIFY(selectedSessionTabHasBottomIndicator(extraTabBar, primary));

    ResultDisplay* primaryDisplay = primaryWindow.findChild<ResultDisplay*>();
    QVERIFY(primaryDisplay != nullptr);
    QTest::mouseClick(primaryDisplay->viewport(), Qt::LeftButton, Qt::NoModifier,
                      primaryDisplay->viewport()->rect().center());
    QCoreApplication::processEvents();

    QEvent primaryDeactivate(QEvent::WindowDeactivate);
    QCoreApplication::sendEvent(&primaryWindow, &primaryDeactivate);
    QEvent extraActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&extraWindow, &extraActivate);
    QCoreApplication::processEvents();

    QTRY_VERIFY(selectedSessionTabHasBottomIndicator(extraTabBar, primary));
}

void TestDisplayUi::focused_dock_search_survives_window_reactivation_focus_replay()
{
    MainWindowStateGuard guard;
    Settings* settings = Settings::instance();
    settings->sessionLayoutJson.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(window.findChildren<Editor*>().size(), 2);

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QLineEdit* searchBox = constantsDock->findChild<QLineEdit*>();
    QVERIFY(searchBox != nullptr);

    constantsDock->show();
    constantsDock->raise();
    QCoreApplication::processEvents();
    QTest::mouseClick(searchBox, Qt::LeftButton);
    QTRY_VERIFY(searchBox->hasFocus());

    QEvent windowDeactivate(QEvent::WindowDeactivate);
    QCoreApplication::sendEvent(&window, &windowDeactivate);
    QEvent windowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&window, &windowActivate);
    for (Editor* editor : window.findChildren<Editor*>()) {
        QFocusEvent focusIn(QEvent::FocusIn, Qt::OtherFocusReason);
        QCoreApplication::sendEvent(editor->viewport(), &focusIn);
    }
    QCoreApplication::processEvents();

    QTRY_VERIFY(searchBox->hasFocus());
    QTest::keyClicks(searchBox, "mol");
    QCOMPARE(searchBox->text(), QStringLiteral("mol"));
}

void TestDisplayUi::focusing_loaded_pane_preserves_its_current_scroll_position()
{
    Settings* appSettings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;
        QString oldSessionLayoutJson;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldWindowPositionSave;
        bool oldHasNumberFormatStyleSetting;

        ~SettingsGuard()
        {
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->windowPositionSave = oldWindowPositionSave;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        appSettings,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        appSettings->sessionLayoutJson,
        appSettings->constantsDockVisible,
        appSettings->functionsDockVisible,
        appSettings->historyDockVisible,
        appSettings->keypadVisible,
        appSettings->formulaBookDockVisible,
        appSettings->variablesDockVisible,
        appSettings->userFunctionsDockVisible,
        appSettings->userUnitsDockVisible,
        appSettings->bitfieldVisible,
        appSettings->windowPositionSave,
        appSettings->hasNumberFormatStyleSetting
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");

    appSettings->sessionLayoutJson.clear();
    appSettings->constantsDockVisible = false;
    appSettings->functionsDockVisible = false;
    appSettings->historyDockVisible = false;
    appSettings->keypadVisible = false;
    appSettings->formulaBookDockVisible = false;
    appSettings->variablesDockVisible = false;
    appSettings->userFunctionsDockVisible = false;
    appSettings->userUnitsDockVisible = false;
    appSettings->bitfieldVisible = false;
    appSettings->windowPositionSave = false;
    appSettings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QCoreApplication::processEvents();

    ResultDisplay* firstDisplay = window.findChild<ResultDisplay*>();
    Editor* firstEditor = window.findChild<Editor*>();
    QVERIFY(firstDisplay != nullptr);
    QVERIFY(firstEditor != nullptr);

    for (int i = 0; i < 80; ++i) {
        firstEditor->setText(QString::number(i));
        QVERIFY(QMetaObject::invokeMethod(&window, "evaluateEditorExpression", Qt::DirectConnection));
    }
    QCoreApplication::processEvents();

    QScrollBar* firstScrollBar = firstDisplay->verticalScrollBar();
    QVERIFY(firstScrollBar->maximum() > 20);
    firstScrollBar->setValue(10);
    QCOMPARE(firstScrollBar->value(), 10);

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();

    const QList<ResultDisplay*> displays = window.findChildren<ResultDisplay*>();
    QCOMPARE(displays.size(), 2);
    ResultDisplay* secondDisplay = displays.first() == firstDisplay ? displays.last() : displays.first();
    QWidget* secondPage = secondDisplay->parentWidget();
    Editor* secondEditor = secondPage ? secondPage->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    QVERIFY(secondEditor != nullptr);
    QTRY_VERIFY(secondEditor->hasFocus());

    // Make the already-loaded first pane diverge from the scroll snapshot that
    // was captured when focus moved to the second pane.
    firstScrollBar->setValue(20);
    QCOMPARE(firstScrollBar->value(), 20);

    QTest::mouseClick(firstDisplay->viewport(), Qt::LeftButton, Qt::NoModifier,
                      firstDisplay->viewport()->rect().center());
    QCoreApplication::processEvents();
    QTRY_VERIFY(firstEditor->hasFocus());

    QTabBar* secondTabBar = paneWidgetForDisplay(secondDisplay)->findChild<QTabBar*>();
    QVERIFY(secondTabBar != nullptr);
    QVERIFY(secondTabBar->isVisible());
    QTest::mouseClick(secondTabBar, Qt::LeftButton, Qt::NoModifier,
                      secondTabBar->tabRect(secondTabBar->currentIndex()).center());
    QCoreApplication::processEvents();
    QTRY_VERIFY(secondEditor->hasFocus());

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    QCOMPARE(firstScrollBar->value(), 20);
}

void TestDisplayUi::persisting_layout_captures_visible_scroll_positions_for_all_panes()
{
    Settings* appSettings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;
        QString oldSessionLayoutJson;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldWindowPositionSave;
        bool oldHasNumberFormatStyleSetting;

        ~SettingsGuard()
        {
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->windowPositionSave = oldWindowPositionSave;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        appSettings,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        appSettings->sessionLayoutJson,
        appSettings->constantsDockVisible,
        appSettings->functionsDockVisible,
        appSettings->historyDockVisible,
        appSettings->keypadVisible,
        appSettings->formulaBookDockVisible,
        appSettings->variablesDockVisible,
        appSettings->userFunctionsDockVisible,
        appSettings->userUnitsDockVisible,
        appSettings->bitfieldVisible,
        appSettings->windowPositionSave,
        appSettings->hasNumberFormatStyleSetting
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");

    appSettings->sessionLayoutJson.clear();
    appSettings->constantsDockVisible = false;
    appSettings->functionsDockVisible = false;
    appSettings->historyDockVisible = false;
    appSettings->keypadVisible = false;
    appSettings->formulaBookDockVisible = false;
    appSettings->variablesDockVisible = false;
    appSettings->userFunctionsDockVisible = false;
    appSettings->userUnitsDockVisible = false;
    appSettings->bitfieldVisible = false;
    appSettings->windowPositionSave = false;
    appSettings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QCoreApplication::processEvents();

    ResultDisplay* firstDisplay = window.findChild<ResultDisplay*>();
    Editor* firstEditor = window.findChild<Editor*>();
    QVERIFY(firstDisplay != nullptr);
    QVERIFY(firstEditor != nullptr);

    for (int i = 0; i < 80; ++i) {
        firstEditor->setText(QString::number(i));
        QVERIFY(QMetaObject::invokeMethod(&window, "evaluateEditorExpression", Qt::DirectConnection));
    }
    QCoreApplication::processEvents();

    QScrollBar* firstScrollBar = firstDisplay->verticalScrollBar();
    QVERIFY(firstScrollBar->maximum() > 30);
    firstScrollBar->setValue(10);
    QCOMPARE(firstScrollBar->value(), 10);

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();

    const QList<ResultDisplay*> displays = window.findChildren<ResultDisplay*>();
    QCOMPARE(displays.size(), 2);
    ResultDisplay* secondDisplay = displays.first() == firstDisplay ? displays.last() : displays.first();
    QWidget* secondPage = secondDisplay->parentWidget();
    Editor* secondEditor = secondPage ? secondPage->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    QVERIFY(secondEditor != nullptr);

    for (int i = 0; i < 80; ++i) {
        secondEditor->setText(QString::number(i + 100));
        QVERIFY(QMetaObject::invokeMethod(&window, "evaluateEditorExpression", Qt::DirectConnection));
    }
    QCoreApplication::processEvents();

    QScrollBar* secondScrollBar = secondDisplay->verticalScrollBar();
    QVERIFY(secondScrollBar->maximum() > 30);
    secondScrollBar->setValue(15);
    QCOMPARE(secondScrollBar->value(), 15);

    firstEditor->setText(QStringLiteral("first pane draft"));
    firstEditor->setCursorPosition(5);
    secondEditor->setText(QStringLiteral("second pane draft"));
    secondEditor->setCursorPosition(6);

    firstScrollBar->setValue(22);
    QCOMPARE(firstScrollBar->value(), 22);

    window.persistSessionAndSettingsForShutdown();

    const QJsonDocument layoutDoc = QJsonDocument::fromJson(appSettings->sessionLayoutJson.toUtf8());
    QVERIFY(layoutDoc.isObject());
    const QJsonArray windows = layoutDoc.object().value(QStringLiteral("windows")).toArray();
    QCOMPARE(windows.size(), 1);
    const QJsonObject root = windows.first().toObject().value(QStringLiteral("root")).toObject();
    QList<int> scrollValues;
    appendPaneScrollValues(root, &scrollValues);

    QVERIFY(scrollValues.contains(22));
    QVERIFY(scrollValues.contains(15));

    QStringList editorTexts;
    appendPaneEditorTexts(root, &editorTexts);
    QVERIFY(editorTexts.contains(QStringLiteral("first pane draft")));
    QVERIFY(editorTexts.contains(QStringLiteral("second pane draft")));
}

void TestDisplayUi::switching_session_tabs_preserves_each_editor_text()
{
    Settings* appSettings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;
        QString oldSessionLayoutJson;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldWindowPositionSave;
        bool oldHasNumberFormatStyleSetting;

        ~SettingsGuard()
        {
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->windowPositionSave = oldWindowPositionSave;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
        }
    } guard {
        appSettings,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        appSettings->sessionLayoutJson,
        appSettings->constantsDockVisible,
        appSettings->functionsDockVisible,
        appSettings->historyDockVisible,
        appSettings->keypadVisible,
        appSettings->formulaBookDockVisible,
        appSettings->variablesDockVisible,
        appSettings->userFunctionsDockVisible,
        appSettings->userUnitsDockVisible,
        appSettings->bitfieldVisible,
        appSettings->windowPositionSave,
        appSettings->hasNumberFormatStyleSetting
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    appSettings->sessionLayoutJson.clear();
    appSettings->constantsDockVisible = false;
    appSettings->functionsDockVisible = false;
    appSettings->historyDockVisible = false;
    appSettings->keypadVisible = false;
    appSettings->formulaBookDockVisible = false;
    appSettings->variablesDockVisible = false;
    appSettings->userFunctionsDockVisible = false;
    appSettings->userUnitsDockVisible = false;
    appSettings->bitfieldVisible = false;
    appSettings->windowPositionSave = false;
    appSettings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(display != nullptr);
    QWidget* pane = paneWidgetForDisplay(display);
    QTabBar* tabBar = pane ? pane->findChild<QTabBar*>() : nullptr;
    QVERIFY(tabBar != nullptr);

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    editor->setText(QStringLiteral("first tab draft"));
    editor->setCursorPosition(5);
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(tabBar->count(), 2);
    QCOMPARE(tabBar->currentIndex(), 1);

    editor->setText(QStringLiteral("second tab draft"));
    editor->setCursorPosition(6);
    QCoreApplication::processEvents();

    QTest::mouseClick(tabBar, Qt::LeftButton, Qt::NoModifier, tabBar->tabRect(0).center());
    QCoreApplication::processEvents();
    QCOMPARE(editor->text(), QStringLiteral("first tab draft"));

    QTest::mouseClick(tabBar, Qt::LeftButton, Qt::NoModifier, tabBar->tabRect(1).center());
    QCoreApplication::processEvents();
    QCOMPARE(editor->text(), QStringLiteral("second tab draft"));
}

void TestDisplayUi::session_tabs_show_full_name_tooltip_on_hover()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QTabBar* tabBar = tabBarForDisplay(display);
    QVERIFY(tabBar != nullptr);

    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QTRY_COMPARE(tabBar->count(), 2);
    QVERIFY(tabBar->isVisible());

    const QString fullName =
        QStringLiteral("A session name that is deliberately much too long to fit "
                       "inside the available tab label area at hover time");
    tabBar->setTabText(0, fullName);
    const QPoint hoverPos = tabBar->tabRect(0).center();
    QMouseEvent moveEvent(QEvent::MouseMove,
                          QPointF(hoverPos),
                          QPointF(tabBar->mapToGlobal(hoverPos)),
                          Qt::NoButton,
                          Qt::NoButton,
                          Qt::NoModifier);
    QCoreApplication::sendEvent(tabBar, &moveEvent);

    QFrame* toolTipPopup =
        tabBar->findChild<QFrame*>(QStringLiteral("sessionTabToolTipPopup"));
    QTRY_VERIFY(toolTipPopup != nullptr && toolTipPopup->isVisible());
    QLabel* toolTipLabel =
        toolTipPopup->findChild<QLabel*>(QStringLiteral("sessionTabToolTipPopupLabel"));
    QVERIFY(toolTipLabel != nullptr);
    QCOMPARE(toolTipLabel->text(), fullName);

    QVERIFY(toolTipPopup->palette().color(QPalette::Window).isValid());
    QVERIFY(toolTipPopup->palette().color(QPalette::WindowText).isValid());
    QVERIFY(toolTipPopup->styleSheet().contains(
        QStringLiteral("border-radius: %1px")
            .arg(UiConfig::CompletionPopupCornerRadius)));

    tabBar->setTabText(0, QStringLiteral("Short"));
    QCoreApplication::sendEvent(tabBar, &moveEvent);
    QTRY_VERIFY(!toolTipPopup->isVisible());
}

void TestDisplayUi::session_tab_context_menu_opens_on_right_click()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->statusBarVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(display != nullptr);
    QTabBar* tabBar = tabBarForDisplay(display);
    QVERIFY(tabBar != nullptr);
    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QTRY_COMPARE(tabBar->count(), 3);
    tabBar->show();
    QCoreApplication::processEvents();

    bool menuOpened = false;
    QStringList menuActionTexts;
    QTimer::singleShot(0, &window, [&]() {
        QMenu* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu == nullptr)
            return;
        menuOpened = true;
        for (QAction* action : menu->actions()) {
            if (!action->isSeparator())
                menuActionTexts.append(action->text());
        }
        menu->close();
    });

    const QPoint tabPosition = tabBar->tabRect(0).center();
    QContextMenuEvent contextMenuEvent(QContextMenuEvent::Mouse,
                                       tabPosition,
                                       tabBar->mapToGlobal(tabPosition));
    QCoreApplication::sendEvent(tabBar, &contextMenuEvent);
    QTRY_VERIFY(menuOpened);
    QVERIFY(menuActionTexts.contains(QStringLiteral("Rename Session")));
    QVERIFY(menuActionTexts.contains(QStringLiteral("Close Session")));
}

void TestDisplayUi::session_tab_navigation_shortcuts_switch_tabs()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->statusBarVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(display != nullptr);
    QTabBar* tabBar = tabBarForDisplay(display);
    QVERIFY(tabBar != nullptr);
    Editor* editor = editorForDisplay(display);
    QVERIFY(editor != nullptr);

    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(tabBar->count(), 3);

    tabBar->setCurrentIndex(0);
    editor->setFocus();
    QTRY_VERIFY(editor->hasFocus());
    QCOMPARE(tabBar->currentIndex(), 0);

    const QKeyCombination next = nextSessionTabShortcut();
    QTest::keyClick(editor, next.key(), next.keyboardModifiers());
    QTRY_COMPARE(tabBar->currentIndex(), 1);

    QTest::keyClick(editor, next.key(), next.keyboardModifiers());
    QTRY_COMPARE(tabBar->currentIndex(), 2);

    const QKeyCombination previous = previousSessionTabShortcut();
    QTest::keyClick(editor, previous.key(), previous.keyboardModifiers());
    QTRY_COMPARE(tabBar->currentIndex(), 1);
}

void TestDisplayUi::new_tab_menu_action_and_shortcut_create_session_in_active_pane()
{
    Settings* appSettings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;
        QString oldSessionLayoutJson;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldWindowPositionSave;
        bool oldHasNumberFormatStyleSetting;

        ~SettingsGuard()
        {
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->windowPositionSave = oldWindowPositionSave;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
        }
    } guard {
        appSettings,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        appSettings->sessionLayoutJson,
        appSettings->constantsDockVisible,
        appSettings->functionsDockVisible,
        appSettings->historyDockVisible,
        appSettings->keypadVisible,
        appSettings->formulaBookDockVisible,
        appSettings->variablesDockVisible,
        appSettings->userFunctionsDockVisible,
        appSettings->userUnitsDockVisible,
        appSettings->bitfieldVisible,
        appSettings->windowPositionSave,
        appSettings->hasNumberFormatStyleSetting
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    appSettings->sessionLayoutJson.clear();
    appSettings->constantsDockVisible = false;
    appSettings->functionsDockVisible = false;
    appSettings->historyDockVisible = false;
    appSettings->keypadVisible = false;
    appSettings->formulaBookDockVisible = false;
    appSettings->variablesDockVisible = false;
    appSettings->userFunctionsDockVisible = false;
    appSettings->userUnitsDockVisible = false;
    appSettings->bitfieldVisible = false;
    appSettings->windowPositionSave = false;
    appSettings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();

    const QList<ResultDisplay*> displays = window.findChildren<ResultDisplay*>();
    QCOMPARE(displays.size(), 2);
    ResultDisplay* firstDisplay = displays.at(0);
    ResultDisplay* secondDisplay = displays.at(1);
    QWidget* firstPane = paneWidgetForDisplay(firstDisplay);
    QWidget* secondPane = paneWidgetForDisplay(secondDisplay);
    QTabBar* firstTabBar = firstPane ? firstPane->findChild<QTabBar*>() : nullptr;
    QTabBar* secondTabBar = secondPane ? secondPane->findChild<QTabBar*>() : nullptr;
    QVERIFY(firstTabBar != nullptr);
    QVERIFY(secondTabBar != nullptr);
    QCOMPARE(firstTabBar->count(), 1);
    QCOMPARE(secondTabBar->count(), 1);

    QTest::mouseClick(secondDisplay->viewport(), Qt::LeftButton, Qt::NoModifier,
                      secondDisplay->viewport()->rect().center());
    QCoreApplication::processEvents();

    QMenu* sessionMenu = menuWithTitle(window.menuBar(), QStringLiteral("&Session"));
    QVERIFY(sessionMenu != nullptr);
    QAction* newTabAction = directMenuActionWithText(sessionMenu, QStringLiteral("New &Tab"));
    QVERIFY(newTabAction != nullptr);
    QVERIFY(directMenuActionWithText(sessionMenu, QStringLiteral("New Session")) == nullptr);

    newTabAction->trigger();
    QCoreApplication::processEvents();

    QCOMPARE(firstTabBar->count(), 1);
    QTRY_COMPARE(secondTabBar->count(), 2);
    QCOMPARE(secondTabBar->currentIndex(), 1);

    const QList<QKeySequence> bindings = QKeySequence::keyBindings(QKeySequence::AddTab);
    QVERIFY(!bindings.isEmpty());
    QVERIFY(newTabAction->shortcuts().contains(bindings.first()));
    const QKeyCombination shortcut = bindings.first()[0];
    QTest::keyClick(&window, shortcut.key(), shortcut.keyboardModifiers());
    QCoreApplication::processEvents();

    QCOMPARE(firstTabBar->count(), 1);
    QTRY_COMPARE(secondTabBar->count(), 3);
    QCOMPARE(secondTabBar->currentIndex(), 2);
}

void TestDisplayUi::new_tab_menu_action_targets_focused_window_when_native_menu_uses_last_window_action()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->statusBarVisible = true;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow firstWindow;
    firstWindow.resize(800, 500);
    firstWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&firstWindow));

    MainWindow secondWindow;
    secondWindow.resize(800, 500);
    secondWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&secondWindow));

    ResultDisplay* firstDisplay = firstWindow.findChild<ResultDisplay*>();
    ResultDisplay* secondDisplay = secondWindow.findChild<ResultDisplay*>();
    QVERIFY(firstDisplay != nullptr);
    QVERIFY(secondDisplay != nullptr);
    Editor* firstEditor = editorForDisplay(firstDisplay);
    Editor* secondEditor = editorForDisplay(secondDisplay);
    QVERIFY(firstEditor != nullptr);
    QVERIFY(secondEditor != nullptr);
    QTabBar* firstTabBar = tabBarForDisplay(firstDisplay);
    QTabBar* secondTabBar = tabBarForDisplay(secondDisplay);
    QVERIFY(firstTabBar != nullptr);
    QVERIFY(secondTabBar != nullptr);
    QCOMPARE(firstTabBar->count(), 1);
    QCOMPARE(secondTabBar->count(), 1);

    QTest::mouseClick(secondDisplay->viewport(), Qt::LeftButton, Qt::NoModifier,
                      secondDisplay->viewport()->rect().center());
    QCoreApplication::processEvents();

    QEvent secondWindowDeactivate(QEvent::WindowDeactivate);
    QCoreApplication::sendEvent(&secondWindow, &secondWindowDeactivate);
    QEvent firstWindowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&firstWindow, &firstWindowActivate);
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&firstWindow, "showNewSessionDialog", Qt::DirectConnection));
    QCoreApplication::processEvents();

    QTRY_COMPARE(firstTabBar->count(), 2);
    QCOMPARE(secondTabBar->count(), 1);

    firstWindow.raise();
    firstWindow.activateWindow();
    firstEditor->setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(([firstEditor]() {
        QWidget* focused = QApplication::focusWidget();
        return focused == firstEditor
            || (focused != nullptr && focused->parentWidget() == firstEditor);
    }()));

    QMenu* secondSessionMenu = menuWithTitle(secondWindow.menuBar(), QStringLiteral("&Session"));
    QVERIFY(secondSessionMenu != nullptr);
    QAction* secondWindowNewTabAction =
        directMenuActionWithText(secondSessionMenu, QStringLiteral("New &Tab"));
    QVERIFY(secondWindowNewTabAction != nullptr);
    secondWindowNewTabAction->trigger();
    QCoreApplication::processEvents();

    QTRY_COMPARE(firstTabBar->count(), 3);
    QCOMPARE(secondTabBar->count(), 1);
}

void TestDisplayUi::view_dock_menu_tracks_and_changes_only_active_window()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->statusBarVisible = true;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow firstWindow;
    firstWindow.resize(800, 500);
    firstWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&firstWindow));

    MainWindow secondWindow;
    secondWindow.resize(800, 500);
    secondWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&secondWindow));

    struct DockSpec {
        const char* setter;
        const char* objectName;
        const char* actionText;
        bool hasFocusArgument;
    };
    const DockSpec dockSpecs[] = {
        { "setFormulaBookDockVisible", "BookDock", "Formula &Book", true },
        { "setConstantsDockVisible", "ConstantsDock", "&Constants", true },
        { "setFunctionsDockVisible", "FunctionsDock", "&Functions", true },
        { "setVariablesDockVisible", "VariablesDock", "User &Variables", true },
        { "setUserFunctionsDockVisible", "UserFunctionsDock", "Use&r Functions", true },
        { "setUserUnitsDockVisible", "UserUnitsDock", "User &Units", true },
        { "setHistoryDockVisible", "HistoryDock", "&History", true },
        { "setBitfieldVisible", "BitfieldDock", "Bitfield", false }
    };
    const auto setDockVisible = [](MainWindow* window, const DockSpec& spec, bool visible) {
        if (!spec.hasFocusArgument) {
            return QMetaObject::invokeMethod(window, spec.setter, Qt::DirectConnection,
                                             Q_ARG(bool, visible));
        }
        return QMetaObject::invokeMethod(window, spec.setter, Qt::DirectConnection,
                                         Q_ARG(bool, visible), Q_ARG(bool, false));
    };
    const auto dockIsVisible = [](MainWindow* window, const DockSpec& spec) {
        QDockWidget* dock = window->findChild<QDockWidget*>(QString::fromLatin1(spec.objectName));
        return dock != nullptr && dock->isVisible();
    };

    QMenu* firstViewMenu = menuWithTitle(firstWindow.menuBar(), QStringLiteral("&View"));
    QMenu* secondViewMenu = menuWithTitle(secondWindow.menuBar(), QStringLiteral("&View"));
    QVERIFY(firstViewMenu != nullptr);
    QVERIFY(secondViewMenu != nullptr);

    for (const DockSpec& spec : dockSpecs) {
        QVERIFY(setDockVisible(&firstWindow, spec, true));
        QVERIFY(dockIsVisible(&firstWindow, spec));
        QVERIFY(!dockIsVisible(&secondWindow, spec));
    }

    firstWindow.raise();
    firstWindow.activateWindow();
    firstWindow.setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&firstWindow));
    QEvent firstWindowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&firstWindow, &firstWindowActivate);
    QCoreApplication::processEvents();

    for (const DockSpec& spec : dockSpecs) {
        QAction* firstAction = directMenuActionWithText(
            firstViewMenu, QString::fromLatin1(spec.actionText));
        QAction* secondAction = directMenuActionWithText(
            secondViewMenu, QString::fromLatin1(spec.actionText));
        QVERIFY(firstAction != nullptr);
        QVERIFY(secondAction != nullptr);
        QVERIFY(firstAction->isChecked());
        QVERIFY(secondAction->isChecked());

        // Simulate the native menu dispatching the inactive window's action.
        secondAction->trigger();
        QCoreApplication::processEvents();
        QVERIFY(!dockIsVisible(&firstWindow, spec));
        QVERIFY(!dockIsVisible(&secondWindow, spec));
        QVERIFY(!firstAction->isChecked());
        QVERIFY(!secondAction->isChecked());
    }

    for (const DockSpec& spec : dockSpecs) {
        QVERIFY(setDockVisible(&secondWindow, spec, true));
        QVERIFY(!dockIsVisible(&firstWindow, spec));
        QVERIFY(dockIsVisible(&secondWindow, spec));
    }

    secondWindow.raise();
    secondWindow.activateWindow();
    secondWindow.setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&secondWindow));
    QEvent secondWindowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&secondWindow, &secondWindowActivate);
    QCoreApplication::processEvents();

    for (const DockSpec& spec : dockSpecs) {
        QAction* firstAction = directMenuActionWithText(
            firstViewMenu, QString::fromLatin1(spec.actionText));
        QAction* secondAction = directMenuActionWithText(
            secondViewMenu, QString::fromLatin1(spec.actionText));
        QVERIFY(firstAction->isChecked());
        QVERIFY(secondAction->isChecked());

        firstAction->trigger();
        QCoreApplication::processEvents();
        QVERIFY(!dockIsVisible(&firstWindow, spec));
        QVERIFY(!dockIsVisible(&secondWindow, spec));
        QVERIFY(!firstAction->isChecked());
        QVERIFY(!secondAction->isChecked());
    }
}

void TestDisplayUi::keypad_zoom_round_trip_restores_button_sizes_data()
{
    QTest::addColumn<int>("mode");
    QTest::newRow("scientific-wide") << int(Settings::KeypadModeScientificWide);
    QTest::newRow("scientific-narrow") << int(Settings::KeypadModeScientificNarrow);
    QTest::newRow("basic") << int(Settings::KeypadModeBasicWide);
    QTest::newRow("custom") << int(Settings::KeypadModeCustom);
}

void TestDisplayUi::keypad_zoom_round_trip_restores_button_sizes()
{
    QFETCH(int, mode);
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    const auto oldCustomKeypad = settings->customKeypad;
    const auto restoreCustomKeypad = qScopeGuard([settings, oldCustomKeypad]() {
        settings->customKeypad = oldCustomKeypad;
    });
    settings->customKeypad.rows = 1;
    settings->customKeypad.columns = 1;
    settings->customKeypad.buttons.clear();
    Settings::CustomKeypadButton customButton;
    customButton.label = QStringLiteral("7");
    customButton.text = QStringLiteral("7");
    customButton.row = 0;
    customButton.column = 0;
    customButton.action = Settings::CustomKeypadActionInsertText;
    settings->customKeypad.buttons.append(customButton);
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->keypadMode = static_cast<Settings::KeypadMode>(mode);
    settings->keypadVisible = true;
    settings->keypadZoomPercent = 100;
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();
    const auto button = [&window]() {
        return keypadButtonWithText(window.findChild<Keypad*>(), QStringLiteral("7"));
    };
    QVERIFY(button() != nullptr);
    const QSize originalSize = button()->size();
    const QFont originalFont = button()->font();
    const QSize originalKeypadSize = window.findChild<Keypad*>()->size();

    for (int zoom : {150, 200, 150}) {
        QAction zoomAction;
        zoomAction.setData(zoom);
        QVERIFY(QMetaObject::invokeMethod(&window, "setKeypadZoom", Qt::DirectConnection,
                                          Q_ARG(QAction*, &zoomAction)));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        QVERIFY(button() != nullptr);
        QVERIFY(button()->width() > originalSize.width());
        QVERIFY(button()->font().pointSizeF() > originalFont.pointSizeF());

        zoomAction.setData(100);
        QVERIFY(QMetaObject::invokeMethod(&window, "setKeypadZoom", Qt::DirectConnection,
                                          Q_ARG(QAction*, &zoomAction)));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        QVERIFY(button() != nullptr);
        QCOMPARE(button()->font(), originalFont);
        QCOMPARE(button()->size(), originalSize);
        QCOMPARE(window.findChild<Keypad*>()->size(), originalKeypadSize);
    }
}

void TestDisplayUi::keypad_fifty_percent_zoom_scales_and_restores_data()
{
    QTest::addColumn<int>("mode");
    QTest::newRow("scientific-wide") << int(Settings::KeypadModeScientificWide);
    QTest::newRow("scientific-narrow") << int(Settings::KeypadModeScientificNarrow);
    QTest::newRow("basic") << int(Settings::KeypadModeBasicWide);
    QTest::newRow("custom") << int(Settings::KeypadModeCustom);
}

void TestDisplayUi::keypad_fifty_percent_zoom_scales_and_restores()
{
    QFETCH(int, mode);
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->keypadMode = static_cast<Settings::KeypadMode>(mode);
    settings->keypadVisible = true;
    settings->keypadZoomPercent = 100;
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&window));
    const auto button = [&window]() {
        return keypadButtonWithText(window.findChild<Keypad*>(), QStringLiteral("7"));
    };
    QVERIFY(button() != nullptr);
    const QSize originalSize = button()->size();
    const QFont originalFont = button()->font();
    QMenu* viewMenu = menuWithTitle(window.menuBar(), QStringLiteral("&View"));
    QVERIFY(viewMenu != nullptr);
    QMenu* keypadMenu = directSubmenuWithTitle(viewMenu, QStringLiteral("&Keypad"));
    QVERIFY(keypadMenu != nullptr);
    QMenu* zoomMenu = directSubmenuWithTitle(keypadMenu, QStringLiteral("&Zoom"));
    QVERIFY(zoomMenu != nullptr);
    const QList<QAction*> zoomActions = zoomMenu->actions();
    QCOMPARE(zoomActions.size(), 4);
    QCOMPARE(zoomActions.at(0)->text(), QStringLiteral("50%"));
    QCOMPARE(zoomActions.at(0)->data().toInt(), 50);
    QCOMPARE(zoomActions.at(1)->data().toInt(), 100);
    QCOMPARE(zoomActions.at(2)->data().toInt(), 150);
    QCOMPARE(zoomActions.at(3)->data().toInt(), 200);
    const auto flushEvents = []() {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    };

    zoomActions.at(0)->trigger();
    flushEvents();
    QVERIFY(button() != nullptr);
    const QSize halfSize = button()->size();
    const QFont halfFont = button()->font();
    QVERIFY(halfSize.width() < originalSize.width());
    QVERIFY(halfSize.height() < originalSize.height());
    QCOMPARE(halfFont.pointSizeF(), originalFont.pointSizeF() * 0.5);
    QVERIFY(zoomActions.at(0)->isChecked());
    QVERIFY(!zoomActions.at(1)->isChecked());
    QCOMPARE(settings->keypadZoomPercent, 50);

    zoomActions.at(2)->trigger();
    flushEvents();
    zoomActions.at(0)->trigger();
    flushEvents();
    QVERIFY(button() != nullptr);
    QCOMPARE(button()->size(), halfSize);
    QCOMPARE(button()->font(), halfFont);
    zoomActions.at(1)->trigger();
    flushEvents();
    QVERIFY(button() != nullptr);
    QCOMPARE(button()->size(), originalSize);
    QCOMPARE(button()->font(), originalFont);

    QVERIFY(QMetaObject::invokeMethod(&window, "restoreWindowKeypadZoom",
                                      Qt::DirectConnection, Q_ARG(int, 50)));
    flushEvents();
    QVERIFY(button() != nullptr);
    QCOMPARE(button()->size(), halfSize);
    QCOMPARE(button()->font(), halfFont);
    window.persistSessionAndSettingsForShutdown();
    const QJsonArray savedWindows = QJsonDocument::fromJson(settings->sessionLayoutJson.toUtf8())
                                       .object().value(QStringLiteral("windows")).toArray();
    QCOMPARE(savedWindows.size(), 1);
    QCOMPARE(savedWindows.first().toObject().value(QStringLiteral("keypadZoomPercent")).toInt(), 50);
}

void TestDisplayUi::keypad_fifty_percent_zoom_survives_settings_reload()
{
    const auto resetSettings = qScopeGuard([]() { UiTestFixture::resetSettings(); });
    UiTestFixture::resetSettings();
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->keypadZoomPercent = 50;
    settings->keypadMode = Settings::KeypadModeBasicWide;
    settings->save();
    settings->keypadZoomPercent = 100;
    settings->load();
    QCOMPARE(settings->keypadZoomPercent, 50);

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QMenu* viewMenu = menuWithTitle(window.menuBar(), QStringLiteral("&View"));
    QVERIFY(viewMenu != nullptr);
    QMenu* keypadMenu = directSubmenuWithTitle(viewMenu, QStringLiteral("&Keypad"));
    QVERIFY(keypadMenu != nullptr);
    QMenu* zoomMenu = directSubmenuWithTitle(keypadMenu, QStringLiteral("&Zoom"));
    QVERIFY(zoomMenu != nullptr);
    QCOMPARE(zoomMenu->actions().size(), 4);
    QVERIFY(zoomMenu->actions().first()->isChecked());
    QCOMPARE(zoomMenu->actions().first()->data().toInt(), 50);
}

void TestDisplayUi::keypad_view_menu_tracks_and_changes_only_active_window()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->keypadZoomPercent = 100;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->statusBarVisible = true;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow firstWindow;
    firstWindow.resize(800, 500);
    firstWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&firstWindow));

    MainWindow secondWindow;
    secondWindow.resize(800, 500);
    secondWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&secondWindow));

    QVERIFY(QMetaObject::invokeMethod(
        &firstWindow, "restoreWindowKeypadLayout", Qt::DirectConnection,
        Q_ARG(bool, true), Q_ARG(int, static_cast<int>(Settings::KeypadModeBasicWide))));
    QVERIFY(QMetaObject::invokeMethod(&firstWindow,
                                      "restoreWindowKeypadZoom",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 150)));
    QVERIFY(firstWindow.findChild<Keypad*>() != nullptr);
    QVERIFY(secondWindow.findChild<Keypad*>() == nullptr);

    QMenu* firstViewMenu = menuWithTitle(firstWindow.menuBar(), QStringLiteral("&View"));
    QMenu* secondViewMenu = menuWithTitle(secondWindow.menuBar(), QStringLiteral("&View"));
    QVERIFY(firstViewMenu != nullptr);
    QVERIFY(secondViewMenu != nullptr);
    QMenu* firstKeypadMenu = directSubmenuWithTitle(firstViewMenu, QStringLiteral("&Keypad"));
    QMenu* secondKeypadMenu = directSubmenuWithTitle(secondViewMenu, QStringLiteral("&Keypad"));
    QVERIFY(firstKeypadMenu != nullptr);
    QVERIFY(secondKeypadMenu != nullptr);
    QMenu* firstZoomMenu = directSubmenuWithTitle(firstKeypadMenu, QStringLiteral("&Zoom"));
    QMenu* secondZoomMenu = directSubmenuWithTitle(secondKeypadMenu, QStringLiteral("&Zoom"));
    QVERIFY(firstZoomMenu != nullptr);
    QVERIFY(secondZoomMenu != nullptr);
    const auto actionForMode = [](QMenu* menu, Settings::KeypadMode mode) {
        for (QAction* action : menu->actions()) {
            if (action->data().isValid()
                && action->data().toInt() == static_cast<int>(mode)) {
                return action;
            }
        }
        return static_cast<QAction*>(nullptr);
    };
    const auto checkedModeCount = [](QMenu* menu) {
        int count = 0;
        for (QAction* action : menu->actions()) {
            if (action->isCheckable() && action->data().isValid() && action->isChecked())
                ++count;
        }
        return count;
    };
    const auto actionForZoom = [](QMenu* menu, int zoomPercent) {
        for (QAction* action : menu->actions()) {
            if (action->data().isValid() && action->data().toInt() == zoomPercent)
                return action;
        }
        return static_cast<QAction*>(nullptr);
    };
    const auto checkedZoomCount = [](QMenu* menu) {
        int count = 0;
        for (QAction* action : menu->actions()) {
            if (action->isCheckable() && action->isChecked())
                ++count;
        }
        return count;
    };

    firstWindow.raise();
    firstWindow.activateWindow();
    firstWindow.setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&firstWindow));
    QEvent firstWindowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&firstWindow, &firstWindowActivate);
    QCoreApplication::processEvents();

    QAction* firstBasicAction = actionForMode(firstKeypadMenu, Settings::KeypadModeBasicWide);
    QAction* secondBasicAction = actionForMode(secondKeypadMenu, Settings::KeypadModeBasicWide);
    QAction* firstDisableAction = actionForMode(firstKeypadMenu, Settings::KeypadModeDisabled);
    QAction* secondDisableAction = actionForMode(secondKeypadMenu, Settings::KeypadModeDisabled);
    QVERIFY(firstBasicAction != nullptr);
    QVERIFY(secondBasicAction != nullptr);
    QVERIFY(firstDisableAction != nullptr);
    QVERIFY(secondDisableAction != nullptr);
    QVERIFY(firstBasicAction->isChecked());
    QVERIFY(secondBasicAction->isChecked());
    QVERIFY(!firstDisableAction->isChecked());
    QVERIFY(!secondDisableAction->isChecked());
    QCOMPARE(checkedModeCount(firstKeypadMenu), 1);
    QCOMPARE(checkedModeCount(secondKeypadMenu), 1);
    QVERIFY(firstZoomMenu->menuAction()->isEnabled());
    QVERIFY(secondZoomMenu->menuAction()->isEnabled());
    QVERIFY(actionForZoom(firstZoomMenu, 150)->isChecked());
    QVERIFY(actionForZoom(secondZoomMenu, 150)->isChecked());
    QCOMPARE(checkedZoomCount(firstZoomMenu), 1);
    QCOMPARE(checkedZoomCount(secondZoomMenu), 1);

    secondWindow.raise();
    secondWindow.activateWindow();
    secondWindow.setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&secondWindow));
    QEvent secondWindowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&secondWindow, &secondWindowActivate);
    QCoreApplication::processEvents();

    QVERIFY(!firstZoomMenu->menuAction()->isEnabled());
    QVERIFY(!secondZoomMenu->menuAction()->isEnabled());
    QVERIFY(actionForZoom(firstZoomMenu, 100)->isChecked());
    QVERIFY(actionForZoom(secondZoomMenu, 100)->isChecked());
    QCOMPARE(checkedZoomCount(firstZoomMenu), 1);
    QCOMPARE(checkedZoomCount(secondZoomMenu), 1);

    // Simulate the native menu dispatching the inactive window's actions.
    firstBasicAction->trigger();
    QCoreApplication::processEvents();
    QVERIFY(firstWindow.findChild<Keypad*>() != nullptr);
    QVERIFY(secondWindow.findChild<Keypad*>() != nullptr);
    QVERIFY(firstZoomMenu->menuAction()->isEnabled());
    QVERIFY(secondZoomMenu->menuAction()->isEnabled());

    QAction* firstZoom200Action = actionForZoom(firstZoomMenu, 200);
    QVERIFY(firstZoom200Action != nullptr);
    firstZoom200Action->trigger();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    QVERIFY(actionForZoom(firstZoomMenu, 200)->isChecked());
    QVERIFY(actionForZoom(secondZoomMenu, 200)->isChecked());
    QCOMPARE(checkedZoomCount(firstZoomMenu), 1);
    QCOMPARE(checkedZoomCount(secondZoomMenu), 1);

    firstWindow.raise();
    firstWindow.activateWindow();
    firstWindow.setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&firstWindow));
    QCoreApplication::sendEvent(&firstWindow, &firstWindowActivate);
    QCoreApplication::processEvents();

    QVERIFY(actionForZoom(firstZoomMenu, 150)->isChecked());
    QVERIFY(actionForZoom(secondZoomMenu, 150)->isChecked());
    QCOMPARE(checkedZoomCount(firstZoomMenu), 1);
    QCOMPARE(checkedZoomCount(secondZoomMenu), 1);

    secondDisableAction->trigger();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    QVERIFY(firstWindow.findChild<Keypad*>() == nullptr);
    QVERIFY(secondWindow.findChild<Keypad*>() != nullptr);
    QAction* firstDisabledAction = actionForMode(firstKeypadMenu, Settings::KeypadModeDisabled);
    QAction* secondDisabledAction = actionForMode(secondKeypadMenu, Settings::KeypadModeDisabled);
    QVERIFY(firstDisabledAction != nullptr);
    QVERIFY(secondDisabledAction != nullptr);
    QVERIFY(firstDisabledAction->isChecked());
    QVERIFY(secondDisabledAction->isChecked());
    QVERIFY(!firstBasicAction->isChecked());
    QVERIFY(!secondBasicAction->isChecked());
    QCOMPARE(checkedModeCount(firstKeypadMenu), 1);
    QCOMPARE(checkedModeCount(secondKeypadMenu), 1);
    QVERIFY(!firstZoomMenu->menuAction()->isEnabled());
    QVERIFY(!secondZoomMenu->menuAction()->isEnabled());
    QVERIFY(actionForZoom(firstZoomMenu, 150)->isChecked());
    QVERIFY(actionForZoom(secondZoomMenu, 150)->isChecked());
    QCOMPARE(checkedZoomCount(firstZoomMenu), 1);
    QCOMPARE(checkedZoomCount(secondZoomMenu), 1);
}

void TestDisplayUi::status_bar_menu_tracks_and_changes_only_active_window()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->statusBarVisible = true;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow firstWindow;
    firstWindow.resize(800, 500);
    firstWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&firstWindow));

    MainWindow secondWindow;
    secondWindow.resize(800, 500);
    secondWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&secondWindow));

    const auto statusBarIsVisible = [](const MainWindow& window) {
        const QStatusBar* bar =
            window.findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly);
        return bar != nullptr && bar->isVisible();
    };
    QVERIFY(statusBarIsVisible(firstWindow));
    QVERIFY(statusBarIsVisible(secondWindow));

    QVERIFY(QMetaObject::invokeMethod(&secondWindow,
                                      "setStatusBarVisible",
                                      Qt::DirectConnection,
                                      Q_ARG(bool, false)));
    QVERIFY(statusBarIsVisible(firstWindow));
    QVERIFY(!statusBarIsVisible(secondWindow));

    QMenu* firstViewMenu = menuWithTitle(firstWindow.menuBar(), QStringLiteral("&View"));
    QMenu* secondViewMenu = menuWithTitle(secondWindow.menuBar(), QStringLiteral("&View"));
    QVERIFY(firstViewMenu != nullptr);
    QVERIFY(secondViewMenu != nullptr);
    QAction* firstStatusBarAction =
        directMenuActionWithText(firstViewMenu, QStringLiteral("&Status Bar"));
    QAction* secondStatusBarAction =
        directMenuActionWithText(secondViewMenu, QStringLiteral("&Status Bar"));
    QVERIFY(firstStatusBarAction != nullptr);
    QVERIFY(secondStatusBarAction != nullptr);

    firstWindow.raise();
    firstWindow.activateWindow();
    firstWindow.setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&firstWindow));
    QEvent firstWindowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&firstWindow, &firstWindowActivate);
    QCoreApplication::processEvents();

    QVERIFY(firstStatusBarAction->isChecked());
    QVERIFY(secondStatusBarAction->isChecked());
    QVERIFY(statusBarIsVisible(firstWindow));
    QVERIFY(!statusBarIsVisible(secondWindow));

    // Simulate a native menu dispatch through the inactive window's QAction.
    secondStatusBarAction->trigger();
    QCoreApplication::processEvents();
    QVERIFY(!statusBarIsVisible(firstWindow));
    QVERIFY(!statusBarIsVisible(secondWindow));
    QVERIFY(!firstStatusBarAction->isChecked());
    QVERIFY(!secondStatusBarAction->isChecked());

    secondWindow.raise();
    secondWindow.activateWindow();
    secondWindow.setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&secondWindow));
    QEvent secondWindowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&secondWindow, &secondWindowActivate);
    QCoreApplication::processEvents();

    firstStatusBarAction->trigger();
    QCoreApplication::processEvents();
    QVERIFY(!statusBarIsVisible(firstWindow));
    QStatusBar* recreatedSecondStatusBar =
        secondWindow.findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly);
    QVERIFY(recreatedSecondStatusBar != nullptr);
    QVERIFY(recreatedSecondStatusBar->isVisible());
    QVERIFY(firstStatusBarAction->isChecked());
    QVERIFY(secondStatusBarAction->isChecked());

    firstWindow.raise();
    firstWindow.activateWindow();
    firstWindow.setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&firstWindow));
    QCoreApplication::sendEvent(&firstWindow, &firstWindowActivate);
    QCoreApplication::processEvents();

    QVERIFY(!firstStatusBarAction->isChecked());
    QVERIFY(!secondStatusBarAction->isChecked());
    QVERIFY(!statusBarIsVisible(firstWindow));
    QVERIFY(statusBarIsVisible(secondWindow));
}

void TestDisplayUi::precision_menu_editor_uses_themed_colors_data()
{
    QTest::addColumn<bool>("smallFont");
    QTest::newRow("default-font") << false;
    QTest::newRow("small-antialiased-font") << true;
}

void TestDisplayUi::precision_menu_editor_uses_themed_colors()
{
    QFETCH(bool, smallFont);
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->menuAppearance = Settings::MenuAppearanceSpeedCrunch;

    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{
        {QStringLiteral("background"), QStringLiteral("#300a24")}
    });
    settings->statusBarVisible = true;
    settings->resultPrecision = 8;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QString failure;
    QColor menuTextColor;
    QColor labelTextColor;
    QColor spinBackgroundColor;
    QColor spinTextColor;
    QColor spinArrowColor;
    QImage spinImage;
    qreal spinImageDevicePixelRatio = 1.0;
    QRect spinEditRect;
    QTimer::singleShot(0, &window, [&]() {
        QMenu* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu == nullptr) {
            failure = QStringLiteral("Precision popup menu was not found.");
            return;
        }

        QLabel* precisionLabel = nullptr;
        for (QLabel* label : menu->findChildren<QLabel*>()) {
            if (label->text() == QStringLiteral("Decimal places:")) {
                precisionLabel = label;
                break;
            }
        }
        if (precisionLabel == nullptr) {
            failure = QStringLiteral("Precision editor label was not found.");
            menu->close();
            return;
        }
        QSpinBox* precisionSpin = menu->findChild<QSpinBox*>();
        if (precisionSpin == nullptr) {
            failure = QStringLiteral("Precision editor spin box was not found.");
            menu->close();
            return;
        }

        if (smallFont) {
            QFont font = precisionSpin->font();
            font.setPixelSize(8);
            font.setWeight(QFont::Thin);
            font.setStyleStrategy(QFont::PreferAntialias);
            precisionSpin->setFont(font);
        }

        menuTextColor = menu->palette().color(QPalette::WindowText);
        labelTextColor = precisionLabel->palette().color(QPalette::WindowText);
        spinBackgroundColor = precisionSpin->palette().color(QPalette::Base);
        spinTextColor = precisionSpin->palette().color(QPalette::Text);
        spinArrowColor = precisionSpin->palette().color(QPalette::ButtonText);

        QStyleOptionSpinBox option;
        option.initFrom(precisionSpin);
        option.subControls = QStyle::SC_All;
        spinEditRect = precisionSpin->style()->subControlRect(QStyle::CC_SpinBox,
                                                              &option,
                                                              QStyle::SC_SpinBoxEditField,
                                                              precisionSpin);
        const QPixmap spinPixmap = precisionSpin->grab();
        spinImageDevicePixelRatio = spinPixmap.devicePixelRatio();
        spinImage = spinPixmap.toImage();
        menu->close();
    });

    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "showPrecisionContextMenu",
                                      Qt::DirectConnection,
                                      Q_ARG(QPoint, QPoint())));
    QVERIFY2(failure.isEmpty(), qPrintable(failure));
    QVERIFY(menuTextColor.isValid());
    QCOMPARE(labelTextColor.name(), menuTextColor.name());

    const QColor base(QStringLiteral("#300a24"));
    const QVector<QColor> shades = generateOklchShades(base, 6, ThemePolarity::Dark);
    const QVector<QColor> foregrounds = aaForegroundsForBackgrounds(shades);
    QCOMPARE(spinBackgroundColor.name(),
             shades.at(UiConfig::DockUnfocusedSelectedItemShade).name());
    QCOMPARE(spinTextColor.name(),
             foregrounds.at(UiConfig::DockUnfocusedSelectedItemShade).name());
    QCOMPARE(spinArrowColor.name(),
             foregrounds.at(UiConfig::DockUnfocusedSelectedItemShade).name());

    const auto imageRect = [spinImageDevicePixelRatio](const QRect& logicalRect) {
        return QRect(qRound(logicalRect.x() * spinImageDevicePixelRatio),
                     qRound(logicalRect.y() * spinImageDevicePixelRatio),
                     qRound(logicalRect.width() * spinImageDevicePixelRatio),
                     qRound(logicalRect.height() * spinImageDevicePixelRatio));
    };
    const QColor expectedBackground =
        shades.at(UiConfig::DockUnfocusedSelectedItemShade);
    const QColor expectedForeground =
        foregrounds.at(UiConfig::DockUnfocusedSelectedItemShade);
    QVERIFY(firstPixelMatchingColor(spinImage,
                                    imageRect(spinEditRect),
                                    expectedBackground,
                                    4) != QPoint(-1, -1));
    QVERIFY(firstPixelMatchingColorBlend(spinImage,
                                        imageRect(spinEditRect),
                                        expectedForeground,
                                        expectedBackground,
                                        8) != QPoint(-1, -1));
    const int arrowColumnWidth = qRound(18 * spinImageDevicePixelRatio);
    const QRect arrowColumn(spinImage.width() - arrowColumnWidth,
                            0,
                            arrowColumnWidth,
                            spinImage.height());
    const int arrowInset = qRound(3 * spinImageDevicePixelRatio);
    const QRect arrowInterior = arrowColumn.adjusted(arrowInset,
                                                     arrowInset,
                                                     -arrowInset,
                                                     -arrowInset);
    const QRect upArrowImageRect(arrowInterior.x(),
                                 arrowInterior.y(),
                                 arrowInterior.width(),
                                 arrowInterior.height() / 2);
    const QRect downArrowImageRect(arrowInterior.x(),
                                   arrowInterior.center().y() + 1,
                                   arrowInterior.width(),
                                   arrowInterior.height() - arrowInterior.height() / 2);
    QVERIFY(firstPixelDistinctFromColor(spinImage,
                                        upArrowImageRect,
                                        expectedBackground,
                                        48) != QPoint(-1, -1));
    QVERIFY(firstPixelDistinctFromColor(spinImage,
                                        downArrowImageRect,
                                        expectedBackground,
                                        48) != QPoint(-1, -1));
}

void TestDisplayUi::status_bar_visibility_persists_for_every_window_during_shutdown()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->statusBarVisible = true;
    settings->angleUnit = 'd';
    settings->resultFormat = 'g';
    settings->resultPrecision = -1;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    const auto statusBarIsVisible = [](const MainWindow* window) {
        const QStatusBar* bar = window != nullptr
            ? window->findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly)
            : nullptr;
        return bar != nullptr && bar->isVisible();
    };
    const auto selectorText = [](const MainWindow* window, const QString& labelText) {
        if (window == nullptr)
            return QString();
        for (QLabel* label : window->findChildren<QLabel*>()) {
            if (label->text() != labelText)
                continue;
            QPushButton* button = label->parentWidget()->findChild<QPushButton*>(
                QString(), Qt::FindDirectChildrenOnly);
            return button != nullptr ? button->text() : QString();
        }
        return QString();
    };
    {
        MainWindow firstWindow;
        firstWindow.resize(800, 500);
        firstWindow.show();
        QVERIFY(QTest::qWaitForWindowExposed(&firstWindow));

        MainWindow secondWindow(false);
        secondWindow.resize(800, 500);
        secondWindow.show();
        QVERIFY(QTest::qWaitForWindowExposed(&secondWindow));

        MainWindow thirdWindow(false);
        thirdWindow.resize(800, 500);
        thirdWindow.show();
        QVERIFY(QTest::qWaitForWindowExposed(&thirdWindow));

        QVERIFY(QMetaObject::invokeMethod(&secondWindow,
                                          "setStatusBarVisible",
                                          Qt::DirectConnection,
                                          Q_ARG(bool, false)));
        QVERIFY(statusBarIsVisible(&firstWindow));
        QVERIFY(!statusBarIsVisible(&secondWindow));
        QVERIFY(statusBarIsVisible(&thirdWindow));
        QVERIFY(settings->statusBarVisible);

        QVERIFY(QMetaObject::invokeMethod(&secondWindow,
                                          "setStatusBarVisible",
                                          Qt::DirectConnection,
                                          Q_ARG(bool, true)));
        QTRY_VERIFY(statusBarIsVisible(&secondWindow));

        secondWindow.raise();
        secondWindow.activateWindow();
        secondWindow.setFocus(Qt::OtherFocusReason);
        QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&secondWindow));
        QVERIFY(QMetaObject::invokeMethod(&secondWindow,
                                          "setAngleModeRadian",
                                          Qt::DirectConnection));
        QVERIFY(QMetaObject::invokeMethod(&secondWindow,
                                          "setResultFormatScientific",
                                          Qt::DirectConnection));
        QVERIFY(QMetaObject::invokeMethod(&secondWindow,
                                          "setResultPrecision3Digits",
                                          Qt::DirectConnection));
        QCOMPARE(selectorText(&firstWindow, QStringLiteral("Angle Mode:")),
                 QStringLiteral("Degree"));

        firstWindow.raise();
        firstWindow.activateWindow();
        firstWindow.setFocus(Qt::OtherFocusReason);
        QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&firstWindow));

        // During application teardown, secondary top-level windows can already
        // be hidden when the primary window persists the layout. Their status
        // bars are still configured as shown and must be saved that way.
        secondWindow.hide();
        thirdWindow.hide();
        firstWindow.persistSessionAndSettingsForShutdown();

        const QJsonDocument savedLayout =
            QJsonDocument::fromJson(settings->sessionLayoutJson.toUtf8());
        QVERIFY(savedLayout.isObject());
        const QJsonArray savedWindows =
            savedLayout.object().value(QStringLiteral("windows")).toArray();
        QCOMPARE(savedWindows.size(), 3);

        int visibleStatusBars = 0;
        int defaultStatusBarSelections = 0;
        int changedStatusBarSelections = 0;
        for (const QJsonValue& value : savedWindows) {
            const QJsonObject savedWindow = value.toObject();
            if (savedWindow.value(QStringLiteral("statusBarVisible")).toBool(false))
                ++visibleStatusBars;
            const QString angleUnit =
                savedWindow.value(QStringLiteral("statusBarAngleUnit")).toString();
            const QString resultFormat =
                savedWindow.value(QStringLiteral("statusBarResultFormat")).toString();
            const int resultPrecision =
                savedWindow.value(QStringLiteral("statusBarResultPrecision")).toInt();
            if (angleUnit == QLatin1String("d")
                && resultFormat == QLatin1String("g")
                && resultPrecision == -1) {
                ++defaultStatusBarSelections;
            } else if (angleUnit == QLatin1String("r")
                       && resultFormat == QLatin1String("e")
                       && resultPrecision == 3) {
                ++changedStatusBarSelections;
            }
        }
        QCOMPARE(visibleStatusBars, 3);
        QCOMPARE(defaultStatusBarSelections, 2);
        QCOMPARE(changedStatusBarSelections, 1);
    }

    QCOMPARE(QJsonDocument::fromJson(settings->sessionLayoutJson.toUtf8())
                 .object()
                 .value(QStringLiteral("windows"))
                 .toArray()
                 .size(),
             3);
}

void TestDisplayUi::status_bar_setting_selectors_update_only_active_window()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->statusBarVisible = true;
    settings->angleUnit = 'd';
    settings->resultFormat = 'g';
    settings->resultPrecision = -1;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow firstWindow;
    firstWindow.resize(900, 500);
    firstWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&firstWindow));

    MainWindow secondWindow;
    secondWindow.resize(900, 500);
    secondWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&secondWindow));

    const auto selectorButton = [](const MainWindow& window, const QString& labelText) {
        for (QLabel* label : window.findChildren<QLabel*>()) {
            if (label->text() == labelText) {
                return label->parentWidget()->findChild<QPushButton*>(
                    QString(), Qt::FindDirectChildrenOnly);
            }
        }
        return static_cast<QPushButton*>(nullptr);
    };
    QPushButton* firstAngle = selectorButton(firstWindow, QStringLiteral("Angle Mode:"));
    QPushButton* firstNotation = selectorButton(firstWindow, QStringLiteral("Notation:"));
    QPushButton* firstPrecision = selectorButton(firstWindow, QStringLiteral("Precision:"));
    QPushButton* secondAngle = selectorButton(secondWindow, QStringLiteral("Angle Mode:"));
    QPushButton* secondNotation = selectorButton(secondWindow, QStringLiteral("Notation:"));
    QPushButton* secondPrecision = selectorButton(secondWindow, QStringLiteral("Precision:"));
    QVERIFY(firstAngle != nullptr);
    QVERIFY(firstNotation != nullptr);
    QVERIFY(firstPrecision != nullptr);
    QVERIFY(secondAngle != nullptr);
    QVERIFY(secondNotation != nullptr);
    QVERIFY(secondPrecision != nullptr);
    QCOMPARE(firstAngle->text(), QStringLiteral("Degree"));
    QCOMPARE(firstNotation->text(), QStringLiteral("Automatic decimal"));
    QCOMPARE(firstPrecision->text(), QStringLiteral("Automatic"));
    QCOMPARE(secondAngle->text(), firstAngle->text());
    QCOMPARE(secondNotation->text(), firstNotation->text());
    QCOMPARE(secondPrecision->text(), firstPrecision->text());

    firstWindow.raise();
    firstWindow.activateWindow();
    firstWindow.setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE(QApplication::activeWindow(), static_cast<QWidget*>(&firstWindow));

    QMenu* secondSettingsMenu =
        menuWithTitle(secondWindow.menuBar(), QStringLiteral("Se&ttings"));
    QVERIFY(secondSettingsMenu != nullptr);
    QMenu* secondAngleMenu =
        directSubmenuWithTitle(secondSettingsMenu, QStringLiteral("&Angle Mode"));
    QVERIFY(secondAngleMenu != nullptr);
    QAction* staleRadianAction =
        directMenuActionWithText(secondAngleMenu, QStringLiteral("&Radian"));
    QVERIFY(staleRadianAction != nullptr);

    QMenu* secondResultsMenu =
        directSubmenuWithTitle(secondSettingsMenu, QStringLiteral("&Results"));
    QVERIFY(secondResultsMenu != nullptr);
    QAction* staleResultSlotsAction =
        directMenuActionWithText(secondResultsMenu,
                                 QStringLiteral("Notation && Precision..."));
    QVERIFY(staleResultSlotsAction != nullptr);

    // Simulate native-menu dispatch through actions owned by the inactive window.
    staleRadianAction->trigger();
    QCoreApplication::processEvents();
    QCOMPARE(firstAngle->text(), QStringLiteral("Radian"));
    QCOMPARE(secondAngle->text(), QStringLiteral("Degree"));

    bool configuredActiveWindowDialog = false;
    QTimer::singleShot(0, &firstWindow, [&]() {
        ResultSlotsDialog* dialog =
            qobject_cast<ResultSlotsDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr || dialog->parentWidget() != &firstWindow)
            return;

        const QList<QComboBox*> notationCombos = dialog->findChildren<QComboBox*>();
        const QList<QCheckBox*> checkBoxes = dialog->findChildren<QCheckBox*>();
        const QList<QSpinBox*> precisionSpins = dialog->findChildren<QSpinBox*>();
        QDialogButtonBox* buttons = dialog->findChild<QDialogButtonBox*>();
        if (notationCombos.isEmpty() || precisionSpins.isEmpty() || buttons == nullptr)
            return;

        QCheckBox* mainAutoPrecision = nullptr;
        for (QCheckBox* checkBox : checkBoxes) {
            if (checkBox->text() == QStringLiteral("Auto")) {
                mainAutoPrecision = checkBox;
                break;
            }
        }
        if (mainAutoPrecision == nullptr)
            return;

        const int scientificIndex =
            notationCombos.constFirst()->findData(QStringLiteral("e"));
        if (scientificIndex < 0)
            return;
        notationCombos.constFirst()->setCurrentIndex(scientificIndex);
        mainAutoPrecision->setChecked(false);
        precisionSpins.constFirst()->setValue(3);
        configuredActiveWindowDialog = true;
        buttons->button(QDialogButtonBox::Ok)->click();
    });
    staleResultSlotsAction->trigger();
    QVERIFY(configuredActiveWindowDialog);

    QCOMPARE(firstNotation->text(), QStringLiteral("Scientific decimal"));
    QCOMPARE(secondNotation->text(), QStringLiteral("Automatic decimal"));
    QCOMPARE(firstPrecision->text(), QStringLiteral("3"));
    QCOMPARE(secondPrecision->text(), QStringLiteral("Automatic"));
}

void TestDisplayUi::new_session_window_menu_action_copies_layout_with_single_fresh_session()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadMode = Settings::KeypadModeBasicWide;
    settings->keypadVisible = true;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = true;
    settings->statusBarVisible = true;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ResultDisplay* sourceDisplay = window.findChild<ResultDisplay*>();
    QVERIFY(sourceDisplay != nullptr);
    QVERIFY(sourceDisplay->session() != nullptr);
    const QString sourceSessionName = sourceDisplay->session()->name();
    Editor* sourceEditor = editorForDisplay(sourceDisplay);
    QVERIFY(sourceEditor != nullptr);
    sourceEditor->setText(QStringLiteral("2+2"));
    QVERIFY(QMetaObject::invokeMethod(&window, "evaluateEditorExpression", Qt::DirectConnection));
    QCOMPARE(sourceDisplay->session()->historySize(), 1);

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(window.findChildren<ResultDisplay*>().size(), 2);

    QMenu* sessionMenu = menuWithTitle(window.menuBar(), QStringLiteral("&Session"));
    QVERIFY(sessionMenu != nullptr);
    QAction* newWindowAction =
        directMenuActionWithText(sessionMenu, QStringLiteral("New &Window"));
    QVERIFY(newWindowAction != nullptr);
    QVERIFY(directMenuActionWithText(sessionMenu, QStringLiteral("New &Window with New Session")) == nullptr);

    const QList<MainWindow*> windowsBefore = topLevelMainWindows();
    QPointer<MainWindow> createdWindow;
    newWindowAction->trigger();
    QTRY_VERIFY(([&]() {
        for (MainWindow* candidate : topLevelMainWindows()) {
            if (!windowsBefore.contains(candidate) && candidate->isVisible()) {
                createdWindow = candidate;
                return true;
            }
        }
        return false;
    }()));

    QTest::qWait(300);
    QCoreApplication::processEvents();

    const QList<ResultDisplay*> createdDisplays = createdWindow->findChildren<ResultDisplay*>();
    QCOMPARE(createdDisplays.size(), 1);
    QDockWidget* constantsDock =
        createdWindow->findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QVERIFY(constantsDock->isVisible());
    QDockWidget* bitfieldDock =
        createdWindow->findChild<QDockWidget*>(QStringLiteral("BitfieldDock"));
    QVERIFY(bitfieldDock != nullptr);
    QVERIFY(bitfieldDock->isVisible());
    QStatusBar* statusBar =
        createdWindow->findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly);
    QVERIFY(statusBar != nullptr);
    QVERIFY(statusBar->isVisible());
    QVERIFY(createdWindow->findChild<Keypad*>() != nullptr);

    ResultDisplay* createdDisplay = createdDisplays.constFirst();
    const Session* session = createdDisplay->session();
    QVERIFY(session != nullptr);
    QCOMPARE(session->historySize(), 0);
    QVERIFY(session->name() != sourceSessionName);

    QTabBar* createdTabBar = tabBarForDisplay(createdDisplay);
    QVERIFY(createdTabBar != nullptr);
    QCOMPARE(createdTabBar->count(), 1);
    QCOMPARE(createdTabBar->tabText(0), session->name());

    createdWindow->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QTRY_VERIFY(createdWindow == nullptr);
}

void TestDisplayUi::session_open_menu_action_uses_open_dialog()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QMenu* sessionMenu = menuWithTitle(window.menuBar(), QStringLiteral("&Session"));
    QVERIFY(sessionMenu != nullptr);
    QAction* openAction = directMenuActionWithText(sessionMenu, QStringLiteral("&Open..."));
    QVERIFY(openAction != nullptr);
    const QList<QKeySequence> openKeyBindings = QKeySequence::keyBindings(QKeySequence::Open);
    QVERIFY(!openKeyBindings.isEmpty());
    QVERIFY(directMenuActionWithText(sessionMenu, QStringLiteral("&Load...")) == nullptr);

    bool shortcutOpenedDialog = false;
    QTimer::singleShot(0, &window, [&shortcutOpenedDialog]() {
        shortcutOpenedDialog = rejectActiveDialogWithTitle(QStringLiteral("Open Session"));
    });
    const QKeyCombination shortcut = openKeyBindings.constFirst()[0];
    QTest::keyClick(&window, shortcut.key(), shortcut.keyboardModifiers());
    QVERIFY(shortcutOpenedDialog);

    bool menuActionOpenedDialog = false;
    QTimer::singleShot(0, &window, [&menuActionOpenedDialog]() {
        menuActionOpenedDialog = rejectActiveDialogWithTitle(QStringLiteral("Open Session"));
    });
    openAction->trigger();
    QVERIFY(menuActionOpenedDialog);
}

void TestDisplayUi::user_definitions_menu_opens_working_dialog_data()
{
    QTest::addColumn<QString>("operation");
    QTest::newRow("cancel") << QStringLiteral("Cancel");
    QTest::newRow("validate") << QStringLiteral("Validate");
    QTest::newRow("apply") << QStringLiteral("Apply");
    QTest::newRow("ok") << QStringLiteral("OK");
}

void TestDisplayUi::user_definitions_menu_opens_working_dialog()
{
    QFETCH(QString, operation);
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->hasNumberFormatStyleSetting = true;

    const QString oldDefinitions = settings->startupUserDefinitions;
    const QString definitionsPath = QDir(Settings::getDataPath()).filePath(QStringLiteral("definitions.json"));
    QFile definitionsFile(definitionsPath);
    const bool hadDefinitionsFile = definitionsFile.exists();
    QByteArray oldFileContents;
    if (hadDefinitionsFile) {
        QVERIFY(definitionsFile.open(QIODevice::ReadOnly));
        oldFileContents = definitionsFile.readAll();
        definitionsFile.close();
    }
    const auto restoreDefinitions = qScopeGuard([&]() {
        settings->startupUserDefinitions = oldDefinitions;
        if (hadDefinitionsFile)
            writeFile(definitionsPath, oldFileContents);
        else
            QFile::remove(definitionsPath);
    });
    Q_UNUSED(restoreDefinitions);

    const QString original = QStringLiteral("dialogexisting=7");
    const QString edited = QStringLiteral("dialogvalue=42\ndialogfunction(x)=x+1\n[dialogunit]=[metre]\n1+1");
    settings->startupUserDefinitions = original;
    UserDefinitions::saveFrom(settings);
    QVERIFY2(QFileInfo::exists(definitionsPath), qPrintable(definitionsPath));

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(display != nullptr);
    const Evaluator* evaluator = display->session()->evaluator();
    QVERIFY(evaluator->hasVariable(QStringLiteral("dialogexisting")));

    QMenu* settingsMenu = menuWithTitle(window.menuBar(), QStringLiteral("Se&ttings"));
    QVERIFY(settingsMenu != nullptr);
    QMenu* symbolsMenu = directSubmenuWithTitle(settingsMenu, QStringLiteral("&Symbols"));
    QVERIFY(symbolsMenu != nullptr);
    QAction* action = directMenuActionWithText(symbolsMenu, QStringLiteral("User &Definitions..."));
    QVERIFY(action != nullptr);

    bool visitedDialog = false;
    bool visitedResults = false;
    QTimer::singleShot(0, &window, [&]() {
        QDialog* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        QVERIFY(dialog != nullptr);
        const auto closeDialog = qScopeGuard([dialog]() {
            if (dialog->isVisible())
                dialog->reject();
        });
        Q_UNUSED(closeDialog);
        QCOMPARE(dialog->windowTitle(), QStringLiteral("User Definitions"));
        QPlainTextEdit* editor = dialog->findChild<QPlainTextEdit*>(QStringLiteral("UserDefinitionsEditor"));
        QPlainTextEdit* lineNumbers = dialog->findChild<QPlainTextEdit*>(QStringLiteral("UserDefinitionsLineNumbers"));
        QDialogButtonBox* buttons = dialog->findChild<QDialogButtonBox*>();
        QVERIFY(editor != nullptr);
        QVERIFY(lineNumbers != nullptr);
        QVERIFY(buttons != nullptr);
        QCOMPARE(editor->toPlainText(), original);
        editor->setPlainText(edited);
        QCOMPARE(lineNumbers->toPlainText(), QStringLiteral("1\n2\n3\n4"));
        visitedDialog = true;

        if (operation == QLatin1String("Cancel")) {
            buttons->button(QDialogButtonBox::Cancel)->click();
            return;
        }
        if (operation == QLatin1String("OK")) {
            buttons->button(QDialogButtonBox::Ok)->click();
            return;
        }

        QPushButton* operationButton = nullptr;
        for (QAbstractButton* button : buttons->buttons()) {
            if (button->text() == operation)
                operationButton = qobject_cast<QPushButton*>(button);
        }
        QVERIFY(operationButton != nullptr);
        QTimer::singleShot(0, dialog, [&]() {
            QMessageBox* results = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(results != nullptr);
            const auto closeResults = qScopeGuard([results]() { results->accept(); });
            Q_UNUSED(closeResults);
            QVERIFY(results->text().contains(QStringLiteral("Imported variables: 1")));
            QVERIFY(results->text().contains(QStringLiteral("Imported functions: 1")));
            QVERIFY(results->text().contains(QStringLiteral("Imported units: 1")));
            QVERIFY(results->text().contains(QStringLiteral("Line numbers with errors: 4")));
            visitedResults = true;
        });
        operationButton->click();
        QVERIFY(dialog->isVisible());
        const bool applied = operation == QLatin1String("Apply");
        QCOMPARE(evaluator->hasVariable(QStringLiteral("dialogvalue")), applied);
        QCOMPARE(settings->startupUserDefinitions, applied ? edited : original);
        buttons->button(applied ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)->click();
    });
    action->trigger();
    QVERIFY(visitedDialog);
    QCOMPARE(visitedResults, operation == QLatin1String("Validate") || operation == QLatin1String("Apply"));

    const bool saved = operation == QLatin1String("OK") || operation == QLatin1String("Apply");
    QCOMPARE(evaluator->hasVariable(QStringLiteral("dialogvalue")), saved);
    QCOMPARE(evaluator->hasUserFunction(QStringLiteral("dialogfunction")), saved);
    QCOMPARE(evaluator->hasUserUnit(QStringLiteral("dialogunit")), saved);
    QCOMPARE(evaluator->hasVariable(QStringLiteral("dialogexisting")), !saved);
    QCOMPARE(settings->startupUserDefinitions, saved ? edited : original);
    UserDefinitions::loadInto(settings);
    QCOMPARE(settings->startupUserDefinitions, saved ? edited : original);
}

void TestDisplayUi::user_definitions_hint_follows_theme_contrast_data()
{
    QTest::addColumn<QColor>("background");
    QTest::addColumn<QColor>("changedBackground");
    QTest::newRow("dark-to-light") << QColor(QStringLiteral("#222134")) << QColor(QStringLiteral("#f7f4e8"));
    QTest::newRow("light-to-dark") << QColor(QStringLiteral("#f7f4e8")) << QColor(QStringLiteral("#222134"));
}

void TestDisplayUi::user_definitions_hint_follows_theme_contrast()
{
    QFETCH(QColor, background);
    QFETCH(QColor, changedBackground);
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->hasNumberFormatStyleSetting = true;
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(QJsonObject{
        {QStringLiteral("background"), background.name()}
    });

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    bool visitedDialog = false;
    QTimer::singleShot(0, &window, [&]() {
        QDialog* dialog = window.findChild<QDialog*>(QStringLiteral("UserDefinitionsDialog"));
        QVERIFY(dialog != nullptr);
        const auto closeDialog = qScopeGuard([dialog]() { dialog->reject(); });
        Q_UNUSED(closeDialog);
        QPlainTextEdit* editor = dialog->findChild<QPlainTextEdit*>(QStringLiteral("UserDefinitionsEditor"));
        QVERIFY(editor != nullptr);
        editor->clear();
        QVERIFY(!editor->placeholderText().isEmpty());

        const auto verifyHint = [editor](const QColor& expectedBackground) {
            const auto luminance = [](const QColor& color) {
                const auto linear = [](double channel) {
                    return channel <= 0.04045 ? channel / 12.92
                        : std::pow((channel + 0.055) / 1.055, 2.4);
                };
                return 0.2126 * linear(color.redF())
                    + 0.7152 * linear(color.greenF())
                    + 0.0722 * linear(color.blueF());
            };
            QCOMPARE(editor->palette().color(QPalette::Active, QPalette::Base), expectedBackground);
            for (const QPalette::ColorGroup group : {QPalette::Active,
                                                    QPalette::Inactive,
                                                    QPalette::Disabled}) {
                const QColor base = editor->palette().color(group, QPalette::Base);
                const QColor hint = editor->palette().color(group, QPalette::PlaceholderText);
                const double baseLuminance = luminance(base);
                const double hintLuminance = luminance(hint);
                const double contrast = (qMax(baseLuminance, hintLuminance) + 0.05)
                    / (qMin(baseLuminance, hintLuminance) + 0.05);
                QVERIFY2(contrast >= 7.0, qPrintable(QStringLiteral("Hint contrast is %1:1").arg(contrast)));
                QCOMPARE(hint.alpha(), 255);
                QCOMPARE(editor->viewport()->palette().color(group, QPalette::PlaceholderText), hint);
            }
        };
        verifyHint(background);
        settings->customColorSchemeJson = themeJsonString(QJsonObject{
            {QStringLiteral("background"), changedBackground.name()}
        });
        QVERIFY(QMetaObject::invokeMethod(&window, "colorSchemeChanged", Qt::DirectConnection));
        verifyHint(changedBackground);
        visitedDialog = true;
    });
    QVERIFY(QMetaObject::invokeMethod(&window, "showUserDefinitionsImportDialog", Qt::DirectConnection));
    QVERIFY(visitedDialog);
}

void TestDisplayUi::clipboard_actions_follow_active_pane_data()
{
    QTest::addColumn<bool>("split");
    QTest::addColumn<bool>("restore");
    QTest::addColumn<bool>("shortcut");
    for (const bool shortcut : {false, true}) {
        const QByteArray method = shortcut ? "shortcut" : "menu";
        QTest::newRow(QByteArray(method + "-fresh").constData()) << false << false << shortcut;
        QTest::newRow(QByteArray(method + "-split").constData()) << true << false << shortcut;
        QTest::newRow(QByteArray(method + "-restored").constData()) << false << true << shortcut;
        QTest::newRow(QByteArray(method + "-restored-split").constData()) << true << true << shortcut;
    }
}

void TestDisplayUi::clipboard_actions_follow_active_pane()
{
    QFETCH(bool, split);
    QFETCH(bool, restore);
    QFETCH(bool, shortcut);
    MainWindowStateGuard guard;
    guard.settings->sessionLayoutJson.clear();
    guard.settings->windowState.clear();
    guard.settings->windowGeometry.clear();
    guard.settings->hasNumberFormatStyleSetting = true;
    const QString oldClipboard = QApplication::clipboard()->text();
    const auto restoreClipboard = qScopeGuard([oldClipboard]() {
        QApplication::clipboard()->setText(oldClipboard);
    });

    if (restore) {
        MainWindow source;
        if (split)
            QVERIFY(QMetaObject::invokeMethod(&source, "splitActivePaneRight", Qt::DirectConnection));
        source.persistSessionAndSettingsForShutdown();
        QVERIFY(!guard.settings->sessionLayoutJson.isEmpty());
    }

    MainWindow window;
    window.show();
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    if (restore) {
        QTRY_COMPARE(window.findChildren<Editor*>().size(), split ? 2 : 1);
        QTRY_VERIFY(window.findChildren<QTabBar*>().first()->count() > 0);
        // Restoration replaces the initial editor asynchronously, even for one pane.
        QTest::qWait(400);
    } else if (split) {
        QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
        QTRY_COMPARE(window.findChildren<Editor*>().size(), 2);
    }
    const QList<Editor*> editors = window.findChildren<Editor*>();
    QMenu* editMenu = menuWithTitle(window.menuBar(), QStringLiteral("&Edit"));
    QVERIFY(editMenu != nullptr);
    QAction* copyAction = directMenuActionWithText(editMenu, QStringLiteral("&Copy"));
    QAction* pasteAction = directMenuActionWithText(editMenu, QStringLiteral("&Paste"));
    QVERIFY(copyAction != nullptr);
    QVERIFY(pasteAction != nullptr);

    for (Editor* editor : editors) {
        QTest::mouseClick(editor->viewport(), Qt::LeftButton);
        QTRY_VERIFY(editor->hasFocus());
        editor->setText(QStringLiteral("123456"));
        QTextCursor cursor = editor->textCursor();
        cursor.setPosition(2);
        cursor.setPosition(4, QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);
        QApplication::clipboard()->setText(QStringLiteral("unchanged"));
        if (shortcut) {
            const QKeyCombination key = QKeySequence(QKeySequence::Copy)[0];
            QTest::keyClick(editor, key.key(), key.keyboardModifiers());
        } else {
            copyAction->trigger();
        }
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("34"));
        QCOMPARE(editor->text(), QStringLiteral("123456"));

        QApplication::clipboard()->setText(QStringLiteral("78"));
        if (shortcut) {
            const QKeyCombination key = QKeySequence(QKeySequence::Paste)[0];
            // Exercise the menu shortcut path as well as the widget's own handler.
            QTest::keyClick(&window, key.key(), key.keyboardModifiers());
        } else {
            pasteAction->trigger();
        }
        QCOMPARE(editor->text(), QStringLiteral("127856"));
        editor->undo();
        QCOMPARE(editor->text(), QStringLiteral("123456"));
        editor->clear();
    }
}

void TestDisplayUi::copy_shortcut_preserves_display_selection_in_other_pane()
{
    MainWindowStateGuard guard;
    guard.settings->sessionLayoutJson.clear();
    guard.settings->windowState.clear();
    guard.settings->windowGeometry.clear();
    guard.settings->hasNumberFormatStyleSetting = true;
    MainWindow window;
    window.show();
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QTRY_COMPARE(window.findChildren<ResultDisplay*>().size(), 2);
    const QList<ResultDisplay*> displays = window.findChildren<ResultDisplay*>();
    for (ResultDisplay* display : displays) {
        display->setPlainText(QStringLiteral("123456"));
        QTextCursor cursor = display->textCursor();
        cursor.setPosition(2);
        cursor.setPosition(4, QTextCursor::KeepAnchor);
        display->setTextCursor(cursor);
        Editor* editor = display->parentWidget()->findChild<Editor*>();
        QVERIFY(editor != nullptr);
        QTRY_VERIFY(editor->hasFocus());
        QApplication::clipboard()->setText(QStringLiteral("unchanged"));
        const QKeyCombination key = QKeySequence(QKeySequence::Copy)[0];
        QTest::keyClick(editor, key.key(), key.keyboardModifiers());
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("34"));
    }
}

void TestDisplayUi::quit_shortcut_triggers_menu_action_from_editor_and_window()
{
    MainWindowStateGuard guard;
    guard.settings->sessionLayoutJson.clear();
    guard.settings->windowState.clear();
    guard.settings->windowGeometry.clear();
    guard.settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.show();
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QMenu* sessionMenu = menuWithTitle(window.menuBar(), QStringLiteral("&Session"));
    QVERIFY(sessionMenu != nullptr);
    QAction* quitAction = directMenuActionWithText(sessionMenu, QStringLiteral("&Quit"));
    QVERIFY(quitAction != nullptr);
    QCOMPARE(quitAction->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Q));
    for (QShortcut* shortcut : window.findChildren<QShortcut*>())
        QVERIFY(!shortcut->keys().contains(quitAction->shortcut()));

    // Observe shortcut dispatch without terminating the test application.
    QVERIFY(quitAction->disconnect(qApp));
    QSignalSpy triggered(quitAction, &QAction::triggered);

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    editor->setFocus();
    QTRY_VERIFY(editor->hasFocus());
    QTest::keyClick(editor, Qt::Key_Q, Qt::ControlModifier);
    QCOMPARE(triggered.count(), 1);

    window.setFocus();
    QTest::keyClick(&window, Qt::Key_Q, Qt::ControlModifier);
    QCOMPARE(triggered.count(), 2);
}

void TestDisplayUi::session_open_sessions_folder_menu_action_opens_session_storage()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QMenu* sessionMenu = menuWithTitle(window.menuBar(), QStringLiteral("&Session"));
    QVERIFY(sessionMenu != nullptr);
    QAction* openSessionsFolderAction =
        directMenuActionWithText(sessionMenu, QStringLiteral("Open Sessions &Folder"));
    QVERIFY(openSessionsFolderAction != nullptr);

    m_capturedUrl = QUrl();
    QDesktopServices::setUrlHandler(QStringLiteral("file"), this, "captureOpenedUrl");
    auto urlHandlerCleanup = qScopeGuard([]() {
        QDesktopServices::unsetUrlHandler(QStringLiteral("file"));
    });

    openSessionsFolderAction->trigger();

    QTRY_VERIFY(m_capturedUrl.isValid());
    QCOMPARE(m_capturedUrl.scheme(), QStringLiteral("file"));

    const QString expectedPath =
        QDir(Settings::getDataPath()).filePath(QStringLiteral("sessions"));
    QCOMPARE(QDir::cleanPath(m_capturedUrl.toLocalFile()), QDir::cleanPath(expectedPath));
    QVERIFY(QDir(expectedPath).exists());
}

void TestDisplayUi::session_import_dialog_opens_valid_json_as_new_tab()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->statusBarVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString importedName =
        QStringLiteral("Imported Session %1").arg(QUuid::createUuid().toString(QUuid::Id128));
    const QString importFilePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("imported.json"));
    writeFile(importFilePath, QJsonDocument(sessionJson(importedName)).toJson(QJsonDocument::Compact));
    const QString savedImportPath = QDir(QDir(Settings::getDataPath()).filePath(QStringLiteral("sessions")))
        .filePath(importedName + QStringLiteral(".json"));
    auto savedImportCleanup = qScopeGuard([savedImportPath]() {
        QFile::remove(savedImportPath);
    });

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(display != nullptr);
    QTabBar* tabBar = tabBarForDisplay(display);
    QVERIFY(tabBar != nullptr);
    QCOMPARE(tabBar->count(), 1);

    QMenu* sessionMenu = menuWithTitle(window.menuBar(), QStringLiteral("&Session"));
    QVERIFY(sessionMenu != nullptr);
    QAction* importAction = directMenuActionWithText(sessionMenu, QStringLiteral("&Import..."));
    QVERIFY(importAction != nullptr);

    bool sawImportDialog = false;
    QTimer::singleShot(0, &window, [&sawImportDialog, &importFilePath]() {
        QFileDialog* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr)
            return;

        sawImportDialog = dialog->windowTitle() == QStringLiteral("Import Session")
            && dialog->acceptMode() == QFileDialog::AcceptOpen
            && dialog->fileMode() == QFileDialog::ExistingFile
            && dialog->defaultSuffix() == QStringLiteral("json");
        dialog->selectFile(importFilePath);
        static_cast<QDialog*>(dialog)->accept();
    });
    importAction->trigger();
    QVERIFY(sawImportDialog);

    QTRY_COMPARE(tabBar->count(), 2);
    QCOMPARE(tabBar->tabText(tabBar->currentIndex()), importedName);
    QVERIFY(display->session() != nullptr);
    QCOMPARE(display->session()->name(), importedName);
    QCOMPARE(display->session()->historySize(), 1);
    QCOMPARE(display->session()->historyEntryAtRef(0).expr(), QStringLiteral("6*7"));
}

void TestDisplayUi::session_import_rejects_invalid_json_without_new_tab()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->statusBarVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString importFilePath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("not-session.json"));
    writeFile(importFilePath, QByteArray("1+1\n"));

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(display != nullptr);
    QTabBar* tabBar = tabBarForDisplay(display);
    QVERIFY(tabBar != nullptr);
    QCOMPARE(tabBar->count(), 1);

    QMenu* sessionMenu = menuWithTitle(window.menuBar(), QStringLiteral("&Session"));
    QVERIFY(sessionMenu != nullptr);
    QAction* importAction = directMenuActionWithText(sessionMenu, QStringLiteral("&Import..."));
    QVERIFY(importAction != nullptr);

    bool sawImportDialog = false;
    QTimer::singleShot(0, &window, [&sawImportDialog, &importFilePath, &window]() {
        QFileDialog* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr)
            return;

        sawImportDialog = dialog->windowTitle() == QStringLiteral("Import Session")
            && dialog->defaultSuffix() == QStringLiteral("json");
        dialog->selectFile(importFilePath);
        const auto acceptMessageBox = []() {
            QMessageBox* messageBox = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (messageBox != nullptr)
                messageBox->accept();
        };
        QTimer::singleShot(20, &window, acceptMessageBox);
        QTimer::singleShot(100, &window, acceptMessageBox);
        static_cast<QDialog*>(dialog)->accept();
    });
    importAction->trigger();
    QVERIFY(sawImportDialog);
    QCOMPARE(tabBar->count(), 1);
}

void TestDisplayUi::session_export_menu_offers_json_without_save_action()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QMenu* sessionMenu = menuWithTitle(window.menuBar(), QStringLiteral("&Session"));
    QVERIFY(sessionMenu != nullptr);
    QVERIFY(directMenuActionWithText(sessionMenu, QStringLiteral("&Save...")) == nullptr);

    QMenu* exportMenu = directSubmenuWithTitle(sessionMenu, QStringLiteral("&Export"));
    QVERIFY(exportMenu != nullptr);
    QVERIFY(!exportMenu->actions().isEmpty());
    QCOMPARE(exportMenu->actions().constFirst()->text(), QStringLiteral("JSON"));
    QAction* jsonAction = directMenuActionWithText(exportMenu, QStringLiteral("JSON"));
    QVERIFY(jsonAction != nullptr);

    bool sawJsonDialog = false;
    QTimer::singleShot(0, &window, [&sawJsonDialog]() {
        QFileDialog* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr)
            return;

        const QStringList files = dialog->selectedFiles();
        const QString selectedFile = files.isEmpty() ? QString() : files.constFirst();
        const QFileInfo fileInfo(selectedFile);
        sawJsonDialog = dialog->windowTitle() == QStringLiteral("Export session as JSON")
            && dialog->defaultSuffix() == QStringLiteral("json")
            && fileInfo.suffix() == QStringLiteral("json")
            && QDir::cleanPath(fileInfo.absolutePath()) == QDir::cleanPath(QDir::homePath());
        dialog->reject();
    });
    jsonAction->trigger();
    QVERIFY(sawJsonDialog);
}

void TestDisplayUi::session_export_dialog_prefills_session_name_data()
{
    QTest::addColumn<QString>("exportSlot");
    QTest::addColumn<QString>("extension");
    QTest::addColumn<QString>("sessionName");
    QTest::addColumn<QString>("baseName");

    for (const auto& format : {qMakePair(QStringLiteral("exportJson"), QStringLiteral("json")),
                               qMakePair(QStringLiteral("exportHtml"), QStringLiteral("html")),
                               qMakePair(QStringLiteral("exportPlainText"), QStringLiteral("txt"))}) {
        QTest::newRow(qPrintable(format.second + QStringLiteral("-named")))
            << format.first << format.second << QStringLiteral("Project notes") << QStringLiteral("Project notes");
        QTest::newRow(qPrintable(format.second + QStringLiteral("-sanitized")))
            << format.first << format.second << QStringLiteral("Budget/2026: Q4") << QStringLiteral("Budget_2026_ Q4");
    }
}

void TestDisplayUi::session_export_dialog_prefills_session_name()
{
    QFETCH(QString, exportSlot);
    QFETCH(QString, extension);
    QFETCH(QString, sessionName);
    QFETCH(QString, baseName);

    MainWindowStateGuard guard;
    MainWindow window(false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(display != nullptr);
    Session* session = const_cast<Session*>(display->session());
    QVERIFY(session != nullptr);
    const QString originalName = session->name();
    const auto restoreName = qScopeGuard([session, originalName]() { session->setName(originalName); });
    session->setName(sessionName);

    bool sawDialog = false;
    QString suggestedFileName;
    QString defaultSuffix;
    QString selectedName;
    QFileDialog::AcceptMode acceptMode = QFileDialog::AcceptOpen;
    QTimer::singleShot(0, &window, [&]() {
        QFileDialog* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr)
            return;

        // Let the export dialog's filename-selection timer run first.
        QTimer::singleShot(0, dialog, [&, dialog]() {
            sawDialog = true;
            const QStringList files = dialog->selectedFiles();
            if (!files.isEmpty())
                suggestedFileName = QFileInfo(files.constFirst()).fileName();
            defaultSuffix = dialog->defaultSuffix();
            acceptMode = dialog->acceptMode();
            if (QLineEdit* fileNameEdit = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
                selectedName = fileNameEdit->selectedText();
            dialog->reject();
        });
    });
    QVERIFY(QMetaObject::invokeMethod(&window, qPrintable(exportSlot), Qt::DirectConnection));
    QVERIFY(sawDialog);
    QCOMPARE(suggestedFileName, baseName + QLatin1Char('.') + extension);
    QCOMPARE(defaultSuffix, extension);
    QCOMPARE(acceptMode, QFileDialog::AcceptSave);
    QCOMPARE(selectedName, baseName);
}

void TestDisplayUi::restore_closed_tab_shortcut_restores_last_closed_session_tab()
{
    Settings* appSettings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;
        QString oldSessionLayoutJson;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldWindowPositionSave;
        bool oldHasNumberFormatStyleSetting;

        ~SettingsGuard()
        {
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->windowPositionSave = oldWindowPositionSave;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
        }
    } guard {
        appSettings,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        appSettings->sessionLayoutJson,
        appSettings->constantsDockVisible,
        appSettings->functionsDockVisible,
        appSettings->historyDockVisible,
        appSettings->keypadVisible,
        appSettings->formulaBookDockVisible,
        appSettings->variablesDockVisible,
        appSettings->userFunctionsDockVisible,
        appSettings->userUnitsDockVisible,
        appSettings->bitfieldVisible,
        appSettings->windowPositionSave,
        appSettings->hasNumberFormatStyleSetting
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    appSettings->sessionLayoutJson.clear();
    appSettings->constantsDockVisible = false;
    appSettings->functionsDockVisible = false;
    appSettings->historyDockVisible = false;
    appSettings->keypadVisible = false;
    appSettings->formulaBookDockVisible = false;
    appSettings->variablesDockVisible = false;
    appSettings->userFunctionsDockVisible = false;
    appSettings->userUnitsDockVisible = false;
    appSettings->bitfieldVisible = false;
    appSettings->windowPositionSave = false;
    appSettings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(display != nullptr);
    QWidget* pane = paneWidgetForDisplay(display);
    QTabBar* tabBar = pane ? pane->findChild<QTabBar*>() : nullptr;
    QVERIFY(tabBar != nullptr);
    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);

    const QKeyCombination shortcut = restoreClosedTabShortcut();
    QTest::keyClick(&window, shortcut.key(), shortcut.keyboardModifiers());
    QCoreApplication::processEvents();
    QCOMPARE(tabBar->count(), 1);

    editor->setText(QStringLiteral("first tab draft"));
    editor->setCursorPosition(editor->text().size());
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(tabBar->count(), 2);
    QCOMPARE(tabBar->currentIndex(), 1);

    editor->setText(QStringLiteral("second tab draft"));
    editor->setCursorPosition(editor->text().size());
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "closeCurrentSession", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(tabBar->count(), 1);
    QCOMPARE(editor->text(), QStringLiteral("first tab draft"));

    QTest::keyClick(&window, shortcut.key(), shortcut.keyboardModifiers());
    QCoreApplication::processEvents();
    QTRY_COMPARE(tabBar->count(), 2);
    QCOMPARE(tabBar->currentIndex(), 1);
    QCOMPARE(editor->text(), QStringLiteral("second tab draft"));

    QTest::keyClick(&window, shortcut.key(), shortcut.keyboardModifiers());
    QCoreApplication::processEvents();
    QCOMPARE(tabBar->count(), 2);
    QCOMPARE(tabBar->currentIndex(), 1);
    QCOMPARE(editor->text(), QStringLiteral("second tab draft"));
}

void TestDisplayUi::session_tabs_reorder_with_horizontal_drag()
{
    Settings* appSettings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;
        QString oldSessionLayoutJson;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldWindowPositionSave;
        bool oldHasNumberFormatStyleSetting;

        ~SettingsGuard()
        {
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->windowPositionSave = oldWindowPositionSave;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        appSettings,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        appSettings->sessionLayoutJson,
        appSettings->constantsDockVisible,
        appSettings->functionsDockVisible,
        appSettings->historyDockVisible,
        appSettings->keypadVisible,
        appSettings->formulaBookDockVisible,
        appSettings->variablesDockVisible,
        appSettings->userFunctionsDockVisible,
        appSettings->userUnitsDockVisible,
        appSettings->bitfieldVisible,
        appSettings->windowPositionSave,
        appSettings->hasNumberFormatStyleSetting
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");

    appSettings->sessionLayoutJson.clear();
    appSettings->constantsDockVisible = false;
    appSettings->functionsDockVisible = false;
    appSettings->historyDockVisible = false;
    appSettings->keypadVisible = false;
    appSettings->formulaBookDockVisible = false;
    appSettings->variablesDockVisible = false;
    appSettings->userFunctionsDockVisible = false;
    appSettings->userUnitsDockVisible = false;
    appSettings->bitfieldVisible = false;
    appSettings->windowPositionSave = false;
    appSettings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QCoreApplication::processEvents();

    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(display != nullptr);
    QWidget* pane = paneWidgetForDisplay(display);
    QTabBar* tabBar = pane ? pane->findChild<QTabBar*>() : nullptr;
    QVERIFY(tabBar != nullptr);
    QVERIFY(tabBar->isVisible());
    QCOMPARE(tabBar->count(), 3);

    const auto dragTab = [tabBar](int from, int to) {
        const QPoint start = tabBar->tabRect(from).center();
        const QPoint end = to == 0
            ? QPoint(tabBar->tabRect(0).left() + 1, tabBar->tabRect(0).center().y())
            : QPoint(tabBar->tabRect(to).right() - 2, tabBar->tabRect(to).center().y());

        sendTabDragMouseEvent(tabBar, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        const int step = qMax(1, qAbs(end.x() - start.x()) / 6);
        if (end.x() >= start.x()) {
            for (int x = start.x(); x <= end.x(); x += step)
                sendTabDragMouseEvent(tabBar, QEvent::MouseMove, QPoint(x, start.y()), Qt::NoButton, Qt::LeftButton);
        } else {
            for (int x = start.x(); x >= end.x(); x -= step)
                sendTabDragMouseEvent(tabBar, QEvent::MouseMove, QPoint(x, start.y()), Qt::NoButton, Qt::LeftButton);
        }
        sendTabDragMouseEvent(tabBar, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
        sendTabDragMouseEvent(tabBar, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
    };

    const QString firstTab = tabBar->tabText(0);
    dragTab(0, 2);
    QCoreApplication::processEvents();
    QCOMPARE(tabBar->tabText(tabBar->count() - 1), firstTab);

    dragTab(tabBar->count() - 1, 0);
    QCoreApplication::processEvents();
    QCOMPARE(tabBar->tabText(0), firstTab);
}

void TestDisplayUi::closing_and_reopening_docks_keeps_attached_widgets()
{
    constexpr int dockLayoutStateVersion = 1;
    Settings* appSettings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;
        QString oldSessionLayoutJson;
        QByteArray oldWindowState;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldHasNumberFormatStyleSetting;

        ~SettingsGuard()
        {
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->windowState = oldWindowState;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        appSettings,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        appSettings->sessionLayoutJson,
        appSettings->windowState,
        appSettings->constantsDockVisible,
        appSettings->functionsDockVisible,
        appSettings->historyDockVisible,
        appSettings->keypadVisible,
        appSettings->formulaBookDockVisible,
        appSettings->variablesDockVisible,
        appSettings->userFunctionsDockVisible,
        appSettings->userUnitsDockVisible,
        appSettings->bitfieldVisible,
        appSettings->hasNumberFormatStyleSetting
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    appSettings->sessionLayoutJson.clear();
    appSettings->windowState.clear();
    appSettings->constantsDockVisible = false;
    appSettings->functionsDockVisible = false;
    appSettings->historyDockVisible = false;
    appSettings->keypadVisible = false;
    appSettings->formulaBookDockVisible = false;
    appSettings->variablesDockVisible = false;
    appSettings->userFunctionsDockVisible = false;
    appSettings->userUnitsDockVisible = false;
    appSettings->bitfieldVisible = false;
    appSettings->hasNumberFormatStyleSetting = true;

    QByteArray legacyDockState;
    QByteArray populatedDockState;
    {
        MainWindow window;
        window.show();
        QCoreApplication::processEvents();

        struct DockSpec {
            const char* setter;
            const char* objectName;
            bool hasFocusArgument;
        };
        const DockSpec dockSpecs[] = {
            { "setBitfieldVisible", "BitfieldDock", false },
            { "setFormulaBookDockVisible", "BookDock", true },
            { "setConstantsDockVisible", "ConstantsDock", true },
            { "setFunctionsDockVisible", "FunctionsDock", true },
            { "setHistoryDockVisible", "HistoryDock", true },
            { "setVariablesDockVisible", "VariablesDock", true },
            { "setUserFunctionsDockVisible", "UserFunctionsDock", true },
            { "setUserUnitsDockVisible", "UserUnitsDock", true }
        };

        const auto invokeVisible = [&window](const DockSpec& spec, bool visible) {
            if (!spec.hasFocusArgument) {
                return QMetaObject::invokeMethod(&window, spec.setter, Qt::DirectConnection,
                                                 Q_ARG(bool, visible));
            }
            return QMetaObject::invokeMethod(&window, spec.setter, Qt::DirectConnection,
                                             Q_ARG(bool, visible), Q_ARG(bool, false));
        };

        const auto dockCount = [&window](const QString& objectName) {
            int count = 0;
            for (QDockWidget* dock : window.findChildren<QDockWidget*>()) {
                if (dock->objectName() == objectName)
                    ++count;
            }
            return count;
        };

        for (const DockSpec& spec : dockSpecs) {
            const QString objectName = QString::fromLatin1(spec.objectName);
            QPointer<QDockWidget> originalDock = window.findChild<QDockWidget*>(objectName);
            QVERIFY(originalDock != nullptr);
            QVERIFY(!originalDock->isVisible());
            QCOMPARE(dockCount(objectName), 1);

            QVERIFY(invokeVisible(spec, true));
            QCoreApplication::processEvents();

            originalDock->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCoreApplication::processEvents();

            QVERIFY(originalDock != nullptr);
            QVERIFY(!originalDock->isVisible());
            QVERIFY(window.dockWidgetArea(originalDock) != Qt::NoDockWidgetArea);
            QCOMPARE(dockCount(objectName), 1);

            const QByteArray hiddenState = window.saveState(dockLayoutStateVersion);
            QVERIFY(window.restoreState(hiddenState, dockLayoutStateVersion));
            QVERIFY(invokeVisible(spec, true));
            QCoreApplication::processEvents();
            QCOMPARE(window.findChild<QDockWidget*>(objectName), originalDock.data());
            QCOMPARE(dockCount(objectName), 1);
        }

        legacyDockState = window.saveState();
        populatedDockState = window.saveState(dockLayoutStateVersion);
    }

    appSettings->constantsDockVisible = false;
    appSettings->functionsDockVisible = false;
    appSettings->historyDockVisible = false;
    appSettings->formulaBookDockVisible = false;
    appSettings->variablesDockVisible = false;
    appSettings->userFunctionsDockVisible = false;
    appSettings->userUnitsDockVisible = false;
    appSettings->bitfieldVisible = false;

    {
        appSettings->windowState = legacyDockState;
        MainWindow legacyStateWindow;
        legacyStateWindow.show();
        QCoreApplication::processEvents();

        QDockWidget* bitfield = legacyStateWindow.findChild<QDockWidget*>(QStringLiteral("BitfieldDock"));
        QVERIFY(bitfield != nullptr);
        QVERIFY(!bitfield->isVisible());
    }

    {
        appSettings->windowState = populatedDockState;
        MainWindow restoredWindow;
        restoredWindow.show();
        QCoreApplication::processEvents();

        const char* dockNames[] = {
            "BitfieldDock", "BookDock", "ConstantsDock", "FunctionsDock",
            "HistoryDock", "VariablesDock", "UserFunctionsDock", "UserUnitsDock"
        };
        for (const char* dockName : dockNames)
            QCOMPARE(restoredWindow.findChildren<QDockWidget*>(QString::fromLatin1(dockName)).size(), 1);
        QDockWidget* bitfield = restoredWindow.findChild<QDockWidget*>(QStringLiteral("BitfieldDock"));
        QVERIFY(bitfield != nullptr);
        QVERIFY(bitfield->isVisible());
    }
}

int main(int argc, char** argv)
{
    return UiTestFixture::run<TestDisplayUi>(argc, argv);
}

#include "testdisplayui.moc"
