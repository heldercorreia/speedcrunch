// This file is part of the SpeedCrunch project
// Copyright (C) 2004 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2005-2006 Johan Thelin <e8johan@gmail.com>
// Copyright (C) 2007-2016 @heldercorreia
// Copyright (C) 2015 Pol Welter <polwelter@gmail.com>
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

#ifndef CORE_SETTINGS_H
#define CORE_SETTINGS_H

#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtCore/QStringList>
#include <QtCore/QList>

class Settings {
public:
    enum UpDownArrowBehavior {
        UpDownArrowBehaviorNever = 0,
        UpDownArrowBehaviorAlways = 1,
        UpDownArrowBehaviorSingleLineOnly = 2
    };

    enum HistorySaving {
        HistorySavingNever = 0,
        HistorySavingOnExit = 1,
        HistorySavingContinuously = 2
    };

    enum KeypadMode {
        KeypadModeDisabled = 0,
        KeypadModeBasicWide = 1,
        KeypadModeBasicNarrow = 2,
        KeypadModeScientificWide = 3,
        KeypadModeScientificNarrow = 4,
        KeypadModeCustom = 5
    };

    enum CustomKeypadButtonAction {
        CustomKeypadActionInsertText = 0,
        CustomKeypadActionBackspace = 1,
        CustomKeypadActionClearExpression = 2,
        CustomKeypadActionEvaluateExpression = 3
    };

    struct CustomKeypadButton {
        int row;
        int column;
        QString label;
        QString text;
        CustomKeypadButtonAction action;
    };

    struct CustomKeypad {
        int rows;
        int columns;
        QList<CustomKeypadButton> buttons;
    };

    static Settings* instance();
    static QString getConfigPath();
    static QString getDataPath();
    static QString getCachePath();
    static CustomKeypad defaultCustomKeypad();

    void load();
    void save();

    char radixCharacter() const; // 0 or '*': Automatic.
    void setRadixCharacter(char c = 0);
    bool isRadixCharacterAuto() const;
    bool isRadixCharacterBoth() const;

    bool complexNumbers;

    char angleUnit; // 'r': radian; 'd': degree; 'g': gradian; 't': turn.

    // 'g': general; 'f': fixed; 'n': engineering; 'e': scientific; 'r': rational;
    // 'b': binary; 'o': octal; 'h': hexadecimal; 's': sexagesimal.
    char resultFormat;
    char alternativeResultFormat; // '\0': disabled.
    char tertiaryResultFormat; // '\0': disabled.
    int resultPrecision; // See HMath documentation.
    char resultFormatComplex; // 'c' cartesian; 'p' polar.

    bool autoAns;
    bool autoCalc;
    bool autoCompletion;
    UpDownArrowBehavior upDownArrowBehavior;
    int digitGrouping;
    bool digitGroupingIntegerPartOnly;
    int maxHistoryEntries; // 0: unlimited.
    HistorySaving historySaving;
    bool leaveLastExpression;
    bool showEmptyHistoryHint;
    bool syntaxHighlighting;
    bool hoverHighlightResults;
    bool windowAlwaysOnTop;
    bool autoResultToClipboard;
    bool simplifyResultExpressions;
    bool windowPositionSave;
    bool singleInstance;
    QString startupUserDefinitions;
    bool startupUserDefinitionsOverwrite;
    bool startupUserDefinitionsApplyBeforeRestore;

    bool constantsDockVisible;
    bool functionsDockVisible;
    bool historyDockVisible;
    bool keypadVisible;
    KeypadMode keypadMode;
    CustomKeypad customKeypad;
    bool formulaBookDockVisible;
    bool statusBarVisible;
    bool menuBarVisible;
    bool variablesDockVisible;
    bool userFunctionsDockVisible;
    bool windowOnfullScreen;
    bool bitfieldVisible;

    QString colorScheme;
    QString displayFont;

    QString language;

    QByteArray windowState;
    QByteArray windowGeometry;
    QByteArray manualWindowGeometry;

private:
    Settings();
    Q_DISABLE_COPY(Settings)
};

#endif
