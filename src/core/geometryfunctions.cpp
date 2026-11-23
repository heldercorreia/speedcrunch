// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/geometryfunctions.h"

#include "core/settings.h"
#include "core/units.h"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <tuple>

namespace {

enum Input { Length, NonnegativeLength, SignedLength, Area, NonnegativeArea, Sides, Angle };

bool isPlainNumber(const Quantity& value)
{
    return value.isDimensionless() && !value.hasUnit();
}

template<int Power, auto formula, Input... inputs>
Quantity evaluate(Function*, const Function::ArgumentList& args)
{
    constexpr std::array<Input, sizeof...(inputs)> signature {inputs...};
    if (args.size() != qsizetype(signature.size()))
        return DMath::nan(InvalidParamCount);

    std::array<HNumber, signature.size()> numbers;
    bool hasPlainMeasurement = false;
    bool hasPhysicalMeasurement = false;
    for (std::size_t i = 0; i < signature.size(); ++i) {
        const Quantity& arg = args.at(i);
        if (arg.isCollection() || arg.isNan() || !arg.isReal())
            return DMath::nan(OutOfDomain);

        const Input input = signature[i];
        if (input == Angle) {
            Quantity radians = arg;
            if (!radians.isDimensionless()
                || !Units::tryConvertAngleToRadians(radians, Settings::instance()->angleUnit)
                || !isPlainNumber(radians))
                return DMath::nan(InvalidDimension);
            numbers[i] = radians.numericValue().real;
            continue;
        }

        const HNumber number = arg.numericValue().real;
        if (input == Sides) {
            if (!isPlainNumber(arg))
                return DMath::nan(InvalidDimension);
            if (!number.isInteger() || number < 3)
                return DMath::nan(OutOfDomain);
        } else {
            const Quantity dimension = input == Area || input == NonnegativeArea
                ? Units::square_metre() : Units::metre();
            if (isPlainNumber(arg))
                hasPlainMeasurement = true;
            else if (arg.sameDimension(dimension))
                hasPhysicalMeasurement = true;
            else
                return DMath::nan(InvalidDimension);

            if ((input != SignedLength && number.isNegative())
                || ((input == Length || input == Area) && number.isZero()))
                return DMath::nan(OutOfDomain);
        }
        numbers[i] = number;
    }
    if (hasPlainMeasurement && hasPhysicalMeasurement)
        return DMath::nan(InvalidDimension);

    const Quantity result(std::apply(formula, numbers));
    if (!hasPhysicalMeasurement || result.isNan())
        return result;
    return result * (Power == 2 ? Units::square_metre() : Units::cubic_metre());
}

HNumber square(const HNumber& x)
{
    return x * x;
}

HNumber cube(const HNumber& x)
{
    return x * x * x;
}

HNumber rectangle(const HNumber& length, const HNumber& width)
{
    return length * width;
}

HNumber triangle(const HNumber& base, const HNumber& height)
{
    return base * height / HNumber(2);
}

HNumber trapezoid(const HNumber& base1, const HNumber& base2, const HNumber& height)
{
    return (base1 + base2) * height / HNumber(2);
}

std::array<HNumber, 3> orderedTriangleSides(const HNumber& side1, const HNumber& side2,
                                          const HNumber& side3)
{
    std::array<HNumber, 3> sides {side1, side2, side3};
    std::sort(sides.rbegin(), sides.rend());
    return sides;
}

HNumber triangleFromSides(const HNumber& side1, const HNumber& side2, const HNumber& side3)
{
    const auto [a, b, c] = orderedTriangleSides(side1, side2, side3);
    const HNumber difference = a - b;
    if (c <= difference)
        return HMath::nan(OutOfDomain);

    // Heron's formula with the terms ordered for nearly flat triangles.
    return HMath::sqrt(a + (b + c)) * HMath::sqrt(c - difference)
         * HMath::sqrt(c + difference) * HMath::sqrt(a + (b - c)) / HNumber(4);
}

HNumber triangleFromAngle(const HNumber& a, const HNumber& b, const HNumber& angle)
{
    // Subtraction absorbs roundoff when an angle unit converts to exactly pi.
    const HNumber remaining = HMath::pi() - angle;
    if (!angle.isPositive() || !remaining.isPositive())
        return HMath::nan(OutOfDomain);
    return a * b * HMath::sin(HMath::min(angle, remaining)) / HNumber(2);
}

HNumber regularApothem(const HNumber& n, const HNumber& side)
{
    return side / (HMath::tan(HMath::pi() / n) * HNumber(2));
}

HNumber regularPolygon(const HNumber& n, const HNumber& side)
{
    return triangle(n * side, regularApothem(n, side));
}

HNumber circle(const HNumber& radius)
{
    return HMath::pi() * square(radius);
}

HNumber ellipse(const HNumber& a, const HNumber& b)
{
    return HMath::pi() * a * b;
}

HNumber annulus(const HNumber& outer, const HNumber& inner)
{
    if (inner > outer)
        return HMath::nan(OutOfDomain);
    return HMath::pi() * (outer - inner) * (outer + inner);
}

bool isSweepAngle(const HNumber& angle)
{
    return !angle.isNan() && !angle.isNegative()
        && !(HMath::pi() * HNumber(2) - angle).isNegative();
}

HNumber circularSector(const HNumber& radius, const HNumber& angle)
{
    if (!isSweepAngle(angle))
        return HMath::nan(OutOfDomain);
    return square(radius) * angle / HNumber(2);
}

HNumber annularSector(const HNumber& outer, const HNumber& inner, const HNumber& angle)
{
    if (inner > outer || !isSweepAngle(angle))
        return HMath::nan(OutOfDomain);
    return (outer - inner) * (outer + inner) * angle / HNumber(2);
}

HNumber smallSegment(const HNumber& radius, const HNumber& angle)
{
    if (angle >= HNumber("0.25"))
        return square(radius) * (angle - HMath::sin(angle)) / HNumber(2);

    // Expand angle - sin(angle) to avoid cancellation for shallow cuts.
    const HNumber angleSquared = square(angle);
    const HNumber first = square(radius * angle) * angle / HNumber(12);
    HNumber sum = first;
    HNumber term = first;
    for (int n = 4; ; n += 2) {
        term = -term * angleSquared / HNumber(n * (n + 1));
        if (term.isNan() || sum + term == sum)
            return sum;
        sum += term;
    }
}

HNumber circularSegmentFromAngle(const HNumber& radius, const HNumber& angle)
{
    if (!isSweepAngle(angle))
        return HMath::nan(OutOfDomain);
    if (angle > HMath::pi())
        return circle(radius) - smallSegment(radius, HMath::pi() * HNumber(2) - angle);
    return smallSegment(radius, angle);
}

HNumber circularSegment(const HNumber& radius, const HNumber& depth)
{
    const HNumber diameter = radius * HNumber(2);
    if (depth > diameter)
        return HMath::nan(OutOfDomain);
    if (depth > radius)
        return circle(radius) - circularSegment(radius, diameter - depth);
    if (depth == radius)
        return circle(radius) / HNumber(2);

    const HNumber halfChordRatio = HMath::sqrt(depth) / HMath::sqrt(diameter - depth);
    return smallSegment(radius, HMath::arctan(halfChordRatio) * HNumber(4));
}

HNumber circleOverlap(const HNumber& r1, const HNumber& r2, const HNumber& distance)
{
    const HNumber outer = HMath::max(r1, r2);
    const HNumber inner = HMath::min(r1, r2);
    const HNumber difference = outer - inner;
    if (distance <= difference)
        return circle(inner);

    const HNumber overlap = inner - (distance - outer);
    if (!overlap.isPositive())
        return 0;

    const HNumber outerDepth = overlap * ((distance - difference) / distance) / HNumber(2);
    const HNumber innerDepth = overlap - outerDepth;
    return circularSegment(outer, outerDepth) + circularSegment(inner, innerDepth);
}

HNumber roundedRectangle(const HNumber& length, const HNumber& width, const HNumber& radius)
{
    if (radius * HNumber(2) > HMath::min(length, width))
        return HMath::nan(OutOfDomain);
    return length * width - (HNumber(4) - HMath::pi()) * square(radius);
}

HNumber stadium(const HNumber& straightLength, const HNumber& radius)
{
    return straightLength * radius * HNumber(2) + circle(radius);
}

HNumber parabolicSegment(const HNumber& chord, const HNumber& rise)
{
    return chord * rise * HNumber(2) / HNumber(3);
}

HNumber volumeCuboid(const HNumber& length, const HNumber& width, const HNumber& height)
{
    return length * width * height;
}

HNumber surfaceCube(const HNumber& edge)
{
    return square(edge) * HNumber(6);
}

HNumber lateralCube(const HNumber& edge)
{
    return square(edge) * HNumber(4);
}

HNumber lateralCuboid(const HNumber& length, const HNumber& width, const HNumber& height)
{
    return (length + width) * height * HNumber(2);
}

HNumber surfaceCuboid(const HNumber& length, const HNumber& width, const HNumber& height)
{
    return length * width * HNumber(2) + lateralCuboid(length, width, height);
}

HNumber surfacePrism(const HNumber& baseArea, const HNumber& perimeter, const HNumber& height)
{
    return baseArea * HNumber(2) + perimeter * height;
}

HNumber lateralTriangularPrism(const HNumber& side1, const HNumber& side2, const HNumber& side3,
                              const HNumber& height)
{
    const auto [a, b, c] = orderedTriangleSides(side1, side2, side3);
    if (c <= a - b)
        return HMath::nan(OutOfDomain);
    return (a + b + c) * height;
}

HNumber surfaceTriangularPrism(const HNumber& a, const HNumber& b, const HNumber& c,
                              const HNumber& height)
{
    return surfacePrism(triangleFromSides(a, b, c), a + b + c, height);
}

HNumber volumeTriangularPrism(const HNumber& a, const HNumber& b, const HNumber& c,
                             const HNumber& height)
{
    return triangleFromSides(a, b, c) * height;
}

HNumber lateralRegularPrism(const HNumber& n, const HNumber& side, const HNumber& height)
{
    return n * side * height;
}

HNumber surfaceRegularPrism(const HNumber& n, const HNumber& side, const HNumber& height)
{
    return surfacePrism(regularPolygon(n, side), n * side, height);
}

HNumber volumeRegularPrism(const HNumber& n, const HNumber& side, const HNumber& height)
{
    return regularPolygon(n, side) * height;
}

HNumber lateralCylinder(const HNumber& radius, const HNumber& height)
{
    return HMath::pi() * radius * height * HNumber(2);
}

HNumber surfaceCylinder(const HNumber& radius, const HNumber& height)
{
    return circle(radius) * HNumber(2) + lateralCylinder(radius, height);
}

HNumber volumeCylinder(const HNumber& radius, const HNumber& height)
{
    return circle(radius) * height;
}

HNumber volumeCylindricalSegment(const HNumber& radius, const HNumber& length,
                                const HNumber& depth)
{
    return circularSegment(radius, depth) * length;
}

HNumber hypotenuse(const HNumber& a, const HNumber& b)
{
    return HMath::sqrt(square(a) + square(b));
}

HNumber lateralConeFrustum(const HNumber& r1, const HNumber& r2, const HNumber& height)
{
    if (r1.isZero() && r2.isZero())
        return HMath::nan(OutOfDomain);
    return HMath::pi() * (r1 + r2) * hypotenuse(height, r1 - r2);
}

HNumber surfaceConeFrustum(const HNumber& r1, const HNumber& r2, const HNumber& height)
{
    return lateralConeFrustum(r1, r2, height) + circle(r1) + circle(r2);
}

HNumber volumeConeFrustum(const HNumber& r1, const HNumber& r2, const HNumber& height)
{
    if (r1.isZero() && r2.isZero())
        return HMath::nan(OutOfDomain);
    return HMath::pi() * height * (square(r1) + r1 * r2 + square(r2)) / HNumber(3);
}

HNumber lateralCone(const HNumber& radius, const HNumber& height)
{
    return lateralConeFrustum(radius, 0, height);
}

HNumber surfaceCone(const HNumber& radius, const HNumber& height)
{
    return surfaceConeFrustum(radius, 0, height);
}

HNumber volumeCone(const HNumber& radius, const HNumber& height)
{
    return volumeCylinder(radius, height) / HNumber(3);
}

HNumber volumePyramid(const HNumber& baseArea, const HNumber& height)
{
    return baseArea * height / HNumber(3);
}

HNumber lateralRectangularPyramid(const HNumber& length, const HNumber& width,
                                 const HNumber& height)
{
    return length * hypotenuse(height, width / HNumber(2)) + width * hypotenuse(height, length / HNumber(2));
}

HNumber surfaceRectangularPyramid(const HNumber& length, const HNumber& width,
                                 const HNumber& height)
{
    return length * width + lateralRectangularPyramid(length, width, height);
}

HNumber volumeRectangularPyramid(const HNumber& length, const HNumber& width,
                                 const HNumber& height)
{
    return volumePyramid(length * width, height);
}

HNumber lateralSquarePyramid(const HNumber& side, const HNumber& height)
{
    return lateralRectangularPyramid(side, side, height);
}

HNumber surfaceSquarePyramid(const HNumber& side, const HNumber& height)
{
    return surfaceRectangularPyramid(side, side, height);
}

HNumber volumeSquarePyramid(const HNumber& side, const HNumber& height)
{
    return volumePyramid(square(side), height);
}

HNumber lateralRegularPyramidFrustum(const HNumber& n, const HNumber& side1,
                                     const HNumber& side2, const HNumber& height)
{
    if (side1.isZero() && side2.isZero())
        return HMath::nan(OutOfDomain);
    const HNumber apothemDifference = regularApothem(n, side1 - side2);
    return triangle(n * (side1 + side2), hypotenuse(height, apothemDifference));
}

HNumber surfaceRegularPyramidFrustum(const HNumber& n, const HNumber& side1,
                                     const HNumber& side2, const HNumber& height)
{
    return regularPolygon(n, side1) + regularPolygon(n, side2)
         + lateralRegularPyramidFrustum(n, side1, side2, height);
}

HNumber volumePyramidFrustum(const HNumber& area1, const HNumber& area2, const HNumber& height)
{
    if (area1.isZero() && area2.isZero())
        return HMath::nan(OutOfDomain);
    return (area1 + area2 + HMath::sqrt(area1) * HMath::sqrt(area2)) * height / HNumber(3);
}

HNumber volumeRegularPyramidFrustum(const HNumber& n, const HNumber& side1,
                                    const HNumber& side2, const HNumber& height)
{
    return volumePyramidFrustum(regularPolygon(n, side1), regularPolygon(n, side2), height);
}

HNumber lateralRegularPyramid(const HNumber& n, const HNumber& side, const HNumber& height)
{
    return lateralRegularPyramidFrustum(n, side, 0, height);
}

HNumber surfaceRegularPyramid(const HNumber& n, const HNumber& side, const HNumber& height)
{
    return regularPolygon(n, side) + lateralRegularPyramid(n, side, height);
}

HNumber volumeRegularPyramid(const HNumber& n, const HNumber& side, const HNumber& height)
{
    return volumePyramid(regularPolygon(n, side), height);
}

HNumber volumePrismoid(const HNumber& end1, const HNumber& middle, const HNumber& end2,
                       const HNumber& height)
{
    return (end1 + middle * HNumber(4) + end2) * height / HNumber(6);
}

HNumber surfaceSphere(const HNumber& radius)
{
    return circle(radius) * HNumber(4);
}

HNumber volumeSphere(const HNumber& radius)
{
    return circle(radius) * radius * HNumber(4) / HNumber(3);
}

HNumber lateralHemisphere(const HNumber& radius)
{
    return circle(radius) * HNumber(2);
}

HNumber surfaceHemisphere(const HNumber& radius)
{
    return circle(radius) * HNumber(3);
}

HNumber volumeHemisphere(const HNumber& radius)
{
    return volumeSphere(radius) / HNumber(2);
}

HNumber lateralSphericalCap(const HNumber& radius, const HNumber& height)
{
    if (height > radius * HNumber(2))
        return HMath::nan(OutOfDomain);
    return lateralCylinder(radius, height);
}

HNumber surfaceSphericalCap(const HNumber& radius, const HNumber& height)
{
    return lateralSphericalCap(radius, height) + HMath::pi() * height * (radius * HNumber(2) - height);
}

HNumber volumeSphericalCap(const HNumber& radius, const HNumber& height)
{
    if (height > radius * HNumber(2))
        return HMath::nan(OutOfDomain);
    return HMath::pi() * square(height) * (radius - height / HNumber(3));
}

bool validSphericalCuts(const HNumber& radius, const HNumber& lower, const HNumber& upper)
{
    return lower >= -radius && upper <= radius && lower < upper;
}

HNumber sphericalSlice(const HNumber& radius, const HNumber& z)
{
    return HMath::pi() * (radius - z) * (radius + z);
}

HNumber lateralSphericalSegment(const HNumber& radius, const HNumber& lower, const HNumber& upper)
{
    if (!validSphericalCuts(radius, lower, upper))
        return HMath::nan(OutOfDomain);
    return lateralCylinder(radius, upper - lower);
}

HNumber surfaceSphericalSegment(const HNumber& radius, const HNumber& lower, const HNumber& upper)
{
    return lateralSphericalSegment(radius, lower, upper)
         + sphericalSlice(radius, lower) + sphericalSlice(radius, upper);
}

HNumber volumeSphericalSegment(const HNumber& radius, const HNumber& lower, const HNumber& upper)
{
    if (!validSphericalCuts(radius, lower, upper))
        return HMath::nan(OutOfDomain);
    return volumePrismoid(sphericalSlice(radius, lower),
                          sphericalSlice(radius, (lower + upper) / HNumber(2)),
                          sphericalSlice(radius, upper), upper - lower);
}

HNumber surfaceCapsule(const HNumber& radius, const HNumber& straightLength)
{
    return surfaceSphere(radius) + lateralCylinder(radius, straightLength);
}

HNumber volumeCapsule(const HNumber& radius, const HNumber& straightLength)
{
    return volumeSphere(radius) + volumeCylinder(radius, straightLength);
}

HNumber lateralHollowCylinder(const HNumber& outer, const HNumber& inner, const HNumber& height)
{
    if (inner >= outer)
        return HMath::nan(OutOfDomain);
    return lateralCylinder(outer + inner, height);
}

HNumber surfaceHollowCylinder(const HNumber& outer, const HNumber& inner, const HNumber& height)
{
    return lateralHollowCylinder(outer, inner, height) + annulus(outer, inner) * HNumber(2);
}

HNumber volumeHollowCylinder(const HNumber& outer, const HNumber& inner, const HNumber& height)
{
    if (inner >= outer)
        return HMath::nan(OutOfDomain);
    return annulus(outer, inner) * height;
}

HNumber surfaceSphericalShell(const HNumber& outer, const HNumber& inner)
{
    if (inner >= outer)
        return HMath::nan(OutOfDomain);
    return surfaceSphere(outer) + surfaceSphere(inner);
}

HNumber volumeSphericalShell(const HNumber& outer, const HNumber& inner)
{
    if (inner >= outer)
        return HMath::nan(OutOfDomain);
    return HMath::pi() * (outer - inner) * (square(outer) + outer * inner + square(inner)) * HNumber(4) / HNumber(3);
}

HNumber surfaceTorus(const HNumber& major, const HNumber& minor)
{
    if (minor >= major)
        return HMath::nan(OutOfDomain);
    return square(HMath::pi()) * major * minor * HNumber(4);
}

HNumber volumeTorus(const HNumber& major, const HNumber& minor)
{
    return surfaceTorus(major, minor) * minor / HNumber(2);
}

// Carlson's R_G with 0 <= x <= z <= y = 1. Choosing the middle argument
// for z keeps all three terms of DLMF 19.21.10 nonnegative.
HNumber ellipticRg(const HNumber& x, const HNumber& z)
{
    if (z.isZero())
        return HNumber("0.5");
    if (x == 1)
        return 1;

    HNumber a = x;
    HNumber b = 1;
    HNumber c = z;
    HNumber rdSum = 0;
    HNumber quarterPower = 1;
    static const HNumber tolerance("1e-52");

    // Duplication gives R_F and R_D together. At this tolerance their
    // weighted means leave a quadratic error below the engine's precision.
    while (b - a > b * tolerance) {
        const HNumber sqrtA = HMath::sqrt(a);
        const HNumber sqrtB = HMath::sqrt(b);
        const HNumber sqrtC = HMath::sqrt(c);
        const HNumber lambda = sqrtA * (sqrtB + sqrtC) + sqrtB * sqrtC;
        rdSum += quarterPower / (sqrtC * (c + lambda));
        quarterPower /= HNumber(4);
        a = (a + lambda) / HNumber(4);
        b = (b + lambda) / HNumber(4);
        c = (c + lambda) / HNumber(4);
        if (a.isNan() || b.isNan() || c.isNan() || rdSum.isNan())
            return HMath::nan(EvalUnstable);
    }
    const HNumber rfMean = (a + b + c) / HNumber(3);
    const HNumber rdMean = (a + b + c * HNumber(3)) / HNumber(5);
    const HNumber rf = HNumber(1) / HMath::sqrt(rfMean);
    const HNumber rd = rdSum * HNumber(3) + quarterPower / (rdMean * HMath::sqrt(rdMean));
    return (z * rf + (z - x) * (HNumber(1) - z) * rd / HNumber(3) + HMath::sqrt(x / z)) / HNumber(2);
}

HNumber ellipticRatioSquared(const HNumber& small, const HNumber& large)
{
    // Below this ratio the change in R_G is smaller than working precision.
    if (small < large * HNumber("1e-60"))
        return 0;
    return square(small / large);
}

HNumber surfaceEllipsoid(const HNumber& axis1, const HNumber& axis2, const HNumber& axis3)
{
    std::array<HNumber, 3> axes {axis1, axis2, axis3};
    std::sort(axes.rbegin(), axes.rend());
    const auto& [a, b, c] = axes;
    // DLMF 19.33.1, scaled by the largest product of two axes.
    return HMath::pi() * a * b * HNumber(4)
         * ellipticRg(ellipticRatioSquared(c, a), ellipticRatioSquared(c, b));
}

HNumber volumeEllipsoid(const HNumber& a, const HNumber& b, const HNumber& c)
{
    return ellipse(a, b) * c * HNumber(4) / HNumber(3);
}

HNumber surfaceSpheroid(const HNumber& equatorial, const HNumber& polar)
{
    return surfaceEllipsoid(equatorial, equatorial, polar);
}

HNumber volumeSpheroid(const HNumber& equatorial, const HNumber& polar)
{
    return volumeEllipsoid(equatorial, equatorial, polar);
}

HNumber lateralEllipticalCylinder(const HNumber& axis1, const HNumber& axis2, const HNumber& height)
{
    const HNumber a = HMath::max(axis1, axis2);
    const HNumber b = HMath::min(axis1, axis2);
    // DLMF 19.30.5 for the perimeter of the ellipse.
    return a * height * HNumber(8) * ellipticRg(0, ellipticRatioSquared(b, a));
}

HNumber surfaceEllipticalCylinder(const HNumber& a, const HNumber& b, const HNumber& height)
{
    return ellipse(a, b) * HNumber(2) + lateralEllipticalCylinder(a, b, height);
}

HNumber volumeEllipticalCylinder(const HNumber& a, const HNumber& b, const HNumber& height)
{
    return ellipse(a, b) * height;
}

HNumber surfaceRegularTetrahedron(const HNumber& edge)
{
    return HMath::sqrt(3) * square(edge);
}

HNumber volumeRegularTetrahedron(const HNumber& edge)
{
    return HMath::sqrt(2) * cube(edge) / HNumber(12);
}

HNumber surfaceRegularOctahedron(const HNumber& edge)
{
    return surfaceRegularTetrahedron(edge) * HNumber(2);
}

HNumber volumeRegularOctahedron(const HNumber& edge)
{
    return volumeRegularTetrahedron(edge) * HNumber(4);
}

HNumber surfaceRegularDodecahedron(const HNumber& edge)
{
    return HMath::sqrt(HNumber(25) + HMath::sqrt(5) * HNumber(10)) * square(edge) * HNumber(3);
}

HNumber volumeRegularDodecahedron(const HNumber& edge)
{
    return (HNumber(15) + HMath::sqrt(5) * HNumber(7)) * cube(edge) / HNumber(4);
}

HNumber surfaceRegularIcosahedron(const HNumber& edge)
{
    return surfaceRegularTetrahedron(edge) * HNumber(5);
}

HNumber volumeRegularIcosahedron(const HNumber& edge)
{
    return (HNumber(3) + HMath::sqrt(5)) * cube(edge) * HNumber(5) / HNumber(12);
}

struct Entry {
    const char* identifier;
    const char* usage;
    const char* title;
    Function::FunctionImpl evaluate;
};

#define AREA(formula, ...) evaluate<2, formula, __VA_ARGS__>
#define VOLUME(formula, ...) evaluate<3, formula, __VA_ARGS__>

const Entry entries[] = {
    {"area_square", "side",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Square"),
     AREA(square, Length)},
    {"area_rectangle", "length; width",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Rectangle"),
     AREA(rectangle, Length, Length)},
    {"area_triangle", "base; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Triangle"),
     AREA(triangle, Length, Length)},
    {"area_triangle_from_sides", "a; b; c",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Triangle from Three Sides"),
     AREA(triangleFromSides, Length, Length, Length)},
    {"area_triangle_from_angle", "a; b; angle",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Triangle from Two Sides and an Angle"),
     AREA(triangleFromAngle, Length, Length, Angle)},
    {"area_parallelogram", "base; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Parallelogram"),
     AREA(rectangle, Length, Length)},
    {"area_trapezoid", "base_1; base_2; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Trapezoid"),
     AREA(trapezoid, Length, Length, Length)},
    {"area_rhombus", "diagonal_1; diagonal_2",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Rhombus"),
     AREA(triangle, Length, Length)},
    {"area_kite", "diagonal_1; diagonal_2",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Kite"),
     AREA(triangle, Length, Length)},
    {"area_regular_polygon", "n; side",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Regular Polygon"),
     AREA(regularPolygon, Sides, Length)},
    {"area_polygon", "vertices [; scale]",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Polygon"),
     GeometryFunctions::areaPolygon},
    {"area_circle", "radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Circle"),
     AREA(circle, Length)},
    {"area_circular_sector", "radius; angle",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Circular Sector"),
     AREA(circularSector, Length, Angle)},
    {"area_circular_segment", "radius; depth",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Circular Segment from Depth"),
     AREA(circularSegment, Length, NonnegativeLength)},
    {"area_circular_segment_from_angle", "radius; angle",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Circular Segment from an Angle"),
     AREA(circularSegmentFromAngle, Length, Angle)},
    {"area_ellipse", "semi_axis_1; semi_axis_2",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of an Ellipse"),
     AREA(ellipse, Length, Length)},
    {"area_annulus", "outer_radius; inner_radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of an Annulus"),
     AREA(annulus, Length, NonnegativeLength)},
    {"area_annular_sector", "outer_radius; inner_radius; angle",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of an Annular Sector"),
     AREA(annularSector, Length, NonnegativeLength, Angle)},
    {"area_rounded_rectangle", "length; width; corner_radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Rounded Rectangle"),
     AREA(roundedRectangle, Length, Length, NonnegativeLength)},
    {"area_stadium", "straight_length; radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Stadium"),
     AREA(stadium, NonnegativeLength, Length)},
    {"area_parabolic_segment", "chord; rise",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area of a Parabolic Segment"),
     AREA(parabolicSegment, Length, NonnegativeLength)},
    {"area_circle_overlap", "radius_1; radius_2; center_distance",
     QT_TRANSLATE_NOOP("FunctionRepo", "Area Shared by Two Circles"),
     AREA(circleOverlap, Length, Length, NonnegativeLength)},
    {"area_cube", "edge",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Cube"),
     AREA(surfaceCube, Length)},
    {"area_cube_lateral", "edge",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of a Cube"),
     AREA(lateralCube, Length)},
    {"volume_cube", "edge",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Cube"),
     VOLUME(cube, Length)},
    {"area_cuboid", "length; width; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Cuboid"),
     AREA(surfaceCuboid, Length, Length, Length)},
    {"area_cuboid_lateral", "length; width; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of a Cuboid"),
     AREA(lateralCuboid, Length, Length, Length)},
    {"volume_cuboid", "length; width; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Cuboid"),
     VOLUME(volumeCuboid, Length, Length, Length)},
    {"area_prism", "base_area; base_perimeter; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Right Prism"),
     AREA(surfacePrism, Area, Length, Length)},
    {"area_prism_lateral", "base_perimeter; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of a Right Prism"),
     AREA(rectangle, Length, Length)},
    {"volume_prism", "base_area; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Prism"),
     VOLUME(rectangle, Area, Length)},
    {"area_triangular_prism", "a; b; c; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Right Triangular Prism"),
     AREA(surfaceTriangularPrism, Length, Length, Length, Length)},
    {"area_triangular_prism_lateral", "a; b; c; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of a Right Triangular Prism"),
     AREA(lateralTriangularPrism, Length, Length, Length, Length)},
    {"volume_triangular_prism", "a; b; c; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Triangular Prism"),
     VOLUME(volumeTriangularPrism, Length, Length, Length, Length)},
    {"area_regular_prism", "n; side; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Regular Prism"),
     AREA(surfaceRegularPrism, Sides, Length, Length)},
    {"area_regular_prism_lateral", "n; side; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of a Regular Prism"),
     AREA(lateralRegularPrism, Sides, Length, Length)},
    {"volume_regular_prism", "n; side; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Regular Prism"),
     VOLUME(volumeRegularPrism, Sides, Length, Length)},
    {"volume_pyramid", "base_area; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Pyramid"),
     VOLUME(volumePyramid, Area, Length)},
    {"area_square_pyramid", "side; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Square Pyramid"),
     AREA(surfaceSquarePyramid, Length, Length)},
    {"area_square_pyramid_lateral", "side; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of a Square Pyramid"),
     AREA(lateralSquarePyramid, Length, Length)},
    {"volume_square_pyramid", "side; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Square Pyramid"),
     VOLUME(volumeSquarePyramid, Length, Length)},
    {"area_rectangular_pyramid", "length; width; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Rectangular Pyramid"),
     AREA(surfaceRectangularPyramid, Length, Length, Length)},
    {"area_rectangular_pyramid_lateral", "length; width; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of a Rectangular Pyramid"),
     AREA(lateralRectangularPyramid, Length, Length, Length)},
    {"volume_rectangular_pyramid", "length; width; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Rectangular Pyramid"),
     VOLUME(volumeRectangularPyramid, Length, Length, Length)},
    {"area_regular_pyramid", "n; side; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Regular Pyramid"),
     AREA(surfaceRegularPyramid, Sides, Length, Length)},
    {"area_regular_pyramid_lateral", "n; side; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of a Regular Pyramid"),
     AREA(lateralRegularPyramid, Sides, Length, Length)},
    {"volume_regular_pyramid", "n; side; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Regular Pyramid"),
     VOLUME(volumeRegularPyramid, Sides, Length, Length)},
    {"area_regular_pyramid_frustum", "n; side_1; side_2; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Regular Pyramid Frustum"),
     AREA(surfaceRegularPyramidFrustum, Sides, NonnegativeLength, NonnegativeLength, Length)},
    {"area_regular_pyramid_frustum_lateral", "n; side_1; side_2; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of a Regular Pyramid Frustum"),
     AREA(lateralRegularPyramidFrustum, Sides, NonnegativeLength, NonnegativeLength, Length)},
    {"volume_regular_pyramid_frustum", "n; side_1; side_2; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Regular Pyramid Frustum"),
     VOLUME(volumeRegularPyramidFrustum, Sides, NonnegativeLength, NonnegativeLength, Length)},
    {"volume_pyramid_frustum", "base_area_1; base_area_2; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Pyramid Frustum"),
     VOLUME(volumePyramidFrustum, NonnegativeArea, NonnegativeArea, Length)},
    {"volume_prismoid", "end_area_1; mid_area; end_area_2; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Prismoid"),
     VOLUME(volumePrismoid, NonnegativeArea, NonnegativeArea, NonnegativeArea, Length)},
    {"area_cylinder", "radius; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Cylinder"),
     AREA(surfaceCylinder, Length, Length)},
    {"area_cylinder_lateral", "radius; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of a Cylinder"),
     AREA(lateralCylinder, Length, Length)},
    {"volume_cylinder", "radius; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Cylinder"),
     VOLUME(volumeCylinder, Length, Length)},
    {"volume_cylindrical_segment", "radius; length; depth",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Cylindrical Segment"),
     VOLUME(volumeCylindricalSegment, Length, Length, NonnegativeLength)},
    {"area_cone", "radius; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Cone"),
     AREA(surfaceCone, Length, Length)},
    {"area_cone_lateral", "radius; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of a Cone"),
     AREA(lateralCone, Length, Length)},
    {"volume_cone", "radius; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Cone"),
     VOLUME(volumeCone, Length, Length)},
    {"area_cone_frustum", "radius_1; radius_2; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Cone Frustum"),
     AREA(surfaceConeFrustum, NonnegativeLength, NonnegativeLength, Length)},
    {"area_cone_frustum_lateral", "radius_1; radius_2; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of a Cone Frustum"),
     AREA(lateralConeFrustum, NonnegativeLength, NonnegativeLength, Length)},
    {"volume_cone_frustum", "radius_1; radius_2; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Cone Frustum"),
     VOLUME(volumeConeFrustum, NonnegativeLength, NonnegativeLength, Length)},
    {"area_hollow_cylinder", "outer_radius; inner_radius; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Hollow Cylinder"),
     AREA(surfaceHollowCylinder, Length, NonnegativeLength, Length)},
    {"area_hollow_cylinder_lateral", "outer_radius; inner_radius; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of a Hollow Cylinder"),
     AREA(lateralHollowCylinder, Length, NonnegativeLength, Length)},
    {"volume_hollow_cylinder", "outer_radius; inner_radius; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Hollow Cylinder"),
     VOLUME(volumeHollowCylinder, Length, NonnegativeLength, Length)},
    {"area_sphere", "radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Sphere"),
     AREA(surfaceSphere, Length)},
    {"volume_sphere", "radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Sphere"),
     VOLUME(volumeSphere, Length)},
    {"area_hemisphere", "radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Hemisphere"),
     AREA(surfaceHemisphere, Length)},
    {"area_hemisphere_lateral", "radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Curved Area of a Hemisphere"),
     AREA(lateralHemisphere, Length)},
    {"volume_hemisphere", "radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Hemisphere"),
     VOLUME(volumeHemisphere, Length)},
    {"area_spherical_cap", "radius; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Spherical Cap"),
     AREA(surfaceSphericalCap, Length, NonnegativeLength)},
    {"area_spherical_cap_lateral", "radius; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Curved Area of a Spherical Cap"),
     AREA(lateralSphericalCap, Length, NonnegativeLength)},
    {"volume_spherical_cap", "radius; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Spherical Cap"),
     VOLUME(volumeSphericalCap, Length, NonnegativeLength)},
    {"area_spherical_segment", "radius; lower_z; upper_z",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Spherical Segment"),
     AREA(surfaceSphericalSegment, Length, SignedLength, SignedLength)},
    {"area_spherical_segment_lateral", "radius; lower_z; upper_z",
     QT_TRANSLATE_NOOP("FunctionRepo", "Curved Area of a Spherical Segment"),
     AREA(lateralSphericalSegment, Length, SignedLength, SignedLength)},
    {"volume_spherical_segment", "radius; lower_z; upper_z",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Spherical Segment"),
     VOLUME(volumeSphericalSegment, Length, SignedLength, SignedLength)},
    {"area_spherical_shell", "outer_radius; inner_radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Spherical Shell"),
     AREA(surfaceSphericalShell, Length, NonnegativeLength)},
    {"volume_spherical_shell", "outer_radius; inner_radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Spherical Shell"),
     VOLUME(volumeSphericalShell, Length, NonnegativeLength)},
    {"area_capsule", "radius; straight_length",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Capsule"),
     AREA(surfaceCapsule, Length, NonnegativeLength)},
    {"volume_capsule", "radius; straight_length",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Capsule"),
     VOLUME(volumeCapsule, Length, NonnegativeLength)},
    {"area_torus", "major_radius; minor_radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Ring Torus"),
     AREA(surfaceTorus, Length, Length)},
    {"volume_torus", "major_radius; minor_radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Ring Torus"),
     VOLUME(volumeTorus, Length, Length)},
    {"area_ellipsoid", "semi_axis_1; semi_axis_2; semi_axis_3",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of an Ellipsoid"),
     AREA(surfaceEllipsoid, Length, Length, Length)},
    {"volume_ellipsoid", "semi_axis_1; semi_axis_2; semi_axis_3",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of an Ellipsoid"),
     VOLUME(volumeEllipsoid, Length, Length, Length)},
    {"area_spheroid", "equatorial_radius; polar_radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Spheroid"),
     AREA(surfaceSpheroid, Length, Length)},
    {"volume_spheroid", "equatorial_radius; polar_radius",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Spheroid"),
     VOLUME(volumeSpheroid, Length, Length)},
    {"area_elliptical_cylinder", "semi_axis_1; semi_axis_2; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of an Elliptical Cylinder"),
     AREA(surfaceEllipticalCylinder, Length, Length, Length)},
    {"area_elliptical_cylinder_lateral", "semi_axis_1; semi_axis_2; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Lateral Area of an Elliptical Cylinder"),
     AREA(lateralEllipticalCylinder, Length, Length, Length)},
    {"volume_elliptical_cylinder", "semi_axis_1; semi_axis_2; height",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of an Elliptical Cylinder"),
     VOLUME(volumeEllipticalCylinder, Length, Length, Length)},
    {"area_regular_tetrahedron", "edge",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Regular Tetrahedron"),
     AREA(surfaceRegularTetrahedron, Length)},
    {"volume_regular_tetrahedron", "edge",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Regular Tetrahedron"),
     VOLUME(volumeRegularTetrahedron, Length)},
    {"area_regular_octahedron", "edge",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Regular Octahedron"),
     AREA(surfaceRegularOctahedron, Length)},
    {"volume_regular_octahedron", "edge",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Regular Octahedron"),
     VOLUME(volumeRegularOctahedron, Length)},
    {"area_regular_dodecahedron", "edge",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Regular Dodecahedron"),
     AREA(surfaceRegularDodecahedron, Length)},
    {"volume_regular_dodecahedron", "edge",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Regular Dodecahedron"),
     VOLUME(volumeRegularDodecahedron, Length)},
    {"area_regular_icosahedron", "edge",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Regular Icosahedron"),
     AREA(surfaceRegularIcosahedron, Length)},
    {"volume_regular_icosahedron", "edge",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Regular Icosahedron"),
     VOLUME(volumeRegularIcosahedron, Length)},
    {"area_tetrahedron", "vertices [; scale]",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Tetrahedron"),
     GeometryFunctions::surfaceAreaTetrahedron},
    {"volume_tetrahedron", "vertices [; scale]",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Tetrahedron"),
     GeometryFunctions::volumeTetrahedron},
    {"area_parallelepiped", "edges [; scale]",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Parallelepiped"),
     GeometryFunctions::surfaceAreaParallelepiped},
    {"volume_parallelepiped", "edges [; scale]",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Parallelepiped"),
     GeometryFunctions::volumeParallelepiped},
    {"area_mesh", "vertices; faces [; scale]",
     QT_TRANSLATE_NOOP("FunctionRepo", "Surface Area of a Triangle Mesh"),
     GeometryFunctions::surfaceAreaMesh},
    {"volume_mesh", "vertices; faces [; scale]",
     QT_TRANSLATE_NOOP("FunctionRepo", "Volume of a Triangle Mesh"),
     GeometryFunctions::volumeMesh},
};

#undef VOLUME
#undef AREA

}

namespace GeometryFunctions {

void registerFunctions(FunctionRepo& repo)
{
    for (const Entry& entry : entries) {
        auto* function = new Function(QString::fromLatin1(entry.identifier), entry.evaluate,
                                      FunctionDomain::Geometry, &repo);
        function->setUsage(QString::fromLatin1(entry.usage));
        repo.insert(function);
    }
}

void retranslateFunctions(FunctionRepo& repo)
{
    const QString domain = FunctionRepo::tr("Geometry");
    for (const Entry& entry : entries) {
        Function* function = repo.find(QString::fromLatin1(entry.identifier));
        function->setName(QCoreApplication::translate("FunctionRepo", entry.title));
        function->setDomain(domain);
    }
}

}
