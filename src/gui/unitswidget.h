// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_UNITSWIDGET_H
#define GUI_UNITSWIDGET_H

#include <QString>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QEvent;
class QLabel;
class QLineEdit;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

// Lists the built-in units with their symbols and aliases, by quantity.
// It can also list the prefixed units, such as kilometre.
class UnitsWidget : public QWidget {
    Q_OBJECT

public:
    struct InitialState {
        // The English name of the quantity, such as "Length". Empty for all.
        QString quantity;
        QString searchText;
        bool showsPrefixedUnits; // Value-initialized to false by {}.
    };

    explicit UnitsWidget(QWidget* parent = nullptr, const InitialState& state = {});
    ~UnitsWidget();

    const QTreeWidgetItem* currentItem() const;
    QString searchText() const;
    void setSearchText(const QString& text);
    // The English name of the selected quantity. Empty when all are shown.
    QString selectedQuantity() const;
    void setSelectedQuantity(const QString& quantity);
    // Whether the list also has prefixed units, such as kilometre.
    bool showsPrefixedUnits() const;
    void setShowsPrefixedUnits(bool show);
    int missingColumnsWidth();

signals:
    // Sends the unit text to insert, such as "[m]".
    void unitSelected(const QString&);

protected:
    void changeEvent(QEvent*) override;

private slots:
    void handleItemActivated(QTreeWidgetItem*, int);
    void triggerFilter();
    void updateList();

private:
    Q_DISABLE_COPY(UnitsWidget)

    void activateCurrentItem();
    void retranslateText();
    void updateControlLabelWidths();

    QTimer* m_filterTimer;
    QComboBox* m_quantity;
    QLabel* m_quantityLabel;
    QTreeWidget* m_units;
    QLabel* m_noMatchLabel;
    QLineEdit* m_searchFilter;
    QLabel* m_searchLabel;
    QCheckBox* m_prefixedUnits;
    QHBoxLayout* m_prefixedUnitsLayout;
    // Kept while the combo box is rebuilt for another language.
    QString m_selectedQuantityKey;
    // The quantity, prefix choice and search text of the rows shown now.
    QString m_populatedFilter;
    bool m_populated = false;
    bool m_pendingRefresh = false;
};

#endif // GUI_UNITSWIDGET_H
