// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/keybindings.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QSet>

namespace KeyBindings {

bool isConfigurable(const QString& id)
{
    static const QSet<QString> excluded = {
        QStringLiteral("editorComplete"),
        QStringLiteral("editorEvaluate"),
        QStringLiteral("sessionImport"),
        QStringLiteral("viewKeypadCustom"),
        QStringLiteral("viewKeypadZoom50"),
        QStringLiteral("viewKeypadZoom100"),
        QStringLiteral("viewKeypadZoom150"),
        QStringLiteral("viewKeypadZoom200"),
        QStringLiteral("settingsBehaviorAutoCompletionAcceptWithEnter"),
        QStringLiteral("settingsBehaviorAutoCompletionBuiltInFunctions"),
        QStringLiteral("settingsBehaviorAutoCompletionBuiltInVariables"),
        QStringLiteral("settingsBehaviorAutoCompletionLongFormUnits"),
        QStringLiteral("settingsBehaviorAutoCompletionUserFunctions"),
        QStringLiteral("settingsBehaviorAutoCompletionUserVariables"),
        QStringLiteral("settingsResultFormatPolarAngle"),
        QStringLiteral("settingsLaunchNewWindow"),
        QStringLiteral("settingsLaunchNewTab"),
        QStringLiteral("settingsResultFormatCustomDigits"),
        QStringLiteral("settingsRadixCharDot"),
        QStringLiteral("settingsRadixCharComma"),
        QStringLiteral("settingsRadixCharDefault"),
        QStringLiteral("settingsRadixCharBoth"),
        QStringLiteral("settingsBehaviorDigitGroupingOneSpace"),
        QStringLiteral("settingsBehaviorDigitGroupingTwoSpaces"),
        QStringLiteral("settingsBehaviorDigitGroupingThreeSpaces"),
        QStringLiteral("settingsBehaviorDigitGroupingNone"),
        QStringLiteral("settingsBehaviorDigitGroupingIntegerPartOnly"),
        QStringLiteral("settingsBehaviorAutoCompletion"),
        QStringLiteral("helpAbout"),
        QStringLiteral("helpDiagnostics"),
        QStringLiteral("helpDonate"),
        QStringLiteral("helpSource"),
        QStringLiteral("helpFeedback"),
        QStringLiteral("helpCommunity"),
        QStringLiteral("settingsBehaviorAlwaysOnTop"),
        QStringLiteral("settingsBehaviorSaveWindowPositionOnExit"),
        QStringLiteral("settingsOpenPrivateTabOnStartup"),
        QStringLiteral("settingsLaunchActivate"),
        QStringLiteral("settingsImaginaryUnitI"),
        QStringLiteral("settingsImaginaryUnitJ"),
        QStringLiteral("settingsUnitNegativeExponentSuperscript"),
        QStringLiteral("settingsUnitNegativeExponentFraction"),
        QStringLiteral("settingsResultFormatCartesian"),
        QStringLiteral("settingsResultFormatPolar"),
        QStringLiteral("settingsResultFormatTrigonometric"),
        QStringLiteral("settingsResultFormatCis"),
        QStringLiteral("settingsResultRoundingHalfAwayFromZero"),
        QStringLiteral("settingsResultRoundingHalfEven"),
        QStringLiteral("settingsResultRoundingTowardZero"),
        QStringLiteral("settingsResultRoundingTowardPositiveInfinity"),
        QStringLiteral("settingsResultRoundingTowardNegativeInfinity"),
        QStringLiteral("settingsSkipClearSessionConfirmation"),
        QStringLiteral("settingsBehaviorAutoResultToClipboard"),
        QStringLiteral("settingsBehaviorSimplifyResultExpressions"),
        QStringLiteral("settingsBehaviorPartialResults"),
        QStringLiteral("settingsRemoveTrailingZeroes"),
        QStringLiteral("settingsBehaviorResultSlots"),
        QStringLiteral("settingsBehaviorUpDownArrowNever"),
        QStringLiteral("settingsBehaviorUpDownArrowAlways"),
        QStringLiteral("settingsBehaviorUpDownArrowSingleLineOnly"),
        QStringLiteral("settingsBehaviorLeaveLastExpression"),
        QStringLiteral("settingsEditingEscapeClearsEditor"),
        QStringLiteral("settingsEditingCursorBlinking"),
        QStringLiteral("settingsEditingInputDigitGrouping"),
        QStringLiteral("settingsBehaviorAutoAns"),
        QStringLiteral("settingsEditingAutomaticInputAdjustments"),
        QStringLiteral("settingsMenuAppearanceSystem"),
        QStringLiteral("settingsMenuAppearanceSpeedCrunch"),
        QStringLiteral("settingsDockAppearanceSystem"),
        QStringLiteral("settingsDockAppearanceSpeedCrunch"),
        QStringLiteral("settingsStatusBarAppearanceSystem"),
        QStringLiteral("settingsStatusBarAppearanceSpeedCrunch"),
        QStringLiteral("settingsToolTipAppearanceSystem"),
        QStringLiteral("settingsToolTipAppearanceSpeedCrunch"),
        QStringLiteral("settingsKeypadAppearanceClassic"),
        QStringLiteral("settingsKeypadAppearanceModern"),
        QStringLiteral("settingsEditorDisplayClassic"),
        QStringLiteral("settingsEditorDisplayModern"),
        QStringLiteral("settingsAppearancePresetClassic"),
        QStringLiteral("settingsAppearancePresetModern"),
        QStringLiteral("settingsAppearancePresetCustom"),
        QStringLiteral("settingsDisplayFont"),
        QStringLiteral("settingsSystemColorModeFollowSystem"),
        QStringLiteral("settingsSystemColorModeLight"),
        QStringLiteral("settingsSystemColorModeDark"),
        QStringLiteral("settingsBehaviorHoverHighlightResults"),
        QStringLiteral("settingsBehaviorSyntaxHighlighting"),
        QStringLiteral("settingsDisplayColorSchemeCustom"),
        QStringLiteral("settingsResetAll"),
        QStringLiteral("settingsLanguage"),
        QStringLiteral("settingsBehaviorNumberFormat"),
        QStringLiteral("sessionExportHtml"),
        QStringLiteral("sessionExportJson"),
        QStringLiteral("sessionExportPlainText")
    };
    return !excluded.contains(id);
}

QList<Action> editorActions()
{
    const QString editing = QCoreApplication::translate("KeyBindings", "Expression Editor");
    const QString scrolling = QCoreApplication::translate("KeyBindings", "Result Scrolling");
    const QString view = QCoreApplication::translate("KeyBindings", "View");
    QList<Action> actions = {
        {QStringLiteral("editorConstants"), editing, QCoreApplication::translate("KeyBindings", "Search Constants"),
         {QKeySequence(Qt::CTRL | Qt::Key_K)}},
        {QStringLiteral("editorHistoryPrevious"), editing, QCoreApplication::translate("KeyBindings", "Previous History Expression"),
         {QKeySequence(Qt::Key_Up)}},
        {QStringLiteral("editorHistoryNext"), editing, QCoreApplication::translate("KeyBindings", "Next History Expression"),
         {QKeySequence(Qt::Key_Down)}},
        {QStringLiteral("settingsDisplayZoomIn"), view, QCoreApplication::translate("KeyBindings", "Zoom In"),
         {QKeySequence(Qt::SHIFT | Qt::Key_Up)}},
        {QStringLiteral("settingsDisplayZoomOut"), view, QCoreApplication::translate("KeyBindings", "Zoom Out"),
         {QKeySequence(Qt::SHIFT | Qt::Key_Down)}},
        {QStringLiteral("resultsScrollPageUp"), scrolling, QCoreApplication::translate("KeyBindings", "Scroll Results Up One Page"),
         {QKeySequence(Qt::Key_PageUp)}},
        {QStringLiteral("resultsScrollPageDown"), scrolling, QCoreApplication::translate("KeyBindings", "Scroll Results Down One Page"),
         {QKeySequence(Qt::Key_PageDown)}},
        {QStringLiteral("resultsScrollLineUp"), scrolling, QCoreApplication::translate("KeyBindings", "Scroll Results Up One Line"),
         {QKeySequence(Qt::SHIFT | Qt::Key_PageUp)}},
        {QStringLiteral("resultsScrollLineDown"), scrolling, QCoreApplication::translate("KeyBindings", "Scroll Results Down One Line"),
         {QKeySequence(Qt::SHIFT | Qt::Key_PageDown)}},
        {QStringLiteral("resultsScrollTop"), scrolling, QCoreApplication::translate("KeyBindings", "Scroll to First Result"), {}},
        {QStringLiteral("resultsScrollBottom"), scrolling, QCoreApplication::translate("KeyBindings", "Scroll to Last Result"), {}}
    };
#ifdef Q_OS_MACOS
    for (Action& action : actions) {
        if (action.id == QLatin1String("resultsScrollTop"))
            action.defaults = {QKeySequence(Qt::CTRL | Qt::Key_PageUp)};
        else if (action.id == QLatin1String("resultsScrollBottom"))
            action.defaults = {QKeySequence(Qt::CTRL | Qt::Key_PageDown)};
    }
#endif
    return actions;
}

QList<QKeySequence> shortcuts(const Action& action, const Overrides& overrides)
{
    const auto entry = overrides.constFind(action.id);
    if (!isConfigurable(action.id) || entry == overrides.cend())
        return action.defaults;
    QList<QKeySequence> result;
    for (const QString& text : entry.value()) {
        const QKeySequence sequence = QKeySequence::fromString(text, QKeySequence::PortableText);
        if (isValid(sequence) && !result.contains(sequence))
            result.append(sequence);
    }
    return result;
}

QStringList portableText(const QList<QKeySequence>& shortcuts)
{
    QStringList result;
    for (const QKeySequence& sequence : shortcuts)
        result.append(sequence.toString(QKeySequence::PortableText));
    return result;
}

QString nativeText(const QList<QKeySequence>& shortcuts)
{
    QStringList result;
    for (const QKeySequence& sequence : shortcuts)
        result.append(sequence.toString(QKeySequence::NativeText));
    return result.join(QStringLiteral(" / "));
}

QString plainText(const QString& menuText)
{
    QString result;
    for (int index = 0; index < menuText.size(); ++index) {
        const QChar character = menuText.at(index);
        if (character == QLatin1Char('&')) {
            if (index + 1 < menuText.size() && menuText.at(index + 1) == QLatin1Char('&'))
                ++index;
            else
                continue;
        }
        result.append(character);
    }
    return result;
}

bool conflicts(const QKeySequence& first, const QKeySequence& second)
{
    return !first.isEmpty() && !second.isEmpty()
        && (first.matches(second) != QKeySequence::NoMatch
            || second.matches(first) != QKeySequence::NoMatch);
}

bool startsWith(const QKeySequence& sequence, const QKeyEvent* event)
{
    if (sequence.isEmpty())
        return false;
    const QKeyCombination combination(event->modifiers() & ~Qt::KeypadModifier,
                                       static_cast<Qt::Key>(event->key()));
    return sequence[0] == combination;
}

bool matchesSingleKey(const QList<QKeySequence>& shortcuts, const QKeyEvent* event)
{
    for (const QKeySequence& sequence : shortcuts) {
        if (sequence.count() == 1 && startsWith(sequence, event))
            return true;
    }
    return false;
}

bool suppressStandardKey(const Overrides& overrides, const QKeyEvent* event)
{
    const struct {
        const char* id;
        QKeySequence::StandardKey key;
    } editing[] = {
        {"editUndo", QKeySequence::Undo}, {"editRedo", QKeySequence::Redo},
        {"editCut", QKeySequence::Cut}, {"editCopy", QKeySequence::Copy},
        {"editPaste", QKeySequence::Paste}, {"editSelectExpression", QKeySequence::SelectAll}
    };
    for (const auto& action : editing) {
        const QString id = QString::fromLatin1(action.id);
        if (overrides.contains(id) && event->matches(action.key)) {
            if (!matchesSingleKey(shortcuts({id, {}, {}, {}}, overrides), event))
                return true;
        }
    }
    return false;
}

bool isValid(const QKeySequence& sequence)
{
    if (sequence.isEmpty())
        return false;
    for (int index = 0; index < sequence.count(); ++index) {
        const Qt::Key key = sequence[index].key();
        if (key == Qt::Key_unknown || key == Qt::Key(0)
            || key == Qt::Key_Control || key == Qt::Key_Shift
            || key == Qt::Key_Alt || key == Qt::Key_Meta)
            return false;
    }
    return true;
}

} // namespace KeyBindings
