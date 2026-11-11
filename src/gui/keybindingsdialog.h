// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GUI_KEYBINDINGSDIALOG_H
#define GUI_KEYBINDINGSDIALOG_H

#include "gui/keybindings.h"

#include <QDialog>

class QAction;
class QDialogButtonBox;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class KeyBindingsDialog : public QDialog {
    Q_OBJECT

public:
    KeyBindingsDialog(const QList<KeyBindings::Action>& actions,
                      const KeyBindings::Overrides& overrides, QWidget* parent = nullptr);
    ~KeyBindingsDialog() override;
    KeyBindings::Overrides overrides() const { return m_overrides; }
    void reject() override;

signals:
    void shortcutsApplied(const KeyBindings::Overrides& overrides);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct ShortcutEditor {
        QKeySequenceEdit* input;
        QAction* disable;
        QAction* restore;
        QKeySequence original;
    };

    QList<KeyBindings::Action> m_actions;
    KeyBindings::Overrides m_overrides;
    KeyBindings::Overrides m_appliedOverrides;
    QMap<QString, QTreeWidgetItem*> m_items;
    QMap<QString, ShortcutEditor> m_editors;
    QLineEdit* m_search;
    QTreeWidget* m_tree;
    QLabel* m_hint;
    QPushButton* m_restoreAll;
    QDialogButtonBox* m_buttons;
    bool m_committing = false;
    bool m_closing = false;

    const KeyBindings::Action* definition(const QString& id) const;
    void focusEditor(const QString& id);
    void updateRows(bool discardEdits = false);
    void updatePlaceholder(const QString& id);
    void updateButtons();
    bool hasPendingShortcuts() const;
    void filterActions();
    bool assignShortcut(const QString& id, const QList<QKeySequence>& shortcuts);
    bool commitShortcut(const QString& id);
    bool commitPendingShortcuts();
    bool apply();
};

#endif
