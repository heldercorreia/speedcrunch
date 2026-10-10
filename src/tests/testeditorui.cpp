// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/editor.h"
#include "gui/editorutils.h"
#include "gui/oklchutils.h"
#include "gui/syntaxhighlighter.h"
#include "gui/uiconfig.h"
#include "core/colorscheme.h"
#include "core/evaluator.h"
#include "core/session.h"
#include "core/settings.h"
#include "core/unicodechars.h"
#include "core/mathdsl.h"
#include "core/numberformatter.h"
#include "core/units.h"
#include "core/userfunction.h"
#include "core/userunit.h"
#include "uitestfixture.h"

#include <QApplication>
#include <QClipboard>
#include <QInputMethodEvent>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QPalette>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QTextLayout>
#include <QElapsedTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWindow>

namespace {
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
}

class TestEditorUi : public QObject {
    Q_OBJECT

private slots:
    void init() { UiTestFixture::resetSettings(); }
    void blocks_consecutive_plus();
    void respects_keyboard_layout_for_equals_and_plus_data();
    void respects_keyboard_layout_for_equals_and_plus();
    void wraps_left_shift_operator_and_blocks_immediate_duplicate_less_than();
    void wraps_right_shift_operator_and_blocks_immediate_duplicate_greater_than();
    void blocks_consecutive_caret();
    void blocks_plus_after_caret();
    void blocks_leading_operators_when_auto_ans_is_off_except_configured_exceptions();
    void keeps_disallowed_start_chars_blocked_when_auto_ans_is_on();
    void allows_special_function_symbols_as_leading_chars();
    void allows_list_start_after_operators();
    void highlights_list_braces_as_parentheses();
    void input_digit_grouping_formats_only_integer_digits_data();
    void input_digit_grouping_formats_only_integer_digits();
    void input_digit_grouping_preserves_typing_selection_and_undo();
    void input_digit_grouping_survives_settings_reload();
    void input_digit_grouping_follows_number_format_data();
    void input_digit_grouping_follows_number_format();
    void input_digit_grouping_paints_separator_glyphs_data();
    void input_digit_grouping_paints_separator_glyphs();
    void auto_ans_rewrite_helper_handles_tilde_and_factorial();
    void blocks_operator_right_after_open_square_bracket();
    void inserts_value_unit_space_brackets_after_number_or_symbol();
    void inserts_closing_square_bracket_only_with_spaces_to_right_data();
    void inserts_closing_square_bracket_only_with_spaces_to_right();
    void ignores_space_on_empty_or_all_space_editor();
    void auto_inserts_space_before_question_comment_only_with_non_space_content();
    void inserts_middle_dot_on_space_after_identifier_or_closed_group();
    void inserts_parenthesis_pair_and_places_cursor_inside();
    void does_not_insert_implicit_mul_sequence_before_open_paren_after_number_or_symbol();
    void does_not_insert_implicit_mul_for_zero_radix_prefix_letters();
    void allows_unit_conversion_tail_after_spaced_subtraction_operator();
    void converts_double_minus_sequence_to_unit_conversion_with_placeholder();
    void converts_double_minus_sequence_to_unit_conversion_after_masculine_ordinal_degree();
    void inserts_unit_conversion_with_placeholder_when_typing_arrow_symbol();
    void treats_spaced_unit_conversion_as_atomic_navigation_and_edit_token();
    void treats_spaced_shift_operators_as_atomic_navigation_and_edit_tokens();
    void backspace_removes_typed_operator_after_unit();
    void editing_keys_bypass_typing_restrictions_data();
    void editing_keys_bypass_typing_restrictions();
    void undo_redo_shortcuts_preserve_expression_data();
    void undo_redo_shortcuts_preserve_expression();
    void typing_undoes_one_key_at_a_time_data();
    void typing_undoes_one_key_at_a_time();
    void non_printable_key_payloads_do_not_replace_selection();
    void clipboard_shortcuts_do_not_insert_key_payloads_data();
    void clipboard_shortcuts_do_not_insert_key_payloads();
    void treats_spaced_question_comment_as_atomic_navigation_and_edit_token();
    void treats_spaced_equal_as_atomic_navigation_and_edit_token();
    void treats_leading_question_comment_as_atomic_navigation_and_edit_token();
    void ignores_space_right_after_spaced_unit_conversion_operator();
    void unit_bracket_context_accepts_div_mul_and_rejects_addition();
    void unit_bracket_context_blocks_space_and_ops_when_only_spaces();
    void unit_bracket_context_accepts_quote_marks_as_arc_units();
    void unit_bracket_context_accepts_quote_marks_via_ime_commit();
    void unit_bracket_context_allows_letter_after_middle_dot();
    void unit_bracket_context_disallows_variables_and_constants();
    void evaluator_accepts_compact_arc_symbol_units();
    void unit_bracket_context_allows_digits_and_minus_only_in_exponent_positions();
    void converts_caret_exponents_to_superscripts_globally();
    void keeps_scientific_notation_exponent_minus_unwrapped();
    void blocks_shift_operator_tail_after_non_plus_operator();
    void rewrites_superscript_exponent_for_radix_and_inserts_mul_space_globally();
    void typing_asterisk_after_superscript_power_keeps_explicit_multiplication();
    void auto_inserts_zero_before_dot_in_configured_contexts();
    void allows_unrestricted_typing_inside_question_comment_context();
    void allows_currency_symbols_after_operators();
    void blocks_dead_circumflex_key_after_existing_caret();
    void blocks_dead_circumflex_key_after_multiplication_operator();
    void blocks_regular_caret_after_multiplication_operator();
    void blocks_ime_preedit_caret_echo_after_existing_caret();
    void blocks_ime_commit_caret_after_existing_caret();
    void blocks_ime_commit_caret_after_multiplication_operator();
    void finds_completion_keyword_after_superscript_power();
    void completes_replacing_trailing_identifier_after_superscript_power();
    void completes_identifier_immediately_after_digit();
    void completes_pi_identifier_as_pi_symbol();
    void accepts_degree_alias_in_unit_brackets_and_normalizes_to_degree_sign();
    void offers_unit_completion_for_degree_symbol_in_unit_context();
    void matches_micro_units_when_typing_u_in_unit_context();
    void unit_context_completion_excludes_user_variables();
    void unit_context_completion_excludes_built_in_functions();
    void unit_context_completion_excludes_user_functions();
    void unit_context_completion_excludes_built_in_variables();
    void unit_context_completion_includes_angle_units_and_long_forms();
    void unit_context_completion_includes_small_positive_si_prefixed_symbols();
    void unit_context_completion_excludes_prefixed_square_and_cubic_metre();
    void completes_binary_prefixed_information_unit_to_short_form_in_unit_context();
    void completes_day_unit_to_short_form_in_unit_context();
    void completes_hour_unit_to_short_form_in_unit_context();
    void completes_affine_temperature_units_in_unit_context();
    void completes_arc_units_in_unit_context();
    void tooltip_does_not_duplicate_degree_symbol_for_explicit_angle_conversion();
    void tooltip_does_not_append_angle_mode_symbol_after_explicit_arcsecond_unit();
    void tooltip_compacts_bracketed_arcminute_and_arcsecond_expression_units();
    void tooltip_rewrites_composite_canonical_angle_symbols_to_aliases();
    void tooltip_shows_radian_suffix_for_negative_sexagesimal_literal();
    void tooltip_trig_output_does_not_append_angle_mode_suffix();
    void tooltip_shows_interpreted_expression_for_non_trig_sexagesimal_expression();
    void tooltip_shows_simplified_line_for_repeated_trig_with_degree_sign();
    void tooltip_shows_simplified_line_for_mixed_revolution_aliases();
    void tooltip_uses_cross_for_literal_number_products_in_simplified_line();
    void tooltip_keeps_quantsp_before_degree_celsius();
    void tooltip_handles_affine_temperature_units_without_arrow_and_with_conversion();
    void tooltip_shows_selection_result_when_selecting_with_shift_arrows();
    void tooltip_shows_selection_result_when_selecting_all_with_keyboard_shortcut();
    void tooltip_shows_selection_result_when_selecting_with_mouse();
    void tooltip_does_not_refresh_current_result_on_caret_arrow_move();
    void tooltip_does_not_refresh_current_result_on_mouse_caret_reposition();
    void tooltip_refreshes_current_result_on_char_deletion();
    void enter_evaluates_when_completion_popup_has_no_explicit_interaction();
    void completion_popup_arrow_keys_change_selection();
    void completion_popup_tab_accepts_selected_item();
    void completion_popup_closes_when_clicking_elsewhere();
    void completion_popup_clicking_row_accepts_item();
    void completion_popup_mouse_selection_keeps_focus_on_owning_editor();
    void completion_popup_scrollbar_click_keeps_popup_open();
    void completion_popup_mouse_wheel_scrolls_popup();
    void completion_popup_escape_closes_and_stays_closed();
    void completion_popup_escape_keeps_focus_on_owning_editor();
    void completion_popup_reopens_after_typing_following_escape();
    void completion_popup_reopens_after_retyping_deleted_letter_data();
    void completion_popup_reopens_after_retyping_deleted_letter();
    void completion_popup_shows_for_two_character_prefixes();
    void completion_popup_uses_positionable_nonactivating_surface();
    void completion_popup_keeps_editor_widgets_non_native_data();
    void completion_popup_keeps_editor_widgets_non_native();
    void completion_popup_follows_editor_geometry_data();
    void completion_popup_follows_editor_geometry();
    void completion_popup_geometry_avoids_editor_overlap_data();
    void completion_popup_geometry_avoids_editor_overlap();
    void completion_popup_shortens_tall_lists_without_covering_editor();
    void completion_popups_configure_editor_anchor_data();
    void completion_popups_configure_editor_anchor();
    void completion_popup_compositor_geometry_data();
    void completion_popup_compositor_geometry();
    void constant_completion_pages_follow_popup_resize();
    void constant_completion_popup_follows_wrapped_editor_cursor();
    void completion_popup_uses_configured_surface_colors();
    void constant_completion_popup_uses_configured_surface_colors();
    void inactive_editor_does_not_show_completion_popup();
    void enter_evaluates_when_cursor_is_immediately_after_operator();
    void completion_popup_uses_expected_icons_for_all_symbol_types();
    void wrap_selection_method_wraps_selected_text();
    void wrap_selection_method_wraps_whole_expression_without_selection();
    void matching_parentheses_use_parens_and_generated_foreground_colors();
    void keeps_wrapped_cursor_line_visible_at_height_cap();
    void editor_height_adds_only_one_line_height_per_visible_line();
    void editor_keeps_unit_descenders_inside_viewport();
    void adding_second_wrapped_character_keeps_first_line_visible();
    void editor_fill_color_is_15_percent_lighter_for_dark_background_role();
    void editor_fill_color_is_15_percent_darker_for_light_background_role();
    void themed_cursor_has_only_two_states_at_display_scale_data();
    void themed_cursor_has_only_two_states_at_display_scale();
    void cursor_free_painting_preserves_text_and_selections_data();
    void cursor_free_painting_preserves_text_and_selections();
};

static QTreeWidget* s_completionPopupTree()
{
    const auto topLevelWidgets = QApplication::topLevelWidgets();
    for (QWidget* widget : topLevelWidgets) {
        if (!widget || !widget->isVisible())
            continue;
        if (widget->objectName() != QStringLiteral("editorCompletionPopup"))
            continue;
        if (QTreeWidget* tree = qobject_cast<QTreeWidget*>(widget))
            return tree;
    }

    QWidget* popup = QApplication::activePopupWidget();
    if (popup) {
        if (QTreeWidget* tree = qobject_cast<QTreeWidget*>(popup))
            return tree;
    }
    for (QWidget* widget : topLevelWidgets) {
        if (!widget || !widget->isVisible())
            continue;
        if (!(widget->windowFlags() & (Qt::Popup | Qt::Tool))
            && widget->objectName() != QStringLiteral("editorCompletionPopup"))
            continue;
        if (QTreeWidget* tree = qobject_cast<QTreeWidget*>(widget))
            return tree;
    }
    return nullptr;
}

static QString s_popupSymbolForIdentifier(QTreeWidget* popup, const QString& identifier)
{
    if (!popup)
        return QString();
    for (int i = 0; i < popup->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = popup->topLevelItem(i);
        if (item && item->text(1) == identifier)
            return item->text(0);
    }
    return QString();
}

static QTreeWidget* s_completionPopupTreeContaining(const QString& identifierPrefix)
{
    const auto topLevelWidgets = QApplication::topLevelWidgets();
    for (QWidget* widget : topLevelWidgets) {
        if (!widget || !widget->isVisible())
            continue;
        if (widget->objectName() != QStringLiteral("editorCompletionPopup"))
            continue;
        QTreeWidget* tree = qobject_cast<QTreeWidget*>(widget);
        if (!tree)
            continue;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = tree->topLevelItem(i);
            if (item && item->text(1).startsWith(identifierPrefix))
                return tree;
        }
    }
    return nullptr;
}

static QList<QTreeWidget*> s_constantCompletionPopupTrees()
{
    QList<QTreeWidget*> trees;
    QWidget* popup = QApplication::activePopupWidget();
    if (!popup || popup->objectName() != QStringLiteral("constantCompletionPopup"))
        return trees;

    trees = popup->findChildren<QTreeWidget*>();
    std::sort(trees.begin(), trees.end(), [](QTreeWidget* left, QTreeWidget* right) {
        return left->objectName() < right->objectName();
    });
    return trees;
}

static bool s_editorOrViewportHasFocus(Editor* editor)
{
    QWidget* focusWidget = QApplication::focusWidget();
    return editor->hasFocus()
        || editor->viewport()->hasFocus()
        || focusWidget == editor
        || focusWidget == editor->viewport();
}


void TestEditorUi::blocks_consecutive_plus()
{
    // State: "1"
    // Action: type '+' twice.
    // Expected: second '+' is ignored.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1"));
    editor.setCursorPosition(editor.text().size());

    QTest::keyClick(&editor, Qt::Key_Plus, Qt::NoModifier);
    const QString afterFirstPlus = editor.text();
    QVERIFY(afterFirstPlus.contains(MathDsl::AddOp));

    QTest::keyClick(&editor, Qt::Key_Plus, Qt::NoModifier);
    QCOMPARE(editor.text(), afterFirstPlus);
}

void TestEditorUi::respects_keyboard_layout_for_equals_and_plus_data()
{
    QTest::addColumn<int>("key");
    QTest::addColumn<Qt::KeyboardModifiers>("modifiers");
    QTest::addColumn<QString>("payload");
    QTest::addColumn<QString>("initial");
    QTest::addColumn<QString>("expected");

    QTest::newRow("portuguese-shift-equals")
        << int(Qt::Key_Equal) << Qt::KeyboardModifiers(Qt::ShiftModifier)
        << QStringLiteral("=") << QStringLiteral("foo") << QStringLiteral("foo = ");
    QTest::newRow("shift-zero-equals-payload")
        << int(Qt::Key_0) << Qt::KeyboardModifiers(Qt::ShiftModifier)
        << QStringLiteral("=") << QStringLiteral("foo") << QStringLiteral("foo = ");
    QTest::newRow("unshifted-equals")
        << int(Qt::Key_Equal) << Qt::KeyboardModifiers(Qt::NoModifier)
        << QStringLiteral("=") << QStringLiteral("foo") << QStringLiteral("foo = ");
    QTest::newRow("us-shift-equals-plus-payload")
        << int(Qt::Key_Equal) << Qt::KeyboardModifiers(Qt::ShiftModifier)
        << QStringLiteral("+") << QStringLiteral("1") << QStringLiteral("1 + ");
    QTest::newRow("plus-key")
        << int(Qt::Key_Plus) << Qt::KeyboardModifiers(Qt::NoModifier)
        << QStringLiteral("+") << QStringLiteral("1") << QStringLiteral("1 + ");
    QTest::newRow("shift-equals-without-payload")
        << int(Qt::Key_Equal) << Qt::KeyboardModifiers(Qt::ShiftModifier)
        << QString() << QStringLiteral("foo") << QStringLiteral("foo = ");
    QTest::newRow("shift-equals-blocks-duplicate")
        << int(Qt::Key_Equal) << Qt::KeyboardModifiers(Qt::ShiftModifier)
        << QStringLiteral("=") << QStringLiteral("foo = ") << QStringLiteral("foo = ");
    QTest::newRow("shift-plus-blocks-duplicate")
        << int(Qt::Key_Equal) << Qt::KeyboardModifiers(Qt::ShiftModifier)
        << QStringLiteral("+") << QStringLiteral("1 + ") << QStringLiteral("1 + ");
}

void TestEditorUi::respects_keyboard_layout_for_equals_and_plus()
{
    QFETCH(int, key);
    QFETCH(Qt::KeyboardModifiers, modifiers);
    QFETCH(QString, payload);
    QFETCH(QString, initial);
    QFETCH(QString, expected);

    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();
    editor.setText(initial);
    editor.setCursorPosition(editor.text().size());

    QKeyEvent event(QEvent::KeyPress, key, modifiers, payload);
    QApplication::sendEvent(&editor, &event);
    QCOMPARE(editor.document()->toRawText(), expected);
}

void TestEditorUi::wraps_left_shift_operator_and_blocks_immediate_duplicate_less_than()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("foo"));
    editor.setCursorPosition(editor.text().size());

    QKeyEvent lessByText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("<"));
    QApplication::sendEvent(&editor, &lessByText);
    const QString afterFirstLess = editor.text();
    QCOMPARE(afterFirstLess, QStringLiteral("foo") + MathDsl::buildWrappedToken(MathDsl::ShiftLeftOp));

    QApplication::sendEvent(&editor, &lessByText);
    QCOMPARE(editor.text(), afterFirstLess);
}

void TestEditorUi::wraps_right_shift_operator_and_blocks_immediate_duplicate_greater_than()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("foo"));
    editor.setCursorPosition(editor.text().size());

    QKeyEvent greaterByText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral(">"));
    QApplication::sendEvent(&editor, &greaterByText);
    const QString afterFirstGreater = editor.text();
    QCOMPARE(afterFirstGreater, QStringLiteral("foo") + MathDsl::buildWrappedToken(MathDsl::ShiftRightOp));

    QApplication::sendEvent(&editor, &greaterByText);
    QCOMPARE(editor.text(), afterFirstGreater);
}

void TestEditorUi::blocks_consecutive_caret()
{
    // State: "2"
    // Action: type '^' twice.
    // Expected: second '^' is ignored.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("2"));
    editor.setCursorPosition(editor.text().size());

    QTest::keyClicks(&editor, QStringLiteral("^"));
    const QString afterFirstCaret = editor.text();
    QVERIFY(afterFirstCaret.contains(QLatin1Char('^')));

    QTest::keyClicks(&editor, QStringLiteral("^"));
    QCOMPARE(editor.text(), afterFirstCaret);
}

void TestEditorUi::blocks_plus_after_caret()
{
    // State: "2^"
    // Action: type '+'.
    // Expected: '+' is ignored.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("2^"));
    editor.setCursorPosition(editor.text().size());

    QTest::keyClick(&editor, Qt::Key_Plus, Qt::NoModifier);
    QCOMPARE(editor.text(), QStringLiteral("2^"));
}

void TestEditorUi::blocks_leading_operators_when_auto_ans_is_off_except_configured_exceptions()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const bool autoAnsBackup = settings->autoAns;
    settings->autoAns = false;

    const auto resetEmpty = [&editor]() {
        editor.setText(QString());
        editor.setCursorPosition(0);
    };
    const auto sendTextKey = [&editor](const QString& s) {
        QKeyEvent byText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, s);
        QApplication::sendEvent(&editor, &byText);
    };
    const auto sendImeCommit = [&editor](const QString& s) {
        QList<QInputMethodEvent::Attribute> attrs;
        QInputMethodEvent imeEvent(QString(), attrs);
        imeEvent.setCommitString(s);
        QApplication::sendEvent(&editor, &imeEvent);
    };

    resetEmpty();
    QTest::keyClick(&editor, Qt::Key_Plus, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QString());
    resetEmpty();
    sendTextKey(QString(MathDsl::AddOp));
    QCOMPARE(editor.document()->toRawText(), QString());

    resetEmpty();
    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QString());
    resetEmpty();
    sendTextKey(QString(MathDsl::MulCrossOp));
    QCOMPARE(editor.document()->toRawText(), QString());
    resetEmpty();
    sendTextKey(QString(MathDsl::MulDotOp));
    QCOMPARE(editor.document()->toRawText(), QString());

    resetEmpty();
    QTest::keyClick(&editor, Qt::Key_Slash, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QString());
    resetEmpty();
    sendTextKey(QString(MathDsl::DivOp));
    QCOMPARE(editor.document()->toRawText(), QString());

    resetEmpty();
    sendTextKey(QStringLiteral("!"));
    QCOMPARE(editor.document()->toRawText(), QString());
    resetEmpty();
    sendTextKey(QStringLiteral("^"));
    QCOMPARE(editor.document()->toRawText(), QString());
    resetEmpty();
    sendTextKey(QStringLiteral(")"));
    QCOMPARE(editor.document()->toRawText(), QString());
    resetEmpty();
    sendTextKey(QStringLiteral(":"));
    QCOMPARE(editor.document()->toRawText(), QString());

    resetEmpty();
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QVERIFY(!editor.document()->toRawText().isEmpty());
    const QString afterFirstLeadingMinus = editor.document()->toRawText();
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), afterFirstLeadingMinus);
    editor.setCursorPosition(0);
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), afterFirstLeadingMinus);
    resetEmpty();
    sendTextKey(QString(MathDsl::SubOp));
    QVERIFY(!editor.document()->toRawText().isEmpty());
    const QString afterFirstLeadingMinusByImeBase = editor.document()->toRawText();
    sendImeCommit(QString(MathDsl::SubOp));
    QCOMPARE(editor.document()->toRawText(), afterFirstLeadingMinusByImeBase);

    resetEmpty();
    QTest::keyClicks(&editor, QStringLiteral("~"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("~"));

    resetEmpty();
    sendTextKey(QStringLiteral("("));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("()"));
    QCOMPARE(editor.textCursor().position(), 1);

    resetEmpty();
    sendTextKey(QStringLiteral("'"));
    QCOMPARE(editor.document()->toRawText(), QString());

    resetEmpty();
    sendTextKey(QStringLiteral("\""));
    QCOMPARE(editor.document()->toRawText(), QString());

    resetEmpty();
    sendTextKey(QStringLiteral("#"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("#"));

    resetEmpty();
    sendTextKey(QStringLiteral("?"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("? "));

    resetEmpty();
    sendTextKey(QString::fromUtf8("€"));
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("€"));

    resetEmpty();
    sendTextKey(QString::fromUtf8("Ж"));
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("Ж"));

    settings->autoAns = autoAnsBackup;
}

void TestEditorUi::keeps_disallowed_start_chars_blocked_when_auto_ans_is_on()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const bool autoAnsBackup = settings->autoAns;
    settings->autoAns = true;

    const auto resetEmpty = [&editor]() {
        editor.setText(QString());
        editor.setCursorPosition(0);
    };
    const auto sendImeCommit = [&editor](const QString& s) {
        QList<QInputMethodEvent::Attribute> attrs;
        QInputMethodEvent imeEvent(QString(), attrs);
        imeEvent.setCommitString(s);
        QApplication::sendEvent(&editor, &imeEvent);
    };

    // Allowed starters in auto-ans mode.
    resetEmpty();
    sendImeCommit(QStringLiteral("!"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("!"));
    resetEmpty();
    sendImeCommit(QStringLiteral("~"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("~"));

    // Disallowed symbols still stay blocked.
    resetEmpty();
    sendImeCommit(QString::fromUtf8("§"));
    QCOMPARE(editor.document()->toRawText(), QString());
    resetEmpty();
    sendImeCommit(QStringLiteral("]"));
    QCOMPARE(editor.document()->toRawText(), QString());

    settings->autoAns = autoAnsBackup;
}

void TestEditorUi::allows_special_function_symbols_as_leading_chars()
{
    const bool autoAnsOff = false;
    const bool autoAnsOn = true;
    const QList<QChar> allowedStarters = {
        UnicodeChars::Summation,
        UnicodeChars::SquareRoot,
        UnicodeChars::CubeRoot,
        UnicodeChars::LowLine,
        UnicodeChars::DollarSign,
        MathDsl::ListStart,
        QChar(0x0436), // Ж
        QChar(0x03C0)  // π
    };
    for (const QChar ch : allowedStarters) {
        QVERIFY(EditorUtils::isAllowedLeadingCharAtExpressionStart(ch, autoAnsOff));
        QVERIFY(EditorUtils::isAllowedLeadingCharAtExpressionStart(ch, autoAnsOn));
    }

    QVERIFY(!EditorUtils::isAllowedLeadingCharAtExpressionStart(QChar(0x00A7), autoAnsOff)); // §
    QVERIFY(!EditorUtils::isAllowedLeadingCharAtExpressionStart(QChar(0x00A7), autoAnsOn)); // §
}

void TestEditorUi::allows_list_start_after_operators()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QString listStart(MathDsl::ListStart);
    const QVector<QPair<QString, QString>> cases = {
        {QStringLiteral("1+"), QStringLiteral("1+")},
        {QStringLiteral("1-"), QStringLiteral("1") + QString(MathDsl::SubOp)},
        {QStringLiteral("1/"), QStringLiteral("1/")},
        {QStringLiteral("1*"), QStringLiteral("1") + QString(MathDsl::MulCrossOp)},
        {QStringLiteral("1^"), QStringLiteral("1^")}
    };
    for (const auto& testCase : cases) {
        editor.setText(testCase.first);
        editor.setCursorPosition(editor.text().size());
        QKeyEvent openListByText(
            QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, listStart);
        QApplication::sendEvent(&editor, &openListByText);
        QCOMPARE(editor.document()->toRawText(), testCase.second + listStart);
    }
}

void TestEditorUi::highlights_list_braces_as_parentheses()
{
    Settings* settings = Settings::instance();
    const bool oldSyntaxHighlighting = settings->syntaxHighlighting;
    settings->syntaxHighlighting = true;

    QPlainTextEdit editor;
    SyntaxHighlighter highlighter(&editor);
    QJsonObject colors;
    colors.insert(QStringLiteral("number"), QStringLiteral("#111111"));
    colors.insert(QStringLiteral("parens"), QStringLiteral("#123456"));
    colors.insert(QStringLiteral("list"), QStringLiteral("#123456"));
    colors.insert(QStringLiteral("operator"), QStringLiteral("#222222"));
    colors.insert(QStringLiteral("separator"), QStringLiteral("#333333"));
    highlighter.setColorScheme(ColorScheme(QJsonDocument(themeJson(colors))));

    editor.setPlainText(QStringLiteral("{1; 2}"));
    highlighter.rehighlight();

    const QList<QTextLayout::FormatRange> formats =
        editor.document()->firstBlock().layout()->formats();
    auto colorAt = [&formats](int pos) {
        for (const QTextLayout::FormatRange& range : formats) {
            if (pos >= range.start && pos < range.start + range.length)
                return range.format.foreground().color();
        }
        return QColor();
    };

    const QColor parensColor(QStringLiteral("#123456"));
    QCOMPARE(colorAt(0), parensColor);
    QCOMPARE(colorAt(5), parensColor);

    settings->syntaxHighlighting = oldSyntaxHighlighting;
}

void TestEditorUi::auto_ans_rewrite_helper_handles_tilde_and_factorial()
{
    const auto modeTilde = EditorUtils::autoAnsRewriteModeForLeadingOperator(QStringLiteral("~"));
    QCOMPARE(modeTilde, EditorUtils::AutoAnsAppendAns);
    QCOMPARE(EditorUtils::applyAutoAnsRewrite(QStringLiteral("~"), modeTilde),
             QStringLiteral("~ans"));

    const auto modeFactorial = EditorUtils::autoAnsRewriteModeForLeadingOperator(QStringLiteral("!"));
    QCOMPARE(modeFactorial, EditorUtils::AutoAnsPrependAns);
    QCOMPARE(EditorUtils::applyAutoAnsRewrite(QStringLiteral("!"), modeFactorial),
             QStringLiteral("ans!"));
}

void TestEditorUi::blocks_operator_right_after_open_square_bracket()
{
    // State: "["
    // Action: type '-'.
    // Expected: operator is ignored at start of unit-bracket context.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("["));
    editor.setCursorPosition(editor.text().size());

    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QCOMPARE(editor.text(), QStringLiteral("["));
}

void TestEditorUi::inserts_value_unit_space_brackets_after_number_or_symbol()
{
    // State: "2" and "π"
    // Action: type '[' via text-based key event.
    // Expected: append "<ValueUnitSpace>[]".
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("2"));
    editor.setCursorPosition(editor.text().size());

    QKeyEvent bracketByText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("["));
    QApplication::sendEvent(&editor, &bracketByText);

    const QString actualAfterNumber = editor.document()->toRawText();
    QCOMPARE(actualAfterNumber, QStringLiteral("2") + QString(MathDsl::QuantSp) + QStringLiteral("[]"));

    editor.setText(QString::fromUtf8("π"));
    editor.setCursorPosition(editor.text().size());

    QApplication::sendEvent(&editor, &bracketByText);

    const QString actualAfterSymbol = editor.document()->toRawText();
    QCOMPARE(actualAfterSymbol, QString::fromUtf8("π") + QString(MathDsl::QuantSp) + QStringLiteral("[]"));

    editor.setText(QStringLiteral("2")
                   + QString(MathDsl::QuantSp)
                   + QStringLiteral("[m]")
                   + QString(MathDsl::MulDotWrapSp)
                   + QString(MathDsl::MulDotOp)
                   + QString(MathDsl::MulDotWrapSp)
                   + QStringLiteral("(pi)"));
    editor.setCursorPosition(editor.text().size());
    QApplication::sendEvent(&editor, &bracketByText);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("2")
                 + QString(MathDsl::QuantSp)
                 + QStringLiteral("[m]")
                 + QString(MathDsl::MulDotWrapSp)
                 + QString(MathDsl::MulDotOp)
                 + QString(MathDsl::MulDotWrapSp)
                 + QStringLiteral("(pi)")
                 + QString(MathDsl::QuantSp)
                 + QStringLiteral("[]"));

    editor.setText(QStringLiteral("2 [K] in "));
    editor.setCursorPosition(editor.text().size());
    QApplication::sendEvent(&editor, &bracketByText);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("2 [K] in")
                 + QString(MathDsl::QuantSp)
                 + QStringLiteral("[]"));
}

void TestEditorUi::inserts_closing_square_bracket_only_with_spaces_to_right_data()
{
    QTest::addColumn<QString>("initialText");
    QTest::addColumn<int>("cursorPosition");
    QTest::addColumn<QString>("expectedText");
    QTest::addColumn<int>("expectedCursorPosition");

    const QString unitSpace = QString(MathDsl::QuantSp);
    QTest::newRow("empty") << QString() << 0 << QStringLiteral("[]") << 1;
    QTest::newRow("end") << QStringLiteral("2") << 1
        << QStringLiteral("2") + unitSpace + QStringLiteral("[]") << 3;
    QTest::newRow("before-trailing-spaces") << QStringLiteral("2   ") << 1
        << QStringLiteral("2") + unitSpace + QStringLiteral("[]   ") << 3;
    QTest::newRow("after-trailing-spaces") << QStringLiteral("2   ") << 4
        << QStringLiteral("2") + unitSpace + QStringLiteral("[]") << 3;
    QTest::newRow("before-unicode-space") << QStringLiteral("2") + unitSpace << 1
        << QStringLiteral("2") + unitSpace + QStringLiteral("[]") + unitSpace << 3;
    QTest::newRow("before-operator") << QStringLiteral("2+3") << 1
        << QStringLiteral("2") + unitSpace + QStringLiteral("[+3") << 3;
    QTest::newRow("before-unit-and-existing-close") << QStringLiteral("2m]") << 1
        << QStringLiteral("2") + unitSpace + QStringLiteral("[m]") << 3;
    QTest::newRow("before-existing-close") << QStringLiteral("2]") << 1
        << QStringLiteral("2") + unitSpace + QStringLiteral("[]") << 3;
    QTest::newRow("inside-spaces-before-content") << QStringLiteral("2  +3") << 2
        << QStringLiteral("2") + unitSpace + QStringLiteral("[ +3") << 3;
    QTest::newRow("start-before-content") << QStringLiteral("2") << 0
        << QStringLiteral("[2") << 1;
    QTest::newRow("after-closed-group") << QStringLiteral("(2)+3") << 3
        << QStringLiteral("(2)") + unitSpace + QStringLiteral("[+3") << 5;
}

void TestEditorUi::inserts_closing_square_bracket_only_with_spaces_to_right()
{
    QFETCH(QString, initialText);
    QFETCH(int, cursorPosition);
    QFETCH(QString, expectedText);
    QFETCH(int, expectedCursorPosition);

    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    for (const int key : {Qt::Key_unknown, Qt::Key_BracketLeft}) {
        editor.setText(initialText);
        editor.setCursorPosition(cursorPosition);
        QKeyEvent openBracket(QEvent::KeyPress, key, Qt::NoModifier,
                              key == Qt::Key_unknown ? QStringLiteral("[") : QString());
        QApplication::sendEvent(&editor, &openBracket);
        QCOMPARE(editor.document()->toRawText(), expectedText);
        QCOMPARE(editor.textCursor().position(), expectedCursorPosition);
    }
}

void TestEditorUi::ignores_space_on_empty_or_all_space_editor()
{
    // State: empty editor and whitespace-only editor.
    // Action: type SPACE.
    // Expected: first leading space inserts; repeated space after space is ignored.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QString());
    editor.setCursorPosition(0);
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QCOMPARE(editor.text(), QStringLiteral(" "));

    const QString allSpaces =
        QStringLiteral(" ")
        + QString(MathDsl::QuantSp)
        + QStringLiteral(" ");
    editor.setText(allSpaces);
    editor.setCursorPosition(editor.text().size());
    const QString beforeSpaceOnAllSpaces = editor.document()->toRawText();
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), beforeSpaceOnAllSpaces);
}

void TestEditorUi::auto_inserts_space_before_question_comment_only_with_non_space_content()
{
    // State: content with non-space, empty, and whitespace-only.
    // Action: type '?'.
    // Expected: " ? " only when non-space already exists.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("2+2"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("?"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("2+2 ? "));

    editor.setText(QString());
    editor.setCursorPosition(0);
    QTest::keyClicks(&editor, QStringLiteral("?"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("? "));

    editor.setText(QStringLiteral("   "));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("?"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("   ? "));

    editor.setText(QStringLiteral("2+2"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent questionByText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("?"));
    QApplication::sendEvent(&editor, &questionByText);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("2+2 ? "));
}

void TestEditorUi::inserts_middle_dot_on_space_after_identifier_or_closed_group()
{
    Settings* settings = Settings::instance();
    const bool autoCompletionBackup = settings->autoCompletion;
    const bool builtInFnBackup = settings->autoCompletionBuiltInFunctions;
    const bool builtInVarBackup = settings->autoCompletionBuiltInVariables;
    const bool userFnBackup = settings->autoCompletionUserFunctions;
    const bool userVarBackup = settings->autoCompletionUserVariables;
    settings->autoCompletion = true;
    settings->autoCompletionBuiltInFunctions = false;
    settings->autoCompletionBuiltInVariables = true;
    settings->autoCompletionUserFunctions = false;
    settings->autoCompletionUserVariables = false;
    struct AutoCompletionRestoreGuard {
        Settings* settings;
        bool autoCompletion;
        bool builtInFunctions;
        bool builtInVariables;
        bool userFunctions;
        bool userVariables;
        ~AutoCompletionRestoreGuard()
        {
            settings->autoCompletion = autoCompletion;
            settings->autoCompletionBuiltInFunctions = builtInFunctions;
            settings->autoCompletionBuiltInVariables = builtInVariables;
            settings->autoCompletionUserFunctions = userFunctions;
            settings->autoCompletionUserVariables = userVariables;
        }
    } autoCompletionRestoreGuard {
        settings,
        autoCompletionBackup,
        builtInFnBackup,
        builtInVarBackup,
        userFnBackup,
        userVarBackup};

    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("p"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    const QString completedIdentifier = editor.document()->toRawText();
    const QString piSymbol = QString(UnicodeChars::Pi);
    QVERIFY(completedIdentifier == QStringLiteral("p ")
        || completedIdentifier == QStringLiteral("pi ")
        || completedIdentifier == piSymbol + QStringLiteral(" "));

    editor.setText(QStringLiteral("cos(3)"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("cos(3) "));
}

void TestEditorUi::inserts_parenthesis_pair_and_places_cursor_inside()
{
    // State: multiple states (empty, numbers, before operator, function name).
    // Action: type '(' (text path and keycode path).
    // Expected: auto-insert "()" only when right side is empty/spaces-only.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();
    const QString groupStart = QString(MathDsl::GroupStart);
    const QString groupEnd = QString(MathDsl::GroupEnd);
    const QString groupPair = groupStart + groupEnd;

    const auto verifyParenInsertionAt = [&editor](const QString& initialText, int cursorPos) {
        editor.setText(initialText);
        editor.setCursorPosition(cursorPos);

        QKeyEvent openParenByText(
            QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(MathDsl::GroupStart));
        QApplication::sendEvent(&editor, &openParenByText);

        const QString after = editor.document()->toRawText();
        const int cursor = editor.textCursor().position();

        QVERIFY(cursor > 0);
        QVERIFY(cursor < after.size());
        QCOMPARE(after.at(cursor - 1), QLatin1Char('('));
        QCOMPARE(after.at(cursor), QLatin1Char(')'));
    };

    verifyParenInsertionAt(QString(), 0);
    verifyParenInsertionAt(QStringLiteral("2"), 1);
    verifyParenInsertionAt(QStringLiteral("1   "), 1);  // only spaces to the right

    editor.setText(QStringLiteral("1+2"));
    editor.setCursorPosition(1);
    QKeyEvent openParenByText(
        QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(MathDsl::GroupStart));
    QApplication::sendEvent(&editor, &openParenByText);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("1(+2"));
    QCOMPARE(editor.textCursor().position(), 2);

    editor.setText(QStringLiteral("1)"));
    editor.setCursorPosition(1);
    QApplication::sendEvent(&editor, &openParenByText);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("1()"));
    QCOMPARE(editor.textCursor().position(), 2);

    editor.setText(QStringLiteral("cos"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent openParenByTextForFunction(
        QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(MathDsl::GroupStart));
    QApplication::sendEvent(&editor, &openParenByTextForFunction);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("cos") + groupPair);
    QCOMPARE(editor.textCursor().position(), 4);

    editor.setText(QString::fromUtf8("cos³"));
    editor.setCursorPosition(editor.text().size());
    QApplication::sendEvent(&editor, &openParenByTextForFunction);
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("cos³") + groupPair);
    QCOMPARE(editor.textCursor().position(), 5);

    editor.setText(QString::fromUtf8("2 pi⁻²³ · cos"));
    editor.setCursorPosition(editor.text().size());
    QApplication::sendEvent(&editor, &openParenByTextForFunction);
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("2 pi⁻²³ · cos") + groupPair);
    QCOMPARE(editor.textCursor().position(),
             QString::fromUtf8("2 pi⁻²³ · cos(").size());

    Session session;
    Evaluator* evaluator = session.evaluator();
    editor.setSession(&session);
    evaluator->unsetAllUserFunctions();
    evaluator->setUserFunction(UserFunction(
        QStringLiteral("user_fun"),
        QStringList() << QStringLiteral("x"),
        QStringLiteral("x")));
    editor.setText(QStringLiteral("user_fun"));
    editor.setCursorPosition(editor.text().size());
    QApplication::sendEvent(&editor, &openParenByTextForFunction);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("user_fun") + groupPair);
    QCOMPARE(editor.textCursor().position(), QStringLiteral("user_fun(").size());
    evaluator->unsetAllUserFunctions();

    editor.setText(QStringLiteral("3"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_ParenLeft, Qt::NoModifier);
    const QString afterKeyCodePath = editor.document()->toRawText();
    const int cursorAfterKeyCodePath = editor.textCursor().position();
    QVERIFY(cursorAfterKeyCodePath > 0);
    QVERIFY(cursorAfterKeyCodePath < afterKeyCodePath.size());
    QCOMPARE(afterKeyCodePath.at(cursorAfterKeyCodePath - 1), QLatin1Char('('));
    QCOMPARE(afterKeyCodePath.at(cursorAfterKeyCodePath), QLatin1Char(')'));
}

void TestEditorUi::does_not_insert_implicit_mul_sequence_before_open_paren_after_number_or_symbol()
{
    // State: "2" and "π".
    // Action: type '(' via text-based key event.
    // Expected: insert "()" without implicit multiplication prefix.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    auto typeGroupStartByText = [&editor]() {
        QKeyEvent openParenByText(
            QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("("));
        QApplication::sendEvent(&editor, &openParenByText);
    };

    editor.setText(QStringLiteral("2"));
    editor.setCursorPosition(editor.text().size());
    typeGroupStartByText();
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("2()"));

    editor.setText(QString::fromUtf8("2³"));
    editor.setCursorPosition(editor.text().size());
    typeGroupStartByText();
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("2³()"));

    editor.setText(QString::fromUtf8("π"));
    editor.setCursorPosition(editor.text().size());
    typeGroupStartByText();
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("π()"));

    editor.setText(QString::fromUtf8("pi³"));
    editor.setCursorPosition(editor.text().size());
    typeGroupStartByText();
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("pi³()"));
}

void TestEditorUi::does_not_insert_implicit_mul_for_zero_radix_prefix_letters()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("0"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("b"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("0b"));

    editor.setText(QStringLiteral("0"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("o"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("0o"));

    editor.setText(QStringLiteral("0"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("x"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("0x"));
}

void TestEditorUi::allows_unit_conversion_tail_after_spaced_subtraction_operator()
{
    // State: "1", then subtraction operator formatting.
    // Action: type GreaterThanSign via text-based key event.
    // Expected: convert to spaced "→ []", cursor inside brackets, parser sees UnitConversion.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);

    const QString afterMinus = editor.document()->toRawText();
    QVERIFY(afterMinus.contains(MathDsl::SubOp));

    QKeyEvent greaterByText(
        QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(UnicodeChars::GreaterThanSign));
    QApplication::sendEvent(&editor, &greaterByText);

    const QString afterGreater = editor.document()->toRawText();
    const QString arrowSequence =
        QString(MathDsl::SubWrapSp)
        + QString(MathDsl::TransOp)
        + QString(MathDsl::SubWrapSp)
        + QStringLiteral("[]");
    QVERIFY(afterGreater.contains(arrowSequence));
    QVERIFY(!afterGreater.contains(UnicodeChars::GreaterThanSign));
    QCOMPARE(editor.textCursor().position(), afterGreater.size() - 1);

    const Tokens tokens = Evaluator::instance()->scan(afterGreater);
    bool hasUnitConversion = false;
    for (const Token& token : tokens) {
        if (token.isOperator() && token.asOperator() == Token::UnitConversion) {
            hasUnitConversion = true;
            break;
        }
    }
    QVERIFY(hasUnitConversion);
}

void TestEditorUi::converts_double_minus_sequence_to_unit_conversion_with_placeholder()
{
    // State: "1".
    // Action: type '--'.
    // Expected: convert to spaced "→ []", cursor inside brackets, parser sees UnitConversion.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);

    const QString afterDoubleMinus = editor.document()->toRawText();
    const QString arrowSequence =
        QString(MathDsl::SubWrapSp)
        + QString(MathDsl::TransOp)
        + QString(MathDsl::SubWrapSp)
        + QStringLiteral("[]");
    QVERIFY(afterDoubleMinus.contains(arrowSequence));
    QCOMPARE(editor.textCursor().position(), afterDoubleMinus.size() - 1);

    const Tokens tokens = Evaluator::instance()->scan(afterDoubleMinus);
    bool hasUnitConversion = false;
    for (const Token& token : tokens) {
        if (token.isOperator() && token.asOperator() == Token::UnitConversion) {
            hasUnitConversion = true;
            break;
        }
    }
    QVERIFY(hasUnitConversion);
}

void TestEditorUi::converts_double_minus_sequence_to_unit_conversion_after_masculine_ordinal_degree()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QString conversion =
        QString(MathDsl::SubWrapSp)
        + QString(MathDsl::TransOp)
        + QString(MathDsl::SubWrapSp)
        + QStringLiteral("[]");
    const QPair<QString, QString> cases[] = {
        {QString::fromUtf8("12º"), QString::fromUtf8("12°") + conversion},
        {QString::fromUtf8("23°"), QString::fromUtf8("23°") + conversion},
        {QString::fromUtf8("23′"), QString::fromUtf8("23′") + conversion},
        {QString::fromUtf8("23″"), QString::fromUtf8("23″") + conversion}
    };

    for (const auto& tc : cases) {
        editor.setText(tc.first);
        editor.setCursorPosition(editor.text().size());
        QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
        QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);

        const QString actual = editor.document()->toRawText();
        QCOMPARE(actual, tc.second);
        QCOMPARE(editor.textCursor().position(), actual.size() - 1);
    }
}

void TestEditorUi::inserts_unit_conversion_with_placeholder_when_typing_arrow_symbol()
{
    // State: "1".
    // Action: type "→" directly.
    // Expected: insert " → []", with cursor inside brackets.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1"));
    editor.setCursorPosition(editor.text().size());

    QKeyEvent rightArrowByText(
        QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(MathDsl::TransOp));
    QApplication::sendEvent(&editor, &rightArrowByText);

    const QString expected =
        QStringLiteral("1")
        + QString(MathDsl::SubWrapSp)
        + QString(MathDsl::TransOp)
        + QString(MathDsl::SubWrapSp)
        + QStringLiteral("[]");
    QCOMPARE(editor.document()->toRawText(), expected);
    QCOMPARE(editor.textCursor().position(), expected.size() - 1);
}

void TestEditorUi::treats_spaced_unit_conversion_as_atomic_navigation_and_edit_token()
{
    // State: "1<space>→<space>2".
    // Action: use Left/Right, Alt+Left/Right, Backspace, Delete near token.
    // Expected: grouped " → " behaves as one atomic token.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QString arrowToken =
        QString(MathDsl::SubWrapSp)
        + QString(MathDsl::TransOp)
        + QString(MathDsl::SubWrapSp);
    const QString expression = QStringLiteral("1") + arrowToken + QStringLiteral("2");

    editor.setText(expression);
    editor.setCursorPosition(4); // between arrow and trailing space
    QTest::keyClick(&editor, Qt::Key_Left, Qt::NoModifier);
    QCOMPARE(editor.textCursor().position(), 1);
    QTest::keyClick(&editor, Qt::Key_Right, Qt::NoModifier);
    QCOMPARE(editor.textCursor().position(), 4);

    editor.setText(expression);
    editor.setCursorPosition(4); // right after grouped token
    QTest::keyClick(&editor, Qt::Key_Left, Qt::AltModifier);
    QCOMPARE(editor.textCursor().position(), 1);
    editor.setCursorPosition(1); // right before grouped token
    QTest::keyClick(&editor, Qt::Key_Right, Qt::AltModifier);
    QCOMPARE(editor.textCursor().position(), 4);

    editor.setText(expression);
    editor.setCursorPosition(4); // immediately after grouped token
    QTest::keyClick(&editor, Qt::Key_Backspace, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("12"));

    editor.setText(expression);
    editor.setCursorPosition(1); // immediately before grouped token
    QTest::keyClick(&editor, Qt::Key_Delete, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("12"));
}

void TestEditorUi::treats_spaced_shift_operators_as_atomic_navigation_and_edit_tokens()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QString leftShift = MathDsl::buildWrappedToken(MathDsl::ShiftLeftOp);
    const QString rightShift = MathDsl::buildWrappedToken(MathDsl::ShiftRightOp);
    const QString leftExpression = QStringLiteral("1") + leftShift + QStringLiteral("2");
    const QString rightExpression = QStringLiteral("1") + rightShift + QStringLiteral("2");

    editor.setText(leftExpression);
    editor.setCursorPosition(5); // right after grouped token
    QTest::keyClick(&editor, Qt::Key_Left, Qt::NoModifier);
    QCOMPARE(editor.textCursor().position(), 1);
    QTest::keyClick(&editor, Qt::Key_Right, Qt::NoModifier);
    QCOMPARE(editor.textCursor().position(), 5);
    QTest::keyClick(&editor, Qt::Key_Backspace, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("12"));

    editor.setText(leftExpression);
    editor.setCursorPosition(1); // right before grouped token
    QTest::keyClick(&editor, Qt::Key_Delete, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("12"));

    editor.setText(rightExpression);
    editor.setCursorPosition(5); // right after grouped token
    QTest::keyClick(&editor, Qt::Key_Left, Qt::NoModifier);
    QCOMPARE(editor.textCursor().position(), 1);
    QTest::keyClick(&editor, Qt::Key_Right, Qt::NoModifier);
    QCOMPARE(editor.textCursor().position(), 5);
    QTest::keyClick(&editor, Qt::Key_Backspace, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("12"));

    editor.setText(rightExpression);
    editor.setCursorPosition(1); // right before grouped token
    QTest::keyClick(&editor, Qt::Key_Delete, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("12"));
}

void TestEditorUi::backspace_removes_typed_operator_after_unit()
{
    Editor editor;
    QTest::keyClicks(&editor, QStringLiteral("2 [g"));
    QTest::keyClick(&editor, Qt::Key_Right); // Leave the auto-inserted unit brackets.
    QTest::keyClicks(&editor, QStringLiteral("/"));
    const QString quantity = QStringLiteral("2") + MathDsl::QuantSp + QStringLiteral("[g]");
    const QString division = MathDsl::buildWrappedToken(MathDsl::DivOp, MathDsl::DivWrap);
    QCOMPARE(editor.document()->toRawText(), quantity + division);

    // Native Backspace events carry a control character; QTest::keyClick
    // does not reproduce that payload on every platform.
    QKeyEvent backspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier,
                        QString(QChar(0x08)));
    QApplication::sendEvent(&editor, &backspace);
    QCOMPARE(editor.document()->toRawText(), quantity);
    QCOMPARE(editor.textCursor().position(), quantity.size());
}

void TestEditorUi::editing_keys_bypass_typing_restrictions_data()
{
    QTest::addColumn<QString>("expression");
    QTest::addColumn<int>("position");
    QTest::addColumn<int>("key");
    QTest::addColumn<QString>("payload");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<int>("expectedPosition");

    const QList<QChar> operators = {
        MathDsl::AddOp, MathDsl::SubOp, MathDsl::DivOp,
        MathDsl::MulCrossOp, MathDsl::PowOp
    };
    const QStringList prefixes = {
        QStringLiteral("2"),
        QStringLiteral("2") + MathDsl::QuantSp + QStringLiteral("[g]"),
        QStringLiteral("2") + MathDsl::QuantSp + QStringLiteral("[g")
    };
    for (int context = 0; context < prefixes.size(); ++context) {
        for (const QChar op : operators) {
            const QString token = context == 2 || op == MathDsl::PowOp
                ? QString(op) : MathDsl::buildWrappedToken(op);
            const QString expression = prefixes.at(context) + token;
            const QByteArray name = QStringLiteral("context-%1-op-%2")
                .arg(context).arg(uint(op.unicode())).toLatin1();
            QTest::newRow(QByteArray(name + "-backspace").constData())
                << expression << int(expression.size()) << int(Qt::Key_Backspace)
                << QString(QChar(0x08)) << prefixes.at(context) << int(prefixes.at(context).size());
            QTest::newRow(QByteArray(name + "-delete").constData())
                << expression + QStringLiteral("7") << int(expression.size()) << int(Qt::Key_Delete)
                << QString(QChar(0x7f)) << expression << int(expression.size());
            QTest::newRow(QByteArray(name + "-escape").constData())
                << expression << int(expression.size()) << int(Qt::Key_Escape)
                << QString(QChar(0x1b)) << expression << int(expression.size());
            QTest::newRow(QByteArray(name + "-tab").constData())
                << expression << int(expression.size()) << int(Qt::Key_Tab)
                << QString(QChar(0x09)) << expression << int(expression.size());
        }
    }
    QTest::newRow("delete-at-expression-start")
        << QStringLiteral("7") << 0 << int(Qt::Key_Delete)
        << QString(QChar(0x7f)) << QString() << 0;
    QTest::newRow("backspace-in-leading-whitespace")
        << QStringLiteral("  7") << 2 << int(Qt::Key_Backspace)
        << QString(QChar(0x08)) << QStringLiteral(" 7") << 1;
    QTest::newRow("escape-in-empty-editor")
        << QString() << 0 << int(Qt::Key_Escape)
        << QString(QChar(0x1b)) << QString() << 0;
}

void TestEditorUi::editing_keys_bypass_typing_restrictions()
{
    QFETCH(QString, expression);
    QFETCH(int, position);
    QFETCH(int, key);
    QFETCH(QString, payload);
    QFETCH(QString, expected);
    QFETCH(int, expectedPosition);

    Editor editor;
    editor.setText(expression);
    editor.setCursorPosition(position);
    QSignalSpy escapeSpy(&editor, SIGNAL(escapePressed()));
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier, payload);
    QApplication::sendEvent(&editor, &event);

    QCOMPARE(editor.document()->toRawText(), expected);
    QCOMPARE(editor.textCursor().position(), expectedPosition);
    QCOMPARE(escapeSpy.count(), key == Qt::Key_Escape ? 1 : 0);
}

void TestEditorUi::undo_redo_shortcuts_preserve_expression_data()
{
    QTest::addColumn<QString>("expression");
    QTest::addColumn<int>("position");
    QTest::addColumn<int>("selectionLength");
    QTest::addColumn<int>("shortcutPosition");
    QTest::addColumn<QString>("payload");

    const QStringList expressions = {
        QStringLiteral("123456"), QStringLiteral("12+34"),
        QStringLiteral("12[m/s]"), QString::fromUtf8("12°34′56″"),
        QStringLiteral("12 ? comment")
    };
    const QStringList payloads = {QString(), QString(QChar(0x1a)), QStringLiteral("z")};
    const QList<int> shortcutPositions = {3, 3, 5, 4, 7};
    for (int context = 0; context < expressions.size(); ++context) {
        for (int payload = 0; payload < payloads.size(); ++payload) {
            for (int selectionLength : {0, 2}) {
                const QByteArray name = QStringLiteral("context-%1-payload-%2-selection-%3")
                    .arg(context).arg(payload).arg(selectionLength).toLatin1();
                QTest::newRow(name.constData())
                    << expressions.at(context) << 2 << selectionLength
                    << shortcutPositions.at(context) << payloads.at(payload);
            }
        }
    }
}

void TestEditorUi::undo_redo_shortcuts_preserve_expression()
{
    QFETCH(QString, expression);
    QFETCH(int, position);
    QFETCH(int, selectionLength);
    QFETCH(int, shortcutPosition);
    QFETCH(QString, payload);

    const auto undoBindings = QKeySequence::keyBindings(QKeySequence::Undo);
    const auto redoBindings = QKeySequence::keyBindings(QKeySequence::Redo);
    QVERIFY(!undoBindings.isEmpty());
    QVERIFY(!redoBindings.isEmpty());
    for (const QKeySequence& undoBinding : undoBindings) {
        for (const QKeySequence& redoBinding : redoBindings) {
            Editor editor;
            editor.setAutoCompletionEnabled(false);
            editor.setText(expression);
            const QString original = editor.document()->toRawText();
            QTextCursor cursor = editor.textCursor();
            cursor.setPosition(position);
            cursor.setPosition(position + selectionLength, QTextCursor::KeepAnchor);
            editor.setTextCursor(cursor);
            QTest::keyClicks(&editor, QStringLiteral("9"));
            const QString edited = editor.document()->toRawText();
            QVERIFY(edited != original);
            editor.setCursorPosition(shortcutPosition);

            // Native shortcuts can carry a control character or printable text.
            const auto undoKey = undoBinding[0];
            QKeyEvent undoEvent(QEvent::KeyPress, undoKey.key(), undoKey.keyboardModifiers(), payload);
            QVERIFY(undoEvent.matches(QKeySequence::Undo));
            QApplication::sendEvent(&editor, &undoEvent);
            QCOMPARE(editor.document()->toRawText(), original);
            QVERIFY(editor.document()->isRedoAvailable());

            // Keep a selection active to catch accidental replacement by shortcut text.
            cursor = editor.textCursor();
            cursor.setPosition(0);
            cursor.setPosition(2, QTextCursor::KeepAnchor);
            editor.setTextCursor(cursor);
            const auto redoKey = redoBinding[0];
            const QString redoPayload = payload.isEmpty() ? QString()
                : payload.at(0).isPrint() ? QStringLiteral("z") : QString(QChar(0x19));
            QKeyEvent redoEvent(QEvent::KeyPress, redoKey.key(), redoKey.keyboardModifiers(), redoPayload);
            QVERIFY(redoEvent.matches(QKeySequence::Redo));
            QApplication::sendEvent(&editor, &redoEvent);
            QCOMPARE(editor.document()->toRawText(), edited);
            QApplication::sendEvent(&editor, &redoEvent);
            QCOMPARE(editor.document()->toRawText(), edited);

            QApplication::sendEvent(&editor, &undoEvent);
            QCOMPARE(editor.document()->toRawText(), original);
            QVERIFY(!editor.document()->isUndoAvailable());
            QApplication::sendEvent(&editor, &undoEvent);
            QCOMPARE(editor.document()->toRawText(), original);
            QVERIFY(editor.document()->isRedoAvailable());
        }
    }
}

void TestEditorUi::typing_undoes_one_key_at_a_time_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("inputMethod");
    for (bool inputMethod : {false, true}) {
        const QByteArray prefix = inputMethod ? "ime-" : "key-";
        QTest::newRow(QByteArray(prefix + "digits").constData()) << QStringLiteral("12345") << inputMethod;
        QTest::newRow(QByteArray(prefix + "operators").constData()) << QStringLiteral("12+34") << inputMethod;
        QTest::newRow(QByteArray(prefix + "units").constData()) << QStringLiteral("12[m/s]") << inputMethod;
        QTest::newRow(QByteArray(prefix + "degrees").constData()) << QString::fromUtf8("12º34'56\"") << inputMethod;
        QTest::newRow(QByteArray(prefix + "superscript-rewrite").constData()) << QStringLiteral("12^34") << inputMethod;
        if (!inputMethod) {
            QTest::newRow("key-unit-conversion-rewrite") << QStringLiteral("12--") << false;
            QTest::newRow("key-parenthesis-pair") << QStringLiteral("12+(34") << false;
        }
    }
}

void TestEditorUi::typing_undoes_one_key_at_a_time()
{
    QFETCH(QString, input);
    QFETCH(bool, inputMethod);
    Editor editor;
    editor.setAutoCompletionEnabled(false);
    editor.setText(QString());
    QStringList states = {editor.document()->toRawText()};
    for (const QChar ch : input) {
        if (inputMethod) {
            QInputMethodEvent event;
            event.setCommitString(QString(ch));
            QApplication::sendEvent(&editor, &event);
        } else if (ch.unicode() < 128) {
            QTest::keyClicks(&editor, QString(ch));
        } else {
            QKeyEvent event(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(ch));
            QApplication::sendEvent(&editor, &event);
        }
        const QString state = editor.document()->toRawText();
        if (state != states.last())
            states.append(state);
    }
    QVERIFY(states.size() > 2);
    for (int i = states.size() - 2; i >= 0; --i) {
        QVERIFY(editor.document()->isUndoAvailable());
        editor.undo();
        QCOMPARE(editor.document()->toRawText(), states.at(i));
    }
    QVERIFY(!editor.document()->isUndoAvailable());
    for (int i = 1; i < states.size(); ++i) {
        QVERIFY(editor.document()->isRedoAvailable());
        editor.redo();
        QCOMPARE(editor.document()->toRawText(), states.at(i));
    }
    QVERIFY(!editor.document()->isRedoAvailable());
}

void TestEditorUi::non_printable_key_payloads_do_not_replace_selection()
{
    for (ushort code : {0x01, 0x19, 0x1a, 0x7f}) {
        Editor editor;
        editor.setText(QStringLiteral("123456"));
        QTextCursor cursor = editor.textCursor();
        cursor.setPosition(2);
        cursor.setPosition(4, QTextCursor::KeepAnchor);
        editor.setTextCursor(cursor);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(QChar(code)));
        QApplication::sendEvent(&editor, &event);
        QCOMPARE(editor.document()->toRawText(), QStringLiteral("123456"));
        QCOMPARE(editor.textCursor().selectedText(), QStringLiteral("34"));
        QVERIFY(!editor.document()->isUndoAvailable());
    }
}

void TestEditorUi::clipboard_shortcuts_do_not_insert_key_payloads_data()
{
    QTest::addColumn<QString>("cutPayload");
    QTest::addColumn<QString>("pastePayload");
    QTest::newRow("empty") << QString() << QString();
    QTest::newRow("control-characters") << QString(QChar(0x18)) << QString(QChar(0x16));
    QTest::newRow("printable-letters") << QStringLiteral("x") << QStringLiteral("v");
}

void TestEditorUi::clipboard_shortcuts_do_not_insert_key_payloads()
{
    QFETCH(QString, cutPayload);
    QFETCH(QString, pastePayload);
    Editor editor;
    editor.setText(QStringLiteral("123456"));
    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(2);
    cursor.setPosition(4, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);
    const auto cutKey = QKeySequence(QKeySequence::Cut)[0];
    QKeyEvent cutEvent(QEvent::KeyPress, cutKey.key(), cutKey.keyboardModifiers(), cutPayload);
    QApplication::sendEvent(&editor, &cutEvent);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("1256"));
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("34"));

    const auto pasteKey = QKeySequence(QKeySequence::Paste)[0];
    QKeyEvent pasteEvent(QEvent::KeyPress, pasteKey.key(), pasteKey.keyboardModifiers(), pastePayload);
    QApplication::sendEvent(&editor, &pasteEvent);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("123456"));
    editor.undo();
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("1256"));
    editor.undo();
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("123456"));
}

void TestEditorUi::treats_spaced_question_comment_as_atomic_navigation_and_edit_token()
{
    // State: "1 ? 2".
    // Action: use Left/Right, Alt+Left/Right, Backspace, Delete near token.
    // Expected: grouped " ? " behaves as one atomic token.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QString commentToken = QStringLiteral(" ? ");
    const QString expression = QStringLiteral("1") + commentToken + QStringLiteral("2");

    editor.setText(expression);
    editor.setCursorPosition(4); // between '?' and trailing space
    QTest::keyClick(&editor, Qt::Key_Left, Qt::NoModifier);
    QCOMPARE(editor.textCursor().position(), 1);
    QTest::keyClick(&editor, Qt::Key_Right, Qt::NoModifier);
    QCOMPARE(editor.textCursor().position(), 4);

    editor.setText(expression);
    editor.setCursorPosition(4); // right after grouped token
    QTest::keyClick(&editor, Qt::Key_Left, Qt::AltModifier);
    QCOMPARE(editor.textCursor().position(), 1);
    editor.setCursorPosition(1); // right before grouped token
    QTest::keyClick(&editor, Qt::Key_Right, Qt::AltModifier);
    QCOMPARE(editor.textCursor().position(), 4);

    editor.setText(expression);
    editor.setCursorPosition(4); // immediately after grouped token
    QTest::keyClick(&editor, Qt::Key_Backspace, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("12"));

    editor.setText(expression);
    editor.setCursorPosition(1); // immediately before grouped token
    QTest::keyClick(&editor, Qt::Key_Delete, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("12"));
}

void TestEditorUi::treats_spaced_equal_as_atomic_navigation_and_edit_token()
{
    // State: "1", then type '=' with editor insertion rules.
    // Action: navigate/edit around grouped token.
    // Expected: grouped " = " behaves as one atomic token.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Equal, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("1 = "));

    editor.insert(QStringLiteral("2"));
    const QString expression = editor.document()->toRawText();
    QCOMPARE(expression, QStringLiteral("1 = 2"));

    editor.setCursorPosition(4); // right after grouped token
    QTest::keyClick(&editor, Qt::Key_Left, Qt::NoModifier);
    QCOMPARE(editor.textCursor().position(), 1);

    editor.setCursorPosition(1); // right before grouped token
    QTest::keyClick(&editor, Qt::Key_Right, Qt::NoModifier);
    QCOMPARE(editor.textCursor().position(), 4);

    editor.setCursorPosition(4); // immediately after grouped token
    QTest::keyClick(&editor, Qt::Key_Backspace, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("12"));

    editor.setText(expression);
    editor.setCursorPosition(1); // immediately before grouped token
    QTest::keyClick(&editor, Qt::Key_Delete, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("12"));
}

void TestEditorUi::treats_leading_question_comment_as_atomic_navigation_and_edit_token()
{
    // State: "? 2" (comment token at start of editor).
    // Action: use Left/Right, Alt+Left/Right, Backspace, Delete near token.
    // Expected: grouped "? " behaves as one atomic token.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QString expression = QStringLiteral("? 2");

    editor.setText(expression);
    editor.setCursorPosition(1); // between '?' and trailing space
    QTest::keyClick(&editor, Qt::Key_Left, Qt::NoModifier);
    QCOMPARE(editor.textCursor().position(), 0);
    QTest::keyClick(&editor, Qt::Key_Right, Qt::NoModifier);
    QCOMPARE(editor.textCursor().position(), 2);

    editor.setText(expression);
    editor.setCursorPosition(2); // right after grouped token
    QTest::keyClick(&editor, Qt::Key_Left, Qt::AltModifier);
    QCOMPARE(editor.textCursor().position(), 0);
    editor.setCursorPosition(0); // right before grouped token
    QTest::keyClick(&editor, Qt::Key_Right, Qt::AltModifier);
    QCOMPARE(editor.textCursor().position(), 2);

    editor.setText(expression);
    editor.setCursorPosition(2); // immediately after grouped token
    QTest::keyClick(&editor, Qt::Key_Backspace, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("2"));

    editor.setText(expression);
    editor.setCursorPosition(0); // immediately before grouped token
    QTest::keyClick(&editor, Qt::Key_Delete, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("2"));
}

void TestEditorUi::ignores_space_right_after_spaced_unit_conversion_operator()
{
    // State: after conversion to spaced "→ []" token with cursor inside [].
    // Action: type SPACE.
    // Expected: ignored because current [] unit context is empty.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QKeyEvent greaterByText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral(">"));
    QApplication::sendEvent(&editor, &greaterByText);

    const QString beforeSpace = editor.document()->toRawText();
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), beforeSpace);
}

void TestEditorUi::unit_bracket_context_accepts_div_mul_and_rejects_addition()
{
    // State: unmatched "[m" unit context.
    // Action: type '+', '/', and '*'.
    // Expected: '+' rejected; '/' and multiplication accepted.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("[m"));
    editor.setCursorPosition(editor.text().size());

    const QString beforePlus = editor.document()->toRawText();
    QTest::keyClick(&editor, Qt::Key_Plus, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), beforePlus);

    QTest::keyClick(&editor, Qt::Key_Slash, Qt::NoModifier);
    const QString afterSlash = editor.document()->toRawText();
    QVERIFY(afterSlash.contains(MathDsl::DivOp));

    editor.setText(QStringLiteral("[m"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);
    const QString afterMul = editor.document()->toRawText();
    QVERIFY(afterMul.contains(MathDsl::MulCrossOp)
            || afterMul.contains(MathDsl::MulDotOp));
}

void TestEditorUi::unit_bracket_context_blocks_space_and_ops_when_only_spaces()
{
    // State: unmatched "[   " and cursor at end.
    // Action: type SPACE and operator keys.
    // Expected: all are ignored while unit context has only whitespace.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QString onlySpaces = QStringLiteral("[   ");

    editor.setText(onlySpaces);
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), onlySpaces);

    editor.setText(onlySpaces);
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), onlySpaces);

    editor.setText(onlySpaces);
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Slash, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), onlySpaces);

    editor.setText(onlySpaces);
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), onlySpaces);

    editor.setText(onlySpaces);
    editor.setCursorPosition(editor.text().size());
    QKeyEvent shiftSlashByText(QEvent::KeyPress, Qt::Key_unknown, Qt::ShiftModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &shiftSlashByText);
    QCOMPARE(editor.document()->toRawText(), onlySpaces);

    editor.setText(onlySpaces);
    editor.setCursorPosition(editor.text().size());
    QKeyEvent keypadSlash(QEvent::KeyPress, Qt::Key_division, Qt::KeypadModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &keypadSlash);
    QCOMPARE(editor.document()->toRawText(), onlySpaces);
}

void TestEditorUi::unit_bracket_context_accepts_quote_marks_as_arc_units()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("["));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent apostropheByText(
        QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("'"));
    QApplication::sendEvent(&editor, &apostropheByText);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[") + QString(UnicodeChars::Prime));

    editor.setText(QStringLiteral("["));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent quoteByText(
        QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("\""));
    QApplication::sendEvent(&editor, &quoteByText);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[") + QString(UnicodeChars::DoublePrime));
}

void TestEditorUi::unit_bracket_context_accepts_quote_marks_via_ime_commit()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    QList<QInputMethodEvent::Attribute> imeAttributes;

    editor.setText(QStringLiteral("["));
    editor.setCursorPosition(editor.text().size());
    QInputMethodEvent apostropheCommit(QString(), imeAttributes);
    apostropheCommit.setCommitString(QStringLiteral("'"));
    QApplication::sendEvent(&editor, &apostropheCommit);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[") + QString(UnicodeChars::Prime));

    editor.setText(QStringLiteral("["));
    editor.setCursorPosition(editor.text().size());
    QInputMethodEvent quoteCommit(QString(), imeAttributes);
    quoteCommit.setCommitString(QStringLiteral("\""));
    QApplication::sendEvent(&editor, &quoteCommit);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[") + QString(UnicodeChars::DoublePrime));
}

void TestEditorUi::unit_bracket_context_allows_letter_after_middle_dot()
{
    // State: "2 [m·]" with cursor after '·'.
    // Action: type letter and then digit.
    // Expected: letter accepted; digit rejected.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("2 [m·]"));
    editor.setCursorPosition(5); // right after middle-dot, before ']'
    QTest::keyClicks(&editor, QStringLiteral("s"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("2 [m·s]"));

    editor.setText(QStringLiteral("2 [m·]"));
    editor.setCursorPosition(5); // right after middle-dot, before ']'
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("2 [m·]"));
}

void TestEditorUi::unit_bracket_context_disallows_variables_and_constants()
{
    // State: expressions "2[foo]", "2[e]", "2[pi]".
    // Action: evaluate with foo defined as variable.
    // Expected: all fail with "unknown unit".
    Session session;
    Evaluator* evaluator = session.evaluator();
    evaluator->setVariable(QStringLiteral("foo"), Quantity(3));

    evaluator->setExpression(QStringLiteral("2[foo]"));
    evaluator->evalUpdateAns();
    QVERIFY(evaluator->error().contains(QStringLiteral("unknown unit"), Qt::CaseInsensitive));

    evaluator->setExpression(QStringLiteral("2[e]"));
    evaluator->evalUpdateAns();
    QVERIFY(evaluator->error().contains(QStringLiteral("unknown unit"), Qt::CaseInsensitive));

    evaluator->setExpression(QStringLiteral("2[pi]"));
    evaluator->evalUpdateAns();
    QVERIFY(evaluator->error().contains(QStringLiteral("unknown unit"), Qt::CaseInsensitive));

    evaluator->unsetVariable(QStringLiteral("foo"));
}

void TestEditorUi::evaluator_accepts_compact_arc_symbol_units()
{
    Session session;
    Evaluator* evaluator = session.evaluator();
    const auto tokensDebug = [evaluator](const QString& expr) {
        const Tokens tokens = evaluator->scan(expr);
        QStringList parts;
        for (const Token& token : tokens) {
            parts.append(QStringLiteral("{t=%1,text='%2'}")
                .arg(static_cast<int>(token.type()))
                .arg(token.text()));
        }
        return QStringLiteral("valid=%1 tokens=%2")
            .arg(tokens.valid() ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(parts.join(QStringLiteral(", ")));
    };

    evaluator->setExpression(QString::fromUtf8("1 [′]"));
    evaluator->evalUpdateAns();
    QVERIFY2(evaluator->error().isEmpty(),
             qPrintable(QStringLiteral("Unexpected error for [′]: %1 (%2)")
                .arg(evaluator->error(), tokensDebug(QString::fromUtf8("1 [′]")))));

    evaluator->setExpression(QString::fromUtf8("1 [″]"));
    evaluator->evalUpdateAns();
    QVERIFY2(evaluator->error().isEmpty(),
             qPrintable(QStringLiteral("Unexpected error for [″]: %1 (%2)")
                .arg(evaluator->error(), tokensDebug(QString::fromUtf8("1 [″]")))));
}

void TestEditorUi::unit_bracket_context_allows_digits_and_minus_only_in_exponent_positions()
{
    // State: many "[m...]" exponent-denominator states.
    // Action: type digits, minus, slash, letters in each state.
    // Expected: only grammar-valid unit-exponent forms are accepted.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("[m"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m2"));

    editor.setText(QStringLiteral("[m^"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + MathDsl::Pow2);

    editor.setText(QStringLiteral("[m^"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("x"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^"));

    editor.setText(QStringLiteral("[m^"));
    editor.setCursorPosition(editor.text().size());
    QList<QInputMethodEvent::Attribute> imeAttributes;
    QInputMethodEvent imeLetterEvent(QString(), imeAttributes);
    imeLetterEvent.setCommitString(QStringLiteral("x"));
    QApplication::sendEvent(&editor, &imeLetterEvent);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^"));

    editor.setText(QStringLiteral("[m^("));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^(2"));

    editor.setText(QStringLiteral("[m^2"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("3"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^23"));

    editor.setText(QStringLiteral("[m^2"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + MathDsl::Pow2 + QString(MathDsl::MulDotOp));

    editor.setText(QStringLiteral("[s") + MathDsl::Pow2);
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[s") + MathDsl::Pow2 + QString(MathDsl::MulDotOp));

    editor.setText(QStringLiteral("[m^2"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashAfterPlainExponent(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashAfterPlainExponent);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + MathDsl::Pow2 + QString(MathDsl::DivOp));

    editor.setText(QStringLiteral("[m^2"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + MathDsl::Pow2 + QString(MathDsl::MulDotOp));

    editor.setText(QStringLiteral("[m^2"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("e"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^2"));

    editor.setText(QStringLiteral("[m"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("eter"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[meter"));

    editor.setText(QStringLiteral("[m"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_ParenLeft, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m()"));

    editor.setText(QStringLiteral("[m"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent nestedBracketByText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("["));
    QApplication::sendEvent(&editor, &nestedBracketByText);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m"));

    editor.setText(QStringLiteral("[m"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent closeBracketByText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("]"));
    QApplication::sendEvent(&editor, &closeBracketByText);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m]"));

    editor.setText(QStringLiteral("[s^222"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent closeBracketAfterExponent(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("]"));
    QApplication::sendEvent(&editor, &closeBracketAfterExponent);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[s") + MathDsl::Pow2 + MathDsl::Pow2 + MathDsl::Pow2 + QStringLiteral("]"));

    editor.setText(QStringLiteral("[m]"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent chainedBracketByText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("["));
    QApplication::sendEvent(&editor, &chainedBracketByText);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m]"));

    editor.setText(QStringLiteral("[m]"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_ParenLeft, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m]()"));

    editor.setText(QStringLiteral("[]")
                   + QString(MathDsl::MulDotWrapSp)
                   + QString(MathDsl::MulDotOp)
                   + QString(MathDsl::MulDotWrapSp)
                   + QStringLiteral("()"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_ParenLeft, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[]")
                 + QString(MathDsl::MulDotWrapSp)
                 + QString(MathDsl::MulDotOp)
                 + QString(MathDsl::MulDotWrapSp)
                 + QStringLiteral("()")
                 + QStringLiteral("()"));

    editor.setText(QStringLiteral("[m^(2"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("3"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^(23"));

    editor.setText(QStringLiteral("[m"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m"));

    editor.setText(QStringLiteral("[m^"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + MathDsl::PowNeg);

    editor.setText(QStringLiteral("[m^("));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^(") + QString(MathDsl::SubOp));

    editor.setText(QStringLiteral("[m") + MathDsl::PowNeg);
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + MathDsl::PowNeg + MathDsl::Pow2);

    editor.setText(QStringLiteral("[m^(") + QString(MathDsl::SubOp));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(MathDsl::SubOp) + QStringLiteral("2"));

    editor.setText(QStringLiteral("[m^(1"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText1(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText1);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^(1/"));

    // m^/ -> reject slash right after exponent start.
    editor.setText(QStringLiteral("[m^"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashAfterExponentStart(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashAfterExponentStart);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^"));

    // m⁻/ -> reject slash right after signed exponent start.
    editor.setText(QStringLiteral("[m") + MathDsl::PowNeg);
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashAfterSignedExponentStart(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashAfterSignedExponentStart);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + MathDsl::PowNeg);

    editor.setText(QStringLiteral("[m^(") + QString(MathDsl::SubOp) + QStringLiteral("1"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText2(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText2);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(MathDsl::SubOp) + QStringLiteral("1/"));
    QKeyEvent slashByText3(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText3);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(MathDsl::SubOp) + QStringLiteral("1/"));

    // m^2/3 -> reject digit after slash (non-parenthesized exponent).
    editor.setText(QStringLiteral("[m^2"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText4(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText4);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + MathDsl::Pow2 + QStringLiteral("/"));
    QTest::keyClicks(&editor, QStringLiteral("s"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + MathDsl::Pow2 + QStringLiteral("/s"));

    editor.setText(QStringLiteral("[m^3"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText4b(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText4b);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + MathDsl::Pow3 + QStringLiteral("/"));
    QTest::keyClicks(&editor, QStringLiteral("s"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + MathDsl::Pow3 + QStringLiteral("/s"));

    // m²·/ -> reject slash right after multiplication separator.
    editor.setText(QStringLiteral("[m") + MathDsl::Pow2 + QString(MathDsl::MulDotOp));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashAfterMulDot(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashAfterMulDot);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + MathDsl::Pow2 + QString(MathDsl::MulDotOp));

    editor.setText(QStringLiteral("[m^2"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText4c(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText4c);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + MathDsl::Pow2 + QStringLiteral("/"));
    QTest::keyClicks(&editor, QStringLiteral("3"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + MathDsl::Pow2 + QStringLiteral("/"));

    // m^/ + "/" -> trim invalid slash after exponent start.
    editor.setText(QStringLiteral("[m^/"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText4d(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText4d);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^"));

    // m^-2/3 -> reject digit after slash (non-parenthesized signed exponent).
    editor.setText(QStringLiteral("[m^") + QString(MathDsl::SubOp) + QStringLiteral("2"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText5(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText5);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + MathDsl::PowNeg + MathDsl::Pow2 + QStringLiteral("/"));
    QTest::keyClicks(&editor, QStringLiteral("3"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + MathDsl::PowNeg + MathDsl::Pow2 + QStringLiteral("/"));

    // m^(2/3 -> accept digit after slash (parenthesized exponent).
    editor.setText(QStringLiteral("[m^(2"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText6(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText6);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^(2/"));
    QTest::keyClicks(&editor, QStringLiteral("s"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^(2/"));
    QTest::keyClicks(&editor, QStringLiteral("3"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^(2/3"));

    // m².3 -> allow radix and following digits inside parenthesized exponent rewrite.
    editor.setText(QStringLiteral("[m") + MathDsl::Pow2);
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^(2.)"));
    QCOMPARE(editor.textCursor().position(),
             editor.document()->toRawText().size() - 1);
    QTest::keyClicks(&editor, QStringLiteral("3"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^(2.3)"));
    QCOMPARE(editor.textCursor().position(),
             editor.document()->toRawText().size() - 1);

    // m^(-2/3 -> accept digit after slash (parenthesized signed exponent).
    editor.setText(QStringLiteral("[m^(") + QString(MathDsl::SubOp) + QStringLiteral("2"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText7(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText7);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(MathDsl::SubOp) + QStringLiteral("2/"));
    QTest::keyClicks(&editor, QStringLiteral("s"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(MathDsl::SubOp) + QStringLiteral("2/"));
    QTest::keyClicks(&editor, QStringLiteral("3"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(MathDsl::SubOp) + QStringLiteral("2/3"));

    editor.setText(QStringLiteral("[m^(-2"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("3"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(MathDsl::SubOp) + QStringLiteral("23"));

    editor.setText(QStringLiteral("[m"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent deadTilde(QEvent::KeyPress, Qt::Key_Dead_Tilde, Qt::NoModifier);
    QApplication::sendEvent(&editor, &deadTilde);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m"));

    editor.setText(QStringLiteral("3 + "));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent deadTildeAfterPlus(QEvent::KeyPress, Qt::Key_Dead_Tilde, Qt::NoModifier);
    QApplication::sendEvent(&editor, &deadTildeAfterPlus);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("3 + ~"));

    editor.setText(QStringLiteral("3 + "));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent regularTildeAfterPlus(
        QEvent::KeyPress, Qt::Key_AsciiTilde, Qt::NoModifier, QStringLiteral("~"));
    QApplication::sendEvent(&editor, &regularTildeAfterPlus);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("3 + ~"));

    editor.setText(QStringLiteral("[m"));
    editor.setCursorPosition(editor.text().size());
    QList<QInputMethodEvent::Attribute> deadImeAttrs;
    QInputMethodEvent deadTildePreedit(QStringLiteral("~"), deadImeAttrs);
    QApplication::sendEvent(&editor, &deadTildePreedit);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m"));

    editor.setText(QStringLiteral("[s") + MathDsl::Pow2);
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("^"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[s") + MathDsl::Pow2);

    editor.setText(QStringLiteral("2")
                   + QString(MathDsl::QuantSp)
                   + QStringLiteral("[m/s")
                   + QString(MathDsl::MulDotOp));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent deadCircumflexInUnitAfterDot(QEvent::KeyPress, Qt::Key_Dead_Circumflex, Qt::NoModifier);
    QApplication::sendEvent(&editor, &deadCircumflexInUnitAfterDot);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("2")
                 + QString(MathDsl::QuantSp)
                 + QStringLiteral("[m/s")
                 + QString(MathDsl::MulDotOp));

    QList<QInputMethodEvent::Attribute> caretImeAttrs;
    QInputMethodEvent caretCommitAfterDot(QString(), caretImeAttrs);
    caretCommitAfterDot.setCommitString(QStringLiteral("^"));
    QApplication::sendEvent(&editor, &caretCommitAfterDot);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("2")
                 + QString(MathDsl::QuantSp)
                 + QStringLiteral("[m/s")
                 + QString(MathDsl::MulDotOp));

    QInputMethodEvent caretPreeditAfterDot(QString::fromUtf8("ˆ"), caretImeAttrs);
    QApplication::sendEvent(&editor, &caretPreeditAfterDot);
    QInputMethodEvent digitCommitAfterCaretPreedit(QString(), caretImeAttrs);
    digitCommitAfterCaretPreedit.setCommitString(QStringLiteral("2"));
    QApplication::sendEvent(&editor, &digitCommitAfterCaretPreedit);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("2")
                 + QString(MathDsl::QuantSp)
                 + QStringLiteral("[m/s")
                 + QString(MathDsl::MulDotOp)
                 + QStringLiteral("2"));

    editor.setText(QStringLiteral("2")
                   + QString(MathDsl::QuantSp)
                   + QStringLiteral("[m")
                   + MathDsl::Pow2
                   + QStringLiteral("/s"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);
    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("2")
                 + QString(MathDsl::QuantSp)
                 + QStringLiteral("[m")
                 + MathDsl::Pow2
                 + QStringLiteral("/s^"));
}

void TestEditorUi::converts_caret_exponents_to_superscripts_globally()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("s^"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("s") + MathDsl::Pow2);

    editor.setText(QStringLiteral("s^"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("s") + MathDsl::PowNeg + MathDsl::Pow2);

    editor.setText(QStringLiteral("[s^"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[s") + MathDsl::Pow2);

    editor.setText(QStringLiteral("s"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent deadCircumflex(QEvent::KeyPress, Qt::Key_Dead_Circumflex, Qt::NoModifier);
    QApplication::sendEvent(&editor, &deadCircumflex);
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("s") + MathDsl::Pow2);

    editor.setText(QStringLiteral("s"));
    editor.setCursorPosition(editor.text().size());
    QList<QInputMethodEvent::Attribute> imeAttrs;
    QInputMethodEvent imeWithCaretPreedit(QString::fromUtf8("ˆ"), imeAttrs);
    QApplication::sendEvent(&editor, &imeWithCaretPreedit);
    QInputMethodEvent imeDigitCommit(QString(), imeAttrs);
    imeDigitCommit.setCommitString(QStringLiteral("2"));
    QApplication::sendEvent(&editor, &imeDigitCommit);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("s") + MathDsl::Pow2);

    editor.setText(QStringLiteral("s") + MathDsl::Pow2);
    editor.setCursorPosition(editor.text().size());
    QInputMethodEvent imeCaretCommit(QString(), imeAttrs);
    imeCaretCommit.setCommitString(QStringLiteral("^"));
    QApplication::sendEvent(&editor, &imeCaretCommit);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("s") + MathDsl::Pow2);

    editor.setText(QStringLiteral("s") + MathDsl::Pow2);
    editor.setCursorPosition(editor.text().size());
    QInputMethodEvent imeCaretPreeditAfterSup(QString::fromUtf8("ˆ"), imeAttrs);
    QApplication::sendEvent(&editor, &imeCaretPreeditAfterSup);
    QInputMethodEvent imePowNegCommit(QString(), imeAttrs);
    imePowNegCommit.setCommitString(QString(MathDsl::PowNeg));
    QApplication::sendEvent(&editor, &imePowNegCommit);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("s") + MathDsl::Pow2);
}

void TestEditorUi::keeps_scientific_notation_exponent_minus_unwrapped()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1e"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("1e") + QString(MathDsl::SubOp));

    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("1e") + QString(MathDsl::SubOp) + QStringLiteral("2"));
}

void TestEditorUi::blocks_shift_operator_tail_after_non_plus_operator()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("foo -"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent lessByText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("<"));
    QApplication::sendEvent(&editor, &lessByText);
    QCOMPARE(editor.text(), QStringLiteral("foo ") + QString(MathDsl::SubOp));

    editor.setText(QStringLiteral("foo /"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent greaterByText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral(">"));
    QApplication::sendEvent(&editor, &greaterByText);
    QCOMPARE(editor.text(), QStringLiteral("foo /"));
}

void TestEditorUi::rewrites_superscript_exponent_for_radix_and_inserts_mul_space_globally()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("2"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("2 "));

    editor.setText(QString::fromUtf8("pi⁴"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Comma, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("pi^(4.)"));
    QCOMPARE(editor.textCursor().position(),
             editor.document()->toRawText().size() - 1);

    editor.setText(QString::fromUtf8("pi⁴"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("pi^(4.)"));
    QCOMPARE(editor.textCursor().position(),
             editor.document()->toRawText().size() - 1);

    editor.setText(QString::fromUtf8("pi⁴"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("pi⁴ "));

    editor.setText(QString::fromUtf8("2²"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("2² "));

    editor.setText(QString::fromUtf8("pi⁴"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);
    QVERIFY2(!editor.document()->toRawText().contains(QLatin1Char('^')),
             qPrintable(QStringLiteral("after first *: ") + editor.document()->toRawText()));
    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);
    QVERIFY2(!editor.document()->toRawText().contains(QLatin1Char('^')),
             qPrintable(editor.document()->toRawText()));
}

void TestEditorUi::typing_asterisk_after_superscript_power_keeps_explicit_multiplication()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QString multiply = QString(MathDsl::MulCrossWrapSp)
        + QString(MathDsl::MulCrossOp)
        + QString(MathDsl::MulCrossWrapSp);
    editor.setText(QStringLiteral("2")
                   + multiply
                   + QStringLiteral("3")
                   + multiply
                   + QStringLiteral("10")
                   + multiply
                   + QStringLiteral("10")
                   + QString(MathDsl::Pow2));
    editor.setCursorPosition(editor.text().size());

    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);

    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("2")
                 + multiply
                 + QStringLiteral("3")
                 + multiply
                 + QStringLiteral("10")
                 + multiply
                 + QStringLiteral("10")
                 + QString(MathDsl::Pow2)
                 + multiply);
    QVERIFY(!editor.document()->toRawText().contains(MathDsl::MulDotOp));
}

void TestEditorUi::auto_inserts_zero_before_dot_in_configured_contexts()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();
    const auto sendTextKey = [&editor](const QString& s) {
        QKeyEvent byText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, s);
        QApplication::sendEvent(&editor, &byText);
    };

    editor.setText(QString());
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("0."));

    editor.setText(QString());
    editor.setCursorPosition(editor.text().size());
    sendTextKey(QStringLiteral("."));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("0."));

    editor.setText(QStringLiteral("1 + "));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("1 + 0."));

    editor.setText(QStringLiteral("1 + "));
    editor.setCursorPosition(editor.text().size());
    sendTextKey(QStringLiteral("."));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("1 + 0."));

    editor.setText(QStringLiteral("   "));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("   0."));

    editor.setText(QStringLiteral("1 × "));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("1 × 0."));

    editor.setText(QStringLiteral("1 − "));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("1 − 0."));

    editor.setText(QStringLiteral("1 / "));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("1 / 0."));

    editor.setText(QStringLiteral(" (  ) "));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral(" (  ) × 0."));

    editor.setText(QStringLiteral("[x]   "));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[x]   × 0."));

    editor.setText(QStringLiteral("pi"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("pi × 0."));

    editor.setText(QStringLiteral("pi  "));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Comma, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("pi  × 0."));

    editor.setText(QStringLiteral("12"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("12."));

    editor.setText(QStringLiteral("0,"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Comma, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("0,"));
}

void TestEditorUi::allows_unrestricted_typing_inside_question_comment_context()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1 ? pi"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QTest::keyClick(&editor, Qt::Key_Comma, Qt::NoModifier);
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);

    QCOMPARE(editor.document()->toRawText(), QStringLiteral("1 ? pi.,  "));
}

void TestEditorUi::allows_currency_symbols_after_operators()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("$123 + "));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Dollar, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("$123 + $"));
}

void TestEditorUi::blocks_dead_circumflex_key_after_existing_caret()
{
    // State: "2^".
    // Action: send dead-circumflex key press.
    // Expected: event is consumed; no duplicate caret.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("2^"));
    editor.setCursorPosition(editor.text().size());

    QKeyEvent deadCircumflex(QEvent::KeyPress, Qt::Key_Dead_Circumflex, Qt::NoModifier);
    QApplication::sendEvent(&editor, &deadCircumflex);

    QCOMPARE(editor.text(), QStringLiteral("2^"));
}

void TestEditorUi::blocks_dead_circumflex_key_after_multiplication_operator()
{
    // State: "3 × ".
    // Action: send dead-circumflex key press.
    // Expected: event is consumed; no caret is inserted.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("3")
                   + QString(MathDsl::MulCrossOp)
                   + QStringLiteral(" "));
    editor.setCursorPosition(editor.text().size());

    QKeyEvent deadCircumflex(QEvent::KeyPress, Qt::Key_Dead_Circumflex, Qt::NoModifier);
    QApplication::sendEvent(&editor, &deadCircumflex);

    QCOMPARE(editor.text(),
             QStringLiteral("3")
                 + QString(MathDsl::MulCrossOp)
                 + QStringLiteral(" "));
}

void TestEditorUi::blocks_regular_caret_after_multiplication_operator()
{
    // State: "3 × ".
    // Action: type regular caret '^'.
    // Expected: caret is ignored.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("3")
                   + QString(MathDsl::MulCrossOp)
                   + QStringLiteral(" "));
    editor.setCursorPosition(editor.text().size());

    QTest::keyClicks(&editor, QStringLiteral("^"));

    QCOMPARE(editor.text(),
             QStringLiteral("3")
                 + QString(MathDsl::MulCrossOp)
                 + QStringLiteral(" "));
}

void TestEditorUi::blocks_ime_preedit_caret_echo_after_existing_caret()
{
    // State: "2^".
    // Action: send IME preedit caret echo.
    // Expected: preedit echo is ignored.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("2^"));
    editor.setCursorPosition(editor.text().size());

    QList<QInputMethodEvent::Attribute> attributes;
    QInputMethodEvent imeEvent(QString::fromUtf8("ˆ"), attributes);
    QApplication::sendEvent(&editor, &imeEvent);

    QCOMPARE(editor.text(), QStringLiteral("2^"));
}

void TestEditorUi::blocks_ime_commit_caret_after_existing_caret()
{
    // State: "2^".
    // Action: send IME commit "^".
    // Expected: committed caret is ignored.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("2^"));
    editor.setCursorPosition(editor.text().size());

    QList<QInputMethodEvent::Attribute> attributes;
    QInputMethodEvent imeEvent(QString(), attributes);
    imeEvent.setCommitString(QStringLiteral("^"));
    QApplication::sendEvent(&editor, &imeEvent);

    QCOMPARE(editor.text(), QStringLiteral("2^"));
}

void TestEditorUi::blocks_ime_commit_caret_after_multiplication_operator()
{
    // State: "3 × ".
    // Action: send IME commit "^".
    // Expected: committed caret is ignored.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("3")
                   + QString(MathDsl::MulCrossOp)
                   + QStringLiteral(" "));
    editor.setCursorPosition(editor.text().size());

    QList<QInputMethodEvent::Attribute> attributes;
    QInputMethodEvent imeEvent(QString(), attributes);
    imeEvent.setCommitString(QStringLiteral("^"));
    QApplication::sendEvent(&editor, &imeEvent);

    QCOMPARE(editor.text(),
             QStringLiteral("3")
                 + QString(MathDsl::MulCrossOp)
                 + QStringLiteral(" "));
}

void TestEditorUi::finds_completion_keyword_after_superscript_power()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QString::fromUtf8("pi²c"));
    editor.setCursorPosition(editor.text().size());

    const QString keyword = editor.getKeyword();
    QVERIFY(!keyword.isEmpty());
    QVERIFY(keyword.startsWith(QLatin1Char('c')));
}

void TestEditorUi::completes_replacing_trailing_identifier_after_superscript_power()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QString::fromUtf8("pi²") + QString(MathDsl::MulDotOp) + QStringLiteral("c"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("cos: Built-in function"))));

    QCOMPARE(
        editor.text(),
        QString::fromUtf8("pi²")
            + QString(MathDsl::MulDotOp)
            + QStringLiteral("cos()"));
}

void TestEditorUi::completes_identifier_immediately_after_digit()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("3c"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(&editor, "triggerAutoComplete", Qt::DirectConnection));
    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QVERIFY(!s_popupSymbolForIdentifier(popup, QStringLiteral("cos")).isEmpty());
    popup->hide();

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("cos: Built-in function"))));

    QCOMPARE(editor.text(), QStringLiteral("3cos()"));
}

void TestEditorUi::completes_pi_identifier_as_pi_symbol()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("p"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("pi: Archimedes' constant Pi"))));

    QCOMPARE(editor.text(), QString::fromUtf8("π"));
}

void TestEditorUi::accepts_degree_alias_in_unit_brackets_and_normalizes_to_degree_sign()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("100 ["));
    editor.setCursorPosition(editor.text().size());

    QTest::keyClicks(&editor, QString::fromUtf8("ºC"));
    QCOMPARE(editor.text(), QString::fromUtf8("100 [°C"));

    // QTest::keyClicks() rejects U+02DA in qasciikey, so verify via setText().
    editor.setText(QString::fromUtf8("100 [˚C"));
    QCOMPARE(editor.text(), QString::fromUtf8("100 [°C"));
}

void TestEditorUi::offers_unit_completion_for_degree_symbol_in_unit_context()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QString::fromUtf8("100 [°"));
    editor.setCursorPosition(editor.text().size());

    const auto localizedChoice = [](const QString& identifier, UnitId id) {
        return QStringLiteral("%1:%2").arg(identifier, tr(unitLocalizedName(id)));
    };
    const QStringList degreeChoices = editor.matchFragment(QString::fromUtf8("°"), true);
    QVERIFY(!degreeChoices.isEmpty());
    QVERIFY(degreeChoices.contains(localizedChoice(QString::fromUtf8("°"), UnitId::Degree)));
    QVERIFY(degreeChoices.contains(localizedChoice(QString::fromUtf8("°C"), UnitId::DegreeCelsius)));

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("u:Unit"))));
    QCOMPARE(editor.text(), QString::fromUtf8("100 [u"));
}

void TestEditorUi::matches_micro_units_when_typing_u_in_unit_context()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const auto localizedChoice = [](const QString& identifier) {
        return QStringLiteral("%1:%2").arg(
            identifier,
            tr(unitLocalizedIdentifierName(identifier).toUtf8().constData()));
    };
    const QStringList uChoices = editor.matchFragment(QStringLiteral("u"), true);
    QVERIFY(uChoices.contains(localizedChoice(QString::fromUtf8("µm"))));
    QVERIFY(uChoices.contains(localizedChoice(QString::fromUtf8("µs"))));
    QVERIFY(uChoices.contains(localizedChoice(QString::fromUtf8("µas"))));
    QVERIFY(!uChoices.contains(localizedChoice(QStringLiteral("uas"))));
}

void TestEditorUi::unit_context_completion_excludes_user_variables()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Session session;
    Evaluator* evaluator = session.evaluator();
    editor.setSession(&session);
    evaluator->setVariable(QStringLiteral("foo"), Quantity(3));

    const QStringList unitChoices = editor.matchFragment(QStringLiteral("fo"), true);
    for (const QString& choice : unitChoices)
        QVERIFY2(!choice.startsWith(QStringLiteral("foo:")), qPrintable(choice));

    evaluator->unsetVariable(QStringLiteral("foo"));
}

void TestEditorUi::unit_context_completion_excludes_built_in_functions()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QStringList choices = editor.matchFragment(QStringLiteral("si"), true);
    for (const QString& choice : choices)
        QVERIFY2(!choice.startsWith(QStringLiteral("sin:")), qPrintable(choice));
}

void TestEditorUi::unit_context_completion_excludes_user_functions()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Session session;
    Evaluator* evaluator = session.evaluator();
    editor.setSession(&session);
    evaluator->setUserFunction(UserFunction(
        QStringLiteral("foof"),
        QStringList() << QStringLiteral("x"),
        QStringLiteral("x")));

    const QStringList choices = editor.matchFragment(QStringLiteral("foo"), true);
    for (const QString& choice : choices)
        QVERIFY2(!choice.startsWith(QStringLiteral("foof:")), qPrintable(choice));

    evaluator->unsetUserFunction(QStringLiteral("foof"));
}

void TestEditorUi::unit_context_completion_excludes_built_in_variables()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QStringList ansChoices = editor.matchFragment(QStringLiteral("an"), true);
    const QStringList eChoices = editor.matchFragment(QStringLiteral("e"), true);
    const QStringList piChoices = editor.matchFragment(QStringLiteral("pi"), true);

    for (const QString& choice : ansChoices)
        QVERIFY2(!choice.startsWith(QStringLiteral("ans:")), qPrintable(choice));
    for (const QString& choice : eChoices)
        QVERIFY2(!choice.startsWith(QStringLiteral("e:")), qPrintable(choice));
    for (const QString& choice : piChoices)
        QVERIFY2(!choice.startsWith(QStringLiteral("pi:")), qPrintable(choice));
}

void TestEditorUi::unit_context_completion_includes_angle_units_and_long_forms()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const auto localizedChoice = [](const QString& identifier, UnitId id) {
        return QStringLiteral("%1:%2").arg(identifier, tr(unitLocalizedName(id)));
    };
    const auto descriptionFor = [](const QStringList& choices, const QString& identifier) {
        for (const QString& choice : choices) {
            if (choice.startsWith(identifier + QStringLiteral(":")))
                return choice.mid(identifier.size() + 1);
        }
        return QString();
    };
    const QStringList radChoices = editor.matchFragment(QStringLiteral("rad"), true);
    QVERIFY(radChoices.contains(localizedChoice(QStringLiteral("rad"), UnitId::Radian)));
    QVERIFY(radChoices.contains(localizedChoice(QStringLiteral("radian"), UnitId::Radian)));

    const QStringList arcChoices = editor.matchFragment(QStringLiteral("arc"), true);
    QVERIFY(arcChoices.contains(localizedChoice(QStringLiteral("arcminute"), UnitId::Arcminute)));
    QVERIFY(arcChoices.contains(localizedChoice(QStringLiteral("arcsecond"), UnitId::Arcsecond)));
    QVERIFY(arcChoices.contains(localizedChoice(QStringLiteral("arcsec"), UnitId::Arcsecond)));

    const QStringList masChoices = editor.matchFragment(QStringLiteral("mas"), true);
    QVERIFY(masChoices.contains(localizedChoice(QStringLiteral("mas"), UnitId::Milliarcsecond)));

    const QStringList uasChoices = editor.matchFragment(QStringLiteral("u"), true);
    QVERIFY(uasChoices.contains(localizedChoice(QString::fromUtf8("µas"), UnitId::Microarcsecond)));

    const QStringList milliArcChoices = editor.matchFragment(QStringLiteral("milliarc"), true);
    QVERIFY(milliArcChoices.contains(localizedChoice(QStringLiteral("milliarcsecond"), UnitId::Milliarcsecond)));

    const QStringList microArcChoices = editor.matchFragment(QStringLiteral("microarc"), true);
    QVERIFY(microArcChoices.contains(localizedChoice(QStringLiteral("microarcsecond"), UnitId::Microarcsecond)));

    const QStringList turnChoices = editor.matchFragment(QStringLiteral("turn"), true);
    QVERIFY(turnChoices.contains(localizedChoice(QStringLiteral("turn"), UnitId::Turn)));

    const QStringList gradChoices = editor.matchFragment(QStringLiteral("grad"), true);
    QVERIFY(gradChoices.contains(localizedChoice(QStringLiteral("gradian"), UnitId::Gradian)));

    const QStringList gonChoices = editor.matchFragment(QStringLiteral("gon"), true);
    QVERIFY(gonChoices.contains(localizedChoice(QStringLiteral("gon"), UnitId::Gradian)));

    const QStringList aChoices = editor.matchFragment(QStringLiteral("a"), true);
    const QString acDescription = descriptionFor(aChoices, QStringLiteral("ac"));
    const QString aCDescription = descriptionFor(aChoices, QStringLiteral("aC"));
    QVERIFY(!acDescription.isEmpty());
    QVERIFY(!aCDescription.isEmpty());
    QCOMPARE(acDescription, QStringLiteral("acre"));
    QVERIFY(aCDescription != QStringLiteral("acre"));
    QVERIFY(aCDescription != tr("Unit"));
}

void TestEditorUi::unit_context_completion_includes_small_positive_si_prefixed_symbols()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const auto hasCompletionIdentifier = [](const QStringList& choices, const QString& identifier) {
        for (const QString& choice : choices) {
            if (choice.startsWith(identifier + QStringLiteral(":")))
                return true;
        }
        return false;
    };

    const QStringList choices = editor.matchFragment(QStringLiteral("da"), true);
    QVERIFY(hasCompletionIdentifier(choices, QStringLiteral("dag")));
    QVERIFY(hasCompletionIdentifier(choices, QStringLiteral("daL")));
    QVERIFY(hasCompletionIdentifier(choices, QStringLiteral("dam")));
    QVERIFY(hasCompletionIdentifier(choices, QStringLiteral("daN")));
    QVERIFY(hasCompletionIdentifier(choices, QStringLiteral("daPa")));
}

void TestEditorUi::unit_context_completion_excludes_prefixed_square_and_cubic_metre()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const auto hasCompletionIdentifier = [](const QStringList& choices, const QString& identifier) {
        for (const QString& choice : choices) {
            if (choice.startsWith(identifier + QStringLiteral(":")))
                return true;
        }
        return false;
    };

    const QStringList choices = editor.matchFragment(QStringLiteral("deci"), true);
    QVERIFY(!hasCompletionIdentifier(choices, QStringLiteral("decisquare_metre")));
    QVERIFY(!hasCompletionIdentifier(choices, QStringLiteral("decicubic_metre")));
}

void TestEditorUi::completes_binary_prefixed_information_unit_to_short_form_in_unit_context()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1 [meb"));
    editor.setCursorPosition(editor.text().size());

    const QStringList mebChoices = editor.matchFragment(QStringLiteral("meb"), true);
    const QString mebibyteChoice = QStringLiteral("%1:%2").arg(
        QStringLiteral("mebibyte"),
        tr(unitLocalizedIdentifierName(QStringLiteral("mebibyte")).toUtf8().constData()));
    QVERIFY(mebChoices.contains(mebibyteChoice));

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("mebibyte:Unit"))));
    QCOMPARE(editor.text(), QStringLiteral("1 [MiB"));
}

void TestEditorUi::completes_day_unit_to_short_form_in_unit_context()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1 [d"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("day:Unit"))));
    QCOMPARE(editor.text(), QStringLiteral("1 [d"));
}

void TestEditorUi::completes_hour_unit_to_short_form_in_unit_context()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1 [h"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("hour:Unit"))));
    QCOMPARE(editor.text(), QStringLiteral("1 [h"));
}

void TestEditorUi::completes_affine_temperature_units_in_unit_context()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("100 [c"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("degree_celsius:Unit"))));
    QCOMPARE(editor.text(), QString::fromUtf8("100 [°C"));

    editor.setText(QStringLiteral("100 [f"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("degree_fahrenheit:Unit"))));
    QCOMPARE(editor.text(), QString::fromUtf8("100 [°F"));

    editor.setText(QString::fromUtf8("100 [º"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QString::fromUtf8("ºC:Unit"))));
    QCOMPARE(editor.text(), QString::fromUtf8("100 [°C"));

    editor.setText(QString::fromUtf8("100 [˚"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QString::fromUtf8("˚C:Unit"))));
    QCOMPARE(editor.text(), QString::fromUtf8("100 [°C"));

    editor.setText(QString::fromUtf8("100 [˚"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QString::fromUtf8("˚F:Unit"))));
    QCOMPARE(editor.text(), QString::fromUtf8("100 [°F"));

    editor.setText(QStringLiteral("100 [deg"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("degC:Unit"))));
    QCOMPARE(editor.text(), QString::fromUtf8("100 [°C"));

    editor.setText(QStringLiteral("100 [ce"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("Cel:Unit"))));
    QCOMPARE(editor.text(), QString::fromUtf8("100 [°C"));

    editor.setText(QStringLiteral("100 [deg"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("degF:Unit"))));
    QCOMPARE(editor.text(), QString::fromUtf8("100 [°F"));

    editor.setText(QStringLiteral("100 [fa"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("Fah:Unit"))));
    QCOMPARE(editor.text(), QString::fromUtf8("100 [°F"));
}

void TestEditorUi::completes_arc_units_in_unit_context()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1 [arc"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("arcminute:Unit"))));
    QVERIFY(editor.text() != QStringLiteral("1 [arc"));

    editor.setText(QStringLiteral("1 [arc"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("arcsecond:Unit"))));
    QVERIFY(editor.text() != QStringLiteral("1 [arc"));
}

void TestEditorUi::tooltip_does_not_duplicate_degree_symbol_for_explicit_angle_conversion()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const char oldAngleUnit = settings->angleUnit;
    const char oldResultFormat = settings->resultFormat;
    settings->angleUnit = 'd';
    settings->resultFormat = 'f';
    Evaluator::instance()->initializeAngleUnits();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    editor.setText(QStringLiteral("1 [rev] -> [°]"));
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();

    QVERIFY(!spy.isEmpty());
    const QString message = spy.takeLast().at(0).toString();
    QVERIFY(message.contains(QString::fromUtf8("= 360°")));
    QVERIFY(!message.contains(QString::fromUtf8("= 360°°")));

    settings->angleUnit = oldAngleUnit;
    settings->resultFormat = oldResultFormat;
    Evaluator::instance()->initializeAngleUnits();
}

void TestEditorUi::tooltip_does_not_append_angle_mode_symbol_after_explicit_arcsecond_unit()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const char oldAngleUnit = settings->angleUnit;
    const char oldResultFormat = settings->resultFormat;
    settings->angleUnit = 'd';
    settings->resultFormat = 'f';
    Evaluator::instance()->initializeAngleUnits();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    editor.setText(QStringLiteral("1 [rev] -> [arcsec]"));
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();

    QVERIFY(!spy.isEmpty());
    const QString message = spy.takeLast().at(0).toString();
    QVERIFY(message.contains(QString(UnicodeChars::DoublePrime)));
    QVERIFY(!message.contains(QString(UnicodeChars::DoublePrime) + UnicodeChars::DegreeSign));

    settings->angleUnit = oldAngleUnit;
    settings->resultFormat = oldResultFormat;
    Evaluator::instance()->initializeAngleUnits();
}

void TestEditorUi::tooltip_compacts_bracketed_arcminute_and_arcsecond_expression_units()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const char oldAngleUnit = settings->angleUnit;
    const char oldResultFormat = settings->resultFormat;
    settings->angleUnit = 'r';
    settings->resultFormat = 'f';
    Evaluator::instance()->initializeAngleUnits();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    editor.setText(QStringLiteral("1 [arcmin]"));
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();

    QVERIFY(!spy.isEmpty());
    const QString arcminuteMessage = spy.takeLast().at(0).toString();
    QVERIFY2(arcminuteMessage.contains(QString::fromUtf8("1′")),
             qPrintable(QStringLiteral("Arcminute tooltip: %1").arg(arcminuteMessage)));
    QVERIFY2(!arcminuteMessage.contains(QString::fromUtf8("1 [′]")),
             qPrintable(QStringLiteral("Arcminute tooltip: %1").arg(arcminuteMessage)));

    editor.setText(QStringLiteral("1 [arcsec]"));
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();

    QVERIFY(!spy.isEmpty());
    const QString arcsecondMessage = spy.takeLast().at(0).toString();
    QVERIFY2(arcsecondMessage.contains(QString::fromUtf8("1″")),
             qPrintable(QStringLiteral("Arcsecond tooltip: %1").arg(arcsecondMessage)));
    QVERIFY2(!arcsecondMessage.contains(QString::fromUtf8("1 [″]")),
             qPrintable(QStringLiteral("Arcsecond tooltip: %1").arg(arcsecondMessage)));

    settings->angleUnit = oldAngleUnit;
    settings->resultFormat = oldResultFormat;
    Evaluator::instance()->initializeAngleUnits();
}

void TestEditorUi::tooltip_rewrites_composite_canonical_angle_symbols_to_aliases()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const char oldAngleUnit = settings->angleUnit;
    const char oldResultFormat = settings->resultFormat;
    settings->angleUnit = 'r';
    settings->resultFormat = 'f';
    Evaluator::instance()->initializeAngleUnits();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    editor.setText(QString::fromUtf8("1 [″/m]"));
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();
    QVERIFY(!spy.isEmpty());
    const QString arcsecondMessage = spy.takeLast().at(0).toString();
    QVERIFY2(arcsecondMessage.contains(Units::arcsecondAliasSymbol()),
             qPrintable(QStringLiteral("Arcsecond composite tooltip: %1").arg(arcsecondMessage)));
    QVERIFY2(!arcsecondMessage.contains(QString::fromUtf8("″/m")),
             qPrintable(QStringLiteral("Arcsecond composite tooltip: %1").arg(arcsecondMessage)));

    editor.setText(QString::fromUtf8("1 [′/m]"));
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();
    QVERIFY(!spy.isEmpty());
    const QString arcminuteMessage = spy.takeLast().at(0).toString();
    QVERIFY2(arcminuteMessage.contains(Units::arcminuteAliasSymbol()),
             qPrintable(QStringLiteral("Arcminute composite tooltip: %1").arg(arcminuteMessage)));
    QVERIFY2(!arcminuteMessage.contains(QString::fromUtf8("′/m")),
             qPrintable(QStringLiteral("Arcminute composite tooltip: %1").arg(arcminuteMessage)));

    editor.setText(QString::fromUtf8("1 [°/m]"));
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();
    QVERIFY(!spy.isEmpty());
    const QString degreeMessage = spy.takeLast().at(0).toString();
    QVERIFY2(degreeMessage.contains(Units::degreeAliasSymbol()),
             qPrintable(QStringLiteral("Degree composite tooltip: %1").arg(degreeMessage)));
    QVERIFY2(!degreeMessage.contains(QString::fromUtf8("°/m")),
             qPrintable(QStringLiteral("Degree composite tooltip: %1").arg(degreeMessage)));

    settings->angleUnit = oldAngleUnit;
    settings->resultFormat = oldResultFormat;
    Evaluator::instance()->initializeAngleUnits();
}

void TestEditorUi::tooltip_shows_radian_suffix_for_negative_sexagesimal_literal()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const char oldAngleUnit = settings->angleUnit;
    const char oldResultFormat = settings->resultFormat;
    settings->angleUnit = 'r';
    settings->resultFormat = 'f';
    Evaluator::instance()->initializeAngleUnits();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    editor.setText(QString::fromUtf8("−57°17′44.80624709635515647336″"));
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();

    QVERIFY(!spy.isEmpty());
    const QString message = spy.takeLast().at(0).toString();
    QVERIFY(message.contains(QString(MathDsl::QuantSp) + Units::angleModeUnitSymbol('r')));

    settings->angleUnit = oldAngleUnit;
    settings->resultFormat = oldResultFormat;
    Evaluator::instance()->initializeAngleUnits();
}

void TestEditorUi::tooltip_trig_output_does_not_append_angle_mode_suffix()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const char oldAngleUnit = settings->angleUnit;
    const char oldResultFormat = settings->resultFormat;
    settings->resultFormat = 'f';

    struct Case {
        char angleUnit;
        QString forbiddenSuffix;
    };
    const QList<Case> cases = {
        {'r', QString(MathDsl::QuantSp) + Units::angleModeUnitSymbol('r')},
        {'d', Units::angleModeUnitSymbol('d')}
    };

    for (const Case& c : cases) {
        settings->angleUnit = c.angleUnit;
        Evaluator::instance()->initializeAngleUnits();

        QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

        editor.setText(QString::fromUtf8("cos(180°)"));
        editor.setCursorPosition(editor.text().size());
        editor.refreshAutoCalc();
        QCoreApplication::processEvents();

        QVERIFY(!spy.isEmpty());
        const QString message = spy.takeLast().at(0).toString();
        QString normalizedMessage = message;
        normalizedMessage.replace(QString(MathDsl::SubOp), QString(MathDsl::SubOpAl1));
        QVERIFY2(normalizedMessage.contains(QStringLiteral("= -1")),
                 qPrintable(QStringLiteral("Expected scalar trig result, got: %1").arg(message)));
        const bool hasBracketedDegreeArg =
            message.contains(QString::fromUtf8("cos(180"))
            && message.contains(QString::fromUtf8("[°])"));
        const bool hasCompactDegreeArg =
            message.contains(QString::fromUtf8("cos(180"))
            && message.contains(QString::fromUtf8("°)"));
        QVERIFY2(hasBracketedDegreeArg || hasCompactDegreeArg,
                 qPrintable(QStringLiteral("Expected interpreted expression line, got: %1").arg(message)));
        const QStringList lines = normalizedMessage.split(QStringLiteral("<br/>"));
        QVERIFY2(!lines.isEmpty(),
                 qPrintable(QStringLiteral("Unexpected empty tooltip message: %1").arg(message)));
        const QString resultLine = lines.last();
        QVERIFY2(!resultLine.contains(c.forbiddenSuffix),
                 qPrintable(QStringLiteral("Unexpected angle suffix in tooltip result line: %1").arg(message)));
        QVERIFY(!resultLine.contains(QString(MathDsl::UnitStart)));
        QVERIFY(!resultLine.contains(QString(MathDsl::UnitEnd)));
    }

    settings->angleUnit = oldAngleUnit;
    settings->resultFormat = oldResultFormat;
    Evaluator::instance()->initializeAngleUnits();
}

void TestEditorUi::tooltip_shows_interpreted_expression_for_non_trig_sexagesimal_expression()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const char oldAngleUnit = settings->angleUnit;
    const char oldResultFormat = settings->resultFormat;
    settings->angleUnit = 'd';
    settings->resultFormat = 'f';
    Evaluator::instance()->initializeAngleUnits();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    const QString expression =
        QString::fromUtf8("round(2.8°) + round(2.8°) + round(2.8°)");
    editor.setText(expression);
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();

    QVERIFY(!spy.isEmpty());
    const QString message = spy.takeLast().at(0).toString();
    QVERIFY2(message.contains(QString::fromUtf8("round(2.8"))
             && message.contains(QString::fromUtf8("[°])")),
             qPrintable(QStringLiteral("Expected interpreted expression line, got: %1").arg(message)));
    QVERIFY2(message.contains(QString::fromUtf8("= 9°")),
             qPrintable(QStringLiteral("Expected degree result line, got: %1").arg(message)));

    settings->angleUnit = oldAngleUnit;
    settings->resultFormat = oldResultFormat;
    Evaluator::instance()->initializeAngleUnits();
}

void TestEditorUi::tooltip_shows_simplified_line_for_repeated_trig_with_degree_sign()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const char oldAngleUnit = settings->angleUnit;
    const char oldResultFormat = settings->resultFormat;
    const bool oldSimplify = settings->simplifyResultExpressions;
    settings->angleUnit = 'd';
    settings->resultFormat = 'f';
    settings->simplifyResultExpressions = true;
    Evaluator::instance()->initializeAngleUnits();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    const QString expression = QString::fromUtf8(
        "cos(180°) + cos(180°) + cos(180°) + cos(180°)");
    editor.setText(expression);
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();

    QVERIFY(!spy.isEmpty());
    const QString message = spy.takeLast().at(0).toString();
    QString normalizedMessage = message;
    normalizedMessage.replace(QString(MathDsl::SubOp), QString(MathDsl::SubOpAl1));
    const bool hasBracketedDegreeSimplifiedLine =
        message.contains(QString::fromUtf8("= 4 · cos(180"))
        && message.contains(QString::fromUtf8("[°])"));
    const bool hasCompactDegreeSimplifiedLine =
        message.contains(QString::fromUtf8("= 4 · cos(180"))
        && message.contains(QString::fromUtf8("°)"));
    QVERIFY2(hasBracketedDegreeSimplifiedLine || hasCompactDegreeSimplifiedLine,
             qPrintable(QStringLiteral("Expected simplified line, got: %1").arg(message)));
    QVERIFY2(normalizedMessage.contains(QStringLiteral("= -4")),
             qPrintable(QStringLiteral("Expected result line, got: %1").arg(message)));

    settings->angleUnit = oldAngleUnit;
    settings->resultFormat = oldResultFormat;
    settings->simplifyResultExpressions = oldSimplify;
    Evaluator::instance()->initializeAngleUnits();
}

void TestEditorUi::tooltip_shows_simplified_line_for_mixed_revolution_aliases()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const char oldAngleUnit = settings->angleUnit;
    const char oldResultFormat = settings->resultFormat;
    const bool oldSimplify = settings->simplifyResultExpressions;
    settings->angleUnit = 'd';
    settings->resultFormat = 'f';
    settings->simplifyResultExpressions = true;
    Evaluator::instance()->initializeAngleUnits();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    const QString expression = QString::fromUtf8(
        "cos(180[rev]) + cos(180[rev]) + cos(180[rev]) + cos(180 [revolution])");
    editor.setText(expression);
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();

    QVERIFY(!spy.isEmpty());
    const QString message = spy.takeLast().at(0).toString();
    QString compactMessage = message;
    compactMessage.remove(QRegularExpression(QStringLiteral("\\s+")));
    compactMessage.remove(MathDsl::QuantSp);
    compactMessage.remove(UnicodeChars::NoBreakSpace);
    QVERIFY2(!message.contains(QString::fromUtf8("3 · cos(180")),
             qPrintable(QStringLiteral("Expected folded repeated term, got: %1").arg(message)));
    QVERIFY2(compactMessage.contains(QString::fromUtf8("=4·cos(180[rev])")),
             qPrintable(QStringLiteral("Expected unified simplified line, got: %1").arg(message)));

    settings->angleUnit = oldAngleUnit;
    settings->resultFormat = oldResultFormat;
    settings->simplifyResultExpressions = oldSimplify;
    Evaluator::instance()->initializeAngleUnits();
}

void TestEditorUi::tooltip_uses_cross_for_literal_number_products_in_simplified_line()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const char oldResultFormat = settings->resultFormat;
    const bool oldSimplify = settings->simplifyResultExpressions;
    settings->resultFormat = 'f';
    settings->simplifyResultExpressions = true;

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    editor.setText(QStringLiteral("pi*2^(-2)*pi+3*4*3"));
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();

    QVERIFY(!spy.isEmpty());
    const QString message = spy.takeLast().at(0).toString();
    const bool hasParenthesizedPowerTerm =
        message.contains(QString::fromUtf8("= π² · (2⁻²) + (3²) × 4"));
    const bool hasUnparenthesizedPowerTerm =
        message.contains(QString::fromUtf8("= π² · (2⁻²) + 3² × 4"));
    QVERIFY2(hasParenthesizedPowerTerm || hasUnparenthesizedPowerTerm,
             qPrintable(QStringLiteral("Expected cross between literal numbers in simplified line, got: %1").arg(message)));

    QSignalSpy interpretedSpy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));
    editor.setText(QStringLiteral("pi*2^(-2)*pi+3^2*4"));
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();

    QVERIFY(!interpretedSpy.isEmpty());
    const QString interpretedMessage = interpretedSpy.takeLast().at(0).toString();
    const bool interpretedHasParenthesizedPowerTerm =
        interpretedMessage.contains(QString::fromUtf8("π · (2⁻²) · π + (3²) × 4"));
    const bool interpretedHasUnparenthesizedPowerTerm =
        interpretedMessage.contains(QString::fromUtf8("π · (2⁻²) · π + 3² × 4"));
    QVERIFY2(interpretedHasParenthesizedPowerTerm || interpretedHasUnparenthesizedPowerTerm,
             qPrintable(QStringLiteral("Expected interpreted line to use cross for numeric-literal product, got: %1")
                 .arg(interpretedMessage)));

    settings->resultFormat = oldResultFormat;
    settings->simplifyResultExpressions = oldSimplify;
}

void TestEditorUi::tooltip_keeps_quantsp_before_degree_celsius()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const char oldResultFormat = settings->resultFormat;
    settings->resultFormat = 'f';

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    editor.setText(QString::fromUtf8("77 [°F] -> [°C]"));
    editor.setCursorPosition(editor.text().size());
    editor.refreshAutoCalc();
    QCoreApplication::processEvents();

    QVERIFY(!spy.isEmpty());
    const QString message = spy.takeLast().at(0).toString();
    const QString expected = QStringLiteral("= 25") + QString(MathDsl::QuantSp) + QString::fromUtf8("°C");
    QVERIFY(message.contains(expected));

    settings->resultFormat = oldResultFormat;
}

void TestEditorUi::tooltip_handles_affine_temperature_units_without_arrow_and_with_conversion()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    Settings* settings = Settings::instance();
    const char oldResultFormat = settings->resultFormat;
    settings->resultFormat = 'f';

    struct Case {
        QString expression;
        QString expected;
    };
    const QList<Case> cases = {
        {QString::fromUtf8("100 [°C]"),
         QStringLiteral("= 100") + QString(MathDsl::QuantSp) + QString::fromUtf8("°C")},
        {QString::fromUtf8("203 [°F]"),
         QStringLiteral("= 203") + QString(MathDsl::QuantSp) + QString::fromUtf8("°F")},
        {QString::fromUtf8("1 [K] -> [°C]"),
         QString(MathDsl::Equals)
             + QStringLiteral(" ")
             + QString(MathDsl::SubOp)
             + QStringLiteral("272.15")
             + QString(MathDsl::QuantSp)
             + QString::fromUtf8("°C")},
        {QString::fromUtf8("1 [K] -> [°F]"),
         QString(MathDsl::Equals)
             + QStringLiteral(" ")
             + QString(MathDsl::SubOp)
             + QStringLiteral("457.87")
             + QString(MathDsl::QuantSp)
             + QString::fromUtf8("°F")}
    };

    for (const Case& c : cases) {
        QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));
        editor.setText(c.expression);
        editor.setCursorPosition(editor.text().size());
        editor.refreshAutoCalc();
        QCoreApplication::processEvents();

        QVERIFY(!spy.isEmpty());
        const QString message = spy.takeLast().at(0).toString();
        QVERIFY2(message.contains(c.expected),
                 qPrintable(QStringLiteral("Expression: %1\nMessage: %2\nExpected fragment: %3")
                                .arg(c.expression, message, c.expected)));
    }

    settings->resultFormat = oldResultFormat;
}

void TestEditorUi::tooltip_shows_selection_result_when_selecting_with_shift_arrows()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    editor.setText(QStringLiteral("1+24"));
    editor.setCursorPosition(editor.text().size());
    QCoreApplication::processEvents();

    QTest::keyClick(&editor, Qt::Key_Left, Qt::ShiftModifier);
    QCoreApplication::processEvents();

    QVERIFY(!spy.isEmpty());
    const QString message = spy.takeLast().at(0).toString();
    QVERIFY2(message.contains(QStringLiteral("Selection result:")),
             qPrintable(QStringLiteral("Expected selection result message, got: %1").arg(message)));
    QVERIFY2(message.contains(QStringLiteral("= 4")),
             qPrintable(QStringLiteral("Expected selected value 4 in message, got: %1").arg(message)));
}

void TestEditorUi::tooltip_shows_selection_result_when_selecting_all_with_keyboard_shortcut()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    editor.setText(QStringLiteral("1+24"));
    editor.setCursorPosition(editor.text().size());
    QCoreApplication::processEvents();
    spy.clear();

    const QList<QKeySequence> bindings = QKeySequence::keyBindings(QKeySequence::SelectAll);
    QVERIFY(!bindings.isEmpty());
    const QKeyCombination selectAll = bindings.first()[0];
    QTest::keyClick(&editor, selectAll.key(), selectAll.keyboardModifiers());
    QCoreApplication::processEvents();

    QVERIFY2(editor.textCursor().hasSelection(),
             qPrintable(QStringLiteral("Expected select-all selection, got cursor position %1")
                            .arg(editor.textCursor().position())));
    QVERIFY(!spy.isEmpty());
    const QString message = spy.takeLast().at(0).toString();
    QVERIFY2(message.contains(QStringLiteral("Selection result:")),
             qPrintable(QStringLiteral("Expected selection result message, got: %1").arg(message)));
    QVERIFY2(message.contains(QStringLiteral("= 25")),
             qPrintable(QStringLiteral("Expected selected value 25 in message, got: %1").arg(message)));
}

void TestEditorUi::tooltip_shows_selection_result_when_selecting_with_mouse()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    editor.setText(QStringLiteral("1+24"));
    editor.setCursorPosition(editor.text().size());
    QCoreApplication::processEvents();
    spy.clear();

    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(3);
    const QPoint selectionStart = editor.cursorRect(cursor).center();
    cursor.setPosition(4);
    const QPoint selectionEnd = editor.cursorRect(cursor).center();

    QTest::mousePress(editor.viewport(), Qt::LeftButton, Qt::NoModifier, selectionStart);
    QTest::mouseMove(editor.viewport(), selectionEnd);
    QCoreApplication::processEvents();
    QVERIFY(spy.isEmpty());

    QTest::mouseRelease(editor.viewport(), Qt::LeftButton, Qt::NoModifier, selectionEnd);
    QCoreApplication::processEvents();

    QVERIFY2(editor.textCursor().hasSelection(),
             qPrintable(QStringLiteral("Expected mouse selection, got cursor position %1")
                            .arg(editor.textCursor().position())));
    QVERIFY(!spy.isEmpty());
    const QString message = spy.takeLast().at(0).toString();
    QVERIFY2(message.contains(QStringLiteral("Selection result:")),
             qPrintable(QStringLiteral("Expected selection result message, got: %1").arg(message)));
    QVERIFY2(message.contains(QStringLiteral("= 4")),
             qPrintable(QStringLiteral("Expected selected value 4 in message, got: %1").arg(message)));
}

void TestEditorUi::tooltip_does_not_refresh_current_result_on_caret_arrow_move()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    editor.setText(QStringLiteral("1+24"));
    editor.setCursorPosition(editor.text().size());
    QCoreApplication::processEvents();
    spy.clear();

    QTest::keyClick(&editor, Qt::Key_Left);
    QCoreApplication::processEvents();

    QVERIFY2(spy.isEmpty(),
             qPrintable(QStringLiteral("Caret movement should not refresh the result tooltip, got: %1")
                            .arg(spy.isEmpty() ? QString() : spy.takeLast().at(0).toString())));
}

void TestEditorUi::tooltip_does_not_refresh_current_result_on_mouse_caret_reposition()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    editor.setText(QStringLiteral("1+24"));
    editor.setCursorPosition(editor.text().size());
    QCoreApplication::processEvents();
    spy.clear();

    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(1);
    const QPoint clickPosition = editor.cursorRect(cursor).center();
    QTest::mouseClick(editor.viewport(), Qt::LeftButton, Qt::NoModifier, clickPosition);
    QCoreApplication::processEvents();

    QVERIFY2(spy.isEmpty(),
             qPrintable(QStringLiteral("Mouse caret movement should not refresh the result tooltip, got: %1")
                            .arg(spy.isEmpty() ? QString() : spy.takeLast().at(0).toString())));
}

void TestEditorUi::tooltip_refreshes_current_result_on_char_deletion()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    QSignalSpy spy(&editor, SIGNAL(autoCalcMessageAvailable(const QString&)));

    editor.setText(QStringLiteral("1+24"));
    editor.setCursorPosition(editor.text().size());
    QCoreApplication::processEvents();
    spy.clear();

    QTest::keyClick(&editor, Qt::Key_Backspace);
    QCoreApplication::processEvents();

    QVERIFY(!spy.isEmpty());
    const QString message = spy.takeLast().at(0).toString();
    QVERIFY2(message.contains(QStringLiteral("Current result:")),
             qPrintable(QStringLiteral("Expected current result message after deletion, got: %1").arg(message)));
    QVERIFY2(message.contains(QStringLiteral("= 3")),
             qPrintable(QStringLiteral("Expected updated value 3 in message, got: %1").arg(message)));
}

void TestEditorUi::enter_evaluates_when_completion_popup_has_no_explicit_interaction()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QString input = QStringLiteral("co");
    editor.setText(input);
    editor.setCursorPosition(input.size());

    QSignalSpy returnPressedSpy(&editor, SIGNAL(returnPressed()));

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);

    QTest::keyClick(popup, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::processEvents();

    QCOMPARE(returnPressedSpy.count(), 1);
    QCOMPARE(editor.text(), input);
}

void TestEditorUi::completion_popup_arrow_keys_change_selection()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("co"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QVERIFY(popup->topLevelItemCount() > 1);

    QTreeWidgetItem* initialItem = popup->currentItem();
    QVERIFY(initialItem);

    QTest::keyClick(popup, Qt::Key_Down, Qt::NoModifier);
    QCoreApplication::processEvents();

    QVERIFY(popup->currentItem());
    QVERIFY(popup->currentItem() != initialItem);
    popup->hide();
}

void TestEditorUi::completion_popup_tab_accepts_selected_item()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("co"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QTreeWidgetItem* cosItem = nullptr;
    for (int i = 0; i < popup->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = popup->topLevelItem(i);
        if (item != nullptr && item->text(1) == QStringLiteral("cos")) {
            cosItem = item;
            break;
        }
    }
    QVERIFY(cosItem);
    popup->setCurrentItem(cosItem);

    QTest::keyClick(&editor, Qt::Key_Tab);
    QCoreApplication::processEvents();

    QCOMPARE(editor.text(), QStringLiteral("cos()"));
    QTRY_VERIFY_WITH_TIMEOUT(s_editorOrViewportHasFocus(&editor), 1000);
}

void TestEditorUi::completion_popup_closes_when_clicking_elsewhere()
{
    QWidget container;
    QVBoxLayout layout(&container);
    Editor editor;
    QPushButton button(QStringLiteral("Other"));
    layout.addWidget(&editor);
    layout.addWidget(&button);
    container.resize(800, 400);
    container.show();
    QVERIFY(QTest::qWaitForWindowExposed(&container));
    editor.setFocus();

    editor.setText(QStringLiteral("co"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);

    QTest::mouseClick(&container,
                      Qt::LeftButton,
                      Qt::NoModifier,
                      QPoint(container.width() - 2, container.height() - 2));
    QCoreApplication::processEvents();

    QVERIFY(!popup->isVisible());
    QTest::qWait(250);
    QVERIFY(!popup->isVisible());
    popup->hide();
}

void TestEditorUi::completion_popup_clicking_row_accepts_item()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("co"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QTreeWidgetItem* cosItem = nullptr;
    for (int i = 0; i < popup->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = popup->topLevelItem(i);
        if (item != nullptr && item->text(1) == QStringLiteral("cos")) {
            cosItem = item;
            break;
        }
    }
    QVERIFY(cosItem);

    popup->scrollToItem(cosItem);
    const QRect itemRect = popup->visualItemRect(cosItem);
    QVERIFY(!itemRect.isEmpty());
    QTest::mouseClick(popup->viewport(), Qt::LeftButton, Qt::NoModifier, itemRect.center());
    QCoreApplication::processEvents();

    QCOMPARE(editor.text(), QStringLiteral("cos()"));
}

void TestEditorUi::completion_popup_mouse_selection_keeps_focus_on_owning_editor()
{
    QWidget container;
    QVBoxLayout layout(&container);
    Editor firstEditor;
    Editor secondEditor;
    layout.addWidget(&firstEditor);
    layout.addWidget(&secondEditor);
    container.show();
    QVERIFY(QTest::qWaitForWindowExposed(&container));

    container.activateWindow();
    secondEditor.setFocus();
    QCoreApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(s_editorOrViewportHasFocus(&secondEditor), 1000);

    firstEditor.setFocus();
    QTRY_VERIFY_WITH_TIMEOUT(s_editorOrViewportHasFocus(&firstEditor), 1000);
    firstEditor.setText(QStringLiteral("co"));
    firstEditor.setCursorPosition(firstEditor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &firstEditor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QTreeWidgetItem* cosItem = nullptr;
    for (int i = 0; i < popup->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = popup->topLevelItem(i);
        if (item != nullptr && item->text(1) == QStringLiteral("cos")) {
            cosItem = item;
            break;
        }
    }
    QVERIFY(cosItem);

    popup->scrollToItem(cosItem);
    const QRect itemRect = popup->visualItemRect(cosItem);
    QVERIFY(!itemRect.isEmpty());
    QTest::mouseClick(popup->viewport(), Qt::LeftButton, Qt::NoModifier, itemRect.center());
    QCoreApplication::processEvents();

    QCOMPARE(firstEditor.text(), QStringLiteral("cos()"));
    QTRY_VERIFY_WITH_TIMEOUT(s_editorOrViewportHasFocus(&firstEditor), 1000);
    QVERIFY(!s_editorOrViewportHasFocus(&secondEditor));
}

void TestEditorUi::completion_popup_scrollbar_click_keeps_popup_open()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("[m"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QVERIFY(popup->verticalScrollBar()->isVisible());

    QTest::mouseClick(popup->verticalScrollBar(), Qt::LeftButton);
    QCoreApplication::processEvents();

    QVERIFY(popup->isVisible());
    popup->hide();
}

void TestEditorUi::completion_popup_mouse_wheel_scrolls_popup()
{
    Settings* settings = Settings::instance();
    const bool userVariablesBackup = settings->autoCompletionUserVariables;
    settings->autoCompletionUserVariables = true;
    struct UserVariablesCompletionRestore {
        Settings* settings;
        bool value;
        ~UserVariablesCompletionRestore() { settings->autoCompletionUserVariables = value; }
    } restore { settings, userVariablesBackup };

    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    for (int i = 0; i < 30; ++i)
        editor.evaluator()->setVariable(QStringLiteral("wheel_var_%1").arg(i), Quantity(i));

    editor.setText(QStringLiteral("wheel_var_"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (popup = s_completionPopupTreeContaining(QStringLiteral("wheel_var_"))) != nullptr,
        1000);
    QVERIFY2(popup->verticalScrollBar()->maximum() > popup->verticalScrollBar()->minimum(),
             qPrintable(QStringLiteral("min=%1 max=%2 rows=%3")
                            .arg(popup->verticalScrollBar()->minimum())
                            .arg(popup->verticalScrollBar()->maximum())
                            .arg(popup->topLevelItemCount())));
    popup->verticalScrollBar()->setValue(popup->verticalScrollBar()->minimum());
    QVERIFY(popup->topLevelItemCount() > 8);
    const int initialValue = popup->verticalScrollBar()->value();

    const QPoint globalWheelPosition =
        popup->viewport()->mapToGlobal(popup->viewport()->rect().center());
    QWheelEvent wheelEvent(
        editor.mapFromGlobal(globalWheelPosition),
        globalWheelPosition,
        QPoint(),
        QPoint(0, -120),
        Qt::NoButton,
        Qt::NoModifier,
        Qt::NoScrollPhase,
        false);
    QCoreApplication::sendEvent(&editor, &wheelEvent);

    QVERIFY2(popup->verticalScrollBar()->value() > initialValue,
             qPrintable(QStringLiteral("initial=%1 current=%2 max=%3")
                            .arg(initialValue)
                            .arg(popup->verticalScrollBar()->value())
                            .arg(popup->verticalScrollBar()->maximum())));
    popup->hide();
}

void TestEditorUi::completion_popup_escape_closes_and_stays_closed()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("co"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);

    QTest::keyClick(popup, Qt::Key_Escape);
    QCoreApplication::processEvents();

    QVERIFY(!popup->isVisible());
    QTest::qWait(250);
    QVERIFY(!popup->isVisible());
}

void TestEditorUi::completion_popup_escape_keeps_focus_on_owning_editor()
{
    QWidget container;
    QVBoxLayout layout(&container);
    Editor firstEditor;
    Editor secondEditor;
    layout.addWidget(&firstEditor);
    layout.addWidget(&secondEditor);
    container.show();
    QVERIFY(QTest::qWaitForWindowExposed(&container));

    container.activateWindow();
    firstEditor.setFocus();
    QTRY_VERIFY_WITH_TIMEOUT(s_editorOrViewportHasFocus(&firstEditor), 1000);
    firstEditor.setText(QStringLiteral("co"));
    firstEditor.setCursorPosition(firstEditor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &firstEditor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(s_editorOrViewportHasFocus(&firstEditor), 1000);
    QVERIFY(!s_editorOrViewportHasFocus(&secondEditor));

    QTest::keyClick(&firstEditor, Qt::Key_Escape);
    QCoreApplication::processEvents();

    QVERIFY(!popup->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(s_editorOrViewportHasFocus(&firstEditor), 1000);
    QVERIFY(!s_editorOrViewportHasFocus(&secondEditor));
}

void TestEditorUi::completion_popup_reopens_after_typing_following_escape()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("co"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QTest::keyClick(popup, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QVERIFY(!popup->isVisible());

    editor.insert(QStringLiteral("s"));
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QVERIFY(popup->isVisible());
    popup->hide();
}

void TestEditorUi::completion_popup_reopens_after_retyping_deleted_letter_data()
{
    QTest::addColumn<QString>("initialText");
    QTest::addColumn<QString>("letter");
    QTest::addColumn<bool>("processDeletion");
    QTest::addColumn<bool>("dismissWithEscape");

    QTest::newRow("a-immediate") << QString() << QStringLiteral("a") << false << false;
    QTest::newRow("a-delayed") << QString() << QStringLiteral("a") << true << false;
    QTest::newRow("c-delayed") << QString() << QStringLiteral("c") << true << false;
    QTest::newRow("s-delayed") << QString() << QStringLiteral("s") << true << false;
    QTest::newRow("co-immediate") << QStringLiteral("c") << QStringLiteral("o") << false << false;
    QTest::newRow("unit-delayed") << QStringLiteral("1[") << QStringLiteral("m") << true << false;
    QTest::newRow("escape-delayed") << QString() << QStringLiteral("a") << true << true;
}

void TestEditorUi::completion_popup_reopens_after_retyping_deleted_letter()
{
    QFETCH(QString, initialText);
    QFETCH(QString, letter);
    QFETCH(bool, processDeletion);
    QFETCH(bool, dismissWithEscape);

    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.activateWindow();
    editor.setFocus();
    QTRY_VERIFY_WITH_TIMEOUT(s_editorOrViewportHasFocus(&editor), 1000);
    editor.setText(initialText);
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, letter);
    const QString completedText = editor.text();
    const int completedPosition = editor.textCursor().position();

    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);

    for (int cycle = 0; cycle < 2; ++cycle) {
        if (dismissWithEscape) {
            QTest::keyClick(&editor, Qt::Key_Escape);
            QVERIFY(!popup->isVisible());
        }
        QTest::keyClick(&editor, Qt::Key_Backspace);
        QCOMPARE(editor.text(), initialText);
        if (processDeletion)
            QTest::qWait(50);

        QTest::keyClicks(&editor, letter);
        QCOMPARE(editor.text(), completedText);
        QCOMPARE(editor.textCursor().position(), completedPosition);
        QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    }
    popup->hide();
}

void TestEditorUi::completion_popup_shows_for_two_character_prefixes()
{
    for (const QString& prefix : {QStringLiteral("co"), QStringLiteral("si")}) {
        Editor editor;
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));
        editor.setFocus();
        editor.setText(prefix);
        editor.setCursorPosition(editor.text().size());

        QVERIFY(QMetaObject::invokeMethod(
            &editor,
            "triggerAutoComplete",
            Qt::DirectConnection));

        QTreeWidget* popup = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
        QVERIFY(popup->isVisible());
        popup->hide();
    }
}

void TestEditorUi::completion_popup_uses_positionable_nonactivating_surface()
{
    QWidget firstWindow;
    QWidget secondWindow;
    QVBoxLayout firstLayout(&firstWindow);
    QVBoxLayout secondLayout(&secondWindow);
    // Match application startup: construct the editor before placing it in a pane.
    Editor editor;
    editor.setAutoCompletionEnabled(false);
    firstLayout.addWidget(&editor);
    firstWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&firstWindow));
    editor.setFocus();

    EditorCompletion* completion = editor.findChild<EditorCompletion*>();
    QVERIFY(completion);
    completion->showCompletion({QStringLiteral("cos:Cosine")});
    QTreeWidget* popup = s_completionPopupTree();
    QVERIFY(popup);
    QCOMPARE(popup->windowType(), Qt::ToolTip);
    QVERIFY(popup->testAttribute(Qt::WA_ShowWithoutActivating));
    QCOMPARE(popup->focusPolicy(), Qt::NoFocus);
    QCOMPARE(popup->windowHandle()->transientParent(), firstWindow.windowHandle());
    QVERIFY(s_editorOrViewportHasFocus(&editor));
    popup->hide();

    // Moving an existing pane into another window must refresh the native parent.
    secondLayout.addWidget(&editor);
    firstWindow.hide();
    secondWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&secondWindow));
    editor.setFocus();
    completion->showCompletion({QStringLiteral("cos:Cosine")});
    QCOMPARE(popup->windowHandle()->transientParent(), secondWindow.windowHandle());
    QVERIFY(s_editorOrViewportHasFocus(&editor));
    popup->hide();
}

void TestEditorUi::completion_popup_keeps_editor_widgets_non_native_data()
{
    QTest::addColumn<bool>("constants");
    QTest::newRow("autocomplete") << false;
    QTest::newRow("constants") << true;
}

void TestEditorUi::completion_popup_keeps_editor_widgets_non_native()
{
    QFETCH(bool, constants);
    QWidget window;
    QVBoxLayout layout(&window);
    Editor editor;
    editor.setAutoCompletionEnabled(false);
    layout.addWidget(&editor);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    editor.setFocus();
    QVERIFY(!editor.testAttribute(Qt::WA_NativeWindow));
    QVERIFY(!editor.viewport()->testAttribute(Qt::WA_NativeWindow));
    QVERIFY(!editor.verticalScrollBar()->testAttribute(Qt::WA_NativeWindow));

    if (constants) {
        QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
        QVERIFY(QApplication::activePopupWidget());
    } else {
        EditorCompletion* completion = editor.findChild<EditorCompletion*>();
        QVERIFY(completion);
        completion->showCompletion({QStringLiteral("cos:Cosine")});
        QVERIFY(s_completionPopupTree());
    }
    QCoreApplication::processEvents();

    // Forcing native siblings creates extra Wayland subsurfaces for the text
    // viewport and scrollbars and changes their backing-store composition.
    QVERIFY(!editor.testAttribute(Qt::WA_NativeWindow));
    QVERIFY(!editor.viewport()->testAttribute(Qt::WA_NativeWindow));
    QVERIFY(!editor.verticalScrollBar()->testAttribute(Qt::WA_NativeWindow));
    if (QWidget* popup = constants ? QApplication::activePopupWidget() : s_completionPopupTree())
        popup->hide();
}

void TestEditorUi::completion_popup_follows_editor_geometry_data()
{
    QTest::addColumn<bool>("wrapped");
    QTest::addColumn<bool>("nearBottom");
    QTest::addColumn<bool>("nearRight");
    QTest::newRow("below") << false << false << false;
    QTest::newRow("above") << false << true << false;
    QTest::newRow("right-edge") << false << false << true;
    QTest::newRow("wrapped") << true << false << false;
    QTest::newRow("wrapped-above") << true << true << false;
}

void TestEditorUi::completion_popup_follows_editor_geometry()
{
    QFETCH(bool, wrapped);
    QFETCH(bool, nearBottom);
    QFETCH(bool, nearRight);

    Editor editor;
    editor.setAutoCompletionEnabled(false);
    editor.setFixedWidth(340);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();
    const QString input = wrapped
        ? QStringLiteral("1234567890 + ").repeated(30) + QStringLiteral("co")
        : QStringLiteral("123 + co");
    editor.setText(input);
    editor.setCursorPosition(input.size());
    QCoreApplication::processEvents();

    const QRect screen = editor.screen()->availableGeometry();
    const int x = nearRight ? screen.right() - 40 : screen.left() + 30;
    const int y = nearBottom ? screen.bottom() - editor.height() + 1 : screen.top() + 80;
    editor.move(x, y);
    QCoreApplication::processEvents();
    QTextCursor anchor = editor.textCursor();
    anchor.movePosition(QTextCursor::StartOfWord);
    const int anchorX = editor.viewport()->mapToGlobal(editor.cursorRect(anchor).topLeft()).x();

    EditorCompletion* completion = editor.findChild<EditorCompletion*>();
    QVERIFY(completion);
    completion->showCompletion({QStringLiteral("cos:Cosine"), QStringLiteral("cosh:Hyperbolic cosine")});
    QTreeWidget* popup = s_completionPopupTree();
    QVERIFY(popup);
    const QPoint editorTop = editor.mapToGlobal(QPoint(0, 0));
    const int belowY = editorTop.y() + editor.height();
    const int expectedY = belowY + popup->height() > screen.bottom() + 1
        ? editorTop.y() - popup->height() : belowY;
    const int expectedX = qBound(screen.left(), anchorX, screen.right() + 1 - popup->width());
    QCOMPARE(popup->geometry().topLeft(), QPoint(expectedX, expectedY));
    QVERIFY(screen.contains(popup->geometry()));
    if (wrapped)
        QVERIFY(editor.verticalScrollBar()->value() > 0);
    popup->hide();
}

void TestEditorUi::completion_popup_geometry_avoids_editor_overlap_data()
{
    QTest::addColumn<QRect>("bounds");
    QTest::addColumn<QRect>("editor");
    QTest::addColumn<int>("height");
    QTest::addColumn<QRect>("expected");
    QTest::newRow("below")
        << QRect(0, 0, 600, 400) << QRect(30, 100, 400, 60) << 100 << QRect(50, 160, 200, 100);
    QTest::newRow("above-at-window-bottom")
        << QRect(0, 0, 600, 400) << QRect(30, 330, 400, 60) << 100 << QRect(50, 230, 200, 100);
    QTest::newRow("shrink-above")
        << QRect(0, 0, 600, 400) << QRect(30, 200, 400, 60) << 300 << QRect(50, 0, 200, 200);
    QTest::newRow("shrink-below")
        << QRect(0, 0, 600, 400) << QRect(30, 80, 400, 60) << 300 << QRect(50, 140, 200, 260);
    QTest::newRow("negative-screen-origin")
        << QRect(0, -400, 600, 400) << QRect(30, -200, 400, 60) << 300 << QRect(50, -400, 200, 200);
    QTest::newRow("no-space")
        << QRect(0, 0, 600, 60) << QRect(30, 0, 400, 60) << 100 << QRect();
}

void TestEditorUi::completion_popup_geometry_avoids_editor_overlap()
{
    QFETCH(QRect, bounds);
    QFETCH(QRect, editor);
    QFETCH(int, height);
    QFETCH(QRect, expected);
    const QRect popup = EditorUtils::completionPopupGeometry(editor, 50, QSize(200, height), bounds);
    QCOMPARE(popup, expected);
    QVERIFY(!popup.intersects(editor));
    if (popup.isValid())
        QVERIFY(bounds.contains(popup));
}

void TestEditorUi::completion_popup_shortens_tall_lists_without_covering_editor()
{
    Editor editor;
    editor.setAutoCompletionEnabled(false);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();
    const QRect screen = editor.screen()->availableGeometry();
    editor.move(screen.left() + 20, screen.center().y() - editor.height() / 2);

    QTreeWidget* popup = editor.findChild<QTreeWidget*>(QStringLiteral("editorCompletionPopup"));
    QVERIFY(popup);
    QFont font = popup->font();
    font.setPixelSize(qMax(1, screen.height() / 8));
    popup->setFont(font);
    QStringList choices;
    for (int i = 0; i < 8; ++i)
        choices.append(QStringLiteral("%1:x").arg(i));
    EditorCompletion* completion = editor.findChild<EditorCompletion*>();
    QVERIFY(completion);
    completion->showCompletion(choices);
    QCoreApplication::processEvents();

    QVERIFY(popup->isVisible());
    const QRect editorRect(editor.mapToGlobal(QPoint(0, 0)), editor.size());
    QVERIFY(!popup->geometry().intersects(editorRect));
    QVERIFY(popup->geometry().top() >= screen.top());
    QVERIFY(popup->geometry().bottom() <= screen.bottom());
    QVERIFY(popup->verticalScrollBar()->maximum() > 0);
    popup->hide();
}

void TestEditorUi::completion_popups_configure_editor_anchor_data()
{
    QTest::addColumn<bool>("constants");
    QTest::newRow("autocomplete") << false;
    QTest::newRow("constants") << true;
}

void TestEditorUi::completion_popups_configure_editor_anchor()
{
    QFETCH(bool, constants);
    QWidget window;
    QVBoxLayout layout(&window);
    layout.addStretch();
    Editor editor;
    editor.setAutoCompletionEnabled(false);
    editor.setFixedWidth(340);
    layout.addWidget(&editor);
    window.resize(400, 400);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // Constant completion is normally constructed lazily on Ctrl+Space.
    ConstantCompletion* constantCompletion = constants ? new ConstantCompletion(&editor) : nullptr;
    QWidget* popup = editor.findChild<QWidget*>(constants
        ? QStringLiteral("constantCompletionPopup") : QStringLiteral("editorCompletionPopup"));
    QVERIFY(popup);
    // Both popups must configure the very first native surface created by show().
    QVERIFY(!popup->windowHandle());
    QRect previousAnchor;
    for (const bool wrapped : {false, true}) {
        const QString input = wrapped
            ? QStringLiteral("1234567890 + ").repeated(30) + QStringLiteral("co")
            : QStringLiteral("123 + co");
        editor.setText(input);
        editor.setCursorPosition(input.size());
        editor.setFocus();
        QCoreApplication::processEvents();
        QTextCursor cursor = editor.textCursor();
        if (!constants)
            cursor.movePosition(QTextCursor::StartOfWord);
        const QRect editorRect(editor.mapTo(&window, QPoint()), editor.size());
        const int cursorX = editor.viewport()->mapTo(&window, editor.cursorRect(cursor).topLeft()).x();
        const QRect expected(qBound(editorRect.left(), cursorX, editorRect.right()),
                             editorRect.top(), 1, editorRect.height());

        if (constants) {
            constantCompletion->showCompletion();
        } else {
            editor.findChild<EditorCompletion*>()->showCompletion({QStringLiteral("cos:Cosine")});
        }
        QVERIFY(popup->isVisible());
        QWindow* native = popup->windowHandle();
        QVERIFY(native);
        QCOMPARE(native->transientParent(), window.windowHandle());
        QCOMPARE(native->property("_q_waylandPopupAnchorRect").toRect(), expected);
        QCOMPARE(native->property("_q_waylandPopupAnchor").value<Qt::Edges>(),
                 Qt::Edges(Qt::BottomEdge | Qt::LeftEdge));
        QCOMPARE(native->property("_q_waylandPopupGravity").value<Qt::Edges>(),
                 Qt::Edges(Qt::BottomEdge | Qt::RightEdge));
        const uint constraints = native->property("_q_waylandPopupConstraintAdjustment").toUInt();
        QVERIFY(constraints & 1); // slide_x
        QVERIFY(constraints & 8); // flip_y
        QVERIFY(constraints & 32); // resize_y
        QVERIFY(!(constraints & 2)); // slide_y would overlap the editor
        if (wrapped) {
            QVERIFY(expected != previousAnchor);
            QVERIFY(editor.verticalScrollBar()->value() > 0);
        }
        previousAnchor = expected;
        popup->hide();
    }
}

void TestEditorUi::completion_popup_compositor_geometry_data()
{
    QTest::addColumn<QRect>("editor");
    QTest::addColumn<int>("cursorX");
    QTest::addColumn<QSize>("requested");
    QTest::addColumn<QRect>("expectedAnchor");
    QTest::addColumn<QRect>("expectedPopup");
    QTest::newRow("below-bottom-of-parent")
        << QRect(10, 340, 400, 60) << 50 << QSize(200, 100)
        << QRect(50, 340, 1, 60) << QRect(50, 400, 200, 100);
    QTest::newRow("negative-guessed-origin")
        << QRect(-600, -400, 400, 60) << -550 << QSize(200, 100)
        << QRect(-550, -400, 1, 60) << QRect(-550, -340, 200, 100);
    QTest::newRow("word-start-scrolled-off-left")
        << QRect(10, 340, 400, 60) << -50 << QSize(200, 100)
        << QRect(10, 340, 1, 60) << QRect(10, 400, 200, 100);
    QTest::newRow("cursor-scrolled-off-right")
        << QRect(10, 340, 400, 60) << 500 << QSize(200, 100)
        << QRect(409, 340, 1, 60) << QRect(409, 400, 200, 100);
    QTest::newRow("tall-and-wide-list")
        << QRect(10, 340, 400, 60) << 50 << QSize(1000, 1000)
        << QRect(50, 340, 1, 60) << QRect(50, 400, 800, 270);
    QTest::newRow("editor-fills-output")
        << QRect(10, 0, 400, 600) << 50 << QSize(200, 100)
        << QRect(50, 0, 1, 600) << QRect();
}

void TestEditorUi::completion_popup_compositor_geometry()
{
    QFETCH(QRect, editor);
    QFETCH(int, cursorX);
    QFETCH(QSize, requested);
    QFETCH(QRect, expectedAnchor);
    QFETCH(QRect, expectedPopup);
    const QRect anchor = EditorUtils::completionPopupAnchorRect(editor, cursorX);
    QCOMPARE(anchor, expectedAnchor);
    const QRect popup = EditorUtils::completionPopupCompositorGeometry(anchor, requested, QSize(800, 600));
    QCOMPARE(popup, expectedPopup);
    QVERIFY(!popup.intersects(editor));
    if (popup.isValid()) {
        // xdg-shell flip_y reflects both anchor and gravity, placing the bottom
        // of the popup at the editor's top, regardless of the parent's origin.
        const QRect flipped(QPoint(anchor.left(), anchor.top() - popup.height()), popup.size());
        QVERIFY(!flipped.intersects(editor));
        QCOMPARE(flipped.bottom() + 1, editor.top());
    }
}

void TestEditorUi::constant_completion_pages_follow_popup_resize()
{
    Editor editor;
    editor.setAutoCompletionEnabled(false);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    ConstantCompletion completion(&editor);
    completion.showCompletion();
    QWidget* popup = QApplication::activePopupWidget();
    QVERIFY(popup);
    QTreeWidget* category = popup->findChild<QTreeWidget*>(QStringLiteral("constantCompletionCategoryPopup"));
    QTreeWidget* constants = popup->findChild<QTreeWidget*>(QStringLiteral("constantCompletionConstantsPopup"));
    QVERIFY(category);
    QVERIFY(constants);
    popup->resize(320, 40);
    QCOMPARE(category->size(), popup->size());
    QCOMPARE(constants->size(), popup->size());
    QCOMPARE(category->x(), 0);
    QCOMPARE(constants->x(), popup->width());
    QTRY_VERIFY(category->verticalScrollBar()->maximum() > 0);

    QVERIFY(QMetaObject::invokeMethod(&completion, "showConstants", Qt::DirectConnection));
    QTRY_COMPARE(category->x(), -popup->width());
    popup->resize(300, 30);
    QCOMPARE(category->size(), popup->size());
    QCOMPARE(constants->size(), popup->size());
    QCOMPARE(category->x(), -popup->width());
    QCOMPARE(constants->x(), 0);
    QTRY_VERIFY(constants->verticalScrollBar()->maximum() > 0);
    popup->hide();
}

void TestEditorUi::constant_completion_popup_follows_wrapped_editor_cursor()
{
    QWidget window;
    QVBoxLayout layout(&window);
    Editor editor;
    editor.setAutoCompletionEnabled(false);
    editor.setFixedWidth(340);
    layout.addWidget(&editor);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    editor.setFocus();
    const QString input = QStringLiteral("1234567890 + ").repeated(30);
    editor.setText(input);
    editor.setCursorPosition(input.size());
    QCoreApplication::processEvents();
    const QRect screen = editor.screen()->availableGeometry();
    window.move(screen.left() + 30, screen.top() + 80);
    QCoreApplication::processEvents();
    const int anchorX = editor.viewport()->mapToGlobal(editor.cursorRect().topLeft()).x();

    QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
    QWidget* popup = QApplication::activePopupWidget();
    QVERIFY(popup);
    QCOMPARE(popup->objectName(), QStringLiteral("constantCompletionPopup"));
    QCOMPARE(popup->windowHandle()->transientParent(), window.windowHandle());
    const QPoint editorTop = editor.mapToGlobal(QPoint(0, 0));
    const int belowY = editorTop.y() + editor.height();
    const int expectedY = belowY + popup->height() > screen.bottom() + 1
        ? editorTop.y() - popup->height() : belowY;
    const int expectedX = qBound(screen.left(), anchorX, screen.right() + 1 - popup->width());
    QCOMPARE(popup->geometry().topLeft(), QPoint(expectedX, expectedY));
    QVERIFY(editor.verticalScrollBar()->value() > 0);
    popup->hide();
}

void TestEditorUi::completion_popup_uses_configured_surface_colors()
{
    const QColor background(QStringLiteral("#334455"));
    const QColor foreground(QStringLiteral("#f4f7fb"));
    const QColor scrollbarThumb(QStringLiteral("#556677"));
    const QColor scrollbarThumbForeground(QStringLiteral("#ffffff"));
    const QColor selectedRow(QStringLiteral("#667788"));
    const QColor selectedRowForeground(QStringLiteral("#101418"));
    const QColor outline(QStringLiteral("#778899"));
    const int cornerRadius = 11;

    Editor editor;
    editor.setThemeCompletionColors(background,
                                    foreground,
                                    scrollbarThumb,
                                    scrollbarThumbForeground,
                                    selectedRow,
                                    selectedRowForeground,
                                    outline,
                                    cornerRadius);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();
    editor.setText(QStringLiteral("co"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QCOMPARE(popup->palette().color(QPalette::Base), background);
    QCOMPARE(popup->palette().color(QPalette::Text), foreground);
    QCOMPARE(popup->palette().color(QPalette::Highlight), selectedRow);
    QCOMPARE(popup->palette().color(QPalette::HighlightedText), selectedRowForeground);
    QCOMPARE(popup->cursor().shape(), Qt::PointingHandCursor);
    QCOMPARE(popup->viewport()->cursor().shape(), Qt::PointingHandCursor);
    QVERIFY(popup->styleSheet().contains(QStringLiteral("background: #556677")));
    QVERIFY(popup->styleSheet().contains(QStringLiteral("QScrollBar:horizontal")));
    QVERIFY(popup->styleSheet().contains(QStringLiteral("QScrollBar::handle:horizontal")));
    QVERIFY(popup->styleSheet().contains(QStringLiteral("border: %1px solid #778899")
                                             .arg(UiConfig::PopupOutlineStrokeWidth)));
    QVERIFY(popup->styleSheet().contains(QStringLiteral("border-radius: 11px")));
    popup->hide();
}

void TestEditorUi::constant_completion_popup_uses_configured_surface_colors()
{
    const QColor background(QStringLiteral("#334455"));
    const QColor foreground(QStringLiteral("#f4f7fb"));
    const QColor scrollbarThumb(QStringLiteral("#556677"));
    const QColor scrollbarThumbForeground(QStringLiteral("#ffffff"));
    const QColor selectedRow(QStringLiteral("#667788"));
    const QColor selectedRowForeground(QStringLiteral("#101418"));
    const QColor outline(QStringLiteral("#778899"));
    const int cornerRadius = 11;

    Editor editor;
    editor.setThemeCompletionColors(background,
                                    foreground,
                                    scrollbarThumb,
                                    scrollbarThumbForeground,
                                    selectedRow,
                                    selectedRowForeground,
                                    outline,
                                    cornerRadius);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);

    QList<QTreeWidget*> popupTrees;
    QTRY_VERIFY_WITH_TIMEOUT(!(popupTrees = s_constantCompletionPopupTrees()).isEmpty(), 1000);
    QCOMPARE(popupTrees.count(), 2);
    for (QTreeWidget* popup : popupTrees) {
        QCOMPARE(popup->palette().color(QPalette::Base), background);
        QCOMPARE(popup->palette().color(QPalette::Text), foreground);
        QCOMPARE(popup->palette().color(QPalette::Highlight), selectedRow);
        QCOMPARE(popup->palette().color(QPalette::HighlightedText), selectedRowForeground);
        QCOMPARE(popup->cursor().shape(), Qt::PointingHandCursor);
        QCOMPARE(popup->viewport()->cursor().shape(), Qt::PointingHandCursor);
        QVERIFY(popup->styleSheet().contains(QStringLiteral("background: #556677")));
        QVERIFY(popup->styleSheet().contains(QStringLiteral("QScrollBar:horizontal")));
        QVERIFY(popup->styleSheet().contains(QStringLiteral("QScrollBar::handle:horizontal")));
        QVERIFY(popup->styleSheet().contains(QStringLiteral("border: %1px solid #778899")
                                                 .arg(UiConfig::PopupOutlineStrokeWidth)));
        QVERIFY(popup->styleSheet().contains(QStringLiteral("border-radius: 11px")));
    }

    if (QWidget* popup = QApplication::activePopupWidget())
        popup->hide();
}

void TestEditorUi::inactive_editor_does_not_show_completion_popup()
{
    QWidget container;
    QVBoxLayout layout(&container);
    Editor firstEditor;
    Editor secondEditor;
    layout.addWidget(&firstEditor);
    layout.addWidget(&secondEditor);
    container.show();
    QVERIFY(QTest::qWaitForWindowExposed(&container));

    firstEditor.setFocus();
    firstEditor.setText(QStringLiteral("co"));
    firstEditor.setCursorPosition(firstEditor.text().size());
    secondEditor.setFocus();
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(
        &firstEditor,
        "triggerAutoComplete",
        Qt::DirectConnection));
    QCoreApplication::processEvents();

    QVERIFY(s_completionPopupTree() == nullptr);
    QTest::qWait(250);
    QVERIFY(s_completionPopupTree() == nullptr);
}

void TestEditorUi::enter_evaluates_when_cursor_is_immediately_after_operator()
{
    struct Case {
        QString expression;
        QChar op;
    };

    const QList<Case> cases = {
        { QStringLiteral("2 + 3"), QLatin1Char('+') },
        { QString::fromUtf8("2 − 3"), QChar(MathDsl::SubOp) },
        { QStringLiteral("2 / 3"), QLatin1Char('/') },
        { QString::fromUtf8("2 × 3"), QChar(MathDsl::MulCrossOp) },
        { QString::fromUtf8("2 [rad·s⁻¹]"), QChar(MathDsl::MulDotOp) },
        { QStringLiteral("2 [rad/s]"), QLatin1Char('/') },
    };

    for (const Case& c : cases) {
        Editor editor;
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));
        editor.setFocus();

        editor.setText(c.expression);
        const int opPos = c.expression.indexOf(c.op);
        QVERIFY2(opPos >= 0, qPrintable(QStringLiteral("Operator not found in expression: %1").arg(c.expression)));
        editor.setCursorPosition(opPos + 1);

        QSignalSpy returnPressedSpy(&editor, SIGNAL(returnPressed()));

        QTest::keyClick(&editor, Qt::Key_Return, Qt::NoModifier);
        QCoreApplication::processEvents();

        QCOMPARE(returnPressedSpy.count(), 1);
        QCOMPARE(editor.text(), c.expression);
    }
}

void TestEditorUi::completion_popup_uses_expected_icons_for_all_symbol_types()
{
    Settings* settings = Settings::instance();
    const bool builtInFnBackup = settings->autoCompletionBuiltInFunctions;
    const bool builtInVarBackup = settings->autoCompletionBuiltInVariables;
    const bool userFnBackup = settings->autoCompletionUserFunctions;
    const bool userVarBackup = settings->autoCompletionUserVariables;
    settings->autoCompletionBuiltInFunctions = true;
    settings->autoCompletionBuiltInVariables = true;
    settings->autoCompletionUserFunctions = true;
    settings->autoCompletionUserVariables = true;
    struct SettingsRestoreGuard {
        Settings* settings;
        bool builtInFn;
        bool builtInVar;
        bool userFn;
        bool userVar;
        ~SettingsRestoreGuard()
        {
            settings->autoCompletionBuiltInFunctions = builtInFn;
            settings->autoCompletionBuiltInVariables = builtInVar;
            settings->autoCompletionUserFunctions = userFn;
            settings->autoCompletionUserVariables = userVar;
        }
    } settingsRestoreGuard {settings, builtInFnBackup, builtInVarBackup, userFnBackup, userVarBackup};

    Session session;
    Evaluator* evaluator = session.evaluator();
    evaluator->unsetAllUserUnits();
    evaluator->unsetAllUserFunctions();
    evaluator->unsetVariable(QStringLiteral("icon_user_var"));
    evaluator->unsetVariable(QStringLiteral("s"));
    evaluator->setExpression(QStringLiteral("icon_user_var = 7"));
    QVERIFY(!evaluator->eval().isNan());
    evaluator->setExpression(QStringLiteral("s = 9"));
    QVERIFY(!evaluator->eval().isNan());
    evaluator->setVariable(QStringLiteral("ans"), Quantity(9), Variable::BuiltIn);
    evaluator->setUserFunction(UserFunction(
        QStringLiteral("icon_user_func"),
        QStringList() << QStringLiteral("t"),
        QStringLiteral("t")));
    evaluator->setExpression(QStringLiteral("2[m]"));
    const Quantity iconUserUnitValue = evaluator->eval();
    QVERIFY(!iconUserUnitValue.isNan());
    evaluator->setUserUnit(UserUnit(
        QStringLiteral("icon_user_unit"),
        iconUserUnitValue,
        QStringLiteral("2[m]"),
        QStringLiteral("[icon_user_unit]=2[m]")));

    Editor editor;
    editor.setSession(&session);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("si"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(&editor, "triggerAutoComplete", Qt::DirectConnection));
    QTreeWidget* popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QCOMPARE(s_popupSymbolForIdentifier(popup, QStringLiteral("sin")), QString::fromUtf8("📚 ƒ"));
    popup->hide();

    editor.setText(QStringLiteral("icon_user_f"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(&editor, "triggerAutoComplete", Qt::DirectConnection));
    popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QCOMPARE(s_popupSymbolForIdentifier(popup, QStringLiteral("icon_user_func")), QString::fromUtf8("👤 ƒ"));
    popup->hide();

    editor.setText(QStringLiteral("a"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(&editor, "triggerAutoComplete", Qt::DirectConnection));
    popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QCOMPARE(s_popupSymbolForIdentifier(popup, QStringLiteral("ans")), QString::fromUtf8("📏 𝑘"));
    popup->hide();

    editor.setText(QStringLiteral("icon_user_"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(&editor, "triggerAutoComplete", Qt::DirectConnection));
    popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QCOMPARE(s_popupSymbolForIdentifier(popup, QStringLiteral("icon_user_var")), QString::fromUtf8("👤 𝑥"));
    popup->hide();

    editor.setText(QStringLiteral("s"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(&editor, "triggerAutoComplete", Qt::DirectConnection));
    popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QCOMPARE(s_popupSymbolForIdentifier(popup, QStringLiteral("s")), QString::fromUtf8("👤 𝑥"));
    popup->hide();

    editor.setText(QStringLiteral("[met"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(&editor, "triggerAutoComplete", Qt::DirectConnection));
    popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QCOMPARE(s_popupSymbolForIdentifier(popup, QStringLiteral("metre")), QString::fromUtf8("📚 𝒖"));
    popup->hide();

    editor.setText(QStringLiteral("[icon_user_u"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(&editor, "triggerAutoComplete", Qt::DirectConnection));
    popup = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((popup = s_completionPopupTree()) != nullptr, 1000);
    QCOMPARE(s_popupSymbolForIdentifier(popup, QStringLiteral("icon_user_unit")), QString::fromUtf8("👤 𝒖"));
    popup->hide();

}

void TestEditorUi::wrap_selection_method_wraps_selected_text()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1+2"));
    QTextCursor cursor = editor.textCursor();
    cursor.setPosition(0);
    cursor.setPosition(1, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);

    editor.wrapSelection();
    QCOMPARE(editor.text(), QStringLiteral("(1)+2"));
}

void TestEditorUi::wrap_selection_method_wraps_whole_expression_without_selection()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1+2"));
    editor.setCursorPosition(0);

    editor.wrapSelection();
    QCOMPARE(editor.text(), QStringLiteral("(1+2)"));
}

void TestEditorUi::matching_parentheses_use_parens_and_generated_foreground_colors()
{
    const QColor parensColor(QStringLiteral("#c8a2c8"));
    const QColor resultBackground(QStringLiteral("#102030"));
    const QColor generatedForeground = aaForegroundForBackground(parensColor);
    const ColorScheme scheme = ColorScheme::fromJsonObject(themeJson(QJsonObject{
        {QStringLiteral("parens"), parensColor.name()},
        {QStringLiteral("background"), QStringLiteral("#010203")}
    }));
    QVERIFY(scheme.isValid());

    Editor editor;
    editor.setThemeSurfaceColor(QColor(QStringLiteral("#203040")), resultBackground);
    editor.setThemePreviewColorScheme(scheme);
    editor.setText(QStringLiteral("()"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "doMatchingPar",
        Qt::DirectConnection));

    const QList<QTextEdit::ExtraSelection> selections = editor.extraSelections();
    QCOMPARE(selections.count(), 2);
    for (const QTextEdit::ExtraSelection& selection : selections) {
        QCOMPARE(selection.format.background().color(), parensColor);
        QCOMPARE(selection.format.foreground().color(), generatedForeground);
        QVERIFY(selection.format.foreground().color() != resultBackground);
    }
}

void TestEditorUi::keeps_wrapped_cursor_line_visible_at_height_cap()
{
    Editor editor;
    editor.setFixedWidth(220);
    editor.resize(220, 80);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral(
        "12 + 312 + 3 + 12 + 312 + 3 + 123 + 123 + 12 + 3 + 123 + "
        "123 + 14 + 2 + 123 + 12 + 3 + 12 + 3 + 12 + 3 + "
        "345345345 + 345345 + 345345345 + 345345345453453453434 + "
        "234 + 234 + 234 + 23423423 + 4234 + 234"));
    editor.setCursorPosition(editor.text().size());

    QTRY_VERIFY(editor.document()->begin().layout()->lineCount() > 5);
    QTRY_VERIFY(editor.cursorRect().top() >= editor.viewport()->rect().top());
    QTRY_VERIFY(editor.cursorRect().bottom() <= editor.viewport()->rect().bottom());
}

void TestEditorUi::editor_height_adds_only_one_line_height_per_visible_line()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));

    editor.setText(QStringLiteral("1"));
    const int singleLineHeight = editor.height();

    editor.setText(QStringLiteral("1\n2\n3\n4\n5"));
    QCOMPARE(editor.height() - singleLineHeight, 4 * editor.fontMetrics().lineSpacing());
}

void TestEditorUi::editor_keeps_unit_descenders_inside_viewport()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("2 [gypq]"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(editor.cursorRect().bottom() < editor.viewport()->rect().bottom());
}

void TestEditorUi::adding_second_wrapped_character_keeps_first_line_visible()
{
    Editor editor;
    editor.setFixedWidth(260);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QString prefix(QStringLiteral("12345678901234567890"));
    editor.setText(prefix);
    editor.setCursorPosition(editor.text().size());

    while (editor.document()->begin().layout()->lineCount() < 2)
        editor.insert(QStringLiteral("1"));

    const int firstWrappedScrollValue = editor.verticalScrollBar()->value();
    editor.insert(QStringLiteral("2"));

    QCOMPARE(editor.document()->begin().layout()->lineCount(), 2);
    QCOMPARE(firstWrappedScrollValue, 0);
    QCOMPARE(editor.verticalScrollBar()->value(), 0);
}

void TestEditorUi::editor_fill_color_is_15_percent_lighter_for_dark_background_role()
{
    Settings* settings = Settings::instance();
    const QString oldColorScheme = settings->colorScheme;
    const QString oldCustomColorSchemeJson = settings->customColorSchemeJson;

    QJsonObject colors;
    colors.insert(QStringLiteral("background"), QStringLiteral("#202020"));
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(colors);

    Editor editor;
    editor.rehighlight();

    const QColor expected = QColor(QStringLiteral("#202020")).lighter(115);
    const QColor activeBase = editor.palette().color(QPalette::Active, QPalette::Base);
    const QColor inactiveBase = editor.palette().color(QPalette::Inactive, QPalette::Base);
    QCOMPARE(activeBase.name(QColor::HexArgb), expected.name(QColor::HexArgb));
    QCOMPARE(inactiveBase.name(QColor::HexArgb), expected.name(QColor::HexArgb));

    settings->colorScheme = oldColorScheme;
    settings->customColorSchemeJson = oldCustomColorSchemeJson;
}

void TestEditorUi::editor_fill_color_is_15_percent_darker_for_light_background_role()
{
    Settings* settings = Settings::instance();
    const QString oldColorScheme = settings->colorScheme;
    const QString oldCustomColorSchemeJson = settings->customColorSchemeJson;

    QJsonObject colors;
    colors.insert(QStringLiteral("background"), QStringLiteral("#d0d0d0"));
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = themeJsonString(colors);

    Editor editor;
    editor.rehighlight();

    const QColor expected = QColor(QStringLiteral("#d0d0d0")).darker(115);
    const QColor activeBase = editor.palette().color(QPalette::Active, QPalette::Base);
    const QColor inactiveBase = editor.palette().color(QPalette::Inactive, QPalette::Base);
    QCOMPARE(activeBase.name(QColor::HexArgb), expected.name(QColor::HexArgb));
    QCOMPARE(inactiveBase.name(QColor::HexArgb), expected.name(QColor::HexArgb));

    settings->colorScheme = oldColorScheme;
    settings->customColorSchemeJson = oldCustomColorSchemeJson;
}

void TestEditorUi::themed_cursor_has_only_two_states_at_display_scale_data()
{
    QTest::addColumn<qreal>("scale");
    QTest::newRow("100-percent") << qreal(1);
    QTest::newRow("125-percent") << qreal(1.25);
    QTest::newRow("150-percent") << qreal(1.5);
    QTest::newRow("200-percent") << qreal(2);
}

void TestEditorUi::themed_cursor_has_only_two_states_at_display_scale()
{
    QFETCH(qreal, scale);
    Editor editor;
    editor.setAutoCalcEnabled(false);
    editor.setAutoCompletionEnabled(false);
    const QColor primary(QStringLiteral("#ff3377"));
    editor.setThemePrimaryColor(primary, false);
    editor.setText(QStringLiteral("123"));
    editor.setCursorPosition(3);
    editor.resize(400, editor.height());
    editor.show();
    editor.activateWindow();
    editor.setFocus();
    QTRY_VERIFY(editor.hasFocus());

    const QRect cursor = editor.cursorRect();
    const QRect themedCursor(cursor.x() + (cursor.width() - 2) / 2,
                             cursor.y(), 2, cursor.height());
    const QRect sample(qFloor((themedCursor.left() - 2) * scale),
                       qFloor(themedCursor.center().y() * scale),
                       qCeil(6 * scale), 1);
    const auto render = [&]() {
        QImage image(QSize(qCeil(editor.viewport()->width() * scale),
                           qCeil(editor.viewport()->height() * scale)),
                     QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(scale);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        editor.viewport()->render(&painter);
        return image;
    };
    editor.setCustomCursorVisible(false);
    const QImage hidden = render();
    QImage visible = hidden.copy();
    {
        QPainter painter(&visible);
        painter.fillRect(themedCursor, primary);
    }
    const QImage hiddenSample = hidden.copy(sample);
    const QImage visibleSample = visible.copy(sample);
    QVERIFY(hiddenSample != visibleSample);
    // Even a hidden custom cursor must not leave Qt's independently blinking
    // zero-width cursor behind at fractional display scales.
    const int flashTime = qMax(1000, QApplication::cursorFlashTime());
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < flashTime) {
        QVERIFY(render().copy(sample) == hiddenSample);
        QTest::qWait(20);
    }

    editor.setCustomCursorVisible(true);
    bool sawVisible = false;
    bool sawHidden = false;
    elapsed.restart();
    while (elapsed.elapsed() < flashTime * 2) {
        const QImage actual = render().copy(sample);
        QVERIFY2(actual == visibleSample || actual == hiddenSample,
                 "Cursor shows an extra color or width during its blink cycle");
        sawVisible |= actual == visibleSample;
        sawHidden |= actual == hiddenSample;
        QTest::qWait(20);
    }
    QVERIFY(sawVisible);
    QVERIFY(sawHidden);
}

void TestEditorUi::cursor_free_painting_preserves_text_and_selections_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("selectText");
    QTest::newRow("placeholder") << QString() << false;
    QTest::newRow("syntax-highlighting") << QStringLiteral("sin(123) + 456") << false;
    QTest::newRow("selected-wrapped-text")
        << QStringLiteral("123 + 456 + 789 + 123 + 456 + 789 + 123 + 456\n123 + 456") << true;
    QTest::newRow("full-width-selection") << QStringLiteral("123\n456\n789") << false;
}

void TestEditorUi::cursor_free_painting_preserves_text_and_selections()
{
    QFETCH(QString, text);
    QFETCH(bool, selectText);
    class NativePaintingEditor : public Editor {
    protected:
        void paintEvent(QPaintEvent* event) override { QPlainTextEdit::paintEvent(event); }
    };
    NativePaintingEditor reference;
    Editor editor;
    for (Editor* target : { static_cast<Editor*>(&reference), &editor }) {
        target->setAutoCalcEnabled(false);
        target->setAutoCompletionEnabled(false);
        target->setThemePrimaryColor(QColor(QStringLiteral("#ff3377")), false);
        target->setPlaceholderText(QStringLiteral("Enter an expression"));
        target->setText(text);
        target->setCursorPosition(0);
        target->setCustomCursorVisible(false);
        target->setTextInteractionFlags(Qt::TextSelectableByMouse);
        target->resize(220, target->height());
        target->show();
        if (selectText)
            target->selectAll();
        if (qstrcmp(QTest::currentDataTag(), "full-width-selection") == 0) {
            QTextEdit::ExtraSelection selection;
            selection.cursor = target->textCursor();
            selection.cursor.setPosition(4);
            selection.format.setBackground(QColor(QStringLiteral("#557799")));
            selection.format.setProperty(QTextFormat::FullWidthSelection, true);
            target->setExtraSelections({ selection });
            // Exercise explicit block backgrounds as well as extra selections.
            QTextCursor blockCursor(target->document());
            QTextBlockFormat format;
            format.setBackground(QColor(QStringLiteral("#774455")));
            blockCursor.setBlockFormat(format);
        }
    }
    QCoreApplication::processEvents();
    reference.clearFocus();
    editor.clearFocus();
    const QImage expected = reference.viewport()->grab().toImage();
    const QImage actual = editor.viewport()->grab().toImage();
    QVERIFY2(actual == expected,
             "Cursor-free painting changes Qt's text, background, or selection rendering");
}

void TestEditorUi::input_digit_grouping_formats_only_integer_digits_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QList<int>>("positions");
    QTest::newRow("short") << QStringLiteral("999") << QList<int>{};
    QTest::newRow("thousand") << QStringLiteral("1000") << QList<int>{0};
    QTest::newRow("million") << QStringLiteral("1234567") << QList<int>{0, 3};
    QTest::newRow("fraction") << QStringLiteral("1234.56789") << QList<int>{0};
    QTest::newRow("comma-fraction") << QStringLiteral("1234,56789") << QList<int>{0};
    QTest::newRow("fraction-only") << QStringLiteral("0.1234567") << QList<int>{};
    QTest::newRow("scientific") << QStringLiteral("1000e1234") << QList<int>{0};
    QTest::newRow("hex") << QStringLiteral("0x12345") << QList<int>{2};
    QTest::newRow("hex-alias") << QStringLiteral("#12345") << QList<int>{1};
    QTest::newRow("binary") << QStringLiteral("0b10101010") << QList<int>{5};
    QTest::newRow("octal") << QStringLiteral("0o1234567") << QList<int>{2, 5};
    QTest::newRow("decimal-prefix") << QStringLiteral("0d1234567") << QList<int>{2, 5};
    QTest::newRow("expression") << QStringLiteral("1000 + 20000") << QList<int>{0, 8};
    QTest::newRow("identifier-comment") << QStringLiteral("x1234 ? 1000") << QList<int>{};
    QTest::newRow("unit-power") << QStringLiteral("1000[m²] + 2000") << QList<int>{0, 11};
    QTest::newRow("manual-spaces") << QStringLiteral("1 234 567") << QList<int>{};
    QTest::newRow("manual-underscores") << QStringLiteral("1_234_567") << QList<int>{};
}

void TestEditorUi::input_digit_grouping_formats_only_integer_digits()
{
    QFETCH(QString, input);
    QFETCH(QList<int>, positions);
    Settings* settings = Settings::instance();
    settings->numberFormatStyle = Settings::NumberFormatThreeDigitSpaceDot;
    settings->applyNumberFormatStyle();
    QPlainTextEdit inputEdit;
    SyntaxHighlighter inputHighlighter(&inputEdit, true);
    inputEdit.setPlainText(input);
    QPlainTextEdit display;
    SyntaxHighlighter displayHighlighter(&display);
    display.setPlainText(input);

    auto groupedPositions = [](QPlainTextEdit& edit) {
        QList<int> result;
        for (const auto& range : edit.document()->firstBlock().layout()->formats()) {
            if (range.format.hasProperty(SyntaxHighlighter::InputDigitSeparator)) {
                for (int pos = range.start; pos < range.start + range.length; ++pos)
                    result.append(pos);
            }
        }
        return result;
    };
    for (bool highlighting : {true, false}) {
        settings->syntaxHighlighting = highlighting;
        for (bool grouping : {false, true, false}) {
            settings->inputDigitGrouping = grouping;
            inputHighlighter.rehighlight();
            displayHighlighter.rehighlight();
            QCOMPARE(groupedPositions(inputEdit), grouping ? positions : QList<int>{});
            QVERIFY(groupedPositions(display).isEmpty());
            QCOMPARE(inputEdit.toPlainText(), input);
        }
    }
}

void TestEditorUi::input_digit_grouping_preserves_typing_selection_and_undo()
{
    Settings::instance()->numberFormatStyle = Settings::NumberFormatThreeDigitSpaceDot;
    Settings::instance()->applyNumberFormatStyle();
    Settings::instance()->inputDigitGrouping = true;
    Editor editor;
    editor.setAutoCalcEnabled(false);
    editor.setAutoCompletionEnabled(false);
    editor.show();
    QTest::keyClicks(&editor, "1000");
    editor.rehighlight();
    QCOMPARE(editor.text(), QStringLiteral("1000"));
    QCOMPARE(editor.textCursor().position(), 4);
    bool hasGap = false;
    for (const auto& range : editor.document()->firstBlock().layout()->formats())
        hasGap |= range.format.hasProperty(SyntaxHighlighter::InputDigitSeparator);
    QVERIFY(hasGap);

    QTest::keyClick(&editor, Qt::Key_Left);
    QCOMPARE(editor.textCursor().position(), 3);
    QTest::keyClick(&editor, Qt::Key_Backspace);
    QCOMPARE(editor.text(), QStringLiteral("100"));
    editor.undo();
    QCOMPARE(editor.text(), QStringLiteral("1000"));
    editor.redo();
    QCOMPARE(editor.text(), QStringLiteral("100"));
    editor.undo();
    editor.selectAll();
    editor.copy();
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("1000"));
    const QTextCursor before = editor.textCursor();
    Settings::instance()->inputDigitGrouping = false;
    editor.rehighlight();
    QCOMPARE(editor.textCursor().position(), before.position());
    QCOMPARE(editor.textCursor().anchor(), before.anchor());
    editor.undo();
    QCOMPARE(editor.text(), QStringLiteral("100"));
}

void TestEditorUi::input_digit_grouping_follows_number_format_data()
{
    QTest::addColumn<int>("style");
    QTest::addColumn<QString>("expected");
    QTest::newRow("ungrouped-dot") << int(Settings::NumberFormatNoGroupingDot) << QStringLiteral("1234567.1234567");
    QTest::newRow("ungrouped-comma") << int(Settings::NumberFormatNoGroupingComma) << QStringLiteral("1234567,1234567");
    QTest::newRow("si-dot") << int(Settings::NumberFormatSIDot) << QStringLiteral("1 234 567.123 456 7");
    QTest::newRow("si-comma") << int(Settings::NumberFormatSIComma) << QStringLiteral("1 234 567,123 456 7");
    QTest::newRow("comma-dot") << int(Settings::NumberFormatThreeDigitCommaDot) << QStringLiteral("1,234,567.1234567");
    QTest::newRow("comma-dot-fraction") << int(Settings::NumberFormatThreeDigitCommaDotFraction) << QStringLiteral("1,234,567.123,456,7");
    QTest::newRow("dot-comma") << int(Settings::NumberFormatThreeDigitDotComma) << QStringLiteral("1.234.567,1234567");
    QTest::newRow("dot-comma-fraction") << int(Settings::NumberFormatThreeDigitDotCommaFraction) << QStringLiteral("1.234.567,123.456.7");
    QTest::newRow("space-dot") << int(Settings::NumberFormatThreeDigitSpaceDot) << QStringLiteral("1 234 567.1234567");
    QTest::newRow("space-comma") << int(Settings::NumberFormatThreeDigitSpaceComma) << QStringLiteral("1 234 567,1234567");
    QTest::newRow("underscore-dot") << int(Settings::NumberFormatThreeDigitUnderscoreDot) << QStringLiteral("1_234_567.1234567");
    QTest::newRow("underscore-dot-fraction") << int(Settings::NumberFormatThreeDigitUnderscoreDotFraction) << QStringLiteral("1_234_567.123_456_7");
    QTest::newRow("underscore-comma") << int(Settings::NumberFormatThreeDigitUnderscoreComma) << QStringLiteral("1_234_567,1234567");
    QTest::newRow("underscore-comma-fraction") << int(Settings::NumberFormatThreeDigitUnderscoreCommaFraction) << QStringLiteral("1_234_567,123_456_7");
    QTest::newRow("indian") << int(Settings::NumberFormatIndianCommaDot) << QStringLiteral("12,34,567.1234567");
}

void TestEditorUi::input_digit_grouping_follows_number_format()
{
    QFETCH(int, style);
    QFETCH(QString, expected);
    Settings* settings = Settings::instance();
    settings->numberFormatStyle = static_cast<Settings::NumberFormatStyle>(style);
    settings->applyNumberFormatStyle();
    settings->inputDigitGrouping = true;
    const QString raw = QStringLiteral("1234567") + QChar(settings->decimalSeparator())
        + QStringLiteral("1234567");
    Editor editor;
    editor.setText(raw);
    editor.rehighlight();
    const auto displayedText = [&editor]() {
        QString display = editor.text();
        const auto ranges = editor.document()->firstBlock().layout()->formats();
        for (auto it = ranges.crbegin(); it != ranges.crend(); ++it) {
            const QString radix = it->format.stringProperty(SyntaxHighlighter::InputRadixReplacement);
            if (!radix.isEmpty())
                display.replace(it->start, 1, radix);
            const QString separator = it->format.stringProperty(SyntaxHighlighter::InputDigitSeparator);
            if (!separator.isEmpty()) {
                for (int pos = it->start + it->length - 1; pos >= it->start; --pos)
                    display.insert(pos + 1, separator);
            }
        }
        return display;
    };
    for (bool syntaxHighlighting : {true, false}) {
        settings->syntaxHighlighting = syntaxHighlighting;
        editor.rehighlight();
        QCOMPARE(displayedText(), expected);
        QCOMPARE(editor.text(), raw);
        QCOMPARE(NumberFormatter::formatNumericLiteralForDisplay(raw), expected);
    }
    if (settings->decimalSeparator() == ',') {
        editor.setText(QStringLiteral("1234567.1234567"));
        editor.rehighlight();
        QCOMPARE(displayedText(), expected);
    }
    editor.setText(expected);
    editor.rehighlight();
    QCOMPARE(displayedText(), expected);
    settings->inputDigitGrouping = false;
    editor.setText(raw);
    editor.rehighlight();
    QCOMPARE(displayedText(), raw);
}

void TestEditorUi::input_digit_grouping_paints_separator_glyphs_data()
{
    QTest::addColumn<bool>("themed");
    QTest::addColumn<bool>("selected");
    QTest::newRow("native") << false << false;
    QTest::newRow("native-selected") << false << true;
    QTest::newRow("themed") << true << false;
    QTest::newRow("themed-selected") << true << true;
}

void TestEditorUi::input_digit_grouping_paints_separator_glyphs()
{
    QFETCH(bool, themed);
    QFETCH(bool, selected);
    Settings* settings = Settings::instance();
    settings->inputDigitGrouping = true;
    Editor editor;
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(20);
    editor.setFont(font);
    editor.setAutoCalcEnabled(false);
    editor.setAutoCompletionEnabled(false);
    editor.resize(600, 120);
    editor.show();
    editor.clearFocus();
    editor.setThemePrimaryColor(themed ? QColor(QStringLiteral("#2277aa")) : QColor(), false);
    for (auto style : {Settings::NumberFormatThreeDigitCommaDot,
                       Settings::NumberFormatThreeDigitDotComma,
                       Settings::NumberFormatThreeDigitUnderscoreDot}) {
        settings->numberFormatStyle = style;
        settings->applyNumberFormatStyle();
        editor.setText(QStringLiteral("1234567.56"));
        editor.rehighlight();
        if (selected)
            editor.selectAll();
        QCoreApplication::processEvents();
        editor.clearFocus();
        const auto separatorWidth = [&editor]() {
            for (const auto& range : editor.document()->firstBlock().layout()->formats()) {
                if (range.format.hasProperty(SyntaxHighlighter::InputDigitSeparator))
                    return range.format.fontLetterSpacing();
            }
            return qreal(0);
        };
        const qreal previousWidth = separatorWidth();
        editor.increaseFontPointSize();
        QVERIFY(separatorWidth() > previousWidth);
        const QImage withSeparators = editor.viewport()->grab().toImage();
        QTextLayout* layout = editor.document()->firstBlock().layout();
        auto formats = layout->formats();
        for (auto& range : formats) {
            range.format.clearProperty(SyntaxHighlighter::InputDigitSeparator);
            range.format.clearProperty(SyntaxHighlighter::InputRadixReplacement);
        }
        layout->setFormats(formats);
        editor.viewport()->update();
        const QImage withoutSeparators = editor.viewport()->grab().toImage();
        QVERIFY2(withSeparators != withoutSeparators, "Input separators must paint glyphs, not just gaps");
    }
}

void TestEditorUi::input_digit_grouping_survives_settings_reload()
{
    Settings* settings = Settings::instance();
    QVERIFY(settings->inputDigitGrouping);
    for (bool enabled : {true, false}) {
        settings->inputDigitGrouping = enabled;
        settings->numberFormatStyle = Settings::NumberFormatThreeDigitCommaDot;
        settings->applyNumberFormatStyle();
        QCOMPARE(settings->inputDigitGrouping, enabled);
        settings->save();
        settings->inputDigitGrouping = !enabled;
        settings->load();
        QCOMPARE(settings->inputDigitGrouping, enabled);
    }
}

int main(int argc, char** argv)
{
    return UiTestFixture::run<TestEditorUi>(argc, argv);
}
#include "testeditorui.moc"
