// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GUI_STARTUPFONTPREPARATION_H
#define GUI_STARTUPFONTPREPARATION_H

#include "core/startupdiagnostics.h"

#include <QChar>
#include <QFontDatabase>
#include <QStringList>
#include <QThread>
#include <QtGlobal>
#include <memory>

class StartupFontPreparation {
public:
    StartupFontPreparation()
    {
        // Some platforms create their font backend on first use without a lock.
        // Create it here, so the worker below cannot race the UI thread for it.
        // This call does not load the font list.
        QFontDatabase::standardSizes();
        useSystemFontForIndicScripts();

        // The first text widget otherwise loads the font database on the UI
        // thread. QFontDatabase is thread-safe, so overlap that work with window
        // construction after QApplication has initialized. The UI thread waits
        // inside Qt when it needs a font before the worker has finished.
        const auto task = StartupDiagnostics::task("Prepare font database (async total)");
        m_thread.reset(QThread::create([task]() {
            {
                StartupDiagnostics::Scope timing("Prepare font database");
                QFontDatabase::families();
            }
            if (task)
                task->finish();
        }));
        m_thread->start();
    }

    ~StartupFontPreparation() { wait(); }

    void wait()
    {
        if (!m_thread)
            return;
        StartupDiagnostics::Scope timing("Wait for font preparation");
        m_thread->wait();
        m_thread.reset();
    }

private:
    // Without this, Qt can pick a font like Adobe Blank for these scripts,
    // and the text is invisible.
    static void useSystemFontForIndicScripts()
    {
#if defined(Q_OS_WIN) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        const QStringList families{QStringLiteral("Nirmala UI")};
        for (const QChar::Script script : {QChar::Script_Bengali, QChar::Script_Devanagari,
                 QChar::Script_Gujarati, QChar::Script_Gurmukhi, QChar::Script_Kannada,
                 QChar::Script_Malayalam, QChar::Script_Oriya, QChar::Script_Sinhala,
                 QChar::Script_Tamil, QChar::Script_Telugu}) {
            QFontDatabase::setApplicationFallbackFontFamilies(script, families);
        }
#endif
    }

    std::unique_ptr<QThread> m_thread;
};

#endif
