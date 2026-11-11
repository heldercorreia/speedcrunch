// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GUI_KEYBINDINGS_H
#define GUI_KEYBINDINGS_H

#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QStringList>

class QKeyEvent;

namespace KeyBindings {

using Overrides = QMap<QString, QStringList>;

struct Action {
    QString id;
    QString category;
    QString label;
    QList<QKeySequence> defaults;
};

QList<Action> editorActions();
bool isConfigurable(const QString& id);
QList<QKeySequence> shortcuts(const Action& action, const Overrides& overrides);
QStringList portableText(const QList<QKeySequence>& shortcuts);
QString nativeText(const QList<QKeySequence>& shortcuts);
QString plainText(const QString& menuText);
bool conflicts(const QKeySequence& first, const QKeySequence& second);
bool startsWith(const QKeySequence& sequence, const QKeyEvent* event);
bool matchesSingleKey(const QList<QKeySequence>& shortcuts, const QKeyEvent* event);
bool suppressStandardKey(const Overrides& overrides, const QKeyEvent* event);
bool isValid(const QKeySequence& sequence);

} // namespace KeyBindings

#endif
