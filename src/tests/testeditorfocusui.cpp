// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mainwindowtesthelpers.h"

using namespace MainWindowTestHelpers;

class EditorFocusTestWindow : public MainWindow {
public:
    using MainWindow::eventFilter;
};

class TestEditorFocusUi : public QObject {
    Q_OBJECT

private slots:
    void init() { UiTestFixture::resetSettings(); }
    void editor_click_activates_pane_before_dock_focus_changes_data();
    void editor_click_activates_pane_before_dock_focus_changes();
    void finishing_constant_search_restores_editor_outline_data();
    void finishing_constant_search_restores_editor_outline();
    void constant_search_restores_owning_pane_after_dock_focus_replay_data();
    void constant_search_restores_owning_pane_after_dock_focus_replay();
};

void TestEditorFocusUi::editor_click_activates_pane_before_dock_focus_changes_data()
{
    QTest::addColumn<bool>("viewport");
    QTest::newRow("editor") << false;
    QTest::newRow("viewport") << true;
}

void TestEditorFocusUi::editor_click_activates_pane_before_dock_focus_changes()
{
    QFETCH(bool, viewport);
    MainWindowStateGuard guard;
    guard.settings->editorDisplayStyle = Settings::EditorDisplayModern;
    guard.settings->constantsDockVisible = true;
    guard.settings->colorScheme = QStringLiteral("Custom");
    const QColor primary(QStringLiteral("#e7a840"));
    guard.settings->customColorSchemeJson = themeJsonString(QJsonObject{
        {QStringLiteral("background"), QStringLiteral("#454257")},
        {QStringLiteral("primary"), primary.name()}
    });

    EditorFocusTestWindow window;
    window.resize(1000, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    auto* editor = window.findChild<Editor*>();
    auto* dock = window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(editor && dock);
    auto* search = dock->findChild<QLineEdit*>();
    QVERIFY(search);
    search->setFocus();
    QTRY_VERIFY(search->hasFocus());
    QTRY_VERIFY(!editorHasPrimaryOutline(editor, primary));
    QTest::qWait(150);

    // Handle the press before applying its focus change.
    QWidget* target = viewport ? editor->viewport() : editor;
    const QPoint position = target->rect().center();
    QMouseEvent press(QEvent::MouseButtonPress, position, target->mapToGlobal(position),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QVERIFY(!window.eventFilter(target, &press));
    editor->setFocus(Qt::MouseFocusReason);
    QVERIFY(editor->hasFocus());
    QVERIFY(editorHasPrimaryOutline(editor, primary));
}

void TestEditorFocusUi::finishing_constant_search_restores_editor_outline_data()
{
    QTest::addColumn<bool>("accept");
    QTest::newRow("early-hide-notification") << false;
    QTest::newRow("insert-before-hide") << true;
}

void TestEditorFocusUi::finishing_constant_search_restores_editor_outline()
{
    QFETCH(bool, accept);
    MainWindowStateGuard guard;
    guard.settings->editorDisplayStyle = Settings::EditorDisplayModern;
    guard.settings->autoCalc = false;
    guard.settings->colorScheme = QStringLiteral("Custom");
    const QColor primary(QStringLiteral("#e7a840"));
    guard.settings->customColorSchemeJson = themeJsonString(QJsonObject{
        {QStringLiteral("background"), QStringLiteral("#454257")},
        {QStringLiteral("primary"), primary.name()}
    });

    MainWindow window;
    window.show();
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(QTest::qWaitForWindowActive(&window));
    auto* editor = window.findChild<Editor*>();
    QVERIFY(editor);
    QTest::mouseClick(editor->viewport(), Qt::LeftButton);
    QTRY_VERIFY(editorHasPrimaryOutline(editor, primary));
    QTest::keyClick(editor, Qt::Key_Space, Qt::ControlModifier);
    QPointer<QWidget> popup = QApplication::activePopupWidget();
    QVERIFY(popup);
    QTRY_VERIFY(editor->isConstantSearchVisible());
    QTRY_VERIFY(!editorHasPrimaryOutline(editor, primary));

    if (accept) {
        editor->insertConstant(QStringLiteral("e"));
    } else {
        // A native hide notification can arrive before Qt clears visibility.
        QHideEvent hide;
        QCoreApplication::sendEvent(popup, &hide);
    }
    QVERIFY(!editor->isConstantSearchVisible());
    QCOMPARE(editor->text(), accept ? QStringLiteral("e") : QString());
    // Check before deferred deletion can trigger another focus transition.
    QVERIFY(editorHasPrimaryOutline(editor, primary));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(!popup);
}

void TestEditorFocusUi::constant_search_restores_owning_pane_after_dock_focus_replay_data()
{
    QTest::addColumn<int>("closeKey");
    QTest::addColumn<bool>("transientFocus");
    for (bool transient : {false, true}) {
        const QString prefix = transient ? QStringLiteral("transient-") : QStringLiteral("passive-");
        QTest::newRow(qPrintable(prefix + QStringLiteral("escape"))) << int(Qt::Key_Escape) << transient;
        QTest::newRow(qPrintable(prefix + QStringLiteral("enter"))) << int(Qt::Key_Return) << transient;
        QTest::newRow(qPrintable(prefix + QStringLiteral("tab"))) << int(Qt::Key_Tab) << transient;
    }
}

void TestEditorFocusUi::constant_search_restores_owning_pane_after_dock_focus_replay()
{
    QFETCH(int, closeKey);
    QFETCH(bool, transientFocus);
    MainWindowStateGuard guard;
    guard.settings->editorDisplayStyle = Settings::EditorDisplayModern;
    guard.settings->autoCalc = false;
    guard.settings->constantsDockVisible = true;
    guard.settings->colorScheme = QStringLiteral("Custom");
    const QColor primary(QStringLiteral("#e7a840"));
    guard.settings->customColorSchemeJson = themeJsonString(QJsonObject{
        {QStringLiteral("background"), QStringLiteral("#454257")},
        {QStringLiteral("primary"), primary.name()}
    });

    MainWindow window;
    window.resize(1000, 600);
    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    window.show();
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(QTest::qWaitForWindowActive(&window));
    const auto editors = window.findChildren<Editor*>();
    QCOMPARE(editors.size(), 2);
    Editor* editor = editors.first();
    auto* dock = window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(dock);
    auto* dockSearch = dock->findChild<QLineEdit*>();
    QVERIFY(dockSearch);
    QTest::mouseClick(editor->viewport(), Qt::LeftButton);
    QTRY_VERIFY(editorHasPrimaryOutline(editor, primary));
    QTest::keyClick(editor, Qt::Key_Space, Qt::ControlModifier);
    QPointer<QWidget> popup = QApplication::activePopupWidget();
    QVERIFY(popup);
    auto* search = popup->findChild<QLineEdit*>(QStringLiteral("constantCompletionSearch"));
    QVERIFY(search);
    QTRY_VERIFY(search->hasFocus());
    search->setText(QStringLiteral("Euler's number"));

    if (transientFocus) {
        // Native focus restoration may briefly visit the previous dock input.
        dockSearch->setFocus(Qt::ActiveWindowFocusReason);
        QVERIFY(dockSearch->hasFocus());
        search->setFocus(Qt::PopupFocusReason);
    } else {
        QFocusEvent replay(QEvent::FocusIn, Qt::ActiveWindowFocusReason);
        QCoreApplication::sendEvent(dockSearch, &replay);
    }
    QVERIFY(search->hasFocus());
    QTest::keyClick(search, static_cast<Qt::Key>(closeKey));
    QTRY_VERIFY(editor->hasFocus());
    QVERIFY2(editorHasPrimaryOutline(editor, primary), qPrintable(editor->styleSheet()));
    // A late passive replay must not deactivate the editor after focus returns.
    QFocusEvent lateReplay(QEvent::FocusIn, Qt::ActiveWindowFocusReason);
    QCoreApplication::sendEvent(dockSearch, &lateReplay);
    QVERIFY(editor->hasFocus());
    QVERIFY(editorHasPrimaryOutline(editor, primary));
    QVERIFY(!editorHasPrimaryOutline(editors.last(), primary));
    QCOMPARE(editor->text(), closeKey == Qt::Key_Escape ? QString() : QStringLiteral("e"));
    QVERIFY(editors.last()->text().isEmpty());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(!popup);
}

int main(int argc, char** argv)
{
    return UiTestFixture::run<TestEditorFocusUi>(argc, argv);
}

#include "testeditorfocusui.moc"
