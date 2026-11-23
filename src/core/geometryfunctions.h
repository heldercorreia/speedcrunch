// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_GEOMETRYFUNCTIONS_H
#define CORE_GEOMETRYFUNCTIONS_H

#include "core/functions.h"

namespace GeometryFunctions {

void registerFunctions(FunctionRepo& repo);
void retranslateFunctions(FunctionRepo& repo);

Quantity areaPolygon(Function*, const Function::ArgumentList& args);
Quantity surfaceAreaTetrahedron(Function*, const Function::ArgumentList& args);
Quantity volumeTetrahedron(Function*, const Function::ArgumentList& args);
Quantity surfaceAreaParallelepiped(Function*, const Function::ArgumentList& args);
Quantity volumeParallelepiped(Function*, const Function::ArgumentList& args);
Quantity surfaceAreaMesh(Function*, const Function::ArgumentList& args);
Quantity volumeMesh(Function*, const Function::ArgumentList& args);

}

#endif
