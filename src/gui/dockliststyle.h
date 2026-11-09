// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GUI_DOCKLISTSTYLE_H
#define GUI_DOCKLISTSTYLE_H

#include <functional>

class QAbstractItemView;
class QDockWidget;
class QLabel;
class QMainWindow;
class QWidget;

namespace DockListStyle {

void apply(QAbstractItemView* view);
void showCenteredNoMatchLabel(QAbstractItemView* view, QLabel* label);
bool dockIsOffScreen(const QMainWindow* window, QDockWidget* dock);
bool shouldDeferPopulation(const QWidget* content);
void refreshWhenExposed(QWidget* content, const std::function<void()>& refresh);

} // namespace DockListStyle

#endif // GUI_DOCKLISTSTYLE_H
