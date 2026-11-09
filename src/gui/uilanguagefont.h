// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GUI_UILANGUAGEFONT_H
#define GUI_UILANGUAGEFONT_H

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QList>
#include <QLocale>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QWidget>

// On Windows, shows the UI in the font Windows uses for the UI language.
//
// Qt's Windows UI font, Segoe UI, has no Chinese, Japanese, Korean, Thai or
// Indic letters, so Qt looks for a fallback font for that text. For these
// scripts it first loads every installed font to learn which ones support the
// script, which opens each font file on the UI thread. With many fonts
// installed, the first menu bar in Chinese took over a second to lay out. The
// first fallback Qt finds can also suit another language, such as a Japanese
// font for Chinese text when Windows itself is in English.
//
// Windows uses these fonts when its display language is the UI language. They
// include Latin letters, so all UI text renders without a fallback font.
namespace UiLanguageFont {

#ifdef Q_OS_WIN
namespace Detail {

struct State {
    QString family; // The applied family, or empty when the UI uses the system fonts.
    bool saved = false;
    QFont systemFont;
    QList<QPair<QByteArray, QFont>> systemClassFonts;
};

inline State& state()
{
    static State instance;
    return instance;
}

// The widget classes that Qt gives fonts of their own from the platform theme.
inline const QList<QByteArray>& widgetClasses()
{
    static const QList<QByteArray> classes {
        "QAbstractItemView", "QCheckBox", "QComboLineEdit", "QComboMenuItem",
        "QDockWidgetTitle", "QHeaderView", "QLabel", "QListBox", "QListView",
        "QMdiSubWindowTitleBar", "QMenu", "QMenuBar", "QMenuItem", "QMessageBox",
        "QPushButton", "QRadioButton", "QSmallFont", "QStatusBar", "QTipLabel",
        "QTitleBar", "QToolButton"
    };
    return classes;
}

inline QString familyFor(const QString& languageCode)
{
    // Resolve the language like MainWindow::createTranslator().
    const QLocale locale(languageCode == QLatin1String("C") ? QLocale().name() : languageCode);
    QString family;
    switch (locale.script()) {
    case QLocale::TraditionalHanScript:
        family = QStringLiteral("Microsoft JhengHei UI");
        break;
    case QLocale::SimplifiedHanScript:
        family = QStringLiteral("Microsoft YaHei UI");
        break;
    case QLocale::JapaneseScript:
        family = QStringLiteral("Yu Gothic UI");
        break;
    case QLocale::KoreanScript:
        family = QStringLiteral("Malgun Gothic");
        break;
    case QLocale::ThaiScript:
        family = QStringLiteral("Leelawadee UI");
        break;
    case QLocale::BengaliScript:
    case QLocale::DevanagariScript:
    case QLocale::GujaratiScript:
    case QLocale::GurmukhiScript:
    case QLocale::KannadaScript:
    case QLocale::MalayalamScript:
    case QLocale::OriyaScript:
    case QLocale::SinhalaScript:
    case QLocale::TamilScript:
    case QLocale::TeluguScript:
        family = QStringLiteral("Nirmala UI");
        break;
    default:
        break;
    }
    // Windows 10 and 11 install all of these. Keep the system fonts otherwise,
    // since requesting a missing family makes Qt search for a fallback font.
    if (!family.isEmpty() && !QFontDatabase::hasFamily(family))
        family.clear();
    return family;
}

// Qt only posts the font change to existing windows. Widgets created before
// that event arrives would inherit the previous font from their window, and
// lay out text in it. Windows don't pass their font on to children with a
// style sheet, such as the themed menu bar, either. Update every widget now.
inline void updateWidgetFonts()
{
    for (QWidget* widget : QApplication::allWidgets()) {
        QEvent event(QEvent::ApplicationFontChange);
        QCoreApplication::sendEvent(widget, &event);
    }
}

inline void applyFamily(const QString& family)
{
    State& s = state();
    if (family.isEmpty()) {
        if (!s.saved)
            return;
        QApplication::setFont(s.systemFont);
        for (const auto& classFont : s.systemClassFonts)
            QApplication::setFont(classFont.second, classFont.first.constData());
        s.saved = false;
        s.systemClassFonts.clear();
        updateWidgetFonts();
        return;
    }

    // Read the class fonts first. Setting the application font clears them.
    const QFont applicationFont = QApplication::font();
    QList<QPair<QByteArray, QFont>> classFonts;
    for (const QByteArray& name : widgetClasses()) {
        const QFont font = QApplication::font(name.constData());
        if (font != applicationFont)
            classFonts.append({name, font});
    }
    if (!s.saved) {
        s.saved = true;
        s.systemFont = applicationFont;
        s.systemClassFonts = classFonts;
    }
    // Keep each font's size and style. Only the family changes.
    const auto withFamily = [&family](QFont font) {
        font.setFamilies(QStringList(family));
        return font;
    };
    QApplication::setFont(withFamily(applicationFont));
    for (const auto& classFont : classFonts)
        QApplication::setFont(withFamily(classFont.second), classFont.first.constData());
    updateWidgetFonts();
}

} // namespace Detail
#endif

// Call this before creating widgets, so they don't lay out text twice, and
// again whenever the UI language changes.
inline void apply(const QString& languageCode)
{
#ifdef Q_OS_WIN
    Detail::State& s = Detail::state();
    const QString family = Detail::familyFor(languageCode);
    if (family == s.family)
        return;
    s.family = family;
    Detail::applyFamily(family);
#else
    Q_UNUSED(languageCode);
#endif
}

// QApplication::setStyle() restores the platform fonts of widget classes.
// Call this after it.
inline void reapplyAfterStyleChange()
{
#ifdef Q_OS_WIN
    const QString family = Detail::state().family;
    if (!family.isEmpty())
        Detail::applyFamily(family);
#endif
}

} // namespace UiLanguageFont

#endif
