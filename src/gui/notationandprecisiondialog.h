// This file is part of the SpeedCrunch project
// Copyright (C) 2026 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef GUI_RESULTSLOTSDIALOG_H
#define GUI_RESULTSLOTSDIALOG_H

#include <QDialog>
#include <array>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QSpinBox;

class ResultSlotsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ResultSlotsDialog(QWidget* parent = nullptr);

signals:
    void settingsApplied();

private:
    struct SlotSettings {
        char notation = '\0';
        int precision = -1;
        bool enabled = true;
        bool complexEnabled = false;
        char complexForm = 'c';
    };

    int currentSlotIndex() const;
    void loadFromSettings();
    void saveCurrentUiToSlot(int slotIndex);
    void loadSlotToUi(int slotIndex);
    void applyToSettings();
    void updateAdvancedModeUi(bool advanced);
    void updatePrecisionLabel();

    QComboBox* m_slot;
    QComboBox* m_notation;
    QCheckBox* m_enabled;
    QCheckBox* m_autoPrecision;
    QSpinBox* m_precision;
    QLabel* m_precisionLabel;
    QComboBox* m_complex;
    QGroupBox* m_selectorGroup;
    QGroupBox* m_settingsGroup;
    QCheckBox* m_advancedMode;
    std::array<SlotSettings, 5> m_slots;
    int m_activeSlotIndex = 0;
};

#endif // GUI_RESULTSLOTSDIALOG_H
