// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mainwindowtesthelpers.h"

using namespace MainWindowTestHelpers;

class TestStartupFocusUi : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init() { UiTestFixture::resetSettings(); }
    void startup_focuses_editor_with_visible_dock_input_data();
    void startup_focuses_editor_with_visible_dock_input();
};

void TestStartupFocusUi::initTestCase()
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

void TestStartupFocusUi::startup_focuses_editor_with_visible_dock_input_data()
{
    QTest::addColumn<QString>("dockName");
    QTest::addColumn<QString>("startupMode");
    QTest::addColumn<bool>("privateTab");
    QTest::addColumn<bool>("backgroundStartup");
    if (QGuiApplication::platformName() == QLatin1String("cocoa")) {
        QTest::newRow("ConstantsDock-split-background")
            << QStringLiteral("ConstantsDock") << QStringLiteral("split") << false << true;
    }
    for (const QString& dock : {QStringLiteral("ConstantsDock"), QStringLiteral("FunctionsDock"),
                               QStringLiteral("VariablesDock"), QStringLiteral("UserFunctionsDock"),
                               QStringLiteral("UserUnitsDock")}) {
        for (const QString& mode : {QStringLiteral("fresh"), QStringLiteral("legacy"),
                                   QStringLiteral("tabs"), QStringLiteral("split"),
                                   QStringLiteral("new-window")}) {
            for (bool privateTab : {false, true}) {
                if (mode == QLatin1String("new-window") && privateTab)
                    continue;
                const QString row = QStringLiteral("%1-%2-%3")
                    .arg(dock, mode, privateTab ? QStringLiteral("private") : QStringLiteral("ordinary"));
                QTest::newRow(qPrintable(row)) << dock << mode << privateTab << false;
            }
        }
    }
}

void TestStartupFocusUi::startup_focuses_editor_with_visible_dock_input()
{
    QFETCH(QString, dockName);
    QFETCH(QString, startupMode);
    QFETCH(bool, privateTab);
    QFETCH(bool, backgroundStartup);
    MainWindowStateGuard guard;
    PrivateSessionStorage storage;
    const auto resetSettings = qScopeGuard([]() { UiTestFixture::resetSettings(); });
    Settings* settings = Settings::instance();
    settings->openPrivateTabOnStartup = privateTab;
    settings->constantsDockVisible = dockName == QLatin1String("ConstantsDock");
    settings->functionsDockVisible = dockName == QLatin1String("FunctionsDock");
    settings->variablesDockVisible = dockName == QLatin1String("VariablesDock");
    settings->userFunctionsDockVisible = dockName == QLatin1String("UserFunctionsDock");
    settings->userUnitsDockVisible = dockName == QLatin1String("UserUnitsDock");
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    const bool savedLayout = startupMode == QLatin1String("tabs")
        || startupMode == QLatin1String("split");
    const QString activeName = savedLayout ? QStringLiteral("Saved-Active") : QStringLiteral("Untitled-1");
    if (savedLayout || startupMode == QLatin1String("legacy")) {
        // Legacy history is migrated after the initial session name is chosen.
        const QString sessionPath = savedLayout
            ? storage.path(activeName)
            : QDir(Settings::getDataPath()).filePath(QStringLiteral("history.json"));
        QDir().mkpath(QFileInfo(sessionPath).path());
        writeFile(sessionPath, QJsonDocument(sessionJson(activeName)).toJson());
    }
    if (savedLayout) {
        QJsonObject savedWindow = startupSessionWindow(activeName, QStringLiteral("saved"));
        if (startupMode == QLatin1String("split")) {
            const QString firstName = QStringLiteral("Saved-First");
            writeFile(storage.path(firstName), QJsonDocument(sessionJson(firstName)).toJson());
            QJsonObject firstPane = startupSessionWindow(firstName, QString())
                .value(QStringLiteral("root")).toObject();
            QJsonObject activePane = savedWindow.value(QStringLiteral("root")).toObject();
            firstPane.insert(QStringLiteral("type"), QStringLiteral("pane"));
            activePane.insert(QStringLiteral("type"), QStringLiteral("pane"));
            savedWindow.insert(QStringLiteral("root"), QJsonObject {
                { QStringLiteral("type"), QStringLiteral("split") },
                { QStringLiteral("active"), activeName },
                { QStringLiteral("orientation"), QStringLiteral("horizontal") },
                { QStringLiteral("children"), QJsonArray { firstPane, activePane } }
            });
        }
        settings->sessionLayoutJson = QString::fromUtf8(QJsonDocument(QJsonObject {
            { QStringLiteral("scheme"), 1 },
            { QStringLiteral("kind"), QStringLiteral("session-layout") },
            { QStringLiteral("activeWindow"), QStringLiteral("saved") },
            { QStringLiteral("windows"), QJsonArray { savedWindow } }
        }).toJson(QJsonDocument::Compact));
    }

    MainWindow window(startupMode != QLatin1String("new-window"));
    if (backgroundStartup)
        window.setWindowFlag(Qt::WindowDoesNotAcceptFocus);
    window.showWhenReady();
    QTRY_VERIFY(window.isVisible());
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    if (backgroundStartup)
        QVERIFY(QApplication::activeWindow() != &window);
    QDockWidget* dock = window.findChild<QDockWidget*>(dockName);
    QVERIFY(dock != nullptr);
    QTRY_VERIFY(dock->isVisible());
    QLineEdit* search = dock->findChild<QLineEdit*>();
    QVERIFY(search != nullptr);

    // A window keeps its focus target even when another app takes focus.
    const auto focusedWidget = [&window]() {
        return QApplication::activeWindow() == &window
            ? QApplication::focusWidget() : window.focusWidget();
    };
    const auto focusedDisplay = [&window, &focusedWidget]() -> ResultDisplay* {
        const QWidget* focus = focusedWidget();
        if (focus == nullptr)
            return nullptr;
        for (ResultDisplay* display : window.findChildren<ResultDisplay*>()) {
            Editor* editor = paneWidgetForDisplay(display)->findChild<Editor*>();
            if (editor != nullptr && (editor == focus || editor->isAncestorOf(focus)))
                return display;
        }
        return nullptr;
    };
    QCOMPARE(window.findChildren<Editor*>().size(), startupMode == QLatin1String("split") ? 2 : 1);
    const auto startupEditorHasFocus = [&]() {
        const ResultDisplay* display = focusedDisplay();
        return display != nullptr && display->session()->isPrivate() == privateTab
            && (privateTab || display->session()->name() == activeName);
    };
    QTRY_VERIFY(startupEditorHasFocus());
    ResultDisplay* display = focusedDisplay();
    if (!privateTab && (savedLayout || startupMode == QLatin1String("legacy")))
        QCOMPARE(display->session()->historySize(), 1);
    if (privateTab && (savedLayout || startupMode == QLatin1String("legacy"))) {
        QTabBar* tabs = tabBarForDisplay(display);
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 2);
    }

    // Delayed window activation must keep the editor selected after restoration.
    QTest::qWait(400);
    QCOMPARE(focusedDisplay(), display);

    // Once startup finishes, choosing the dock search must still work normally.
    QTest::mouseClick(search, Qt::LeftButton);
    QTRY_COMPARE(focusedWidget(), static_cast<QWidget*>(search));
    // The dock input's delayed focus handling ends after 100 ms.
    QTest::qWait(150);
    QCOMPARE(focusedWidget(), static_cast<QWidget*>(search));
}

int main(int argc, char** argv)
{
    return UiTestFixture::run<TestStartupFocusUi>(argc, argv);
}

#include "teststartupfocusui.moc"
