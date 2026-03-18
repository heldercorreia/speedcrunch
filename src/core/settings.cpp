// This file is part of the SpeedCrunch project
// Copyright (C) 2004, 2005, 2007, 2008 Ariya Hidayat <ariya@kde.org>
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

#include "core/settings.h"

#include "math/floatconfig.h"

#include <QDir>
#include <QLocale>
#include <QSettings>
#include <QApplication>
#include <QFileInfo>
#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtCore/QStandardPaths>

#include <filesystem>

#ifdef Q_OS_WIN
# define WIN32_LEAN_AND_MEAN
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <windows.h>
# include <shlobj.h>
#endif


// The current config revision. Based on the application version number
// ('1200' corresponds to 0.12.0, '10300' would be 1.3.0 etc.). When making
// a backwards-incompatible change to the config format, bump this number to
// the next release (if not already happened), then update the migration code
// in createQSettings. Don't bump the config version unnecessarily for
// releases that don't contain incompatible changes.
static const int ConfigVersion = 1200;


static const char* DefaultColorScheme = "Terminal";

namespace {
static const int MinCustomKeypadDimension = 1;
static const int MaxCustomKeypadDimension = 20;

bool isValidCustomAction(int action)
{
    return action >= static_cast<int>(Settings::CustomKeypadActionInsertText)
        && action <= static_cast<int>(Settings::CustomKeypadActionEvaluateExpression);
}

QJsonObject serializeCustomKeypad(const Settings::CustomKeypad& keypad)
{
    QJsonObject json;
    json.insert(QStringLiteral("rows"), keypad.rows);
    json.insert(QStringLiteral("columns"), keypad.columns);

    QJsonArray buttonsJson;
    for (const auto& button : keypad.buttons) {
        QJsonObject item;
        item.insert(QStringLiteral("row"), button.row);
        item.insert(QStringLiteral("column"), button.column);
        item.insert(QStringLiteral("label"), button.label);
        item.insert(QStringLiteral("text"), button.text);
        item.insert(QStringLiteral("action"), static_cast<int>(button.action));
        buttonsJson.push_back(item);
    }
    json.insert(QStringLiteral("buttons"), buttonsJson);
    return json;
}

bool deserializeCustomKeypad(const QJsonObject& json, Settings::CustomKeypad* keypad)
{
    if (!keypad || !json.contains(QStringLiteral("rows")) || !json.contains(QStringLiteral("columns")))
        return false;

    const int rows = json.value(QStringLiteral("rows")).toInt(0);
    const int columns = json.value(QStringLiteral("columns")).toInt(0);
    if (rows < MinCustomKeypadDimension || rows > MaxCustomKeypadDimension
            || columns < MinCustomKeypadDimension || columns > MaxCustomKeypadDimension) {
        return false;
    }

    Settings::CustomKeypad parsed;
    parsed.rows = rows;
    parsed.columns = columns;

    const QJsonArray buttonsJson = json.value(QStringLiteral("buttons")).toArray();
    for (const auto& buttonValue : buttonsJson) {
        if (!buttonValue.isObject())
            continue;
        const QJsonObject buttonJson = buttonValue.toObject();
        const int row = buttonJson.value(QStringLiteral("row")).toInt(-1);
        const int column = buttonJson.value(QStringLiteral("column")).toInt(-1);
        const int action = buttonJson.value(QStringLiteral("action")).toInt(
            static_cast<int>(Settings::CustomKeypadActionInsertText));
        if (row < 0 || row >= rows || column < 0 || column >= columns || !isValidCustomAction(action))
            continue;

        Settings::CustomKeypadButton button;
        button.row = row;
        button.column = column;
        button.label = buttonJson.value(QStringLiteral("label")).toString();
        button.text = buttonJson.value(QStringLiteral("text")).toString();
        button.action = static_cast<Settings::CustomKeypadButtonAction>(action);
        parsed.buttons.append(button);
    }

    *keypad = parsed;
    return true;
}

bool shouldUseNonAtomicSettingsSync(const QString& fileName)
{
    const QFileInfo fileInfo(fileName);
    if (!fileInfo.exists())
        return false;

    // Renaming a temporary file over a linked target breaks the link.
    if (fileInfo.isSymLink())
        return true;

    std::error_code error;
    const auto linkCount = std::filesystem::hard_link_count(
        std::filesystem::path(fileName.toStdU16String()), error);
    return !error && linkCount > 1;
}
} // namespace

QString Settings::getConfigPath()
{
#ifdef SPEEDCRUNCH_PORTABLE
    return QApplication::applicationDirPath();
#elif defined(Q_OS_WIN)
    // On Windows, use AppData/Roaming/SpeedCrunch, the same path as getDataPath.
    return getDataPath();
#else
    // Everywhere else, use `QStandardPaths::ConfigLocation`/SpeedCrunch:
    // * OSX: ~/Library/Preferences/SpeedCrunch
    // * Linux: ~/.config/SpeedCrunch
    return QString("%1/%2").arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation),
                                QCoreApplication::applicationName());
#endif
}

QString Settings::getDataPath()
{
#ifdef SPEEDCRUNCH_PORTABLE
    return QApplication::applicationDirPath();
#elif QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#elif defined(Q_OS_WIN)
    // We can't use AppDataLocation, so we simply use the Win32 API to emulate it.
    WCHAR w32path[MAX_PATH];
    HRESULT result = SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, w32path);
    Q_ASSERT(SUCCEEDED(result));
    QString path = QString::fromWCharArray(w32path);
    QString orgName = QCoreApplication::organizationName();
    QString appName = QCoreApplication::applicationName();
    if (!orgName.isEmpty()) {
        path.append('\\');
        path.append(orgName);
    }
    if (!appName.isEmpty()) {
        path.append('\\');
        path.append(appName);
    }
    return QDir::fromNativeSeparators(path);
#else
    // Any non-Windows with Qt < 5.4. Since DataLocation and AppDataLocation are
    // equivalent outside of Windows, that should be fine.
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation);
#endif
}

QString Settings::getCachePath()
{
#ifdef SPEEDCRUNCH_PORTABLE
    return QApplication::applicationDirPath();
#else
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#endif
}

static Settings* s_settingsInstance = 0;
static char s_radixCharacter = 0;

static void s_deleteSettings()
{
    delete s_settingsInstance;
}

static QSettings* createQSettings(const QString& key);

Settings* Settings::instance()
{
    if (!s_settingsInstance) {
        s_settingsInstance = new Settings;
        s_settingsInstance->load();
        qAddPostRoutine(s_deleteSettings);
    }

    return s_settingsInstance;
}

Settings::CustomKeypad Settings::defaultCustomKeypad()
{
    Settings::CustomKeypad custom;
    custom.rows = 4;
    custom.columns = 5;

    static const struct {
        int row;
        int column;
        const char* label;
        const char* text;
        Settings::CustomKeypadButtonAction action;
    } defaults[] = {
        {0, 0, "7", "7", Settings::CustomKeypadActionInsertText},
        {0, 1, "8", "8", Settings::CustomKeypadActionInsertText},
        {0, 2, "9", "9", Settings::CustomKeypadActionInsertText},
        {0, 3, "÷", "÷", Settings::CustomKeypadActionInsertText},
        {0, 4, "⌧", "", Settings::CustomKeypadActionClearExpression},
        {1, 0, "4", "4", Settings::CustomKeypadActionInsertText},
        {1, 1, "5", "5", Settings::CustomKeypadActionInsertText},
        {1, 2, "6", "6", Settings::CustomKeypadActionInsertText},
        {1, 3, "×", "×", Settings::CustomKeypadActionInsertText},
        {1, 4, "⌫", "", Settings::CustomKeypadActionBackspace},
        {2, 0, "1", "1", Settings::CustomKeypadActionInsertText},
        {2, 1, "2", "2", Settings::CustomKeypadActionInsertText},
        {2, 2, "3", "3", Settings::CustomKeypadActionInsertText},
        {2, 3, "−", "−", Settings::CustomKeypadActionInsertText},
        {2, 4, "(", "(", Settings::CustomKeypadActionInsertText},
        {3, 0, "0", "0", Settings::CustomKeypadActionInsertText},
        {3, 1, ".", ".", Settings::CustomKeypadActionInsertText},
        {3, 2, "=", "", Settings::CustomKeypadActionEvaluateExpression},
        {3, 3, "+", "+", Settings::CustomKeypadActionInsertText},
        {3, 4, ")", ")", Settings::CustomKeypadActionInsertText}
    };

    const int count = int(sizeof defaults / sizeof defaults[0]);
    for (int i = 0; i < count; ++i) {
        Settings::CustomKeypadButton button;
        button.row = defaults[i].row;
        button.column = defaults[i].column;
        button.label = QString::fromUtf8(defaults[i].label);
        button.text = QString::fromUtf8(defaults[i].text);
        button.action = defaults[i].action;
        custom.buttons.append(button);
    }

    return custom;
}

Settings::Settings()
{
    digitGroupingIntegerPartOnly = false;
    singleInstance = true;
}

void Settings::load()
{
    const char* KEY = "SpeedCrunch";

    QSettings* settings = createQSettings(KEY);
    if (!settings)
        return;

    QString key = QString::fromLatin1(KEY) + QLatin1String("/General/");

    // Angle mode special case.
    QString angleUnitStr;
    angleUnitStr = settings->value(key + QLatin1String("AngleMode"), "r").toString();
    if (angleUnitStr != QLatin1String("r") && angleUnitStr != QLatin1String("d") && angleUnitStr != QLatin1String("g"))
        angleUnit = 'r';
    else
        angleUnit = angleUnitStr.at(0).toLatin1();

    // Radix character special case.
    QString radixCharStr;
    radixCharStr = settings->value(key + QLatin1String("RadixCharacter"), "*").toString();
    setRadixCharacter(radixCharStr.at(0).toLatin1());

    autoAns = settings->value(key + QLatin1String("AutoAns"), false).toBool();
    autoCalc = settings->value(key + QLatin1String("AutoCalc"), true).toBool();
    autoCompletion = settings->value(key + QLatin1String("AutoCompletion"), true).toBool();
    const QString historySavingKey = key + QLatin1String("HistorySaving");
    const QString historySavingLegacyKey = key + QLatin1String("HistorySavingPolicy");
    if (settings->contains(historySavingKey)) {
        historySaving = static_cast<HistorySaving>(settings->value(historySavingKey).toInt());
    } else if (settings->contains(historySavingLegacyKey)) {
        // Backward compatibility with previous key name.
        historySaving = static_cast<HistorySaving>(settings->value(historySavingLegacyKey).toInt());
    } else {
        // Backward compatibility with previous boolean setting.
        historySaving = settings->value(key + QLatin1String("SessionSave"), true).toBool()
            ? HistorySavingOnExit
            : HistorySavingNever;
    }
    if (historySaving != HistorySavingNever
            && historySaving != HistorySavingOnExit
            && historySaving != HistorySavingContinuously) {
        historySaving = HistorySavingOnExit;
    }
    leaveLastExpression = settings->value(key + QLatin1String("LeaveLastExpression"), false).toBool();
    showEmptyHistoryHint = settings->value(key + QLatin1String("ShowEmptyHistoryHint"), true).toBool();
    language = settings->value(key + QLatin1String("Language"), "C").toString();
    syntaxHighlighting = settings->value(key + QLatin1String("SyntaxHighlighting"), true).toBool();
    hoverHighlightResults = settings->value(key + QLatin1String("HoverHighlightResults"), true).toBool();
    autoResultToClipboard = settings->value(key + QLatin1String("AutoResultToClipboard"), false).toBool();
    windowPositionSave = settings->value(key + QLatin1String("WindowPositionSave"), true).toBool();
    singleInstance = settings->value(key + QLatin1String("SingleInstance"), true).toBool();
    complexNumbers = settings->value(key + QLatin1String("ComplexNumbers"), false).toBool();

    digitGrouping = settings->value(key + QLatin1String("DigitGrouping"), 0).toInt();
    digitGrouping = std::min(3, std::max(0, digitGrouping));
    digitGroupingIntegerPartOnly = settings->value(key + QLatin1String("DigitGroupingIntegerPartOnly"), false).toBool();
    maxHistoryEntries = settings->value(key + QLatin1String("MaxHistoryEntries"), 100).toInt();
    maxHistoryEntries = std::max(0, maxHistoryEntries);

    key = KEY + QLatin1String("/Format/");

    // Format special case.
    QString format = settings->value(key + QLatin1String("Type"), 'f').toString();
    if (format != "g" && format != "f" && format != "e" && format != "n"&& format != "h"
        && format != "o" && format != "b" && format != "s")
        resultFormat = 'f';
    else
        resultFormat = format.at(0).toLatin1();

    QString alternativeFormat = settings->value(key + QLatin1String("AlternativeType"), "").toString();
    if (alternativeFormat.isEmpty())
        alternativeResultFormat = '\0';
    else if (alternativeFormat != "g" && alternativeFormat != "f" && alternativeFormat != "e"
             && alternativeFormat != "n" && alternativeFormat != "h" && alternativeFormat != "o"
             && alternativeFormat != "b" && alternativeFormat != "s")
        alternativeResultFormat = '\0';
    else
        alternativeResultFormat = alternativeFormat.at(0).toLatin1();

    QString tertiaryFormat = settings->value(key + QLatin1String("TertiaryType"), "").toString();
    if (tertiaryFormat.isEmpty())
        tertiaryResultFormat = '\0';
    else if (tertiaryFormat != "g" && tertiaryFormat != "f" && tertiaryFormat != "e"
             && tertiaryFormat != "n" && tertiaryFormat != "h" && tertiaryFormat != "o"
             && tertiaryFormat != "b" && tertiaryFormat != "s")
        tertiaryResultFormat = '\0';
    else
        tertiaryResultFormat = tertiaryFormat.at(0).toLatin1();

    // Complex format special case.
    QString cmplxFormat = settings->value(key + QLatin1String("ComplexForm"), 'c').toString();
    if (cmplxFormat != "c" && cmplxFormat != "p")
        resultFormatComplex = 'c';
    else
        resultFormatComplex = cmplxFormat.at(0).toLatin1();

    resultPrecision = settings->value(key + QLatin1String("Precision"), -1).toInt();

    if (resultPrecision > DECPRECISION)
        resultPrecision = DECPRECISION;

    key = KEY + QLatin1String("/Layout/");
    customKeypad = defaultCustomKeypad();
    windowOnfullScreen = settings->value(key + QLatin1String("WindowOnFullScreen"), false).toBool();
    historyDockVisible = settings->value(key + QLatin1String("HistoryDockVisible"), false).toBool();
    int keypadModeValue = static_cast<int>(KeypadModeBasicWide);
    if (settings->contains(key + QLatin1String("KeypadMode"))) {
        keypadModeValue = settings->value(key + QLatin1String("KeypadMode")).toInt();
    } else if (settings->contains(key + QLatin1String("KeypadVisible"))) {
        // Legacy compatibility for pre-keypad-mode settings.
        keypadModeValue = settings->value(key + QLatin1String("KeypadVisible"), false).toBool()
            ? static_cast<int>(KeypadModeScientificWide)
            : static_cast<int>(KeypadModeDisabled);
    }
    if (keypadModeValue >= static_cast<int>(KeypadModeDisabled)
            && keypadModeValue <= static_cast<int>(KeypadModeCustom)) {
        keypadMode = static_cast<KeypadMode>(keypadModeValue);
    } else {
        keypadMode = KeypadModeBasicWide;
    }
    if (keypadMode == KeypadModeBasicNarrow)
        keypadMode = KeypadModeBasicWide;
    keypadVisible = (keypadMode == KeypadModeBasicWide
        || keypadMode == KeypadModeScientificWide
        || keypadMode == KeypadModeScientificNarrow
        || keypadMode == KeypadModeCustom);
    const QString customKeypadJson = settings->value(key + QLatin1String("CustomKeypad"), QString()).toString();
    if (!customKeypadJson.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(customKeypadJson.toUtf8());
        if (doc.isObject())
            deserializeCustomKeypad(doc.object(), &customKeypad);
    }
    statusBarVisible = settings->value(key + QLatin1String("StatusBarVisible"), false).toBool();
    menuBarVisible = settings->value(key + QLatin1String("MenuBarVisible"), true).toBool();
    functionsDockVisible = settings->value(key + QLatin1String("FunctionsDockVisible"), false).toBool();
    variablesDockVisible = settings->value(key + QLatin1String("VariablesDockVisible"), false).toBool();
    userFunctionsDockVisible = settings->value(key + QLatin1String("UserFunctionsDockVisible"), false).toBool();
    formulaBookDockVisible = settings->value(key + QLatin1String("FormulaBookDockVisible"), false).toBool();
    constantsDockVisible = settings->value(key + QLatin1String("ConstantsDockVisible"), false).toBool();
    windowAlwaysOnTop = settings->value(key + QLatin1String("WindowAlwaysOnTop"), false).toBool();
    bitfieldVisible = settings->value(key + QLatin1String("BitfieldVisible"), false).toBool();

    windowState = settings->value(key + QLatin1String("State")).toByteArray();
    windowGeometry = settings->value(key + QLatin1String("WindowGeometry")).toByteArray();
    manualWindowGeometry = settings->value(key + QLatin1String("ManualWindowGeometry")).toByteArray();

    key = KEY + QLatin1String("/Display/");
    displayFont = settings->value(key + QLatin1String("DisplayFont"), QFont().toString()).toString();
    colorScheme = settings->value(key + QLatin1String("ColorSchemeName"), DefaultColorScheme).toString();

    delete settings;
}

void Settings::save()
{
    const QString KEY = QString::fromLatin1("SpeedCrunch");

    QSettings* settings = createQSettings(KEY);
    if (!settings)
        return;

    QString key = KEY + QLatin1String("/General/");

    settings->setValue(key + QLatin1String("HistorySaving"), static_cast<int>(historySaving));
    settings->setValue(key + QLatin1String("LeaveLastExpression"), leaveLastExpression);
    settings->setValue(key + QLatin1String("ShowEmptyHistoryHint"), showEmptyHistoryHint);
    settings->setValue(key + QLatin1String("AutoCompletion"), autoCompletion);
    settings->setValue(key + QLatin1String("AutoAns"), autoAns);
    settings->setValue(key + QLatin1String("AutoCalc"), autoCalc);
    settings->setValue(key + QLatin1String("SyntaxHighlighting"), syntaxHighlighting);
    settings->setValue(key + QLatin1String("HoverHighlightResults"), hoverHighlightResults);
    settings->setValue(key + QLatin1String("DigitGrouping"), digitGrouping);
    settings->setValue(key + QLatin1String("DigitGroupingIntegerPartOnly"), digitGroupingIntegerPartOnly);
    settings->setValue(key + QLatin1String("MaxHistoryEntries"), maxHistoryEntries);
    settings->setValue(key + QLatin1String("AutoResultToClipboard"), autoResultToClipboard);
    settings->setValue(key + QLatin1String("Language"), language);
    settings->setValue(key + QLatin1String("WindowPositionSave"), windowPositionSave);
    settings->setValue(key + QLatin1String("SingleInstance"), singleInstance);
    settings->setValue(key + QLatin1String("ComplexNumbers"), complexNumbers);

    settings->setValue(key + QLatin1String("AngleMode"), QString(QChar(angleUnit)));

    char c = 'C';
    if (s_radixCharacter != 0)
        c = s_radixCharacter;
    settings->setValue(key + QLatin1String("RadixCharacter"), QString(QChar(c)));

    key = KEY + QLatin1String("/Format/");

    settings->setValue(key + QLatin1String("Type"), QString(QChar(resultFormat)));
    settings->setValue(key + QLatin1String("AlternativeType"),
        alternativeResultFormat == '\0' ? QString() : QString(QChar(alternativeResultFormat)));
    settings->setValue(key + QLatin1String("TertiaryType"),
        tertiaryResultFormat == '\0' ? QString() : QString(QChar(tertiaryResultFormat)));
    settings->setValue(key + QLatin1String("ComplexForm"), QString(QChar(resultFormatComplex)));
    settings->setValue(key + QLatin1String("Precision"), resultPrecision);

    key = KEY + QLatin1String("/Layout/");

    settings->setValue(key + QLatin1String("FormulaBookDockVisible"), formulaBookDockVisible);
    settings->setValue(key + QLatin1String("ConstantsDockVisible"), constantsDockVisible);
    settings->setValue(key + QLatin1String("FunctionsDockVisible"), functionsDockVisible);
    settings->setValue(key + QLatin1String("HistoryDockVisible"), historyDockVisible);
    settings->setValue(key + QLatin1String("KeypadMode"), static_cast<int>(keypadMode));
    settings->setValue(key + QLatin1String("WindowOnFullScreen"), windowOnfullScreen);
    settings->setValue(key + QLatin1String("KeypadVisible"),
        keypadMode == KeypadModeBasicWide
        || keypadMode == KeypadModeScientificWide
        || keypadMode == KeypadModeScientificNarrow
        || keypadMode == KeypadModeCustom);
    settings->setValue(key + QLatin1String("CustomKeypad"),
        QString::fromUtf8(QJsonDocument(serializeCustomKeypad(customKeypad)).toJson(QJsonDocument::Compact)));
    settings->setValue(key + QLatin1String("StatusBarVisible"), statusBarVisible);
    settings->setValue(key + QLatin1String("MenuBarVisible"), menuBarVisible);
    settings->setValue(key + QLatin1String("VariablesDockVisible"), variablesDockVisible);
    settings->setValue(key + QLatin1String("UserFunctionsDockVisible"), userFunctionsDockVisible);
    settings->setValue(key + QLatin1String("WindowAlwaysOnTop"), windowAlwaysOnTop);
    settings->setValue(key + QLatin1String("State"), windowState);
    settings->setValue(key + QLatin1String("WindowGeometry"), windowGeometry);
    settings->setValue(key + QLatin1String("ManualWindowGeometry"), manualWindowGeometry);
    settings->setValue(key + QLatin1String("BitfieldVisible"), bitfieldVisible);

    key = KEY + QLatin1String("/Display/");

    settings->setValue(key + QLatin1String("DisplayFont"), displayFont);
    settings->setValue(key + QLatin1String("ColorSchemeName"), colorScheme);


    delete settings;
}

char Settings::radixCharacter() const
{
    if (isRadixCharacterAuto() || isRadixCharacterBoth()) {
        #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            // In Qt 6, decimalPoint returns a QString
            QChar decimalPoint = QLocale().decimalPoint().at(0);
        #else
            // In Qt 5, decimalPoint returns a QChar
            QChar decimalPoint = QLocale().decimalPoint();
        #endif
        return decimalPoint.toLatin1();
    }

    return s_radixCharacter;
}

bool Settings::isRadixCharacterAuto() const
{
    return s_radixCharacter == 0;
}

bool Settings::isRadixCharacterBoth() const
{
    return s_radixCharacter == '*';
}

void Settings::setRadixCharacter(char c)
{
    s_radixCharacter = (c != ',' && c != '.' && c != '*') ? 0 : c;
}


// Settings migration from legacy (0.11 and before) to 0.12 (ConfigVersion 1200).
static void migrateSettings_legacyTo1200(QSettings* settings, const QString& KEY)
{
#ifdef SPEEDCRUNCH_PORTABLE
    // This is the same as the new path, but let's cut down
    QSettings* legacy = new QSettings(Settings::getConfigPath() + "/" + KEY + ".ini", QSettings::IniFormat);
#else
    QSettings* legacy = new QSettings(QSettings::NativeFormat, QSettings::UserScope, KEY, KEY);
#endif

    QString legacyFile = legacy->fileName();
    if (legacyFile != settings->fileName()) {
        // If the file names are different, we assume the legacy settings were in a
        // different location (most were, but e.g. not on the portable version) so we
        // copy everything over, then delete the old settings. On Windows, the file
        // name may also be a registry path, but the same reasoning applies.
        for (auto& key : legacy->allKeys())
            settings->setValue(key, legacy->value(key));

#ifdef Q_OS_WIN
        // On Windows, check if the legacy settings were in the registry; if so, just clear()
        // them.
        if (legacy->format() == QSettings::NativeFormat) {
            legacy->clear();
            delete legacy;
        } else {
            delete legacy;
            QFile::remove(legacyFile);
        }
#else
        // On other platforms, delete the legacy settings file.
        delete legacy;
        QFile::remove(legacyFile);
#endif
    } else {
        // If both settings objects point to the same location, removing the old stuff
        // may well break the new settings.
        delete legacy;
    }


    // ColorScheme -> ColorSchemeName
    QString colorSchemeName;
    switch (settings->value("SpeedCrunch/Display/ColorScheme", -1).toInt()) {
    case 0:
        colorSchemeName = "Terminal";
        break;
    case 1:
        colorSchemeName = "Standard";
        break;
    case 2:
        colorSchemeName = "Sublime";
        break;
    default:
        colorSchemeName = DefaultColorScheme;
        break;
    }
    settings->setValue("SpeedCrunch/Display/ColorSchemeName", colorSchemeName);
    settings->remove("SpeedCrunch/Display/ColorScheme");

    // DigitGrouping (bool) -> DigitGrouping (int)
    bool groupDigits = settings->value("SpeedCrunch/General/DigitGrouping", false).toBool();
    settings->setValue("SpeedCrunch/General/DigitGrouping", groupDigits ? 1 : 0);
}


QSettings* createQSettings(const QString& KEY)
{
    QSettings* settings = new QSettings(Settings::getConfigPath() + "/" + KEY + ".ini", QSettings::IniFormat);
    settings->setAtomicSyncRequired(!shouldUseNonAtomicSettingsSync(settings->fileName()));
    int ver = settings->value("ConfigVersion", 0).toInt();
    switch (ver) {
    case 0:
        migrateSettings_legacyTo1200(settings, KEY);
        break;
    }
    settings->setValue("ConfigVersion", ConfigVersion);
    return settings;
}
