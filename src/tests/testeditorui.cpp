// This file is part of the SpeedCrunch project
// Copyright (C) 2026 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "gui/editor.h"
#include "gui/editorutils.h"
#include "core/evaluator.h"
#include "core/settings.h"
#include "core/unicodechars.h"
#include "math/operatorchars.h"

#include <QApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QTest>

class TestEditorUi : public QObject {
    Q_OBJECT

private slots:
    void blocks_consecutive_plus();
    void blocks_consecutive_caret();
    void blocks_plus_after_caret();
    void blocks_leading_operators_when_auto_ans_is_off_except_configured_exceptions();
    void keeps_disallowed_start_chars_blocked_when_auto_ans_is_on();
    void auto_ans_rewrite_helper_handles_tilde_and_factorial();
    void blocks_operator_right_after_open_square_bracket();
    void inserts_value_unit_space_brackets_after_number_or_symbol();
    void ignores_space_on_empty_or_all_space_editor();
    void auto_inserts_space_before_question_comment_only_with_non_space_content();
    void inserts_middle_dot_on_space_after_identifier_or_closed_group();
    void inserts_parenthesis_pair_and_places_cursor_inside();
    void inserts_implicit_mul_sequence_before_open_paren_after_number_or_symbol();
    void does_not_insert_implicit_mul_for_zero_radix_prefix_letters();
    void allows_unit_conversion_tail_after_spaced_subtraction_operator();
    void inserts_unit_conversion_with_placeholder_when_typing_arrow_symbol();
    void treats_spaced_unit_conversion_as_atomic_navigation_and_edit_token();
    void treats_spaced_question_comment_as_atomic_navigation_and_edit_token();
    void treats_spaced_equal_as_atomic_navigation_and_edit_token();
    void treats_leading_question_comment_as_atomic_navigation_and_edit_token();
    void ignores_space_right_after_spaced_unit_conversion_operator();
    void unit_bracket_context_accepts_div_mul_and_rejects_addition();
    void unit_bracket_context_allows_letter_after_middle_dot();
    void unit_bracket_context_disallows_variables_and_constants();
    void unit_bracket_context_allows_digits_and_minus_only_in_exponent_positions();
    void converts_caret_exponents_to_superscripts_globally();
    void rewrites_superscript_exponent_for_radix_and_inserts_mul_space_globally();
    void ignores_dot_and_comma_after_non_numerical_char();
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
    void completes_pi_identifier_as_pi_symbol();
    void accepts_degree_alias_in_unit_brackets_and_normalizes_to_degree_sign();
    void offers_unit_completion_for_degree_symbol_in_unit_context();
    void unit_context_completion_includes_angle_units_and_long_forms();
    void completes_are_unit_to_short_form_in_unit_context();
    void completes_day_unit_to_short_form_in_unit_context();
    void completes_hour_unit_to_short_form_in_unit_context();
    void completes_affine_temperature_units_in_unit_context();
    void enter_evaluates_when_completion_popup_has_no_explicit_interaction();
};

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
    QVERIFY(afterFirstPlus.contains(OperatorChars::AdditionSign));

    QTest::keyClick(&editor, Qt::Key_Plus, Qt::NoModifier);
    QCOMPARE(editor.text(), afterFirstPlus);
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
    sendTextKey(QString(OperatorChars::AdditionSign));
    QCOMPARE(editor.document()->toRawText(), QString());

    resetEmpty();
    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QString());
    resetEmpty();
    sendTextKey(QString(OperatorChars::MulCrossSign));
    QCOMPARE(editor.document()->toRawText(), QString());
    resetEmpty();
    sendTextKey(QString(OperatorChars::MulDotSign));
    QCOMPARE(editor.document()->toRawText(), QString());

    resetEmpty();
    QTest::keyClick(&editor, Qt::Key_Slash, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QString());
    resetEmpty();
    sendTextKey(QString(OperatorChars::DivisionSign));
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
    sendTextKey(QString(OperatorChars::SubtractionSign));
    QVERIFY(!editor.document()->toRawText().isEmpty());
    const QString afterFirstLeadingMinusByImeBase = editor.document()->toRawText();
    sendImeCommit(QString(OperatorChars::SubtractionSign));
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
    QCOMPARE(actualAfterNumber, QStringLiteral("2") + QString(OperatorChars::ValueUnitSpace) + QStringLiteral("[]"));

    editor.setText(QString::fromUtf8("π"));
    editor.setCursorPosition(editor.text().size());

    QApplication::sendEvent(&editor, &bracketByText);

    const QString actualAfterSymbol = editor.document()->toRawText();
    QCOMPARE(actualAfterSymbol, QString::fromUtf8("π") + QString(OperatorChars::ValueUnitSpace) + QStringLiteral("[]"));

    editor.setText(QStringLiteral("2")
                   + QString(OperatorChars::ValueUnitSpace)
                   + QStringLiteral("[m]")
                   + QString(OperatorChars::MulDotSpace)
                   + QString(OperatorChars::MulDotSign)
                   + QString(OperatorChars::MulDotSpace)
                   + QStringLiteral("(pi)"));
    editor.setCursorPosition(editor.text().size());
    QApplication::sendEvent(&editor, &bracketByText);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("2")
                 + QString(OperatorChars::ValueUnitSpace)
                 + QStringLiteral("[m]")
                 + QString(OperatorChars::MulDotSpace)
                 + QString(OperatorChars::MulDotSign)
                 + QString(OperatorChars::MulDotSpace)
                 + QStringLiteral("(pi)")
                 + QString(OperatorChars::ValueUnitSpace)
                 + QStringLiteral("[]"));

    editor.setText(QStringLiteral("2 [K] in "));
    editor.setCursorPosition(editor.text().size());
    QApplication::sendEvent(&editor, &bracketByText);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("2 [K] in")
                 + QString(OperatorChars::ValueUnitSpace)
                 + QStringLiteral("[]"));
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
        + QString(OperatorChars::ValueUnitSpace)
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
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("pi"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("pi "));

    editor.setText(QStringLiteral("cos(3)"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("cos(3) "));
}

void TestEditorUi::inserts_parenthesis_pair_and_places_cursor_inside()
{
    // State: multiple states (empty, numbers, before operator, function name).
    // Action: type '(' (text path and keycode path).
    // Expected: auto-insert "()", cursor lands between.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const auto verifyParenInsertionAt = [&editor](const QString& initialText, int cursorPos) {
        editor.setText(initialText);
        editor.setCursorPosition(cursorPos);

        QKeyEvent openParenByText(
            QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("("));
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
    verifyParenInsertionAt(QStringLiteral("1+2"), 1);   // right before '+'
    verifyParenInsertionAt(QStringLiteral("1)"), 1);    // right before ')'

    editor.setText(QStringLiteral("cos"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent openParenByTextForFunction(
        QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("("));
    QApplication::sendEvent(&editor, &openParenByTextForFunction);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("cos()"));
    QCOMPARE(editor.textCursor().position(), 4);

    editor.setText(QString::fromUtf8("cos³"));
    editor.setCursorPosition(editor.text().size());
    QApplication::sendEvent(&editor, &openParenByTextForFunction);
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("cos³()"));
    QCOMPARE(editor.textCursor().position(), 5);

    editor.setText(QString::fromUtf8("2 pi⁻²³ · cos"));
    editor.setCursorPosition(editor.text().size());
    QApplication::sendEvent(&editor, &openParenByTextForFunction);
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("2 pi⁻²³ · cos()"));
    QCOMPARE(editor.textCursor().position(),
             QString::fromUtf8("2 pi⁻²³ · cos(").size());

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

void TestEditorUi::inserts_implicit_mul_sequence_before_open_paren_after_number_or_symbol()
{
    // State: "2" and "π".
    // Action: type '(' via text-based key event.
    // Expected: insert "<MulDotSpace><MulDotSign><MulDotSpace>()".
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QString mulDotSequence =
        QString(OperatorChars::MulDotSpace)
        + QString(OperatorChars::MulDotSign)
        + QString(OperatorChars::MulDotSpace);
    const QString mulCrossSequence =
        QString(OperatorChars::MulCrossSpace)
        + QString(OperatorChars::MulCrossSign)
        + QString(OperatorChars::MulCrossSpace);

    auto typeOpenParenByText = [&editor]() {
        QKeyEvent openParenByText(
            QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("("));
        QApplication::sendEvent(&editor, &openParenByText);
    };

    editor.setText(QStringLiteral("2"));
    editor.setCursorPosition(editor.text().size());
    typeOpenParenByText();
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("2") + mulCrossSequence + QStringLiteral("()"));

    editor.setText(QString::fromUtf8("2³"));
    editor.setCursorPosition(editor.text().size());
    typeOpenParenByText();
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("2³") + mulCrossSequence + QStringLiteral("()"));

    editor.setText(QString::fromUtf8("π"));
    editor.setCursorPosition(editor.text().size());
    typeOpenParenByText();
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("π") + mulDotSequence + QStringLiteral("()"));

    editor.setText(QString::fromUtf8("pi³"));
    editor.setCursorPosition(editor.text().size());
    typeOpenParenByText();
    QCOMPARE(editor.document()->toRawText(), QString::fromUtf8("pi³") + mulDotSequence + QStringLiteral("()"));
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
    // Action: type '>' via text-based key event.
    // Expected: convert to spaced "→ []", cursor inside brackets, parser sees UnitConversion.
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);

    const QString afterMinus = editor.document()->toRawText();
    QVERIFY(afterMinus.contains(OperatorChars::SubtractionSign));

    QKeyEvent greaterByText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral(">"));
    QApplication::sendEvent(&editor, &greaterByText);

    const QString afterGreater = editor.document()->toRawText();
    const QString arrowSequence =
        QString(OperatorChars::SubtractionSpace)
        + QString(UnicodeChars::RightwardsArrow)
        + QString(OperatorChars::SubtractionSpace)
        + QStringLiteral("[]");
    QVERIFY(afterGreater.contains(arrowSequence));
    QVERIFY(!afterGreater.contains(QLatin1Char('>')));
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
        QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(UnicodeChars::RightwardsArrow));
    QApplication::sendEvent(&editor, &rightArrowByText);

    const QString expected =
        QStringLiteral("1")
        + QString(OperatorChars::SubtractionSpace)
        + QString(UnicodeChars::RightwardsArrow)
        + QString(OperatorChars::SubtractionSpace)
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
        QString(OperatorChars::SubtractionSpace)
        + QString(UnicodeChars::RightwardsArrow)
        + QString(OperatorChars::SubtractionSpace);
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
    // Expected: unit context inserts middle dot.
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
    QCOMPARE(editor.document()->toRawText(),
             beforeSpace.left(beforeSpace.size() - 1)
                 + QString(OperatorChars::MulDotSign)
                 + QStringLiteral("]"));
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
    QVERIFY(afterSlash.contains(OperatorChars::DivisionSign));

    editor.setText(QStringLiteral("[m"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);
    const QString afterMul = editor.document()->toRawText();
    QVERIFY(afterMul.contains(OperatorChars::MulCrossSign)
            || afterMul.contains(OperatorChars::MulDotSign));
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
    Evaluator* evaluator = Evaluator::instance();
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
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m"));

    editor.setText(QStringLiteral("[m^"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + QChar(0x00B2));

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
             QStringLiteral("[m") + QChar(0x00B2) + QString(OperatorChars::MulDotSign));

    editor.setText(QStringLiteral("[s") + QChar(0x00B2));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Space, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[s") + QChar(0x00B2) + QString(OperatorChars::MulDotSign));

    editor.setText(QStringLiteral("[m^2"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashAfterPlainExponent(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashAfterPlainExponent);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + QChar(0x00B2) + QString(OperatorChars::DivisionSign));

    editor.setText(QStringLiteral("[m^2"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + QChar(0x00B2) + QString(OperatorChars::MulDotSign));

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
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + QString(OperatorChars::MulDotSign) + QStringLiteral("()"));

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
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[s") + QChar(0x00B2) + QChar(0x00B2) + QChar(0x00B2) + QStringLiteral("]"));

    editor.setText(QStringLiteral("[m]"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent chainedBracketByText(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("["));
    QApplication::sendEvent(&editor, &chainedBracketByText);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m]"));

    editor.setText(QStringLiteral("[m]"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_ParenLeft, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m]")
                 + QString(OperatorChars::MulDotSpace)
                 + QString(OperatorChars::MulDotSign)
                 + QString(OperatorChars::MulDotSpace)
                 + QStringLiteral("()"));

    editor.setText(QStringLiteral("[]")
                   + QString(OperatorChars::MulDotSpace)
                   + QString(OperatorChars::MulDotSign)
                   + QString(OperatorChars::MulDotSpace)
                   + QStringLiteral("()"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_ParenLeft, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[]")
                 + QString(OperatorChars::MulDotSpace)
                 + QString(OperatorChars::MulDotSign)
                 + QString(OperatorChars::MulDotSpace)
                 + QStringLiteral("()")
                 + QString(OperatorChars::MulDotSpace)
                 + QString(OperatorChars::MulDotSign)
                 + QString(OperatorChars::MulDotSpace)
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
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + QChar(0x207B));

    editor.setText(QStringLiteral("[m^("));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^(") + QString(OperatorChars::SubtractionSign));

    editor.setText(QStringLiteral("[m") + QChar(0x207B));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + QChar(0x207B) + QChar(0x00B2));

    editor.setText(QStringLiteral("[m^(") + QString(OperatorChars::SubtractionSign));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(OperatorChars::SubtractionSign) + QStringLiteral("2"));

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
    editor.setText(QStringLiteral("[m") + QChar(0x207B));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashAfterSignedExponentStart(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashAfterSignedExponentStart);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + QChar(0x207B));

    editor.setText(QStringLiteral("[m^(") + QString(OperatorChars::SubtractionSign) + QStringLiteral("1"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText2(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText2);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(OperatorChars::SubtractionSign) + QStringLiteral("1/"));
    QKeyEvent slashByText3(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText3);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(OperatorChars::SubtractionSign) + QStringLiteral("1/"));

    // m^2/3 -> reject digit after slash (non-parenthesized exponent).
    editor.setText(QStringLiteral("[m^2"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText4(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText4);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + QChar(0x00B2) + QStringLiteral("/"));
    QTest::keyClicks(&editor, QStringLiteral("s"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + QChar(0x00B2) + QStringLiteral("/s"));

    editor.setText(QStringLiteral("[m^3"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText4b(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText4b);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + QChar(0x00B3) + QStringLiteral("/"));
    QTest::keyClicks(&editor, QStringLiteral("s"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + QChar(0x00B3) + QStringLiteral("/s"));

    // m²·/ -> reject slash right after multiplication separator.
    editor.setText(QStringLiteral("[m") + QChar(0x00B2) + QString(OperatorChars::MulDotSign));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashAfterMulDot(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashAfterMulDot);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + QChar(0x00B2) + QString(OperatorChars::MulDotSign));

    editor.setText(QStringLiteral("[m^2"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText4c(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText4c);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + QChar(0x00B2) + QStringLiteral("/"));
    QTest::keyClicks(&editor, QStringLiteral("3"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m") + QChar(0x00B2) + QStringLiteral("/"));

    // m^/ + "/" -> trim invalid slash after exponent start.
    editor.setText(QStringLiteral("[m^/"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText4d(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText4d);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[m^"));

    // m^-2/3 -> reject digit after slash (non-parenthesized signed exponent).
    editor.setText(QStringLiteral("[m^") + QString(OperatorChars::SubtractionSign) + QStringLiteral("2"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText5(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText5);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + QChar(0x207B) + QChar(0x00B2) + QStringLiteral("/"));
    QTest::keyClicks(&editor, QStringLiteral("3"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m") + QChar(0x207B) + QChar(0x00B2) + QStringLiteral("/"));

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
    editor.setText(QStringLiteral("[m") + QChar(0x00B2));
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
    editor.setText(QStringLiteral("[m^(") + QString(OperatorChars::SubtractionSign) + QStringLiteral("2"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent slashByText7(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("/"));
    QApplication::sendEvent(&editor, &slashByText7);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(OperatorChars::SubtractionSign) + QStringLiteral("2/"));
    QTest::keyClicks(&editor, QStringLiteral("s"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(OperatorChars::SubtractionSign) + QStringLiteral("2/"));
    QTest::keyClicks(&editor, QStringLiteral("3"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(OperatorChars::SubtractionSign) + QStringLiteral("2/3"));

    editor.setText(QStringLiteral("[m^(-2"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("3"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("[m^(") + QString(OperatorChars::SubtractionSign) + QStringLiteral("23"));

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

    editor.setText(QStringLiteral("[s") + QChar(0x00B2));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("^"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[s") + QChar(0x00B2));

    editor.setText(QStringLiteral("2")
                   + QString(OperatorChars::ValueUnitSpace)
                   + QStringLiteral("[m/s")
                   + QString(OperatorChars::MulDotSign));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent deadCircumflexInUnitAfterDot(QEvent::KeyPress, Qt::Key_Dead_Circumflex, Qt::NoModifier);
    QApplication::sendEvent(&editor, &deadCircumflexInUnitAfterDot);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("2")
                 + QString(OperatorChars::ValueUnitSpace)
                 + QStringLiteral("[m/s")
                 + QString(OperatorChars::MulDotSign));

    QList<QInputMethodEvent::Attribute> caretImeAttrs;
    QInputMethodEvent caretCommitAfterDot(QString(), caretImeAttrs);
    caretCommitAfterDot.setCommitString(QStringLiteral("^"));
    QApplication::sendEvent(&editor, &caretCommitAfterDot);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("2")
                 + QString(OperatorChars::ValueUnitSpace)
                 + QStringLiteral("[m/s")
                 + QString(OperatorChars::MulDotSign));

    QInputMethodEvent caretPreeditAfterDot(QString::fromUtf8("ˆ"), caretImeAttrs);
    QApplication::sendEvent(&editor, &caretPreeditAfterDot);
    QInputMethodEvent digitCommitAfterCaretPreedit(QString(), caretImeAttrs);
    digitCommitAfterCaretPreedit.setCommitString(QStringLiteral("2"));
    QApplication::sendEvent(&editor, &digitCommitAfterCaretPreedit);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("2")
                 + QString(OperatorChars::ValueUnitSpace)
                 + QStringLiteral("[m/s")
                 + QString(OperatorChars::MulDotSign));

    editor.setText(QStringLiteral("2")
                   + QString(OperatorChars::ValueUnitSpace)
                   + QStringLiteral("[m")
                   + QChar(0x00B2)
                   + QStringLiteral("/s"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);
    QTest::keyClick(&editor, Qt::Key_Asterisk, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("2")
                 + QString(OperatorChars::ValueUnitSpace)
                 + QStringLiteral("[m")
                 + QChar(0x00B2)
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
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("s") + QChar(0x00B2));

    editor.setText(QStringLiteral("s^"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Minus, Qt::NoModifier);
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(),
             QStringLiteral("s") + QChar(0x207B) + QChar(0x00B2));

    editor.setText(QStringLiteral("[s^"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("[s") + QChar(0x00B2));

    editor.setText(QStringLiteral("s"));
    editor.setCursorPosition(editor.text().size());
    QKeyEvent deadCircumflex(QEvent::KeyPress, Qt::Key_Dead_Circumflex, Qt::NoModifier);
    QApplication::sendEvent(&editor, &deadCircumflex);
    QTest::keyClicks(&editor, QStringLiteral("2"));
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("s") + QChar(0x00B2));

    editor.setText(QStringLiteral("s"));
    editor.setCursorPosition(editor.text().size());
    QList<QInputMethodEvent::Attribute> imeAttrs;
    QInputMethodEvent imeWithCaretPreedit(QString::fromUtf8("ˆ"), imeAttrs);
    QApplication::sendEvent(&editor, &imeWithCaretPreedit);
    QInputMethodEvent imeDigitCommit(QString(), imeAttrs);
    imeDigitCommit.setCommitString(QStringLiteral("2"));
    QApplication::sendEvent(&editor, &imeDigitCommit);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("s") + QChar(0x00B2));

    editor.setText(QStringLiteral("s") + QChar(0x00B2));
    editor.setCursorPosition(editor.text().size());
    QInputMethodEvent imeCaretCommit(QString(), imeAttrs);
    imeCaretCommit.setCommitString(QStringLiteral("^"));
    QApplication::sendEvent(&editor, &imeCaretCommit);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("s") + QChar(0x00B2));
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

void TestEditorUi::ignores_dot_and_comma_after_non_numerical_char()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("pi"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("pi"));

    editor.setText(QStringLiteral("pi"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Comma, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("pi"));

    editor.setText(QStringLiteral("12"));
    editor.setCursorPosition(editor.text().size());
    QTest::keyClick(&editor, Qt::Key_Period, Qt::NoModifier);
    QCOMPARE(editor.document()->toRawText(), QStringLiteral("12."));
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
                   + QString(OperatorChars::MulCrossSign)
                   + QStringLiteral(" "));
    editor.setCursorPosition(editor.text().size());

    QKeyEvent deadCircumflex(QEvent::KeyPress, Qt::Key_Dead_Circumflex, Qt::NoModifier);
    QApplication::sendEvent(&editor, &deadCircumflex);

    QCOMPARE(editor.text(),
             QStringLiteral("3")
                 + QString(OperatorChars::MulCrossSign)
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
                   + QString(OperatorChars::MulCrossSign)
                   + QStringLiteral(" "));
    editor.setCursorPosition(editor.text().size());

    QTest::keyClicks(&editor, QStringLiteral("^"));

    QCOMPARE(editor.text(),
             QStringLiteral("3")
                 + QString(OperatorChars::MulCrossSign)
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
                   + QString(OperatorChars::MulCrossSign)
                   + QStringLiteral(" "));
    editor.setCursorPosition(editor.text().size());

    QList<QInputMethodEvent::Attribute> attributes;
    QInputMethodEvent imeEvent(QString(), attributes);
    imeEvent.setCommitString(QStringLiteral("^"));
    QApplication::sendEvent(&editor, &imeEvent);

    QCOMPARE(editor.text(),
             QStringLiteral("3")
                 + QString(OperatorChars::MulCrossSign)
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

    editor.setText(QString::fromUtf8("pi²") + QString(OperatorChars::MulDotSign) + QStringLiteral("c"));
    editor.setCursorPosition(editor.text().size());

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("cos: Built-in function"))));

    QCOMPARE(
        editor.text(),
        QString::fromUtf8("pi²")
            + QString(OperatorChars::MulDotSign)
            + QStringLiteral("cos()"));
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
}

void TestEditorUi::offers_unit_completion_for_degree_symbol_in_unit_context()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QString::fromUtf8("100 [°"));
    editor.setCursorPosition(editor.text().size());

    const QStringList degreeChoices = editor.matchFragment(QString::fromUtf8("°"), true);
    QVERIFY(!degreeChoices.isEmpty());
    QVERIFY(degreeChoices.contains(QString::fromUtf8("°:Unit")));
    QVERIFY(degreeChoices.contains(QString::fromUtf8("°C:Unit")));

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("u:Unit"))));
    QCOMPARE(editor.text(), QString::fromUtf8("100 [u"));
}

void TestEditorUi::unit_context_completion_includes_angle_units_and_long_forms()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QStringList radChoices = editor.matchFragment(QStringLiteral("rad"), true);
    QVERIFY(radChoices.contains(QStringLiteral("rad:Unit")));
    QVERIFY(radChoices.contains(QStringLiteral("radian:Unit")));

    const QStringList arcChoices = editor.matchFragment(QStringLiteral("arc"), true);
    QVERIFY(arcChoices.contains(QStringLiteral("arcminute:Unit")));
    QVERIFY(arcChoices.contains(QStringLiteral("arcsecond:Unit")));

    const QStringList turnChoices = editor.matchFragment(QStringLiteral("turn"), true);
    QVERIFY(turnChoices.contains(QStringLiteral("turn:Unit")));

    const QStringList gradChoices = editor.matchFragment(QStringLiteral("grad"), true);
    QVERIFY(gradChoices.contains(QStringLiteral("grad:Unit")));
    QVERIFY(gradChoices.contains(QStringLiteral("gradian:Unit")));
}

void TestEditorUi::completes_are_unit_to_short_form_in_unit_context()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    editor.setText(QStringLiteral("1 [a"));
    editor.setCursorPosition(editor.text().size());
    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "autoComplete",
        Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("are:Unit"))));
    QCOMPARE(editor.text(), QStringLiteral("1 [a"));
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
}

void TestEditorUi::enter_evaluates_when_completion_popup_has_no_explicit_interaction()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    const QString input = QString::fromUtf8("∑(1;10;n");
    editor.setText(input);
    editor.setCursorPosition(input.size());

    QSignalSpy returnPressedSpy(&editor, SIGNAL(returnPressed()));

    QVERIFY(QMetaObject::invokeMethod(
        &editor,
        "triggerAutoComplete",
        Qt::DirectConnection));

    QWidget* popup = QApplication::activePopupWidget();
    QVERIFY(popup);

    QTest::keyClick(popup, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::processEvents();

    QCOMPARE(returnPressedSpy.count(), 1);
    QCOMPARE(editor.text(), input);
}

QTEST_MAIN(TestEditorUi)
#include "testeditorui.moc"
