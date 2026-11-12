// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GUI_SCRIPTEDTEXT_H
#define GUI_SCRIPTEDTEXT_H

#include <QFont>
#include <QList>
#include <QPainter>
#include <QString>
#include <QStyle>
#include <QTextCharFormat>
#include <QTextLayout>
#include <QTextOption>

// Unicode superscript and subscript characters, as in "μ₀" or "m·s⁻¹", are
// missing from many UI fonts, and a missing glyph makes Qt search every
// installed font for a fallback. Drawing them as ordinary characters raised
// or lowered in a smaller size keeps every glyph in the widget's own font.
namespace ScriptedText {

// Views that set this property to true draw their item text with scripts.
inline constexpr char ViewProperty[] = "scriptedItemText";

enum class Position { Normal, Superscript, Subscript };

struct Mapped {
    QChar character;
    Position position;
};

// Returns the plain character that stands in for c, and where to draw it.
// Characters that are not scripts may be swapped for a common lookalike.
inline Mapped map(QChar c)
{
    const char16_t u = c.unicode();
    switch (u) {
    case 0x00B9: return {QChar(u'1'), Position::Superscript}; // ¹
    case 0x00B2: return {QChar(u'2'), Position::Superscript}; // ²
    case 0x00B3: return {QChar(u'3'), Position::Superscript}; // ³
    case 0x2070: return {QChar(u'0'), Position::Superscript}; // ⁰
    case 0x2071: return {QChar(u'i'), Position::Superscript}; // ⁱ
    case 0x207A: return {QChar(u'+'), Position::Superscript}; // ⁺
    case 0x207B: return {QChar(0x2212), Position::Superscript}; // ⁻ as −
    case 0x207C: return {QChar(u'='), Position::Superscript}; // ⁼
    case 0x207D: return {QChar(u'('), Position::Superscript}; // ⁽
    case 0x207E: return {QChar(u')'), Position::Superscript}; // ⁾
    case 0x207F: return {QChar(u'n'), Position::Superscript}; // ⁿ
    case 0x208A: return {QChar(u'+'), Position::Subscript}; // ₊
    case 0x208B: return {QChar(0x2212), Position::Subscript}; // ₋ as −
    case 0x208C: return {QChar(u'='), Position::Subscript}; // ₌
    case 0x208D: return {QChar(u'('), Position::Subscript}; // ₍
    case 0x208E: return {QChar(u')'), Position::Subscript}; // ₎
    case 0x2090: return {QChar(u'a'), Position::Subscript}; // ₐ
    case 0x2091: return {QChar(u'e'), Position::Subscript}; // ₑ
    case 0x2092: return {QChar(u'o'), Position::Subscript}; // ₒ
    case 0x2093: return {QChar(u'x'), Position::Subscript}; // ₓ
    case 0x2095: return {QChar(u'h'), Position::Subscript}; // ₕ
    case 0x2096: return {QChar(u'k'), Position::Subscript}; // ₖ
    case 0x2097: return {QChar(u'l'), Position::Subscript}; // ₗ
    case 0x2098: return {QChar(u'm'), Position::Subscript}; // ₘ
    case 0x2099: return {QChar(u'n'), Position::Subscript}; // ₙ
    case 0x209A: return {QChar(u'p'), Position::Subscript}; // ₚ
    case 0x209B: return {QChar(u's'), Position::Subscript}; // ₛ
    case 0x209C: return {QChar(u't'), Position::Subscript}; // ₜ
    case 0x1D62: return {QChar(u'i'), Position::Subscript}; // ᵢ
    case 0x1D63: return {QChar(u'r'), Position::Subscript}; // ᵣ
    case 0x1D64: return {QChar(u'u'), Position::Subscript}; // ᵤ
    case 0x1D65: return {QChar(u'v'), Position::Subscript}; // ᵥ
    case 0x2C7C: return {QChar(u'j'), Position::Subscript}; // ⱼ
    case 0x03F5: return {QChar(0x03B5), Position::Normal}; // ϵ as ε
    case 0x210F: return {QChar(0x0127), Position::Normal}; // ℏ as ħ
    case 0x2126: return {QChar(0x03A9), Position::Normal}; // Ohm sign as Ω
    default:
        break;
    }
    if (u >= 0x2074 && u <= 0x2079) // ⁴ to ⁹
        return {QChar(char16_t(u'4' + (u - 0x2074))), Position::Superscript};
    if (u >= 0x2080 && u <= 0x2089) // ₀ to ₉
        return {QChar(char16_t(u'0' + (u - 0x2080))), Position::Subscript};
    return {c, Position::Normal};
}

// Returns whether drawing the text as is could need a fallback font.
inline bool needsLayout(const QString& text)
{
    for (const QChar c : text) {
        const Mapped mapped = map(c);
        if (mapped.position != Position::Normal || mapped.character != c)
            return true;
    }
    return false;
}

struct Run {
    int start;
    int length;
    Position position;
};

struct Converted {
    QString text;
    QList<Run> runs; // Superscript and subscript runs only.
};

inline Converted convert(const QString& text)
{
    Converted result;
    result.text.reserve(text.size());
    for (const QChar c : text) {
        const Mapped mapped = map(c);
        const int index = int(result.text.size());
        result.text.append(mapped.character);
        if (mapped.position == Position::Normal)
            continue;
        if (!result.runs.isEmpty()) {
            Run& last = result.runs.last();
            if (last.position == mapped.position && last.start + last.length == index) {
                ++last.length;
                continue;
            }
        }
        result.runs.append({index, 1, mapped.position});
    }
    return result;
}

// Returns the text with its plain stand-ins, for measuring without scripts.
inline QString plain(const QString& text)
{
    return needsLayout(text) ? convert(text).text : text;
}

inline QTextCharFormat scriptFormat(Position position, const QFont& font)
{
    QTextCharFormat format;
    format.setFont(font);
    if (position == Position::Superscript) {
        format.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
    } else if (position == Position::Subscript) {
        format.setVerticalAlignment(QTextCharFormat::AlignSubScript);
    }
    return format;
}

// Appends the converted text to a layout's text and formats. The base format
// applies to the whole appended text, including its scripts.
inline void append(const QString& text, const QTextCharFormat& base, const QFont& font,
                   QString* layoutText, QList<QTextLayout::FormatRange>* formats)
{
    const Converted converted = convert(text);
    const int offset = int(layoutText->size());
    layoutText->append(converted.text);
    int cursor = 0;
    const auto addRange = [&](int start, int length, const QTextCharFormat& format) {
        if (length <= 0 || !format.isValid() || format.properties().isEmpty())
            return;
        formats->append({offset + start, length, format});
    };
    for (const Run& run : converted.runs) {
        addRange(cursor, run.start - cursor, base);
        QTextCharFormat format = base;
        format.merge(scriptFormat(run.position, font));
        addRange(run.start, run.length, format);
        cursor = run.start + run.length;
    }
    addRange(cursor, int(converted.text.size()) - cursor, base);
}

struct Line {
    QString text;
    QList<QTextLayout::FormatRange> formats;
};

inline Line line(const QString& text, const QFont& font)
{
    Line result;
    append(text, QTextCharFormat(), font, &result.text, &result.formats);
    return result;
}

inline qreal width(const QString& text, const QFont& font)
{
    const Line converted = line(text, font);
    QTextLayout layout(converted.text, font);
    layout.setFormats(converted.formats);
    QTextOption option;
    option.setWrapMode(QTextOption::NoWrap);
    layout.setTextOption(option);
    layout.beginLayout();
    QTextLine textLine = layout.createLine();
    layout.endLayout();
    return textLine.isValid() ? textLine.naturalTextWidth() : 0;
}

// Shortens the text with an ellipsis until it fits, like QFontMetrics does.
inline QString elided(const QString& text, const QFont& font, qreal maxWidth)
{
    if (width(text, font) <= maxWidth)
        return text;
    const QString ellipsis(QChar(0x2026));
    for (int length = int(text.size()) - 1; length > 0; --length) {
        const QString candidate = text.left(length) + ellipsis;
        if (width(candidate, font) <= maxWidth)
            return candidate;
    }
    return QString();
}

// Draws one line of text, vertically centered in the rectangle.
inline void draw(QPainter* painter, const QRect& rect, const QString& text, const QFont& font,
                 const QColor& color, Qt::Alignment alignment, Qt::LayoutDirection direction)
{
    const Line converted = line(elided(text, font, rect.width()), font);
    if (converted.text.isEmpty())
        return;
    QTextLayout layout(converted.text, font);
    layout.setFormats(converted.formats);
    QTextOption option;
    option.setTextDirection(direction);
    option.setAlignment(QStyle::visualAlignment(direction, alignment & Qt::AlignHorizontal_Mask));
    option.setWrapMode(QTextOption::NoWrap);
    layout.setTextOption(option);
    layout.beginLayout();
    QTextLine textLine = layout.createLine();
    if (!textLine.isValid()) {
        layout.endLayout();
        return;
    }
    textLine.setLineWidth(rect.width());
    layout.endLayout();
    painter->save();
    painter->setClipRect(rect, Qt::IntersectClip);
    painter->setPen(color);
    layout.draw(painter, QPointF(rect.left(), rect.top() + (rect.height() - textLine.height()) / 2));
    painter->restore();
}

// Returns escaped rich text with scripts as <sup> and <sub> elements.
inline QString toHtml(const QString& text)
{
    const Converted converted = convert(text);
    QString result;
    int cursor = 0;
    for (const Run& run : converted.runs) {
        result += converted.text.mid(cursor, run.start - cursor).toHtmlEscaped();
        const QLatin1String tag(run.position == Position::Superscript ? "sup" : "sub");
        result += QLatin1Char('<') + tag + QLatin1Char('>')
            + converted.text.mid(run.start, run.length).toHtmlEscaped()
            + QLatin1String("</") + tag + QLatin1Char('>');
        cursor = run.start + run.length;
    }
    result += converted.text.mid(cursor).toHtmlEscaped();
    return result;
}

} // namespace ScriptedText

#endif // GUI_SCRIPTEDTEXT_H
