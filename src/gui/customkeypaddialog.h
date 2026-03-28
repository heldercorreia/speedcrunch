// This file is part of the SpeedCrunch project
// Copyright (C) 2026 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef GUI_CUSTOMKEYPADDIALOG_H
#define GUI_CUSTOMKEYPADDIALOG_H

#include "core/settings.h"

#include <QDialog>
#include <QList>

class QSpinBox;
class QTableWidget;

class CustomKeypadDialog : public QDialog {
    Q_OBJECT

public:
    explicit CustomKeypadDialog(const Settings::CustomKeypad& keypad, QWidget* parent = nullptr);

    Settings::CustomKeypad customKeypad() const;

private:
    enum PresetLayout {
        PresetLayoutBasicWide = 0,
        PresetLayoutScientificWide = 1,
        PresetLayoutScientificNarrow = 2
    };

    struct Cell {
        QString label;
        QString text;
        Settings::CustomKeypadButtonAction action;
    };

    int cellIndex(int row, int column) const;
    void buildTableFromCells();
    void rebuildCellStorage(int rows, int columns);
    void applyPresetLayout(PresetLayout presetLayout);
    QList<Cell> readCellsFromTable() const;

    QSpinBox* m_rowsSpin;
    QSpinBox* m_columnsSpin;
    QTableWidget* m_table;
    QList<Cell> m_cells;
    int m_currentRows;
    int m_currentColumns;
};

#endif
