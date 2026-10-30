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

int main(int argc, char** argv)
{
    return UiTestFixture::run<TestEditorFocusUi>(argc, argv);
}

#include "testeditorfocusui.moc"
