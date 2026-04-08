// This file is part of the SpeedCrunch project
// Copyright (C) 2015 Pol Welter <polwelter@gmail.com>
// Copyright (C) 2016 @heldercorreia
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

#include "units.h"

#include "core/unicodechars.h"
#include "quantity.h"
#include "rational.h"

#include <QString>
#include <QStringList>
#include <QSet>

namespace {

const QMap<QString, QString>& s_unitShortNames()
{
    static const QMap<QString, QString> names = {
        {"meter", "m"},
        {"second", "s"},
        {"kilogram", "kg"},
        {"ampere", "A"},
        {"mole", "mol"},
        {"candela", "cd"},
        {"kelvin", "K"},
        {"bit", "b"},
        {"byte", "B"},
        {"sqmeter", "m2"},
        {"cbmeter", "m3"},
        {"newton", "N"},
        {"hertz", "Hz"},
        {"joule", "J"},
        {"watt", "W"},
        {"pascal", "Pa"},
        {"coulomb", "C"},
        {"volt", "V"},
        {"ohm", QString::fromUtf8("Ω")},
        {"farad", "F"},
        {"tesla", "T"},
        {"weber", "Wb"},
        {"henry", "H"},
        {"siemens", "S"},
        {"becquerel", "Bq"},
        {"gray", "Gy"},
        {"sievert", "Sv"},
        {"katal", "kat"},
        {"steradian", "sr"},
        {"lumen", "lm"},
        {"lux", "lx"},
        {"metric_ton", "t"},
        {"gram", "g"},
        {"liter", "L"},
        {"electronvolt", "eV"},
        {"degree", QString(UnicodeChars::DegreeSign)},
        {"deg", QString(UnicodeChars::DegreeSign)}
    };
    return names;
}

const QList<QPair<QString, QString>>& s_siPrefixSymbols()
{
    static const QList<QPair<QString, QString>> prefixes = {
        {"quecto", "q"}, {"ronto", "r"}, {"yocto", "y"}, {"zepto", "z"},
        {"atto", "a"}, {"femto", "f"}, {"pico", "p"}, {"nano", "n"},
        {"micro", QString::fromUtf8("µ")}, {"milli", "m"}, {"centi", "c"}, {"deci", "d"},
        {"deca", "da"}, {"hecto", "h"}, {"kilo", "k"}, {"mega", "M"},
        {"giga", "G"}, {"tera", "T"}, {"peta", "P"}, {"exa", "E"},
        {"zetta", "Z"}, {"yotta", "Y"}, {"ronna", "R"}, {"quetta", "Q"}
    };
    return prefixes;
}

bool shouldAddSiPrefixedShortAlias(const QString& prefixSymbol, const QString& unitShortName)
{
    if (prefixSymbol != QLatin1String("da") && prefixSymbol != QLatin1String("h"))
        return true;

    const QString alias = prefixSymbol + unitShortName;
    return alias == QLatin1String("hPa")
           || alias == QLatin1String("hm")
           || alias == QLatin1String("hL")
           || alias == QLatin1String("hl")
           || alias == QLatin1String("dag");
}

bool isUnitIdentifierChar(const QChar& ch)
{
    return ch.isLetterOrNumber() || ch == QChar('_')
           || ch == QChar(0x00B5) // µ
           || ch == QChar(0x03A9); // Ω
}

} // namespace

#define UNIT_CACHE(name, value) \
    const Quantity Units::name() \
    { \
        if (!m_cache.contains(#name)) \
            m_cache[#name] = value; \
        return (m_cache[#name]); \
    }

#define BASE_UNIT_CACHE(name, dimension) \
    const Quantity Units::name() \
    { \
        if (!m_cache.contains(#name)) { \
            Quantity name(1); \
            name.modifyDimension(dimension, 1); \
            m_cache[#name] = name; \
        } \
        return m_cache[#name]; \
    }

QHash<QMap<QString, Rational>, Unit> Units::m_matchLookup;
QMap<QString, Quantity> Units::m_cache;


void Units::pushUnit(Quantity q, QString name)
{
    q.cleanDimension();
    Unit u(name, q);
    m_matchLookup.insert(q.getDimension(), u);
}

/*
 * initialize the lookup table for automatic matching
 */
void Units::initTable()
{
    m_matchLookup.clear();
    pushUnit(joule(), "joule");                                 // energy
    pushUnit(newton(), "newton");                               // force
    pushUnit(watt(), "watt");                                   // power
    pushUnit(pascal(), "pascal");                               // pressure or energy density
    pushUnit(coulomb(), "coulomb");                             // charge
    pushUnit(volt(), "volt");                                   // electrical potential
    pushUnit(volt()*volt()*ampere(), "volt² ampere");           // canonical mixed electrical-power form
    pushUnit(volt()*volt()/ampere(), "volt² ampere⁻¹");         // canonical electrical mixed form
    pushUnit(ohm(), "ohm");                                     // el. resistance
    pushUnit(siemens(), "siemens");                             // el. conductance
    pushUnit(ohm()*meter(), "ohm meter");                       // el. resistivity
    pushUnit(siemens()/meter(), "siemens meter⁻¹");             // el. conductivity
    pushUnit(siemens()/meter()/mole(), "siemens meter⁻¹ mole⁻¹"); // molar conductivity
    pushUnit(farad(), "farad");                                 // capacitance
    pushUnit(tesla(), "tesla");                                 // magnetic flux density
    pushUnit(weber(), "weber");                                 // magnetic flux
    pushUnit(henry(), "henry");                                 // inductance
    pushUnit(coulomb()/cbmeter(), "coulomb meter⁻³");           // electric charge density
    pushUnit(coulomb()/sqmeter(), "coulomb meter⁻²");           // surface charge density or el. flux
    pushUnit(coulomb()/kilogram(), "coulomb kilogram⁻¹");       // exposure
    pushUnit(farad()/meter(), "farad meter⁻¹");                 // permittivity
    pushUnit(henry()/meter(), "henry meter⁻¹");                 // permeability
    pushUnit(Quantity(1)/meter()/tesla(), "meter⁻¹ tesla⁻¹");   // inverse meter per tesla
    pushUnit(joule()/kilogram()/kelvin(), "joule kilogram⁻¹ kelvin⁻¹"); // specific heat capacity
    pushUnit(joule()/mole()/kelvin(), "joule mole⁻¹ kelvin⁻¹");         // molar heat capacity
    pushUnit(mole()/second()/cbmeter(), "mole second⁻¹ meter⁻³");       // catalytic activity
    pushUnit(newton()/meter(), "newton meter⁻¹");               // surface tension
    pushUnit(pascal()*second(), "pascal second");               // dynamic viscosity
    pushUnit(volt()/meter(), "volt meter⁻¹");                   // el. field
    pushUnit(watt()/meter()/kelvin(), "watt meter⁻¹ kelvin⁻¹"); // thermal conductivity
    pushUnit(watt()/sqmeter(), "watt/meter²");                  // heat flux
    pushUnit(joule()/kelvin(), "joule/kelvin");                 // entropy or heat capacity
    pushUnit(joule()/kilogram(), "joule/kilogram");             // specific energy
    pushUnit(kilogram()*sqmeter()*volt()/(second()*second()*second()*second()),
             "kilogram meter² volt second⁻⁴");                  // canonical mixed base+derived form
}

void Units::findUnit(Quantity& q)
{
    QString unit_name = "";
    CNumber unit(1);
    q.cleanDimension();

    if (m_matchLookup.isEmpty())
        initTable();

    const QMap<QString, Rational> dim = q.getDimension();
    // Policy: only auto-canonicalize dimensions that map to a single,
    // overwhelmingly standard named unit without major semantic collision.
    // Example: N·m is intentionally kept explicit in multiplication code
    // (see Quantity::operator*) and only becomes J on explicit conversion.
    // These denylist entries intentionally keep expressions expanded to
    // avoid silently choosing one meaning among multiple valid ones.
    const auto denyAutoCanonicalization = [&](const QString& candidateName,
                                              const QMap<QString, Rational>& candidateDim) {
        auto makeDim = [](std::initializer_list<std::pair<const char*, int>> items) {
            QMap<QString, Rational> d;
            for (const auto& item : items)
                d.insert(QString::fromLatin1(item.first), Rational(item.second));
            return d;
        };

        static const QMap<QString, Rational> perSecondDim =
            makeDim({{"time", -1}});
        static const QMap<QString, Rational> specificEnergyDim =
            makeDim({{"length", 2}, {"time", -2}});

        const QString name = candidateName.toLower();
        if ((name == QLatin1String("hertz") || name == QLatin1String("becquerel"))
            && candidateDim == perSecondDim)
        {
            return true;
        }
        if ((name == QLatin1String("gray") || name == QLatin1String("sievert"))
            && candidateDim == specificEnergyDim)
        {
            return true;
        }
        return false;
    };
    if (m_matchLookup.contains(dim)) {
        Unit temp(m_matchLookup[dim]);
        // For denylisted dimensions, keep base/compound form instead of forcing
        // a potentially ambiguous derived-unit name.
        if (!denyAutoCanonicalization(temp.name, dim))
            q.setDisplayUnit(temp.value.numericValue(), temp.name);
    } else {
        const auto superscriptFromExponent = [](const Rational& value) {
            QString exponent = value.toString();
            if (exponent.contains('/'))
                exponent = "^(" + exponent+')';
            else if (exponent == "1")
                exponent = "";
            else
                exponent = '^' + exponent;

            if (exponent == QLatin1String("^0")) return QString::fromUtf8("⁰");
            if (exponent == QLatin1String("^2")) return QString::fromUtf8("²");
            if (exponent == QLatin1String("^3")) return QString::fromUtf8("³");
            if (exponent == QLatin1String("^4")) return QString::fromUtf8("⁴");
            if (exponent == QLatin1String("^5")) return QString::fromUtf8("⁵");
            if (exponent == QLatin1String("^6")) return QString::fromUtf8("⁶");
            if (exponent == QLatin1String("^7")) return QString::fromUtf8("⁷");
            if (exponent == QLatin1String("^8")) return QString::fromUtf8("⁸");
            if (exponent == QLatin1String("^9")) return QString::fromUtf8("⁹");
            if (exponent == QLatin1String("^-1")) return QString::fromUtf8("⁻¹");
            if (exponent == QLatin1String("^-2")) return QString::fromUtf8("⁻²");
            if (exponent == QLatin1String("^-3")) return QString::fromUtf8("⁻³");
            if (exponent == QLatin1String("^-4")) return QString::fromUtf8("⁻⁴");
            if (exponent == QLatin1String("^-5")) return QString::fromUtf8("⁻⁵");
            if (exponent == QLatin1String("^-6")) return QString::fromUtf8("⁻⁶");
            if (exponent == QLatin1String("^-7")) return QString::fromUtf8("⁻⁷");
            if (exponent == QLatin1String("^-8")) return QString::fromUtf8("⁻⁸");
            if (exponent == QLatin1String("^-9")) return QString::fromUtf8("⁻⁹");
            return exponent;
        };

        struct UnitFactor {
            QString name;
            Rational exponent;
        };

        QList<UnitFactor> factors;
        const auto appendFactor = [&](const QString& name, const Rational& exponent) {
            factors.append(UnitFactor{name, exponent});
        };

        // Prefer recognizable derived SI units before base-unit fallback.
        QMap<QString, Rational> remaining = q.getDimension();
        static const QList<Unit> derivedCandidates = {
            Unit("tesla", tesla()),
            Unit("weber", weber()),
            Unit("henry", henry()),
            Unit("farad", farad()),
            Unit("ohm", ohm()),
            Unit("siemens", siemens()),
            Unit("volt", volt()),
            Unit("coulomb", coulomb()),
            Unit("joule", joule()),
            Unit("newton", newton()),
            Unit("pascal", pascal()),
            Unit("watt", watt()),
            Unit("hertz", hertz()),
            Unit("becquerel", becquerel()),
            Unit("gray", gray()),
            Unit("sievert", sievert()),
            Unit("katal", katal()),
            Unit("lumen", lumen()),
            Unit("lux", lux()),
            Unit("steradian", steradian())
        };

        bool progress = true;
        while (progress) {
            progress = false;
            for (const Unit& candidate : derivedCandidates) {
                Quantity qc(candidate.value);
                qc.cleanDimension();
                const auto cdim = qc.getDimension();
                if (cdim.isEmpty() || cdim.size() < 2)
                    continue;

                bool firstRatio = true;
                Rational ratio;
                bool fits = true;
                for (auto it = cdim.constBegin(); it != cdim.constEnd(); ++it) {
                    if (!remaining.contains(it.key())) {
                        fits = false;
                        break;
                    }
                    const Rational rv = remaining.value(it.key());
                    const Rational cv = it.value();
                    Rational r(rv.numerator() * cv.denominator(),
                               rv.denominator() * cv.numerator());
                    if (firstRatio) {
                        ratio = r;
                        firstRatio = false;
                    } else if (r != ratio) {
                        fits = false;
                        break;
                    }
                }

                if (!fits || firstRatio || ratio.isZero() || ratio.denominator() != 1)
                    continue;

                const Rational ratioInt(ratio.numerator(), 1);
                for (auto it = cdim.constBegin(); it != cdim.constEnd(); ++it) {
                    const QString key = it.key();
                    remaining[key] -= ratioInt * it.value();
                    if (remaining[key].isZero())
                        remaining.remove(key);
                }
                if (!denyAutoCanonicalization(candidate.name, qc.getDimension()))
                    appendFactor(candidate.name, ratioInt);
                else {
                    // Keep base-unit expansion for denied automatic canonicalizations.
                    // Revert the tentative reduction and continue searching.
                    for (auto it = cdim.constBegin(); it != cdim.constEnd(); ++it) {
                        const QString key = it.key();
                        remaining[key] += ratioInt * it.value();
                        if (remaining[key].isZero())
                            remaining.remove(key);
                    }
                    continue;
                }
                progress = true;
                break;
            }
        }

        // Keep auto-generated base-unit products in canonical output order:
        // kilogram, meter, second, ampere, kelvin, mole, candela, information.
        // (Angle units such as radian/steradian are represented explicitly when used.)
        const QStringList preferredOrder = {
            "mass",
            "length",
            "time",
            "el. current",
            "temperature",
            "amount",
            "luminous intensity",
            "information"
        };

        auto appendDimension = [&](const QString& key, const Rational& value) {
            if (key == "length")
                appendFactor("meter", value);
            else if (key == "time")
                appendFactor("second", value);
            else if (key == "mass")
                appendFactor("kilogram", value);
            else if (key == "el. current")
                appendFactor("ampere", value);
            else if (key == "amount")
                appendFactor("mole", value);
            else if (key == "luminous intensity")
                appendFactor("candela", value);
            else if (key == "temperature")
                appendFactor("kelvin", value);
            else if (key == "information")
                appendFactor("byte", value);
            else
                appendFactor(key, value);
        };

        for (const QString& key : preferredOrder) {
            if (remaining.contains(key))
                appendDimension(key, remaining.value(key));
        }
        for (auto i = remaining.constBegin(); i != remaining.constEnd(); ++i) {
            if (!preferredOrder.contains(i.key()))
                appendDimension(i.key(), i.value());
        }
        // Rendering policy for auto-generated unit products:
        // 1) If there is at least one positive exponent, render negative exponents
        //    in the denominator (for readability and textbook familiarity).
        //    Example: kilogram⋅meter⋅second⁻¹ -> kilogram⋅meter / second.
        // 2) If there are no positive exponents, keep pure inverse form with
        //    negative superscripts to avoid "1 / ..." noise.
        //    Example: second⁻¹ (not 1 / second).
        //
        // This policy only changes presentation. It does not affect dimensions.
        QStringList numeratorTerms;
        QStringList denominatorTerms;
        const Rational zero(0);

        for (const UnitFactor& factor : factors) {
            if (factor.exponent > zero) {
                numeratorTerms << (factor.name + superscriptFromExponent(factor.exponent));
            } else if (factor.exponent < zero) {
                const Rational absExponent(-factor.exponent.numerator(),
                                           factor.exponent.denominator());
                denominatorTerms << (factor.name + superscriptFromExponent(absExponent));
            }
        }

        const QString numeratorText = numeratorTerms.join(QString::fromUtf8("⋅"));
        const QString denominatorText = denominatorTerms.join(QString::fromUtf8("⋅"));
        if (!numeratorTerms.isEmpty() && !denominatorTerms.isEmpty()) {
            unit_name = ' ' + numeratorText + " / ";
            if (denominatorTerms.size() > 1)
                unit_name += '(' + denominatorText + ')';
            else
                unit_name += denominatorText;
        } else if (!numeratorTerms.isEmpty()) {
            unit_name = ' ' + numeratorText;
        } else if (!denominatorTerms.isEmpty()) {
            // Keep inverse-only units in exponent form (s⁻¹, m⁻¹⋅s⁻², ...).
            QStringList inverseTerms;
            for (const UnitFactor& factor : factors) {
                if (factor.exponent < zero)
                    inverseTerms << (factor.name + superscriptFromExponent(factor.exponent));
            }
            unit_name = ' ' + inverseTerms.join(QString::fromUtf8("⋅"));
        }

        q.setDisplayUnit(unit, unit_name.trimmed());
    }
}

// This list contains the units that wil be set as builtin variables by the evaluator.
const QList<Unit> Units::getList()
{
    enum SiPrefixPolicy {
        NoSiPrefixes = 0,
        PositiveSiPrefixes = 1 << 0,
        NegativeSiPrefixes = 1 << 1
    };
    struct UnitAliasSpec {
        QString longName;
        Quantity value;
        QString shortName;
        QString alternateShortName;
        SiPrefixPolicy siPrefixPolicy;
    };
    struct PrefixSpec {
        QString longName;
        QString symbol;
        int power;
        Quantity value;
    };

    QList<Unit> result;
    QSet<QString> seenNames;
    QList<UnitAliasSpec> unitAliasSpecs;

    const auto addUnique = [&](const QString& name, const Quantity& value) {
        if (name.isEmpty() || seenNames.contains(name))
            return;
        result.append(Unit(name, value));
        seenNames.insert(name);
    };
    const auto addBareUnit = [&](const QString& name, const Quantity& value) {
        addUnique(name, value);
    };
    const auto addUnitWithAliasesImpl = [&](const QString& name,
                                            const Quantity& value,
                                            const QString& shortName,
                                            const QString& alternateShortName,
                                            SiPrefixPolicy siPrefixPolicy = static_cast<SiPrefixPolicy>(
                                                PositiveSiPrefixes | NegativeSiPrefixes)) {
        addUnique(name, value);
        if (!shortName.isEmpty())
            addUnique(shortName, value);
        if (!alternateShortName.isEmpty())
            addUnique(alternateShortName, value);

        UnitAliasSpec unitSpec = {
            name, value, shortName, alternateShortName, siPrefixPolicy
        };
        unitAliasSpecs.append(unitSpec);
    };
    const auto addUnit = [&](const QString& name,
                             const Quantity& value,
                             const QString& shortName = QString(),
                             SiPrefixPolicy siPrefixPolicy = static_cast<SiPrefixPolicy>(
                                 PositiveSiPrefixes | NegativeSiPrefixes)) {
        addUnitWithAliasesImpl(name, value, shortName, QString(), siPrefixPolicy);
    };
    const auto addUnitWithAliases = [&](const QString& name,
                                        const Quantity& value,
                                        const QString& shortName,
                                        const QString& alternateShortName) {
        addUnitWithAliasesImpl(name,
                               value,
                               shortName,
                               alternateShortName,
                               static_cast<SiPrefixPolicy>(PositiveSiPrefixes | NegativeSiPrefixes));
    };

    // Dimension primitives used by the unit system:
    // SI base units plus one internal primitive for information quantities.
    addUnit("meter", meter(), "m");
    addUnit("second", second(), "s");
    addUnit("kilogram", kilogram(), "kg");
    addUnit("ampere", ampere(), "A");
    addUnit("mole", mole(), "mol");
    addUnit("candela", candela(), "cd");
    addUnit("kelvin", kelvin(), "K");
    addUnit("bit", bit(), "b", PositiveSiPrefixes);

    // SI decimal prefixes (long-form names and selected symbol aliases).
    addBareUnit("quecto", quecto());
    addBareUnit("ronto", ronto());
    addBareUnit("yocto", yocto());
    addBareUnit("zepto", zepto());
    addBareUnit("atto", atto());
    addBareUnit("femto", femto());
    addBareUnit("pico", pico());
    addBareUnit("nano", nano());
    addBareUnit("micro", micro());
    addBareUnit("milli", milli());
    addBareUnit("centi", centi());
    addBareUnit("deci", deci());
    addBareUnit("deca", deca());
    addBareUnit("hecto", hecto());
    addBareUnit("kilo", kilo());
    addBareUnit("mega", mega());
    addBareUnit("giga", giga());
    addBareUnit("tera", tera());
    addBareUnit("peta", peta());
    addBareUnit("exa", exa());
    addBareUnit("zetta", zetta());
    addBareUnit("yotta", yotta());
    addBareUnit("ronna", ronna());
    addBareUnit("quetta", quetta());
    addBareUnit("µ", micro());
    addBareUnit("μ", micro());

    // Binary prefixes (IEC).
    addBareUnit("kibi", kibi());
    addBareUnit("mebi", mebi());
    addBareUnit("gibi", gibi());
    addBareUnit("tebi", tebi());
    addBareUnit("pebi", pebi());
    addBareUnit("exbi", exbi());
    addBareUnit("zebi", zebi());
    addBareUnit("yobi", yobi());
    addBareUnit("robi", robi());
    addBareUnit("quebi", quebi());

    // SI-derived and geometric units.
    addUnit("sqmeter", sqmeter(), "m2");
    addUnit("cbmeter", cbmeter(), "m3");
    addUnit("newton", newton(), "N");
    addUnit("hertz", hertz(), "Hz");
    addUnit("joule", joule(), "J");
    addUnit("watt", watt(), "W");
    addUnit("pascal", pascal(), "Pa");
    addUnit("coulomb", coulomb(), "C");
    addUnit("volt", volt(), "V");
    addUnit("ohm", ohm(), QString::fromUtf8("Ω"));
    addUnit("farad", farad(), "F");
    addUnit("tesla", tesla(), "T");
    addUnit("weber", weber(), "Wb");
    addUnit("henry", henry(), "H");
    addUnit("siemens", siemens(), "S");
    addUnit("becquerel", becquerel(), "Bq");
    addUnit("gray", gray(), "Gy");
    addUnit("sievert", sievert(), "Sv");
    addUnit("katal", katal(), "kat");
    addUnit("steradian", steradian(), "sr");
    addUnit("lumen", lumen(), "lm");
    addUnit("lux", lux(), "lx");

    // Mass units.
    addUnit("metric_ton", metric_ton(), "t");
    addUnit("short_ton", short_ton(), QString(), NoSiPrefixes);
    addUnit("long_ton", long_ton(), QString(), NoSiPrefixes);
    addUnit("pound", pound(), "lb", NoSiPrefixes);
    addUnit("ounce", ounce(), "oz", NoSiPrefixes);
    addUnit("grain", grain(), "gr", NoSiPrefixes);
    addUnit("gram", gram(), "g");
    addUnit("atomic_mass_unit", atomic_mass_unit(), "u", NoSiPrefixes);
    addUnit("dalton", atomic_mass_unit(), "Da", NoSiPrefixes);
    addUnit("carat", carat(), "ct", NoSiPrefixes);

    // Length units.
    addUnit("micron", micron(), QString::fromUtf8("µm"), NoSiPrefixes);
    addUnit("angstrom", angstrom(), QString::fromUtf8("Å"), NoSiPrefixes);
    addUnit("astronomical_unit", astronomical_unit(), "au", NoSiPrefixes);
    addUnit("lightyear", lightyear(), "ly", NoSiPrefixes);
    addUnit("lightsecond", lightsecond(), "ls", NoSiPrefixes);
    addUnit("lightminute", lightminute(), "lmin", NoSiPrefixes);
    addUnit("parsec", parsec(), "pc", PositiveSiPrefixes);
    addUnit("inch", inch(), QString(), NoSiPrefixes);
    addUnit("foot", foot(), "ft", NoSiPrefixes);
    addUnit("yard", yard(), "yd", NoSiPrefixes);
    addUnit("mile", mile(), "mi", NoSiPrefixes);
    addUnit("rod", rod(), "rd", NoSiPrefixes);
    addUnit("furlong", furlong(), "fur", NoSiPrefixes);
    addUnit("fathom", fathom(), "fth", NoSiPrefixes);
    addUnit("nautical_mile", nautical_mile(), "nmi", NoSiPrefixes);
    addUnit("cable", cable(), "cab", NoSiPrefixes);

    // Area units.
    addUnit("are", are(), "a", NoSiPrefixes);
    addUnit("hectare", hectare(), "ha", NoSiPrefixes);
    addUnit("decare", decare(), "daa", NoSiPrefixes);
    addUnit("acre", acre(), "ac", NoSiPrefixes);

    // Volume units.
    addUnit("UK_gallon", UK_gallon(), QString(), NoSiPrefixes);
    addUnit("US_gallon", US_gallon(), "gal", NoSiPrefixes);
    addBareUnit("gallon_US", US_gallon());
    addBareUnit("gallon_UK", UK_gallon());
    addBareUnit("imperial_gallon", UK_gallon());
    addUnit("UK_quart", UK_quart(), QString(), NoSiPrefixes);
    addUnit("US_quart", US_quart(), "qt", NoSiPrefixes);
    addBareUnit("quart_US", US_quart());
    addBareUnit("quart_UK", UK_quart());
    addUnit("UK_pint", UK_pint(), QString(), NoSiPrefixes);
    addUnit("US_pint", US_pint(), "pt", NoSiPrefixes);
    addBareUnit("pint_US", US_pint());
    addBareUnit("pint_UK", UK_pint());
    addUnit("UK_fluid_ounce", UK_fluid_ounce(), QString(), NoSiPrefixes);
    addUnit("US_fluid_ounce", US_fluid_ounce(), "fl_oz", NoSiPrefixes);
    addBareUnit("fluid_ounce_US", US_fluid_ounce());
    addBareUnit("fluid_ounce_UK", UK_fluid_ounce());
    addUnitWithAliases("liter", liter(), "L", "l");

    // Time units.
    addUnit("minute", minute(), "min", NoSiPrefixes);
    addUnit("hour", hour(), "h", NoSiPrefixes);
    addUnit("day", day(), "d", NoSiPrefixes);
    addUnit("week", week(), "wk", NoSiPrefixes);
    addUnit("century", century(), "cy", NoSiPrefixes);
    addUnit("julian_year", julian_year(), QString(), NoSiPrefixes);
    addUnit("tropical_year", tropical_year(), QString(), NoSiPrefixes);
    addUnit("sidereal_year", sidereal_year(), QString(), NoSiPrefixes);
    addBareUnit("year_julian", julian_year());
    addBareUnit("year_tropical", tropical_year());
    addBareUnit("year_sidereal", sidereal_year());

    // Fractional / concentration-like units.
    addUnit("percent", percent(), QString(), NoSiPrefixes);
    addUnit("ppm", ppm(), QString(), NoSiPrefixes);
    addUnit("ppb", ppb(), QString(), NoSiPrefixes);
    addUnit("karat", karat(), "Kt", NoSiPrefixes);

    // Pressure units.
    addUnit("bar", bar(), "bar");
    addUnit("atmosphere", atmosphere(), "atm", NoSiPrefixes);
    addUnit("torr", torr(), "Torr", NoSiPrefixes);
    addUnit("pounds_per_sqinch", pounds_per_sqinch(), "psi", NoSiPrefixes);

    // Energy units.
    addUnit("electronvolt", electronvolt(), "eV");
    addUnit("electron_volt", electronvolt());
    addUnit("calorie", calorie(), "cal", NoSiPrefixes);
    addUnit("british_thermal_unit", british_thermal_unit(), "BTU", NoSiPrefixes);
    Quantity hartreeEnergy = HNumber("4.3597447222060e-18") * joule();
    hartreeEnergy.setDisplayUnit(joule().numericValue(), "joule");
    addUnit("hartree_energy_unit", hartreeEnergy, "Eh", NoSiPrefixes);

    // Information units.
    addUnit("nat", nat(), "nat", NoSiPrefixes);
    addUnit("hartley", hartley(), "Hart", NoSiPrefixes);
    addUnit("byte", byte(), "B", PositiveSiPrefixes);

    // Cooking units.
    addUnit("tablespoon", tablespoon(), "tbsp", NoSiPrefixes);
    addUnit("teaspoon", teaspoon(), "tsp", NoSiPrefixes);
    addUnit("cup", cup(), "cup", NoSiPrefixes);

    // Miscellaneous derived constants/units.
    addBareUnit("gravity", gravity());
    addBareUnit("force", gravity());
    addBareUnit("speed_of_light", speed_of_light());
    addBareUnit("speed_of_sound_STP", speed_of_sound_STP());
    addBareUnit("elementary_charge", elementary_charge());
    addUnit("knot", knot(), "kn", NoSiPrefixes);
    addUnit("horsepower", horsepower(), "hp", NoSiPrefixes);

    // Automatic SI-prefixed aliases for every prefixable unit.
    const QList<PrefixSpec> siPrefixes = {
        {"quecto", "q", -30, quecto()},
        {"ronto", "r", -27, ronto()},
        {"yocto", "y", -24, yocto()},
        {"zepto", "z", -21, zepto()},
        {"atto", "a", -18, atto()},
        {"femto", "f", -15, femto()},
        {"pico", "p", -12, pico()},
        {"nano", "n", -9, nano()},
        {"micro", QString::fromUtf8("µ"), -6, micro()},
        {"milli", "m", -3, milli()},
        {"centi", "c", -2, centi()},
        {"deci", "d", -1, deci()},
        {"deca", "da", 1, deca()},
        {"hecto", "h", 2, hecto()},
        {"kilo", "k", 3, kilo()},
        {"mega", "M", 6, mega()},
        {"giga", "G", 9, giga()},
        {"tera", "T", 12, tera()},
        {"peta", "P", 15, peta()},
        {"exa", "E", 18, exa()},
        {"zetta", "Z", 21, zetta()},
        {"yotta", "Y", 24, yotta()},
        {"ronna", "R", 27, ronna()},
        {"quetta", "Q", 30, quetta()}
    };

    for (const PrefixSpec& prefix : siPrefixes) {
        for (const UnitAliasSpec& unit : unitAliasSpecs) {
            if (prefix.power > 0 && !(unit.siPrefixPolicy & PositiveSiPrefixes))
                continue;
            if (prefix.power < 0 && !(unit.siPrefixPolicy & NegativeSiPrefixes))
                continue;
            const Quantity prefixedValue = prefix.value * unit.value;
            addUnique(prefix.longName + unit.longName, prefixedValue);
            if (!unit.shortName.isEmpty()
                && shouldAddSiPrefixedShortAlias(prefix.symbol, unit.shortName))
                addUnique(prefix.symbol + unit.shortName, prefixedValue);
            if (!unit.alternateShortName.isEmpty()
                && shouldAddSiPrefixedShortAlias(prefix.symbol, unit.alternateShortName))
                addUnique(prefix.symbol + unit.alternateShortName, prefixedValue);
        }
    }

    // Automatic binary-prefixed aliases are intentionally scoped to
    // information units only (bit/byte).
    const QList<PrefixSpec> binaryPrefixes = {
        {"kibi", "Ki", 10, kibi()},
        {"mebi", "Mi", 20, mebi()},
        {"gibi", "Gi", 30, gibi()},
        {"tebi", "Ti", 40, tebi()},
        {"pebi", "Pi", 50, pebi()},
        {"exbi", "Ei", 60, exbi()},
        {"zebi", "Zi", 70, zebi()},
        {"yobi", "Yi", 80, yobi()},
        {"robi", "Ri", 90, robi()},
        {"quebi", "Qi", 100, quebi()}
    };
    const QList<UnitAliasSpec> binaryPrefixUnits = {
        {"bit", bit(), "b", QString(), NoSiPrefixes},
        {"byte", byte(), "B", QString(), NoSiPrefixes}
    };
    for (const PrefixSpec& prefix : binaryPrefixes) {
        for (const UnitAliasSpec& unit : binaryPrefixUnits) {
            const Quantity prefixedValue = prefix.value * unit.value;
            addUnique(prefix.longName + unit.longName, prefixedValue);
            if (!unit.shortName.isEmpty())
                addUnique(prefix.symbol + unit.shortName, prefixedValue);
        }
    }

    return result;
}

QString Units::shortDisplayName(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return trimmed;

    int suffixStart = trimmed.size();
    while (suffixStart > 0) {
        const QChar ch = trimmed.at(suffixStart - 1);
        const bool isSuperscriptDigit =
            ch == QChar(0x00B2) || ch == QChar(0x00B3)
            || (ch.unicode() >= 0x2070 && ch.unicode() <= 0x2079);
        const bool isSuperscriptSign = ch == QChar(0x207B) || ch == QChar(0x207A);
        if (!isSuperscriptDigit && !isSuperscriptSign)
            break;
        --suffixStart;
    }
    const QString baseName = trimmed.left(suffixStart);
    const QString exponentSuffix = trimmed.mid(suffixStart);

    const auto& shortNames = s_unitShortNames();
    if (shortNames.contains(baseName))
        return shortNames.value(baseName) + exponentSuffix;

    // Collapse long SI-prefixed unit names (e.g. millivolt -> mV).
    const auto& prefixes = s_siPrefixSymbols();
    for (const auto& prefix : prefixes) {
        const QString& prefixLong = prefix.first;
        if (!baseName.startsWith(prefixLong))
            continue;

        const QString base = baseName.mid(prefixLong.size());
        if (shortNames.contains(base))
            return prefix.second + shortNames.value(base) + exponentSuffix;
    }

    return trimmed;
}

QString Units::formatUnitTokenForDisplay(const QString& token)
{
    QString display = shortDisplayName(token);
    if (display.size() >= 2) {
        const QChar last = display.at(display.size() - 1);
        if ((last == QLatin1Char('2') || last == QLatin1Char('3'))
            && display.at(display.size() - 2).isLetter()) {
            display.chop(1);
            display += (last == QLatin1Char('2')) ? QChar(0x00B2) : QChar(0x00B3);
        }
    }
    return display;
}

QString Units::normalizeUnitTextForDisplay(const QString& text)
{
    if (text.isEmpty())
        return text;

    QString normalized;
    normalized.reserve(text.size());

    for (int i = 0; i < text.size();) {
        const QChar ch = text.at(i);
        if (!isUnitIdentifierChar(ch)) {
            normalized.append(ch);
            ++i;
            continue;
        }

        int j = i + 1;
        while (j < text.size() && isUnitIdentifierChar(text.at(j)))
            ++j;

        const QString token = text.mid(i, j - i);
        normalized.append(formatUnitTokenForDisplay(token));
        i = j;
    }

    return normalized;
}

BASE_UNIT_CACHE(meter, "length")
BASE_UNIT_CACHE(second, "time")
BASE_UNIT_CACHE(kilogram, "mass")
BASE_UNIT_CACHE(ampere, "el. current")
BASE_UNIT_CACHE(mole, "amount")
BASE_UNIT_CACHE(kelvin, "temperature")
BASE_UNIT_CACHE(candela, "luminous intensity")
// Internal primitive for the information dimension (not an SI base unit).
BASE_UNIT_CACHE(byte, "information")

UNIT_CACHE(quecto, HNumber("1e-30"))
UNIT_CACHE(ronto, HNumber("1e-27"))
UNIT_CACHE(yocto, HNumber("1e-24"))
UNIT_CACHE(zepto, HNumber("1e-21"))
UNIT_CACHE(atto, HNumber("1e-18"))
UNIT_CACHE(femto, HNumber("1e-15"))
UNIT_CACHE(pico, HNumber("1e-12"))
UNIT_CACHE(nano, HNumber("1e-9"))
UNIT_CACHE(micro, HNumber("1e-6"))
UNIT_CACHE(milli, HNumber("1e-3"))
UNIT_CACHE(centi, HNumber("1e-2"))
UNIT_CACHE(deci, HNumber("1e-1"))

UNIT_CACHE(deca, HNumber("1e1"))
UNIT_CACHE(hecto, HNumber("1e2"))
UNIT_CACHE(kilo, HNumber("1e3"))
UNIT_CACHE(mega, HNumber("1e6"))
UNIT_CACHE(giga, HNumber("1e9"))
UNIT_CACHE(tera, HNumber("1e12"))
UNIT_CACHE(peta, HNumber("1e15"))
UNIT_CACHE(exa, HNumber("1e18"))
UNIT_CACHE(zetta, HNumber("1e21"))
UNIT_CACHE(yotta, HNumber("1e24"))
UNIT_CACHE(ronna, HNumber("1e27"))
UNIT_CACHE(quetta, HNumber("1e30"))

UNIT_CACHE(kibi, HNumber("1024"))
UNIT_CACHE(mebi, kibi()*kibi())
UNIT_CACHE(gibi, kibi()*mebi())
UNIT_CACHE(tebi, kibi()*gibi())
UNIT_CACHE(pebi, kibi()*tebi())
UNIT_CACHE(exbi, kibi()*pebi())
UNIT_CACHE(zebi, kibi()*exbi())
UNIT_CACHE(yobi, kibi()*zebi())
UNIT_CACHE(robi, kibi()*yobi())
UNIT_CACHE(quebi, kibi()*robi())

UNIT_CACHE(newton,              meter() * kilogram() / (second()*second()))
UNIT_CACHE(hertz,               Quantity(1) / second())
UNIT_CACHE(pascal,              newton() / sqmeter())
UNIT_CACHE(joule,               newton() * meter())
UNIT_CACHE(watt,                joule() / second())
UNIT_CACHE(coulomb,             ampere() * second())
UNIT_CACHE(volt,                watt() / ampere())
UNIT_CACHE(farad,               coulomb() / volt())
UNIT_CACHE(ohm,                 volt() / ampere())
UNIT_CACHE(siemens,             ampere() / volt())
UNIT_CACHE(weber,               volt() * second())
UNIT_CACHE(tesla,               weber() / sqmeter())
UNIT_CACHE(henry,               weber() / ampere())
UNIT_CACHE(becquerel,           Quantity(1) / second())
UNIT_CACHE(gray,                joule() / kilogram())
UNIT_CACHE(sievert,             joule() / kilogram())
UNIT_CACHE(katal,               mole() / second())
UNIT_CACHE(steradian,           1)
UNIT_CACHE(lumen,               candela()*steradian())
UNIT_CACHE(lux,                 lumen()/sqmeter())

UNIT_CACHE(sqmeter,             meter() * meter())
UNIT_CACHE(cbmeter,             sqmeter() * meter())

UNIT_CACHE(metric_ton,          mega()*gram())
UNIT_CACHE(gram,                milli()*kilogram())
UNIT_CACHE(pound,               HNumber("0.45359237") * kilogram())
UNIT_CACHE(ounce,               pound() / HNumber(16))
UNIT_CACHE(grain,               pound() / HNumber(7000))
UNIT_CACHE(short_ton,           HNumber(2000) * pound())
UNIT_CACHE(long_ton,            HNumber(2240) * pound())
UNIT_CACHE(atomic_mass_unit,    HNumber("1.66053906892e-27") * kilogram()) // CODATA 2022: https://physics.nist.gov/cgi-bin/cuu/Value?ukg
UNIT_CACHE(carat,               HNumber(200) * milli()*gram()) // Do not confuse with karat below.

UNIT_CACHE(micron,              micro()*meter())
UNIT_CACHE(angstrom,            HNumber("1e-10") * meter())
UNIT_CACHE(astronomical_unit,   HNumber("149597870700") * meter()) // IAU 2012 Resolution B2.
UNIT_CACHE(lightyear,           speed_of_light() * julian_year())
UNIT_CACHE(lightminute,         speed_of_light() * minute())
UNIT_CACHE(lightsecond,         speed_of_light() * second())
UNIT_CACHE(parsec,              HNumber(648000)/HMath::pi() * astronomical_unit()) // IAU 2015 Resolution B2.
UNIT_CACHE(inch,                HNumber("0.0254") * meter()) // International inch.
UNIT_CACHE(foot,                HNumber(12) * inch())
UNIT_CACHE(yard,                HNumber(36) * inch())
UNIT_CACHE(mile,                HNumber(1760) * yard())
UNIT_CACHE(rod,                 HNumber("5.5") * yard())
UNIT_CACHE(furlong,             HNumber(40) * rod())
UNIT_CACHE(fathom,              HNumber(6) * foot())
UNIT_CACHE(nautical_mile,       HNumber("1852") * meter())
UNIT_CACHE(cable,               HNumber("0.1") * nautical_mile())

UNIT_CACHE(are,                 HNumber(100) * sqmeter())
UNIT_CACHE(hectare,             HNumber(100) * are())
UNIT_CACHE(decare,              HNumber(10) * are())
UNIT_CACHE(acre,                mile()*mile() / HNumber(640))

UNIT_CACHE(US_gallon,           HNumber("3.785411784") * liter())
UNIT_CACHE(UK_gallon,           HNumber("4.54609") * liter())
UNIT_CACHE(US_quart,            US_gallon() / HNumber(4))
UNIT_CACHE(UK_quart,            UK_gallon() / HNumber(4))
UNIT_CACHE(US_pint,             US_gallon() / HNumber(8))
UNIT_CACHE(UK_pint,             UK_gallon() / HNumber(8))
UNIT_CACHE(US_fluid_ounce,      US_gallon() / HNumber(128))
UNIT_CACHE(UK_fluid_ounce,      UK_gallon() / HNumber(160))
UNIT_CACHE(liter,               milli() * cbmeter())


UNIT_CACHE(minute,              HNumber(60) * second())
UNIT_CACHE(hour,                HNumber(60) * minute())
UNIT_CACHE(day,                 HNumber(24) * hour())
UNIT_CACHE(week,                HNumber(7) * day())
UNIT_CACHE(century,             HNumber(100) * julian_year())
UNIT_CACHE(julian_year,         HNumber("365.25") * day())
UNIT_CACHE(tropical_year,       HNumber("365.242190402") * day()) // Approx.: changes over time due to Earth's precession.
UNIT_CACHE(sidereal_year,       HNumber("365.256363004") * day()) // http://hpiers.obspm.fr/eop-pc/models/constants.html

UNIT_CACHE(percent,             HNumber("0.01"))
UNIT_CACHE(ppm,                 HNumber("1e-6"))
UNIT_CACHE(ppb,                 HNumber("1e-9"))
UNIT_CACHE(karat,               Rational(1,24).toHNumber()) // Do not confuse with carat above.

UNIT_CACHE(bar,                 HNumber("1e5") * pascal())
UNIT_CACHE(atmosphere,          HNumber("1.01325") * bar())
UNIT_CACHE(torr,                atmosphere() / HNumber(760))
UNIT_CACHE(pounds_per_sqinch,   pound() * gravity() / (inch()*inch()))

UNIT_CACHE(electronvolt,        elementary_charge() * volt())
UNIT_CACHE(calorie,             HNumber("4.184") * joule()) // International Table calorie.
UNIT_CACHE(british_thermal_unit, HNumber("1055.056") * joule()) // International standard ISO 31-4.

UNIT_CACHE(nat,                 bit() / HMath::ln(2))
UNIT_CACHE(hartley,             HMath::ln(10) * nat())
UNIT_CACHE(bit,                 byte() / HNumber(8))

UNIT_CACHE(tablespoon,          HNumber(15) * milli()*liter())
UNIT_CACHE(teaspoon,            HNumber(5) * milli()*liter())
UNIT_CACHE(cup,                 HNumber(240) * milli()*liter())

UNIT_CACHE(gravity,             HNumber("9.80665") * newton() / kilogram()) // 3rd CGPM (1901, CR 70).
UNIT_CACHE(speed_of_light,      HNumber(299792458) * meter() / second())
UNIT_CACHE(elementary_charge,   HNumber("1.602176634e-19") * coulomb()) // CODATA 2022: https://physics.nist.gov/cgi-bin/cuu/Value?e
UNIT_CACHE(speed_of_sound_STP,  HNumber(331) * meter()/second())
UNIT_CACHE(knot,                nautical_mile()/hour())
UNIT_CACHE(horsepower,          HNumber(550) * foot() * pound() * gravity() / second()) // Imperial horsepower.
