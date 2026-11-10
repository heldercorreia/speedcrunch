// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SPEEDCRUNCH_UITESTFIXTURE_H
#define SPEEDCRUNCH_UITESTFIXTURE_H

#ifndef SPEEDCRUNCH_UI_TEST
#error UiTestFixture requires the SPEEDCRUNCH_UI_TEST build definition
#endif

#include "core/settings.h"

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QKeyEvent>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

namespace UiTestFixture {

inline bool isWayland()
{
    // The same Linux binary can use either xcb or Wayland at runtime.
    return QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
}

inline bool activateWindow(QWidget* window)
{
    window->activateWindow();
    if (isWayland() && !QTest::qWaitFor([window]() {
            return QApplication::activeWindow() == window
                && QGuiApplication::focusWindow() == window->windowHandle();
        }, 1000)) {
        QTest::qSkip("The Wayland compositor declines test window activation without native user input.",
                     __FILE__, __LINE__);
        return false;
    }
    return true;
}

class Application : public QApplication {
public:
    using QApplication::QApplication;

    bool notify(QObject* receiver, QEvent* event) override
    {
        if (receiver != nullptr && receiver->isWidgetType() && event->spontaneous()
            && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
            const auto* key = static_cast<QKeyEvent*>(event);
            if (key->timestamp() == 0 && key->nativeScanCode() == 0
                && key->nativeVirtualKey() == 0 && key->nativeModifiers() == 0) {
                // QtTest marks synthetic widget keys as spontaneous. Qt's
                // AT-SPI bridge can queue them for asynchronous delivery.
                // sendEvent keeps test input synchronous and dispatches shortcuts.
                return QCoreApplication::sendEvent(receiver, event);
            }
        }
        return QApplication::notify(receiver, event);
    }
};

inline void resetSettings()
{
    QSettings persisted(Settings::getConfigPath() + QStringLiteral("/SpeedCrunch.ini"),
                        QSettings::IniFormat);
    persisted.clear();
    // Start with the current schema so loading defaults cannot migrate the
    // user's legacy native settings into the isolated test directory.
    persisted.setValue(QStringLiteral("ConfigVersion"), 1200);
    persisted.sync();
    if (persisted.status() != QSettings::NoError)
        qFatal("Cannot reset isolated UI test settings.");
    Settings* settings = Settings::instance();
    settings->load();
    settings->hasNumberFormatStyleSetting = true;
    settings->windowPositionSave = false;
}

template<class Test>
int run(int argc, char** argv)
{
    QTemporaryDir storage(QDir::tempPath() + QStringLiteral("/speedcrunch-ui-test-XXXXXX"));
    if (!storage.isValid()) {
        qCritical("Cannot create temporary application storage for UI tests.");
        return 1;
    }
    qputenv("SPEEDCRUNCH_UI_TEST_STORAGE", storage.path().toUtf8());
    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
#if defined(Q_OS_WIN)
    // Windows offscreen uses FreeType, which does not discover system fonts.
    // Keep an explicit font directory when the caller supplies one.
    if (qgetenv("QT_QPA_PLATFORM").startsWith("offscreen")
        && qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR")) {
        const QString windowsDirectory = qEnvironmentVariable("SystemRoot");
        if (!windowsDirectory.isEmpty()) {
            const QString fonts = QDir(windowsDirectory).filePath(QStringLiteral("Fonts"));
            if (QDir(fonts).exists())
                qputenv("QT_QPA_FONTDIR", fonts.toUtf8());
        }
    }
#endif
    Application app(argc, argv);
    for (const QString& path : {Settings::getConfigPath(), Settings::getDataPath(),
                                Settings::getCachePath()}) {
        if (!QDir().mkpath(path)) {
            qCritical("Cannot create UI test storage at %s.", qPrintable(path));
            return 1;
        }
    }
    resetSettings();

    // A test that accidentally enables the first-run prompt should fail
    // instead of blocking forever in its nested event loop.
    QTimer startupDialogWatchdog;
    QObject::connect(&startupDialogWatchdog, &QTimer::timeout, &app, []() {
        QDialog* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (dialog && dialog->inherits("NumberFormatDialog")
            && !qApp->property("speedcrunchExpectedNumberFormatDialog").toBool()) {
            QTest::qFail("Unexpected first-run number format dialog", __FILE__, __LINE__);
            dialog->reject();
        }
    });
    startupDialogWatchdog.start(50);
    Test test;
    return QTest::qExec(&test, argc, argv);
}

} // namespace UiTestFixture

#endif
