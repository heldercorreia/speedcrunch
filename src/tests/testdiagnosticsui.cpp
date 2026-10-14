// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mainwindowtesthelpers.h"
#include "core/startupdiagnostics.h"
#include "gui/diagnosticsdialog.h"
#include "gui/diagnosticapplication.h"
#include <QProcess>
#include <QThread>

using namespace MainWindowTestHelpers;

class TestDiagnosticsUi : public QObject {
    Q_OBJECT
private slots:
    void init() { UiTestFixture::resetSettings(); }
    void timings_aggregate_calls_and_freeze_after_startup();
    void worker_and_queued_restore_keep_startup_open();
    void report_includes_platform_and_language_details();
    void help_action_opens_plain_text_report_and_copies_everything();
    void saved_session_restore_records_worker_and_ui_phases();
    void application_finishes_recording_after_paint_and_restore();
};

void TestDiagnosticsUi::timings_aggregate_calls_and_freeze_after_startup()
{
    StartupDiagnostics::begin();
    {
        StartupDiagnostics::Scope parent("Parent");
        for (int i = 0; i < 3; ++i) {
            StartupDiagnostics::Scope child("Child");
            QTest::qSleep(1);
        }
        StartupDiagnostics::firstPaintFinished();
        StartupDiagnostics::finishWhenIdle();
        QVERIFY(!StartupDiagnostics::snapshot().complete);
    }
    StartupDiagnostics::finishWhenIdle();
    const auto snapshot = StartupDiagnostics::snapshot();
    QVERIFY(snapshot.complete);
    QCOMPARE(snapshot.phases.size(), 2);
    QCOMPARE(snapshot.phases.at(1).calls, 3);
    QVERIFY(snapshot.phases.at(1).totalNs >= snapshot.phases.at(1).longestNs);
    QVERIFY(snapshot.phases.at(0).totalNs >= snapshot.phases.at(1).totalNs);
    {
        StartupDiagnostics::Scope later("After startup");
        QTest::qSleep(1);
    }
    QCOMPARE(StartupDiagnostics::snapshot().phases.size(), snapshot.phases.size());
    QCOMPARE(StartupDiagnostics::snapshot().elapsedNs, snapshot.elapsedNs);
}

void TestDiagnosticsUi::worker_and_queued_restore_keep_startup_open()
{
    StartupDiagnostics::begin();
    auto task = StartupDiagnostics::task("Async restore");
    bool applied = false;
    QThread* thread = QThread::create([task, &applied]() {
        StartupDiagnostics::Scope read("Read JSON");
        QTest::qSleep(2);
        QMetaObject::invokeMethod(qApp, [task, &applied]() {
            StartupDiagnostics::Scope apply("Apply JSON");
            applied = true;
        }, Qt::QueuedConnection);
    });
    thread->start();
    QVERIFY(thread->wait(5000));
    delete thread;
    task.reset();
    StartupDiagnostics::firstPaintFinished();
    StartupDiagnostics::finishWhenIdle();
    QVERIFY(!StartupDiagnostics::snapshot().complete);
    QVERIFY(!applied);
    QCoreApplication::processEvents();
    QVERIFY(applied);
    StartupDiagnostics::finishWhenIdle();
    const auto snapshot = StartupDiagnostics::snapshot();
    QVERIFY(snapshot.complete);
    QCOMPARE(snapshot.phases.size(), 3);
    QVERIFY(snapshot.phases.at(1).worker);
    QVERIFY(!snapshot.phases.at(2).worker);
    QVERIFY(snapshot.phases.first().totalNs >= snapshot.phases.at(1).totalNs);
}

void TestDiagnosticsUi::report_includes_platform_and_language_details()
{
    MainWindowStateGuard guard;
    StartupDiagnostics::begin();
    MainWindow window(false);
    StartupDiagnostics::firstPaintFinished();
    StartupDiagnostics::finishWhenIdle();
    guard.settings->language = QStringLiteral("C");
    const QString report = appDiagnosticsReport(QStringLiteral("Loaded sessions: 1\n"));
    QVERIFY(report.contains(QStringLiteral("SpeedCrunch " SPEEDCRUNCH_VERSION)));
#ifdef SPEEDCRUNCH_PORTABLE
    QVERIFY(report.contains(QStringLiteral("Portable: yes")));
#else
    QVERIFY(report.contains(QStringLiteral("Portable: no")));
#endif
    QVERIFY(report.contains(QSysInfo::prettyProductName()));
    QVERIFY(report.contains(QSysInfo::productVersion()));
    QVERIFY(report.contains(QStringLiteral("Qt (runtime / build): %1 / %2")
                            .arg(qVersion(), QT_VERSION_STR)));
    QVERIFY(report.contains(QStringLiteral("App language: System (%1)").arg(QLocale().name())));
    QVERIFY(report.contains(QStringLiteral("System language: %1").arg(QLocale::system().name())));
    QVERIFY(report.contains(QStringLiteral("Qt platform: %1").arg(QGuiApplication::platformName())));
    QVERIFY(report.contains(QStringLiteral("Status: complete")));
    QVERIFY(report.contains(QStringLiteral("Construct main window (total)")));
    QVERIFY(report.contains(QStringLiteral("Create constants dock")));
    QVERIFY(report.contains(QStringLiteral("Create menus")));
    guard.settings->language = QStringLiteral("fr_FR");
    QVERIFY(appDiagnosticsReport(QString()).contains(QStringLiteral("App language: fr_FR")));
}

void TestDiagnosticsUi::help_action_opens_plain_text_report_and_copies_everything()
{
    MainWindowStateGuard guard;
    StartupDiagnostics::begin();
    MainWindow window(false);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    StartupDiagnostics::firstPaintFinished();
    StartupDiagnostics::finishWhenIdle();
    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor);
    editor->setText(QStringLiteral("sensitiveCalculation123"));
    QAction* action = window.findChild<QAction*>(QStringLiteral("AppDiagnosticsAction"));
    QVERIFY(action);
    QCOMPARE(action->menuRole(), QAction::NoRole);
    bool inHelp = false;
    for (QAction* menuAction : window.menuBar()->actions()) {
        if (QMenu* menu = menuAction->menu()) {
            if (menu->title().remove('&') == QStringLiteral("Help"))
                inHelp = menu->actions().contains(action);
        }
    }
    QVERIFY(inHelp);
    QString copiedReport;
    const QString settingsPath = Settings::getConfigPath() + QStringLiteral("/SpeedCrunch.ini");
    QFile settingsFile(settingsPath);
    QVERIFY(settingsFile.open(QIODevice::ReadOnly));
    const QByteArray settingsBefore = settingsFile.readAll();
    settingsFile.close();
    QTimer::singleShot(0, &window, [&]() {
        auto* dialog = window.findChild<DiagnosticsDialog*>();
        if (!dialog)
            return;
        const auto close = qScopeGuard([dialog]() { dialog->reject(); });
        QPlainTextEdit* text = dialog->findChild<QPlainTextEdit*>(QStringLiteral("DiagnosticsText"));
        QPushButton* copy = dialog->findChild<QPushButton*>(QStringLiteral("CopyDiagnostics"));
        QVERIFY(text);
        QVERIFY(copy);
        QVERIFY(text->isReadOnly());
        QCOMPARE(text->lineWrapMode(), QPlainTextEdit::NoWrap);
        QVERIFY(text->toPlainText().contains(QStringLiteral("Loaded sessions: 1")));
        QVERIFY(!text->toPlainText().contains(QStringLiteral("sensitiveCalculation123")));
        QVERIFY(!text->toPlainText().contains(Settings::getConfigPath()));
        text->selectAll();
        text->moveCursor(QTextCursor::Start);
        QTest::mouseClick(copy, Qt::LeftButton);
        copiedReport = QApplication::clipboard()->text();
        QCOMPARE(copiedReport, text->toPlainText());
        if (qEnvironmentVariableIsSet("SPEEDCRUNCH_DIAGNOSTICS_SCREENSHOT"))
            QVERIFY(dialog->grab().save(qEnvironmentVariable("SPEEDCRUNCH_DIAGNOSTICS_SCREENSHOT")));
    });
    action->trigger();
    QVERIFY(!copiedReport.isEmpty());
    QVERIFY(settingsFile.open(QIODevice::ReadOnly));
    QCOMPARE(settingsFile.readAll(), settingsBefore);
    QVERIFY(StartupDiagnostics::snapshot().complete);
}

void TestDiagnosticsUi::saved_session_restore_records_worker_and_ui_phases()
{
    MainWindowStateGuard guard;
    const QString name = QStringLiteral("Diagnostics restore test");
    Session session;
    session.setName(name);
    session.setEditorText(QStringLiteral("privateExpression456"));
    QJsonObject json;
    session.serialize(json);
    const QString directory = Settings::getDataPath() + QStringLiteral("/sessions");
    QVERIFY(QDir().mkpath(directory));
    const QString filename = QStringLiteral("diagnostics-restore-test.json");
    QFile file(QDir(directory).filePath(filename));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(json).toJson());
    file.close();
    const QJsonObject tab{{"name", name}, {"file", filename},
                          {"editor", QJsonObject{{"text", "privateExpression456"}}}};
    const QJsonObject root{{"type", "tabs"}, {"active", name}, {"tabs", QJsonArray{tab}}};
    const QJsonObject savedWindow{{"id", "diagnostics-window"}, {"root", root}};
    const QJsonObject layout{{"scheme", 1}, {"kind", "session-layout"},
                            {"windows", QJsonArray{savedWindow}},
                            {"activeWindow", "diagnostics-window"}};
    guard.settings->sessionLayoutJson = QString::fromUtf8(QJsonDocument(layout).toJson());
    StartupDiagnostics::begin();
    MainWindow window;
    StartupDiagnostics::firstPaintFinished();
    StartupDiagnostics::finishWhenIdle();
    QVERIFY(!StartupDiagnostics::snapshot().complete);
    QTRY_VERIFY(window.findChild<Editor*>()->text() == QStringLiteral("privateExpression456"));
    QTRY_VERIFY(([&]() {
        StartupDiagnostics::finishWhenIdle();
        return StartupDiagnostics::snapshot().complete;
    })());
    const auto snapshot = StartupDiagnostics::snapshot();
    bool readOnWorker = false;
    bool deserialized = false;
    bool restoredPanes = false;
    for (const auto& phase : snapshot.phases) {
        if (phase.name == QStringLiteral("Read and parse session JSON"))
            readOnWorker = phase.worker && phase.calls == 1;
        if (phase.name == QStringLiteral("Deserialize session"))
            deserialized = !phase.worker && phase.calls == 1;
        if (phase.name == QStringLiteral("Rebuild session panes and tabs"))
            restoredPanes = !phase.worker && phase.calls == 1;
    }
    QVERIFY(readOnWorker);
    QVERIFY(deserialized);
    QVERIFY(restoredPanes);
    const QString report = appDiagnosticsReport(QString());
    QVERIFY(!report.contains(name));
    QVERIFY(!report.contains(QStringLiteral("privateExpression456")));
    QVERIFY(!report.contains(filename));
}

void TestDiagnosticsUi::application_finishes_recording_after_paint_and_restore()
{
    QProcess process;
    process.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--startup-probe")});
    QVERIFY(process.waitForFinished(10000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

static int runStartupProbe(int argc, char** argv)
{
    QTemporaryDir storage;
    if (!storage.isValid())
        return 1;
    qputenv("SPEEDCRUNCH_UI_TEST_STORAGE", storage.path().toUtf8());
    StartupDiagnostics::begin();
    DiagnosticApplication application(argc, argv);
    UiTestFixture::resetSettings();
    MainWindow window;
    window.show();
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &application, [&]() {
        const auto snapshot = StartupDiagnostics::snapshot();
        if (!snapshot.complete)
            return;
        bool painted = false;
        bool restored = false;
        for (const auto& phase : snapshot.phases) {
            if (phase.name == QStringLiteral("Paint startup widgets"))
                painted = phase.calls > 0;
            if (phase.name == QStringLiteral("Restore single session (async total)"))
                restored = phase.calls > 0;
        }
        application.exit(painted && restored && snapshot.firstPaintNs >= 0 ? 0 : 2);
    });
    poll.start(10);
    QTimer::singleShot(5000, &application, [&]() { application.exit(3); });
    return application.exec();
}

int main(int argc, char** argv)
{
    if (argc == 2 && QByteArray(argv[1]) == "--startup-probe")
        return runStartupProbe(argc, argv);
    return UiTestFixture::run<TestDiagnosticsUi>(argc, argv);
}

#include "testdiagnosticsui.moc"
