// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "startuploadtest.h"
#include "core/session.h"
#include "core/sessionjsonkeys.h"
#include "core/settings.h"
#include "core/startupdiagnostics.h"
#include "gui/diagnosticsdialog.h"
#include "gui/mainwindow.h"
#include "gui/resultdisplay.h"
#include "gui/resultlineformatutils.h"

#include <QApplication>
#include <QAction>
#include <QCommandLineParser>
#include <QDir>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLayout>
#include <QMainWindow>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QWindow>

#include <memory>

int startupLoadApplicationMain(int argc, char* argv[]);

namespace {

struct Workload {
    QString name;
    int tabs;
    int history;
    int panes;
    bool docks;
};

Workload workload;
QString stylePreset;
QString reportPath;

QString outputPath(const QString& suffix)
{
    const QFileInfo report(reportPath);
    return report.dir().filePath(report.completeBaseName() + suffix);
}

bool selectScenario(const QString& name, const QString& preset)
{
    if ((name != QLatin1String("baseline") && name != QLatin1String("heavy"))
        || (preset != QLatin1String("classic") && preset != QLatin1String("modern")))
        return false;
    workload = name == QLatin1String("heavy") ? Workload {name, 30, 1000, 6, true}
                                              : Workload {name, 1, 10, 1, false};
    stylePreset = preset;
    return true;
}

struct DockPlacement {
    const char* name;
    Qt::DockWidgetArea area;
};

const DockPlacement dockPlacements[] = {
    {"BookDock", Qt::LeftDockWidgetArea},
    {"VariablesDock", Qt::LeftDockWidgetArea},
    {"UserFunctionsDock", Qt::RightDockWidgetArea},
    {"UserUnitsDock", Qt::RightDockWidgetArea},
    {"ConstantsDock", Qt::TopDockWidgetArea},
    {"FunctionsDock", Qt::TopDockWidgetArea},
    {"HistoryDock", Qt::TopDockWidgetArea},
    {"BitfieldDock", Qt::BottomDockWidgetArea}
};

QJsonObject savedWindow(const QJsonObject& node)
{
    // Only placeholder widgets are needed to save a portable Qt dock layout.
    // The measured process restores it with the real application widgets.
    QMainWindow layout;
    layout.setCentralWidget(new QWidget(&layout));
    layout.setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    layout.setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    layout.setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
    layout.setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    QJsonArray openDocks;
    if (workload.docks) {
        QHash<Qt::DockWidgetArea, QDockWidget*> previous;
        for (const DockPlacement& placement : dockPlacements) {
            auto* dock = new QDockWidget(&layout);
            dock->setObjectName(QString::fromLatin1(placement.name));
            dock->setWidget(new QWidget(dock));
            dock->setMinimumSize(160, 120);
            layout.addDockWidget(placement.area, dock);
            if (QDockWidget* first = previous.value(placement.area)) {
                const Qt::Orientation orientation = placement.area == Qt::LeftDockWidgetArea
                    || placement.area == Qt::RightDockWidgetArea ? Qt::Vertical : Qt::Horizontal;
                layout.splitDockWidget(first, dock, orientation);
            }
            previous.insert(placement.area, dock);
            openDocks.append(dock->objectName());
        }
    }
    const QRect available = QGuiApplication::primaryScreen()->availableGeometry().adjusted(24, 32, -24, -24);
    layout.resize(QSize(workload.docks ? 1280 : 640, workload.docks ? 800 : 480).boundedTo(available.size()));
    layout.move(available.center() - layout.rect().center());
    layout.layout()->activate();
    return QJsonObject {
        {QStringLiteral("id"), QStringLiteral("load")},
        {QStringLiteral("root"), node},
        {QStringLiteral("openDocks"), openDocks},
        {QStringLiteral("windowState"), QString::fromLatin1(layout.saveState(1).toBase64())},
        {QStringLiteral("geometry"), QString::fromLatin1(layout.saveGeometry().toBase64())}
    };
}

QString sessionName(int index)
{
    return QStringLiteral("L%1").arg(index + 1, 2, 10, QLatin1Char('0'));
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit())
        return true;
    qCritical("Cannot write %s: %s", qPrintable(path), qPrintable(file.errorString()));
    return false;
}

bool prepareWorkload(const QString& root)
{
    for (const QString& directory : {QStringLiteral("config"), QStringLiteral("data/sessions"),
                                      QStringLiteral("cache")}) {
        if (!QDir(root).mkpath(directory))
            return false;
    }
    qputenv("SPEEDCRUNCH_UI_TEST_STORAGE", root.toUtf8());
    {
        QSettings settings(QDir(root).filePath(QStringLiteral("config/SpeedCrunch.ini")), QSettings::IniFormat);
        settings.setValue(QStringLiteral("ConfigVersion"), 1200);
        settings.setValue(QStringLiteral("SpeedCrunch/General/NumberFormatStyle"), 0);
        settings.sync();
        if (settings.status() != QSettings::NoError)
            return false;
    }
    Session source;
    Evaluator* evaluator = source.evaluator();
    QJsonArray history;
    for (int i = 0; i < workload.history; ++i) {
        QJsonObject entry;
        const QString expression = QStringLiteral("%1*7").arg(i + 1);
        evaluator->setExpression(expression);
        const Quantity result = evaluator->evalUpdateAns();
        if (!evaluator->error().isEmpty()) {
            qCritical("Cannot evaluate startup history: %s", qPrintable(evaluator->error()));
            return false;
        }
        HistoryEntry item(expression, result, evaluator->interpretedExpression());
        // Save the same expression and result lines as a normal calculation.
        item.setRenderedLines(ResultLineFormatUtils::formatResultLinesForDisplay(
            item.expr(), item.interpretedExpr(), result, true, true, evaluator));
        item.setEditTimestamp(0);
        item.serialize(entry);
        history.append(entry);
    }
    QJsonArray panes;
    for (int pane = 0; pane < workload.panes; ++pane) {
        QJsonArray tabs;
        const int first = pane * workload.tabs / workload.panes;
        const int last = (pane + 1) * workload.tabs / workload.panes;
        for (int i = first; i < last; ++i) {
            const QString name = sessionName(i);
            const QJsonObject session {
                {QLatin1String(SessionJsonKeys::Schema), QLatin1String(SessionJsonKeys::SchemaDialect)},
                {QLatin1String(SessionJsonKeys::Id), QLatin1String(SessionJsonKeys::SchemaId)},
                {QLatin1String(SessionJsonKeys::Session), name},
                {QLatin1String(SessionJsonKeys::Limit), workload.history},
                {QLatin1String(SessionJsonKeys::History), history},
                {QLatin1String(SessionJsonKeys::Variables), QJsonArray()},
                {QLatin1String(SessionJsonKeys::Functions), QJsonArray()},
                {QLatin1String(SessionJsonKeys::Units), QJsonArray()},
                {QLatin1String(SessionJsonKeys::Globals), QJsonArray()}
            };
            if (!writeFile(QDir(root).filePath(QStringLiteral("data/sessions/%1.json").arg(name)),
                           QJsonDocument(session).toJson(QJsonDocument::Compact)))
                return false;
            tabs.append(QJsonObject {{QStringLiteral("name"), name}});
        }
        panes.append(QJsonObject {
            {QStringLiteral("type"), QStringLiteral("pane")},
            {QStringLiteral("active"), sessionName(first)},
            {QStringLiteral("tabs"), tabs}
        });
    }
    QJsonObject node = panes.first().toObject();
    if (workload.panes > 1) {
        QJsonArray rows;
        for (int i = 0; i < panes.size(); i += 2) {
            rows.append(QJsonObject {
                {QStringLiteral("type"), QStringLiteral("split")},
                {QStringLiteral("orientation"), QStringLiteral("horizontal")},
                {QStringLiteral("sizes"), QJsonArray {1, 1}},
                {QStringLiteral("children"), QJsonArray {panes.at(i), panes.at(i + 1)}}
            });
        }
        node = QJsonObject {
            {QStringLiteral("type"), QStringLiteral("split")},
            {QStringLiteral("orientation"), QStringLiteral("vertical")},
            {QStringLiteral("active"), sessionName(0)},
            {QStringLiteral("sizes"), QJsonArray {1, 1, 1}},
            {QStringLiteral("children"), rows}
        };
    } else {
        node.insert(QStringLiteral("type"), QStringLiteral("tabs"));
    }
    const QJsonObject layout {
        {QStringLiteral("scheme"), 1},
        {QStringLiteral("kind"), QStringLiteral("session-layout")},
        {QStringLiteral("activeWindow"), QStringLiteral("load")},
        {QStringLiteral("windows"), QJsonArray {savedWindow(node)}}
    };
    QSettings settings(QDir(root).filePath(QStringLiteral("config/SpeedCrunch.ini")), QSettings::IniFormat);
    settings.setValue(QStringLiteral("SpeedCrunch/General/OpenPrivateTabOnStartup"), false);
    const bool classic = stylePreset == QLatin1String("classic");
    const QString displayPrefix = QStringLiteral("SpeedCrunch/Display/");
    settings.setValue(displayPrefix + QStringLiteral("MenuAppearance"), int(classic
        ? Settings::MenuAppearanceSystem : Settings::MenuAppearanceSpeedCrunch));
    settings.setValue(displayPrefix + QStringLiteral("DockAppearance"), int(classic
        ? Settings::DockAppearanceSystem : Settings::DockAppearanceSpeedCrunch));
    settings.setValue(displayPrefix + QStringLiteral("StatusBarAppearance"), int(classic
        ? Settings::StatusBarAppearanceSystem : Settings::StatusBarAppearanceSpeedCrunch));
    settings.setValue(displayPrefix + QStringLiteral("EditorDisplayStyle"), int(classic
        ? Settings::EditorDisplayClassic : Settings::EditorDisplayModern));
    settings.setValue(displayPrefix + QStringLiteral("KeypadAppearance"), int(classic
        ? Settings::KeypadAppearanceClassic : Settings::KeypadAppearanceModern));
    const QString prefix = QStringLiteral("SpeedCrunch/Layout/");
    settings.setValue(prefix + QStringLiteral("SessionLayoutJson"),
                      QString::fromUtf8(QJsonDocument(layout).toJson(QJsonDocument::Compact)));
    settings.setValue(prefix + QStringLiteral("KeypadMode"), int(workload.docks
        ? Settings::KeypadModeScientificWide : Settings::KeypadModeDisabled));
    settings.setValue(prefix + QStringLiteral("KeypadZoomPercent"), workload.docks ? 50 : 100);
    settings.setValue(prefix + QStringLiteral("StatusBarVisible"), workload.docks);
    for (const QString& key : {QStringLiteral("HistoryDockVisible"), QStringLiteral("FunctionsDockVisible"),
                                 QStringLiteral("VariablesDockVisible"), QStringLiteral("UserFunctionsDockVisible"),
                                 QStringLiteral("UserUnitsDockVisible"), QStringLiteral("FormulaBookDockVisible"),
                                 QStringLiteral("ConstantsDockVisible"), QStringLiteral("BitfieldVisible")})
        settings.setValue(prefix + key, workload.docks);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

} // namespace

void StartupLoadTest::watch(MainWindow& window)
{
    auto* poll = new QTimer(&window);
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();
    QObject::connect(poll, &QTimer::timeout, &window, [&window, poll, elapsed]() {
        const bool timeout = elapsed->elapsed() > 480000;
        QString error;
        if (QApplication::activeModalWidget())
            error = QStringLiteral("Unexpected modal dialog during startup");
        else if (timeout)
            error = QStringLiteral("Startup does not finish within 480 seconds");
        else if (StartupDiagnostics::isRecording() || window.m_sessionRestorePending
                 || !window.windowHandle() || !window.windowHandle()->isExposed())
            return;

        poll->stop();
        const auto snapshot = StartupDiagnostics::snapshot();
        const bool classic = stylePreset == QLatin1String("classic");
        const bool presetMatches = classic ? window.m_actions.settingsAppearancePresetClassic->isChecked()
                                           : window.m_actions.settingsAppearancePresetModern->isChecked();
        if (error.isEmpty() && !presetMatches)
            error = QStringLiteral("Restored appearance does not match the selected style preset");
        int tabs = 0;
        for (QTabBar* bar : window.m_paneTabBars)
            tabs += bar->count();
        qint64 history = 0;
        if (error.isEmpty() && (window.m_loadedSessions.size() != workload.tabs || tabs != workload.tabs
                               || window.m_paneTabBars.size() != workload.panes))
            error = QStringLiteral("Restored session, tab or pane count differs from the workload");
        QJsonArray paneGeometry;
        QList<QRect> paneRects;
        for (ResultDisplay* display : window.m_paneTabBars.keys()) {
            QWidget* pane = display;
            while (pane && !qobject_cast<QSplitter*>(pane->parentWidget()))
                pane = pane->parentWidget();
            if (!pane || !pane->isVisible() || !display->isVisible()
                || display->viewport()->visibleRegion().isEmpty()) {
                if (error.isEmpty())
                    error = QStringLiteral("A restored session pane is not visible");
                continue;
            }
            const QRect rect(pane->mapTo(&window, QPoint()), pane->size());
            for (const QRect& previous : paneRects) {
                if (rect.intersects(previous) && error.isEmpty())
                    error = QStringLiteral("Restored session panes overlap");
            }
            if (error.isEmpty() && (!window.rect().contains(rect) || rect.width() < 200 || rect.height() < 100))
                error = QStringLiteral("A restored session pane is clipped or too small to inspect");
            paneRects.append(rect);
            paneGeometry.append(QJsonObject {
                {QStringLiteral("session"), display->session()->name()},
                {QStringLiteral("x"), rect.x()}, {QStringLiteral("y"), rect.y()},
                {QStringLiteral("width"), rect.width()}, {QStringLiteral("height"), rect.height()}
            });
        }
        for (int i = 0; i < workload.tabs; ++i) {
            const Session* session = window.m_loadedSessions.value(sessionName(i));
            if (!session || session->historySize() != workload.history) {
                if (error.isEmpty())
                    error = QStringLiteral("History does not restore for %1").arg(sessionName(i));
            }
            if (session) {
                history += session->historySize();
                for (int entry = 0; entry < session->historySize(); ++entry) {
                    if (!session->historyEntryAtRef(entry).hasRenderedLines() && error.isEmpty())
                        error = QStringLiteral("Saved display lines do not restore for %1").arg(sessionName(i));
                }
            }
        }
        int openDocks = 0;
        for (QDockWidget* dock : window.m_allDocks) {
            if (dock->isVisible())
                ++openDocks;
        }
        const bool keypadVisible = window.m_widgets.keypad && window.m_widgets.keypad->isVisible();
        if (error.isEmpty() && (openDocks != (workload.docks ? 8 : 0)
                               || keypadVisible != workload.docks))
            error = QStringLiteral("Dock or keypad visibility differs from the workload");
        if (workload.docks && error.isEmpty()) {
            QList<QRect> dockRects;
            for (const DockPlacement& placement : dockPlacements) {
                QDockWidget* dock = window.findChild<QDockWidget*>(QString::fromLatin1(placement.name));
                if (!dock || window.dockWidgetArea(dock) != placement.area || dock->isFloating()
                    || !window.tabifiedDockWidgets(dock).isEmpty() || dock->visibleRegion().isEmpty()) {
                    error = QStringLiteral("Dock is not separately visible in its saved area: %1")
                        .arg(QString::fromLatin1(placement.name));
                    break;
                }
                const QRect rect(dock->mapTo(&window, QPoint()), dock->size());
                if (!window.rect().contains(rect) && error.isEmpty())
                    error = QStringLiteral("A restored dock is clipped by the window");
                for (const QRect& previous : dockRects) {
                    if (rect.intersects(previous))
                        error = QStringLiteral("Restored docks overlap");
                }
                dockRects.append(rect);
            }
            const QRect available = window.screen()->availableGeometry();
            if (!available.contains(window.frameGeometry())) {
                error = QStringLiteral("Startup window extends outside the available screen");
                qInfo() << "Frame" << window.frameGeometry() << "screen" << available
                        << "minimum" << window.minimumSizeHint();
            }
        }
        if (error.isEmpty() && (!snapshot.started || !snapshot.complete || snapshot.firstPaintNs < 0))
            error = QStringLiteral("Startup diagnostics are incomplete");

        const QString counts = QStringLiteral(
            "Startup workload: %1\nLoaded sessions: %2\nTabs: %3\nVisible session panes: %4\n"
            "Loaded history entries: %5\nOpen docks: %6\nKeypad visible: %7\nValidation: %8\n"
            "Style preset: %9\n")
            .arg(workload.name).arg(window.m_loadedSessions.size()).arg(tabs)
            .arg(window.m_paneTabBars.size()).arg(history).arg(openDocks).arg(keypadVisible)
            .arg(error.isEmpty() ? QStringLiteral("passed") : error)
            .arg(classic ? QStringLiteral("Classic") : QStringLiteral("Modern"));
        const bool reportWritten = writeFile(reportPath, appDiagnosticsReport(counts).toUtf8());
        QJsonArray phases;
        for (const auto& phase : snapshot.phases) {
            phases.append(QJsonObject {
                {QStringLiteral("name"), phase.name}, {QStringLiteral("worker"), phase.worker},
                {QStringLiteral("startMs"), phase.firstStartNs / 1e6},
                {QStringLiteral("totalMs"), phase.totalNs / 1e6},
                {QStringLiteral("maxMs"), phase.longestNs / 1e6},
                {QStringLiteral("calls"), phase.calls}
            });
        }
        const QJsonObject summary {
            {QStringLiteral("workload"), workload.name},
            {QStringLiteral("preset"), stylePreset},
            {QStringLiteral("passed"), error.isEmpty()}, {QStringLiteral("error"), error},
            {QStringLiteral("complete"), snapshot.complete},
            {QStringLiteral("elapsedMs"), snapshot.elapsedNs / 1e6},
            {QStringLiteral("firstPaintMs"), snapshot.firstPaintNs / 1e6},
            {QStringLiteral("sessions"), window.m_loadedSessions.size()},
            {QStringLiteral("tabs"), tabs}, {QStringLiteral("panes"), window.m_paneTabBars.size()},
            {QStringLiteral("paneGeometry"), paneGeometry},
            {QStringLiteral("windowWidth"), window.width()}, {QStringLiteral("windowHeight"), window.height()},
            {QStringLiteral("historyEntries"), history}, {QStringLiteral("openDocks"), openDocks},
            {QStringLiteral("keypadVisible"), keypadVisible}, {QStringLiteral("phases"), phases}
        };
        const bool summaryWritten = writeFile(outputPath(QStringLiteral(".json")),
                                              QJsonDocument(summary).toJson());
        const bool screenshotWritten = window.grab().save(outputPath(QStringLiteral(".png")));
        qInfo().noquote() << counts;
        qInfo("Elapsed from main(): %.3f ms. Report: %s", snapshot.elapsedNs / 1e6, qPrintable(reportPath));
        QCoreApplication::exit(error.isEmpty() && reportWritten && summaryWritten && screenshotWritten ? 0 : 1);
    });
    poll->start(100);
}

int main(int argc, char* argv[])
{
    // The child must enter main.cpp before constructing any Qt application.
    if (argc == 5 && QByteArray(argv[1]) == "--startup-load-child") {
        const QString name = QString::fromLocal8Bit(argv[2]);
        if (!selectScenario(name, QString::fromLocal8Bit(argv[3])))
            return 1;
        reportPath = QString::fromLocal8Bit(argv[4]);
        if (qEnvironmentVariableIsEmpty("SPEEDCRUNCH_UI_TEST_STORAGE"))
            return 1;
        return startupLoadApplicationMain(argc, argv);
    }
    QApplication application(argc, argv);
    if (!QGuiApplication::primaryScreen()) {
        qCritical("Startup load tests require an available screen");
        return 1;
    }
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("workload"), QStringLiteral("baseline or heavy"), QStringLiteral("name")});
    parser.addOption({QStringLiteral("preset"), QStringLiteral("classic or modern"), QStringLiteral("name")});
    parser.addOption({QStringLiteral("report"), QStringLiteral("Diagnostic report path"), QStringLiteral("path")});
    parser.process(application);
    const QString name = parser.value(QStringLiteral("workload"));
    if (!selectScenario(name, parser.value(QStringLiteral("preset")))
        || parser.value(QStringLiteral("report")).isEmpty())
        parser.showHelp(1);
    reportPath = QFileInfo(parser.value(QStringLiteral("report"))).absoluteFilePath();
    if (!QDir().mkpath(QFileInfo(reportPath).absolutePath()))
        return 1;
    // Remove previous outputs so a crash cannot leave stale results.
    for (const QString& path : QStringList {reportPath, outputPath(QStringLiteral(".json")),
                                           outputPath(QStringLiteral(".png"))}) {
        if (QFile::exists(path) && !QFile::remove(path)) {
            qCritical("Cannot remove previous report: %s", qPrintable(path));
            return 1;
        }
    }
    QTemporaryDir storage(QDir::tempPath() + QStringLiteral("/speedcrunch-startup-load-XXXXXX"));
    if (!storage.isValid() || !prepareWorkload(storage.path())) {
        qCritical("Cannot prepare isolated startup workload");
        return 1;
    }
    QProcess child;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("SPEEDCRUNCH_UI_TEST_STORAGE"), storage.path());
    environment.insert(QStringLiteral("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"), QStringLiteral("1"));
    environment.insert(QStringLiteral("QT_FORCE_STDERR_LOGGING"), QStringLiteral("1"));
    if (environment.value(QStringLiteral("QT_QPA_PLATFORM")).isEmpty()) {
#ifdef Q_OS_WIN
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("windows"));
#elif defined(Q_OS_MACOS)
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("cocoa"));
#else
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("xcb"));
#endif
    }
    child.setProcessEnvironment(environment);
    child.setProcessChannelMode(QProcess::ForwardedChannels);
    child.start(QCoreApplication::applicationFilePath(),
                {QStringLiteral("--startup-load-child"), name, stylePreset, reportPath});
    if (!child.waitForStarted() || !child.waitForFinished(540000)) {
        qCritical("Startup workload process fails: %s", qPrintable(child.errorString()));
        child.kill();
        child.waitForFinished();
        return 1;
    }
    if (child.exitStatus() != QProcess::NormalExit || child.exitCode() != 0)
        return 1;
    QFile summaryFile(outputPath(QStringLiteral(".json")));
    if (!QFileInfo(reportPath).isFile() || !summaryFile.open(QIODevice::ReadOnly)) {
        qCritical("Startup workload exits without writing both reports");
        return 1;
    }
    const QJsonObject summary = QJsonDocument::fromJson(summaryFile.readAll()).object();
    return summary.value(QStringLiteral("passed")).toBool()
        && summary.value(QStringLiteral("complete")).toBool()
        && summary.value(QStringLiteral("workload")).toString() == name
        && summary.value(QStringLiteral("preset")).toString() == stylePreset ? 0 : 1;
}
