// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mainwindowtesthelpers.h"
#include "gui/keybindings.h"
#include "gui/keybindingsdialog.h"
#include "gui/platformactivation.h"

#include <QKeySequenceEdit>
#include <QToolButton>

using namespace MainWindowTestHelpers;

namespace {

void showForInput(QWidget& widget)
{
    if (QGuiApplication::platformName() == QLatin1String("cocoa"))
        activateLaunchWindow(&widget, LaunchRequest());
    else {
        widget.show();
        widget.activateWindow();
    }
    QVERIFY(QTest::qWaitForWindowActive(&widget));
}

QAction* windowAction(MainWindow& window, const QString& id)
{
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->property("keyBindingId").toString() == id)
            return action;
    }
    return nullptr;
}

QTreeWidgetItem* shortcutRow(KeyBindingsDialog& dialog, const QString& id)
{
    QTreeWidget* tree = dialog.findChild<QTreeWidget*>(QStringLiteral("ShortcutActions"));
    QTreeWidgetItemIterator it(tree);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole).toString() == id)
            return *it;
        ++it;
    }
    return nullptr;
}

QKeySequenceEdit* shortcutRecorder(KeyBindingsDialog& dialog, const QString& id)
{
    return dialog.findChild<QKeySequenceEdit*>(QStringLiteral("ShortcutSequence.") + id);
}

bool editShortcut(KeyBindingsDialog& dialog, const QString& id, const QKeySequence& sequence)
{
    QTreeWidgetItem* row = shortcutRow(dialog, id);
    if (!row)
        return false;
    auto* tree = dialog.findChild<QTreeWidget*>(QStringLiteral("ShortcutActions"));
    tree->setCurrentItem(row);
    shortcutRecorder(dialog, id)->setKeySequence(sequence);
    return true;
}

void answerReassign(bool accept)
{
    QTimer::singleShot(0, qApp, [accept]() {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(box);
        if (accept) {
            for (QAbstractButton* button : box->buttons()) {
                if (box->buttonRole(button) == QMessageBox::AcceptRole) {
                    button->click();
                    return;
                }
            }
            QFAIL("Missing Reassign button");
        } else {
            box->button(QMessageBox::Cancel)->click();
        }
    });
}

} // namespace

class TestKeyBindingsUi : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { qApp->setQuitOnLastWindowClosed(false); }
    void init() { UiTestFixture::resetSettings(); }
    void inline_boxes_offer_default_custom_and_disabled_states();
    void inline_recording_stages_changes_and_keeps_apply_cancel_scope();
    void cancelling_during_recording_does_not_prompt_for_conflicts();
    void inline_boxes_follow_filter_and_keyboard_focus();
    void inline_recording_checks_conflicts_after_the_sequence_finishes_data();
    void inline_recording_checks_conflicts_after_the_sequence_finishes();
    void row_menu_can_restore_a_conflicting_default();
    void restore_all_discards_an_unfinished_inline_recording();
    void recorder_accepts_a_multi_step_sequence();
    void settings_menu_offers_a_searchable_action_catalog();
    void shortcut_menu_position_and_trimmed_sections();
    void recording_mode_focuses_input_and_shows_prompt();
    void focusing_recorder_does_not_replace_default_alternatives();
    void calculation_shortcuts_are_grouped_under_results();
    void custom_column_only_shows_overrides();
    void double_click_records_shortcut_data();
    void double_click_records_shortcut();
    void defaults_and_disabled_bindings_survive_reload();
    void removed_saved_bindings_stay_inactive();
    void enter_remains_reserved_for_evaluation();
    void apply_updates_existing_windows_and_new_panes();
    void constants_support_a_custom_multi_step_shortcut();
    void evaluation_stays_on_enter_and_result_scrolling_uses_custom_keys();
    void zoom_in_and_out_can_be_rebound();
    void custom_history_shortcuts_work_with_arrow_history_disabled();
    void multi_step_shortcuts_work_from_floating_docks();
    void custom_edit_keys_replace_native_editor_bindings();
    void removed_completion_shortcut_preserves_automatic_completion();
    void dock_search_keeps_native_text_editing_shortcuts();
    void conflicts_can_be_cancelled_or_reassigned();
    void prefix_conflicts_work_in_both_directions();
    void apply_cancel_and_restore_defaults_keep_their_expected_scope();
    void focus_navigation_does_not_keep_a_removed_default();
    void reset_all_settings_restores_shortcuts_in_open_windows();
    void reset_clears_session_definitions_data();
    void reset_clears_session_definitions();
    void reset_refreshes_definition_docks();
};

void TestKeyBindingsUi::inline_boxes_offer_default_custom_and_disabled_states()
{
    const QString id = QStringLiteral("editorConstants");
    KeyBindingsDialog dialog(KeyBindings::editorActions(), {});
    showForInput(dialog);
    auto* tree = dialog.findChild<QTreeWidget*>(QStringLiteral("ShortcutActions"));
    for (const KeyBindings::Action& action : KeyBindings::editorActions()) {
        auto* row = shortcutRow(dialog, action.id);
        QVERIFY(row);
        auto* cell = tree->itemWidget(row, 2);
        QVERIFY(cell);
        auto* input = cell->findChild<QKeySequenceEdit*>();
        QVERIFY(input);
        QVERIFY(input->keySequence().isEmpty());
        QCOMPARE(input->findChild<QLineEdit*>()->placeholderText(), QStringLiteral("Use default"));
        QVERIFY(!input->accessibleName().isEmpty());
        auto* options = cell->findChild<QToolButton*>(QStringLiteral("ShortcutOptions.") + action.id);
        QVERIFY(options && options->menu());
    }
    QVERIFY(!dialog.findChild<QPushButton*>(QStringLiteral("AssignShortcut")));
    auto* input = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("ShortcutSequence.") + id);
    auto* disable = dialog.findChild<QAction*>(QStringLiteral("DisableShortcut.") + id);
    auto* restore = dialog.findChild<QAction*>(QStringLiteral("RestoreShortcutDefault.") + id);
    QVERIFY(disable->isEnabled());
    QVERIFY(!restore->isEnabled());
    disable->trigger();
    QVERIFY(dialog.overrides().contains(id));
    QVERIFY(dialog.overrides().value(id).isEmpty());
    QCOMPARE(input->findChild<QLineEdit*>()->placeholderText(), QStringLiteral("Disabled"));
    QVERIFY(!disable->isEnabled());
    QVERIFY(restore->isEnabled());
    restore->trigger();
    QVERIFY(dialog.overrides().isEmpty());
    QCOMPARE(input->findChild<QLineEdit*>()->placeholderText(), QStringLiteral("Use default"));
}

void TestKeyBindingsUi::inline_recording_stages_changes_and_keeps_apply_cancel_scope()
{
    KeyBindingsDialog dialog(KeyBindings::editorActions(), {});
    QSignalSpy applied(&dialog, &KeyBindingsDialog::shortcutsApplied);
    showForInput(dialog);
    const QString id = QStringLiteral("editorConstants");
    auto* input = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("ShortcutSequence.") + id);
    input->setFocus();
    QTRY_VERIFY(input->hasFocus());
    QTest::keyClick(input, Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
    QTest::keyClick(input, Qt::Key_C, Qt::ControlModifier | Qt::AltModifier);
    QTRY_COMPARE(dialog.overrides().value(id), QStringList{QStringLiteral("Ctrl+Alt+K, Ctrl+Alt+C")});
    QCOMPARE(applied.count(), 0);
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    QTest::mouseClick(buttons->button(QDialogButtonBox::Apply), Qt::LeftButton);
    QCOMPARE(applied.count(), 1);
    input->setFocus();
    QTest::keyClick(input, Qt::Key_J, Qt::ControlModifier | Qt::AltModifier);
    // Apply also finishes a recording before Qt's one-second timeout.
    QTest::mouseClick(buttons->button(QDialogButtonBox::Apply), Qt::LeftButton);
    QCOMPARE(applied.count(), 2);
    QCOMPARE(dialog.overrides().value(id), QStringList{QStringLiteral("Ctrl+Alt+J")});
    input->setFocus();
    QTest::keyClick(input, Qt::Key_M, Qt::ControlModifier | Qt::AltModifier);
    QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel), Qt::LeftButton);
    QCoreApplication::processEvents();
    QCOMPARE(applied.count(), 2);
}

void TestKeyBindingsUi::cancelling_during_recording_does_not_prompt_for_conflicts()
{
    const QList<KeyBindings::Action> actions = {
        {QStringLiteral("first"), QStringLiteral("Actions"), QStringLiteral("First"),
         {QKeySequence(QStringLiteral("Ctrl+K"))}},
        {QStringLiteral("second"), QStringLiteral("Actions"), QStringLiteral("Second"), {}}
    };
    KeyBindingsDialog dialog(actions, {});
    showForInput(dialog);
    auto* input = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("ShortcutSequence.second"));
    input->setFocus();
    QTest::keyClick(input, Qt::Key_K, Qt::ControlModifier);
    QSignalSpy applied(&dialog, &KeyBindingsDialog::shortcutsApplied);
    QTest::mouseClick(dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel), Qt::LeftButton);
    QCoreApplication::processEvents();
    QVERIFY(!QApplication::activeModalWidget());
    QCOMPARE(dialog.result(), int(QDialog::Rejected));
    QVERIFY(dialog.overrides().isEmpty());
    QCOMPARE(applied.count(), 0);
}

void TestKeyBindingsUi::inline_boxes_follow_filter_and_keyboard_focus()
{
    KeyBindingsDialog dialog(KeyBindings::editorActions(), {});
    showForInput(dialog);
    auto* search = dialog.findChild<QLineEdit*>(QStringLiteral("ShortcutSearch"));
    auto* input = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("ShortcutSequence.editorConstants"));
    auto* other = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("ShortcutSequence.editorHistoryPrevious"));
    search->setText(QStringLiteral("Search Constants"));
    QTRY_VERIFY(input->isVisible());
    QTRY_VERIFY(!other->isVisible());
    QTest::keyClick(search, Qt::Key_Tab);
    for (int step = 0; step < 4 && !input->hasFocus(); ++step)
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
    QVERIFY(input->hasFocus());
    QCOMPARE(input->findChild<QLineEdit*>()->placeholderText(), QStringLiteral("Press shortcut..."));
    QTest::keyClick(input, Qt::Key_Tab);
    auto* options = dialog.findChild<QToolButton*>(QStringLiteral("ShortcutOptions.editorConstants"));
    QVERIFY(options->hasFocus());
    search->setFocus();
    search->clear();
    QTRY_VERIFY(other->isVisible());
    QVERIFY(dialog.overrides().isEmpty());
}

void TestKeyBindingsUi::inline_recording_checks_conflicts_after_the_sequence_finishes_data()
{
    QTest::addColumn<bool>("reassign");
    QTest::newRow("cancel") << false;
    QTest::newRow("reassign") << true;
}

void TestKeyBindingsUi::inline_recording_checks_conflicts_after_the_sequence_finishes()
{
    QFETCH(bool, reassign);
    const QList<KeyBindings::Action> actions = {
        {QStringLiteral("first"), QStringLiteral("Actions"), QStringLiteral("First"),
         {QKeySequence(QStringLiteral("Ctrl+K")), QKeySequence(QStringLiteral("Ctrl+J"))}},
        {QStringLiteral("second"), QStringLiteral("Actions"), QStringLiteral("Second"), {}}
    };
    KeyBindingsDialog dialog(actions, {});
    showForInput(dialog);
    auto* input = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("ShortcutSequence.second"));
    input->setFocus();
    bool responded = false;
    QTimer response;
    connect(&response, &QTimer::timeout, &dialog, [&]() {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box)
            return;
        response.stop();
        responded = true;
        QCOMPARE(input->keySequence(), QKeySequence(QStringLiteral("Ctrl+K, Ctrl+C")));
        if (reassign) {
            for (QAbstractButton* button : box->buttons()) {
                if (box->buttonRole(button) == QMessageBox::AcceptRole) {
                    button->click();
                    return;
                }
            }
            QFAIL("Missing Reassign button");
        } else {
            box->button(QMessageBox::Cancel)->click();
        }
    });
    response.start(10);
    QTest::keyClick(input, Qt::Key_K, Qt::ControlModifier);
    QVERIFY(!responded);
    QTest::keyClick(input, Qt::Key_C, Qt::ControlModifier);
    QVERIFY(!responded);
    QTRY_VERIFY(responded);
    if (reassign) {
        QCOMPARE(dialog.overrides().value(QStringLiteral("first")), QStringList{QStringLiteral("Ctrl+J")});
        QCOMPARE(dialog.overrides().value(QStringLiteral("second")), QStringList{QStringLiteral("Ctrl+K, Ctrl+C")});
    } else {
        QVERIFY(dialog.overrides().isEmpty());
        QVERIFY(input->keySequence().isEmpty());
    }
}

void TestKeyBindingsUi::row_menu_can_restore_a_conflicting_default()
{
    const QList<KeyBindings::Action> actions = {
        {QStringLiteral("first"), QStringLiteral("Actions"), QStringLiteral("First"),
         {QKeySequence(QStringLiteral("Ctrl+K"))}},
        {QStringLiteral("second"), QStringLiteral("Actions"), QStringLiteral("Second"),
         {QKeySequence(QStringLiteral("Ctrl+J"))}}
    };
    const KeyBindings::Overrides saved = {{QStringLiteral("first"), {}},
        {QStringLiteral("second"), {QStringLiteral("Ctrl+K")}}};
    KeyBindingsDialog dialog(actions, saved);
    auto* restore = dialog.findChild<QAction*>(QStringLiteral("RestoreShortcutDefault.first"));
    answerReassign(false);
    restore->trigger();
    QCOMPARE(dialog.overrides(), saved);
    answerReassign(true);
    restore->trigger();
    QVERIFY(!dialog.overrides().contains(QStringLiteral("first")));
    QVERIFY(dialog.overrides().contains(QStringLiteral("second")));
    QVERIFY(dialog.overrides().value(QStringLiteral("second")).isEmpty());
    auto* other = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("ShortcutSequence.second"));
    QVERIFY(other->keySequence().isEmpty());
    QCOMPARE(other->findChild<QLineEdit*>()->placeholderText(), QStringLiteral("Disabled"));
}

void TestKeyBindingsUi::restore_all_discards_an_unfinished_inline_recording()
{
    const QString id = QStringLiteral("editorConstants");
    KeyBindingsDialog dialog(KeyBindings::editorActions(), {{id, {QStringLiteral("Ctrl+Alt+K")}}});
    showForInput(dialog);
    auto* input = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("ShortcutSequence.") + id);
    QTest::mouseClick(input->findChild<QLineEdit*>(), Qt::LeftButton);
    QTRY_VERIFY(input->hasFocus());
    QTest::keyClick(input, Qt::Key_J, Qt::ControlModifier | Qt::AltModifier);
    QTest::mouseClick(dialog.findChild<QPushButton*>(QStringLiteral("RestoreAllShortcutDefaults")), Qt::LeftButton);
    QCoreApplication::processEvents();
    QVERIFY(dialog.overrides().isEmpty());
    QVERIFY(input->keySequence().isEmpty());
    QCOMPARE(input->findChild<QLineEdit*>()->placeholderText(), QStringLiteral("Use default"));
}

void TestKeyBindingsUi::recorder_accepts_a_multi_step_sequence()
{
    KeyBindingsDialog dialog(KeyBindings::editorActions(), {});
    showForInput(dialog);
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QVERIFY(editShortcut(dialog, QStringLiteral("editorConstants"), QKeySequence()));
    auto* recorder = shortcutRecorder(dialog, QStringLiteral("editorConstants"));
    recorder->setFocus();
    QTest::keyClick(recorder, Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
    QTest::keyClick(recorder, Qt::Key_C, Qt::ControlModifier | Qt::AltModifier);
    QTest::qWait(1100);
    QCOMPARE(recorder->keySequence(), QKeySequence(QStringLiteral("Ctrl+Alt+K, Ctrl+Alt+C")));
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    QCOMPARE(dialog.overrides().value(QStringLiteral("editorConstants")),
             QStringList{QStringLiteral("Ctrl+Alt+K, Ctrl+Alt+C")});
}

void TestKeyBindingsUi::double_click_records_shortcut_data()
{
    QTest::addColumn<int>("column");
    QTest::newRow("action") << 0;
    QTest::newRow("default") << 1;
    QTest::newRow("custom") << 2;
}

void TestKeyBindingsUi::double_click_records_shortcut()
{
    QFETCH(int, column);
    KeyBindingsDialog dialog(KeyBindings::editorActions(), {});
    showForInput(dialog);
    auto* tree = dialog.findChild<QTreeWidget*>(QStringLiteral("ShortcutActions"));
    auto* recorder = shortcutRecorder(dialog, QStringLiteral("editorHistoryPrevious"));
    QVERIFY(tree && recorder);
    const QString id = QStringLiteral("editorHistoryPrevious");
    QTreeWidgetItem* row = shortcutRow(dialog, id);
    QVERIFY(row);
    tree->scrollToItem(row);
    tree->setFocus();
    const QRect rect = tree->visualItemRect(row);
    const QPoint position(tree->columnViewportPosition(column) + tree->columnWidth(column) / 2,
                          rect.center().y());
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, position);
    QTest::mouseDClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, position);
    QTRY_VERIFY(focusIsWithin(recorder));
    QVERIFY(dialog.overrides().isEmpty());
    QTest::keyClick(recorder, Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
    QTest::qWait(1100);
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    QCOMPARE(dialog.overrides().value(id), QStringList{QStringLiteral("Ctrl+Alt+K")});
}

void TestKeyBindingsUi::custom_column_only_shows_overrides()
{
    const QString id = QStringLiteral("editorConstants");
    const QKeySequence original(Qt::CTRL | Qt::Key_Space);
    const QKeySequence custom(QStringLiteral("Ctrl+Alt+K"));
    KeyBindingsDialog dialog(KeyBindings::editorActions(), {});
    auto* tree = dialog.findChild<QTreeWidget*>(QStringLiteral("ShortcutActions"));
    QVERIFY(tree);
    QCOMPARE(tree->headerItem()->text(1), QStringLiteral("Default"));
    QCOMPARE(tree->headerItem()->text(2), QStringLiteral("Custom"));
    QTreeWidgetItem* row = shortcutRow(dialog, id);
    QVERIFY(row);
    QVERIFY(shortcutRecorder(dialog, id)->keySequence().isEmpty());
    QCOMPARE(shortcutRecorder(dialog, id)->findChild<QLineEdit*>()->placeholderText(), QStringLiteral("Use default"));
    QCOMPARE(row->text(1), original.toString(QKeySequence::NativeText));
    QVERIFY(editShortcut(dialog, id, custom));
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    QCOMPARE(shortcutRecorder(dialog, id)->keySequence(), custom);
    QCOMPARE(row->text(1), original.toString(QKeySequence::NativeText));
    dialog.findChild<QAction*>(QStringLiteral("DisableShortcut.") + id)->trigger();
    QVERIFY(shortcutRecorder(dialog, id)->keySequence().isEmpty());
    QCOMPARE(shortcutRecorder(dialog, id)->findChild<QLineEdit*>()->placeholderText(), QStringLiteral("Disabled"));
    dialog.findChild<QAction*>(QStringLiteral("RestoreShortcutDefault.") + id)->trigger();
    QVERIFY(shortcutRecorder(dialog, id)->keySequence().isEmpty());
    QCOMPARE(shortcutRecorder(dialog, id)->findChild<QLineEdit*>()->placeholderText(), QStringLiteral("Use default"));
    QCOMPARE(row->text(1), original.toString(QKeySequence::NativeText));

    KeyBindings::Overrides saved;
    saved.insert(id, {custom.toString(QKeySequence::PortableText)});
    KeyBindingsDialog restored(KeyBindings::editorActions(), saved);
    QCOMPARE(shortcutRecorder(restored, id)->keySequence(), custom);
    saved[id].clear();
    KeyBindingsDialog disabled(KeyBindings::editorActions(), saved);
    QVERIFY(shortcutRecorder(disabled, id)->keySequence().isEmpty());
    QCOMPARE(shortcutRecorder(disabled, id)->findChild<QLineEdit*>()->placeholderText(), QStringLiteral("Disabled"));
}

void TestKeyBindingsUi::settings_menu_offers_a_searchable_action_catalog()
{
    MainWindow window(false);
    QAction* settings = windowAction(window, QStringLiteral("settingsKeyBindings"));
    QVERIFY(settings);
    QTimer::singleShot(0, &window, [&]() {
        auto* dialog = qobject_cast<KeyBindingsDialog*>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        const auto close = qScopeGuard([&]() { dialog->reject(); });
        auto* tree = dialog->findChild<QTreeWidget*>(QStringLiteral("ShortcutActions"));
        QVERIFY(tree);
        for (const QString& id : {
             QStringLiteral("editorComplete"),
             QStringLiteral("editorEvaluate"),
             QStringLiteral("viewKeypadCustom"),
             QStringLiteral("sessionImport"),
             QStringLiteral("viewKeypadZoom50"), QStringLiteral("viewKeypadZoom100"),
             QStringLiteral("viewKeypadZoom150"), QStringLiteral("viewKeypadZoom200"),
             QStringLiteral("settingsBehaviorAutoCompletionAcceptWithEnter"),
             QStringLiteral("settingsBehaviorAutoCompletionBuiltInFunctions"),
             QStringLiteral("settingsBehaviorAutoCompletionBuiltInVariables"),
             QStringLiteral("settingsBehaviorAutoCompletionLongFormUnits"),
             QStringLiteral("settingsBehaviorAutoCompletionUserFunctions"),
             QStringLiteral("settingsBehaviorAutoCompletionUserVariables"),
             QStringLiteral("settingsResultFormatPolarAngle"),
             QStringLiteral("settingsLaunchNewWindow"), QStringLiteral("settingsLaunchNewTab"),
             QStringLiteral("settingsResultFormatCustomDigits"),
             QStringLiteral("settingsRadixCharDot"),
             QStringLiteral("settingsRadixCharComma"),
             QStringLiteral("settingsRadixCharDefault"),
             QStringLiteral("settingsRadixCharBoth"),
             QStringLiteral("settingsBehaviorDigitGroupingOneSpace"),
             QStringLiteral("settingsBehaviorDigitGroupingTwoSpaces"),
             QStringLiteral("settingsBehaviorDigitGroupingThreeSpaces"),
             QStringLiteral("settingsBehaviorDigitGroupingNone"),
             QStringLiteral("settingsBehaviorDigitGroupingIntegerPartOnly"),
             QStringLiteral("settingsBehaviorAutoCompletion"),
             QStringLiteral("helpAbout"),
             QStringLiteral("helpDiagnostics"),
             QStringLiteral("helpDonate"),
             QStringLiteral("helpSource"),
             QStringLiteral("helpFeedback"),
             QStringLiteral("helpCommunity"),
             QStringLiteral("settingsBehaviorAlwaysOnTop"),
             QStringLiteral("settingsBehaviorSaveWindowPositionOnExit"),
             QStringLiteral("settingsOpenPrivateTabOnStartup"),
             QStringLiteral("settingsLaunchActivate"),
             QStringLiteral("settingsImaginaryUnitI"),
             QStringLiteral("settingsImaginaryUnitJ"),
             QStringLiteral("settingsUnitNegativeExponentSuperscript"),
             QStringLiteral("settingsUnitNegativeExponentFraction"),
             QStringLiteral("settingsResultFormatCartesian"),
             QStringLiteral("settingsResultFormatPolar"),
             QStringLiteral("settingsResultFormatTrigonometric"),
             QStringLiteral("settingsResultFormatCis"),
             QStringLiteral("settingsResultRoundingHalfAwayFromZero"),
             QStringLiteral("settingsResultRoundingHalfEven"),
             QStringLiteral("settingsResultRoundingTowardZero"),
             QStringLiteral("settingsResultRoundingTowardPositiveInfinity"),
             QStringLiteral("settingsResultRoundingTowardNegativeInfinity"),
             QStringLiteral("settingsSkipClearSessionConfirmation"),
             QStringLiteral("settingsBehaviorAutoResultToClipboard"),
             QStringLiteral("settingsBehaviorSimplifyResultExpressions"),
             QStringLiteral("settingsBehaviorPartialResults"),
             QStringLiteral("settingsRemoveTrailingZeroes"),
             QStringLiteral("settingsBehaviorResultSlots"),
             QStringLiteral("settingsBehaviorUpDownArrowNever"),
             QStringLiteral("settingsBehaviorUpDownArrowAlways"),
             QStringLiteral("settingsBehaviorUpDownArrowSingleLineOnly"),
             QStringLiteral("settingsBehaviorLeaveLastExpression"),
             QStringLiteral("settingsEditingEscapeClearsEditor"),
             QStringLiteral("settingsEditingCursorBlinking"),
             QStringLiteral("settingsEditingInputDigitGrouping"),
             QStringLiteral("settingsBehaviorAutoAns"),
             QStringLiteral("settingsEditingAutomaticInputAdjustments"),
             QStringLiteral("settingsMenuAppearanceSystem"),
             QStringLiteral("settingsMenuAppearanceSpeedCrunch"),
             QStringLiteral("settingsDockAppearanceSystem"),
             QStringLiteral("settingsDockAppearanceSpeedCrunch"),
             QStringLiteral("settingsStatusBarAppearanceSystem"),
             QStringLiteral("settingsStatusBarAppearanceSpeedCrunch"),
             QStringLiteral("settingsToolTipAppearanceSystem"),
             QStringLiteral("settingsToolTipAppearanceSpeedCrunch"),
             QStringLiteral("settingsKeypadAppearanceClassic"),
             QStringLiteral("settingsKeypadAppearanceModern"),
             QStringLiteral("settingsEditorDisplayClassic"),
             QStringLiteral("settingsEditorDisplayModern"),
             QStringLiteral("settingsAppearancePresetClassic"),
             QStringLiteral("settingsAppearancePresetModern"),
             QStringLiteral("settingsAppearancePresetCustom"),
             QStringLiteral("settingsDisplayFont"),
             QStringLiteral("settingsSystemColorModeFollowSystem"),
             QStringLiteral("settingsSystemColorModeLight"),
             QStringLiteral("settingsSystemColorModeDark"),
             QStringLiteral("settingsBehaviorHoverHighlightResults"),
             QStringLiteral("settingsBehaviorSyntaxHighlighting"),
             QStringLiteral("settingsDisplayColorSchemeCustom"),
             QStringLiteral("settingsResetAll"),
             QStringLiteral("settingsLanguage"),
             QStringLiteral("settingsBehaviorNumberFormat"),
             QStringLiteral("sessionExportHtml"),
             QStringLiteral("sessionExportJson"),
             QStringLiteral("sessionExportPlainText")})
            QVERIFY2(!shortcutRow(*dialog, id), qPrintable(id));
        for (const QString& id : {QStringLiteral("editorConstants"), QStringLiteral("editorHistoryPrevious"),
             QStringLiteral("settingsDisplayZoomIn"), QStringLiteral("settingsDisplayZoomOut"),
             QStringLiteral("navigationNextTab"), QStringLiteral("navigationNextFocus"),
             QStringLiteral("viewSplitRight"), QStringLiteral("resultsScrollTop"),
             QStringLiteral("viewKeypadDisabled"),
             QStringLiteral("settingsResultFormatRational"), QStringLiteral("helpManual")})
            QVERIFY2(shortcutRow(*dialog, id), qPrintable(id));
        auto* search = dialog->findChild<QLineEdit*>(QStringLiteral("ShortcutSearch"));
        search->setText(QStringLiteral("zoom"));
        QVERIFY(!shortcutRow(*dialog, QStringLiteral("settingsDisplayZoomIn"))->isHidden());
        QVERIFY(shortcutRow(*dialog, QStringLiteral("sessionOpen"))->isHidden());
        search->clear();
        QVERIFY(!shortcutRow(*dialog, QStringLiteral("sessionOpen"))->isHidden());
        const QString screenshot = qEnvironmentVariable("SPEEDCRUNCH_TEST_KEYBINDINGS_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            if (qEnvironmentVariableIntValue("SPEEDCRUNCH_TEST_KEYBINDINGS_LARGE_FONT")) {
                QFont font = dialog->font();
                font.setPointSizeF(font.pointSizeF() * 1.5);
                dialog->setFont(font);
            }
            dialog->show();
            QTest::qWait(100);
            QVERIFY(dialog->grab().save(screenshot));
        }
    });
    settings->trigger();
    QVERIFY(Settings::instance()->keyBindings.isEmpty());
}

void TestKeyBindingsUi::shortcut_menu_position_and_trimmed_sections()
{
    MainWindow window(false);
    QMenu* menu = menuWithTitle(window.menuBar(), QStringLiteral("Se&ttings"));
    QVERIFY(menu);
    const auto entries = menu->actions();
    const int definitions = entries.indexOf(windowAction(window, QStringLiteral("sessionImportUserDefinitions")));
    QVERIFY(definitions >= 0);
    QCOMPARE(entries.indexOf(windowAction(window, QStringLiteral("settingsKeyBindings"))), definitions + 1);
    QTimer::singleShot(0, &window, [&]() {
        auto* dialog = qobject_cast<KeyBindingsDialog*>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        const auto close = qScopeGuard([&]() { dialog->reject(); });
        auto* tree = dialog->findChild<QTreeWidget*>(QStringLiteral("ShortcutActions"));
        QVERIFY(tree);
        const QStringList removed = {QStringLiteral("Settings / Appearance / Keypad / Zoom"),
            QStringLiteral("Settings / Editing / Autocomplete"),
            QStringLiteral("Settings / Results / Complex Form"),
            QStringLiteral("Settings / Window / On Subsequent Launch")};
        for (int index = 0; index < tree->topLevelItemCount(); ++index)
            QVERIFY(!removed.contains(tree->topLevelItem(index)->text(0)));
        auto* disable = shortcutRow(*dialog, QStringLiteral("viewKeypadDisabled"));
        QVERIFY(disable && disable->parent());
        QCOMPARE(disable->text(0), QStringLiteral("Disable"));
        QCOMPARE(disable->parent()->text(0), QStringLiteral("View / Keypad"));
    });
    windowAction(window, QStringLiteral("settingsKeyBindings"))->trigger();
}

void TestKeyBindingsUi::recording_mode_focuses_input_and_shows_prompt()
{
    KeyBindingsDialog dialog(KeyBindings::editorActions(), {});
    showForInput(dialog);
    auto* tree = dialog.findChild<QTreeWidget*>(QStringLiteral("ShortcutActions"));
    auto* recorder = shortcutRecorder(dialog, QStringLiteral("editorConstants"));
    auto* hint = dialog.findChild<QLabel*>(QStringLiteral("ShortcutHint"));
    QVERIFY(tree && recorder && hint);
    auto* row = shortcutRow(dialog, QStringLiteral("editorConstants"));
    QVERIFY(row);
    const QPoint position = tree->visualItemRect(row).center();
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, position);
    QTest::mouseDClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, position);
    QTRY_VERIFY(recorder->hasFocus());
    auto* input = recorder->findChild<QLineEdit*>();
    QVERIFY(input);
    QCOMPARE(input->placeholderText(), QStringLiteral("Press shortcut..."));
    QVERIFY(hint->text().contains(QStringLiteral("Click a Custom box")));
    QVERIFY(dialog.overrides().isEmpty());
    const QString screenshot = qEnvironmentVariable("SPEEDCRUNCH_TEST_RECORDING_SCREENSHOT");
    if (!screenshot.isEmpty())
        QVERIFY(dialog.grab().save(screenshot));
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
    QTRY_COMPARE(dialog.overrides().value(QStringLiteral("editorConstants")), QStringList{QStringLiteral("Ctrl+Alt+K")});
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    QCOMPARE(dialog.overrides().value(QStringLiteral("editorConstants")), QStringList{QStringLiteral("Ctrl+Alt+K")});
}

void TestKeyBindingsUi::focusing_recorder_does_not_replace_default_alternatives()
{
    const QList<KeyBindings::Action> actions = {{QStringLiteral("example"), QStringLiteral("Actions"),
        QStringLiteral("Example"), {QKeySequence(QStringLiteral("Ctrl+T")), QKeySequence(QStringLiteral("Ctrl+N"))}}};
    KeyBindingsDialog dialog(actions, {});
    showForInput(dialog);
    auto* recorder = shortcutRecorder(dialog, QStringLiteral("example"));
    auto* tree = dialog.findChild<QTreeWidget*>(QStringLiteral("ShortcutActions"));
    QVERIFY(recorder && tree);
    recorder->setFocus();
    QTRY_VERIFY(recorder->hasFocus());
    tree->setFocus();
    QTRY_VERIFY(tree->hasFocus());
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    QVERIFY(dialog.overrides().isEmpty());
}

void TestKeyBindingsUi::calculation_shortcuts_are_grouped_under_results()
{
    MainWindow window(false);
    QTimer::singleShot(0, &window, [&]() {
        auto* dialog = qobject_cast<KeyBindingsDialog*>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        const auto close = qScopeGuard([&]() { dialog->reject(); });
        const QString precision = QStringLiteral("Settings / Results / Precision");
        for (const QString& suffix : {QStringLiteral("0Digits"), QStringLiteral("2Digits"),
             QStringLiteral("3Digits"), QStringLiteral("8Digits"), QStringLiteral("15Digits"),
             QStringLiteral("50Digits"), QStringLiteral("AutoPrecision")}) {
            auto* row = shortcutRow(*dialog, QStringLiteral("settingsResultFormat") + suffix);
            QVERIFY2(row && row->parent(), qPrintable(suffix));
            QCOMPARE(row->parent()->text(0), precision);
        }
        const QString notation = QStringLiteral("Settings / Results / Notation");
        for (const QString& suffix : {QStringLiteral("General"), QStringLiteral("Fixed"),
             QStringLiteral("Engineering"), QStringLiteral("Scientific"), QStringLiteral("Binary"),
             QStringLiteral("Octal"), QStringLiteral("Hexadecimal"), QStringLiteral("Rational"),
             QStringLiteral("Sexagesimal")}) {
            auto* row = shortcutRow(*dialog, QStringLiteral("settingsResultFormat") + suffix);
            QVERIFY2(row && row->parent(), qPrintable(suffix));
            const bool decimal = suffix == QLatin1String("General") || suffix == QLatin1String("Fixed")
                || suffix == QLatin1String("Engineering") || suffix == QLatin1String("Scientific");
            QCOMPARE(row->parent()->text(0), decimal ? notation + QStringLiteral(" / Decimal") : notation);
        }
    });
    windowAction(window, QStringLiteral("settingsKeyBindings"))->trigger();
}

void TestKeyBindingsUi::removed_saved_bindings_stay_inactive()
{
    Settings* settings = Settings::instance();
    const QStringList removed = {
        QStringLiteral("helpDiagnostics"), QStringLiteral("helpDonate"),
        QStringLiteral("settingsBehaviorAlwaysOnTop"), QStringLiteral("sessionExportJson"),
        QStringLiteral("settingsDisplayFont"), QStringLiteral("settingsResultRoundingHalfEven")
    };
    for (const QString& id : removed)
        settings->keyBindings.insert(id, {QStringLiteral("Ctrl+Alt+K")});
    settings->keyBindings.insert(QStringLiteral("editorComplete"), {QStringLiteral("Ctrl+Alt+M")});
    settings->keyBindings.insert(QStringLiteral("editorEvaluate"), {});
    settings->save();
    settings->load();
    MainWindow window(false);
    for (const QString& id : removed) {
        QAction* action = windowAction(window, id);
        QVERIFY(action);
        QVERIFY(action->shortcuts().isEmpty());
        QVERIFY(!settings->keyBindings.contains(id));
    }
    QVERIFY(!settings->keyBindings.contains(QStringLiteral("editorComplete")));
    QVERIFY(!settings->keyBindings.contains(QStringLiteral("editorEvaluate")));
    showForInput(window);
    Editor* editor = window.findChild<Editor*>();
    editor->setFocus();
    QTRY_VERIFY(editor->hasFocus());
    QSignalSpy evaluated(editor, &Editor::returnPressed);
    editor->setText(QStringLiteral("1+2"));
    QTest::keyClick(editor, Qt::Key_Return);
    QCOMPARE(evaluated.count(), 1);
}

void TestKeyBindingsUi::enter_remains_reserved_for_evaluation()
{
    KeyBindingsDialog dialog(KeyBindings::editorActions(), {});
    QVERIFY(editShortcut(dialog, QStringLiteral("editorConstants"), QKeySequence(Qt::Key_Return)));
    QTimer::singleShot(0, &dialog, []() {
        auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(warning);
        const auto close = qScopeGuard([&]() { warning->accept(); });
        QCOMPARE(warning->icon(), QMessageBox::Warning);
        QVERIFY(warning->text().contains(QStringLiteral("Enter and Return evaluate expressions.")));
    });
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    QVERIFY(dialog.overrides().isEmpty());
}

void TestKeyBindingsUi::defaults_and_disabled_bindings_survive_reload()
{
    Settings* settings = Settings::instance();
    settings->keyBindings.insert(QStringLiteral("editorConstants"), {QStringLiteral("Ctrl+Alt+K, Ctrl+Alt+C")});
    settings->keyBindings.insert(QStringLiteral("sessionOpen"), {});
    settings->keyBindings.insert(QStringLiteral("futureAction"), {QStringLiteral("Alt+F12")});
    const auto expected = settings->keyBindings;
    settings->save();
    settings->keyBindings.clear();
    settings->load();
    QCOMPARE(settings->keyBindings, expected);
    MainWindow window(false);
    QVERIFY(windowAction(window, QStringLiteral("sessionOpen"))->shortcuts().isEmpty());
    QVERIFY(!windowAction(window, QStringLiteral("sessionNewTab"))->shortcuts().isEmpty());
    QVERIFY(settings->resetToDefaults());
    QVERIFY(settings->keyBindings.isEmpty());
}

void TestKeyBindingsUi::apply_updates_existing_windows_and_new_panes()
{
    MainWindow first(false);
    MainWindow second(false);
    const QKeySequence custom(QStringLiteral("Ctrl+Alt+K, Ctrl+Alt+C"));
    QTimer::singleShot(0, &first, [&]() {
        auto* dialog = qobject_cast<KeyBindingsDialog*>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        const auto close = qScopeGuard([&]() { dialog->reject(); });
        QVERIFY(editShortcut(*dialog, QStringLiteral("editorConstants"), custom));
        auto* buttons = dialog->findChild<QDialogButtonBox*>();
        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::instance()->keyBindings.value(QStringLiteral("editorConstants")),
                 QStringList{custom.toString(QKeySequence::PortableText)});
    });
    windowAction(first, QStringLiteral("settingsKeyBindings"))->trigger();
    QVERIFY(QMetaObject::invokeMethod(&second, "splitActivePaneRight", Qt::DirectConnection));
    for (MainWindow* window : {&first, &second}) {
        for (Editor* editor : window->findChildren<Editor*>()) {
            auto* action = editor->findChild<QAction*>(QStringLiteral("KeyBinding.editorConstants"));
            QVERIFY(action);
            QCOMPARE(action->shortcuts(), QList<QKeySequence>{custom});
        }
    }
    Settings::instance()->keyBindings.clear();
    Settings::instance()->load();
    QCOMPARE(Settings::instance()->keyBindings.value(QStringLiteral("editorConstants")),
             QStringList{custom.toString(QKeySequence::PortableText)});
}

void TestKeyBindingsUi::constants_support_a_custom_multi_step_shortcut()
{
    Settings::instance()->keyBindings.insert(QStringLiteral("editorConstants"),
                                            {QStringLiteral("Ctrl+Alt+K, Ctrl+Alt+C")});
    MainWindow window(false);
    showForInput(window);
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    Editor* editor = window.findChild<Editor*>();
    editor->setAutoCompletionEnabled(false);
    editor->setText(QStringLiteral("123"));
    editor->setFocus();
    QTRY_VERIFY(editor->hasFocus());
    QTest::keyClick(editor, Qt::Key_Space, Qt::ControlModifier);
    QVERIFY(!editor->isConstantSearchVisible());
    const QString before = editor->text();
    QTest::keyClick(editor, Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
    QVERIFY(!editor->isConstantSearchVisible());
    QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier | Qt::AltModifier);
    QTRY_VERIFY(editor->isConstantSearchVisible());
    QCOMPARE(editor->text(), before);
    editor->cancelConstantCompletion();
}

void TestKeyBindingsUi::evaluation_stays_on_enter_and_result_scrolling_uses_custom_keys()
{
    Settings* settings = Settings::instance();
    settings->keyBindings.insert(QStringLiteral("editorEvaluate"), {QStringLiteral("Ctrl+Alt+E")});
    settings->keyBindings.insert(QStringLiteral("resultsScrollPageUp"), {QStringLiteral("Ctrl+Alt+U")});
    Editor editor;
    showForInput(editor);
    editor.setFocus();
    QTRY_VERIFY(editor.hasFocus());
    QSignalSpy evaluations(&editor, &Editor::returnPressed);
    QSignalSpy scrolling(&editor, &Editor::pageUpPressed);
    QTest::keyClick(&editor, Qt::Key_Return);
    QTest::keyClick(&editor, Qt::Key_PageUp);
    QCOMPARE(evaluations.count(), 1);
    QCOMPARE(scrolling.count(), 0);
    QTest::keyClick(&editor, Qt::Key_E, Qt::ControlModifier | Qt::AltModifier);
    QTest::keyClick(&editor, Qt::Key_U, Qt::ControlModifier | Qt::AltModifier);
    QCOMPARE(evaluations.count(), 1);
    QCOMPARE(scrolling.count(), 1);
}

void TestKeyBindingsUi::zoom_in_and_out_can_be_rebound()
{
    Settings* settings = Settings::instance();
    settings->keyBindings.insert(QStringLiteral("settingsDisplayZoomIn"), {QStringLiteral("Ctrl+Alt+I")});
    settings->keyBindings.insert(QStringLiteral("settingsDisplayZoomOut"), {QStringLiteral("Ctrl+Alt+O")});
    MainWindow window(false);
    showForInput(window);
    Editor* editor = window.findChild<Editor*>();
    editor->setFocus();
    QTRY_VERIFY(editor->hasFocus());
    const qreal initial = editor->font().pointSizeF();
    QTest::keyClick(editor, Qt::Key_Up, Qt::ShiftModifier);
    QCOMPARE(editor->font().pointSizeF(), initial);
    QTest::keyClick(editor, Qt::Key_I, Qt::ControlModifier | Qt::AltModifier);
    QVERIFY(editor->font().pointSizeF() > initial);
    QTest::keyClick(editor, Qt::Key_O, Qt::ControlModifier | Qt::AltModifier);
    QCOMPARE(editor->font().pointSizeF(), initial);
    windowAction(window, QStringLiteral("settingsDisplayZoomIn"))->trigger();
    QVERIFY(editor->font().pointSizeF() > initial);
}

void TestKeyBindingsUi::custom_history_shortcuts_work_with_arrow_history_disabled()
{
    Settings* settings = Settings::instance();
    settings->upDownArrowBehavior = Settings::UpDownArrowBehaviorNever;
    settings->keyBindings.insert(QStringLiteral("editorHistoryPrevious"), {QStringLiteral("Ctrl+Alt+P")});
    settings->keyBindings.insert(QStringLiteral("editorHistoryNext"), {QStringLiteral("Ctrl+Alt+N")});
    MainWindow window(false);
    showForInput(window);
    Editor* editor = window.findChild<Editor*>();
    editor->setAutoCompletionEnabled(false);
    editor->setFocus();
    QTRY_VERIFY(editor->hasFocus());
    editor->setText(QStringLiteral("12+34"));
    QTest::keyClick(editor, Qt::Key_Return);
    editor->setText(QStringLiteral("99"));
    QTest::keyClick(editor, Qt::Key_Up);
    QCOMPARE(editor->text(), QStringLiteral("99"));
    QTest::keyClick(editor, Qt::Key_P, Qt::ControlModifier | Qt::AltModifier);
    QCOMPARE(editor->text(), QStringLiteral("12+34"));
    QTest::keyClick(editor, Qt::Key_N, Qt::ControlModifier | Qt::AltModifier);
    QCOMPARE(editor->text(), QStringLiteral("99"));
}

void TestKeyBindingsUi::multi_step_shortcuts_work_from_floating_docks()
{
    Settings::instance()->keyBindings.insert(QStringLiteral("navigationNextFocus"),
                                            {QStringLiteral("Ctrl+Alt+K, Ctrl+Alt+F")});
    MainWindow window(false);
    showForInput(window);
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(QMetaObject::invokeMethod(&window, "setConstantsDockVisible", Qt::DirectConnection,
                                     Q_ARG(bool, true), Q_ARG(bool, false)));
    auto* dock = window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(dock);
    dock->setFloating(true);
    showForInput(*dock);
    QLineEdit* search = dock->findChild<QLineEdit*>();
    QVERIFY(search);
    QTest::mouseClick(search, Qt::LeftButton);
    QTRY_VERIFY(search->hasFocus());
    QSignalSpy triggered(windowAction(window, QStringLiteral("navigationNextFocus")), &QAction::triggered);
    QTest::keyClick(search, Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
    QTest::keyClick(search, Qt::Key_F, Qt::ControlModifier | Qt::AltModifier);
    QCOMPARE(triggered.count(), 1);
}

void TestKeyBindingsUi::custom_edit_keys_replace_native_editor_bindings()
{
    Settings::instance()->keyBindings.insert(QStringLiteral("editPaste"), {QStringLiteral("Ctrl+Alt+P")});
    Settings::instance()->keyBindings.insert(QStringLiteral("editCopy"), {QStringLiteral("Ctrl+Alt+C")});
    MainWindow window(false);
    showForInput(window);
    Editor* editor = window.findChild<Editor*>();
    editor->setAutoCompletionEnabled(false);
    editor->setFocus();
    QTRY_VERIFY(editor->hasFocus());
    QApplication::clipboard()->setText(QStringLiteral("123"));
    QTest::keyClick(editor, Qt::Key_V, Qt::ControlModifier);
    QVERIFY(editor->text().isEmpty());
    QTest::keyClick(editor, Qt::Key_P, Qt::ControlModifier | Qt::AltModifier);
    QCOMPARE(editor->text(), QStringLiteral("123"));
    editor->selectAll();
    QApplication::clipboard()->setText(QStringLiteral("keep"));
    QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier);
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("keep"));
    QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier | Qt::AltModifier);
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("123"));
}

void TestKeyBindingsUi::removed_completion_shortcut_preserves_automatic_completion()
{
    Settings::instance()->keyBindings.insert(QStringLiteral("editorComplete"), {QStringLiteral("Ctrl+Alt+M")});
    Editor editor;
    editor.setAutoCompletionEnabled(false);
    editor.setText(QStringLiteral("sq"));
    editor.setCursorPosition(editor.text().size());
    showForInput(editor);
    editor.setFocus();
    QTRY_VERIFY(editor.hasFocus());
    QTest::keyClick(&editor, Qt::Key_M, Qt::ControlModifier | Qt::AltModifier);
    auto* popup = editor.findChild<QTreeWidget*>(QStringLiteral("editorCompletionPopup"));
    QVERIFY(popup);
    QVERIFY(!popup->isVisible());
    editor.setText(QStringLiteral("sq"));
    editor.setCursorPosition(editor.text().size());
    editor.setAutoCompletionEnabled(true);
    QVERIFY(QMetaObject::invokeMethod(&editor, "triggerAutoComplete", Qt::DirectConnection));
    QTRY_VERIFY(popup->isVisible());
    QVERIFY(popup->topLevelItemCount() > 0);
    QTest::keyClick(&editor, Qt::Key_Tab);
    QVERIFY(!popup->isVisible());
    QVERIFY(editor.text() != QStringLiteral("sq"));
    QVERIFY(editor.isAutoCompletionEnabled());
}

void TestKeyBindingsUi::dock_search_keeps_native_text_editing_shortcuts()
{
    MainWindow window(false);
    showForInput(window);
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(QTest::qWaitForWindowActive(&window));
    QVERIFY(QMetaObject::invokeMethod(&window, "setConstantsDockVisible", Qt::DirectConnection,
                                     Q_ARG(bool, true), Q_ARG(bool, true)));
    auto* dock = window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(dock);
    QLineEdit* search = dock->findChild<QLineEdit*>();
    QVERIFY(search);
    search->setText(QStringLiteral("constant search"));
    QTest::mouseClick(search, Qt::LeftButton);
    QTRY_VERIFY(search->hasFocus());
    QTest::keyClick(search, Qt::Key_A, Qt::ControlModifier);
    QCOMPARE(search->selectedText(), QStringLiteral("constant search"));
    QApplication::clipboard()->setText(QStringLiteral("replace"));
    QTest::keyClick(search, Qt::Key_V, Qt::ControlModifier);
    QCOMPARE(search->text(), QStringLiteral("replace"));
    QVERIFY(search->hasFocus());
}

void TestKeyBindingsUi::conflicts_can_be_cancelled_or_reassigned()
{
    const QList<KeyBindings::Action> actions = {
        {QStringLiteral("copy"), QStringLiteral("Editing"), QStringLiteral("Copy"),
         {QKeySequence(QStringLiteral("Ctrl+C")), QKeySequence(QStringLiteral("Ctrl+Insert"))}},
        {QStringLiteral("constants"), QStringLiteral("Editing"), QStringLiteral("Constants"),
         {QKeySequence(QStringLiteral("Ctrl+Space"))}}
    };
    KeyBindingsDialog dialog(actions, {});
    QVERIFY(editShortcut(dialog, QStringLiteral("constants"), QKeySequence(QStringLiteral("Ctrl+C"))));
    answerReassign(false);
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    QVERIFY(dialog.overrides().isEmpty());
    QVERIFY(shortcutRecorder(dialog, QStringLiteral("constants"))->keySequence().isEmpty());
    QVERIFY(editShortcut(dialog, QStringLiteral("constants"), QKeySequence(QStringLiteral("Ctrl+C"))));
    answerReassign(true);
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    QCOMPARE(dialog.overrides().value(QStringLiteral("copy")), QStringList{QStringLiteral("Ctrl+Ins")});
    QCOMPARE(dialog.overrides().value(QStringLiteral("constants")), QStringList{QStringLiteral("Ctrl+C")});
}

void TestKeyBindingsUi::prefix_conflicts_work_in_both_directions()
{
    for (bool shorter : {false, true}) {
        const QKeySequence single(QStringLiteral("Ctrl+K"));
        const QKeySequence multiple(QStringLiteral("Ctrl+K, Ctrl+C"));
        const QList<KeyBindings::Action> actions = {
            {QStringLiteral("first"), QStringLiteral("Actions"), QStringLiteral("First"),
             {shorter ? multiple : single}},
            {QStringLiteral("second"), QStringLiteral("Actions"), QStringLiteral("Second"), {}}
        };
        KeyBindingsDialog dialog(actions, {});
        QVERIFY(editShortcut(dialog, QStringLiteral("second"), shorter ? single : multiple));
        answerReassign(true);
        dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
        QVERIFY(dialog.overrides().contains(QStringLiteral("first")));
        QVERIFY(dialog.overrides().value(QStringLiteral("first")).isEmpty());
        QCOMPARE(dialog.overrides().value(QStringLiteral("second")),
                 QStringList{(shorter ? single : multiple).toString(QKeySequence::PortableText)});
    }
}

void TestKeyBindingsUi::apply_cancel_and_restore_defaults_keep_their_expected_scope()
{
    const QList<KeyBindings::Action> actions = KeyBindings::editorActions();
    KeyBindingsDialog dialog(actions, {});
    QSignalSpy applied(&dialog, &KeyBindingsDialog::shortcutsApplied);
    QVERIFY(editShortcut(dialog, QStringLiteral("editorConstants"), QKeySequence(QStringLiteral("Ctrl+Alt+C"))));
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();
    QCOMPARE(applied.count(), 1);
    QVERIFY(editShortcut(dialog, QStringLiteral("editorConstants"), QKeySequence(QStringLiteral("Ctrl+Alt+K"))));
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click();
    QCOMPARE(applied.count(), 1);
    const auto committed = qvariant_cast<KeyBindings::Overrides>(applied.at(0).at(0));
    QCOMPARE(committed.value(QStringLiteral("editorConstants")), QStringList{QStringLiteral("Ctrl+Alt+C")});

    KeyBindingsDialog restore(actions, committed);
    QSignalSpy restored(&restore, &KeyBindingsDialog::shortcutsApplied);
    QVERIFY(editShortcut(restore, QStringLiteral("editorConstants"), QKeySequence(QStringLiteral("Ctrl+Alt+C"))));
    restore.findChild<QAction*>(QStringLiteral("DisableShortcut.editorConstants"))->trigger();
    QVERIFY(restore.overrides().contains(QStringLiteral("editorConstants")));
    QVERIFY(restore.overrides().value(QStringLiteral("editorConstants")).isEmpty());
    restore.findChild<QAction*>(QStringLiteral("RestoreShortcutDefault.editorConstants"))->trigger();
    QVERIFY(restore.overrides().isEmpty());
    restore.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    QCOMPARE(restored.count(), 1);
    QVERIFY(qvariant_cast<KeyBindings::Overrides>(restored.at(0).at(0)).isEmpty());
}

void TestKeyBindingsUi::focus_navigation_does_not_keep_a_removed_default()
{
    Settings::instance()->keyBindings.insert(QStringLiteral("navigationNextFocus"),
                                            {QStringLiteral("Ctrl+Alt+K, Ctrl+Alt+F")});
    MainWindow window(false);
    showForInput(window);
    Editor* editor = window.findChild<Editor*>();
    editor->setFocus();
    QTRY_VERIFY(editor->hasFocus());
    QSignalSpy triggered(windowAction(window, QStringLiteral("navigationNextFocus")), &QAction::triggered);
    QTest::keyClick(editor, Qt::Key_F6);
    QCOMPARE(triggered.count(), 0);
    QTest::keyClick(editor, Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
    QTest::keyClick(editor, Qt::Key_F, Qt::ControlModifier | Qt::AltModifier);
    QCOMPARE(triggered.count(), 1);
}

void TestKeyBindingsUi::reset_all_settings_restores_shortcuts_in_open_windows()
{
    Settings::instance()->keyBindings.insert(QStringLiteral("sessionOpen"), {QStringLiteral("Ctrl+Alt+O")});
    Settings::instance()->keyBindings.insert(QStringLiteral("editorConstants"), {});
    MainWindow first(false);
    MainWindow second(false);
    QTimer::singleShot(0, &first, []() {
        auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        dialog->button(QMessageBox::Yes)->click();
    });
    windowAction(first, QStringLiteral("settingsResetAll"))->trigger();
    QVERIFY(Settings::instance()->keyBindings.isEmpty());
    for (MainWindow* window : {&first, &second}) {
        QCOMPARE(windowAction(*window, QStringLiteral("sessionOpen"))->shortcuts(),
                 QKeySequence::keyBindings(QKeySequence::Open));
        Editor* editor = window->findChild<Editor*>();
        QCOMPARE(editor->findChild<QAction*>(QStringLiteral("KeyBinding.editorConstants"))->shortcut(),
                 QKeySequence(Qt::CTRL | Qt::Key_Space));
    }
}

void TestKeyBindingsUi::reset_clears_session_definitions_data()
{
    QTest::addColumn<bool>("confirm");
    QTest::addColumn<bool>("definitionsOnly");
    QTest::newRow("confirm") << true << false;
    QTest::newRow("cancel") << false << false;
    QTest::newRow("definitions-only") << true << true;
}

void TestKeyBindingsUi::reset_clears_session_definitions()
{
    QFETCH(bool, confirm);
    QFETCH(bool, definitionsOnly);
    PrivateSessionStorage storage;
    Settings* settings = Settings::instance();
    settings->startupUserDefinitions = QStringLiteral("sharedvalue=99");
    settings->skipClearSessionConfirmation = true;
    settings->autoCalc = false;
    UserDefinitions::saveFrom(settings);
    MainWindow window(false);
    auto* editor = window.findChild<Editor*>();
    auto* display = window.findChild<ResultDisplay*>();
    QVERIFY(editor && display);
    for (const QString& expression : {QStringLiteral("localvalue=12"),
                                     QStringLiteral("localfunc(x)=x+1"),
                                     QStringLiteral("[localunit]=[metre]")}) {
        editor->setText(expression);
        QVERIFY(QMetaObject::invokeMethod(&window, "evaluateEditorExpression"));
    }
    const Session* session = display->session();
    QVERIFY(session->hasVariable(QStringLiteral("localvalue")));
    QCOMPARE(session->UserFunctionsToList().size(), 1);
    QCOMPARE(session->userUnitsToList().size(), 1);
    if (definitionsOnly) {
        QAction* clear = windowAction(window, QStringLiteral("sessionClear"));
        QVERIFY(clear);
        QCOMPARE(KeyBindings::plainText(clear->text()), QStringLiteral("Clear History"));
        clear->trigger();
        QCOMPARE(session->historySize(), 0);
        QVERIFY(session->hasVariable(QStringLiteral("localvalue")));
        QCOMPARE(session->UserFunctionsToList().size(), 1);
        QCOMPARE(session->userUnitsToList().size(), 1);
    } else {
        editor->setText(QStringLiteral("unfinished +"));
    }
    const int historyBefore = session->historySize();
    const QString inputBefore = editor->text();
    bool asked = false;
    QTimer::singleShot(0, &window, [&]() {
        auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        const auto close = qScopeGuard([&]() { dialog->done(confirm ? QMessageBox::Yes : QMessageBox::No); });
        asked = true;
        QVERIFY(dialog->text().startsWith(QStringLiteral("Reset this session's calculations")));
        QCOMPARE(dialog->defaultButton(), dialog->button(QMessageBox::No));
    });
    QAction* reset = windowAction(window, QStringLiteral("editClearHistory"));
    QVERIFY(reset);
    QCOMPARE(KeyBindings::plainText(reset->text()), QStringLiteral("Reset"));
    reset->trigger();
    QVERIFY(asked);
    QCOMPARE(session->historySize(), confirm ? 0 : historyBefore);
    QCOMPARE(editor->text(), confirm ? QString() : inputBefore);
    QCOMPARE(session->hasVariable(QStringLiteral("localvalue")), !confirm);
    QCOMPARE(session->UserFunctionsToList().size(), confirm ? 0 : 1);
    QCOMPARE(session->userUnitsToList().size(), confirm ? 0 : 1);
    QVERIFY(session->hasVariable(QStringLiteral("sharedvalue")));
}

void TestKeyBindingsUi::reset_refreshes_definition_docks()
{
    PrivateSessionStorage storage;
    Settings::instance()->autoCalc = false;
    MainWindow window(false);
    showForInput(window);
    auto* editor = window.findChild<Editor*>();
    QVERIFY(editor);
    for (const QString& expression : {QStringLiteral("localvalue=12"),
                                     QStringLiteral("localfunc(x)=x+1"),
                                     QStringLiteral("[localunit]=[metre]")}) {
        editor->setText(expression);
        QVERIFY(QMetaObject::invokeMethod(&window, "evaluateEditorExpression"));
    }
    const QStringList setters = {QStringLiteral("setVariablesDockVisible"),
        QStringLiteral("setUserFunctionsDockVisible"), QStringLiteral("setUserUnitsDockVisible")};
    const QStringList names = {QStringLiteral("VariablesDock"), QStringLiteral("UserFunctionsDock"),
                               QStringLiteral("UserUnitsDock")};
    QList<QDockWidget*> docks;
    for (int index = 0; index < setters.size(); ++index) {
        QVERIFY(QMetaObject::invokeMethod(&window, setters.at(index).toLatin1().constData(),
                                         Q_ARG(bool, true), Q_ARG(bool, false)));
        auto* dock = window.findChild<QDockWidget*>(names.at(index));
        QVERIFY2(dock, qPrintable(names.at(index)));
        docks.append(dock);
        dock->raise();
        auto* list = dock->findChild<QTreeWidget*>();
        QVERIFY(list);
        QTRY_VERIFY(list->topLevelItemCount() > 0);
    }
    QTimer::singleShot(0, &window, []() {
        auto* confirmation = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(confirmation);
        confirmation->button(QMessageBox::Yes)->click();
    });
    windowAction(window, QStringLiteral("editClearHistory"))->trigger();
    for (QDockWidget* dock : docks) {
        dock->raise();
        auto* list = dock->findChild<QTreeWidget*>();
        QVERIFY(list);
        QTRY_COMPARE(list->topLevelItemCount(), 0);
    }
}

int main(int argc, char** argv)
{
    return UiTestFixture::run<TestKeyBindingsUi>(argc, argv);
}

#include "testkeybindingsui.moc"
