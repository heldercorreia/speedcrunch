// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

// Reads the title bar color that Windows actually uses. CTest runs this suite
// once per native style, because each style gives Qt a different palette.

#include "mainwindowtesthelpers.h"
#include "gui/numberformatdialog.h"

#include <QProxyStyle>
#include <QStyleHints>

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
};

void TestWindowFrameUi::initTestCase()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    QSKIP("Requires Qt 6.8 or newer");
#endif
    if (QGuiApplication::platformName() != QLatin1String("windows"))
        QSKIP("Requires the native Windows platform plugin");
    // A style that cannot load must fail here, not test the default style.
    const QString requestedStyle = qEnvironmentVariable("QT_STYLE_OVERRIDE");
    if (!requestedStyle.isEmpty())
        QVERIFY2(usesStyle(requestedStyle), qPrintable(requestedStyle));
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
    if (dialogDark && usesStyle(QStringLiteral("windowsvista")))
        QEXPECT_FAIL("", "The Vista style keeps dialog palettes light", Continue);
    QCOMPARE(*actualDialogDark, dialogDark);
}

int main(int argc, char** argv)
{
    return UiTestFixture::run<TestWindowFrameUi>(argc, argv);
}

#include "testwindowframeui.moc"
