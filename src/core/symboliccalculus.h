// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_SYMBOLICCALCULUS_H
#define CORE_SYMBOLICCALCULUS_H

#include "core/opcode.h"
#include "math/quantity.h"

#include <functional>
#include <optional>

class Evaluator;

namespace SymbolicCalculus {

inline constexpr int MaxSourceLength = 16384;
using EvaluateSpan = std::function<Quantity(int first, int count, const QString& variable,
                                           const Quantity& value)>;
using UnitLookup = std::function<bool(const QString&, Quantity*)>;
using VariableLookup = std::function<std::optional<Quantity>(const QString&)>;

bool isNamedInfinity(const QString& name);

Quantity evaluate(const Evaluator&, const QVector<Opcode>&, int first,
                  const QVector<Quantity>&, const QStringList& identifiers,
                  const EvaluateSpan&, const UnitLookup&, const VariableLookup&, QString* error);

}

#endif
