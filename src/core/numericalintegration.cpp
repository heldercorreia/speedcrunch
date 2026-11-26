// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/numericalintegration.h"
#include "math/floatnum/floatconfig.h"

#include <QVector>

#include <optional>

namespace {

constexpr int MaxEvaluations = 12000;
constexpr int MaxDepth = 18;

struct Node {
    HNumber point;
    HNumber weight;
};

struct Legendre {
    HNumber value;
    HNumber slope;
};

Legendre legendre(int order, const HNumber& x)
{
    HNumber previous = 1;
    HNumber current = x;
    for (int n = 2; n <= order; ++n) {
        const HNumber next = (HNumber(2 * n - 1) * x * current - HNumber(n - 1) * previous) / n;
        previous = current;
        current = next;
    }
    return {current, HNumber(order) * (previous - x * current) / (HNumber(1) - x * x)};
}

QVector<Node> gaussLegendre(int order)
{
    const HNumber rootTolerance = HMath::raise(HNumber(10), 6 - DECPRECISION);
    QVector<Node> nodes;
    for (int i = 1; i <= order / 2; ++i) {
        HNumber x = HMath::cos(HMath::pi() * HNumber(4 * i - 1) / (4 * order + 2));
        bool converged = false;
        for (int step = 0; step < 24; ++step) {
            const Legendre polynomial = legendre(order, x);
            const HNumber correction = polynomial.value / polynomial.slope;
            if (correction.isNan())
                return {};
            x -= correction;
            if (HMath::abs(correction) <= rootTolerance) {
                converged = true;
                break;
            }
        }
        if (!converged)
            return {};
        const HNumber slope = legendre(order, x).slope;
        nodes.append({x, HNumber(2) / ((HNumber(1) - x * x) * slope * slope)});
    }
    return nodes;
}

struct Estimate {
    HNumber value = 0;
    HNumber magnitude = 0;
};

struct Panel {
    HNumber lower;
    HNumber upper;
    Estimate estimate;
    int depth;
    // Only the first panels have a quick error estimate from the coarse rule.
    std::optional<HNumber> error;
};

}

NumericalIntegration::Result NumericalIntegration::integrate(
    const HNumber& lower, const HNumber& upper, const HNumber& tolerance, int initialPanels,
    const std::function<HNumber(const HNumber&)>& sample)
{
    static const QVector<Node> coarse = gaussLegendre(16);
    static const QVector<Node> fine = gaussLegendre(32);
    if (coarse.isEmpty() || fine.isEmpty())
        return {Result::LimitExceeded, 0, 0};

    int evaluations = 0;
    auto estimate = [&](const HNumber& left, const HNumber& right, const QVector<Node>& nodes) {
        Estimate result;
        const HNumber halfWidth = (right - left) / 2;
        const HNumber middle = left + halfWidth;
        if (evaluations + 2 * nodes.size() > MaxEvaluations)
            return Estimate{HMath::nan(TooExpensive), 0};
        for (const Node& node : nodes) {
            const HNumber offset = halfWidth * node.point;
            const HNumber a = sample(middle - offset);
            const HNumber b = sample(middle + offset);
            evaluations += 2;
            if (a.isNan() || b.isNan())
                return Estimate{a.isNan() ? a : b, 0};
            result.value += node.weight * (a + b);
            result.magnitude += node.weight * (HMath::abs(a) + HMath::abs(b));
        }
        return Estimate{result.value * halfWidth, result.magnitude * halfWidth};
    };
    const auto failure = [](const HNumber& value) {
        return Result{value.error() == TooExpensive ? Result::LimitExceeded : Result::InvalidSample, 0, 0};
    };

    const HNumber width = upper - lower;
    const HNumber panelWidth = width / initialPanels;
    QVector<Panel> pending;
    HNumber magnitude = 0;
    HNumber left = lower;
    for (int i = 0; i < initialPanels; ++i) {
        const HNumber right = i + 1 == initialPanels ? upper : lower + HNumber(i + 1) * panelWidth;
        Panel panel{left, right, estimate(left, right, fine), 0, std::nullopt};
        if (!panel.estimate.value.isNan())
            panel.error = HNumber(4) * HMath::abs(panel.estimate.value - estimate(left, right, coarse).value);
        magnitude += panel.estimate.magnitude;
        pending.append(panel);
        left = right;
    }

    if (magnitude.isZero()) {
        for (const Panel& current : pending) {
            if (current.estimate.value.isNan() && current.estimate.value.error() != TooExpensive)
                return {Result::InvalidSample, 0, 0};
        }
        return {Result::LimitExceeded, 0, 0};
    }
    const HNumber errorBudget = tolerance * magnitude;
    HNumber total = 0;
    HNumber totalError = 0;
    auto accept = [&](const Panel& panel, const HNumber& value, const HNumber& error) {
        if (error > errorBudget * (panel.upper - panel.lower) / width)
            return false;
        total += value;
        totalError += error;
        return true;
    };
    while (!pending.isEmpty()) {
        const Panel current = pending.takeLast();
        if (current.estimate.value.isNan())
            return failure(current.estimate.value);
        if (current.error) {
            if (current.error->isNan())
                return failure(*current.error);
            if (accept(current, current.estimate.value, *current.error))
                continue;
        }

        const HNumber middle = current.lower + (current.upper - current.lower) / 2;
        if (current.depth >= MaxDepth || middle == current.lower || middle == current.upper)
            return {Result::LimitExceeded, 0, 0};
        const Panel first{current.lower, middle, estimate(current.lower, middle, fine), current.depth + 1, std::nullopt};
        if (first.estimate.value.isNan())
            return failure(first.estimate.value);
        const Panel second{middle, current.upper, estimate(middle, current.upper, fine), current.depth + 1, std::nullopt};
        if (second.estimate.value.isNan())
            return failure(second.estimate.value);

        // The coarse rule makes the error look much larger than it is. The halves
        // are far more accurate than the whole panel, so the difference between
        // the two is a closer bound for the error of the halves.
        const HNumber refined = first.estimate.value + second.estimate.value;
        const HNumber error = HMath::abs(refined - current.estimate.value);
        if (error.isNan())
            return failure(error);
        if (accept(current, refined, error))
            continue;
        pending.append(first);
        pending.append(second);
    }

    if (total.isNan() || totalError.isNan() || totalError > errorBudget)
        return {Result::LimitExceeded, 0, 0};
    return {Result::Converged, total, totalError};
}
