// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GUI_DIAGNOSTICAPPLICATION_H
#define GUI_DIAGNOSTICAPPLICATION_H

#include "core/startupdiagnostics.h"
#include "gui/mainwindow.h"
#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QEvent>
#include <QTimer>

class DiagnosticApplication : public QApplication {
public:
    DiagnosticApplication(int& argc, char** argv) : QApplication(argc, argv)
    {
        connect(QAbstractEventDispatcher::instance(), &QAbstractEventDispatcher::aboutToBlock,
                this, []() { StartupDiagnostics::finishWhenIdle(); });
    }

    bool notify(QObject* receiver, QEvent* event) override
    {
        if (event->type() != QEvent::Paint || !StartupDiagnostics::isRecording())
            return QApplication::notify(receiver, event);
        StartupDiagnostics::Scope timing("Paint startup widgets");
        const bool result = QApplication::notify(receiver, event);
        if (!m_firstPaintQueued && qobject_cast<MainWindow*>(receiver)) {
            m_firstPaintQueued = true;
            QTimer::singleShot(0, this, []() { StartupDiagnostics::firstPaintFinished(); });
        }
        return result;
    }

private:
    bool m_firstPaintQueued = false;
};

#endif
