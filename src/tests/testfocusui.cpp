// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mainwindowtesthelpers.h"

using namespace MainWindowTestHelpers;

class TestFocusUi : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init() { UiTestFixture::resetSettings(); }
    void main_window_applies_primary_role_to_active_editor_and_dock_selection();
    void current_result_tooltip_stays_hidden_after_escape_and_arrow_caret_move();
    void current_result_tooltip_stays_hidden_after_escape_and_mouse_caret_move();
    void current_result_tooltip_hides_when_dragging_splitters();
    void f6_cycles_focus_between_editor_and_visible_dock_controls_data();
    void f6_cycles_focus_between_editor_and_visible_dock_controls();
    void f6_cycles_focus_with_another_main_window_visible();
    void f6_cycles_focus_with_another_main_window_visible_data();
    void dock_search_focus_suppresses_editor_primary_outline_across_panes();
    void dock_selection_inserts_into_active_session_pane_after_focus_transfer();
    void clicking_tab_activates_own_pane_in_nested_split_layout();
    void active_pane_survives_window_reactivation_focus_replay();
    void extra_window_activation_restores_own_active_tab_indicator();
    void startup_focuses_editor_with_visible_dock_input_data();
    void startup_focuses_editor_with_visible_dock_input();
    void focused_dock_search_survives_window_reactivation_focus_replay();
    void focusing_loaded_pane_preserves_its_current_scroll_position();
    void clipboard_actions_follow_active_pane_data();
    void clipboard_actions_follow_active_pane();
    void copy_shortcut_preserves_display_selection_in_other_pane();
    void escape_clears_only_the_focused_editor_data();
    void escape_clears_only_the_focused_editor();
    void escape_dismisses_completion_and_preview_before_clearing();
    void escape_still_cancels_history_edit_data();
    void escape_still_cancels_history_edit();
};

void TestFocusUi::escape_clears_only_the_focused_editor_data()
{
    QTest::addColumn<bool>("enabled");
    QTest::newRow("default-off") << false;
    QTest::newRow("enabled") << true;
}

void TestFocusUi::escape_clears_only_the_focused_editor()
{
    QFETCH(bool, enabled);
    Settings::instance()->autoCalc = false;
    Settings::instance()->escapeClearsEditor = enabled;
    MainWindow first;
    MainWindow second;
    QVERIFY(QMetaObject::invokeMethod(&first, "splitActivePaneRight", Qt::DirectConnection));
    first.show();
    second.show();
    QVERIFY(QTest::qWaitForWindowExposed(&first));
    QVERIFY(QTest::qWaitForWindowExposed(&second));
    const QList<Editor*> editors = first.findChildren<Editor*>() + second.findChildren<Editor*>();
    QVERIFY(editors.size() >= 3);
    for (Editor* target : editors) {
        for (Editor* editor : editors) {
            editor->setAutoCompletionEnabled(false);
            editor->setText(QStringLiteral("123\n456"));
        }
        target->window()->activateWindow();
        target->setFocus();
        QTRY_VERIFY(target->hasFocus());
        QTest::keyClick(target, Qt::Key_Escape);
        for (Editor* editor : editors) {
            QCOMPARE(editor->text(), enabled && editor == target
                         ? QString() : QStringLiteral("123\n456"));
        }
        QVERIFY(target->hasFocus());
        QTest::keyClick(target, Qt::Key_Escape);
        QCOMPARE(target->text(), enabled ? QString() : QStringLiteral("123\n456"));
    }

    first.activateWindow();
    QLineEdit search(&first);
    search.show();
    search.setFocus();
    QTRY_VERIFY(search.hasFocus());
    for (Editor* editor : editors)
        editor->setText(QStringLiteral("789"));
    QTest::keyClick(&search, Qt::Key_Escape);
    for (Editor* editor : editors)
        QCOMPARE(editor->text(), QStringLiteral("789"));
}

void TestFocusUi::escape_dismisses_completion_and_preview_before_clearing()
{
    Settings::instance()->escapeClearsEditor = true;
    Settings::instance()->autoCalc = false;
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor);
    editor->setFocus();
    QTRY_VERIFY(editor->hasFocus());
    editor->setText(QStringLiteral("co"));
    editor->setCursorPosition(editor->text().size());
    QVERIFY(QMetaObject::invokeMethod(editor, "triggerAutoComplete", Qt::DirectConnection));
    auto completionPopup = []() -> QTreeWidget* {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (auto* tree = qobject_cast<QTreeWidget*>(widget)) {
                if (tree->isVisible())
                    return tree;
            }
        }
        return nullptr;
    };
    QTRY_VERIFY(completionPopup());
    QTreeWidget* popup = completionPopup();
    QTest::keyClick(popup, Qt::Key_Escape);
    QVERIFY(!popup->isVisible());
    QCOMPARE(editor->text(), QStringLiteral("co"));
    QTRY_VERIFY(editor->hasFocus());
    QTest::keyClick(editor, Qt::Key_Escape);
    QVERIFY(editor->text().isEmpty());

    editor->setAutoCompletionEnabled(false);
    editor->setAutoCalcEnabled(true);
    editor->setText(QStringLiteral("1+24"));
    editor->refreshAutoCalc();
    QTRY_VERIFY(visibleResultPreviewText(window).contains(QStringLiteral("Current result:")));
    QTest::keyClick(editor, Qt::Key_Escape);
    QTRY_VERIFY(visibleResultPreviewText(window).isEmpty());
    QCOMPARE(editor->text(), QStringLiteral("1+24"));
    QTest::keyClick(editor, Qt::Key_Escape);
    QVERIFY(editor->text().isEmpty());
}

void TestFocusUi::escape_still_cancels_history_edit_data()
{
    escape_clears_only_the_focused_editor_data();
}

void TestFocusUi::escape_still_cancels_history_edit()
{
    QFETCH(bool, enabled);
    Settings::instance()->escapeClearsEditor = enabled;
    Settings::instance()->autoCalc = false;
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    Editor* editor = window.findChild<Editor*>();
    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(editor);
    QVERIFY(display);
    editor->setAutoCompletionEnabled(false);
    editor->setFocus();
    editor->setText(QStringLiteral("2+3"));
    QTest::keyClick(editor, Qt::Key_Return);
    const int count = display->session()->historySize();
    QVERIFY(count > 0);
    QVERIFY(QMetaObject::invokeMethod(&window, "startHistoryEntryEdit", Qt::DirectConnection,
                                     Q_ARG(int, count - 1)));
    editor->setText(QStringLiteral("6+7"));
    QTest::keyClick(editor, Qt::Key_Escape);
    QCOMPARE(editor->text(), QStringLiteral("6+7"));
    QTest::keyClick(editor, Qt::Key_Escape);
    QVERIFY(editor->text().isEmpty());
    QCOMPARE(display->session()->historySize(), count);
    editor->setText(QStringLiteral("8+9"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(display->session()->historySize(), count + 1);
}

void TestFocusUi::initTestCase()
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

void TestFocusUi::main_window_applies_primary_role_to_active_editor_and_dock_selection()
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

void TestFocusUi::current_result_tooltip_stays_hidden_after_escape_and_arrow_caret_move()
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

void TestFocusUi::current_result_tooltip_stays_hidden_after_escape_and_mouse_caret_move()
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

void TestFocusUi::current_result_tooltip_hides_when_dragging_splitters()
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

void TestFocusUi::f6_cycles_focus_between_editor_and_visible_dock_controls_data()
{
    QTest::addColumn<bool>("savedDockLayout");
    QTest::newRow("default-layout") << false;
    QTest::newRow("saved-visible-dock") << true;
}

void TestFocusUi::f6_cycles_focus_between_editor_and_visible_dock_controls()
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

void TestFocusUi::f6_cycles_focus_with_another_main_window_visible()
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

void TestFocusUi::f6_cycles_focus_with_another_main_window_visible_data()
{
    QTest::addColumn<bool>("floatingDock");
    QTest::newRow("docked") << false;
    QTest::newRow("floating") << true;
}

void TestFocusUi::dock_search_focus_suppresses_editor_primary_outline_across_panes()
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

void TestFocusUi::dock_selection_inserts_into_active_session_pane_after_focus_transfer()
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

void TestFocusUi::clicking_tab_activates_own_pane_in_nested_split_layout()
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

void TestFocusUi::active_pane_survives_window_reactivation_focus_replay()
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

void TestFocusUi::extra_window_activation_restores_own_active_tab_indicator()
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

void TestFocusUi::startup_focuses_editor_with_visible_dock_input_data()
{
    QTest::addColumn<QString>("dockName");
    QTest::addColumn<QString>("startupMode");
    QTest::addColumn<bool>("privateTab");
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
                QTest::newRow(qPrintable(row)) << dock << mode << privateTab;
            }
        }
    }
}

void TestFocusUi::startup_focuses_editor_with_visible_dock_input()
{
    QFETCH(QString, dockName);
    QFETCH(QString, startupMode);
    QFETCH(bool, privateTab);
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
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QDockWidget* dock = window.findChild<QDockWidget*>(dockName);
    QVERIFY(dock != nullptr);
    QTRY_VERIFY(dock->isVisible());
    QLineEdit* search = dock->findChild<QLineEdit*>();
    QVERIFY(search != nullptr);
    const auto focusedDisplay = [&window]() -> ResultDisplay* {
        for (ResultDisplay* display : window.findChildren<ResultDisplay*>()) {
            Editor* editor = paneWidgetForDisplay(display)->findChild<Editor*>();
            if (editor != nullptr && focusIsWithin(editor))
                return display;
        }
        return nullptr;
    };
    QTRY_VERIFY(focusedDisplay() != nullptr);
    QTRY_COMPARE(window.findChildren<Editor*>().size(), startupMode == QLatin1String("split") ? 2 : 1);
    QTRY_VERIFY(focusedDisplay() != nullptr && focusedDisplay()->session()->isPrivate() == privateTab);
    if (!privateTab)
        QTRY_VERIFY(focusedDisplay() != nullptr && focusedDisplay()->session()->name() == activeName);
    if (!privateTab && (savedLayout || startupMode == QLatin1String("legacy")))
        QTRY_VERIFY(focusedDisplay() != nullptr && focusedDisplay()->session()->historySize() == 1);
    if (privateTab && (savedLayout || startupMode == QLatin1String("legacy")))
        QTRY_VERIFY(focusedDisplay() != nullptr && tabBarForDisplay(focusedDisplay())->count() == 2);

    // Delayed window activation must keep the editor selected after restoration.
    QTest::qWait(400);
    QVERIFY(focusedDisplay() != nullptr);
    QVERIFY(!search->hasFocus());

    // Once startup finishes, choosing the dock search must still work normally.
    QTest::mouseClick(search, Qt::LeftButton);
    QTRY_VERIFY(search->hasFocus());
    QTest::qWait(400);
    QVERIFY(search->hasFocus());
}

void TestFocusUi::focused_dock_search_survives_window_reactivation_focus_replay()
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

void TestFocusUi::focusing_loaded_pane_preserves_its_current_scroll_position()
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

void TestFocusUi::clipboard_actions_follow_active_pane_data()
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

void TestFocusUi::clipboard_actions_follow_active_pane()
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

void TestFocusUi::copy_shortcut_preserves_display_selection_in_other_pane()
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

int main(int argc, char** argv)
{
    return UiTestFixture::run<TestFocusUi>(argc, argv);
}

#include "testfocusui.moc"
