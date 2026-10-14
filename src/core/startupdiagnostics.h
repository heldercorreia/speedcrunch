// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_STARTUPDIAGNOSTICS_H
#define CORE_STARTUPDIAGNOSTICS_H

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QThread>
#include <QVector>
#include <atomic>
#include <memory>

// Only startup is measured. Fixed phase names keep the report free of user data.
// Repeated phases are aggregated so restoring many sessions uses bounded memory.
namespace StartupDiagnostics {

struct Phase {
    QString name;
    bool worker = false;
    qint64 firstStartNs = 0;
    qint64 totalNs = 0;
    qint64 longestNs = 0;
    int calls = 0;
    int running = 0;
};

struct Snapshot {
    QVector<Phase> phases;
    qint64 elapsedNs = 0;
    qint64 firstPaintNs = -1;
    bool started = false;
    bool complete = false;
};

namespace Detail {
struct State {
    QMutex mutex;
    QElapsedTimer clock;
    QVector<Phase> phases;
    std::atomic_bool recording{false};
    quint64 generation = 0;
    int openScopes = 0;
    qint64 firstPaintNs = -1;
    qint64 finishedNs = 0;

};

inline State& state()
{
    static State value;
    return value;
}
}

inline void begin()
{
    auto& state = Detail::state();
    QMutexLocker lock(&state.mutex);
    ++state.generation;
    state.phases.clear();
    state.openScopes = 0;
    state.firstPaintNs = -1;
    state.finishedNs = 0;
    state.clock.start();
    state.recording.store(true);
}

inline bool isRecording() { return Detail::state().recording.load(); }

class Scope {
public:
    explicit Scope(const char* name) { start(name); }
    ~Scope() { finish(); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    void next(const char* name)
    {
        finish();
        start(name);
    }

    void finish()
    {
        if (m_index < 0)
            return;
        auto& state = Detail::state();
        QMutexLocker lock(&state.mutex);
        if (m_generation == state.generation) {
            const qint64 elapsed = state.clock.nsecsElapsed() - m_startNs;
            auto& phase = state.phases[m_index];
            phase.totalNs += elapsed;
            phase.longestNs = qMax(phase.longestNs, elapsed);
            ++phase.calls;
            --phase.running;
            --state.openScopes;
        }
        m_index = -1;
    }

private:
    void start(const char* name)
    {
        auto& state = Detail::state();
        if (!state.recording.load())
            return;
        QMutexLocker lock(&state.mutex);
        if (!state.recording.load())
            return;
        m_generation = state.generation;
        m_startNs = state.clock.nsecsElapsed();
        const bool worker = QCoreApplication::instance()
            && QThread::currentThread() != QCoreApplication::instance()->thread();
        const QString label = QString::fromLatin1(name);
        for (int i = 0; i < state.phases.size(); ++i) {
            if (state.phases[i].name == label && state.phases[i].worker == worker) {
                m_index = i;
                break;
            }
        }
        if (m_index < 0) {
            m_index = state.phases.size();
            state.phases.append(Phase{label, worker, m_startNs});
        }
        ++state.phases[m_index].running;
        ++state.openScopes;
    }

    quint64 m_generation = 0;
    qint64 m_startNs = 0;
    int m_index = -1;
};

// A shared scope spans a worker and its queued UI callback, including queue wait.
inline std::shared_ptr<Scope> task(const char* name)
{
    return isRecording() ? std::make_shared<Scope>(name) : nullptr;
}

inline void firstPaintFinished()
{
    auto& state = Detail::state();
    QMutexLocker lock(&state.mutex);
    if (!state.recording.load() || state.firstPaintNs >= 0)
        return;
    state.firstPaintNs = state.clock.nsecsElapsed();
}

// The dispatcher reaches this point after handling paints and queued callbacks.
inline void finishWhenIdle()
{
    auto& state = Detail::state();
    QMutexLocker lock(&state.mutex);
    if (!state.recording.load() || state.firstPaintNs < 0 || state.openScopes != 0)
        return;
    state.finishedNs = state.clock.nsecsElapsed();
    state.recording.store(false);
}

inline Snapshot snapshot()
{
    auto& state = Detail::state();
    QMutexLocker lock(&state.mutex);
    Snapshot result;
    result.started = state.clock.isValid();
    if (!result.started)
        return result;
    result.phases = state.phases;
    result.complete = !state.recording.load();
    result.elapsedNs = result.complete ? state.finishedNs : state.clock.nsecsElapsed();
    result.firstPaintNs = state.firstPaintNs;
    return result;
}

}

#endif
