// This file is part of the SpeedCrunch project
// Copyright (C) 2004, 2007 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2005, 2006 Johan Thelin <e8johan@gmail.com>
// Copyright (C) 2007-2016 @heldercorreia
// Copyright (C) 2011 Daniel Schäufele <git@schaeufele.org>
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

#include "gui/mainwindow.h"

#include "core/constants.h"
#include "core/evaluator.h"
#include "core/functions.h"
#include "core/numberformatter.h"
#include "core/settings.h"
#include "core/session.h"
#include "core/variable.h"
#include "core/sessionhistory.h"
#include "core/userfunction.h"
#include "gui/aboutbox.h"
#include "gui/bitfieldwidget.h"
#include "gui/bookdock.h"
#include "gui/genericdock.h"
#include "gui/constantswidget.h"
#include "gui/editorutils.h"
#include "gui/functionswidget.h"
#include "gui/historywidget.h"
#include "gui/userfunctionlistwidget.h"
#include "gui/variablelistwidget.h"
#include "gui/editor.h"
#include "gui/historywidget.h"
#include "gui/manualwindow.h"
#include "core/manualserver.h"
#include "gui/resultdisplay.h"
#include "gui/syntaxhighlighter.h"
#include "math/floatconfig.h"

#include <QLatin1String>
#include <QLocale>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>
#include <QUrl>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFont>
#include <QFontDialog>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QScrollBar>
#include <QStatusBar>
#include <QToolTip>
#include <QVBoxLayout>
#include <QJsonDocument>

#ifdef Q_OS_WIN32
#include "windows.h"
#include <shlobj.h>
#endif // Q_OS_WIN32

QTranslator* MainWindow::createTranslator(const QString& langCode)
{
    QTranslator* translator = new QTranslator;
    QLocale locale(langCode == "C" ? QLocale().name() : langCode);

    if(!translator->load(locale, QString(":/locale/"))) {
        // There are regional Portuguese translations only for Brazil and Portugal.
        // Unsupported Portuguese variants (e.g. pt_AO) should fall back to pt_PT.
        if (locale.language() == QLocale::Portuguese) {
            const QLocale::Territory territory = locale.territory();
            if (territory != QLocale::Brazil && territory != QLocale::Portugal) {
                if (translator->load(QLocale(QLocale::Portuguese, QLocale::Portugal), QString(":/locale/")))
                    return translator;
            }
        }

        // Strip the country and try to find a generic translation for this language
        locale = QLocale(locale.language());
        if (!translator->load(locale, QString(":/locale/"))) {
            // Handle the case where the translation file cannot be loaded
            // For example, log an error, use a default language, etc.
        }
    }

    return translator;
}

void MainWindow::createUi()
{
    createActions();
    createActionGroups();
    createActionShortcuts();
    createMenus();
    createFixedWidgets();
    createFixedConnections();

    setWindowTitle("SpeedCrunch");
    setWindowIcon(QPixmap(":/speedcrunch.png"));

    m_copyWidget = m_widgets.editor;
}

void MainWindow::createActions()
{
    m_actions.sessionExportHtml = new QAction(this);
    m_actions.sessionExportPlainText = new QAction(this);
    m_actions.sessionImport = new QAction(this);
    m_actions.sessionLoad = new QAction(this);
    m_actions.sessionQuit = new QAction(this);
    m_actions.sessionSave = new QAction(this);
    m_actions.editClearExpression = new QAction(this);
    m_actions.editClearHistory = new QAction(this);
    m_actions.editCopyLastResult = new QAction(this);
    m_actions.editCopy = new QAction(this);
    m_actions.editPaste = new QAction(this);
    m_actions.editSelectExpression = new QAction(this);
    m_actions.editWrapSelection = new QAction(this);
    m_actions.viewConstants = new QAction(this);
    m_actions.viewFullScreenMode = new QAction(this);
    m_actions.viewFunctions = new QAction(this);
    m_actions.viewHistory = new QAction(this);
    m_actions.viewKeypad = new QAction(this);
    m_actions.viewFormulaBook = new QAction(this);
    m_actions.viewStatusBar = new QAction(this);
    m_actions.viewMenuBar = new QAction(this);
    m_actions.viewVariables = new QAction(this);
    m_actions.viewBitfield = new QAction(this);
    m_actions.viewUserFunctions = new QAction(this);
    m_actions.settingsAngleUnitDegree = new QAction(this);
    m_actions.settingsAngleUnitRadian = new QAction(this);
    m_actions.settingsAngleUnitGradian = new QAction(this);
    m_actions.settingsAngleUnitCycle = new QAction(this);
    m_actions.settingsBehaviorAlwaysOnTop = new QAction(this);
    m_actions.settingsBehaviorAutoAns = new QAction(this);
    m_actions.settingsBehaviorAutoCompletion = new QAction(this);
    m_actions.settingsBehaviorLeaveLastExpression = new QAction(this);
    m_actions.settingsBehaviorPartialResults = new QAction(this);
    m_actions.settingsBehaviorHistorySavingNever = new QAction(this);
    m_actions.settingsBehaviorHistorySavingOnExit = new QAction(this);
    m_actions.settingsBehaviorHistorySavingContinuously = new QAction(this);
    m_actions.settingsBehaviorSaveWindowPositionOnExit = new QAction(this);
    m_actions.settingsBehaviorSyntaxHighlighting = new QAction(this);
    m_actions.settingsBehaviorHoverHighlightResults = new QAction(this);
    m_actions.settingsBehaviorDigitGroupingNone = new QAction(this);
    m_actions.settingsBehaviorDigitGroupingOneSpace = new QAction(this);
    m_actions.settingsBehaviorDigitGroupingTwoSpaces = new QAction(this);
    m_actions.settingsBehaviorDigitGroupingThreeSpaces = new QAction(this);
    m_actions.settingsBehaviorDigitGroupingIntegerPartOnly = new QAction(this);
    m_actions.settingsBehaviorAutoResultToClipboard = new QAction(this);
    m_actions.settingsBehaviorHistorySizeLimit = new QAction(this);
    m_actions.settingsBehaviorComplexNumbers = new QAction(this);
    m_actions.settingsDisplayFont = new QAction(this);
    m_actions.settingsLanguage = new QAction(this);
    m_actions.settingsRadixCharComma = new QAction(this);
    m_actions.settingsRadixCharDefault = new QAction(this);
    m_actions.settingsRadixCharDot = new QAction(this);
    m_actions.settingsRadixCharBoth = new QAction(this);
    m_actions.settingsResultFormat0Digits = new QAction(this);
    m_actions.settingsResultFormat15Digits = new QAction(this);
    m_actions.settingsResultFormat2Digits = new QAction(this);
    m_actions.settingsResultFormat3Digits = new QAction(this);
    m_actions.settingsResultFormat50Digits = new QAction(this);
    m_actions.settingsResultFormat8Digits = new QAction(this);
    m_actions.settingsResultFormatAutoPrecision = new QAction(this);
    m_actions.settingsResultFormatBinary = new QAction(this);
    m_actions.settingsResultFormatEngineering = new QAction(this);
    m_actions.settingsResultFormatFixed = new QAction(this);
    m_actions.settingsResultFormatGeneral = new QAction(this);
    m_actions.settingsResultFormatHexadecimal = new QAction(this);
    m_actions.settingsResultFormatOctal = new QAction(this);
    m_actions.settingsResultFormatScientific = new QAction(this);
    m_actions.settingsResultFormatCartesian= new QAction(this);
    m_actions.settingsResultFormatPolar = new QAction(this);
    m_actions.settingsResultFormatSexagesimal = new QAction(this);
    m_actions.settingsAlternativeResultFormatDisabled = new QAction(this);
    m_actions.settingsAlternativeResultFormatGeneral = new QAction(this);
    m_actions.settingsAlternativeResultFormatFixed = new QAction(this);
    m_actions.settingsAlternativeResultFormatEngineering = new QAction(this);
    m_actions.settingsAlternativeResultFormatScientific = new QAction(this);
    m_actions.settingsAlternativeResultFormatBinary = new QAction(this);
    m_actions.settingsAlternativeResultFormatOctal = new QAction(this);
    m_actions.settingsAlternativeResultFormatHexadecimal = new QAction(this);
    m_actions.settingsAlternativeResultFormatSexagesimal = new QAction(this);
    m_actions.settingsTertiaryResultFormatDisabled = new QAction(this);
    m_actions.settingsTertiaryResultFormatGeneral = new QAction(this);
    m_actions.settingsTertiaryResultFormatFixed = new QAction(this);
    m_actions.settingsTertiaryResultFormatEngineering = new QAction(this);
    m_actions.settingsTertiaryResultFormatScientific = new QAction(this);
    m_actions.settingsTertiaryResultFormatBinary = new QAction(this);
    m_actions.settingsTertiaryResultFormatOctal = new QAction(this);
    m_actions.settingsTertiaryResultFormatHexadecimal = new QAction(this);
    m_actions.settingsTertiaryResultFormatSexagesimal = new QAction(this);
    m_actions.helpManual = new QAction(this);
    m_actions.helpUpdates = new QAction(this);
    m_actions.helpFeedback = new QAction(this);
    m_actions.helpCommunity = new QAction(this);
    m_actions.helpNews = new QAction(this);
    m_actions.helpDonate = new QAction(this);
    m_actions.helpAbout = new QAction(this);
    m_actions.contextHelp = new QAction(this);

    m_actions.settingsAngleUnitDegree->setCheckable(true);
    m_actions.settingsAngleUnitRadian->setCheckable(true);
    m_actions.settingsAngleUnitGradian->setCheckable(true);
    m_actions.settingsBehaviorAlwaysOnTop->setCheckable(true);
    m_actions.settingsBehaviorAutoAns->setCheckable(true);
    m_actions.settingsBehaviorAutoCompletion->setCheckable(true);
    m_actions.settingsBehaviorLeaveLastExpression->setCheckable(true);
    m_actions.settingsBehaviorPartialResults->setCheckable(true);
    m_actions.settingsBehaviorHistorySavingNever->setCheckable(true);
    m_actions.settingsBehaviorHistorySavingNever->setData(Settings::HistorySavingNever);
    m_actions.settingsBehaviorHistorySavingOnExit->setCheckable(true);
    m_actions.settingsBehaviorHistorySavingOnExit->setData(Settings::HistorySavingOnExit);
    m_actions.settingsBehaviorHistorySavingContinuously->setCheckable(true);
    m_actions.settingsBehaviorHistorySavingContinuously->setData(Settings::HistorySavingContinuously);
    m_actions.settingsBehaviorSaveWindowPositionOnExit->setCheckable(true);
    m_actions.settingsBehaviorSyntaxHighlighting->setCheckable(true);
    m_actions.settingsBehaviorHoverHighlightResults->setCheckable(true);
    m_actions.settingsBehaviorDigitGroupingNone->setCheckable(true);
    m_actions.settingsBehaviorDigitGroupingNone->setData(0);
    m_actions.settingsBehaviorDigitGroupingOneSpace->setCheckable(true);
    m_actions.settingsBehaviorDigitGroupingOneSpace->setData(1);
    m_actions.settingsBehaviorDigitGroupingTwoSpaces->setCheckable(true);
    m_actions.settingsBehaviorDigitGroupingTwoSpaces->setData(2);
    m_actions.settingsBehaviorDigitGroupingThreeSpaces->setCheckable(true);
    m_actions.settingsBehaviorDigitGroupingThreeSpaces->setData(3);
    m_actions.settingsBehaviorDigitGroupingIntegerPartOnly->setCheckable(true);
    m_actions.settingsBehaviorAutoResultToClipboard->setCheckable(true);
    m_actions.settingsBehaviorComplexNumbers->setCheckable(true);
    m_actions.settingsRadixCharComma->setCheckable(true);
    m_actions.settingsRadixCharDefault->setCheckable(true);
    m_actions.settingsRadixCharDot->setCheckable(true);
    m_actions.settingsRadixCharBoth->setCheckable(true);
    m_actions.settingsResultFormat0Digits->setCheckable(true);
    m_actions.settingsResultFormat15Digits->setCheckable(true);
    m_actions.settingsResultFormat2Digits->setCheckable(true);
    m_actions.settingsResultFormat3Digits->setCheckable(true);
    m_actions.settingsResultFormat50Digits->setCheckable(true);
    m_actions.settingsResultFormat8Digits->setCheckable(true);
    m_actions.settingsResultFormatAutoPrecision->setCheckable(true);
    m_actions.settingsResultFormatBinary->setCheckable(true);
    m_actions.settingsResultFormatCartesian->setCheckable(true);
    m_actions.settingsResultFormatEngineering->setCheckable(true);
    m_actions.settingsResultFormatFixed->setCheckable(true);
    m_actions.settingsResultFormatGeneral->setCheckable(true);
    m_actions.settingsResultFormatHexadecimal->setCheckable(true);
    m_actions.settingsResultFormatOctal->setCheckable(true);
    m_actions.settingsResultFormatPolar->setCheckable(true);
    m_actions.settingsResultFormatScientific->setCheckable(true);
    m_actions.settingsResultFormatSexagesimal->setCheckable(true);
    m_actions.settingsAlternativeResultFormatDisabled->setCheckable(true);
    m_actions.settingsAlternativeResultFormatGeneral->setCheckable(true);
    m_actions.settingsAlternativeResultFormatFixed->setCheckable(true);
    m_actions.settingsAlternativeResultFormatEngineering->setCheckable(true);
    m_actions.settingsAlternativeResultFormatScientific->setCheckable(true);
    m_actions.settingsAlternativeResultFormatBinary->setCheckable(true);
    m_actions.settingsAlternativeResultFormatOctal->setCheckable(true);
    m_actions.settingsAlternativeResultFormatHexadecimal->setCheckable(true);
    m_actions.settingsAlternativeResultFormatSexagesimal->setCheckable(true);
    m_actions.settingsTertiaryResultFormatDisabled->setCheckable(true);
    m_actions.settingsTertiaryResultFormatGeneral->setCheckable(true);
    m_actions.settingsTertiaryResultFormatFixed->setCheckable(true);
    m_actions.settingsTertiaryResultFormatEngineering->setCheckable(true);
    m_actions.settingsTertiaryResultFormatScientific->setCheckable(true);
    m_actions.settingsTertiaryResultFormatBinary->setCheckable(true);
    m_actions.settingsTertiaryResultFormatOctal->setCheckable(true);
    m_actions.settingsTertiaryResultFormatHexadecimal->setCheckable(true);
    m_actions.settingsTertiaryResultFormatSexagesimal->setCheckable(true);
    m_actions.viewConstants->setCheckable(true);
    m_actions.viewFullScreenMode->setCheckable(true);
    m_actions.viewFunctions->setCheckable(true);
    m_actions.viewHistory->setCheckable(true);
    m_actions.viewKeypad->setCheckable(true);
    m_actions.viewFormulaBook->setCheckable(true);
    m_actions.viewStatusBar->setCheckable(true);
    m_actions.viewMenuBar->setCheckable(true);
    m_actions.viewVariables->setCheckable(true);
    m_actions.viewBitfield->setCheckable(true);
    m_actions.viewUserFunctions->setCheckable(true);

    const auto schemes = ColorScheme::enumerate(); // TODO: use qAsConst().
    for (auto& colorScheme : schemes) {
        auto action = new QAction(this);
        action->setCheckable(true);
        action->setText(colorScheme);
        action->setData(colorScheme);
        m_actions.settingsDisplayColorSchemes.append(action);
    }
}

void MainWindow::retranslateText()
{
    QTranslator* tr = 0;
    tr = createTranslator(m_settings->language);
    if (tr) {
        if (m_translator) {
            qApp->removeTranslator(m_translator);
            m_translator->deleteLater();
        }

        qApp->installTranslator(tr);
        m_translator = tr;
    } else {
        qApp->removeTranslator(m_translator);
        m_translator = 0;
    }

    setMenusText();
    setActionsText();
    setStatusBarText();
    setWidgetsDirection();
}

void MainWindow::setStatusBarText()
{
    if (m_status.angleUnit) {
        m_status.angleUnitLabel->setText(MainWindow::tr("Angle Unit:"));
        m_status.resultFormatLabel->setText(MainWindow::tr("Result Format:"));
        m_status.complexNumbersLabel->setText(MainWindow::tr("Complex Numbers:"));
        m_status.complexFormatLabel->setText(MainWindow::tr("Complex Format:"));
        m_status.angleUnit->setText(statusBarAngleUnitValue());
        m_status.resultFormat->setText(statusBarResultFormatValue());
        m_status.complexNumbers->setText(statusBarComplexNumbersValue());
        m_status.complexFormat->setText(statusBarComplexFormatValue());

        m_status.angleUnit->setToolTip(MainWindow::tr("Angle unit"));
        m_status.resultFormat->setToolTip(MainWindow::tr("Result format"));
        m_status.complexNumbers->setToolTip(MainWindow::tr("Complex numbers"));
        m_status.complexFormat->setToolTip(MainWindow::tr("Complex format"));
        m_status.complexFormatSection->setVisible(m_settings->complexNumbers);
    }
}

QString MainWindow::statusBarAngleUnitValue() const
{
    return (m_settings->angleUnit == 'r' ? MainWindow::tr("Radian")
        : (m_settings->angleUnit == 'g') ? MainWindow::tr("Gradian") : MainWindow::tr("Degree"));
}

QString MainWindow::statusBarResultFormatValue() const
{
    switch (m_settings->resultFormat) {
        case 'b': return MainWindow::tr("Binary");
        case 'o': return MainWindow::tr("Octal");
        case 'h': return MainWindow::tr("Hexadecimal");
        case 's': return MainWindow::tr("Sexagesimal");
        case 'f': return MainWindow::tr("Fixed decimal");
        case 'n': return MainWindow::tr("Engineering decimal");
        case 'e': return MainWindow::tr("Scientific decimal");
        case 'g': return MainWindow::tr("General decimal");
        default : return QString();
    }
}

QString MainWindow::statusBarComplexNumbersValue() const
{
    return m_settings->complexNumbers ? MainWindow::tr("On") : MainWindow::tr("Off");
}

QString MainWindow::statusBarComplexFormatValue() const
{
    return m_settings->resultFormatComplex == 'p' ? MainWindow::tr("Polar") : MainWindow::tr("Cartesian");
}

void MainWindow::setActionsText()
{
    m_actions.sessionExportHtml->setText(MainWindow::tr("&HTML"));
    m_actions.sessionExportPlainText->setText(MainWindow::tr("Plain &text"));
    m_actions.sessionImport->setText(MainWindow::tr("&Import..."));
    m_actions.sessionLoad->setText(MainWindow::tr("&Load..."));
    m_actions.sessionQuit->setText(MainWindow::tr("&Quit"));
    m_actions.sessionSave->setText(MainWindow::tr("&Save..."));

    m_actions.editClearExpression->setText(MainWindow::tr("Clear E&xpression"));
    m_actions.editClearHistory->setText(MainWindow::tr("Clear &History"));
    m_actions.editCopyLastResult->setText(MainWindow::tr("Copy Last &Result"));
    m_actions.editCopy->setText(MainWindow::tr("&Copy"));
    m_actions.editPaste->setText(MainWindow::tr("&Paste"));
    m_actions.editSelectExpression->setText(MainWindow::tr("&Select Expression"));
    m_actions.editWrapSelection->setText(MainWindow::tr("&Wrap Selection in Parentheses"));

    m_actions.viewConstants->setText(MainWindow::tr("&Constants"));
    m_actions.viewFullScreenMode->setText(MainWindow::tr("F&ull Screen Mode"));
    m_actions.viewFunctions->setText(MainWindow::tr("&Functions"));
    m_actions.viewHistory->setText(MainWindow::tr("&History"));
    m_actions.viewKeypad->setText(MainWindow::tr("&Keypad"));
    m_actions.viewFormulaBook->setText(MainWindow::tr("Formula &Book"));
    m_actions.viewStatusBar->setText(MainWindow::tr("&Status Bar"));
    m_actions.viewMenuBar->setText(MainWindow::tr("Main &Menu"));
    m_actions.viewVariables->setText(MainWindow::tr("&Variables"));
    m_actions.viewBitfield->setText(MainWindow::tr("Bitfield"));
    m_actions.viewUserFunctions->setText(MainWindow::tr("Use&r Functions"));

    m_actions.settingsAngleUnitDegree->setText(MainWindow::tr("&Degree"));
    m_actions.settingsAngleUnitRadian->setText(MainWindow::tr("&Radian"));
    m_actions.settingsAngleUnitGradian->setText(MainWindow::tr("&Gradian"));
    m_actions.settingsAngleUnitCycle->setText(MainWindow::tr("&Cycle Unit"));
    m_actions.settingsBehaviorAlwaysOnTop->setText(MainWindow::tr("Always on &Top"));
    m_actions.settingsBehaviorAutoAns->setText(MainWindow::tr("Automatic Result &Reuse"));
    m_actions.settingsBehaviorAutoCompletion->setText(MainWindow::tr("Automatic &Completion"));
    m_actions.settingsBehaviorPartialResults->setText(MainWindow::tr("Live Results &Tooltip"));
    m_actions.settingsBehaviorHistorySavingNever->setText(MainWindow::tr("&Never"));
    m_actions.settingsBehaviorHistorySavingOnExit->setText(MainWindow::tr("On &Exit"));
    m_actions.settingsBehaviorHistorySavingContinuously->setText(MainWindow::tr("&Continuously"));
    m_actions.settingsBehaviorSaveWindowPositionOnExit->setText(MainWindow::tr("Save &Window Positon on Exit"));
    m_actions.settingsBehaviorSyntaxHighlighting->setText(MainWindow::tr("Syntax &Highlighting"));
    m_actions.settingsBehaviorHoverHighlightResults->setText(MainWindow::tr("Hover Highlighting"));
    m_actions.settingsBehaviorDigitGroupingNone->setText(MainWindow::tr("Disabled"));
    m_actions.settingsBehaviorDigitGroupingOneSpace->setText(MainWindow::tr("Small Space"));
    m_actions.settingsBehaviorDigitGroupingTwoSpaces->setText(MainWindow::tr("Medium Space"));
    m_actions.settingsBehaviorDigitGroupingThreeSpaces->setText(MainWindow::tr("Large Space"));
    m_actions.settingsBehaviorDigitGroupingIntegerPartOnly->setText(MainWindow::tr("Group Integer Part Only"));
    m_actions.settingsBehaviorLeaveLastExpression->setText(MainWindow::tr("Leave &Last Expression"));
    m_actions.settingsBehaviorAutoResultToClipboard->setText(MainWindow::tr("Automatic &Result to Clipboard"));
    m_actions.settingsBehaviorHistorySizeLimit->setText(MainWindow::tr("History Size &Limit..."));
    m_actions.settingsBehaviorComplexNumbers->setText(MainWindow::tr("Enable Complex Numbers"));
    m_actions.settingsRadixCharComma->setText(MainWindow::tr("&Comma"));
    m_actions.settingsRadixCharDefault->setText(MainWindow::tr("&System Default"));
    m_actions.settingsRadixCharDot->setText(MainWindow::tr("&Dot"));
    m_actions.settingsRadixCharBoth->setText(MainWindow::tr("Dot &And Comma"));
    m_actions.settingsResultFormat0Digits->setText(MainWindow::tr("&0 Digits"));
    m_actions.settingsResultFormat15Digits->setText(MainWindow::tr("&15 Digits"));
    m_actions.settingsResultFormat2Digits->setText(MainWindow::tr("&2 Digits"));
    m_actions.settingsResultFormat3Digits->setText(MainWindow::tr("&3 Digits"));
    m_actions.settingsResultFormat50Digits->setText(MainWindow::tr("&50 Digits"));
    m_actions.settingsResultFormat8Digits->setText(MainWindow::tr("&8 Digits"));
    m_actions.settingsResultFormatAutoPrecision->setText(MainWindow::tr("&Automatic"));
    m_actions.settingsResultFormatGeneral->setText(MainWindow::tr("&General"));
    m_actions.settingsResultFormatFixed->setText(MainWindow::tr("&Fixed Decimal"));
    m_actions.settingsResultFormatEngineering->setText(MainWindow::tr("&Engineering"));
    m_actions.settingsResultFormatScientific->setText(MainWindow::tr("&Scientific"));
    m_actions.settingsResultFormatBinary->setText(MainWindow::tr("&Binary"));
    m_actions.settingsResultFormatOctal->setText(MainWindow::tr("&Octal"));
    m_actions.settingsResultFormatHexadecimal->setText(MainWindow::tr("&Hexadecimal"));
    m_actions.settingsResultFormatSexagesimal->setText(MainWindow::tr("&Sexagesimal"));
    m_actions.settingsAlternativeResultFormatDisabled->setText(MainWindow::tr("&Disabled"));
    m_actions.settingsAlternativeResultFormatGeneral->setText(MainWindow::tr("&General"));
    m_actions.settingsAlternativeResultFormatFixed->setText(MainWindow::tr("&Fixed Decimal"));
    m_actions.settingsAlternativeResultFormatEngineering->setText(MainWindow::tr("&Engineering"));
    m_actions.settingsAlternativeResultFormatScientific->setText(MainWindow::tr("&Scientific"));
    m_actions.settingsAlternativeResultFormatBinary->setText(MainWindow::tr("&Binary"));
    m_actions.settingsAlternativeResultFormatOctal->setText(MainWindow::tr("&Octal"));
    m_actions.settingsAlternativeResultFormatHexadecimal->setText(MainWindow::tr("&Hexadecimal"));
    m_actions.settingsAlternativeResultFormatSexagesimal->setText(MainWindow::tr("&Sexagesimal"));
    m_actions.settingsTertiaryResultFormatDisabled->setText(MainWindow::tr("&Disabled"));
    m_actions.settingsTertiaryResultFormatGeneral->setText(MainWindow::tr("&General"));
    m_actions.settingsTertiaryResultFormatFixed->setText(MainWindow::tr("&Fixed Decimal"));
    m_actions.settingsTertiaryResultFormatEngineering->setText(MainWindow::tr("&Engineering"));
    m_actions.settingsTertiaryResultFormatScientific->setText(MainWindow::tr("&Scientific"));
    m_actions.settingsTertiaryResultFormatBinary->setText(MainWindow::tr("&Binary"));
    m_actions.settingsTertiaryResultFormatOctal->setText(MainWindow::tr("&Octal"));
    m_actions.settingsTertiaryResultFormatHexadecimal->setText(MainWindow::tr("&Hexadecimal"));
    m_actions.settingsTertiaryResultFormatSexagesimal->setText(MainWindow::tr("&Sexagesimal"));
    m_actions.settingsResultFormatCartesian->setText(MainWindow::tr("&Cartesian"));
    m_actions.settingsResultFormatPolar->setText(MainWindow::tr("&Polar"));
    m_actions.settingsDisplayFont->setText(MainWindow::tr("&Font..."));
    m_actions.settingsLanguage->setText(MainWindow::tr("&Language..."));

    m_actions.helpManual->setText(MainWindow::tr("User &Manual"));
    m_actions.contextHelp->setText(MainWindow::tr("Context Help"));
    m_actions.helpUpdates->setText(MainWindow::tr("Check &Updates"));
    m_actions.helpFeedback->setText(MainWindow::tr("Send &Feedback"));
    m_actions.helpCommunity->setText(MainWindow::tr("Join &Community"));
    m_actions.helpNews->setText(MainWindow::tr("&News Feed"));
    m_actions.helpDonate->setText(MainWindow::tr("&Donate"));
    m_actions.helpAbout->setText(MainWindow::tr("About &SpeedCrunch"));
}

void MainWindow::createActionGroups()
{
    m_actionGroups.resultFormat = new QActionGroup(this);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatBinary);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatGeneral);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatFixed);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatEngineering);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatScientific);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatOctal);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatHexadecimal);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatSexagesimal);

    m_actionGroups.alternativeResultFormat = new QActionGroup(this);
    m_actionGroups.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatDisabled);
    m_actionGroups.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatBinary);
    m_actionGroups.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatGeneral);
    m_actionGroups.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatFixed);
    m_actionGroups.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatEngineering);
    m_actionGroups.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatScientific);
    m_actionGroups.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatOctal);
    m_actionGroups.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatHexadecimal);
    m_actionGroups.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatSexagesimal);

    m_actionGroups.tertiaryResultFormat = new QActionGroup(this);
    m_actionGroups.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatDisabled);
    m_actionGroups.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatBinary);
    m_actionGroups.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatGeneral);
    m_actionGroups.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatFixed);
    m_actionGroups.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatEngineering);
    m_actionGroups.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatScientific);
    m_actionGroups.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatOctal);
    m_actionGroups.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatHexadecimal);
    m_actionGroups.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatSexagesimal);

    m_actionGroups.complexFormat = new QActionGroup(this);
    m_actionGroups.complexFormat->addAction(m_actions.settingsResultFormatCartesian);
    m_actionGroups.complexFormat->addAction(m_actions.settingsResultFormatPolar);

    m_actionGroups.radixChar = new QActionGroup(this);
    m_actionGroups.radixChar->addAction(m_actions.settingsRadixCharDefault);
    m_actionGroups.radixChar->addAction(m_actions.settingsRadixCharDot);
    m_actionGroups.radixChar->addAction(m_actions.settingsRadixCharComma);
    m_actionGroups.radixChar->addAction(m_actions.settingsRadixCharBoth);

    m_actionGroups.digits = new QActionGroup(this);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormatAutoPrecision);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormat0Digits);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormat2Digits);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormat3Digits);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormat8Digits);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormat15Digits);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormat50Digits);

    m_actionGroups.angle = new QActionGroup(this);
    m_actionGroups.angle->addAction(m_actions.settingsAngleUnitDegree);
    m_actionGroups.angle->addAction(m_actions.settingsAngleUnitRadian);
    m_actionGroups.angle->addAction(m_actions.settingsAngleUnitGradian);

    m_actionGroups.colorScheme = new QActionGroup(this);
    const auto schemes = m_actions.settingsDisplayColorSchemes;
    for (auto& action : schemes)
        m_actionGroups.colorScheme->addAction(action);

    m_actionGroups.digitGrouping = new QActionGroup(this);
    m_actionGroups.digitGrouping->addAction(m_actions.settingsBehaviorDigitGroupingNone);
    m_actionGroups.digitGrouping->addAction(m_actions.settingsBehaviorDigitGroupingOneSpace);
    m_actionGroups.digitGrouping->addAction(m_actions.settingsBehaviorDigitGroupingTwoSpaces);
    m_actionGroups.digitGrouping->addAction(m_actions.settingsBehaviorDigitGroupingThreeSpaces);

    m_actionGroups.historySaving = new QActionGroup(this);
    m_actionGroups.historySaving->addAction(m_actions.settingsBehaviorHistorySavingNever);
    m_actionGroups.historySaving->addAction(m_actions.settingsBehaviorHistorySavingOnExit);
    m_actionGroups.historySaving->addAction(m_actions.settingsBehaviorHistorySavingContinuously);
}

void MainWindow::createActionShortcuts()
{
    m_actions.sessionLoad->setShortcut(Qt::CTRL | Qt::Key_L);
    m_actions.sessionQuit->setShortcut(Qt::CTRL | Qt::Key_Q);
    m_actions.sessionSave->setShortcut(Qt::CTRL | Qt::Key_S);
    m_actions.editClearHistory->setShortcut(Qt::CTRL | Qt::Key_N);
    m_actions.editCopyLastResult->setShortcut(Qt::CTRL | Qt::Key_R);
    m_actions.editCopy->setShortcut(Qt::CTRL | Qt::Key_C);
    m_actions.editPaste->setShortcut(Qt::CTRL | Qt::Key_V);
    m_actions.editSelectExpression->setShortcut(Qt::CTRL | Qt::Key_A);
    m_actions.editWrapSelection->setShortcut(Qt::CTRL | Qt::Key_P);
    m_actions.viewBitfield->setShortcut(Qt::CTRL | Qt::Key_6);
    m_actions.viewConstants->setShortcut(Qt::CTRL | Qt::Key_2);
    m_actions.viewFullScreenMode->setShortcut(Qt::Key_F11);
    m_actions.viewFunctions->setShortcut(Qt::CTRL | Qt::Key_3);
    m_actions.viewHistory->setShortcut(Qt::CTRL | Qt::Key_7);
    m_actions.viewKeypad->setShortcut(Qt::CTRL | Qt::Key_K);
    m_actions.viewFormulaBook->setShortcut(Qt::CTRL | Qt::Key_1);
    m_actions.viewStatusBar->setShortcut(Qt::CTRL | Qt::Key_B);
    m_actions.viewVariables->setShortcut(Qt::CTRL | Qt::Key_4);
    m_actions.viewUserFunctions->setShortcut(Qt::CTRL | Qt::Key_5);
    m_actions.settingsResultFormatGeneral->setShortcut(Qt::Key_F2);
    m_actions.settingsResultFormatFixed->setShortcut(Qt::Key_F3);
    m_actions.settingsResultFormatEngineering->setShortcut(Qt::Key_F4);
    m_actions.settingsResultFormatScientific->setShortcut(Qt::Key_F5);
    m_actions.settingsResultFormatBinary->setShortcut(Qt::Key_F6);
    m_actions.settingsResultFormatOctal->setShortcut(Qt::Key_F7);
    m_actions.settingsResultFormatHexadecimal->setShortcut(Qt::Key_F8);
    m_actions.settingsResultFormatSexagesimal->setShortcut(Qt::Key_F9);
    m_actions.settingsAngleUnitCycle->setShortcut(Qt::Key_F10);
    m_actions.settingsRadixCharDot->setShortcut(Qt::CTRL | Qt::Key_Period);
    m_actions.settingsRadixCharComma->setShortcut(Qt::CTRL | Qt::Key_Comma);
    m_actions.contextHelp->setShortcut(Qt::Key_F1);
}

void MainWindow::createMenus()
{
    m_menus.session = new QMenu("", this);
    menuBar()->addMenu(m_menus.session);
    m_menus.session->addAction(m_actions.sessionLoad);
    m_menus.session->addAction(m_actions.sessionSave);
    m_menus.session->addSeparator();
    m_menus.session->addAction(m_actions.sessionImport);
    m_menus.sessionExport = m_menus.session->addMenu("");
    m_menus.sessionExport->addAction(m_actions.sessionExportPlainText);
    m_menus.sessionExport->addAction(m_actions.sessionExportHtml);
    m_menus.session->addSeparator();
    m_menus.session->addAction(m_actions.sessionQuit);

    m_menus.edit = new QMenu("", this);
    menuBar()->addMenu(m_menus.edit);
    m_menus.edit->addAction(m_actions.editCopy);
    m_menus.edit->addAction(m_actions.editCopyLastResult);
    m_menus.edit->addAction(m_actions.editPaste);
    m_menus.edit->addAction(m_actions.editSelectExpression);
    m_menus.edit->addAction(m_actions.editClearExpression);
    m_menus.edit->addAction(m_actions.editClearHistory);
    m_menus.edit->addAction(m_actions.editWrapSelection);

    m_menus.view = new QMenu("", this);
    menuBar()->addMenu(m_menus.view);
    m_menus.view->addAction(m_actions.viewKeypad);
    m_menus.view->addSeparator();
    m_menus.view->addAction(m_actions.viewFormulaBook);
    m_menus.view->addAction(m_actions.viewConstants);
    m_menus.view->addAction(m_actions.viewFunctions);
    m_menus.view->addAction(m_actions.viewVariables);
    m_menus.view->addAction(m_actions.viewUserFunctions);
    m_menus.view->addAction(m_actions.viewBitfield);
    m_menus.view->addAction(m_actions.viewHistory);
    m_menus.view->addSeparator();
    m_menus.view->addAction(m_actions.viewStatusBar);
#if !defined(Q_OS_MACOS)
    m_menus.view->addAction(m_actions.viewMenuBar);
#endif
    m_menus.view->addSeparator();
    m_menus.view->addAction(m_actions.viewFullScreenMode);

    m_menus.settings = new QMenu("", this);
    menuBar()->addMenu(m_menus.settings);

    m_menus.resultFormat = m_menus.settings->addMenu("");

    m_menus.decimal = m_menus.resultFormat->addMenu("");
    m_menus.decimal->addAction(m_actions.settingsResultFormatGeneral);
    m_menus.decimal->addAction(m_actions.settingsResultFormatFixed);
    m_menus.decimal->addAction(m_actions.settingsResultFormatEngineering);
    m_menus.decimal->addAction(m_actions.settingsResultFormatScientific);

    m_menus.resultFormat->addAction(m_actions.settingsResultFormatBinary);
    m_menus.resultFormat->addAction(m_actions.settingsResultFormatOctal);
    m_menus.resultFormat->addAction(m_actions.settingsResultFormatHexadecimal);
    m_menus.resultFormat->addAction(m_actions.settingsResultFormatSexagesimal);
    m_menus.resultFormat->addSeparator();

    m_menus.precision = m_menus.resultFormat->addMenu("");
    m_menus.precision->addAction(m_actions.settingsResultFormatAutoPrecision);
    m_menus.precision->addAction(m_actions.settingsResultFormat0Digits);
    m_menus.precision->addAction(m_actions.settingsResultFormat2Digits);
    m_menus.precision->addAction(m_actions.settingsResultFormat3Digits);
    m_menus.precision->addAction(m_actions.settingsResultFormat8Digits);
    m_menus.precision->addAction(m_actions.settingsResultFormat15Digits);
    m_menus.precision->addAction(m_actions.settingsResultFormat50Digits);

    m_menus.complexFormat = m_menus.resultFormat->addMenu("");
    m_menus.complexFormat->addAction(m_actions.settingsResultFormatCartesian);
    m_menus.complexFormat->addAction(m_actions.settingsResultFormatPolar);

    m_menus.settings->addSeparator();

    m_menus.alternativeResultFormat = m_menus.settings->addMenu("");
    m_menus.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatDisabled);
    m_menus.alternativeResultFormat->addSeparator();
    m_menus.alternativeDecimal = m_menus.alternativeResultFormat->addMenu("");
    m_menus.alternativeDecimal->addAction(m_actions.settingsAlternativeResultFormatGeneral);
    m_menus.alternativeDecimal->addAction(m_actions.settingsAlternativeResultFormatFixed);
    m_menus.alternativeDecimal->addAction(m_actions.settingsAlternativeResultFormatEngineering);
    m_menus.alternativeDecimal->addAction(m_actions.settingsAlternativeResultFormatScientific);
    m_menus.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatBinary);
    m_menus.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatOctal);
    m_menus.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatHexadecimal);
    m_menus.alternativeResultFormat->addAction(m_actions.settingsAlternativeResultFormatSexagesimal);

    m_menus.tertiaryResultFormat = m_menus.settings->addMenu("");
    m_menus.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatDisabled);
    m_menus.tertiaryResultFormat->addSeparator();
    m_menus.tertiaryDecimal = m_menus.tertiaryResultFormat->addMenu("");
    m_menus.tertiaryDecimal->addAction(m_actions.settingsTertiaryResultFormatGeneral);
    m_menus.tertiaryDecimal->addAction(m_actions.settingsTertiaryResultFormatFixed);
    m_menus.tertiaryDecimal->addAction(m_actions.settingsTertiaryResultFormatEngineering);
    m_menus.tertiaryDecimal->addAction(m_actions.settingsTertiaryResultFormatScientific);
    m_menus.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatBinary);
    m_menus.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatOctal);
    m_menus.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatHexadecimal);
    m_menus.tertiaryResultFormat->addAction(m_actions.settingsTertiaryResultFormatSexagesimal);

    m_menus.settings->addSeparator();

    m_menus.inputFormat = m_menus.settings->addMenu("");

    m_menus.radixChar = m_menus.inputFormat->addMenu("");
    m_menus.radixChar->addAction(m_actions.settingsRadixCharDefault);
    m_menus.radixChar->addSeparator();
    m_menus.radixChar->addAction(m_actions.settingsRadixCharDot);
    m_menus.radixChar->addAction(m_actions.settingsRadixCharComma);
    m_menus.radixChar->addAction(m_actions.settingsRadixCharBoth);

    m_menus.angleUnit = m_menus.settings->addMenu("");
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitDegree);
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitRadian);
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitGradian);
    m_menus.angleUnit->addSeparator();
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitCycle);

    m_menus.digitGrouping = m_menus.settings->addMenu("");
    m_menus.digitGrouping->addAction(m_actions.settingsBehaviorDigitGroupingNone);
    m_menus.digitGrouping->addSeparator();
    m_menus.digitGrouping->addAction(m_actions.settingsBehaviorDigitGroupingOneSpace);
    m_menus.digitGrouping->addAction(m_actions.settingsBehaviorDigitGroupingTwoSpaces);
    m_menus.digitGrouping->addAction(m_actions.settingsBehaviorDigitGroupingThreeSpaces);
    m_menus.digitGrouping->addSeparator();
    m_menus.digitGrouping->addAction(m_actions.settingsBehaviorDigitGroupingIntegerPartOnly);

    m_menus.history = m_menus.settings->addMenu("");
    m_menus.historySaving = m_menus.history->addMenu("");
    m_menus.historySaving->addAction(m_actions.settingsBehaviorHistorySavingNever);
    m_menus.historySaving->addAction(m_actions.settingsBehaviorHistorySavingOnExit);
    m_menus.historySaving->addAction(m_actions.settingsBehaviorHistorySavingContinuously);
    m_menus.history->addAction(m_actions.settingsBehaviorHistorySizeLimit);

    m_menus.behavior = m_menus.settings->addMenu("");
    m_menus.behavior->addAction(m_actions.settingsBehaviorSaveWindowPositionOnExit);
    m_menus.behavior->addSeparator();
    m_menus.behavior->addAction(m_actions.settingsBehaviorPartialResults);
    m_menus.behavior->addAction(m_actions.settingsBehaviorAutoAns);
    m_menus.behavior->addAction(m_actions.settingsBehaviorAutoCompletion);
    m_menus.behavior->addAction(m_actions.settingsBehaviorSyntaxHighlighting);
    m_menus.behavior->addAction(m_actions.settingsBehaviorHoverHighlightResults);

    m_menus.behavior->addAction(m_actions.settingsBehaviorLeaveLastExpression);
    m_menus.behavior->addAction(m_actions.settingsBehaviorComplexNumbers);
    m_menus.behavior->addSeparator();
    m_menus.behavior->addAction(m_actions.settingsBehaviorAlwaysOnTop);
    m_menus.behavior->addAction(m_actions.settingsBehaviorAutoResultToClipboard);

    m_menus.display = m_menus.settings->addMenu("");
    m_menus.colorScheme = m_menus.display->addMenu("");
    const auto schemes = m_actions.settingsDisplayColorSchemes; // TODO: Qt 5.7's qAsConst().
    for (auto& action : schemes)
        m_menus.colorScheme->addAction(action);
    m_menus.display->addAction(m_actions.settingsDisplayFont);

    m_menus.settings->addAction(m_actions.settingsLanguage);

    m_menus.help = new QMenu("", this);
    menuBar()->addMenu(m_menus.help);
    m_menus.help->addAction(m_actions.helpManual);
    m_menus.help->addAction(m_actions.contextHelp);
    m_menus.help->addSeparator();
    m_menus.help->addAction(m_actions.helpUpdates);
    m_menus.help->addAction(m_actions.helpFeedback);
    m_menus.help->addAction(m_actions.helpCommunity);
    m_menus.help->addAction(m_actions.helpNews);
    m_menus.help->addAction(m_actions.helpDonate);
    m_menus.help->addSeparator();
    m_menus.help->addAction(m_actions.helpAbout);

    addActions(menuBar()->actions());
}

void MainWindow::setMenusText()
{
    m_menus.session->setTitle(MainWindow::tr("&Session"));
    m_menus.sessionExport->setTitle(MainWindow::tr("&Export"));
    m_menus.edit->setTitle(MainWindow::tr("&Edit"));
    m_menus.view->setTitle(MainWindow::tr("&View"));
    m_menus.settings->setTitle(MainWindow::tr("Se&ttings"));
    m_menus.resultFormat->setTitle(MainWindow::tr("Result &Format"));
    m_menus.alternativeResultFormat->setTitle(MainWindow::tr("Secondary Result Format"));
    m_menus.tertiaryResultFormat->setTitle(MainWindow::tr("Tertiary Result Format"));
    m_menus.inputFormat->setTitle(MainWindow::tr("&Input Format"));
    m_menus.radixChar->setTitle(MainWindow::tr("Radix &Character"));
    m_menus.decimal->setTitle(MainWindow::tr("&Decimal"));
    m_menus.alternativeDecimal->setTitle(MainWindow::tr("&Decimal"));
    m_menus.tertiaryDecimal->setTitle(MainWindow::tr("&Decimal"));
    m_menus.precision->setTitle(MainWindow::tr("&Precision"));
    m_menus.angleUnit->setTitle(MainWindow::tr("&Angle Unit"));
    m_menus.complexFormat->setTitle(MainWindow::tr("Comple&x Format"));
    m_menus.behavior->setTitle(MainWindow::tr("&Behavior"));
    m_menus.history->setTitle(MainWindow::tr("&History"));
    m_menus.historySaving->setTitle(MainWindow::tr("History &Saving"));
    m_menus.display->setTitle(MainWindow::tr("&Display"));
    m_menus.colorScheme->setTitle(MainWindow::tr("Color Scheme"));
    m_menus.help->setTitle(MainWindow::tr("&Help"));
    m_menus.digitGrouping->setTitle(MainWindow::tr("Digit Grouping"));
}

void MainWindow::createStatusBar()
{
    QStatusBar* bar = statusBar();

    m_status.angleUnitSection = new QWidget(bar);
    m_status.resultFormatSection = new QWidget(bar);
    m_status.complexNumbersSection = new QWidget(bar);
    m_status.complexFormatSection = new QWidget(bar);

    m_status.angleUnitLabel = new QLabel(m_status.angleUnitSection);
    m_status.resultFormatLabel = new QLabel(m_status.resultFormatSection);
    m_status.complexNumbersLabel = new QLabel(m_status.complexNumbersSection);
    m_status.complexFormatLabel = new QLabel(m_status.complexFormatSection);

    m_status.angleUnit = new QPushButton(bar);
    m_status.resultFormat = new QPushButton(bar);
    m_status.complexNumbers = new QPushButton(bar);
    m_status.complexFormat = new QPushButton(bar);

    m_status.angleUnit->setParent(m_status.angleUnitSection);
    m_status.resultFormat->setParent(m_status.resultFormatSection);
    m_status.complexNumbers->setParent(m_status.complexNumbersSection);
    m_status.complexFormat->setParent(m_status.complexFormatSection);

    QHBoxLayout* angleLayout = new QHBoxLayout(m_status.angleUnitSection);
    QHBoxLayout* formatLayout = new QHBoxLayout(m_status.resultFormatSection);
    QHBoxLayout* complexLayout = new QHBoxLayout(m_status.complexNumbersSection);
    QHBoxLayout* complexFormatLayout = new QHBoxLayout(m_status.complexFormatSection);
    angleLayout->setContentsMargins(0, 0, 0, 0);
    formatLayout->setContentsMargins(0, 0, 0, 0);
    complexLayout->setContentsMargins(0, 0, 0, 0);
    complexFormatLayout->setContentsMargins(0, 0, 0, 0);
    angleLayout->setSpacing(2);
    formatLayout->setSpacing(2);
    complexLayout->setSpacing(2);
    complexFormatLayout->setSpacing(2);
    angleLayout->addWidget(m_status.angleUnitLabel);
    angleLayout->addWidget(m_status.angleUnit);
    formatLayout->addWidget(m_status.resultFormatLabel);
    formatLayout->addWidget(m_status.resultFormat);
    complexLayout->addWidget(m_status.complexNumbersLabel);
    complexLayout->addWidget(m_status.complexNumbers);
    complexFormatLayout->addWidget(m_status.complexFormatLabel);
    complexFormatLayout->addWidget(m_status.complexFormat);

    QFont boldFont = m_status.angleUnitLabel->font();
    boldFont.setBold(true);
    m_status.angleUnitLabel->setFont(boldFont);
    m_status.resultFormatLabel->setFont(boldFont);
    m_status.complexNumbersLabel->setFont(boldFont);
    m_status.complexFormatLabel->setFont(boldFont);

    m_status.angleUnit->setFocusPolicy(Qt::NoFocus);
    m_status.resultFormat->setFocusPolicy(Qt::NoFocus);
    m_status.complexNumbers->setFocusPolicy(Qt::NoFocus);
    m_status.complexFormat->setFocusPolicy(Qt::NoFocus);

    m_status.angleUnit->setFlat(true);
    m_status.resultFormat->setFlat(true);
    m_status.complexNumbers->setFlat(true);
    m_status.complexFormat->setFlat(true);

    m_status.angleUnit->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_status.angleUnit->addAction(m_actions.settingsAngleUnitDegree);
    m_status.angleUnit->addAction(m_actions.settingsAngleUnitRadian);
    m_status.angleUnit->addAction(m_actions.settingsAngleUnitGradian);

    m_status.resultFormat->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_status.resultFormat, SIGNAL(customContextMenuRequested(const QPoint&)),
        SLOT(showResultFormatContextMenu(const QPoint&)));
    m_status.complexNumbers->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_status.complexNumbers->addAction(m_actions.settingsBehaviorComplexNumbers);
    m_status.complexFormat->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_status.complexFormat->addAction(m_actions.settingsResultFormatCartesian);
    m_status.complexFormat->addAction(m_actions.settingsResultFormatPolar);

    connect(m_status.angleUnit, SIGNAL(clicked()), SLOT(cycleAngleUnits()));
    connect(m_status.resultFormat, SIGNAL(clicked()), SLOT(cycleResultFormats()));
    connect(m_status.complexNumbers, SIGNAL(clicked()), m_actions.settingsBehaviorComplexNumbers, SLOT(trigger()));
    connect(m_status.complexFormat, &QPushButton::clicked, this, [this]() {
        if (m_actions.settingsResultFormatCartesian->isChecked())
            m_actions.settingsResultFormatPolar->trigger();
        else
            m_actions.settingsResultFormatCartesian->trigger();
    });

    bar->addWidget(m_status.resultFormatSection);
    bar->addWidget(m_status.angleUnitSection);
    bar->addWidget(m_status.complexNumbersSection);
    bar->addWidget(m_status.complexFormatSection);

    setStatusBarText();
}

void MainWindow::createFixedWidgets()
{
    m_widgets.root = new QWidget(this);
    setCentralWidget(m_widgets.root);

    m_layouts.root = new QVBoxLayout(m_widgets.root);
    m_layouts.root->setSpacing(0);
    m_layouts.root->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* displayLayout = new QHBoxLayout();
    displayLayout->setSpacing(0);
    m_widgets.display = new ResultDisplay(m_widgets.root);
    m_widgets.display->setFrameStyle(QFrame::NoFrame);
    displayLayout->addWidget(m_widgets.display);
    m_layouts.root->addLayout(displayLayout);

    QHBoxLayout* editorLayout = new QHBoxLayout();
    editorLayout->setSpacing(0);
    m_widgets.editor = new Editor(m_widgets.root);
    m_widgets.editor->setFrameStyle(QFrame::NoFrame);
    m_widgets.editor->setFocus();
    editorLayout->addWidget(m_widgets.editor);
    m_layouts.root->addLayout(editorLayout);

    m_widgets.state = new QLabel(this);
    m_widgets.state->setPalette(QToolTip::palette());
    m_widgets.state->setAutoFillBackground(true);
    m_widgets.state->setFrameShape(QFrame::Box);
    m_widgets.state->hide();
}

void MainWindow::createBitField() {
    m_widgets.bitField = new BitFieldWidget(m_widgets.root);
    m_layouts.root->addWidget(m_widgets.bitField);
    m_widgets.bitField->show();
    m_widgets.display->scrollToBottom();
    connect(m_widgets.bitField, SIGNAL(bitsChanged(const QString&)), SLOT(handleBitsChanged(const QString&)));
    m_settings->bitfieldVisible = true;
}

void MainWindow::createKeypad()
{
    m_widgets.keypad = new Keypad(m_widgets.root);
    m_widgets.keypad->setFocusPolicy(Qt::NoFocus);

    connect(m_widgets.keypad, SIGNAL(buttonPressed(Keypad::Button)), SLOT(handleKeypadButtonPress(Keypad::Button)));
    connect(this, SIGNAL(radixCharacterChanged()), m_widgets.keypad, SLOT(handleRadixCharacterChange()));

    m_layouts.keypad = new QHBoxLayout();
    m_layouts.keypad->addStretch();
    m_layouts.keypad->addWidget(m_widgets.keypad);
    m_layouts.keypad->addStretch();
    m_layouts.root->addLayout(m_layouts.keypad);

    m_widgets.keypad->show();
    m_settings->keypadVisible = true;
}

void MainWindow::createBookDock(bool)
{
    m_docks.book = new BookDock(this);
    m_docks.book->setObjectName("BookDock");
    m_docks.book->installEventFilter(this);
    m_docks.book->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.book,
            SIGNAL(expressionSelected(const QString&)),
            SLOT(insertTextIntoEditor(const QString&)));

    // No focus for this dock.
    addTabifiedDock(m_docks.book, false);
    m_settings->formulaBookDockVisible = true;
}

void MainWindow::createConstantsDock(bool takeFocus)
{
    m_docks.constants = new GenericDock<ConstantsWidget>("MainWindow", QT_TR_NOOP("Constants"), this);
    m_docks.constants->setObjectName("ConstantsDock");
    m_docks.constants->installEventFilter(this);
    m_docks.constants->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.constants->widget(), &ConstantsWidget::constantSelected,
            this, &MainWindow::insertConstantIntoEditor);
    connect(this, &MainWindow::radixCharacterChanged,
            m_docks.constants->widget(), &ConstantsWidget::handleRadixCharacterChange);

    addTabifiedDock(m_docks.constants, takeFocus);
    m_settings->constantsDockVisible = true;
}

void MainWindow::createFunctionsDock(bool takeFocus)
{
    m_docks.functions = new GenericDock<FunctionsWidget>("MainWindow", QT_TR_NOOP("Functions"), this);
    m_docks.functions->setObjectName("FunctionsDock");
    m_docks.functions->installEventFilter(this);
    m_docks.functions->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.functions->widget(), &FunctionsWidget::functionSelected,
            this, &MainWindow::insertFunctionIntoEditor);

    addTabifiedDock(m_docks.functions, takeFocus);
    m_settings->functionsDockVisible = true;
}

void MainWindow::createHistoryDock(bool)
{
    m_docks.history = new GenericDock<HistoryWidget>("MainWindow", QT_TR_NOOP("History"), this);
    m_docks.history->setObjectName("HistoryDock");
    m_docks.history->installEventFilter(this);
    m_docks.history->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.history->widget(), &HistoryWidget::expressionSelected,
            this, &MainWindow::insertTextIntoEditor);
    connect(m_docks.history->widget(), &HistoryWidget::removeHistoryEntryRequested,
            this, &MainWindow::removeHistoryEntryAt);
    connect(m_docks.history->widget(), &HistoryWidget::removeHistoryEntriesAboveRequested,
            this, &MainWindow::removeHistoryEntriesAbove);
    connect(m_docks.history->widget(), &HistoryWidget::removeHistoryEntriesBelowRequested,
            this, &MainWindow::removeHistoryEntriesBelow);
    connect(this, &MainWindow::historyChanged,
            m_docks.history->widget(), &HistoryWidget::updateHistory);

    // No focus for this dock.
    addTabifiedDock(m_docks.history, false);
    m_settings->historyDockVisible = true;
}

void MainWindow::createVariablesDock(bool takeFocus)
{
    m_docks.variables = new GenericDock<VariableListWidget>("MainWindow", QT_TR_NOOP("Variables"), this);
    m_docks.variables->setObjectName("VariablesDock");
    m_docks.variables->installEventFilter(this);
    m_docks.variables->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.variables->widget(), &VariableListWidget::variableSelected,
            this, &MainWindow::insertVariableIntoEditor);
    connect(this, &MainWindow::radixCharacterChanged,
            m_docks.variables->widget(), &VariableListWidget::updateList);
    connect(this, &MainWindow::variablesChanged,
            m_docks.variables->widget(), &VariableListWidget::updateList);

    addTabifiedDock(m_docks.variables, takeFocus);
    m_settings->variablesDockVisible = true;
}

void MainWindow::createUserFunctionsDock(bool takeFocus)
{
    m_docks.userFunctions = new GenericDock<UserFunctionListWidget>("MainWindow", QT_TR_NOOP("User Functions"), this);
    m_docks.userFunctions->setObjectName("UserFunctionsDock");
    m_docks.userFunctions->installEventFilter(this);
    m_docks.userFunctions->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.userFunctions->widget(), &UserFunctionListWidget::userFunctionSelected,
            this, &MainWindow::insertUserFunctionIntoEditor);
    connect(m_docks.userFunctions->widget(), &UserFunctionListWidget::userFunctionEdited,
            this, &MainWindow::insertUserFunctionIntoEditor);
    connect(this, &MainWindow::radixCharacterChanged,
            m_docks.userFunctions->widget(), &UserFunctionListWidget::updateList);
    connect(this, &MainWindow::functionsChanged,
            m_docks.userFunctions->widget(), &UserFunctionListWidget::updateList);

    addTabifiedDock(m_docks.userFunctions, takeFocus);
    m_settings->userFunctionsDockVisible = true;
}

void MainWindow::addTabifiedDock(QDockWidget* newDock, bool takeFocus, Qt::DockWidgetArea area)
{
    connect(newDock, &QDockWidget::visibilityChanged, this, &MainWindow::handleDockWidgetVisibilityChanged);
    addDockWidget(area, newDock);
    // Try to find an existing dock we can tabify with.
    const auto allDocks = m_allDocks; // TODO: Use Qt 5.7's qAsConst().
    for (auto& d : allDocks) {
        if (dockWidgetArea(d) == area)
            tabifyDockWidget(d, newDock);
    }
    m_allDocks.append(newDock);
    newDock->show();
    newDock->raise();
    if (takeFocus)
        newDock->setFocus();
}

void MainWindow::deleteDock(QDockWidget* dock)
{
    removeDockWidget(dock);
    m_allDocks.removeAll(dock);
    disconnect(dock);
    dock->deleteLater();
}

void MainWindow::createFixedConnections()
{
    connect(m_actions.sessionExportHtml, SIGNAL(triggered()), SLOT(exportHtml()));
    connect(m_actions.sessionExportPlainText, SIGNAL(triggered()), SLOT(exportPlainText()));
    connect(m_actions.sessionImport, SIGNAL(triggered()), SLOT(showSessionImportDialog()));
    connect(m_actions.sessionLoad, SIGNAL(triggered()), SLOT(showSessionLoadDialog()));
    connect(m_actions.sessionQuit, SIGNAL(triggered()), SLOT(close()));
    connect(m_actions.sessionSave, SIGNAL(triggered()), SLOT(saveSessionDialog()));

    connect(m_actions.editClearExpression, SIGNAL(triggered()), SLOT(clearEditorAndBitfield()));
    connect(m_actions.editClearHistory, SIGNAL(triggered()), SLOT(clearHistory()));
    connect(m_actions.editCopyLastResult, SIGNAL(triggered()), SLOT(copyResultToClipboard()));
    connect(m_actions.editCopy, SIGNAL(triggered()), SLOT(copy()));
    connect(m_actions.editPaste, SIGNAL(triggered()), m_widgets.editor, SLOT(paste()));
    connect(m_actions.editSelectExpression, SIGNAL(triggered()), SLOT(selectEditorExpression()));
    connect(m_actions.editWrapSelection, SIGNAL(triggered()), SLOT(wrapSelection()));

    connect(m_actions.viewFullScreenMode, SIGNAL(toggled(bool)), SLOT(setFullScreenEnabled(bool)));
    connect(m_actions.viewKeypad, SIGNAL(toggled(bool)), SLOT(setKeypadVisible(bool)));
    connect(m_actions.viewStatusBar, SIGNAL(toggled(bool)), SLOT(setStatusBarVisible(bool)));
#if !defined(Q_OS_MACOS)
    connect(m_actions.viewMenuBar, SIGNAL(toggled(bool)), SLOT(setMenuBarVisible(bool)));
#endif
    connect(m_actions.viewBitfield, SIGNAL(toggled(bool)), SLOT(setBitfieldVisible(bool)));

    connect(m_actions.viewConstants, SIGNAL(triggered(bool)), SLOT(setConstantsDockVisible(bool)));
    connect(m_actions.viewFunctions, SIGNAL(triggered(bool)), SLOT(setFunctionsDockVisible(bool)));
    connect(m_actions.viewHistory, SIGNAL(triggered(bool)), SLOT(setHistoryDockVisible(bool)));
    connect(m_actions.viewFormulaBook, SIGNAL(triggered(bool)), SLOT(setFormulaBookDockVisible(bool)));
    connect(m_actions.viewVariables, SIGNAL(triggered(bool)), SLOT(setVariablesDockVisible(bool)));
    connect(m_actions.viewUserFunctions, SIGNAL(triggered(bool)), SLOT(setUserFunctionsDockVisible(bool)));

    connect(m_actions.settingsAngleUnitDegree, SIGNAL(triggered()), SLOT(setAngleModeDegree()));
    connect(m_actions.settingsAngleUnitRadian, SIGNAL(triggered()), SLOT(setAngleModeRadian()));
    connect(m_actions.settingsAngleUnitGradian, SIGNAL(triggered()), SLOT(setAngleModeGradian()));
    connect(m_actions.settingsAngleUnitCycle, SIGNAL(triggered()), SLOT(cycleAngleUnits()));

    connect(m_actions.settingsBehaviorAlwaysOnTop, SIGNAL(toggled(bool)), SLOT(setAlwaysOnTopEnabled(bool)));
    connect(m_actions.settingsBehaviorAutoCompletion, SIGNAL(toggled(bool)), SLOT(setAutoCompletionEnabled(bool)));
    connect(m_actions.settingsBehaviorAutoAns, SIGNAL(toggled(bool)), SLOT(setAutoAnsEnabled(bool)));
    connect(m_actions.settingsBehaviorPartialResults, SIGNAL(toggled(bool)), SLOT(setAutoCalcEnabled(bool)));
    connect(m_actionGroups.historySaving, SIGNAL(triggered(QAction*)), SLOT(setHistorySaving(QAction*)));
    connect(m_actions.settingsBehaviorHistorySizeLimit, SIGNAL(triggered()), SLOT(setHistorySizeLimit()));
    connect(m_actions.settingsBehaviorSaveWindowPositionOnExit, SIGNAL(toggled(bool)), SLOT(setWindowPositionSaveEnabled(bool)));
    connect(m_actions.settingsBehaviorSyntaxHighlighting, SIGNAL(toggled(bool)), SLOT(setSyntaxHighlightingEnabled(bool)));
    connect(m_actions.settingsBehaviorHoverHighlightResults, SIGNAL(toggled(bool)), SLOT(setHoverHighlightResultsEnabled(bool)));
    connect(m_actionGroups.digitGrouping, SIGNAL(triggered(QAction*)), SLOT(setDigitGrouping(QAction*)));
    connect(m_actions.settingsBehaviorDigitGroupingIntegerPartOnly, SIGNAL(toggled(bool)), SLOT(setDigitGroupingIntegerPartOnlyEnabled(bool)));
    connect(m_actions.settingsBehaviorLeaveLastExpression, SIGNAL(toggled(bool)), SLOT(setLeaveLastExpressionEnabled(bool)));
    connect(m_actions.settingsBehaviorAutoResultToClipboard, SIGNAL(toggled(bool)), SLOT(setAutoResultToClipboardEnabled(bool)));
    connect(m_actions.settingsBehaviorComplexNumbers, SIGNAL(toggled(bool)), SLOT(setComplexNumbers(bool)));

    connect(m_actions.settingsRadixCharComma, SIGNAL(triggered()), SLOT(setRadixCharacterComma()));
    connect(m_actions.settingsRadixCharDefault, SIGNAL(triggered()), SLOT(setRadixCharacterAutomatic()));
    connect(m_actions.settingsRadixCharDot, SIGNAL(triggered()), SLOT(setRadixCharacterDot()));
    connect(m_actions.settingsRadixCharBoth, SIGNAL(triggered()), SLOT(setRadixCharacterBoth()));

    connect(m_actions.settingsResultFormat0Digits, &QAction::triggered, [this]() { this->setResultPrecision(0); });
    connect(m_actions.settingsResultFormat15Digits, SIGNAL(triggered()), SLOT(setResultPrecision15Digits()));
    connect(m_actions.settingsResultFormat2Digits, SIGNAL(triggered()), SLOT(setResultPrecision2Digits()));
    connect(m_actions.settingsResultFormat3Digits, SIGNAL(triggered()), SLOT(setResultPrecision3Digits()));
    connect(m_actions.settingsResultFormat50Digits, SIGNAL(triggered()), SLOT(setResultPrecision50Digits()));
    connect(m_actions.settingsResultFormat8Digits, SIGNAL(triggered()), SLOT(setResultPrecision8Digits()));
    connect(m_actions.settingsResultFormatAutoPrecision, SIGNAL(triggered()), SLOT(setResultPrecisionAutomatic()));
    connect(m_actions.settingsResultFormatBinary, SIGNAL(triggered()), SLOT(setResultFormatBinary()));
    connect(m_actions.settingsResultFormatCartesian, SIGNAL(triggered()), SLOT(setResultFormatCartesian()));
    connect(m_actions.settingsResultFormatEngineering, SIGNAL(triggered()), SLOT(setResultFormatEngineering()));
    connect(m_actions.settingsResultFormatFixed, SIGNAL(triggered()), SLOT(setResultFormatFixed()));
    connect(m_actions.settingsResultFormatGeneral, SIGNAL(triggered()), SLOT(setResultFormatGeneral()));
    connect(m_actions.settingsResultFormatHexadecimal, SIGNAL(triggered()), SLOT(setResultFormatHexadecimal()));
    connect(m_actions.settingsResultFormatOctal, SIGNAL(triggered()), SLOT(setResultFormatOctal()));
    connect(m_actions.settingsResultFormatPolar, SIGNAL(triggered()), SLOT(setResultFormatPolar()));
    connect(m_actions.settingsResultFormatSexagesimal, SIGNAL(triggered()), SLOT(setResultFormatSexagesimal()));
    connect(m_actions.settingsResultFormatScientific, SIGNAL(triggered()), SLOT(setResultFormatScientific()));
    connect(m_actions.settingsAlternativeResultFormatDisabled, SIGNAL(triggered()), SLOT(setAlternativeResultFormatDisabled()));
    connect(m_actions.settingsAlternativeResultFormatGeneral, SIGNAL(triggered()), SLOT(setAlternativeResultFormatGeneral()));
    connect(m_actions.settingsAlternativeResultFormatFixed, SIGNAL(triggered()), SLOT(setAlternativeResultFormatFixed()));
    connect(m_actions.settingsAlternativeResultFormatEngineering, SIGNAL(triggered()), SLOT(setAlternativeResultFormatEngineering()));
    connect(m_actions.settingsAlternativeResultFormatScientific, SIGNAL(triggered()), SLOT(setAlternativeResultFormatScientific()));
    connect(m_actions.settingsAlternativeResultFormatBinary, SIGNAL(triggered()), SLOT(setAlternativeResultFormatBinary()));
    connect(m_actions.settingsAlternativeResultFormatOctal, SIGNAL(triggered()), SLOT(setAlternativeResultFormatOctal()));
    connect(m_actions.settingsAlternativeResultFormatHexadecimal, SIGNAL(triggered()), SLOT(setAlternativeResultFormatHexadecimal()));
    connect(m_actions.settingsAlternativeResultFormatSexagesimal, SIGNAL(triggered()), SLOT(setAlternativeResultFormatSexagesimal()));
    connect(m_actions.settingsTertiaryResultFormatDisabled, SIGNAL(triggered()), SLOT(setTertiaryResultFormatDisabled()));
    connect(m_actions.settingsTertiaryResultFormatGeneral, SIGNAL(triggered()), SLOT(setTertiaryResultFormatGeneral()));
    connect(m_actions.settingsTertiaryResultFormatFixed, SIGNAL(triggered()), SLOT(setTertiaryResultFormatFixed()));
    connect(m_actions.settingsTertiaryResultFormatEngineering, SIGNAL(triggered()), SLOT(setTertiaryResultFormatEngineering()));
    connect(m_actions.settingsTertiaryResultFormatScientific, SIGNAL(triggered()), SLOT(setTertiaryResultFormatScientific()));
    connect(m_actions.settingsTertiaryResultFormatBinary, SIGNAL(triggered()), SLOT(setTertiaryResultFormatBinary()));
    connect(m_actions.settingsTertiaryResultFormatOctal, SIGNAL(triggered()), SLOT(setTertiaryResultFormatOctal()));
    connect(m_actions.settingsTertiaryResultFormatHexadecimal, SIGNAL(triggered()), SLOT(setTertiaryResultFormatHexadecimal()));
    connect(m_actions.settingsTertiaryResultFormatSexagesimal, SIGNAL(triggered()), SLOT(setTertiaryResultFormatSexagesimal()));

    connect(m_actions.settingsLanguage, SIGNAL(triggered()), SLOT(showLanguageChooserDialog()));

    connect(m_actions.helpManual, SIGNAL(triggered()), SLOT(showManualWindow()));
    connect(m_actions.contextHelp, SIGNAL(triggered()), SLOT(showContextHelp()));
    connect(m_actions.helpUpdates, SIGNAL(triggered()), SLOT(openUpdatesURL()));
    connect(m_actions.helpFeedback, SIGNAL(triggered()), SLOT(openFeedbackURL()));
    connect(m_actions.helpCommunity, SIGNAL(triggered()), SLOT(openCommunityURL()));
    connect(m_actions.helpNews, SIGNAL(triggered()), SLOT(openNewsURL()));
    connect(m_actions.helpDonate, SIGNAL(triggered()), SLOT(openDonateURL()));
    connect(m_actions.helpAbout, SIGNAL(triggered()), SLOT(showAboutDialog()));

    connect(m_widgets.editor, SIGNAL(autoCalcDisabled()), SLOT(hideStateLabel()));
    connect(m_widgets.editor, SIGNAL(autoCalcMessageAvailable(const QString&)), SLOT(handleAutoCalcMessageAvailable(const QString&)));
    connect(m_widgets.editor, SIGNAL(autoCalcQuantityAvailable(const Quantity&)), SLOT(handleAutoCalcQuantityAvailable(const Quantity&)));
    connect(m_widgets.editor, SIGNAL(returnPressed()), SLOT(evaluateEditorExpression()));
    connect(m_widgets.editor, SIGNAL(escapePressed()), SLOT(cancelHistoryEntryEdit()));
    connect(m_widgets.editor, SIGNAL(shiftDownPressed()), SLOT(decreaseDisplayFontPointSize()));
    connect(m_widgets.editor, SIGNAL(shiftUpPressed()), SLOT(increaseDisplayFontPointSize()));
    connect(m_widgets.editor, SIGNAL(controlPageUpPressed()), m_widgets.display, SLOT(scrollToTop()));
    connect(m_widgets.editor, SIGNAL(controlPageDownPressed()), m_widgets.display, SLOT(scrollToBottom()));
    connect(m_widgets.editor, SIGNAL(shiftPageUpPressed()), m_widgets.display, SLOT(scrollLineUp()));
    connect(m_widgets.editor, SIGNAL(shiftPageDownPressed()), m_widgets.display, SLOT(scrollLineDown()));
    connect(m_widgets.editor, SIGNAL(pageUpPressed()), m_widgets.display, SLOT(scrollPageUp()));
    connect(m_widgets.editor, SIGNAL(pageDownPressed()), m_widgets.display, SLOT(scrollPageDown()));
    connect(m_widgets.editor, SIGNAL(textChanged()), SLOT(handleEditorTextChange()));
    connect(m_widgets.editor, SIGNAL(copyAvailable(bool)), SLOT(handleCopyAvailable(bool)));
    connect(m_widgets.editor, SIGNAL(copySequencePressed()), SLOT(copy()));
    connect(m_widgets.editor, SIGNAL(selectionChanged()), SLOT(handleEditorSelectionChange()));
    connect(this, SIGNAL(historyChanged()), m_widgets.editor, SLOT(updateHistory()));

    connect(m_widgets.display, SIGNAL(copyAvailable(bool)), SLOT(handleCopyAvailable(bool)));
    connect(m_widgets.display, SIGNAL(expressionSelected(const QString&)), SLOT(insertTextIntoEditor(const QString&)));
    connect(m_widgets.display, SIGNAL(editHistoryEntryRequested(int)), SLOT(startHistoryEntryEdit(int)));
    connect(m_widgets.display, SIGNAL(cancelHistoryEditRequested()), SLOT(cancelHistoryEntryEdit()));
    connect(m_widgets.display, SIGNAL(removeHistoryEntryRequested(int)), SLOT(removeHistoryEntryAt(int)));
    connect(m_widgets.display, SIGNAL(removeHistoryEntriesAboveRequested(int)), SLOT(removeHistoryEntriesAbove(int)));
    connect(m_widgets.display, SIGNAL(removeHistoryEntriesBelowRequested(int)), SLOT(removeHistoryEntriesBelow(int)));
    connect(m_widgets.display, SIGNAL(selectionChanged()), SLOT(handleDisplaySelectionChange()));
    connect(m_widgets.display, SIGNAL(shiftWheelUp()), SLOT(increaseDisplayFontPointSize()));
    connect(m_widgets.display, SIGNAL(shiftWheelDown()), SLOT(decreaseDisplayFontPointSize()));
    connect(m_widgets.display, SIGNAL(controlWheelUp()), SLOT(increaseDisplayFontPointSize()));
    connect(m_widgets.display, SIGNAL(controlWheelDown()), SLOT(decreaseDisplayFontPointSize()));
    connect(m_widgets.display, SIGNAL(shiftControlWheelDown()), SLOT(decreaseOpacity()));
    connect(m_widgets.display, SIGNAL(shiftControlWheelUp()), SLOT(increaseOpacity()));
    connect(this, SIGNAL(historyChanged()), m_widgets.display, SLOT(refresh()));

    connect(this, SIGNAL(radixCharacterChanged()), m_widgets.display, SLOT(refresh()));
    connect(this, SIGNAL(radixCharacterChanged()), m_widgets.editor, SLOT(refreshAutoCalc()));
    connect(this, SIGNAL(angleUnitChanged()), m_widgets.display, SLOT(refreshLastHistoryEntry()));
    connect(this, SIGNAL(angleUnitChanged()), m_widgets.editor, SLOT(refreshAutoCalc()));
    connect(this, SIGNAL(complexNumbersChanged()), m_widgets.display, SLOT(refreshLastHistoryEntry()));
    connect(this, SIGNAL(complexNumbersChanged()), m_widgets.editor, SLOT(refreshAutoCalc()));
    connect(this, SIGNAL(resultFormatChanged()), m_widgets.display, SLOT(refreshLastHistoryEntry()));
    connect(this, SIGNAL(resultFormatChanged()), m_widgets.editor, SLOT(refreshAutoCalc()));
    connect(this, SIGNAL(resultPrecisionChanged()), m_widgets.display, SLOT(refreshLastHistoryEntry()));
    connect(this, SIGNAL(resultPrecisionChanged()), m_widgets.editor, SLOT(refreshAutoCalc()));
    connect(this, SIGNAL(colorSchemeChanged()), m_widgets.display, SLOT(rehighlight()));
    connect(this, SIGNAL(colorSchemeChanged()), m_widgets.editor, SLOT(rehighlight()));
    connect(m_actionGroups.colorScheme, &QActionGroup::hovered, this, &MainWindow::applyColorSchemeFromAction);
    connect(m_menus.colorScheme, &QMenu::aboutToHide, this, &MainWindow::revertColorScheme);
    connect(m_menus.colorScheme, &QMenu::aboutToShow, this, &MainWindow::saveColorSchemeToRevert);
    connect(this, SIGNAL(syntaxHighlightingChanged()), m_widgets.display, SLOT(rehighlight()));
    connect(this, SIGNAL(syntaxHighlightingChanged()), m_widgets.editor, SLOT(rehighlight()));

    connect(m_actions.settingsDisplayFont, SIGNAL(triggered()), SLOT(showFontDialog()));

    const auto schemes = m_actions.settingsDisplayColorSchemes;
    for (auto& action : schemes) // TODO: Use Qt 5.7's qAsConst();
        connect(action, SIGNAL(triggered()), SLOT(applySelectedColorScheme()));

    connect(this, SIGNAL(languageChanged()), SLOT(retranslateText()));
}

void MainWindow::applySettings()
{
    emit languageChanged();

    setFormulaBookDockVisible(m_settings->formulaBookDockVisible, false);
    m_actions.viewFormulaBook->setChecked(m_settings->formulaBookDockVisible);

    setConstantsDockVisible(m_settings->constantsDockVisible, false);
    m_actions.viewConstants->setChecked(m_settings->constantsDockVisible);

    setFunctionsDockVisible(m_settings->functionsDockVisible, false);
    m_actions.viewFunctions->setChecked(m_settings->functionsDockVisible);

    setHistoryDockVisible(m_settings->historyDockVisible, false);
    m_actions.viewHistory->setChecked(m_settings->historyDockVisible);

    setVariablesDockVisible(m_settings->variablesDockVisible, false);
    m_actions.viewVariables->setChecked(m_settings->variablesDockVisible);

    setUserFunctionsDockVisible(m_settings->userFunctionsDockVisible, false);
    m_actions.viewUserFunctions->setChecked(m_settings->userFunctionsDockVisible);

    m_actions.viewBitfield->setChecked(m_settings->bitfieldVisible);
    m_actions.viewKeypad->setChecked(m_settings->keypadVisible);
    m_actions.viewStatusBar->setChecked(m_settings->statusBarVisible);
#if !defined(Q_OS_MACOS)
    setMenuBarVisible(m_settings->menuBarVisible);
    m_actions.viewMenuBar->setChecked(m_settings->menuBarVisible);
#endif

    if (!restoreGeometry(m_settings->windowGeometry)) {
        // We couldn't restore the saved geometry; that means it was either empty
        // or just isn't valid anymore so we use default size and position.
        resize(640, 480);
        QRect screenGeometry = QGuiApplication::primaryScreen()->availableGeometry();
        move(screenGeometry.center() - rect().center());
    }
    restoreState(m_settings->windowState);

    m_actions.viewFullScreenMode->setChecked(m_settings->windowOnfullScreen);
    m_actions.settingsBehaviorAlwaysOnTop->setChecked(m_settings->windowAlwaysOnTop);

    if (m_settings->angleUnit == 'r')
        m_actions.settingsAngleUnitRadian->setChecked(true);
    else if (m_settings->angleUnit == 'd')
        m_actions.settingsAngleUnitDegree->setChecked(true);
    else if (m_settings->angleUnit == 'g')
        m_actions.settingsAngleUnitGradian->setChecked(true);

    if (m_settings->historySaving == Settings::HistorySavingNever) {
        m_actions.settingsBehaviorHistorySavingNever->setChecked(true);
    } else if (m_settings->historySaving == Settings::HistorySavingContinuously) {
        m_actions.settingsBehaviorHistorySavingContinuously->setChecked(true);
    } else {
        m_actions.settingsBehaviorHistorySavingOnExit->setChecked(true);
    }

    if (m_settings->historySaving != Settings::HistorySavingNever)
        restoreSession();

    m_actions.settingsBehaviorLeaveLastExpression->setChecked(m_settings->leaveLastExpression);
    m_actions.settingsBehaviorSaveWindowPositionOnExit->setChecked(m_settings->windowPositionSave);


    checkInitialResultFormat();
    checkInitialAlternativeResultFormat();
    checkInitialTertiaryResultFormat();
    checkInitialResultPrecision();
    checkInitialComplexFormat();

    if (m_settings->isRadixCharacterAuto())
        m_actions.settingsRadixCharDefault->setChecked(true);
    else if (m_settings->isRadixCharacterBoth())
        m_actions.settingsRadixCharBoth->setChecked(true);
    else if (m_settings->radixCharacter() == '.')
        m_actions.settingsRadixCharDot->setChecked(true);
    else if (m_settings->radixCharacter() == ',')
        m_actions.settingsRadixCharComma->setChecked(true);

    if (m_settings->autoAns)
        m_actions.settingsBehaviorAutoAns->setChecked(true);
    else
        setAutoAnsEnabled(false);

    if (m_settings->autoCalc)
        m_actions.settingsBehaviorPartialResults->setChecked(true);
    else
        setAutoCalcEnabled(false);

    if (m_settings->autoCompletion)
        m_actions.settingsBehaviorAutoCompletion->setChecked(true);
    else
        setAutoCompletionEnabled(false);

    checkInitialDigitGrouping();

    if (m_settings->syntaxHighlighting)
        m_actions.settingsBehaviorSyntaxHighlighting->setChecked(true);
    else
        setSyntaxHighlightingEnabled(false);

    if (m_settings->hoverHighlightResults)
        m_actions.settingsBehaviorHoverHighlightResults->setChecked(true);
    else
        setHoverHighlightResultsEnabled(false);

    if (m_settings->autoResultToClipboard)
        m_actions.settingsBehaviorAutoResultToClipboard->setChecked(true);
    else
        setAutoResultToClipboardEnabled(false);

    m_actions.settingsBehaviorDigitGroupingIntegerPartOnly->setChecked(m_settings->digitGroupingIntegerPartOnly);

    m_actions.settingsBehaviorComplexNumbers->setChecked(m_settings->complexNumbers);

    QFont font;
    font.fromString(m_settings->displayFont);
    m_widgets.display->setFont(font);
    m_widgets.editor->setFont(font);

    m_widgets.display->scrollToBottom();

    const auto schemes = m_actions.settingsDisplayColorSchemes;
    for (auto& action : schemes) {
        if (m_settings->colorScheme == action->data().toString())
            action->setChecked(true);
    }

    if (m_widgets.display->isEmpty())
        QTimer::singleShot(0, this, SLOT(showReadyMessage()));
}

void MainWindow::showManualWindow()
{
    if (m_widgets.manual) {
        m_widgets.manual->raise();
        m_widgets.manual->activateWindow();
        return;
    }

    m_widgets.manual = new ManualWindow();
    if (!m_widgets.manual->restoreGeometry(m_settings->manualWindowGeometry))
        m_widgets.manual->resize(640, 480);
    m_widgets.manual->show();
    connect(m_widgets.manual, SIGNAL(windowClosed()), SLOT(handleManualClosed()));
}

void MainWindow::showContextHelp()
{
    QString kw = "";
    if(m_widgets.editor->hasFocus()) {
        kw = m_widgets.editor->getKeyword();
        if (kw != "") {
            auto url = m_manualServer->urlForKeyword(kw);
            if (url.isValid()) {
                showManualWindow();
                m_widgets.manual->openPage(url);
            }
        }
    }
}

void MainWindow::showReadyMessage()
{
    showStateLabel(tr("Type an expression here"));
}

void MainWindow::checkInitialResultFormat()
{
    switch (m_settings->resultFormat) {
        case 'g': m_actions.settingsResultFormatGeneral->setChecked(true); break;
        case 'n': m_actions.settingsResultFormatEngineering->setChecked(true); break;
        case 'e': m_actions.settingsResultFormatScientific->setChecked(true); break;
        case 'h': m_actions.settingsResultFormatHexadecimal->setChecked(true); break;
        case 'o': m_actions.settingsResultFormatOctal->setChecked(true); break;
        case 'b': m_actions.settingsResultFormatBinary->setChecked(true); break;
        case 's': m_actions.settingsResultFormatSexagesimal->setChecked(true); break;
        default : m_actions.settingsResultFormatFixed->setChecked(true);
    }
}

void MainWindow::checkInitialAlternativeResultFormat()
{
    switch (m_settings->alternativeResultFormat) {
        case 'g': m_actions.settingsAlternativeResultFormatGeneral->setChecked(true); break;
        case 'n': m_actions.settingsAlternativeResultFormatEngineering->setChecked(true); break;
        case 'e': m_actions.settingsAlternativeResultFormatScientific->setChecked(true); break;
        case 'h': m_actions.settingsAlternativeResultFormatHexadecimal->setChecked(true); break;
        case 'o': m_actions.settingsAlternativeResultFormatOctal->setChecked(true); break;
        case 'b': m_actions.settingsAlternativeResultFormatBinary->setChecked(true); break;
        case 's': m_actions.settingsAlternativeResultFormatSexagesimal->setChecked(true); break;
        case 'f': m_actions.settingsAlternativeResultFormatFixed->setChecked(true); break;
        default : m_actions.settingsAlternativeResultFormatDisabled->setChecked(true);
    }
}

void MainWindow::checkInitialTertiaryResultFormat()
{
    switch (m_settings->tertiaryResultFormat) {
        case 'g': m_actions.settingsTertiaryResultFormatGeneral->setChecked(true); break;
        case 'n': m_actions.settingsTertiaryResultFormatEngineering->setChecked(true); break;
        case 'e': m_actions.settingsTertiaryResultFormatScientific->setChecked(true); break;
        case 'h': m_actions.settingsTertiaryResultFormatHexadecimal->setChecked(true); break;
        case 'o': m_actions.settingsTertiaryResultFormatOctal->setChecked(true); break;
        case 'b': m_actions.settingsTertiaryResultFormatBinary->setChecked(true); break;
        case 's': m_actions.settingsTertiaryResultFormatSexagesimal->setChecked(true); break;
        case 'f': m_actions.settingsTertiaryResultFormatFixed->setChecked(true); break;
        default : m_actions.settingsTertiaryResultFormatDisabled->setChecked(true);
    }
}

void MainWindow::checkInitialComplexFormat()
{
    switch (m_settings->resultFormatComplex) {
    case 'c':
        m_actions.settingsResultFormatCartesian->setChecked(true);
        break;
    case 'p':
        m_actions.settingsResultFormatPolar->setChecked(true);
        break;
    }
}

void MainWindow::checkInitialResultPrecision()
{
    switch (m_settings->resultPrecision) {
        case 0: m_actions.settingsResultFormat0Digits->setChecked(true); break;
        case 2: m_actions.settingsResultFormat2Digits->setChecked(true); break;
        case 3: m_actions.settingsResultFormat3Digits->setChecked(true); break;
        case 8: m_actions.settingsResultFormat8Digits->setChecked(true); break;
        case 15: m_actions.settingsResultFormat15Digits->setChecked(true); break;
        case 50: m_actions.settingsResultFormat50Digits->setChecked(true); break;
        default: m_actions.settingsResultFormatAutoPrecision->setChecked(true);
    }
}

void MainWindow::checkInitialDigitGrouping()
{
    switch (m_settings->digitGrouping) {
        case 1: m_actions.settingsBehaviorDigitGroupingOneSpace->setChecked(true); break;
        case 2: m_actions.settingsBehaviorDigitGroupingTwoSpaces->setChecked(true); break;
        case 3: m_actions.settingsBehaviorDigitGroupingThreeSpaces->setChecked(true); break;
        default:
        case 0: m_actions.settingsBehaviorDigitGroupingNone->setChecked(true); break;
    }
}



void MainWindow::saveSettings()
{
    m_settings->windowGeometry = m_settings->windowPositionSave ? saveGeometry() : QByteArray();
    if (m_widgets.manual)
        m_settings->manualWindowGeometry = m_settings->windowPositionSave ? m_widgets.manual->saveGeometry() : QByteArray();
    m_settings->windowState = saveState();
    m_settings->displayFont = m_widgets.display->font().toString();

    m_settings->save();
}

void MainWindow::saveSession(QString & fname)
{
    QFile file(fname);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }


    QJsonObject json;
    m_session->serialize(json);
    QJsonDocument doc(json);
    file.write(doc.toJson());

    file.close();
}

MainWindow::MainWindow()
    : QMainWindow()
{

    m_session = new Session();
    m_constants = Constants::instance();
    m_evaluator = Evaluator::instance();
    m_functions = FunctionRepo::instance();
    m_evaluator->setSession(m_session);
    m_evaluator->initializeBuiltInVariables();

    m_translator = 0;
    m_settings = Settings::instance();
    DMath::complexMode = m_settings->complexNumbers;

    m_widgets.manual = 0;
    m_widgets.keypad  = 0;

    m_conditions.autoAns = false;

    m_docks.book = 0;
    m_docks.history = 0;
    m_docks.constants = 0;
    m_docks.functions = 0;
    m_docks.variables = 0;
    m_docks.userFunctions = 0;

    m_status.angleUnit = 0;
    m_status.angleUnitSection = 0;
    m_status.angleUnitLabel = 0;
    m_status.resultFormat = 0;
    m_status.resultFormatSection = 0;
    m_status.resultFormatLabel = 0;
    m_status.complexNumbers = 0;
    m_status.complexNumbersSection = 0;
    m_status.complexNumbersLabel = 0;
    m_status.complexFormat = 0;
    m_status.complexFormatSection = 0;
    m_status.complexFormatLabel = 0;

    m_copyWidget = 0;
    m_pendingHistoryEditIndex = -1;

    createUi();
    applySettings();

    m_manualServer = ManualServer::instance();
    connect(this, SIGNAL(languageChanged()), m_manualServer, SLOT(ensureCorrectLanguage()));
}

MainWindow::~MainWindow()
{
    if (m_docks.book)
        deleteBookDock();
    if (m_docks.constants)
        deleteConstantsDock();
    if (m_docks.variables)
        deleteVariablesDock();
    if (m_docks.userFunctions)
        deleteUserFunctionsDock();
    if (m_docks.functions)
        deleteFunctionsDock();
    if (m_docks.history)
        deleteHistoryDock();
    delete m_session;
}

void MainWindow::showAboutDialog()
{
    AboutBox dialog(this);
    dialog.resize(480, 640);
    dialog.exec();
}

void MainWindow::clearHistory()
{
    if (m_session->historyToList().isEmpty())
        return;

    const QMessageBox::StandardButton confirmation = QMessageBox::question(
        this,
        tr("Clear History"),
        tr("Are you sure you want to clear the calculation history?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (confirmation != QMessageBox::Yes)
        return;

    m_session->clearHistory();
    m_pendingHistoryEditIndex = -1;
    m_widgets.display->setEditingHistoryIndex(-1);
    clearEditorAndBitfield();
    emit historyChanged();

    m_conditions.autoAns = false;
}

void MainWindow::clearEditor()
{
    m_widgets.editor->clear();
    m_widgets.editor->setFocus();
}

void MainWindow::clearEditorAndBitfield()
{
    clearEditor();
    if (m_widgets.bitField)
        m_widgets.bitField->clear();
}

void MainWindow::copyResultToClipboard()
{
    QClipboard* cb = QApplication::clipboard();
    Quantity q = m_evaluator->getVariable(QLatin1String("ans")).value();
    QString strToCopy(NumberFormatter::format(q));
    strToCopy.replace(QChar(0x2212), QChar('-'));
    cb->setText(strToCopy, QClipboard::Clipboard);
}

void MainWindow::decreaseOpacity()
{
    if (windowOpacity() > 0.4)
        setWindowOpacity(windowOpacity() - 0.1);
}

void MainWindow::increaseOpacity()
{
    if (windowOpacity() < 1.0)
        setWindowOpacity(windowOpacity() + 0.1);
}

void MainWindow::deleteVariables()
{
    m_session->clearVariables();

    if (m_settings->variablesDockVisible)
        m_docks.variables->widget()->updateList();
}

void MainWindow::deleteUserFunctions()
{
    m_session->clearUserFunctions();

    if (m_settings->userFunctionsDockVisible)
        m_docks.userFunctions->widget()->updateList();
}

void MainWindow::setResultPrecision2Digits()
{
    setResultPrecision(2);
}

void MainWindow::setResultPrecision3Digits()
{
    setResultPrecision(3);
}

void MainWindow::setResultPrecision8Digits()
{
    setResultPrecision(8);
}

void MainWindow::setResultPrecision15Digits()
{
    setResultPrecision(15);
}

void MainWindow::setResultPrecision50Digits()
{
    setResultPrecision(50);
}

void MainWindow::setResultPrecisionAutomatic()
{
    setResultPrecision(-1);
}

void MainWindow::applySelectedColorScheme()
{
    m_settings->colorScheme = m_actionGroups.colorScheme->checkedAction()->data().toString();
    emit colorSchemeChanged();
}

void MainWindow::applyColorSchemeFromAction(QAction* action)
{
    m_settings->colorScheme = action->data().toString();
    emit colorSchemeChanged();
}

void MainWindow::saveColorSchemeToRevert()
{
    m_colorSchemeToRevert = m_settings->colorScheme;
}

void MainWindow::revertColorScheme()
{
    m_settings->colorScheme = m_colorSchemeToRevert;
    emit colorSchemeChanged();
}

void MainWindow::selectEditorExpression()
{
    activateWindow();
    m_widgets.editor->selectAll();
    m_widgets.editor->setFocus();
}

void MainWindow::hideStateLabel()
{
    m_widgets.state->hide();
}

void MainWindow::showSessionLoadDialog()
{
    QString filters = tr("SpeedCrunch Sessions (*.json);;All Files (*)");
    QString fname = QFileDialog::getOpenFileName(this, tr("Load Session"), QString(), filters);
    if (fname.isEmpty())
        return;

    QFile file(fname);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't read from file %1").arg(fname));
        return;
    }

    // Ask for merge with current session.
    bool merge;
    QString mergeMsg = tr(
        "Merge session being loaded with current session?\n"
        "If no, current variables and display will be cleared."
    );
    QMessageBox::StandardButton button =
        QMessageBox::question(this, tr("Merge?"), mergeMsg,
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);

    if (button == QMessageBox::Yes)
        merge = true;
    else if (button == QMessageBox::No)
        merge = false;
    else return;

    QByteArray data = file.readAll();
    QJsonDocument doc(QJsonDocument::fromJson(data));
    m_session->deSerialize(doc.object(), merge);

    file.close();
    emit historyChanged();
    emit variablesChanged();
    emit functionsChanged();

}

void MainWindow::wrapSelection()
{
    m_widgets.editor->wrapSelection();
}

void MainWindow::saveSessionDialog()
{
    QString filters = tr("SpeedCrunch Sessions (*.json);;All Files (*)");
    QString fname = QFileDialog::getSaveFileName(this, tr("Save Session"), QString(), filters);
    if (fname.isEmpty())
        return;

    QFile file(fname);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't write to file %1").arg(fname));
        return;
    }

    QJsonObject json;
    m_session->serialize(json);
    QJsonDocument doc(json);
    file.write(doc.toJson());

    file.close();
}

void MainWindow::showSessionImportDialog()
{
    QString filters = tr("All Files (*)");
    QString fname = QFileDialog::getOpenFileName(this, tr("Import Session"), QString(), filters);
    if (fname.isEmpty())
        return;

    QFile file(fname);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't read from file %1").arg(fname));
        return;
    }

    // Ask for merge with current session.
    QString mergeMsg = tr(
        "Merge session being imported with current session?\n"
        "If no, current variables and display will be cleared."
    );

    QMessageBox::StandardButton button =
        QMessageBox::question(this, tr("Merge?"), mergeMsg,
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);

    if (button == QMessageBox::Cancel)
        return;
    if (button == QMessageBox::No) {
        m_session->clearHistory();
        m_session->clearVariables();
        m_session->clearUserFunctions();
    }

    QTextStream stream(&file);
    QString exp = stream.readLine();
    bool ignoreAll = false;
    while (!exp.isNull()) {
        const QString normalizedExp =
            EditorUtils::normalizeExpressionOperators(exp);
        m_widgets.editor->setText(normalizedExp);

        QString str = m_evaluator->autoFix(normalizedExp);

        m_evaluator->setExpression(str);

        Quantity result = m_evaluator->evalUpdateAns();
        if (!m_evaluator->error().isEmpty()) {
            if (!ignoreAll) {
                QMessageBox::StandardButton button =
                    QMessageBox::warning(this, tr("Error"), tr("Ignore error?") + "\n" + m_evaluator->error(),
                        QMessageBox::Yes | QMessageBox::YesToAll
                        | QMessageBox::Cancel, QMessageBox::Yes);

                if (button == QMessageBox::Cancel)
                    return;
                if (button == QMessageBox::YesToAll)
                    ignoreAll = true;
            }
        } else {
            m_session->addHistoryEntry(HistoryEntry(normalizedExp, result));
            m_widgets.editor->setText(str);
            m_widgets.editor->selectAll();
            m_widgets.editor->stopAutoCalc();
            m_widgets.editor->stopAutoComplete();
            if(!result.isNan())
                m_conditions.autoAns = true;
        }

        exp = stream.readLine();
    }

    file.close();
    emit historyChanged();
    emit variablesChanged();
    emit functionsChanged();

    if (!isActiveWindow())
        activateWindow();
}

void MainWindow::setAlwaysOnTopEnabled(bool b)
{
    m_settings->windowAlwaysOnTop = b;

    QPoint cur = mapToGlobal(QPoint(0, 0));
    if (b)
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    else
        setWindowFlags(windowFlags() & (~ Qt::WindowStaysOnTopHint));
    move(cur);
    show();
}

void MainWindow::setAutoAnsEnabled(bool b)
{
    m_settings->autoAns = b;
}

void MainWindow::setAutoCalcEnabled(bool b)
{
    m_settings->autoCalc = b;
    m_widgets.editor->setAutoCalcEnabled(b);
}

void MainWindow::setHistorySaving(QAction* action)
{
    if (!action)
        return;
    m_settings->historySaving = static_cast<Settings::HistorySaving>(action->data().toInt());
}

void MainWindow::setHistorySizeLimit()
{
    bool ok = false;
    const int current = m_settings->maxHistoryEntries;
    const int value = QInputDialog::getInt(
        this,
        tr("History Size Limit"),
        tr("Maximum number of history entries (0 = unlimited):"),
        current,
        0,
        1000000,
        100,
        &ok);

    if (!ok || value == current)
        return;

    m_settings->maxHistoryEntries = value;
    m_session->applyHistoryLimit();
    m_conditions.autoAns = !m_session->historyToList().empty();
    emit historyChanged();
}

void MainWindow::setLeaveLastExpressionEnabled(bool b)
{
    m_settings->leaveLastExpression = b;
}

void MainWindow::setWindowPositionSaveEnabled(bool b)
{
    m_settings->windowPositionSave = b;
}

void MainWindow::setAutoCompletionEnabled(bool b)
{
    m_settings->autoCompletion = b;
    m_widgets.editor->setAutoCompletionEnabled(b);
}

void MainWindow::setBitfieldVisible(bool b)
{
    if (b)
        createBitField();
    else
        deleteBitField();
}

void MainWindow::setSyntaxHighlightingEnabled(bool b)
{
    m_settings->syntaxHighlighting = b;
    m_menus.digitGrouping->setEnabled(b);
    emit syntaxHighlightingChanged();
}

void MainWindow::setDigitGrouping(QAction *action)
{
    m_settings->digitGrouping = action->data().toInt();
    emit historyChanged();
    m_widgets.editor->refreshAutoCalc();
    emit syntaxHighlightingChanged();
}

void MainWindow::setDigitGroupingIntegerPartOnlyEnabled(bool b)
{
    m_settings->digitGroupingIntegerPartOnly = b;
    emit historyChanged();
    m_widgets.editor->refreshAutoCalc();
    emit syntaxHighlightingChanged();
}

void MainWindow::setAutoResultToClipboardEnabled(bool b)
{
    m_settings->autoResultToClipboard = b;
}

void MainWindow::setHoverHighlightResultsEnabled(bool b)
{
    m_settings->hoverHighlightResults = b;
    m_widgets.display->setHoverHighlightEnabled(b);
}

void MainWindow::setComplexNumbers(bool b)
{
    m_settings->complexNumbers = b;
    setStatusBarText();
    emit complexNumbersChanged();
    m_evaluator->initializeBuiltInVariables();
    DMath::complexMode = b;
}

void MainWindow::setAngleModeDegree()
{
    if (m_settings->angleUnit == 'd')
        return;

    m_settings->angleUnit = 'd';

    setStatusBarText();

    m_evaluator->initializeAngleUnits();
    emit angleUnitChanged();
}

void MainWindow::setAngleModeRadian()
{
    if (m_settings->angleUnit == 'r')
        return;

    m_settings->angleUnit = 'r';

    setStatusBarText();

    m_evaluator->initializeAngleUnits();
    emit angleUnitChanged();
}

void MainWindow::setAngleModeGradian()
{
    if (m_settings->angleUnit == 'g')
        return;

    m_settings->angleUnit = 'g';

    setStatusBarText();

    m_evaluator->initializeAngleUnits();
    emit angleUnitChanged();
}

inline static QString documentsLocation()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void MainWindow::exportHtml()
{
    QString fname = QFileDialog::getSaveFileName(this, tr("Export session as HTML"),
        documentsLocation(), tr("HTML file (*.html)"));

    if (fname.isEmpty())
        return;

    QFile file(fname);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't write to file %1").arg(fname));
        return;
    }

    QTextStream stream(& file);
    stream << m_widgets.display->exportHtml();

    file.close();
}

void MainWindow::exportPlainText()
{
    QString fname = QFileDialog::getSaveFileName(this, tr("Export session as plain text"),                                                 
                            documentsLocation(), tr("Text file (*.txt);;Any file (*.*)"));

    if (fname.isEmpty())
        return;

    QFile file(fname);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't write to file %1").arg(fname));
        return;
    }

    QByteArray text;
    QTextStream stream(&text, QIODevice::WriteOnly | QIODevice::Text);
    stream << m_widgets.display->document()->toPlainText();
    stream.flush();

    file.write(text);
    file.close();
}

void MainWindow::setWidgetsDirection()
{
    QLocale::Language lang = QLocale().language();
    bool rtlSystem = (lang == QLocale::Hebrew || lang == QLocale::Arabic || lang == QLocale::Persian);

    QString code = m_settings->language;
    bool rtlCustom = (code.contains("he") || code.contains("ar") || code.contains("fa"));

    if ((m_settings->language == "C" && rtlSystem) || rtlCustom)
        qApp->setLayoutDirection(Qt::RightToLeft);
    else
        qApp->setLayoutDirection(Qt::LeftToRight);
}

void MainWindow::showFontDialog()
{
    bool ok;
    QFont f = QFontDialog::getFont(&ok, m_widgets.display->font(), this, tr("Display font"));
    if (!ok)
        return;
    m_widgets.display->setFont(f);
    m_widgets.editor->setFont(f);
    if (m_widgets.state->isVisible())
        showStateLabel(m_widgets.state->text());
}

void MainWindow::setStatusBarVisible(bool b)
{
    b ? createStatusBar() : deleteStatusBar();
    m_settings->statusBarVisible = b;
}

void MainWindow::setMenuBarVisible(bool b)
{
    menuBar()->setVisible(b);
    m_settings->menuBarVisible = b;
}

void MainWindow::showStateLabel(const QString& msg)
{
    m_widgets.state->setFont(m_widgets.editor->font());
    m_widgets.state->setText(msg);
    m_widgets.state->adjustSize();
    m_widgets.state->show();
    m_widgets.state->raise();
    const int height = m_widgets.state->height();
    QPoint pos = mapFromGlobal(m_widgets.editor->mapToGlobal(QPoint(0, -height)));
    m_widgets.state->move(pos);
}

void MainWindow::handleAutoCalcMessageAvailable(const QString& message)
{
    showStateLabel(message);
}

void MainWindow::handleAutoCalcQuantityAvailable(const Quantity& quantity)
{
    if (m_settings->bitfieldVisible)
        m_widgets.bitField->updateBits(quantity);
}

void MainWindow::setFullScreenEnabled(bool b)
{
    m_settings->windowOnfullScreen = b;
    b ? showFullScreen() : showNormal();
}

bool MainWindow::eventFilter(QObject* o, QEvent* e)
{
    if (o == m_docks.book) {
        if (e->type() == QEvent::Close) {
            deleteBookDock();
            return true;
        }
        return false;
    }

    if (o == m_docks.constants) {
        if (e->type() == QEvent::Close) {
            deleteConstantsDock();
            return true;
        }
        return false;
    }

    if (o == m_docks.functions) {
        if (e->type() == QEvent::Close) {
            deleteFunctionsDock();
            return true;
        }
        return false;
    }

    if (o == m_docks.history) {
        if (e->type() == QEvent::Close) {
            deleteHistoryDock();
            return true;
        }
        return false;
    }

    if (o == m_docks.variables) {
        if (e->type() == QEvent::Close) {
            deleteVariablesDock();
            return true;
        }
        return false;
    }

    if (o == m_docks.userFunctions) {
        if (e->type() == QEvent::Close) {
            deleteUserFunctionsDock();
            return true;
        }
        return false;
    }

    return QMainWindow::eventFilter(o, e);
}

void MainWindow::deleteKeypad()
{
    disconnect(m_widgets.keypad);
    m_widgets.keypad->deleteLater();
    m_widgets.keypad = 0;

    m_layouts.root->removeItem(m_layouts.keypad);
    m_layouts.keypad->deleteLater();
    m_layouts.keypad = 0;

    m_settings->keypadVisible = false;
}

void MainWindow::deleteStatusBar()
{
    statusBar()->hide();
    m_status.angleUnitSection->deleteLater();
    m_status.angleUnit = 0;
    m_status.angleUnitSection = 0;
    m_status.angleUnitLabel = 0;

    m_status.resultFormatSection->deleteLater();
    m_status.resultFormat = 0;
    m_status.resultFormatSection = 0;
    m_status.resultFormatLabel = 0;

    m_status.complexNumbersSection->deleteLater();
    m_status.complexNumbers = 0;
    m_status.complexNumbersSection = 0;
    m_status.complexNumbersLabel = 0;

    m_status.complexFormatSection->deleteLater();
    m_status.complexFormat = 0;
    m_status.complexFormatSection = 0;
    m_status.complexFormatLabel = 0;

    setStatusBar(0);
}

void MainWindow::deleteBitField()
{
    m_widgets.bitField->hide();
    m_layouts.root->removeWidget(m_widgets.bitField);
    disconnect(m_widgets.bitField);
    m_widgets.bitField->deleteLater();
    m_widgets.bitField = 0;
    m_settings->bitfieldVisible = false;
}

void MainWindow::deleteBookDock()
{
    if (!m_docks.book)
        return;

    deleteDock(m_docks.book);
    m_docks.book = nullptr;
    m_actions.viewFormulaBook->setChecked(false);
    m_settings->formulaBookDockVisible = false;
}

void MainWindow::deleteConstantsDock()
{
    if (!m_docks.constants)
        return;

    deleteDock(m_docks.constants);
    m_docks.constants = nullptr;
    m_actions.viewConstants->setChecked(false);
    m_settings->constantsDockVisible = false;
}

void MainWindow::deleteFunctionsDock()
{
    if (!m_docks.functions)
        return;

    deleteDock(m_docks.functions);
    m_docks.functions = nullptr;
    m_actions.viewFunctions->setChecked(false);
    m_settings->functionsDockVisible = false;
}

void MainWindow::deleteHistoryDock()
{
    if (!m_docks.history)
        return;

    deleteDock(m_docks.history);
    m_docks.history = nullptr;
    m_actions.viewHistory->setChecked(false);
    m_settings->historyDockVisible = false;
}

void MainWindow::deleteVariablesDock()
{
    if (!m_docks.variables)
        return;

    deleteDock(m_docks.variables);
    m_docks.variables = nullptr;
    m_actions.viewVariables->setChecked(false);
    m_settings->variablesDockVisible = false;
}

void MainWindow::deleteUserFunctionsDock()
{
    if (!m_docks.userFunctions)
        return;

    deleteDock(m_docks.userFunctions);
    m_docks.userFunctions = nullptr;
    m_actions.viewUserFunctions->setChecked(false);
    m_settings->userFunctionsDockVisible = false;
}

void MainWindow::setFunctionsDockVisible(bool b, bool takeFocus)
{
    if (b)
        createFunctionsDock(takeFocus);
    else
        deleteFunctionsDock();
}

void MainWindow::setFormulaBookDockVisible(bool b, bool takeFocus)
{
    if (b)
        createBookDock(takeFocus);
    else
        deleteBookDock();
}

void MainWindow::setConstantsDockVisible(bool b, bool takeFocus)
{
    if (b)
        createConstantsDock(takeFocus);
    else
        deleteConstantsDock();
}

void MainWindow::setHistoryDockVisible(bool b, bool takeFocus)
{
    if (b)
        createHistoryDock(takeFocus);
    else
        deleteHistoryDock();
}

void MainWindow::setVariablesDockVisible(bool b, bool takeFocus)
{
    if (b)
        createVariablesDock(takeFocus);
    else
        deleteVariablesDock();
}

void MainWindow::setUserFunctionsDockVisible(bool b, bool takeFocus)
{
    if (b)
        createUserFunctionsDock(takeFocus);
    else
        deleteUserFunctionsDock();
}

void MainWindow::setKeypadVisible(bool b)
{
    if (b)
        createKeypad();
    else
        deleteKeypad();
}

void MainWindow::setResultFormatBinary()
{
    setResultFormat('b');
    setStatusBarText();
}

void MainWindow::setResultFormatCartesian()
{
    if (m_settings->resultFormatComplex == 'c')
        return;

    m_settings->resultFormatComplex = 'c';
    setStatusBarText();
    emit resultFormatChanged();
}

void MainWindow::setResultFormatEngineering()
{
    setResultFormat('n');
    setStatusBarText();
}

void MainWindow::setResultFormatFixed()
{
    setResultFormat('f');
    setStatusBarText();
}
void MainWindow::setResultFormatGeneral()
{
    setResultFormat('g');
    setStatusBarText();
}

void MainWindow::setResultFormatHexadecimal()
{
    setResultFormat('h');
    setStatusBarText();
}

void MainWindow::setResultFormatOctal()
{
    setResultFormat('o');
    setStatusBarText();
}

void MainWindow::setResultFormatPolar()
{
    if (m_settings->resultFormatComplex == 'p')
        return;

    m_settings->resultFormatComplex = 'p';
    setStatusBarText();
    emit resultFormatChanged();
}

void MainWindow::setResultFormatScientific()
{
    setResultFormat('e');
    setStatusBarText();
}

void MainWindow::setResultFormatSexagesimal()
{
    setResultFormat('s');
    setStatusBarText();
}

void MainWindow::setAlternativeResultFormat(char c)
{
    if (m_settings->alternativeResultFormat == c)
        return;

    m_settings->alternativeResultFormat = c;
    emit resultFormatChanged();
}

void MainWindow::setAlternativeResultFormatDisabled()
{
    setAlternativeResultFormat('\0');
}

void MainWindow::setAlternativeResultFormatGeneral()
{
    setAlternativeResultFormat('g');
}

void MainWindow::setAlternativeResultFormatFixed()
{
    setAlternativeResultFormat('f');
}

void MainWindow::setAlternativeResultFormatEngineering()
{
    setAlternativeResultFormat('n');
}

void MainWindow::setAlternativeResultFormatScientific()
{
    setAlternativeResultFormat('e');
}

void MainWindow::setAlternativeResultFormatBinary()
{
    setAlternativeResultFormat('b');
}

void MainWindow::setAlternativeResultFormatOctal()
{
    setAlternativeResultFormat('o');
}

void MainWindow::setAlternativeResultFormatHexadecimal()
{
    setAlternativeResultFormat('h');
}

void MainWindow::setAlternativeResultFormatSexagesimal()
{
    setAlternativeResultFormat('s');
}

void MainWindow::setTertiaryResultFormat(char c)
{
    if (m_settings->tertiaryResultFormat == c)
        return;

    m_settings->tertiaryResultFormat = c;
    emit resultFormatChanged();
}

void MainWindow::setTertiaryResultFormatDisabled()
{
    setTertiaryResultFormat('\0');
}

void MainWindow::setTertiaryResultFormatGeneral()
{
    setTertiaryResultFormat('g');
}

void MainWindow::setTertiaryResultFormatFixed()
{
    setTertiaryResultFormat('f');
}

void MainWindow::setTertiaryResultFormatEngineering()
{
    setTertiaryResultFormat('n');
}

void MainWindow::setTertiaryResultFormatScientific()
{
    setTertiaryResultFormat('e');
}

void MainWindow::setTertiaryResultFormatBinary()
{
    setTertiaryResultFormat('b');
}

void MainWindow::setTertiaryResultFormatOctal()
{
    setTertiaryResultFormat('o');
}

void MainWindow::setTertiaryResultFormatHexadecimal()
{
    setTertiaryResultFormat('h');
}

void MainWindow::setTertiaryResultFormatSexagesimal()
{
    setTertiaryResultFormat('s');
}

void MainWindow::insertConstantIntoEditor(const QString& c)
{
    if (c.isEmpty())
        return;

    QString s = c;
    s.replace('.', m_settings->radixCharacter());
    insertTextIntoEditor(s);
}

void MainWindow::insertTextIntoEditor(const QString& s)
{
    if (s.isEmpty())
        return;

    bool shouldAutoComplete = m_widgets.editor->isAutoCompletionEnabled();
    m_widgets.editor->setAutoCompletionEnabled(false);
    m_widgets.editor->insert(s);
    m_widgets.editor->setAutoCompletionEnabled(shouldAutoComplete);

    if (!isActiveWindow())
        activateWindow();
    m_widgets.editor->setFocus();
}

void MainWindow::insertFunctionIntoEditor(const QString& f)
{
    if (f.isEmpty())
        return;
    insertTextIntoEditor(f + "()");
    QTextCursor cursor = m_widgets.editor->textCursor();
    cursor.movePosition(QTextCursor::PreviousCharacter);
    m_widgets.editor->setTextCursor(cursor);
}

void MainWindow::handleKeypadButtonPress(Keypad::Button b)
{
    switch (b) {
    case Keypad::Key0: insertTextIntoEditor("0"); break;
    case Keypad::Key1: insertTextIntoEditor("1"); break;
    case Keypad::Key2: insertTextIntoEditor("2"); break;
    case Keypad::Key3: insertTextIntoEditor("3"); break;
    case Keypad::Key4: insertTextIntoEditor("4"); break;
    case Keypad::Key5: insertTextIntoEditor("5"); break;
    case Keypad::Key6: insertTextIntoEditor("6"); break;
    case Keypad::Key7: insertTextIntoEditor("7"); break;
    case Keypad::Key8: insertTextIntoEditor("8"); break;
    case Keypad::Key9: insertTextIntoEditor("9"); break;

    case Keypad::KeyPlus: insertTextIntoEditor("+"); break;
    case Keypad::KeyMinus: insertTextIntoEditor("−"); break;
    case Keypad::KeyTimes: insertTextIntoEditor("×"); break;
    case Keypad::KeyDivide: insertTextIntoEditor("÷"); break;

    case Keypad::KeyEE: insertTextIntoEditor("e"); break;
    case Keypad::KeyLeftPar: insertTextIntoEditor("("); break;
    case Keypad::KeyRightPar: insertTextIntoEditor(")"); break;
    case Keypad::KeyRaise: insertTextIntoEditor("^"); break;
    case Keypad::KeyPercent: insertTextIntoEditor("%"); break;
    case Keypad::KeyFactorial: insertTextIntoEditor("!"); break;

    case Keypad::KeyX: insertTextIntoEditor("x"); break;
    case Keypad::KeyXEquals: insertTextIntoEditor("x="); break;
    case Keypad::KeyPi: insertTextIntoEditor("pi"); break;
    case Keypad::KeyAns: insertTextIntoEditor("ans"); break;

    case Keypad::KeySqrt: insertTextIntoEditor("sqrt("); break;
    case Keypad::KeyLn: insertTextIntoEditor("ln("); break;
    case Keypad::KeyExp:insertTextIntoEditor("exp("); break;
    case Keypad::KeySin: insertTextIntoEditor("sin("); break;
    case Keypad::KeyCos: insertTextIntoEditor("cos("); break;
    case Keypad::KeyTan: insertTextIntoEditor("tan("); break;
    case Keypad::KeyAcos: insertTextIntoEditor("arccos("); break;
    case Keypad::KeyAtan: insertTextIntoEditor("arctan("); break;
    case Keypad::KeyAsin: insertTextIntoEditor("arcsin("); break;

    case Keypad::KeyRadixChar: insertTextIntoEditor(QString(m_settings->radixCharacter())); break;

    case Keypad::KeyClear: clearEditor(); break;
    case Keypad::KeyEquals: evaluateEditorExpression(); break;

    default: break;
    }
}

void MainWindow::openUpdatesURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1("https://heldercorreia.bitbucket.io/speedcrunch/")));
}

void MainWindow::openFeedbackURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1("https://bitbucket.org/heldercorreia/speedcrunch/issues?status=new&status=open")));
}

void MainWindow::openCommunityURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1("https://groups.google.com/group/speedcrunch/")));
}

void MainWindow::openNewsURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1("http://speedcrunch.blogspot.com/")));
}

void MainWindow::openDonateURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1("https://heldercorreia.bitbucket.io/speedcrunch/donate.html")));
}

void MainWindow::copy()
{
    m_copyWidget->copy();
}

void MainWindow::restoreSession() {
    QString data_path = Settings::getDataPath();
    QDir qdir;
    qdir.mkpath(data_path);
    data_path.append("/history.json");

    QFile file(data_path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray data = file.readAll();
    QJsonDocument doc(QJsonDocument::fromJson(data));
    m_session->deSerialize(doc.object(), true);

    file.close();
    emit historyChanged();
    emit variablesChanged();
    emit functionsChanged();

    m_conditions.autoAns = !m_session->historyToList().empty();
}

void MainWindow::evaluateEditorExpression()
{
    QString expr = m_evaluator->autoFix(m_widgets.editor->text());
    const bool isCommentOnly = Evaluator::isCommentOnlyExpression(expr);

    if (expr.isEmpty())
        return;

    if (m_pendingHistoryEditIndex >= 0) {
        const int previousDisplayScrollValue = m_widgets.display->verticalScrollBar()->value();
        const int historySize = m_session->historyToList().size();
        if (m_pendingHistoryEditIndex >= historySize) {
            m_pendingHistoryEditIndex = -1;
            m_widgets.display->setEditingHistoryIndex(-1);
            m_widgets.editor->clear();
            m_widgets.display->verticalScrollBar()->setValue(previousDisplayScrollValue);
        } else {
            const QStringList previousExpressions = historyExpressions();
            QStringList updatedExpressions = previousExpressions;
            updatedExpressions[m_pendingHistoryEditIndex] = expr;

            int errorIndex = -1;
            QString errorText;
            if (!rebuildSessionFromExpressions(updatedExpressions, &errorIndex, &errorText)) {
                rebuildSessionFromExpressions(previousExpressions);
                emit historyChanged();
                emit variablesChanged();
                emit functionsChanged();
                m_widgets.display->verticalScrollBar()->setValue(previousDisplayScrollValue);
                showStateLabel(tr("Could not recalculate from calculation %1: %2").arg(errorIndex + 1).arg(errorText));
                return;
            }

            m_pendingHistoryEditIndex = -1;
            m_widgets.display->setEditingHistoryIndex(-1);
            emit historyChanged();
            emit variablesChanged();
            emit functionsChanged();
            m_widgets.display->verticalScrollBar()->setValue(previousDisplayScrollValue);
            m_widgets.editor->clear();

            m_widgets.editor->stopAutoCalc();
            m_widgets.editor->stopAutoComplete();
            return;
        }
    }

    m_evaluator->setExpression(expr);
    Quantity result = m_evaluator->evalUpdateAns();

    if (!m_evaluator->error().isEmpty()) {
        showStateLabel(m_evaluator->error());
        return;
    }

    if (m_evaluator->isUserFunctionAssign()) {
        result = CMath::nan();
        emit functionsChanged();
    } else if (result.isNan() && !isCommentOnly)
        return;

    m_session->addHistoryEntry(HistoryEntry(expr, result));
    if (m_settings->historySaving == Settings::HistorySavingContinuously)
        saveSessionToDefaultPath();
    emit historyChanged();
    emit variablesChanged();

    if (m_settings->bitfieldVisible)
        m_widgets.bitField->updateBits(result);

    if (m_settings->autoResultToClipboard)
        copyResultToClipboard();

    if (m_settings->leaveLastExpression)
        m_widgets.editor->selectAll();
    else
        m_widgets.editor->clear();

    m_widgets.editor->stopAutoCalc();
    m_widgets.editor->stopAutoComplete();
    if (!result.isNan())
        m_conditions.autoAns = true;
}

void MainWindow::startHistoryEntryEdit(int index)
{
    const int historySize = m_session->historyToList().size();
    if (index < 0 || index >= historySize)
        return;

    m_pendingHistoryEditIndex = index;
    m_widgets.display->setEditingHistoryIndex(index);
    m_widgets.editor->setText(m_session->historyEntryAt(index).expr());
    m_widgets.editor->setFocus();
    m_widgets.editor->setCursorPosition(m_widgets.editor->text().size());
    showStateLabel(tr("Editing calculation. Press Escape to cancel."));
}

void MainWindow::cancelHistoryEntryEdit()
{
    if (m_pendingHistoryEditIndex < 0)
        return;

    const int previousDisplayScrollValue = m_widgets.display->verticalScrollBar()->value();
    m_pendingHistoryEditIndex = -1;
    m_widgets.display->setEditingHistoryIndex(-1);
    m_widgets.display->verticalScrollBar()->setValue(previousDisplayScrollValue);
    m_widgets.editor->clear();
    showReadyMessage();
}

QStringList MainWindow::historyExpressions() const
{
    QStringList expressions;
    const QList<HistoryEntry> history = m_session->historyToList();
    expressions.reserve(history.size());
    for (int i = 0; i < history.size(); ++i)
        expressions.append(history.at(i).expr());
    return expressions;
}

bool MainWindow::rebuildSessionFromExpressions(const QStringList& expressions, int* errorIndex, QString* errorText)
{
    if (errorIndex)
        *errorIndex = -1;
    if (errorText)
        *errorText = QString();

    m_session->clearHistory();
    m_session->clearVariables();
    m_session->clearUserFunctions();
    m_evaluator->initializeBuiltInVariables();
    m_conditions.autoAns = false;

    for (int i = 0; i < expressions.size(); ++i) {
        const QString currentExpr = expressions.at(i);
        const bool isCommentOnly = Evaluator::isCommentOnlyExpression(currentExpr);

        m_evaluator->setExpression(currentExpr);
        Quantity result = m_evaluator->evalUpdateAns();
        if (!m_evaluator->error().isEmpty()) {
            if (errorIndex)
                *errorIndex = i;
            if (errorText)
                *errorText = m_evaluator->error();
            return false;
        }

        if (m_evaluator->isUserFunctionAssign())
            result = CMath::nan();
        else if (result.isNan() && !isCommentOnly)
            continue;

        m_session->addHistoryEntry(HistoryEntry(currentExpr, result));
        if (!result.isNan())
            m_conditions.autoAns = true;
    }

    return true;
}

void MainWindow::removeHistoryEntryAt(int index)
{
    const int historySize = m_session->historyToList().size();
    if (index < 0 || index >= historySize)
        return;

    m_session->removeHistoryEntryAt(index);
    if (m_pendingHistoryEditIndex == index)
        m_pendingHistoryEditIndex = -1;
    else if (m_pendingHistoryEditIndex > index)
        --m_pendingHistoryEditIndex;
    m_widgets.display->setEditingHistoryIndex(m_pendingHistoryEditIndex);
    m_conditions.autoAns = !m_session->historyToList().empty();
    emit historyChanged();
}

void MainWindow::removeHistoryEntriesAbove(int index)
{
    const int historySize = m_session->historyToList().size();
    if (historySize == 0 || index <= 0 || index >= historySize)
        return;

    for (int i = 0; i < index; ++i)
        m_session->removeHistoryEntryAt(0);

    if (m_pendingHistoryEditIndex >= 0) {
        if (m_pendingHistoryEditIndex < index)
            m_pendingHistoryEditIndex = -1;
        else
            m_pendingHistoryEditIndex -= index;
    }
    m_widgets.display->setEditingHistoryIndex(m_pendingHistoryEditIndex);
    m_conditions.autoAns = !m_session->historyToList().empty();
    emit historyChanged();
}

void MainWindow::removeHistoryEntriesBelow(int index)
{
    const int historySize = m_session->historyToList().size();
    if (historySize == 0 || index < 0 || index >= historySize - 1)
        return;

    for (int i = historySize - 1; i > index; --i)
        m_session->removeHistoryEntryAt(i);

    if (m_pendingHistoryEditIndex > index)
        m_pendingHistoryEditIndex = -1;
    m_widgets.display->setEditingHistoryIndex(m_pendingHistoryEditIndex);
    m_conditions.autoAns = !m_session->historyToList().empty();
    emit historyChanged();
}

void MainWindow::clearTextEditSelection(QPlainTextEdit* edit)
{
    QTextCursor cursor = edit->textCursor();
    if (cursor.hasSelection()) {
        cursor.clearSelection();
        edit->setTextCursor(cursor);
    }
}

void MainWindow::handleManualClosed()
{
    disconnect(m_widgets.manual);
    m_settings->manualWindowGeometry = m_settings->windowPositionSave ? m_widgets.manual->saveGeometry() : QByteArray();
    m_widgets.manual->deleteLater();
    m_widgets.manual = 0;
}

void MainWindow::handleDisplaySelectionChange()
{
    clearTextEditSelection(m_widgets.editor);
    const QTextCursor displayCursor = m_widgets.display->textCursor();
    if (displayCursor.hasSelection()) {
        m_widgets.editor->autoCalcSelection(displayCursor.selectedText());
        return;
    }

    if (m_widgets.editor->text().trimmed().isEmpty()) {
        hideStateLabel();
        return;
    }

    m_widgets.editor->refreshAutoCalc();
}

void MainWindow::handleEditorSelectionChange()
{
    clearTextEditSelection(m_widgets.display);
    if (m_widgets.editor->textCursor().hasSelection())
        return;

    if (m_widgets.editor->text().trimmed().isEmpty()) {
        hideStateLabel();
        return;
    }

    m_widgets.editor->refreshAutoCalc();
}

void MainWindow::handleCopyAvailable(bool copyAvailable)
{
    if (!copyAvailable)
        return;
    QPlainTextEdit* const textEdit = static_cast<QPlainTextEdit*>(sender());
    if (textEdit)
        m_copyWidget = textEdit;
}

void MainWindow::handleBitsChanged(const QString& str)
{
    Quantity num(CNumber(str.toLatin1().data()));
    auto result = DMath::format(num, Quantity::Format::Fixed() + Quantity::Format::Hexadecimal());
    insertTextIntoEditor(result);
    showStateLabel(QString("Current value: %1").arg(NumberFormatter::format(num)));

    auto cursor = m_widgets.editor->textCursor();
    if (cursor.hasSelection())
        cursor.removeSelectedText();
    cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, result.length());
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, result.length());
    m_widgets.editor->setTextCursor(cursor);
}

void MainWindow::handleEditorTextChange()
{
    m_widgets.display->clearHoverFeedback();
    clearTextEditSelection(m_widgets.display);
    if (m_widgets.editor->text().trimmed().isEmpty()) {
        hideStateLabel();
        return;
    }

    if (m_conditions.autoAns && m_settings->autoAns) {
        QString expr = m_evaluator->autoFix(m_widgets.editor->text());
        if (expr.isEmpty())
            return;

        Tokens tokens = m_evaluator->scan(expr);
        if (tokens.count() == 1) {
            bool operatorCondition =
                tokens.at(0).asOperator() == Token::Addition
                || tokens.at(0).asOperator() == Token::Subtraction
                || tokens.at(0).asOperator() == Token::Multiplication
                || tokens.at(0).asOperator() == Token::Division
                || tokens.at(0).asOperator() == Token::Exponentiation;
            if (operatorCondition) {
                m_conditions.autoAns = false;
                expr.prepend("ans");
                m_widgets.editor->setText(expr);
                m_widgets.editor->setCursorPosition(expr.length());
            }
        }
    }
}

void MainWindow::handleDockWidgetVisibilityChanged(bool visible)
{
    QDockWidget* dock = qobject_cast<QDockWidget*>(sender());
    if (!dock)
        return;

    // Pass the focus back to the editor if the dock that is being hidden has the focus.
    QWidget* focusWidget = dock->focusWidget();
    if (focusWidget && !visible && focusWidget->hasFocus())
        m_widgets.editor->setFocus();
}

void MainWindow::insertVariableIntoEditor(const QString& v)
{
    insertTextIntoEditor(v);
}

void MainWindow::insertUserFunctionIntoEditor(const QString& v)
{
    insertTextIntoEditor(v);
}

void MainWindow::setRadixCharacterAutomatic()
{
    setRadixCharacter(0);
}

void MainWindow::setRadixCharacterDot()
{
    setRadixCharacter('.');
}

void MainWindow::setRadixCharacterComma()
{
    setRadixCharacter(',');
}

void MainWindow::setRadixCharacterBoth()
{
    setRadixCharacter('*');
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    if (m_widgets.manual) {
        m_widgets.manual->close();
    }
    saveSettings();
    if (m_settings->historySaving == Settings::HistorySavingOnExit)
        saveSessionToDefaultPath();
    e->accept();
}

void MainWindow::saveSessionToDefaultPath()
{
    QString data_path = Settings::getDataPath();
    QDir qdir;
    qdir.mkpath(data_path);
    data_path.append("/history.json");
    saveSession(data_path);
}

void MainWindow::setResultPrecision(int p)
{
    if (m_settings->resultPrecision == p)
        return;

    m_settings->resultPrecision = p;
    emit resultPrecisionChanged();
}

void MainWindow::setResultFormat(char c)
{
    if (m_settings->resultFormat == c)
        return;

    m_settings->resultFormat = c;
    emit resultFormatChanged();
}

void MainWindow::setRadixCharacter(char c)
{
    m_settings->setRadixCharacter(c);
    emit radixCharacterChanged();
}

void MainWindow::increaseDisplayFontPointSize()
{
    m_widgets.display->increaseFontPointSize();
    m_widgets.editor->increaseFontPointSize();
    if (m_widgets.state->isVisible())
        showStateLabel(m_widgets.state->text());
}

void MainWindow::decreaseDisplayFontPointSize()
{
    m_widgets.display->decreaseFontPointSize();
    m_widgets.editor->decreaseFontPointSize();
    if (m_widgets.state->isVisible())
        showStateLabel(m_widgets.state->text());
}

void MainWindow::showLanguageChooserDialog()
{
    QMap<QString, QString> map;

    // List all available translations from the resource files
    QDir localeDir(":/locale/", "*.qm");
    QFileInfoList localeList = localeDir.entryInfoList();
    for (int i = 0; i < localeList.size(); ++i) {
        QFileInfo fileInfo = localeList.at(i);
        QString localeName = fileInfo.baseName();
        QString langName = QLocale(localeName).nativeLanguageName();

        // Kludges for region-specific translations
        if(localeName == "es") langName = QString::fromUtf8("Español (Latinoamérica)");
        if(localeName == "es_ES") langName = QString::fromUtf8("Español (España)");
        if(localeName == "pt_BR") langName = QString::fromUtf8("Português (Brasil)");
        if(localeName == "pt_PT") langName = QString::fromUtf8("Português (Portugal)");

        // The first letter is not always capitalized so force it
        langName[0] = langName[0].toUpper();
        map.insert(langName, localeName);
    }

    const auto values = map.values();
    int current = values.indexOf(m_settings->language) + 1;

    QString defaultKey = tr("System Default");
    QStringList keys(QStringList() << defaultKey << map.keys());

    bool ok;
    QString langName = QInputDialog::getItem(this, tr("Language"), tr("Select the language:"),
        keys, current, false, &ok);
    if (ok && !langName.isEmpty()) {
        QString value = (langName == defaultKey) ? QLatin1String("C") : map.value(langName);
        if (m_settings->language != value) {
            m_settings->language = value;
            emit languageChanged();
        }
    }
}

void MainWindow::showResultFormatContextMenu(const QPoint& point)
{
    m_menus.resultFormat->popup(m_status.resultFormat->mapToGlobal(point));
}

void MainWindow::cycleAngleUnits()
{
    if (m_actions.settingsAngleUnitDegree->isChecked())
        m_actions.settingsAngleUnitRadian->trigger();
    else if (m_actions.settingsAngleUnitRadian->isChecked())
        m_actions.settingsAngleUnitGradian->trigger();
    else if (m_actions.settingsAngleUnitGradian->isChecked())
        m_actions.settingsAngleUnitDegree->trigger();
}

void MainWindow::cycleResultFormats()
{
  if (m_actions.settingsResultFormatGeneral->isChecked())
      m_actions.settingsResultFormatFixed->trigger();
  else if (m_actions.settingsResultFormatFixed->isChecked())
      m_actions.settingsResultFormatEngineering->trigger();
  else if (m_actions.settingsResultFormatEngineering->isChecked())
      m_actions.settingsResultFormatScientific->trigger();
  else if (m_actions.settingsResultFormatScientific->isChecked())
      m_actions.settingsResultFormatBinary->trigger();
  else if (m_actions.settingsResultFormatBinary->isChecked())
      m_actions.settingsResultFormatOctal->trigger();
  else if (m_actions.settingsResultFormatOctal->isChecked())
      m_actions.settingsResultFormatHexadecimal->trigger();
  else if (m_actions.settingsResultFormatHexadecimal->isChecked())
      m_actions.settingsResultFormatSexagesimal->trigger();
  else if (m_actions.settingsResultFormatSexagesimal->isChecked())
      m_actions.settingsResultFormatGeneral->trigger();
}
