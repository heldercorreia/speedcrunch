// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_NUMERICALINTEGRATION_H
#define CORE_NUMERICALINTEGRATION_H

#include "math/hmath.h"

#include <functional>

namespace NumericalIntegration {

struct Result {
    enum Status { Converged, InvalidSample, LimitExceeded };
    Status status;
    HNumber value;
    HNumber error;
};

Result integrate(const HNumber& lower, const HNumber& upper, const HNumber& tolerance,
                 int initialPanels, const std::function<HNumber(const HNumber&)>& sample);

}

#endif
