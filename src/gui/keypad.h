// This file is part of the SpeedCrunch project
// Copyright (C) 2005-2006 Johan Thelin <e8johan@gmail.com>
// Copyright (C) 2007-2009, 2014 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; see the file COPYING.  If not, write to
// the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
// Boston, MA 02110-1301, USA.

#ifndef GUI_KEYPAD_H
#define GUI_KEYPAD_H

#include <QHash>
#include <QList>
#include <QWidget>

class QPushButton;

class Keypad : public QWidget {
    Q_OBJECT

public:
    enum LayoutMode {
        LayoutModeScientificWide = 0,
        LayoutModeBasicWide = 1,
        LayoutModeScientificNarrow = 2
    };

    enum Button {
        Key0, Key1, Key2, Key3, Key4, Key5, Key6, Key7, Key8, Key9,
        KeyEquals, KeyPlus, KeyMinus, KeyTimes, KeyDivide,
        KeyRadixChar, KeyClear, KeyEE, KeyLeftPar, KeyRightPar,
        KeyRaise, KeySqrt, KeyCbrt, KeyLg, KeyMod, KeyBackspace, KeyPercent, KeyFactorial, KeyPi, KeyAns,
        KeyX, KeyXEquals, KeyExp, KeyLn, KeySin, KeyAsin, KeyCos,
        KeyAcos, KeyTan, KeyAtan
    };

    struct CustomButtonDescription {
        QString label;
        QString text;
        int action;
        int row;
        int column;
    };

    explicit Keypad(LayoutMode layoutMode = LayoutModeScientificWide, QWidget* parent = 0);
    explicit Keypad(const QList<CustomButtonDescription>& customButtons, QWidget* parent = 0);

signals:
    void buttonPressed(Keypad::Button) const;
    void customButtonPressed(int action, const QString& text) const;

public slots:
    void handleRadixCharacterChange();
    void retranslateText();

protected:
    virtual void changeEvent(QEvent*);

private:
    Q_DISABLE_COPY(Keypad)

    QPushButton* key(Button button) const;
    void createButtons();
    void disableButtonFocus();
    void layoutButtons();
    void createCustomButtons();
    void layoutCustomButtons();
    void setButtonTooltips();
    void sizeButtons();
    void sizeCustomButtons();

    static const struct KeyDescription {
        QString label;
        Button button;
        bool boldFont;
        int gridRow;
        int gridColumn;
    } keyDescriptions[];

    LayoutMode m_layoutMode;
    bool m_isCustom;
    QHash<Button, QPair<QPushButton*, const KeyDescription*> > keys;
    QList<CustomButtonDescription> m_customButtons;
    QList<QPushButton*> m_customWidgets;
};

#endif
