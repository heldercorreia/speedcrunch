// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/unitswidget.h"

#include "core/mathdsl.h"
#include "core/startupdiagnostics.h"
#include "core/units.h"
#include "gui/dockliststyle.h"
#include "gui/themedlineedit.h"
#include "gui/uiconfig.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPair>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStringList>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

enum Column { NameColumn, SymbolColumn, AliasesColumn, ColumnCount };

constexpr int kControlRowHorizontalPadding = 8;
constexpr int kControlRowSpacing = 6;

const char* quantitySourceText(UnitQuantity quantity)
{
    switch (quantity) {
    case UnitQuantity::Length: return QT_TRANSLATE_NOOP("UnitsWidget", "Length");
    case UnitQuantity::Time: return QT_TRANSLATE_NOOP("UnitsWidget", "Time");
    case UnitQuantity::Mass: return QT_TRANSLATE_NOOP("UnitsWidget", "Mass");
    case UnitQuantity::ElectricCurrent: return QT_TRANSLATE_NOOP("UnitsWidget", "Electric current");
    case UnitQuantity::ThermodynamicTemperature: return QT_TRANSLATE_NOOP("UnitsWidget", "Thermodynamic temperature");
    case UnitQuantity::AmountOfSubstance: return QT_TRANSLATE_NOOP("UnitsWidget", "Amount of substance");
    case UnitQuantity::LuminousIntensity: return QT_TRANSLATE_NOOP("UnitsWidget", "Luminous intensity");
    case UnitQuantity::Information: return QT_TRANSLATE_NOOP("UnitsWidget", "Information");
    case UnitQuantity::PlaneAngle: return QT_TRANSLATE_NOOP("UnitsWidget", "Plane angle");
    case UnitQuantity::SolidAngle: return QT_TRANSLATE_NOOP("UnitsWidget", "Solid angle");
    case UnitQuantity::Frequency: return QT_TRANSLATE_NOOP("UnitsWidget", "Frequency");
    case UnitQuantity::Force: return QT_TRANSLATE_NOOP("UnitsWidget", "Force");
    case UnitQuantity::Pressure: return QT_TRANSLATE_NOOP("UnitsWidget", "Pressure");
    case UnitQuantity::Stress: return QT_TRANSLATE_NOOP("UnitsWidget", "Stress");
    case UnitQuantity::Energy: return QT_TRANSLATE_NOOP("UnitsWidget", "Energy");
    case UnitQuantity::Work: return QT_TRANSLATE_NOOP("UnitsWidget", "Work");
    case UnitQuantity::AmountOfHeat: return QT_TRANSLATE_NOOP("UnitsWidget", "Amount of heat");
    case UnitQuantity::Power: return QT_TRANSLATE_NOOP("UnitsWidget", "Power");
    case UnitQuantity::RadiantFlux: return QT_TRANSLATE_NOOP("UnitsWidget", "Radiant flux");
    case UnitQuantity::ElectricCharge: return QT_TRANSLATE_NOOP("UnitsWidget", "Electric charge");
    case UnitQuantity::ElectricPotentialDifference: return QT_TRANSLATE_NOOP("UnitsWidget", "Electric potential difference");
    case UnitQuantity::Capacitance: return QT_TRANSLATE_NOOP("UnitsWidget", "Capacitance");
    case UnitQuantity::ElectricResistance: return QT_TRANSLATE_NOOP("UnitsWidget", "Electric resistance");
    case UnitQuantity::ElectricConductance: return QT_TRANSLATE_NOOP("UnitsWidget", "Electric conductance");
    case UnitQuantity::MagneticFlux: return QT_TRANSLATE_NOOP("UnitsWidget", "Magnetic flux");
    case UnitQuantity::MagneticFluxDensity: return QT_TRANSLATE_NOOP("UnitsWidget", "Magnetic flux density");
    case UnitQuantity::Inductance: return QT_TRANSLATE_NOOP("UnitsWidget", "Inductance");
    case UnitQuantity::CelsiusTemperature: return QT_TRANSLATE_NOOP("UnitsWidget", "Celsius temperature");
    case UnitQuantity::LuminousFlux: return QT_TRANSLATE_NOOP("UnitsWidget", "Luminous flux");
    case UnitQuantity::Illuminance: return QT_TRANSLATE_NOOP("UnitsWidget", "Illuminance");
    case UnitQuantity::ActivityReferredToARadionuclide: return QT_TRANSLATE_NOOP("UnitsWidget", "Activity referred to a radionuclide");
    case UnitQuantity::AbsorbedDose: return QT_TRANSLATE_NOOP("UnitsWidget", "Absorbed dose");
    case UnitQuantity::Kerma: return QT_TRANSLATE_NOOP("UnitsWidget", "Kerma");
    case UnitQuantity::DoseEquivalent: return QT_TRANSLATE_NOOP("UnitsWidget", "Dose equivalent");
    case UnitQuantity::CatalyticActivity: return QT_TRANSLATE_NOOP("UnitsWidget", "Catalytic activity");
    case UnitQuantity::Area: return QT_TRANSLATE_NOOP("UnitsWidget", "Area");
    case UnitQuantity::Volume: return QT_TRANSLATE_NOOP("UnitsWidget", "Volume");
    case UnitQuantity::Speed: return QT_TRANSLATE_NOOP("UnitsWidget", "Speed");
    case UnitQuantity::Velocity: return QT_TRANSLATE_NOOP("UnitsWidget", "Velocity");
    case UnitQuantity::DimensionlessRatio: return QT_TRANSLATE_NOOP("UnitsWidget", "Dimensionless ratio");
    case UnitQuantity::Acceleration: return QT_TRANSLATE_NOOP("UnitsWidget", "Acceleration");
    case UnitQuantity::DynamicViscosity: return QT_TRANSLATE_NOOP("UnitsWidget", "Dynamic viscosity");
    case UnitQuantity::KinematicViscosity: return QT_TRANSLATE_NOOP("UnitsWidget", "Kinematic viscosity");
    case UnitQuantity::MagneticFieldStrength: return QT_TRANSLATE_NOOP("UnitsWidget", "Magnetic field strength");
    case UnitQuantity::Unspecified: break;
    }
    return nullptr;
}

// Quantities that at least one built-in unit has, in UnitQuantity order.
QList<UnitQuantity> listedQuantities()
{
    QList<UnitQuantity> quantities;
    for (const UnitId id : builtInUnitIds()) {
        for (const UnitQuantity quantity : unitQuantities(id)) {
            if (quantitySourceText(quantity) && !quantities.contains(quantity))
                quantities.append(quantity);
        }
    }
    std::sort(quantities.begin(), quantities.end());
    return quantities;
}

// The editor shows the same text in its unit completion list.
QString translatedUnitDescription(const QString& source)
{
    if (source.isEmpty() || !isUnitLocalizedNameTranslatable(source))
        return source;
    return QCoreApplication::translate("Editor", source.toUtf8().constData());
}

bool hasSuperscriptPower(const QString& text)
{
    return std::any_of(text.cbegin(), text.cend(), MathDsl::isSuperscriptPowerChar);
}

// Prefers the symbol, which results also use. Falls back to the name when
// the symbol alone does not name this unit. The evaluator also reads a
// superscript in a symbol as a power, so in³ becomes a power of in and loses
// the display of the cubic inch.
QString insertedUnitText(UnitId id)
{
    const QString symbol = unitSymbol(id);
    const bool symbolNamesUnit = !symbol.isEmpty()
        && Units::isBuiltInUnitIdentifier(symbol)
        && unitIdForName(symbol) == id
        && !hasSuperscriptPower(symbol);
    return QStringLiteral("[%1]").arg(symbolNamesUnit ? symbol : unitName(id));
}

struct UnitRow {
    UnitId id;
    QString name;
    QString symbol;
    QString aliases;
    // The source text of the unit name in the user's language.
    QString description;
    QString insertedText;
    bool prefixed;
};

// Each built-in unit, followed by its prefixed forms.
const QList<UnitRow>& unitRows()
{
    static const QList<UnitRow> rows = []() {
        const QString separator = QStringLiteral(", ");
        QList<UnitRow> list;
        for (const UnitId id : builtInUnitIds()) {
            list.append({id, unitName(id), unitSymbol(id), unitAliases(id).join(separator),
                         QString::fromUtf8(unitLocalizedName(id)), insertedUnitText(id), false});
            for (const PrefixedUnitVariant& variant : prefixedUnitVariants(id)) {
                // The prefixed forms here have exact symbols, but keep the
                // same rule for a symbol with a power.
                const bool useSymbol = !variant.symbol.isEmpty() && !hasSuperscriptPower(variant.symbol);
                list.append({id, variant.name, variant.symbol, variant.aliases.join(separator),
                             variant.localizedName,
                             QStringLiteral("[%1]").arg(useSymbol ? variant.symbol : variant.name),
                             true});
            }
        }
        return list;
    }();
    return rows;
}

} // namespace

UnitsWidget::UnitsWidget(QWidget* parent, const InitialState& state)
    : QWidget(parent)
    , m_filterTimer(new QTimer(this))
    , m_quantity(new QComboBox(this))
    , m_quantityLabel(new QLabel(this))
    , m_units(new QTreeWidget(this))
    , m_noMatchLabel(new QLabel(m_units))
    , m_searchFilter(new ThemedLineEdit(this))
    , m_searchLabel(new QLabel(this))
    , m_prefixedUnits(new QCheckBox(this))
    , m_prefixedUnitsLayout(new QHBoxLayout)
{
    m_filterTimer->setInterval(500);
    m_filterTimer->setSingleShot(true);

    m_units->setAutoScroll(true);
    m_units->setColumnCount(ColumnCount);
    m_units->setEditTriggers(QTreeWidget::NoEditTriggers);
    m_units->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_units->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_units->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_units->setRootIsDecorated(false);
    m_units->setSelectionBehavior(QTreeWidget::SelectRows);
    DockListStyle::apply(m_units);

    m_noMatchLabel->setAlignment(Qt::AlignCenter);
    m_noMatchLabel->adjustSize();
    m_noMatchLabel->hide();
    m_searchFilter->setClearButtonEnabled(true);

    m_quantity->setEditable(false);
    m_quantity->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    QWidget* quantityBox = new QWidget(this);
    QHBoxLayout* quantityLayout = new QHBoxLayout;
    quantityLayout->addWidget(m_quantityLabel);
    quantityLayout->addWidget(m_quantity);
    quantityLayout->setSpacing(kControlRowSpacing);
    quantityLayout->setContentsMargins(kControlRowHorizontalPadding,
                                       UiConfig::DockSearchRowVerticalPadding,
                                       kControlRowHorizontalPadding,
                                       0);
    quantityBox->setLayout(quantityLayout);

    QWidget* searchBox = new QWidget(this);
    QHBoxLayout* searchLayout = new QHBoxLayout;
    searchLayout->addWidget(m_searchLabel);
    searchLayout->addWidget(m_searchFilter);
    searchLayout->setSpacing(kControlRowSpacing);
    searchLayout->setContentsMargins(kControlRowHorizontalPadding,
                                     UiConfig::DockSearchRowVerticalPadding,
                                     kControlRowHorizontalPadding,
                                     UiConfig::DockSearchRowVerticalPadding);
    searchBox->setLayout(searchLayout);

    // The left margin lines the check box up with the fields above it.
    QWidget* prefixedUnitsBox = new QWidget(this);
    m_prefixedUnitsLayout->addWidget(m_prefixedUnits);
    m_prefixedUnitsLayout->setContentsMargins(kControlRowHorizontalPadding,
                                              0,
                                              kControlRowHorizontalPadding,
                                              UiConfig::DockSearchRowVerticalPadding);
    prefixedUnitsBox->setLayout(m_prefixedUnitsLayout);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(quantityBox);
    layout->addWidget(searchBox);
    layout->addWidget(prefixedUnitsBox);
    layout->addWidget(m_units);
    setLayout(layout);

    QWidget::setTabOrder(m_searchFilter, m_prefixedUnits);
    QWidget::setTabOrder(m_prefixedUnits, m_units);
    setFocusProxy(m_searchFilter);

    {
        const QSignalBlocker searchBlocker(m_searchFilter);
        m_searchFilter->setText(state.searchText);
    }
    m_prefixedUnits->setChecked(state.showsPrefixedUnits);
    m_selectedQuantityKey = state.quantity;

    connect(m_filterTimer, &QTimer::timeout, this, &UnitsWidget::updateList);
    connect(m_quantity, &QComboBox::activated, this, [this]() {
        m_selectedQuantityKey = selectedQuantity();
        updateList();
    });
    connect(m_prefixedUnits, &QCheckBox::toggled, this, &UnitsWidget::updateList);
    connect(m_units, &QTreeWidget::itemActivated, this, &UnitsWidget::handleItemActivated);
    connect(m_searchFilter, &QLineEdit::textChanged, this, &UnitsWidget::triggerFilter);
    for (const QKeySequence& key : {QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Enter)}) {
        QShortcut* shortcut = new QShortcut(key, this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, &UnitsWidget::activateCurrentItem);
    }

    DockListStyle::refreshWhenExposed(this, [this]() {
        if (m_pendingRefresh)
            updateList();
    });
    retranslateText();
}

UnitsWidget::~UnitsWidget()
{
    m_filterTimer->stop();
}

void UnitsWidget::updateList()
{
    m_filterTimer->stop();
    if (DockListStyle::shouldDeferPopulation(this)) {
        StartupDiagnostics::AppearanceScope timing("Defer off-screen units dock list", this);
        m_pendingRefresh = true;
        return;
    }
    m_pendingRefresh = false;
    const QString term = m_searchFilter->text();
    const int quantityData = m_quantity->currentData().toInt();
    const bool showsPrefixed = m_prefixedUnits->isChecked();
    const QString filter = QString::number(quantityData)
        + (showsPrefixed ? QStringLiteral("|p|") : QStringLiteral("|-|")) + term;
    const bool unchanged = m_populated && filter == m_populatedFilter;
    StartupDiagnostics::AppearanceScope timing(unchanged
        ? "Skip unchanged units dock list" : "Populate units dock list", this);
    if (unchanged)
        return;

    setUpdatesEnabled(false);
    m_units->clear();
    QList<QTreeWidgetItem*> items;
    for (const UnitRow& unit : unitRows()) {
        if (unit.prefixed && !showsPrefixed)
            continue;
        if (quantityData >= 0
            && !unitQuantities(unit.id).contains(static_cast<UnitQuantity>(quantityData)))
            continue;
        QStringList row(ColumnCount);
        row[NameColumn] = unit.name;
        row[SymbolColumn] = unit.symbol;
        row[AliasesColumn] = unit.aliases;

        // The description is not shown but lets users search in their language.
        const QString description = translatedUnitDescription(unit.description);
        const auto matches = [&term](const QString& text) {
            return text.contains(term, Qt::CaseInsensitive);
        };
        if (!term.isEmpty() && !std::any_of(row.cbegin(), row.cend(), matches) && !matches(description))
            continue;

        QTreeWidgetItem* item = new QTreeWidgetItem(row);
        item->setData(NameColumn, Qt::UserRole, unit.insertedText);
        for (int column = 0; column < ColumnCount; ++column)
            item->setTextAlignment(column, Qt::AlignLeft | Qt::AlignVCenter);
        items.append(item);
    }
    m_units->addTopLevelItems(items);

    for (int column = 0; column < ColumnCount; ++column)
        m_units->resizeColumnToContents(column);

    if (m_units->topLevelItemCount() > 0) {
        m_noMatchLabel->hide();
        m_units->sortItems(NameColumn, Qt::AscendingOrder);
    } else {
        DockListStyle::showCenteredNoMatchLabel(m_units, m_noMatchLabel);
    }

    m_populatedFilter = filter;
    m_populated = true;
    setUpdatesEnabled(true);
}

void UnitsWidget::retranslateText()
{
    QStringList titles(ColumnCount);
    titles[NameColumn] = tr("Name");
    titles[SymbolColumn] = tr("Symbol");
    titles[AliasesColumn] = tr("Aliases");
    m_units->setHeaderLabels(titles);

    m_searchLabel->setText(tr("Search"));
    m_quantityLabel->setText(tr("Quantity"));
    m_prefixedUnits->setText(tr("Include prefixed units"));
    updateControlLabelWidths();
    m_noMatchLabel->setText(tr("No match found"));

    // Names sort in the user's language. Items keep the quantity as data.
    QList<QPair<QString, int>> quantities;
    for (const UnitQuantity quantity : listedQuantities())
        quantities.append({tr(quantitySourceText(quantity)), static_cast<int>(quantity)});
    std::sort(quantities.begin(), quantities.end(), [](const auto& a, const auto& b) {
        return QString::localeAwareCompare(a.first, b.first) < 0;
    });
    m_quantity->clear();
    m_quantity->addItem(tr("All"), -1);
    for (const auto& quantity : std::as_const(quantities))
        m_quantity->addItem(quantity.first, quantity.second);

    // The search also uses translated unit names, so filter again.
    m_populated = false;
    setSelectedQuantity(m_selectedQuantityKey);
}

const QTreeWidgetItem* UnitsWidget::currentItem() const
{
    return m_units->currentItem();
}

QString UnitsWidget::searchText() const
{
    return m_searchFilter->text();
}

void UnitsWidget::setSearchText(const QString& text)
{
    m_searchFilter->setText(text);
}

QString UnitsWidget::selectedQuantity() const
{
    const int data = m_quantity->currentData().toInt();
    if (data < 0)
        return QString();
    const char* source = quantitySourceText(static_cast<UnitQuantity>(data));
    return source ? QString::fromLatin1(source) : QString();
}

void UnitsWidget::setSelectedQuantity(const QString& quantity)
{
    int index = 0;
    for (int i = 1; i < m_quantity->count(); ++i) {
        const char* source = quantitySourceText(static_cast<UnitQuantity>(m_quantity->itemData(i).toInt()));
        if (source && quantity == QLatin1String(source)) {
            index = i;
            break;
        }
    }
    m_quantity->setCurrentIndex(index);
    m_selectedQuantityKey = selectedQuantity();
    updateList();
}

bool UnitsWidget::showsPrefixedUnits() const
{
    return m_prefixedUnits->isChecked();
}

void UnitsWidget::setShowsPrefixedUnits(bool show)
{
    // Toggling the check box updates the list.
    m_prefixedUnits->setChecked(show);
}

int UnitsWidget::missingColumnsWidth()
{
    if (m_pendingRefresh)
        updateList();
    return DockListStyle::missingColumnsWidth(m_units);
}

void UnitsWidget::handleItemActivated(QTreeWidgetItem* item, int /*column*/)
{
    if (item)
        emit unitSelected(item->data(NameColumn, Qt::UserRole).toString());
}

void UnitsWidget::activateCurrentItem()
{
    handleItemActivated(m_units->currentItem(), NameColumn);
}

void UnitsWidget::triggerFilter()
{
    m_filterTimer->stop();
    if (DockListStyle::shouldDeferPopulation(this)) {
        m_pendingRefresh = true;
        return;
    }
    m_filterTimer->start();
}

void UnitsWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        StartupDiagnostics::AppearanceContext context("Units dock LanguageChange");
        retranslateText();
        return;
    }
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange)
        QTimer::singleShot(0, this, [this]() { updateControlLabelWidths(); });
}

void UnitsWidget::updateControlLabelWidths()
{
    const int labelWidth = std::max(m_quantityLabel->sizeHint().width(), m_searchLabel->sizeHint().width());
    m_quantityLabel->setFixedWidth(labelWidth);
    m_searchLabel->setFixedWidth(labelWidth);
    QMargins margins = m_prefixedUnitsLayout->contentsMargins();
    margins.setLeft(kControlRowHorizontalPadding + labelWidth + kControlRowSpacing);
    m_prefixedUnitsLayout->setContentsMargins(margins);
}
