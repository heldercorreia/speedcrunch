// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

// Reads the title bar color that Windows actually uses. CTest runs this suite
// once per native style, because each style gives Qt a different palette.

#include "mainwindowtesthelpers.h"
#include "gui/numberformatdialog.h"

#include <QHeaderView>
#include <QProxyStyle>
#include <QStyleHints>
#include <QTreeWidget>

#include <optional>

#include <windows.h>
#include <dwmapi.h>

using namespace MainWindowTestHelpers;

Q_DECLARE_METATYPE(Settings::SystemColorMode)

namespace {
// The Windows SDK names this DWMWA_USE_IMMERSIVE_DARK_MODE since 10.0.22000.
constexpr DWORD ImmersiveDarkModeAttribute = 20;

const QString DarkThemeBackground = QStringLiteral("#1f3229");
const QString LightThemeBackground = QStringLiteral("#f0e8d8");

std::optional<bool> hasDarkTitleBar(QWidget& window)
{
    BOOL dark = FALSE;
    const HRESULT result = DwmGetWindowAttribute(reinterpret_cast<HWND>(window.winId()),
                                                 ImmersiveDarkModeAttribute, &dark, sizeof(dark));
    if (FAILED(result))
        return std::nullopt;
    return dark != FALSE;
}

bool isDark(const QPalette& palette)
{
    return palette.color(QPalette::WindowText).lightness()
        > palette.color(QPalette::Window).lightness();
}

bool chooseColorMode(MainWindow& window, Settings::SystemColorMode mode)
{
    auto* menu = window.findChild<QMenu*>(QStringLiteral("SystemColorModeMenu"));
    if (menu == nullptr)
        return false;
    for (QAction* action : menu->actions()) {
        if (action->data().toInt() == mode) {
            action->trigger();
            return true;
        }
    }
    return false;
}

bool usesStyle(const QString& name)
{
    QStyle* style = QApplication::style();
    while (auto* proxy = qobject_cast<QProxyStyle*>(style))
        style = proxy->baseStyle();
    return style->objectName().compare(name, Qt::CaseInsensitive) == 0;
}
} // namespace

class TestWindowFrameUi : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init() { UiTestFixture::resetSettings(); }
    void cleanup();
    void title_bars_follow_color_mode_and_theme_data();
    void title_bars_follow_color_mode_and_theme();
    void open_dialogs_follow_color_mode_changes();
    void dark_mode_keeps_native_layout();
};

void TestWindowFrameUi::initTestCase()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    QSKIP("Requires Qt 6.8 or newer", );
#endif
    if (QGuiApplication::platformName() != QLatin1String("windows"))
        QSKIP("Requires the native Windows platform plugin", );
    // A style that cannot load must fail here, not test the default style.
    // Dark mode can replace the Vista style, so check it in light mode.
    const QString requestedStyle = qEnvironmentVariable("QT_STYLE_OVERRIDE");
    if (!requestedStyle.isEmpty()) {
        Settings::instance()->systemColorMode = Settings::SystemColorModeLight;
        MainWindow::initializeApplicationStyle();
        QVERIFY2(usesStyle(requestedStyle), qPrintable(requestedStyle));
    }
}

void TestWindowFrameUi::cleanup()
{
    Settings::instance()->systemColorMode = Settings::SystemColorModeFollowSystem;
    MainWindow::initializeApplicationStyle();
}

void TestWindowFrameUi::title_bars_follow_color_mode_and_theme_data()
{
    QTest::addColumn<Settings::SystemColorMode>("mode");
    QTest::addColumn<QString>("themeBackground");
    QTest::addColumn<bool>("windowsDark");
    QTest::addColumn<bool>("dialogDark");

    QTest::newRow("dark mode, dark theme")
        << Settings::SystemColorModeDark << DarkThemeBackground << true << true;
    QTest::newRow("dark mode, light theme")
        << Settings::SystemColorModeDark << LightThemeBackground << false << true;
    QTest::newRow("light mode, dark theme")
        << Settings::SystemColorModeLight << DarkThemeBackground << false << false;
}

void TestWindowFrameUi::title_bars_follow_color_mode_and_theme()
{
    QFETCH(Settings::SystemColorMode, mode);
    QFETCH(QString, themeBackground);
    QFETCH(bool, windowsDark);
    QFETCH(bool, dialogDark);

    MainWindowStateGuard guard;
    guard.settings->colorScheme = QStringLiteral("Custom");
    guard.settings->customColorSchemeJson = themeJsonString(
        QJsonObject {{QStringLiteral("background"), themeBackground}});
    guard.settings->systemColorMode = mode;
    // Set the mode before any window exists, as startup does. A later change
    // updates existing title bars, which would hide a window created too soon.
    MainWindow::initializeApplicationStyle();

    MainWindow first(false);
    MainWindow second(false);
    NumberFormatDialog dialog(&first);

    const std::optional<bool> firstDark = hasDarkTitleBar(first);
    QVERIFY(firstDark.has_value());
    QCOMPARE(*firstDark, windowsDark);

    const std::optional<bool> secondDark = hasDarkTitleBar(second);
    QVERIFY(secondDark.has_value());
    QCOMPARE(*secondDark, windowsDark);

    const std::optional<bool> actualDialogDark = hasDarkTitleBar(dialog);
    QVERIFY(actualDialogDark.has_value());
    QCOMPARE(*actualDialogDark, dialogDark);
    // The dialog contents match its title bar.
    QCOMPARE(isDark(dialog.palette()), dialogDark);
}

void TestWindowFrameUi::open_dialogs_follow_color_mode_changes()
{
    MainWindowStateGuard guard;
    guard.settings->colorScheme = QStringLiteral("Custom");
    guard.settings->customColorSchemeJson = themeJsonString(
        QJsonObject {{QStringLiteral("background"), DarkThemeBackground}});
    guard.settings->systemColorMode = Settings::SystemColorModeLight;
    MainWindow::initializeApplicationStyle();

    MainWindow window(false);
    NumberFormatDialog dialog(&window);
    QVERIFY(chooseColorMode(window, Settings::SystemColorModeLight));

    for (const bool dark : {true, false}) {
        QVERIFY(chooseColorMode(window, dark ? Settings::SystemColorModeDark
                                             : Settings::SystemColorModeLight));
        QTRY_COMPARE(isDark(dialog.palette()), dark);
        QTRY_COMPARE(hasDarkTitleBar(dialog), std::optional<bool>(dark));
        // The main window has a dark theme, so its title bar follows the mode.
        QTRY_COMPARE(hasDarkTitleBar(window), std::optional<bool>(dark));
    }
}

void TestWindowFrameUi::dark_mode_keeps_native_layout()
{
    MainWindowStateGuard guard;
    guard.settings->constantsDockVisible = true;
    guard.settings->historyDockVisible = true;
    guard.settings->systemColorMode = Settings::SystemColorModeLight;
    MainWindow::initializeApplicationStyle();

    MainWindow window(false);
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* dock = window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(dock);
    auto* tree = dock->findChild<QTreeWidget*>();
    QVERIFY(tree);
    QTRY_VERIFY(tree->topLevelItemCount() > 0);
    QMenu* sessionMenu = window.menuBar()->actions().value(0)->menu();
    QVERIFY(sessionMenu);

    // Fusion draws dark mode in place of the Vista style. Only the colors
    // should change, not the sizes or when shortcuts are underlined.
    const auto layout = [&]() {
        QStringList result;
        QMenuBar* bar = window.menuBar();
        QStyleOptionMenuItem barOption;
        barOption.initFrom(bar);
        result << QStringLiteral("underline %1").arg(
            bar->style()->styleHint(QStyle::SH_UnderlineShortcut, &barOption, bar));
        for (QAction* action : bar->actions()) {
            if (action->isVisible()) {
                const QRect rect = bar->actionGeometry(action);
                result << QStringLiteral("%1 %2,%3 %4x%5").arg(action->text()).arg(rect.x())
                              .arg(rect.y()).arg(rect.width()).arg(rect.height());
            }
        }
        const QSize menu = sessionMenu->sizeHint();
        result << QStringLiteral("menu %1x%2").arg(menu.width()).arg(menu.height());
        for (QDockWidget* shown : window.findChildren<QDockWidget*>()) {
            if (!shown->isVisible())
                continue;
            const QRect rect = shown->geometry();
            result << QStringLiteral("%1 %2,%3 %4x%5 contents %6").arg(shown->objectName()).arg(rect.x())
                          .arg(rect.y()).arg(rect.width()).arg(rect.height())
                          .arg(shown->widget() ? shown->widget()->height() : -1);
        }
        result << QStringLiteral("header %1").arg(tree->header()->sizeHint().height());
        result << QStringLiteral("row %1").arg(tree->visualItemRect(tree->topLevelItem(0)).height());
        return result;
    };
    const QStringList light = layout();
    QVERIFY(chooseColorMode(window, Settings::SystemColorModeDark));
    QTRY_COMPARE(QGuiApplication::styleHints()->colorScheme(), Qt::ColorScheme::Dark);
    QTRY_VERIFY(isDark(QApplication::palette()));
    QTRY_COMPARE(layout(), light);
}

int main(int argc, char** argv)
{
    return UiTestFixture::run<TestWindowFrameUi>(argc, argv);
}

#include "testwindowframeui.moc"
