// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mainwindowtesthelpers.h"

using namespace MainWindowTestHelpers;

class TestDisplayUi : public QObject {
    Q_OBJECT

private slots:
    void user_definition_comments_survive_creation_and_editing_data();
    void user_definition_comments_survive_creation_and_editing();
    void result_display_keeps_comments_literal_data();
    void result_display_keeps_comments_literal();
    void initTestCase();
    void init() { UiTestFixture::resetSettings(); }
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
    void result_display_preserves_unreduced_quotients_data();
    void result_display_preserves_unreduced_quotients();
    void result_display_reduces_combinable_quotient_factors_data();
    void result_display_reduces_combinable_quotient_factors();
    void result_display_copies_calculation_as_single_line_data();
    void result_display_copies_calculation_as_single_line();
    void result_display_copies_expression_and_result_data();
    void result_display_copies_expression_and_result();
    void result_display_context_menu_tracks_session_state_data();
    void result_display_context_menu_tracks_session_state();
    void result_display_context_menu_copies_selection();
    void result_display_remove_menu_targets_clicked_calculation();
};

void TestDisplayUi::user_definition_comments_survive_creation_and_editing_data()
{
    QTest::addColumn<QString>("definition");
    QTest::addColumn<QString>("dockName");
    QTest::addColumn<bool>("paste");
    for (bool paste : {false, true}) {
        const QByteArray mode = paste ? "paste" : "type";
        QTest::newRow(QByteArray("variable-").append(mode).constData())
            << QStringLiteral("examplevar = 4") << QStringLiteral("VariablesDock") << paste;
        QTest::newRow(QByteArray("function-").append(mode).constData())
            << QStringLiteral("examplefunc(x) = x*2") << QStringLiteral("UserFunctionsDock") << paste;
        QTest::newRow(QByteArray("unit-").append(mode).constData())
            << QStringLiteral("[exampleunit] = 4[m]") << QStringLiteral("UserUnitsDock") << paste;
    }
}

void TestDisplayUi::user_definition_comments_survive_creation_and_editing()
{
    QFETCH(QString, definition);
    QFETCH(QString, dockName);
    QFETCH(bool, paste);
    MainWindowStateGuard guard;
    Settings* settings = Settings::instance();
    settings->variablesDockVisible = true;
    settings->userFunctionsDockVisible = true;
    settings->userUnitsDockVisible = true;
    settings->leaveLastExpression = false;
    MainWindow window(false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    Editor* editor = window.findChild<Editor*>();
    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QDockWidget* dock = window.findChild<QDockWidget*>(dockName);
    QVERIFY(editor);
    QVERIFY(display);
    QVERIFY(dock);
    dock->show();
    dock->raise();
    QWidget* widget = dock->widget();
    QTreeWidget* tree = widget->findChild<QTreeWidget*>();
    QVERIFY(tree);
    const QString comment = QStringLiteral("2*2 pi sqrt(4) 10000 [m * s^2] -> [km] 'note' ? 3/2-1");
    const QString suffix = QStringLiteral(" ? ") + comment;
    const QString oldClipboard = QApplication::clipboard()->text();
    const auto restoreClipboard = qScopeGuard([&]() {
        QApplication::clipboard()->setText(oldClipboard);
    });
    editor->clear();
    if (paste) {
        QApplication::clipboard()->setText(definition + suffix);
        editor->paste();
    } else {
        editor->setText(definition);
        editor->setCursorPosition(editor->text().size());
        QTest::keyClicks(editor, QStringLiteral("?") + comment);
    }
    QVERIFY2(editor->text().endsWith(suffix), qPrintable(editor->text()));
    QVERIFY(QMetaObject::invokeMethod(&window, "evaluateEditorExpression"));

    for (int edit = 0; edit < 2; ++edit) {
        QVERIFY(QMetaObject::invokeMethod(widget, "updateList"));
        QTreeWidgetItem* item = nullptr;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            if (tree->topLevelItem(i)->text(0).startsWith(QStringLiteral("example")))
                item = tree->topLevelItem(i);
        }
        QVERIFY(item);
        const QString expectedComment = comment + (edit ? QStringLiteral(" 5*5") : QString());
        QCOMPARE(item->text(2), expectedComment);
        QVERIFY(!item->text(1).contains(QLatin1Char('?')));
        QVERIFY2(display->toPlainText().contains(QStringLiteral(" ? ") + expectedComment),
                 qPrintable(display->toPlainText()));
        display->setSession(display->session());
        QVERIFY(display->toPlainText().contains(QStringLiteral(" ? ") + expectedComment));
        tree->setCurrentItem(item);
        item->setSelected(true);
        editor->clear();
        QVERIFY(QMetaObject::invokeMethod(widget, "editItem"));
        QVERIFY2(editor->text().endsWith(QStringLiteral(" ? ") + expectedComment),
                 qPrintable(editor->text()));
        if (edit == 0) {
            editor->setCursorPosition(editor->text().size());
            editor->insert(QStringLiteral(" 5*5"));
            QVERIFY(QMetaObject::invokeMethod(&window, "evaluateEditorExpression"));
        }
    }
}

void TestDisplayUi::result_display_keeps_comments_literal_data()
{
    QTest::addColumn<QString>("expression");
    QTest::newRow("arithmetic") << QStringLiteral("2*2");
    QTest::newRow("unit") << QStringLiteral("2*[m]");
    QTest::newRow("conversion") << QStringLiteral("2[m] -> [cm]");
    QTest::newRow("angle") << QStringLiteral("1°");
    QTest::newRow("angle-conversion") << QStringLiteral("1° in [rad]");
    QTest::newRow("list") << QStringLiteral("{2*2;3}");
    QTest::newRow("comment-only") << QString();
}

void TestDisplayUi::result_display_keeps_comments_literal()
{
    QFETCH(QString, expression);
    const QString comment = QStringLiteral("2*2 pi sqrt(4) 10000 [m * s^2] -> [km] 'note' ? 3/2-1\tµ");
    const QString source = expression + QStringLiteral(" ? ") + comment;
    Session session;
    Evaluator* evaluator = session.evaluator();
    evaluator->setExpression(source);
    Quantity result = evaluator->evalUpdateAns();
    QVERIFY2(evaluator->error().isEmpty(), qPrintable(evaluator->error()));
    const QString interpreted = evaluator->interpretedExpression();
    if (!interpreted.isEmpty())
        QCOMPARE(interpreted.mid(interpreted.indexOf(QLatin1Char('?')) + 2), comment);
    for (const QString& displayInterpreted : {QString(), interpreted}) {
        ResultDisplay display;
        display.setSession(&session);
        display.append(source, result, displayInterpreted);
        const QString line = display.document()->firstBlock().text();
        QCOMPARE(line.mid(line.indexOf(QLatin1Char('?')) + 2), comment);
    }
}

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
        QMenu* copyMenu = directSubmenuWithTitle(menu.data(), QStringLiteral("Copy"));
        QVERIFY(copyMenu != nullptr);
        QAction* action = directMenuActionWithText(copyMenu, QStringLiteral("Calculation"));
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
    QMenu* copyMenu = directSubmenuWithTitle(menu.data(), QStringLiteral("Copy"));
    QVERIFY(copyMenu != nullptr);
    QAction* action = directMenuActionWithText(copyMenu, QStringLiteral("Calculation"));
    QVERIFY(action != nullptr);
    action->trigger();
    QCOMPARE(QApplication::clipboard()->text(), expected);
}

void TestDisplayUi::result_display_preserves_unreduced_quotients_data()
{
    QTest::addColumn<QString>("expression");
    QTest::newRow("previous-answer") << QStringLiteral("ans/9");
    QTest::newRow("decimal-denominator") << QStringLiteral("ans/0.25");
    QTest::newRow("constant") << QStringLiteral("e/7");
    QTest::newRow("function") << QStringLiteral("cos(pi)/11");
    QTest::newRow("power") << QStringLiteral("ans^2/13");
    QTest::newRow("product") << QStringLiteral("ans*e/5");
    QTest::newRow("grouped-denominator") << QStringLiteral("ans/(17*e)");
    QTest::newRow("negative-numerator") << QStringLiteral("(-ans)/8");
    QTest::newRow("negative-denominator") << QStringLiteral("ans/(-6)");
    QTest::newRow("sum") << QStringLiteral("ans/10+e/7");
    QTest::newRow("comment") << QStringLiteral("ans/12 ? previous answer");
    QTest::newRow("unicode-division") << QString::fromUtf8("ans÷16");
}

void TestDisplayUi::result_display_preserves_unreduced_quotients()
{
    QFETCH(QString, expression);
    Settings::instance()->simplifyResultExpressions = true;
    Settings::instance()->multipleResultLinesEnabled = false;
    Session session;
    Evaluator* evaluator = session.evaluator();
    evaluator->setExpression(QStringLiteral("4"));
    QCOMPARE(evaluator->evalUpdateAns(), Quantity(4));
    evaluator->setExpression(expression);
    const Quantity value = evaluator->evalUpdateAns();
    QVERIFY2(evaluator->error().isEmpty(), qPrintable(evaluator->error()));

    const QString interpreted = evaluator->interpretedExpression();
    const QString withoutComment = interpreted.section(QLatin1Char('?'), 0, 0).trimmed();
    QCOMPARE(Evaluator::simplifyInterpretedExpression(interpreted), withoutComment);
    QCOMPARE(Evaluator::formatInterpretedExpressionSimplifiedForDisplay(interpreted, evaluator),
             Evaluator::formatInterpretedExpressionForDisplay(withoutComment, evaluator));

    EvaluationContext context;
    context.main.fmt = 'f';
    context.main.prec = 20;
    session.addHistoryEntry(HistoryEntry(expression, value, interpreted, context));
    ResultDisplay display;
    display.setSession(&session);
    const QStringList lines = display.toPlainText().trimmed().split(QLatin1Char('\n'));
    QCOMPARE(lines.size(), 2);
    if (expression == QStringLiteral("ans/9"))
        QCOMPARE(lines.last(), QStringLiteral("= 0.44444444444444444444"));
}

void TestDisplayUi::result_display_reduces_combinable_quotient_factors_data()
{
    QTest::addColumn<QString>("expression");
    QTest::addColumn<QString>("simplified");
    QTest::newRow("repeated-factor") << QStringLiteral("ans*ans/25") << QStringLiteral("1/25*ans^2");
    QTest::newRow("numeric-coefficient") << QStringLiteral("6*ans/15") << QStringLiteral("2/5*ans");
    QTest::newRow("numeric-denominators") << QStringLiteral("ans/2/7") << QStringLiteral("1/14*ans");
    QTest::newRow("cancellation") << QStringLiteral("ans*e/e/8") << QStringLiteral("1/8*ans");
    QTest::newRow("neutral-factor") << QStringLiteral("ans/1") << QStringLiteral("ans");
}

void TestDisplayUi::result_display_reduces_combinable_quotient_factors()
{
    QFETCH(QString, expression);
    QFETCH(QString, simplified);
    Settings::instance()->simplifyResultExpressions = true;
    Settings::instance()->multipleResultLinesEnabled = false;
    Session session;
    Evaluator* evaluator = session.evaluator();
    evaluator->setExpression(QStringLiteral("4"));
    QCOMPARE(evaluator->evalUpdateAns(), Quantity(4));
    evaluator->setExpression(expression);
    const Quantity value = evaluator->evalUpdateAns();
    QVERIFY2(evaluator->error().isEmpty(), qPrintable(evaluator->error()));

    const QString interpreted = evaluator->interpretedExpression();
    QCOMPARE(Evaluator::simplifyInterpretedExpression(interpreted), simplified);
    session.addHistoryEntry(HistoryEntry(expression, value, interpreted));
    ResultDisplay display;
    display.setSession(&session);
    const QStringList lines = display.toPlainText().trimmed().split(QLatin1Char('\n'));
    QCOMPARE(lines.size(), 3);
    QCOMPARE(lines.at(1), QStringLiteral("= ")
             + Evaluator::formatInterpretedExpressionForDisplay(simplified, evaluator));
}

void TestDisplayUi::result_display_copies_calculation_as_single_line_data()
{
    QTest::addColumn<QStringList>("lines");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<bool>("failed");
    QTest::newRow("simple")
        << QStringList({QStringLiteral("120 / 8"), QStringLiteral("= 15")})
        << QStringLiteral("120 / 8 = 15") << false;
    QTest::newRow("simplification-and-formats")
        << QStringList({QStringLiteral("1:120:3600"), QStringLiteral("= 4:00:00"),
                        QStringLiteral("= 14400 s"), QStringLiteral("= 1.44 × 10⁴ s")})
        << QStringLiteral("1:120:3600 = 4:00:00 = 14400 s = 1.44 × 10⁴ s") << false;
    QTest::newRow("wrapped-unicode-and-spacing")
        << QStringList({QStringLiteral("123456789 [m] − 98765432 [m] + 1 [m] + 2 [m]"),
                        QStringLiteral("= 24\u2009567\u2009360 m"),
                        QStringLiteral("= 2.456736 × 10⁷ m")})
        << QStringLiteral("123456789 [m] − 98765432 [m] + 1 [m] + 2 [m] = 24\u2009567\u2009360 m = 2.456736 × 10⁷ m")
        << false;
    QTest::newRow("failed")
        << QStringList({QStringLiteral("1 / 0")}) << QStringLiteral("1 / 0") << true;
}

void TestDisplayUi::result_display_copies_calculation_as_single_line()
{
    QFETCH(QStringList, lines);
    QFETCH(QString, expected);
    QFETCH(bool, failed);
    const QString oldClipboard = QApplication::clipboard()->text();
    const auto restoreClipboard = qScopeGuard([oldClipboard]() {
        QApplication::clipboard()->setText(oldClipboard);
    });

    Session session;
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

    QTextCursor selection(display.document()->firstBlock());
    selection.select(QTextCursor::BlockUnderCursor);
    display.setTextCursor(selection);
    const int selectionStart = selection.selectionStart();
    const int selectionEnd = selection.selectionEnd();
    if (qstrcmp(QTest::currentDataTag(), "wrapped-unicode-and-spacing") == 0)
        QVERIFY(display.document()->find(lines.first()).block().layout()->lineCount() > 1);

    // Copy the recorded text even after the current display settings change.
    Settings::instance()->resultFormat = 'h';
    Settings::instance()->simplifyResultExpressions = false;
    for (const QString& line : lines) {
        QTextCursor clickedLine = display.document()->find(line);
        QVERIFY(!clickedLine.isNull());
        clickedLine.movePosition(QTextCursor::StartOfBlock);
        display.verticalScrollBar()->setValue(clickedLine.blockNumber());
        QCoreApplication::processEvents();
        QScopedPointer<QMenu> menu(display.createContextMenu(display.cursorRect(clickedLine).center()));
        QMenu* copyMenu = directSubmenuWithTitle(menu.data(), QStringLiteral("Copy"));
        QVERIFY(copyMenu != nullptr);
        QAction* action = directMenuActionWithText(copyMenu, QStringLiteral("Calculation as Single Line"));
        QVERIFY(action != nullptr);
        QVERIFY(action->isEnabled());
        QApplication::clipboard()->setText(QStringLiteral("unchanged"));
        action->trigger();
        QCOMPARE(QApplication::clipboard()->text(), expected);
        QCOMPARE(display.textCursor().selectionStart(), selectionStart);
        QCOMPARE(display.textCursor().selectionEnd(), selectionEnd);
    }
}

void TestDisplayUi::result_display_copies_expression_and_result_data()
{
    QTest::addColumn<bool>("failed");
    QTest::newRow("result") << false;
    QTest::newRow("failed") << true;
}

void TestDisplayUi::result_display_copies_expression_and_result()
{
    QFETCH(bool, failed);
    const QString oldClipboard = QApplication::clipboard()->text();
    const auto restoreClipboard = qScopeGuard([oldClipboard]() {
        QApplication::clipboard()->setText(oldClipboard);
    });

    Session session;
    const QString expression = failed ? QStringLiteral("1 / 0") : QStringLiteral("2 + 3");
    session.addHistoryEntry(HistoryEntry(expression, failed ? DMath::nan() : Quantity(5)));
    MenuTestResultDisplay display;
    display.resize(400, 200);
    display.setSession(&session);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));
    const QTextCursor expressionCursor(display.document()->firstBlock());
    QScopedPointer<QMenu> menu(display.createContextMenu(display.cursorRect(expressionCursor).center()));
    QMenu* copyMenu = directSubmenuWithTitle(menu.data(), QStringLiteral("Copy"));
    QVERIFY(copyMenu != nullptr);
    QAction* copyExpression = directMenuActionWithText(copyMenu, QStringLiteral("Expression"));
    QAction* copyResult = directMenuActionWithText(copyMenu, QStringLiteral("Result"));
    QVERIFY(copyExpression != nullptr);
    QVERIFY(copyResult != nullptr);
    QVERIFY(copyExpression->isEnabled());
    QCOMPARE(copyResult->isEnabled(), !failed);
    copyExpression->trigger();
    QCOMPARE(QApplication::clipboard()->text(), expression);
    if (!failed) {
        copyResult->trigger();
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("5"));
    }
}

void TestDisplayUi::result_display_context_menu_tracks_session_state_data()
{
    QTest::addColumn<bool>("hasCalculation");
    QTest::addColumn<bool>("isPrivate");
    QTest::newRow("empty") << false << false;
    QTest::newRow("calculation") << true << false;
    QTest::newRow("private-calculation") << true << true;
}

void TestDisplayUi::result_display_context_menu_tracks_session_state()
{
    QFETCH(bool, hasCalculation);
    QFETCH(bool, isPrivate);
    Session session;
    session.setPrivate(isPrivate);
    if (hasCalculation)
        session.addHistoryEntry(HistoryEntry(QStringLiteral("2 + 3"), Quantity(5)));
    MenuTestResultDisplay display;
    display.resize(400, 200);
    display.setSession(&session);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));
    const QTextCursor cursor(display.document()->firstBlock());
    QScopedPointer<QMenu> menu(display.createContextMenu(display.cursorRect(cursor).center()));
    QMenu* copyMenu = directSubmenuWithTitle(menu.data(), QStringLiteral("Copy"));
    QVERIFY(copyMenu != nullptr);
    QCOMPARE(copyMenu->isEnabled(), hasCalculation);
    const QString selectAllText = QStringLiteral("Select All\t")
        + QKeySequence(QKeySequence::SelectAll).toString(QKeySequence::NativeText);
    QAction* selectAll = directMenuActionWithText(menu.data(), selectAllText);
    QVERIFY(selectAll != nullptr);
    QCOMPARE(selectAll->isEnabled(), hasCalculation);
    QMenu* sessionMenu = directSubmenuWithTitle(menu.data(), QStringLiteral("Session"));
    QVERIFY(sessionMenu != nullptr);
    QAction* privacyAction = directMenuActionWithText(sessionMenu,
        isPrivate ? QStringLiteral("Keep") : QStringLiteral("Make Private..."));
    QVERIFY(privacyAction != nullptr);
    QVERIFY(directMenuActionWithText(sessionMenu,
        isPrivate ? QStringLiteral("Make Private...") : QStringLiteral("Keep")) == nullptr);
    QSignalSpy privacySpy(&display, &ResultDisplay::convertSessionPrivacyRequested);
    privacyAction->trigger();
    QCOMPARE(privacySpy.count(), 1);
    QAction* openAction = directMenuActionWithText(sessionMenu, QStringLiteral("Open..."));
    QVERIFY(openAction != nullptr);
    QSignalSpy openSpy(&display, &ResultDisplay::openSessionRequested);
    openAction->trigger();
    QCOMPARE(openSpy.count(), 1);
}

void TestDisplayUi::result_display_context_menu_copies_selection()
{
    const QString oldClipboard = QApplication::clipboard()->text();
    const auto restoreClipboard = qScopeGuard([oldClipboard]() {
        QApplication::clipboard()->setText(oldClipboard);
    });
    Session session;
    session.addHistoryEntry(HistoryEntry(QStringLiteral("2 + 3"), Quantity(5)));
    MenuTestResultDisplay display;
    display.resize(400, 200);
    display.setSession(&session);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));
    QTextCursor cursor(display.document()->firstBlock());
    cursor.select(QTextCursor::BlockUnderCursor);
    display.setTextCursor(cursor);
    QScopedPointer<QMenu> menu(display.createContextMenu(display.cursorRect(cursor).center()));
    QMenu* copyMenu = directSubmenuWithTitle(menu.data(), QStringLiteral("Copy"));
    QVERIFY(copyMenu != nullptr);
    QAction* selectionAction = directMenuActionWithText(copyMenu, QStringLiteral("Selection\t")
        + QKeySequence(QKeySequence::Copy).toString(QKeySequence::NativeText));
    QVERIFY(selectionAction != nullptr);
    QVERIFY(selectionAction->isEnabled());
    selectionAction->trigger();
    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("2 + 3"));
    QCOMPARE(display.textCursor().selectedText(), QStringLiteral("2 + 3"));
    QAction* selectAll = directMenuActionWithText(menu.data(), QStringLiteral("Select All\t")
        + QKeySequence(QKeySequence::SelectAll).toString(QKeySequence::NativeText));
    QVERIFY(selectAll != nullptr);
    selectAll->trigger();
    QCOMPARE(display.textCursor().selectionStart(), 0);
    QCOMPARE(display.textCursor().selectionEnd(), display.document()->characterCount() - 1);
}

void TestDisplayUi::result_display_remove_menu_targets_clicked_calculation()
{
    Session session;
    for (int i = 0; i < 3; ++i)
        session.addHistoryEntry(HistoryEntry(QString::number(i), Quantity(i)));
    MenuTestResultDisplay display;
    display.resize(400, 300);
    display.setSession(&session);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));
    QTextCursor clicked = display.document()->find(QStringLiteral("1"));
    QVERIFY(!clicked.isNull());
    QScopedPointer<QMenu> menu(display.createContextMenu(display.cursorRect(clicked).center()));
    QMenu* removeMenu = directSubmenuWithTitle(menu.data(), QStringLiteral("Remove"));
    QVERIFY(removeMenu != nullptr);
    QSignalSpy removeSpy(&display, &ResultDisplay::removeHistoryEntryRequested);
    QSignalSpy aboveSpy(&display, &ResultDisplay::removeHistoryEntriesAboveRequested);
    QSignalSpy belowSpy(&display, &ResultDisplay::removeHistoryEntriesBelowRequested);
    const QList<QSignalSpy*> spies = {&removeSpy, &aboveSpy, &belowSpy};
    const QStringList labels = {QStringLiteral("This Calculation"), QStringLiteral("All Calculations Above"),
                                QStringLiteral("All Calculations Below")};
    for (int i = 0; i < labels.size(); ++i) {
        QAction* action = directMenuActionWithText(removeMenu, labels.at(i));
        QVERIFY(action != nullptr);
        action->trigger();
        for (int j = 0; j < spies.size(); ++j) {
            QCOMPARE(spies.at(j)->count(), i == j ? 1 : 0);
            if (i == j)
                QCOMPARE(spies.at(j)->first().first().toInt(), 1);
            spies.at(j)->clear();
        }
    }
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
    QMenu* sessionMenu = directSubmenuWithTitle(menu, QStringLiteral("Session"));
    QVERIFY(sessionMenu != nullptr);
    QAction* importAction = directMenuActionWithText(sessionMenu, QStringLiteral("&Import..."));
    QVERIFY(importAction != nullptr);
    QMenu* exportMenu = directSubmenuWithTitle(sessionMenu, QStringLiteral("&Export"));
    QVERIFY(exportMenu != nullptr);
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

int main(int argc, char** argv)
{
    return UiTestFixture::run<TestDisplayUi>(argc, argv);
}

#include "testdisplayui.moc"
