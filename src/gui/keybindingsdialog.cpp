// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/keybindingsdialog.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QStyleOptionToolButton>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

class ShortcutOptionsButton : public QToolButton {
public:
    using QToolButton::QToolButton;

protected:
    void initStyleOption(QStyleOptionToolButton* option) const override
    {
        QToolButton::initStyleOption(option);
        // The ellipsis already identifies this as a menu button.
        option->features &= ~QStyleOptionToolButton::HasMenu;
    }
};

} // namespace

KeyBindingsDialog::KeyBindingsDialog(const QList<KeyBindings::Action>& actions,
                                   const KeyBindings::Overrides& overrides, QWidget* parent)
    : QDialog(parent)
    , m_actions(actions)
    , m_overrides(overrides)
    , m_appliedOverrides(overrides)
    , m_search(new QLineEdit(this))
    , m_tree(new QTreeWidget(this))
    , m_hint(new QLabel(this))
    , m_restoreAll(new QPushButton(tr("Restore All Defaults"), this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                    | QDialogButtonBox::Apply, this))
{
    setObjectName(QStringLiteral("KeyBindingsDialog"));
    setWindowTitle(tr("Keyboard Shortcuts"));
    resize(960, 640);
    setMinimumSize(640, 420);

    auto* searchLabel = new QLabel(tr("&Search actions:"), this);
    searchLabel->setBuddy(m_search);
    m_search->setObjectName(QStringLiteral("ShortcutSearch"));
    m_search->setClearButtonEnabled(true);
    m_search->setPlaceholderText(tr("Search by action, category, or shortcut"));
    m_tree->setObjectName(QStringLiteral("ShortcutActions"));
    m_tree->setHeaderLabels({tr("Action"), tr("Default"), tr("Custom")});
    m_tree->headerItem()->setToolTip(2, tr("Click a box and press a shortcut. Use the row menu to disable it or restore its default."));
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_tree->setColumnWidth(1, 210);
    m_tree->setColumnWidth(2, 270);

    QMap<QString, QTreeWidgetItem*> groups;
    QWidget* previous = m_tree;
    for (const KeyBindings::Action& action : m_actions) {
        QTreeWidgetItem* group = groups.value(action.category);
        if (!group) {
            group = new QTreeWidgetItem(m_tree, {action.category});
            group->setFlags(group->flags() & ~Qt::ItemIsSelectable);
            group->setFirstColumnSpanned(true);
            group->setExpanded(true);
            groups.insert(action.category, group);
        }
        auto* item = new QTreeWidgetItem(group, {action.label});
        item->setData(0, Qt::UserRole, action.id);
        item->setToolTip(0, action.category + QStringLiteral(" / ") + action.label);
        m_items.insert(action.id, item);

        auto* cell = new QWidget(m_tree);
        cell->setAutoFillBackground(true);
        auto* input = new QKeySequenceEdit(cell);
        input->setObjectName(QStringLiteral("ShortcutSequence.") + action.id);
        input->setProperty("keyBindingId", action.id);
        input->setClearButtonEnabled(true);
        input->setAccessibleName(tr("Custom shortcut for %1").arg(action.label));
        input->installEventFilter(this);
        auto* options = new ShortcutOptionsButton(cell);
        options->setObjectName(QStringLiteral("ShortcutOptions.") + action.id);
        options->setPopupMode(QToolButton::InstantPopup);
        options->setText(QStringLiteral("..."));
        options->setAutoRaise(true);
        options->setToolTip(tr("Shortcut options for %1").arg(action.label));
        options->setAccessibleName(options->toolTip());
        auto* menu = new QMenu(options);
        QAction* disable = menu->addAction(tr("Disable Shortcut"));
        disable->setObjectName(QStringLiteral("DisableShortcut.") + action.id);
        QAction* restore = menu->addAction(tr("Restore Default"));
        restore->setObjectName(QStringLiteral("RestoreShortcutDefault.") + action.id);
        options->setMenu(menu);
        auto* row = new QHBoxLayout(cell);
        row->setContentsMargins(4, 3, 4, 3);
        row->setSpacing(2);
        row->addWidget(input, 1);
        row->addWidget(options);
        m_tree->setItemWidget(item, 2, cell);
        m_editors.insert(action.id, {input, disable, restore, {}});
        setTabOrder(previous, input);
        setTabOrder(input, options);
        previous = options;

        const QString id = action.id;
        connect(input, &QKeySequenceEdit::keySequenceChanged, this, &KeyBindingsDialog::updateButtons);
        connect(input, &QKeySequenceEdit::editingFinished, this, [this, id]() {
            // Let a clicked Cancel or Restore button run before finishing this edit.
            QTimer::singleShot(0, this, [this, id]() {
                if (!m_closing)
                    commitShortcut(id);
            });
        });
        connect(menu, &QMenu::aboutToShow, this, &KeyBindingsDialog::updateButtons);
        connect(disable, &QAction::triggered, this, [this, id]() {
            assignShortcut(id, {});
            m_editors[id].original = m_editors[id].input->keySequence();
            updateRows();
        });
        connect(restore, &QAction::triggered, this, [this, id]() {
            const QScopedValueRollback<bool> guard(m_committing, true);
            assignShortcut(id, definition(id)->defaults);
            m_editors[id].original = m_editors[id].input->keySequence();
            updateRows();
        });
    }
    setTabOrder(m_search, m_tree);
    m_restoreAll->setObjectName(QStringLiteral("RestoreAllShortcutDefaults"));
    m_restoreAll->setAutoDefault(false);
    m_hint->setObjectName(QStringLiteral("ShortcutHint"));
    m_hint->setText(tr("Click a Custom box and press your shortcut. "
                       "Use the row menu to disable a shortcut or restore its default. "
                       "Sequences can contain up to four key combinations."));
    m_hint->setWordWrap(true);
    auto* footer = new QHBoxLayout;
    footer->addWidget(m_restoreAll);
    footer->addStretch();
    footer->addWidget(m_buttons);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(searchLabel);
    layout->addWidget(m_search);
    layout->addWidget(m_tree, 1);
    layout->addWidget(m_hint);
    layout->addLayout(footer);

    connect(m_search, &QLineEdit::textChanged, this, &KeyBindingsDialog::filterActions);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        const QString id = item ? item->data(0, Qt::UserRole).toString() : QString();
        // Native item views can restore focus after emitting doubleClicked.
        QTimer::singleShot(0, this, [this, id]() { focusEditor(id); });
    });
    connect(m_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int) {
        focusEditor(item ? item->data(0, Qt::UserRole).toString() : QString());
    });
    connect(m_restoreAll, &QPushButton::clicked, this, [this]() {
        m_overrides.clear();
        updateRows(true);
    });
    connect(m_buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, [this]() { apply(); });
    connect(m_buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (apply()) {
            m_closing = true;
            accept();
        }
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &KeyBindingsDialog::reject);
    updateRows(true);
    m_search->setFocus();
}

KeyBindingsDialog::~KeyBindingsDialog()
{
    // Losing focus during child destruction can finish a recording.
    for (const ShortcutEditor& editor : m_editors) {
        editor.input->removeEventFilter(this);
        disconnect(editor.input, nullptr, this, nullptr);
    }
    disconnect(m_tree, nullptr, this, nullptr);
    disconnect(m_search, nullptr, this, nullptr);
}

void KeyBindingsDialog::reject()
{
    m_closing = true;
    QDialog::reject();
}

bool KeyBindingsDialog::eventFilter(QObject* watched, QEvent* event)
{
    auto* input = qobject_cast<QKeySequenceEdit*>(watched);
    if (input) {
        const QString id = input->property("keyBindingId").toString();
        if (event->type() == QEvent::FocusIn) {
            m_tree->setCurrentItem(m_items.value(id));
            m_tree->scrollToItem(m_items.value(id));
            updatePlaceholder(id);
        } else if (event->type() == QEvent::KeyRelease) {
            QTimer::singleShot(0, this, &KeyBindingsDialog::updateButtons);
        }
    }
    return QDialog::eventFilter(watched, event);
}

const KeyBindings::Action* KeyBindingsDialog::definition(const QString& id) const
{
    for (const KeyBindings::Action& action : m_actions) {
        if (action.id == id)
            return &action;
    }
    return nullptr;
}

void KeyBindingsDialog::focusEditor(const QString& id)
{
    if (!m_editors.contains(id))
        return;
    auto* input = m_editors.value(id).input;
    input->setFocus(Qt::ShortcutFocusReason);
    if (auto* text = input->findChild<QLineEdit*>())
        text->selectAll();
}

void KeyBindingsDialog::updatePlaceholder(const QString& id)
{
    const ShortcutEditor& editor = m_editors[id];
    if (auto* text = editor.input->findChild<QLineEdit*>()) {
        text->setPlaceholderText(editor.input->hasFocus() ? tr("Press shortcut...")
            : m_overrides.contains(id) ? tr("Disabled") : tr("Use default"));
    }
}

void KeyBindingsDialog::updateRows(bool discardEdits)
{
    for (const KeyBindings::Action& action : m_actions) {
        QTreeWidgetItem* item = m_items.value(action.id);
        const QList<QKeySequence> keys = KeyBindings::shortcuts(action, m_overrides);
        const QString current = KeyBindings::nativeText(keys);
        const QString defaults = KeyBindings::nativeText(action.defaults);
        item->setText(1, defaults.isEmpty() ? tr("None") : defaults);
        item->setToolTip(1, item->text(1));
        ShortcutEditor& editor = m_editors[action.id];
        if (discardEdits || editor.input->keySequence() == editor.original) {
            editor.original = m_overrides.contains(action.id) && !keys.isEmpty() ? keys.first() : QKeySequence();
            const QSignalBlocker blocker(editor.input);
            editor.input->setKeySequence(editor.original);
            updatePlaceholder(action.id);
        }
        const bool editorAction = action.id.startsWith(QLatin1String("editor"))
            || action.id.startsWith(QLatin1String("resultsScroll"))
            || action.id.startsWith(QLatin1String("settingsDisplayZoom"));
        QString hint = m_overrides.contains(action.id)
            ? (current.isEmpty() ? tr("This shortcut is disabled.") : tr("Custom shortcut: %1.").arg(current))
            : tr("Uses the default shortcut: %1.").arg(item->text(1));
        hint += QLatin1Char(' ') + (editorAction ? tr("Works in the expression editor.")
            : tr("Works in the active SpeedCrunch window."));
        if (keys.size() > 1)
            hint += QLatin1Char(' ') + tr("Recording a shortcut replaces the existing alternatives.");
        hint += QLatin1Char(' ') + tr("System shortcuts may be intercepted before they reach SpeedCrunch.");
        editor.input->setAccessibleDescription(hint);
    }
    filterActions();
    updateButtons();
}

bool KeyBindingsDialog::hasPendingShortcuts() const
{
    for (const ShortcutEditor& editor : m_editors) {
        if (editor.input->keySequence() != editor.original)
            return true;
    }
    return false;
}

void KeyBindingsDialog::updateButtons()
{
    for (const KeyBindings::Action& action : m_actions) {
        const ShortcutEditor& editor = m_editors[action.id];
        const bool pending = editor.input->keySequence() != editor.original;
        editor.disable->setEnabled(pending || !KeyBindings::shortcuts(action, m_overrides).isEmpty());
        editor.restore->setEnabled(pending || m_overrides.contains(action.id));
    }
    const bool pending = hasPendingShortcuts();
    m_restoreAll->setEnabled(pending || !m_overrides.isEmpty());
    m_buttons->button(QDialogButtonBox::Apply)->setEnabled(pending || m_overrides != m_appliedOverrides);
}

void KeyBindingsDialog::filterActions()
{
    const QString query = m_search->text().trimmed();
    for (int index = 0; index < m_tree->topLevelItemCount(); ++index) {
        QTreeWidgetItem* group = m_tree->topLevelItem(index);
        bool visible = false;
        for (int child = 0; child < group->childCount(); ++child) {
            QTreeWidgetItem* item = group->child(child);
            const QString searchable = group->text(0) + QLatin1Char(' ') + item->text(0)
                + QLatin1Char(' ') + item->text(1)
                + QLatin1Char(' ') + KeyBindings::portableText(KeyBindings::shortcuts(
                    *definition(item->data(0, Qt::UserRole).toString()), m_overrides)).join(QLatin1Char(' '));
            const bool matches = searchable.contains(query, Qt::CaseInsensitive);
            item->setHidden(!matches);
            visible |= matches;
        }
        group->setHidden(!visible);
        if (!query.isEmpty())
            group->setExpanded(true);
    }
}

bool KeyBindingsDialog::assignShortcut(const QString& id, const QList<QKeySequence>& keys)
{
    const KeyBindings::Action* selected = definition(id);
    if (!selected)
        return true;
    for (const QKeySequence& sequence : keys) {
        if (!KeyBindings::isValid(sequence)) {
            QMessageBox::warning(this, tr("Invalid Shortcut"), tr("Choose a key combination to assign."));
            return false;
        }
        for (int index = 0; index < sequence.count(); ++index) {
            const QKeyCombination combination = sequence[index];
            if ((combination.key() == Qt::Key_Return || combination.key() == Qt::Key_Enter
                 || combination.key() == Qt::Key_Escape || combination.key() == Qt::Key_Tab
                 || combination.key() == Qt::Key_Backtab)
                && (combination.keyboardModifiers() == Qt::NoModifier
                    || combination.keyboardModifiers() == Qt::ShiftModifier)) {
                QMessageBox::warning(this, tr("Reserved Key"),
                    tr("Enter and Return evaluate expressions. Escape closes popups, and Tab moves focus. "
                       "Choose another shortcut."));
                return false;
            }
        }
    }
    QMap<QString, QList<QKeySequence>> replacements;
    QStringList conflicts;
    for (const KeyBindings::Action& action : m_actions) {
        if (action.id == id)
            continue;
        const QList<QKeySequence> current = KeyBindings::shortcuts(action, m_overrides);
        QList<QKeySequence> remaining;
        for (const QKeySequence& existing : current) {
            bool conflict = false;
            for (const QKeySequence& candidate : keys)
                conflict |= KeyBindings::conflicts(candidate, existing);
            if (!conflict)
                remaining.append(existing);
        }
        if (remaining != current) {
            replacements.insert(action.id, remaining);
            conflicts.append(action.category + QStringLiteral(" / ") + action.label);
        }
    }
    if (!conflicts.isEmpty()) {
        QMessageBox confirmation(QMessageBox::Question, tr("Shortcut Already Assigned"),
            tr("This shortcut conflicts with:\n%1").arg(conflicts.join(QLatin1Char('\n'))),
            QMessageBox::Cancel, this);
        confirmation.setInformativeText(tr("Reassign it and remove the conflicting bindings? "
            "A shorter sequence also conflicts with sequences that begin with the same keys."));
        QPushButton* reassign = confirmation.addButton(tr("Reassign"), QMessageBox::AcceptRole);
        confirmation.setDefaultButton(QMessageBox::Cancel);
        confirmation.setEscapeButton(QMessageBox::Cancel);
        confirmation.exec();
        if (confirmation.clickedButton() != reassign)
            return false;
    }
    replacements.insert(id, keys);
    for (auto replacement = replacements.cbegin(); replacement != replacements.cend(); ++replacement) {
        const KeyBindings::Action* action = definition(replacement.key());
        if (replacement.value() == action->defaults)
            m_overrides.remove(action->id);
        else
            m_overrides.insert(action->id, KeyBindings::portableText(replacement.value()));
    }
    return true;
}

bool KeyBindingsDialog::commitShortcut(const QString& id)
{
    if (m_committing || m_closing)
        return true;
    ShortcutEditor& editor = m_editors[id];
    const QKeySequence sequence = editor.input->keySequence();
    if (sequence == editor.original) {
        updatePlaceholder(id);
        return true;
    }
    const QScopedValueRollback<bool> guard(m_committing, true);
    const bool assigned = assignShortcut(id, sequence.isEmpty() ? QList<QKeySequence>() : QList<QKeySequence>{sequence});
    editor.original = editor.input->keySequence();
    updateRows();
    return assigned;
}

bool KeyBindingsDialog::commitPendingShortcuts()
{
    for (const QString& id : m_editors.keys()) {
        if (!commitShortcut(id))
            return false;
    }
    return true;
}

bool KeyBindingsDialog::apply()
{
    if (!commitPendingShortcuts())
        return false;
    if (m_overrides != m_appliedOverrides) {
        emit shortcutsApplied(m_overrides);
        m_appliedOverrides = m_overrides;
    }
    updateButtons();
    return true;
}
