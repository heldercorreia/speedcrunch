// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mainwindowtesthelpers.h"

using namespace MainWindowTestHelpers;

class TestSessionsUi : public QObject {
    Q_OBJECT

public slots:
    void captureOpenedUrl(const QUrl& url) { m_capturedUrl = url; }

private slots:
    void initTestCase();
    void init() { UiTestFixture::resetSettings(); }
    void saved_window_ui_state_overrides_defaults_before_show();
    void visible_window_applies_restored_dock_and_keypad_layout();
    void always_on_top_toggles_preserve_window_geometry();
    void session_tab_context_menu_opens_on_right_click();
    void persisting_layout_captures_visible_scroll_positions_for_all_panes();
    void switching_session_tabs_preserves_each_editor_text();
    void session_tabs_show_full_name_tooltip_on_hover();
    void session_tab_navigation_shortcuts_switch_tabs();
    void new_tab_menu_action_and_shortcut_create_session_in_active_pane();
    void new_tab_menu_action_targets_focused_window_when_native_menu_uses_last_window_action();
    void new_session_window_menu_action_copies_layout_with_single_fresh_session();
    void session_open_menu_action_uses_open_dialog();
    void session_open_sessions_folder_menu_action_opens_session_storage();
    void session_import_dialog_opens_valid_json_as_new_tab();
    void session_import_rejects_invalid_json_without_new_tab();
    void session_export_menu_offers_json_without_save_action();
    void session_export_dialog_prefills_session_name_data();
    void session_export_dialog_prefills_session_name();
    void restore_closed_tab_shortcut_restores_last_closed_session_tab();
    void session_tabs_reorder_with_horizontal_drag();

private:
    QUrl m_capturedUrl;
};

void TestSessionsUi::initTestCase()
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

void TestSessionsUi::always_on_top_toggles_preserve_window_geometry()
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

void TestSessionsUi::saved_window_ui_state_overrides_defaults_before_show()
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

void TestSessionsUi::visible_window_applies_restored_dock_and_keypad_layout()
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

void TestSessionsUi::persisting_layout_captures_visible_scroll_positions_for_all_panes()
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

void TestSessionsUi::switching_session_tabs_preserves_each_editor_text()
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

void TestSessionsUi::session_tabs_show_full_name_tooltip_on_hover()
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

void TestSessionsUi::session_tab_context_menu_opens_on_right_click()
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

void TestSessionsUi::session_tab_navigation_shortcuts_switch_tabs()
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

void TestSessionsUi::new_tab_menu_action_and_shortcut_create_session_in_active_pane()
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

void TestSessionsUi::new_tab_menu_action_targets_focused_window_when_native_menu_uses_last_window_action()
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

void TestSessionsUi::new_session_window_menu_action_copies_layout_with_single_fresh_session()
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

void TestSessionsUi::session_open_menu_action_uses_open_dialog()
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

void TestSessionsUi::session_open_sessions_folder_menu_action_opens_session_storage()
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

void TestSessionsUi::session_import_dialog_opens_valid_json_as_new_tab()
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

void TestSessionsUi::session_import_rejects_invalid_json_without_new_tab()
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

void TestSessionsUi::session_export_menu_offers_json_without_save_action()
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

void TestSessionsUi::session_export_dialog_prefills_session_name_data()
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

void TestSessionsUi::session_export_dialog_prefills_session_name()
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

void TestSessionsUi::restore_closed_tab_shortcut_restores_last_closed_session_tab()
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

void TestSessionsUi::session_tabs_reorder_with_horizontal_drag()
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

int main(int argc, char** argv)
{
    return UiTestFixture::run<TestSessionsUi>(argc, argv);
}

#include "testsessionsui.moc"
