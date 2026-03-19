// This file is part of the SpeedCrunch project
// Copyright (C) 2026 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef GUI_FUNCTIONTOOLTIPUTILS_H
#define GUI_FUNCTIONTOOLTIPUTILS_H

#include <QString>

class Evaluator;

namespace FunctionTooltipUtils {

QString activeFunctionUsageTooltip(const Evaluator* evaluator,
                                   const QString& expression,
                                   int cursorPosition);

}

#endif
