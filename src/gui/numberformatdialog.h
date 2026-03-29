// This file is part of the SpeedCrunch project
// Copyright (C) 2026 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef GUI_NUMBERFORMATDIALOG_H
#define GUI_NUMBERFORMATDIALOG_H

#include "core/settings.h"

#include <QDialog>
#include <QVector>

class QComboBox;

class NumberFormatDialog : public QDialog {
    Q_OBJECT

public:
    explicit NumberFormatDialog(QWidget* parent = nullptr);

    void setSelection(Settings::NumberFormatStyle style);
    Settings::NumberFormatStyle selectedStyle() const;

signals:
    void selectionChanged(Settings::NumberFormatStyle style);

private:
    QComboBox* m_styles;
    QVector<Settings::NumberFormatStyle> m_styleItems;
};

#endif // GUI_NUMBERFORMATDIALOG_H
