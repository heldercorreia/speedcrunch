// SPDX-FileCopyrightText: 2008-2015, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/bookdock.h"

#include "core/book.h"
#include "core/evaluator.h"
#include "core/settings.h"
#include "core/mathdsl.h"
#include "gui/bookgeometrydiagrams.h"
#include "gui/editorutils.h"
#include "gui/menustyleutils.h"
#include "gui/oklchutils.h"

#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QFontMetricsF>
#include <QPalette>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

namespace {

constexpr QStringView diagramPrefix = u"book-diagram:geometry/";
constexpr int diagramObjectType = QTextFormat::UserObject;

class GeometryDiagramRenderer : public QObject, public QTextObjectInterface {
    Q_OBJECT
    Q_INTERFACES(QTextObjectInterface)

public:
    explicit GeometryDiagramRenderer(QTextBrowser* browser) : QObject(browser) {}

    QSizeF intrinsicSize(QTextDocument* document, int, const QTextFormat&) override
    {
        const QFontMetricsF metrics(document->defaultFont(), document->documentLayout()->paintDevice());
        return BookGeometryDiagrams::canvasSize * (metrics.height() / 16);
    }

    void drawObject(QPainter* painter, const QRectF& rect, QTextDocument*, int,
                    const QTextFormat& format) override
    {
        const auto* browser = static_cast<QTextBrowser*>(parent());
        const QString name = format.stringProperty(QTextFormat::ImageName);
        const QPalette& palette = browser->palette();
        BookGeometryDiagrams::draw(*painter, rect, QStringView(name).mid(diagramPrefix.size()),
                                   palette.color(QPalette::Text), palette.color(QPalette::Base));
    }
};

void setGeometryDiagramFormat(QTextDocument* document)
{
    // The HTML image gives the diagram a place and a text description.
    QTextCursor cursor = document->find(QString(QChar::ObjectReplacementCharacter));
    if (cursor.isNull())
        return;

    QTextCharFormat format = cursor.charFormat();
    if (format.stringProperty(QTextFormat::ImageName).startsWith(diagramPrefix)) {
        format.setObjectType(diagramObjectType);
        cursor.setCharFormat(format);
    }
}

QString toSuperscriptDigits(const QString& text)
{
    QString out;
    out.reserve(text.size());

    for (const QChar ch : text) {
        const QChar superscript = MathDsl::asciiDigitToSuperscript(ch);
        out += superscript.isNull() ? ch : superscript;
    }

    return out;
}

QString formatFormulaForEditorInsertion(const QString& text)
{
    QString formatted = text;

    // Convert exponent forms such as ^2 or ^(12) to superscripts.
    static const QRegularExpression s_powerPattern(
        QStringLiteral(R"(\^(?:\(([0-9]+)\)|([0-9]+)))"));
    int offset = 0;
    auto it = s_powerPattern.globalMatch(formatted);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString exponentDigits = match.captured(1).isEmpty()
            ? match.captured(2)
            : match.captured(1);
        const QString superscript = toSuperscriptDigits(exponentDigits);
        const int start = match.capturedStart(0) + offset;
        const int length = match.capturedLength(0);
        formatted.replace(start, length, superscript);
        offset += superscript.size() - length;
    }

    // Use regular spaces around the main binary operators.
    const QString ms(MathDsl::AddWrap);
    const QString replacement = ms + QStringLiteral("\\1") + ms;
    static const QRegularExpression s_binaryOps(
        QStringLiteral(R"(\s*([=+\-−·×/*])\s*)"));
    formatted.replace(s_binaryOps, replacement);

    return formatted;
}

} // namespace

void TextBrowser::changeEvent(QEvent* event)
{
    QTextBrowser::changeEvent(event);
    if (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::ApplicationPaletteChange
        || event->type() == QEvent::StyleChange) {
        emit paletteStyleChanged();
    }
}

BookDock::BookDock(QWidget* parent)
    : DockWidget(parent)
    , m_book(new Book(this))
{
    QWidget* widget = new QWidget(this);
    QVBoxLayout* bookLayout = new QVBoxLayout;
    bookLayout->setContentsMargins(0, 0, 0, 0);
    bookLayout->setSpacing(0);

    m_browser = new TextBrowser(this);
    m_browser->document()->documentLayout()->registerHandler(
        diagramObjectType, new GeometryDiagramRenderer(m_browser));
    m_browser->setFrameShape(QFrame::NoFrame);
    m_browser->setLineWrapMode(QTextEdit::NoWrap);
    m_browser->setOpenLinks(false);
    m_browser->setOpenExternalLinks(false);
    m_browser->setAutoFillBackground(true);
    updatePaletteStyle();

    connect(m_browser, SIGNAL(anchorClicked(const QUrl&)), SLOT(handleAnchorClick(const QUrl&)));
    connect(m_browser, SIGNAL(paletteStyleChanged()), SLOT(schedulePaletteStyleRefresh()));

    bookLayout->addWidget(m_browser);
    widget->setLayout(bookLayout);
    setWidget(widget);

    retranslateText();
    openPage(QUrl("index"));
}

void BookDock::setContentSurfaceColors(const QColor& background, const QColor& foreground)
{
    if (m_contentBackground == background && m_contentForeground == foreground)
        return;

    m_contentBackground = background;
    m_contentForeground = foreground;
    schedulePaletteStyleRefresh();
}

void BookDock::handleAnchorClick(const QUrl& url)
{
    if (url.scheme() == QLatin1String("function")) {
        emit functionSelected(url.path());
    } else if (url.toString().startsWith("formula:")) {
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
        QString expression = url.toString(QUrl::DecodeReserved).mid(8);
#else
        QString expression = url.toString().mid(8);
#endif
        expression = EditorUtils::normalizeExpressionOperatorsForEditorInput(expression);

        Evaluator* evaluator = Evaluator::instance();
        evaluator->setExpression(expression);
        if (evaluator->isValid()) {
            const QString interpreted = evaluator->interpretedExpression();
            if (!interpreted.isEmpty())
                expression = Evaluator::formatInterpretedExpressionForDisplay(interpreted, evaluator);
        }

        expression.replace(MathDsl::MulCrossOp, MathDsl::MulDotOp);
        expression = formatFormulaForEditorInsertion(expression);
        emit expressionSelected(expression);
    } else
        openPage(url);
}

void BookDock::openPage(const QUrl& url)
{
    const QString page = url.toString().isEmpty() ? QStringLiteral("index") : url.toString();
    const QString content = m_book->getPageContent(page);
    if (!content.isNull()) {
        m_pageContent = content;
        refreshCurrentPage();
    }
    m_currentPage = page;
}

void BookDock::retranslateText()
{
    setWindowTitle(tr("Formula Book"));
    const QString content = m_book->getCurrentPageContent();
    if (!content.isNull()) {
        m_pageContent = content;
        refreshCurrentPage();
    }
}

void BookDock::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateText();
    } else if (event->type() == QEvent::PaletteChange
               || event->type() == QEvent::ApplicationPaletteChange
               || event->type() == QEvent::StyleChange) {
        schedulePaletteStyleRefresh();
    } else {
        QDockWidget::changeEvent(event);
    }
}

QString BookDock::currentPage() const
{
    return m_currentPage.isEmpty() ? QStringLiteral("index") : m_currentPage;
}

int BookDock::missingContentWidth() const
{
    // Pages do not wrap, so their widest line sets the width they need.
    const QAbstractTextDocumentLayout* layout = m_browser->document()->documentLayout();
    const int contentWidth = qCeil(layout->documentSize().width());
    return qMax(0, contentWidth - m_browser->viewport()->width());
}

void BookDock::schedulePaletteStyleRefresh()
{
    if (m_refreshingPaletteStyle || m_paletteStyleRefreshPending)
        return;

    // Parent polishing sends several palette and style events in one pass.
    // Render the current page once after those changes settle.
    m_paletteStyleRefreshPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_paletteStyleRefreshPending = false;
        refreshCurrentPage();
    });
}

void BookDock::refreshCurrentPage()
{
    if (m_refreshingPaletteStyle)
        return;

    QScopedValueRollback<bool> refreshing(m_refreshingPaletteStyle, true);
    updatePaletteStyle();
    if (m_pageContent.isNull())
        return;

    const QString styledContent = applyPaletteStyle(m_pageContent);
    if (styledContent == m_styledContent)
        return;
    m_styledContent = styledContent;
    m_browser->setHtml(styledContent);
    setGeometryDiagramFormat(m_browser->document());
}

QString BookDock::applyPaletteStyle(const QString& content) const
{
    const QPalette palette = m_browser->palette();
    const QColor background = palette.color(QPalette::Base);
    const QColor foreground = palette.color(QPalette::Text);
    const bool system = Settings::instance()->dockAppearance == Settings::DockAppearanceSystem;
    const QColor formulaLink = system ? palette.color(QPalette::Link)
        : aaForegroundForBackground(background, palette.color(QPalette::Link));
    const QColor sectionLink = system ? formulaLink
        : generateSecondaryLinkFromBackground(background, formulaLink);
    const QString sectionLinkName =
        sectionLink.isValid() ? sectionLink.name() : foreground.name();
    QString styledContent = content;
    styledContent.replace(QStringLiteral("background-color: #ffffff;"),
                          QStringLiteral("background-color: %1;").arg(background.name()));
    styledContent.replace(QStringLiteral("color: #000000;"),
                          QStringLiteral("color: %1;").arg(foreground.name()));
    styledContent.replace(QStringLiteral("color: #555;"),
                          QStringLiteral("color: %1;").arg(sectionLinkName));
    styledContent.replace(QStringLiteral("color: SteelBlue;"),
                          QStringLiteral("color: %1;").arg(formulaLink.name()));
    return styledContent;
}

void BookDock::updatePaletteStyle()
{
    QPalette palette = Settings::instance()->dockAppearance == Settings::DockAppearanceSystem
        ? MenuStyle::systemPalette(m_browser) : this->palette();
    palette.setColor(QPalette::Base,
                     m_contentBackground.isValid()
                         ? m_contentBackground
                         : palette.color(QPalette::Window));
    palette.setColor(QPalette::Text,
                     m_contentForeground.isValid()
                         ? m_contentForeground
                         : palette.color(QPalette::WindowText));
    m_browser->setPalette(palette);
    m_browser->viewport()->setPalette(palette);
}

#include "bookdock.moc"
