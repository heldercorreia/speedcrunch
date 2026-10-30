// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SPEEDCRUNCH_STARTUPLOADSTATISTICS_H
#define SPEEDCRUNCH_STARTUPLOADSTATISTICS_H

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QPair>
#include <QSet>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace StartupLoadStatistics {

inline QJsonObject statistics(const QVector<double>& values)
{
    if (values.isEmpty())
        return {};
    const double mean = std::accumulate(values.cbegin(), values.cend(), 0.0) / values.size();
    double variance = 0;
    for (double value : values)
        variance += (value - mean) * (value - mean);
    return {
        {QStringLiteral("mean"), mean},
        {QStringLiteral("min"), *std::min_element(values.cbegin(), values.cend())},
        {QStringLiteral("max"), *std::max_element(values.cbegin(), values.cend())},
        {QStringLiteral("stddev"), std::sqrt(variance / values.size())}
    };
}

inline QJsonObject summarize(const QJsonArray& samples)
{
    if (samples.isEmpty())
        return {};
    const QJsonObject first = samples.first().toObject();
    QVector<double> elapsed;
    QVector<double> firstPaint;
    struct PhaseSamples {
        QString name;
        bool worker;
        QVector<double> totals;
        double starts = 0;
        double calls = 0;
        double maxCall = 0;
        int present = 0;
    };
    QVector<PhaseSamples> phases;
    QHash<QPair<QString, bool>, qsizetype> indices;
    for (qsizetype run = 0; run < samples.size(); ++run) {
        const QJsonObject sample = samples.at(run).toObject();
        if (!sample.value(QStringLiteral("passed")).toBool()
            || !sample.value(QStringLiteral("complete")).toBool()
            || !sample.value(QStringLiteral("phases")).isArray()
            || sample.value(QStringLiteral("workload")) != first.value(QStringLiteral("workload"))
            || sample.value(QStringLiteral("preset")) != first.value(QStringLiteral("preset")))
            return {};
        for (const QString& key : {QStringLiteral("elapsedMs"), QStringLiteral("firstPaintMs")}) {
            const QJsonValue value = sample.value(key);
            if (!value.isDouble() || !std::isfinite(value.toDouble()) || value.toDouble() < 0)
                return {};
        }
        elapsed.append(sample.value(QStringLiteral("elapsedMs")).toDouble());
        firstPaint.append(sample.value(QStringLiteral("firstPaintMs")).toDouble());
        QSet<QPair<QString, bool>> seen;
        for (const QJsonValue& value : sample.value(QStringLiteral("phases")).toArray()) {
            const QJsonObject phase = value.toObject();
            const auto key = qMakePair(phase.value(QStringLiteral("name")).toString(),
                                       phase.value(QStringLiteral("worker")).toBool());
            if (key.first.isEmpty() || seen.contains(key))
                return {};
            seen.insert(key);
            for (const QString& field : {QStringLiteral("totalMs"), QStringLiteral("startMs"),
                                         QStringLiteral("maxMs"), QStringLiteral("calls")}) {
                const QJsonValue measurement = phase.value(field);
                if (!measurement.isDouble() || !std::isfinite(measurement.toDouble())
                    || measurement.toDouble() < 0)
                    return {};
            }
            auto index = indices.constFind(key);
            if (index == indices.cend()) {
                indices.insert(key, phases.size());
                phases.append(PhaseSamples {key.first, key.second, QVector<double>(samples.size(), 0)});
                index = indices.constFind(key);
            }
            auto& collected = phases[index.value()];
            collected.totals[run] += phase.value(QStringLiteral("totalMs")).toDouble();
            collected.starts += phase.value(QStringLiteral("startMs")).toDouble();
            collected.calls += phase.value(QStringLiteral("calls")).toDouble();
            collected.maxCall = qMax(collected.maxCall, phase.value(QStringLiteral("maxMs")).toDouble());
            ++collected.present;
        }
    }
    QJsonArray averagedPhases;
    for (const PhaseSamples& phase : phases) {
        const QJsonObject stats = statistics(phase.totals);
        averagedPhases.append(QJsonObject {
            {QStringLiteral("name"), phase.name}, {QStringLiteral("worker"), phase.worker},
            {QStringLiteral("startMs"), phase.starts / phase.present},
            {QStringLiteral("totalMs"), stats.value(QStringLiteral("mean"))},
            {QStringLiteral("maxMs"), phase.maxCall},
            {QStringLiteral("calls"), phase.calls / samples.size()},
            {QStringLiteral("runsPresent"), phase.present},
            {QStringLiteral("statistics"), stats}
        });
    }
    const QJsonObject elapsedStats = statistics(elapsed);
    const QJsonObject paintStats = statistics(firstPaint);
    // Geometry and the screenshot describe the last run. Phase totals are means.
    QJsonObject result = samples.last().toObject();
    result.remove(QStringLiteral("run"));
    result.remove(QStringLiteral("report"));
    result.insert(QStringLiteral("runs"), samples.size());
    result.insert(QStringLiteral("aggregation"), QStringLiteral("arithmetic mean"));
    result.insert(QStringLiteral("representativeRun"), samples.size());
    result.insert(QStringLiteral("elapsedMs"), elapsedStats.value(QStringLiteral("mean")));
    result.insert(QStringLiteral("firstPaintMs"), paintStats.value(QStringLiteral("mean")));
    result.insert(QStringLiteral("statistics"), QJsonObject {
        {QStringLiteral("elapsedMs"), elapsedStats}, {QStringLiteral("firstPaintMs"), paintStats}
    });
    result.insert(QStringLiteral("phases"), averagedPhases);
    result.insert(QStringLiteral("samples"), samples);
    return result;
}

} // namespace StartupLoadStatistics

#endif
