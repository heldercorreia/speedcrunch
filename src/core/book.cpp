// SPDX-FileCopyrightText: 2013-2014, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/startupdiagnostics.h"

#include "core/book.h"

#include <initializer_list>

// No font-family: Qt turns a family list into fallback families, and loading
// those makes Qt read every installed font family, which takes most of a second
// on systems with many fonts. The dock's default font covers all page text.
#define STYLE \
    "<style>" \
        "body {" \
            "background-color: #ffffff;" \
            "color: #000000;" \
        "}" \
        \
        ".variable {" \
            "font-weight: bold;" \
            "font-size: small;" \
        "}" \
        \
        ".unit {" \
            "font-size: large;" \
            "font-style: italic;" \
        "}" \
        \
        ".caption {" \
            "font-style: italic;" \
            "font-size: small;" \
            "padding-left: 10px;" \
        \
        "}" \
        \
        "a:link {" \
            "text-decoration: none;" \
        "}" \
        \
        ".page-link a:link {" \
            "color: #555;" \
        "}" \
        \
        ".formula a:link {" \
            "color: SteelBlue;" \
            "font-size: large;" \
        "}" \
    "</style>"

#define BEGIN QString("<html><head>" STYLE "</head><body>")+
#define SECTION(s) "<h3>"+(s)+"</h3>"
#define BR "<br />"
#define INDEX_LINK "<span class=\"page-link\"><a href=\"index\">"+Book::tr("Index")+"</a></span>"
#define LINK(link,text) "<span class=\"page-link\"><a href=\""#link"\">"+text+"</a></span>"
#define TITLE(s) "<h1>"+(s)+"</h1><div>"
#define SUBTITLE(s) "<h2>"#s+"</h2><div>"
// Clickable formula without a unit block.
#define FORMULA(link,text) "<span class=\"formula\"><a href=\"formula:"#link"\">"#text"</a></span><br />"
// Clickable formula with the same unit text used for editor insertion and display.
#define FORMULA_UNIT(link,text,unit) "<span class=\"formula\"><a href=\"formula:"#link" ["#unit"]\">"#text"</a></span><span class=\"unit\">&nbsp;&nbsp;"#unit"</span><br />"
// Clickable formula with editor unit text displayed "as" separate HTML unit markup.
#define FORMULA_UNIT_AS(link,text,unit,displayUnit) "<span class=\"formula\"><a href=\"formula:"#link" ["#unit"]\">"#text"</a></span><span class=\"unit\">&nbsp;&nbsp;"#displayUnit"</span><br />"
#define TABLE "<table><tr>"
#define ROW "</tr><tr>"
#define _TABLE "</div></tr></table>"
#define VARIABLE(s) "<td class=\"variable\">"#s"</td>"
#define CAPTION(s) "<td class=\"caption\">"+(s)+"</td>"
#define END "</body></html>"

namespace {

QString geometryChapterLinks();

const auto makeIndexPage = []() -> QString
{
    return
        BEGIN
        TITLE(Book::tr("Index"))
        SECTION(Book::tr("Algebra"))
        LINK(algebra/quadratic-equation, Book::tr("Quadratic Equation")) BR
        LINK(algebra/log-base-conversion, Book::tr("Logarithmic Base Conversion")) BR
        SECTION(Book::tr("Geometry")) +
        geometryChapterLinks() +
        SECTION(Book::tr("Electronics"))
        LINK(electronics/ohmslaw, Book::tr("Ohm's Law")) BR
        LINK(electronics/power, Book::tr("Power")) BR
        LINK(electronics/reactance, Book::tr("Reactance")) BR
        LINK(electronics/resonance, Book::tr("Resonance")) BR
        SECTION(Book::tr("Radio Frequency"))
        LINK(rf/antennas, Book::tr("Antennas")) BR
        LINK(rf/impedance, Book::tr("Characteristic Impedance (coax)")) BR
        LINK(rf/propagation, Book::tr("Velocity of Propagation (coax)")) BR
        LINK(rf/swr, Book::tr("Standing Wave Ratio & Return Loss")) BR
        LINK(rf/wavelength, Book::tr("Free Space Wavelength")) BR
        END;
};

const auto makeAlgebraQuadraticEquationPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Quadratic Equation"))
        SUBTITLE(a&middot;x<sup>2</sup> + b&middot;x + c = 0)
        FORMULA(x1 = (-b + sqrt(b%5e2 - 4*a*c))/(2*a), x<sub>1</sub> = (-b + &radic;(b<sup>2</sup> - 4&middot;a&middot;c)) / (2&middot;a))
        FORMULA(x2 = (-b - sqrt(b%5e2 - 4*a*c))/(2*a), x<sub>2</sub> = (-b - &radic;(b<sup>2</sup> - 4&middot;a&middot;c)) / (2&middot;a))
        END;
};

const auto makeAlgebraLogBaseConversionPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Logarithmic Base Conversion"))
        FORMULA(y = log(x) / log(a), log<sub>a</sub>x = log(x) / log(a))
        END;
};

const auto makeElectronicsOhmsLawPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Ohm's Law"))
        FORMULA_UNIT(R = V / I, R = V / I, &Omega;)
        FORMULA_UNIT(V = I * R, V = I&middot;R, V)
        FORMULA_UNIT(I = V / R, I = V / R, A)
        TABLE
        VARIABLE(R) CAPTION(Book::tr("resistance")) ROW
        VARIABLE(V) CAPTION(Book::tr("voltage")) ROW
        VARIABLE(I) CAPTION(Book::tr("current")) ROW
        _TABLE
        END;
};

const auto makeElectronicsPowerPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Power"))
        FORMULA_UNIT(P = I * V, P = I&middot;V, W)
        FORMULA_UNIT(P = I%5e2 * R, P = I<sup>2</sup>&middot;R, W)
        FORMULA_UNIT(P = V%5e2 / R, P = V<sup>2</sup> / R, W)
        FORMULA_UNIT(I = P / V, I = P / V, A)
        FORMULA_UNIT(I = sqrt(P / R), I = &radic;(P / R), A)
        FORMULA_UNIT(V = P / I, V = P / I, V)
        FORMULA_UNIT(V = sqrt(P * R), V = &radic;(P&middot;R), V)
        FORMULA_UNIT(R = V%5e2 / P, R = V<sup>2</sup> / P, &Omega;)
        FORMULA_UNIT(R = P / I%5e2, R = P / I<sup>2</sup>, &Omega;)
        TABLE
        VARIABLE(P) CAPTION(Book::tr("power")) ROW
        VARIABLE(I) CAPTION(Book::tr("current")) ROW
        VARIABLE(V) CAPTION(Book::tr("voltage")) ROW
        VARIABLE(R) CAPTION(Book::tr("resistance")) ROW
        _TABLE
        END;
};

const auto makeElectronicsReactancePage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Reactance"))
        FORMULA_UNIT(Xl = 2 * π * f * L, X<sub>L</sub> = 2&middot;π&middot;f&middot;L, &Omega;)
        FORMULA_UNIT(Xc = 1 / (2 * π * f * C), X<sub>C</sub> = 1 / (2&middot;π&middot;f&middot;C), &Omega;)
        FORMULA_UNIT(L = Xl / (2 * π * f), L = X<sub>L</sub> / (2&middot;π&middot;f), H)
        FORMULA_UNIT(C = 1 / (2 * π * f * Xc), C = 1 / (2&middot;π&middot;f&middot;X<sub>C</sub>), F)
        FORMULA_UNIT(f = Xl / (2 * π * L), f = X<sub>L</sub> / (2&middot;π&middot;L), Hz)
        FORMULA_UNIT(f = 1 / (Xc * 2 * π * C), f = 1 / (X<sub>C</sub>&middot;2&middot;π&middot;C), Hz)
        TABLE
        VARIABLE(X<sub>L</sub>) CAPTION(Book::tr("inductive reactance")) ROW
        VARIABLE(X<sub>C</sub>) CAPTION(Book::tr("capacitive reactance")) ROW
        VARIABLE(L) CAPTION(Book::tr("inductance")) ROW
        VARIABLE(C) CAPTION(Book::tr("capacitance")) ROW
        VARIABLE(f) CAPTION(Book::tr("frequency")) ROW
        _TABLE
        END;
};

const auto makeElectronicsResonancePage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Resonance"))
        FORMULA_UNIT(f = 1 / (2 * π * sqrt(L * C)), f = 1 / (2&middot;π&middot;&radic;(L&middot;C)), Hz)
        FORMULA_UNIT(L = 1 / (4 * π%5e2 * f%5e2 * C), L = 1 / (4&middot;π<sup>2</sup>&middot;f<sup>2</sup>&middot;C), H)
        FORMULA_UNIT(C = 1 / (4 * π%5e2 * f%5e2 * L), C = 1 / (4&middot;π<sup>2</sup>&middot;f<sup>2</sup>&middot;L), F)
        TABLE
        VARIABLE(f) CAPTION(Book::tr("resonance frequency")) ROW
        VARIABLE(L) CAPTION(Book::tr("inductance")) ROW
        VARIABLE(C) CAPTION(Book::tr("capacitance")) ROW
        _TABLE
        END;
};

QString circlePerimeters()
{
    return
        FORMULA(P = 2 * π * r, P = 2&middot;π&middot;r)
        FORMULA(P = π * d, P = π&middot;d);
}

QString cubeDiagonals()
{
    return
        FORMULA(Df = sqrt(2) * l, D<sub>f</sub> = &radic;2&middot;l)
        FORMULA(Ds = sqrt(3) * l, D<sub>s</sub> = &radic;3&middot;l);
}

QString sectorArcLength()
{
    return FORMULA(L = theta/180 * π * r, L = &pi;r&theta;&deg; / 180);
}

struct GeometryFormula {
    const char* function;
    const char* equation;
};

struct GeometrySymbol {
    const char* name;
    const char* meaning;
};

struct GeometryShape {
    const char* page;
    const char* title;
    std::initializer_list<GeometryFormula> formulas;
    std::initializer_list<GeometrySymbol> symbols;
    QString (*extraFormulas)() = nullptr;
};

struct GeometryGroup {
    const char* title;
    std::initializer_list<GeometryShape> shapes;
};

struct GeometryChapter {
    const char* page;
    const char* title;
    std::initializer_list<GeometryGroup> groups;
};

constexpr GeometrySymbol area{"A", QT_TRANSLATE_NOOP("Book", "area")};
constexpr GeometrySymbol surfaceArea{"A", QT_TRANSLATE_NOOP("Book", "surface area")};
constexpr GeometrySymbol lateralArea{"Aₗ", QT_TRANSLATE_NOOP("Book", "lateral area")};
constexpr GeometrySymbol curvedArea{"Aₗ", QT_TRANSLATE_NOOP("Book", "curved area")};
constexpr GeometrySymbol volume{"V", QT_TRANSLATE_NOOP("Book", "volume")};
constexpr GeometrySymbol length{"l", QT_TRANSLATE_NOOP("Book", "length")};
constexpr GeometrySymbol width{"w", QT_TRANSLATE_NOOP("Book", "width")};
constexpr GeometrySymbol height{"h", QT_TRANSLATE_NOOP("Book", "height")};
constexpr GeometrySymbol base{"b", QT_TRANSLATE_NOOP("Book", "base length")};
constexpr GeometrySymbol baseArea{"B", QT_TRANSLATE_NOOP("Book", "base area")};
constexpr GeometrySymbol basePerimeter{"P", QT_TRANSLATE_NOOP("Book", "base perimeter")};
constexpr GeometrySymbol sideCount{"n", QT_TRANSLATE_NOOP("Book", "number of sides")};
constexpr GeometrySymbol side{"s", QT_TRANSLATE_NOOP("Book", "side length")};
constexpr GeometrySymbol edge{"s", QT_TRANSLATE_NOOP("Book", "edge length")};
constexpr GeometrySymbol radius{"r", QT_TRANSLATE_NOOP("Book", "radius")};
constexpr GeometrySymbol diameter{"d", QT_TRANSLATE_NOOP("Book", "diameter")};
constexpr GeometrySymbol outerRadius{"R", QT_TRANSLATE_NOOP("Book", "outer radius")};
constexpr GeometrySymbol innerRadius{"r", QT_TRANSLATE_NOOP("Book", "inner radius")};
constexpr GeometrySymbol sweep{"θ", QT_TRANSLATE_NOOP("Book", "sweep angle in radians")};
constexpr GeometrySymbol semiaxes{"a, b", QT_TRANSLATE_NOOP("Book", "semiaxes")};
constexpr GeometrySymbol diagonals{"d₁, d₂", QT_TRANSLATE_NOOP("Book", "diagonal lengths")};
constexpr GeometrySymbol segmentDepth{"d", QT_TRANSLATE_NOOP("Book", "depth from the chord to the arc")};
constexpr GeometrySymbol straightLength{"L", QT_TRANSLATE_NOOP("Book", "straight length")};
constexpr GeometrySymbol coordinateScale{"k", QT_TRANSLATE_NOOP("Book", "coordinate scale (defaults to 1)")};

const std::initializer_list<GeometryShape> planeShapes = {
    {"geometry/annular-sector", QT_TRANSLATE_NOOP("Book", "Annular Sector"),
     {{"area_annular_sector", "A = (R² − r²)θ / 2"}},
     {area, outerRadius, innerRadius, sweep}},
    {"geometry/annulus", QT_TRANSLATE_NOOP("Book", "Annulus"),
     {{"area_annulus", "A = π(R² − r²)"}},
     {area, outerRadius, innerRadius}},
    {"geometry/circle", QT_TRANSLATE_NOOP("Book", "Circle"),
     {{"area_circle", "A = πr² = π(d / 2)²"}},
     {area, radius, diameter, {"P", QT_TRANSLATE_NOOP("Book", "perimeter")}},
     circlePerimeters},
    {"geometry/circle-overlap", QT_TRANSLATE_NOOP("Book", "Circle Overlap"),
     {{"area_circle_overlap", "A = S(r₁, h₁) + S(r₂, h₂)"}},
     {area, {"r₁, r₂", QT_TRANSLATE_NOOP("Book", "circle radii")},
      {"d", QT_TRANSLATE_NOOP("Book", "distance between centers")},
      {"S(r, h)", "r²(θ − sin(θ)) / 2"},
      {"θ", "4 arcsin(√(h / (2r)))"},
      {"h₁", "r₁ − (d² + r₁² − r₂²) / (2d)"},
      {"h₂", "r₁ + r₂ − d − h₁"},
      {"d ≥ r₁ + r₂", "A = 0"},
      {"d ≤ |r₁ − r₂|", "A = π min(r₁, r₂)²"}}},
    {"geometry/sector", QT_TRANSLATE_NOOP("Book", "Circular Sector"),
     {{"area_circular_sector", "A = r²θ / 2 = rL / 2"}},
     {area, radius, sweep, {"L", QT_TRANSLATE_NOOP("Book", "arc length")},
      {"θ°", QT_TRANSLATE_NOOP("Book", "central angle (degrees)")}},
     sectorArcLength},
    {"geometry/circular-segment", QT_TRANSLATE_NOOP("Book", "Circular Segment"),
     {{"area_circular_segment", "A = r² arccos(x / r) − x√(r² − x²)"},
      {"area_circular_segment_from_angle", "A = r²(θ − sin(θ)) / 2"}},
     {area, radius, segmentDepth, {"x", "r − d"}, sweep}},
    {"geometry/ellipse", QT_TRANSLATE_NOOP("Book", "Ellipse"),
     {{"area_ellipse", "A = πab"}},
     {area, semiaxes}},
    {"geometry/kite", QT_TRANSLATE_NOOP("Book", "Kite"),
     {{"area_kite", "A = d₁d₂ / 2"}},
     {area, diagonals}},
    {"geometry/parabolic-segment", QT_TRANSLATE_NOOP("Book", "Parabolic Segment"),
     {{"area_parabolic_segment", "A = 2bh / 3"}},
     {area, {"b", QT_TRANSLATE_NOOP("Book", "chord length")},
      {"h", QT_TRANSLATE_NOOP("Book", "rise from the chord to the vertex")}}},
    {"geometry/parallelogram", QT_TRANSLATE_NOOP("Book", "Parallelogram"),
     {{"area_parallelogram", "A = bh"}},
     {area, base, height}},
    {"geometry/polygon", QT_TRANSLATE_NOOP("Book", "Polygon"),
     {{"area_polygon", "A = k² |Σ (x₁y₂ − y₁x₂)| / 2"}},
     {area, {"(x₁, y₁), (x₂, y₂)", QT_TRANSLATE_NOOP("Book", "ends of each edge in order")},
      {"Σ", QT_TRANSLATE_NOOP("Book", "sum over all edges")}, coordinateScale}},
    {"geometry/rectangle", QT_TRANSLATE_NOOP("Book", "Rectangle"),
     {{"area_rectangle", "A = lw"}},
     {area, length, width}},
    {"geometry/regular-polygon", QT_TRANSLATE_NOOP("Book", "Regular Polygon"),
     {{"area_regular_polygon", "A = ns² / (4 tan(π / n))"}},
     {area, sideCount, side}},
    {"geometry/rhombus", QT_TRANSLATE_NOOP("Book", "Rhombus"),
     {{"area_rhombus", "A = d₁d₂ / 2"}},
     {area, diagonals}},
    {"geometry/right-triangle", QT_TRANSLATE_NOOP("Book", "Right Triangle"),
     {{"hypot", "c = √(a² + b²)"}},
     {{"a, b", QT_TRANSLATE_NOOP("Book", "leg lengths")},
      {"c", QT_TRANSLATE_NOOP("Book", "hypotenuse length")}}},
    {"geometry/rounded-rectangle", QT_TRANSLATE_NOOP("Book", "Rounded Rectangle"),
     {{"area_rounded_rectangle", "A = lw − (4 − π)r²"}},
     {area, length, width, {"r", QT_TRANSLATE_NOOP("Book", "corner radius")}}},
    {"geometry/square", QT_TRANSLATE_NOOP("Book", "Square"),
     {{"area_square", "A = s²"}},
     {area, side}},
    {"geometry/stadium", QT_TRANSLATE_NOOP("Book", "Stadium"),
     {{"area_stadium", "A = 2rL + πr²"}},
     {area, straightLength, radius}},
    {"geometry/trapezoid", QT_TRANSLATE_NOOP("Book", "Trapezoid"),
     {{"area_trapezoid", "A = (b₁ + b₂)h / 2"}},
     {area, {"b₁, b₂", QT_TRANSLATE_NOOP("Book", "parallel base lengths")}, height}},
    {"geometry/triangle", QT_TRANSLATE_NOOP("Book", "Triangle"),
     {{"area_triangle", "A = bh / 2"},
      {"area_triangle_from_sides", "A = √(s(s − a)(s − b)(s − c))"},
      {"area_triangle_from_angle", "A = ab sin(θ) / 2"}},
     {area, base, height, {"a, b, c", QT_TRANSLATE_NOOP("Book", "side lengths")},
      {"s", "(a + b + c) / 2"}, {"θ", QT_TRANSLATE_NOOP("Book", "angle between sides a and b")}}},
};

const std::initializer_list<GeometryShape> prismsAndPyramids = {
    {"geometry/cube", QT_TRANSLATE_NOOP("Book", "Cube"),
     {{"area_cube", "A = 6l²"},
      {"area_cube_lateral", "Aₗ = 4l²"},
      {"volume_cube", "V = l³"}},
     {surfaceArea, lateralArea, volume, {"l", QT_TRANSLATE_NOOP("Book", "edge length")},
      {"D<sub>f</sub>", QT_TRANSLATE_NOOP("Book", "face diagonal")},
      {"D<sub>s</sub>", QT_TRANSLATE_NOOP("Book", "space diagonal")}},
     cubeDiagonals},
    {"geometry/cuboid", QT_TRANSLATE_NOOP("Book", "Cuboid"),
     {{"area_cuboid", "A = 2(lw + lh + wh)"},
      {"area_cuboid_lateral", "Aₗ = 2(l + w)h"},
      {"volume_cuboid", "V = lwh"}},
     {surfaceArea, lateralArea, volume, length, width, height}},
    {"geometry/prism", QT_TRANSLATE_NOOP("Book", "Prism"),
     {{"area_prism", "A = 2B + Ph"},
      {"area_prism_lateral", "Aₗ = Ph"},
      {"volume_prism", "V = Bh"}},
     {{"A, Aₗ", QT_TRANSLATE_NOOP("Book", "surface and lateral area of a right prism")},
      volume, baseArea, basePerimeter, height}},
    {"geometry/prismoid", QT_TRANSLATE_NOOP("Book", "Prismoid"),
     {{"volume_prismoid", "V = h(B₁ + 4Bₘ + B₂) / 6"}},
     {volume, {"B₁, B₂", QT_TRANSLATE_NOOP("Book", "end areas")},
      {"Bₘ", QT_TRANSLATE_NOOP("Book", "area halfway between the ends")}, height}},
    {"geometry/pyramid", QT_TRANSLATE_NOOP("Book", "Pyramid"),
     {{"volume_pyramid", "V = Bh / 3"}},
     {volume, baseArea, height}},
    {"geometry/pyramid-frustum", QT_TRANSLATE_NOOP("Book", "Pyramid Frustum"),
     {{"volume_pyramid_frustum", "V = h(B₁ + B₂ + √(B₁B₂)) / 3"}},
     {volume, {"B₁, B₂", QT_TRANSLATE_NOOP("Book", "base areas")}, height}},
    {"geometry/rectangular-pyramid", QT_TRANSLATE_NOOP("Book", "Rectangular Pyramid"),
     {{"area_rectangular_pyramid", "A = lw + Aₗ"},
      {"area_rectangular_pyramid_lateral", "Aₗ = ls₁ + ws₂"},
      {"volume_rectangular_pyramid", "V = lwh / 3"}},
     {surfaceArea, lateralArea, volume, length, width, height,
      {"s₁", "√(h² + (w / 2)²)"}, {"s₂", "√(h² + (l / 2)²)"}}},
    {"geometry/regular-prism", QT_TRANSLATE_NOOP("Book", "Regular Prism"),
     {{"area_regular_prism", "A = 2B + Aₗ"},
      {"area_regular_prism_lateral", "Aₗ = nsh"},
      {"volume_regular_prism", "V = Bh"}},
     {surfaceArea, lateralArea, volume, sideCount, side, height,
      {"B", "ns² / (4 tan(π / n))"}}},
    {"geometry/regular-pyramid", QT_TRANSLATE_NOOP("Book", "Regular Pyramid"),
     {{"area_regular_pyramid", "A = B + Aₗ"},
      {"area_regular_pyramid_lateral", "Aₗ = ns√(h² + a²) / 2"},
      {"volume_regular_pyramid", "V = Bh / 3"}},
     {surfaceArea, lateralArea, volume, sideCount, side, height,
      {"a", "s / (2 tan(π / n))"}, {"B", "nsa / 2"}}},
    {"geometry/regular-pyramid-frustum", QT_TRANSLATE_NOOP("Book", "Regular Pyramid Frustum"),
     {{"area_regular_pyramid_frustum", "A = B₁ + B₂ + Aₗ"},
      {"area_regular_pyramid_frustum_lateral", "Aₗ = n(s₁ + s₂)t / 2"},
      {"volume_regular_pyramid_frustum", "V = h(B₁ + B₂ + √(B₁B₂)) / 3"}},
     {surfaceArea, lateralArea, volume, sideCount,
      {"s₁, s₂", QT_TRANSLATE_NOOP("Book", "base side lengths")}, height,
      {"k", "1 / (2 tan(π / n))"}, {"B₁, B₂", "nks₁² / 2, nks₂² / 2"},
      {"t", "√(h² + k²(s₁ − s₂)²)"}}},
    {"geometry/square-pyramid", QT_TRANSLATE_NOOP("Book", "Square Pyramid"),
     {{"area_square_pyramid", "A = s² + Aₗ"},
      {"area_square_pyramid_lateral", "Aₗ = 2s√(h² + (s / 2)²)"},
      {"volume_square_pyramid", "V = s²h / 3"}},
     {surfaceArea, lateralArea, volume, side, height}},
    {"geometry/triangular-prism", QT_TRANSLATE_NOOP("Book", "Triangular Prism"),
     {{"area_triangular_prism", "A = 2B + Aₗ"},
      {"area_triangular_prism_lateral", "Aₗ = (a + b + c)h"},
      {"volume_triangular_prism", "V = Bh"}},
     {surfaceArea, lateralArea, volume,
      {"a, b, c", QT_TRANSLATE_NOOP("Book", "base side lengths")}, height,
      {"s", "(a + b + c) / 2"}, {"B", "√(s(s − a)(s − b)(s − c))"}}},
};

constexpr GeometrySymbol ellipticIntegral{"Rɢ", QT_TRANSLATE_NOOP("Book", "Carlson's elliptic integral")};

const std::initializer_list<GeometryShape> roundedSolids = {
    {"geometry/capsule", QT_TRANSLATE_NOOP("Book", "Capsule"),
     {{"area_capsule", "A = 2πr(L + 2r)"},
      {"volume_capsule", "V = πr²(L + 4r / 3)"}},
     {surfaceArea, volume, radius, straightLength}},
    {"geometry/cone", QT_TRANSLATE_NOOP("Book", "Cone"),
     {{"area_cone", "A = πr(r + s)"},
      {"area_cone_lateral", "Aₗ = πrs"},
      {"volume_cone", "V = πr²h / 3"}},
     {surfaceArea, curvedArea, volume, radius, height,
      {"s", QT_TRANSLATE_NOOP("Book", "slant height")}, {"s", "√(r² + h²)"}}},
    {"geometry/cone-frustum", QT_TRANSLATE_NOOP("Book", "Cone Frustum"),
     {{"area_cone_frustum", "A = π(r₁² + r₂²) + Aₗ"},
      {"area_cone_frustum_lateral", "Aₗ = π(r₁ + r₂)s"},
      {"volume_cone_frustum", "V = πh(r₁² + r₁r₂ + r₂²) / 3"}},
     {surfaceArea, curvedArea, volume, {"r₁, r₂", QT_TRANSLATE_NOOP("Book", "base radii")},
      height, {"s", "√(h² + (r₁ − r₂)²)"}}},
    {"geometry/cylinder", QT_TRANSLATE_NOOP("Book", "Cylinder"),
     {{"area_cylinder", "A = 2πr(r + h)"},
      {"area_cylinder_lateral", "Aₗ = 2πrh"},
      {"volume_cylinder", "V = πr²h"}},
     {surfaceArea, curvedArea, volume, radius, height}},
    {"geometry/cylindrical-segment", QT_TRANSLATE_NOOP("Book", "Cylindrical Segment"),
     {{"volume_cylindrical_segment", "V = Lr²(θ − sin(θ)) / 2"}},
     {volume, radius, {"L", QT_TRANSLATE_NOOP("Book", "cylinder length")}, segmentDepth,
      {"θ", "4 arcsin(√(d / (2r)))"}}},
    {"geometry/ellipsoid", QT_TRANSLATE_NOOP("Book", "Ellipsoid"),
     {{"area_ellipsoid", "A = 4π Rɢ((bc)², (ac)², (ab)²)"},
      {"volume_ellipsoid", "V = 4πabc / 3"}},
     {surfaceArea, volume, {"a, b, c", QT_TRANSLATE_NOOP("Book", "semiaxes")}, ellipticIntegral}},
    {"geometry/elliptical-cylinder", QT_TRANSLATE_NOOP("Book", "Elliptical Cylinder"),
     {{"area_elliptical_cylinder", "A = 2πab + Aₗ"},
      {"area_elliptical_cylinder_lateral", "Aₗ = 8h Rɢ(0, a², b²)"},
      {"volume_elliptical_cylinder", "V = πabh"}},
     {surfaceArea, curvedArea, volume, semiaxes, height, ellipticIntegral}},
    {"geometry/hemisphere", QT_TRANSLATE_NOOP("Book", "Hemisphere"),
     {{"area_hemisphere", "A = 3πr²"},
      {"area_hemisphere_lateral", "Aₗ = 2πr²"},
      {"volume_hemisphere", "V = 2πr³ / 3"}},
     {surfaceArea, curvedArea, volume, radius}},
    {"geometry/hollow-cylinder", QT_TRANSLATE_NOOP("Book", "Hollow Cylinder"),
     {{"area_hollow_cylinder", "A = 2π(R² − r²) + Aₗ"},
      {"area_hollow_cylinder_lateral", "Aₗ = 2π(R + r)h"},
      {"volume_hollow_cylinder", "V = π(R² − r²)h"}},
     {surfaceArea, curvedArea, volume, outerRadius, innerRadius, height}},
    {"geometry/sphere", QT_TRANSLATE_NOOP("Book", "Sphere"),
     {{"area_sphere", "A = 4πr² = πd²"},
      {"volume_sphere", "V = 4πr³ / 3 = πd³ / 6"}},
     {surfaceArea, volume, radius, diameter}},
    {"geometry/spherical-cap", QT_TRANSLATE_NOOP("Book", "Spherical Cap"),
     {{"area_spherical_cap", "A = πh(4r − h)"},
      {"area_spherical_cap_lateral", "Aₗ = 2πrh"},
      {"volume_spherical_cap", "V = πh²(r − h / 3)"}},
     {surfaceArea, curvedArea, volume, radius,
      {"h", QT_TRANSLATE_NOOP("Book", "height from the cut to the tip")}}},
    {"geometry/spherical-segment", QT_TRANSLATE_NOOP("Book", "Spherical Segment"),
     {{"area_spherical_segment", "A = Aₗ + π(2r² − z₁² − z₂²)"},
      {"area_spherical_segment_lateral", "Aₗ = 2πr(z₂ − z₁)"},
      {"volume_spherical_segment", "V = F(z₂) − F(z₁)"}},
     {surfaceArea, curvedArea, volume, radius,
      {"z₁, z₂", QT_TRANSLATE_NOOP("Book", "lower and upper cuts from center")},
      {"F(z)", "πz(r² − z² / 3)"}}},
    {"geometry/spherical-shell", QT_TRANSLATE_NOOP("Book", "Spherical Shell"),
     {{"area_spherical_shell", "A = 4π(R² + r²)"},
      {"volume_spherical_shell", "V = 4π(R³ − r³) / 3"}},
     {surfaceArea, volume, outerRadius, innerRadius}},
    {"geometry/spheroid", QT_TRANSLATE_NOOP("Book", "Spheroid"),
     {{"area_spheroid", "A = 4π Rɢ((ac)², (ac)², a⁴)"},
      {"volume_spheroid", "V = 4πa²c / 3"}},
     {surfaceArea, volume, {"a", QT_TRANSLATE_NOOP("Book", "equatorial radius")},
      {"c", QT_TRANSLATE_NOOP("Book", "polar radius")}, ellipticIntegral}},
    {"geometry/torus", QT_TRANSLATE_NOOP("Book", "Ring Torus"),
     {{"area_torus", "A = 4π²Rr"},
      {"volume_torus", "V = 2π²Rr²"}},
     {surfaceArea, volume, {"R", QT_TRANSLATE_NOOP("Book", "major radius")},
      {"r", QT_TRANSLATE_NOOP("Book", "tube radius")}}},
};

constexpr GeometrySymbol faceVertices{"p, q, r", QT_TRANSLATE_NOOP("Book", "vertices of each face in order")};
constexpr GeometrySymbol sumOfFaces{"Σ", QT_TRANSLATE_NOOP("Book", "sum over all faces")};
constexpr GeometrySymbol cornerEdges{"a, b, c", QT_TRANSLATE_NOOP("Book", "edge vectors from one vertex")};

const std::initializer_list<GeometryShape> polyhedraAndMeshes = {
    {"geometry/parallelepiped", QT_TRANSLATE_NOOP("Book", "Parallelepiped"),
     {{"area_parallelepiped", "A = 2k²(‖a × b‖ + ‖a × c‖ + ‖b × c‖)"},
      {"volume_parallelepiped", "V = k³ |a · (b × c)|"}},
     {surfaceArea, volume, cornerEdges, coordinateScale}},
    {"geometry/regular-dodecahedron", QT_TRANSLATE_NOOP("Book", "Regular Dodecahedron"),
     {{"area_regular_dodecahedron", "A = 3√(25 + 10√5)s²"},
      {"volume_regular_dodecahedron", "V = (15 + 7√5)s³ / 4"}},
     {surfaceArea, volume, edge}},
    {"geometry/regular-icosahedron", QT_TRANSLATE_NOOP("Book", "Regular Icosahedron"),
     {{"area_regular_icosahedron", "A = 5√3 · s²"},
      {"volume_regular_icosahedron", "V = 5(3 + √5)s³ / 12"}},
     {surfaceArea, volume, edge}},
    {"geometry/regular-octahedron", QT_TRANSLATE_NOOP("Book", "Regular Octahedron"),
     {{"area_regular_octahedron", "A = 2√3 · s²"},
      {"volume_regular_octahedron", "V = √2 · s³ / 3"}},
     {surfaceArea, volume, edge}},
    {"geometry/regular-tetrahedron", QT_TRANSLATE_NOOP("Book", "Regular Tetrahedron"),
     {{"area_regular_tetrahedron", "A = √3 · s²"},
      {"volume_regular_tetrahedron", "V = √2 · s³ / 12"}},
     {surfaceArea, volume, edge}},
    {"geometry/tetrahedron", QT_TRANSLATE_NOOP("Book", "Tetrahedron"),
     {{"area_tetrahedron", "A = k² Σ ‖(q − p) × (r − p)‖ / 2"},
      {"volume_tetrahedron", "V = k³ |a · (b × c)| / 6"}},
     {surfaceArea, volume, faceVertices, sumOfFaces, cornerEdges, coordinateScale}},
    {"geometry/mesh", QT_TRANSLATE_NOOP("Book", "Triangle Mesh"),
     {{"area_mesh", "A = k² Σ ‖(q − p) × (r − p)‖ / 2"},
      {"volume_mesh", "V = k³ |Σ p · (q × r)| / 6"}},
     {surfaceArea, {"V", QT_TRANSLATE_NOOP("Book", "volume of a closed mesh")},
      faceVertices, sumOfFaces, coordinateScale}},
};

const GeometryChapter geometryChapters[] = {
    {"geometry/2d", QT_TRANSLATE_NOOP("Book", "2D Shapes"), {{nullptr, planeShapes}}},
    {"geometry/3d", QT_TRANSLATE_NOOP("Book", "3D Shapes"),
     {{QT_TRANSLATE_NOOP("Book", "Prisms and Pyramids"), prismsAndPyramids},
      {QT_TRANSLATE_NOOP("Book", "Rounded Solids"), roundedSolids},
      {QT_TRANSLATE_NOOP("Book", "Polyhedra and Meshes"), polyhedraAndMeshes}}},
};

QString pageLink(const char* path, const char* title)
{
    return QStringLiteral("<span class=\"page-link\"><a href=\"%1\">%2</a></span>")
        .arg(QString::fromLatin1(path), Book::tr(title).toHtmlEscaped());
}

QString geometryChapterLinks()
{
    QString links;
    for (const GeometryChapter& chapter : geometryChapters)
        links += pageLink(chapter.page, chapter.title) + BR;
    return links;
}

QString makeGeometryChapterPage(const GeometryChapter& chapter)
{
    QString html = BEGIN INDEX_LINK TITLE(Book::tr(chapter.title).toHtmlEscaped());
    for (const GeometryGroup& group : chapter.groups) {
        if (group.title)
            html += SECTION(Book::tr(group.title).toHtmlEscaped());
        for (const GeometryShape& shape : group.shapes)
            html += pageLink(shape.page, shape.title) + BR;
    }
    return html + "</div>" END;
}

QString makeGeometryShapePage(const GeometryShape& shape, const GeometryChapter& chapter)
{
    const QString title = Book::tr(shape.title).toHtmlEscaped();
    QString html = BEGIN INDEX_LINK + " / " + pageLink(chapter.page, chapter.title) + TITLE(title);
    html += QStringLiteral("<div><img src=\"book-diagram:%1\" alt=\"%2\" /></div>")
        .arg(QString::fromLatin1(shape.page), title);
    for (const GeometryFormula& formula : shape.formulas) {
        html += QStringLiteral("<span class=\"formula\"><a href=\"function:%1\" title=\"%1()\">%2</a></span><br />")
            .arg(QString::fromLatin1(formula.function), QString::fromUtf8(formula.equation).toHtmlEscaped());
    }
    if (shape.extraFormulas)
        html += SECTION(Book::tr("Other Formulas")) + shape.extraFormulas();
    html += "<table>";
    for (const GeometrySymbol& symbol : shape.symbols) {
        html += QStringLiteral("<tr><td class=\"variable\">%1</td><td class=\"caption\">%2</td></tr>")
            .arg(QString::fromUtf8(symbol.name), Book::tr(symbol.meaning).toHtmlEscaped());
    }
    return html + "</table></div>" END;
}

const auto makeRFAntennasPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Radio Frequency"))
        FORMULA_UNIT_AS(Pd = Pin / (4 * π * r%5e2), Pd = P<sub>in</sub> / (4&middot;π&middot;r<sup>2</sup>), W/m%5e2, W&middot;m<sup>-2</sup>)
        FORMULA_UNIT_AS(E = sqrt(Pd * 120 * π), E = &radic;(P<sub>d</sub>&middot;120&middot;π), V/m, V&middot;m<sup>-1</sup>)
        FORMULA_UNIT_AS(E = sqrt((Pin * 120 * π) / (4 * π * r%5e2)), E = &radic;[(P<sub>in</sub>&middot;120&middot;π) / (4&middot;π&middot;r<sup>2</sup>)], V/m, V&middot;m<sup>-1</sup>)
        FORMULA_UNIT_AS(E = sqrt(30 * Pin) / r, E = &radic;(30&middot;P<sub>in</sub>) / r, V/m, V&middot;m<sup>-1</sup>)
        FORMULA_UNIT_AS(Ed = E * sqrt(D), Ed = E&middot;&radic;D, V/m, V&middot;m<sup>-1</sup>)
        FORMULA_UNIT_AS(Edp = E * sqrt(1.64), Edp = E&middot;&radic;1.64, V/m, V&middot;m<sup>-1</sup>)
        END;
};

const auto makeRFImpedancePage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Characteristic Impedance (coax)"))
        FORMULA_UNIT(Zo = 59.959 / sqrt(er) * ln(d1/d2), Z<sub>0</sub> = 59.959 / &radic;e<sub>r</sub> &middot; ln(d<sub>1</sub>/d<sub>2</sub>), &Omega;)
        TABLE
        VARIABLE(e<sub>r) CAPTION(Book::tr("dielectric constant")) ROW
        VARIABLE(d<sub>1) CAPTION(Book::tr("outer conductor's inner diameter")) ROW
        VARIABLE(d<sub>2</sub>) CAPTION(Book::tr("inner conductor's outer diameter"))
        _TABLE
        END;
};

const auto makeRFPropagationPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Velocity of Propagation (coax)"))
        SUBTITLE(V<sub>p</sub> = c / &radic;e<sub>r</sub>)
        FORMULA_UNIT_AS(Vp = 3e8 / sqrt(er), V<sub>p</sub> = 3e8 / &radic;e<sub>r</sub>, m/s, m&middot;s<sup>-1</sup>)
        FORMULA_UNIT_AS(Vp = 299792458 / sqrt(er), V<sub>p</sub> = 299792458 / &radic;e<sub>r</sub>, m/s, m&middot;s<sup>-1</sup>)
        FORMULA_UNIT_AS(Vp = 983571056.43045 / sqrt(er), V<sub>p</sub> = 983571056.43045 / &radic;e<sub>r</sub>, ft/s, ft&middot;s<sup>-1</sup>)
        TABLE
        VARIABLE(c) CAPTION(Book::tr("speed of light")) ROW
        VARIABLE(e) CAPTION(Book::tr("dielectric constant")) ROW
        _TABLE
        END;
};

const auto makeRFSWRPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Standing Wave Ratio & Return Loss"))
        FORMULA(SWR = (1+r) / (1-r), SWR = (1 + &rho;) / (1 - &rho;))
        FORMULA(r = (SWR-1) / (SWR+1), &rho; = (SWR - 1) / (SWR + 1))
        FORMULA(r = (Z-1) / (Z+1), &rho; = (Z - 1) / (Z + 1))
        FORMULA_UNIT(Z = (1+r) / (1-r), Z = (1 + &rho;) / (1 - &rho;), &Omega;)
        FORMULA(RL = -20 * log(r), R<sub>L</sub> = -20&middot;log(&rho;))
        FORMULA(RL = -20 * log((SWR-1)/(SWR+1)), R<sub>L</sub> = -20&middot;log[(SWR - 1) / (SWR + 1)])
        FORMULA_UNIT(Pr = Pin * r%5e2, P<sub>r</sub> = P<sub>in</sub>&middot;&rho;<sup>2</sup>, W)
        FORMULA_UNIT(Pt = Pin * (1-r%5e2), P<sub>t</sub> = P<sub>in</sub>&middot;(1-&rho;<sup>2</sup>), W)
        TABLE
        VARIABLE(P<sub>in</sub>) CAPTION(Book::tr("input power") + " (W)") ROW
        VARIABLE(P<sub>r</sub>) CAPTION(Book::tr("reflected power") + " (W)") ROW
        VARIABLE(P<sub>t</sub>) CAPTION(Book::tr("transmitted power") + " (W)") ROW
        VARIABLE(R<sub>L</sub>) CAPTION(Book::tr("return loss") + " (dB)") ROW
        VARIABLE(&rho;) CAPTION(Book::tr("reflection coefficient")) ROW
        VARIABLE(SWR) CAPTION(Book::tr("standing wave ratio")) ROW
        VARIABLE(Z) CAPTION(Book::tr("normalized impedance")) ROW
        _TABLE
        END;
};

const auto makeRFWavelengthPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Free Space Wavelength"))
        SUBTITLE(&lambda;<sub>0</sub> = c / f)
        FORMULA_UNIT(L = 3e8 / f, L = 3&times;10<sup>8</sup> / f, m)
        FORMULA_UNIT(L = 299792458 / f, L = 299792458 / f, m)
        FORMULA_UNIT(L = 983571056.43045 / f, L = 983571056.43045 / f, ft)
        TABLE
        VARIABLE(f) CAPTION(Book::tr("frequency (Hz)")) ROW
        _TABLE
        END;
};

} // namespace

void Book::createPages()
{
    StartupDiagnostics::Scope timing("Register formula book pages");
    addPage("index", +makeIndexPage);
    addPage("algebra/quadratic-equation", +makeAlgebraQuadraticEquationPage);
    addPage("algebra/log-base-conversion", +makeAlgebraLogBaseConversionPage);
    addPage("electronics/ohmslaw", +makeElectronicsOhmsLawPage);
    addPage("electronics/power", +makeElectronicsPowerPage);
    addPage("electronics/reactance", +makeElectronicsReactancePage);
    addPage("electronics/resonance", +makeElectronicsResonancePage);
    for (const GeometryChapter& chapter : geometryChapters) {
        addPage(QString::fromLatin1(chapter.page), [&chapter] { return makeGeometryChapterPage(chapter); });
        for (const GeometryGroup& group : chapter.groups) {
            for (const GeometryShape& shape : group.shapes) {
                addPage(QString::fromLatin1(shape.page), [&shape, &chapter] {
                    return makeGeometryShapePage(shape, chapter);
                });
            }
        }
    }
    addPage("rf/antennas", +makeRFAntennasPage);
    addPage("rf/impedance", +makeRFImpedancePage);
    addPage("rf/propagation", +makeRFPropagationPage);
    addPage("rf/swr", +makeRFSWRPage);
    addPage("rf/wavelength", +makeRFWavelengthPage);
}
