// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/geometryfunctions.h"

#include "core/units.h"

#include <algorithm>
#include <array>
#include <map>
#include <numeric>
#include <utility>

namespace {

using Point = std::array<HNumber, 3>;
using Index = qsizetype;
using Face = std::array<Index, 3>;

Point difference(const Point& a, const Point& b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

Point cross(const Point& a, const Point& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

HNumber dot(const Point& a, const Point& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

HNumber norm(const Point& v)
{
    return HMath::sqrt(dot(v, v));
}

int sign(const HNumber& value)
{
    if (value.isPositive())
        return 1;
    return value.isNegative() ? -1 : 0;
}

// Project onto the two axes other than the one named by drop.
HNumber turn(const Point& a, const Point& b, const Point& c, int drop)
{
    const int x = (drop + 1) % 3;
    const int y = (drop + 2) % 3;
    return (b[x] - a[x]) * (c[y] - a[y]) - (b[y] - a[y]) * (c[x] - a[x]);
}

bool between(const HNumber& x, const HNumber& a, const HNumber& b)
{
    return x >= HMath::min(a, b) && x <= HMath::max(a, b);
}

bool onSegment(const Point& a, const Point& b, const Point& p, int drop)
{
    const int x = (drop + 1) % 3;
    const int y = (drop + 2) % 3;
    return between(p[x], a[x], b[x]) && between(p[y], a[y], b[y]) && turn(a, b, p, drop).isZero();
}

bool properCrossing(const Point& a, const Point& b, const Point& c, const Point& d, int drop)
{
    return sign(turn(a, b, c, drop)) * sign(turn(a, b, d, drop)) < 0
        && sign(turn(c, d, a, drop)) * sign(turn(c, d, b, drop)) < 0;
}

bool segmentsMeet(const Point& a, const Point& b, const Point& c, const Point& d)
{
    return properCrossing(a, b, c, d, 2)
        || onSegment(a, b, c, 2) || onSegment(a, b, d, 2)
        || onSegment(c, d, a, 2) || onSegment(c, d, b, 2);
}

bool isSimplePolygon(const QVector<Point>& points)
{
    const Index count = points.size();
    for (Index i = 0; i < count; ++i) {
        const Index next = (i + 1) % count;
        const Point& a = points[i];
        const Point& b = points[next];
        const Point& c = points[(next + 1) % count];
        if (a == b || onSegment(a, b, c, 2) || onSegment(b, c, a, 2))
            return false;

        for (Index j = i + 2; j < count; ++j) {
            const Index after = (j + 1) % count;
            if (after != i && segmentsMeet(a, b, points[j], points[after]))
                return false;
        }
    }
    return true;
}

Error readPoints(const Quantity& matrix, int dimensions, Index minRows, QVector<Point>& points)
{
    if (!matrix.isMatrix() || matrix.columns() != dimensions || matrix.rows() < minRows)
        return OutOfDomain;

    const auto rows = matrix.matrix();
    points.reserve(rows.size());
    for (const auto& row : rows) {
        Point point {0, 0, 0};
        for (int col = 0; col < dimensions; ++col) {
            const Quantity& value = row[col];
            if (value.isCollection() || !value.isReal() || value.isNan())
                return OutOfDomain;
            if (value.hasUnit() || !value.isDimensionless())
                return InvalidDimension;
            point[col] = value.numericValue().real;
        }
        points.append(point);
    }
    return Success;
}

Error readScale(const Function::ArgumentList& args, Index required, Quantity& scale)
{
    if (args.size() != required && args.size() != required + 1)
        return InvalidParamCount;
    if (args.size() == required)
        return Success;

    scale = args.last();
    if (scale.isCollection() || scale.isNan() || !scale.isReal() || !scale.isPositive())
        return OutOfDomain;
    if (!scale.sameDimension(Units::metre()) && (scale.hasUnit() || !scale.isDimensionless()))
        return InvalidDimension;
    return Success;
}

Quantity scaled(const HNumber& value, const Quantity& scale, int power)
{
    return DMath::raise(scale, power) * value;
}

template<bool Volume, bool Tetrahedron>
Quantity coordinateSolid(const Function::ArgumentList& args)
{
    Quantity scale(1);
    if (const Error error = readScale(args, 1, scale))
        return DMath::nan(error);

    const Index rows = Tetrahedron ? 4 : 3;
    QVector<Point> points;
    if (const Error error = readPoints(args[0], 3, rows, points))
        return DMath::nan(error);
    if (points.size() != rows)
        return DMath::nan(OutOfDomain);

    const Point a = Tetrahedron ? difference(points[1], points[0]) : points[0];
    const Point b = Tetrahedron ? difference(points[2], points[0]) : points[1];
    const Point c = Tetrahedron ? difference(points[3], points[0]) : points[2];

    const Point ab = cross(a, b);
    const HNumber determinant = dot(ab, c);
    if (determinant.isZero() || determinant.isNan())
        return DMath::nan(determinant.isNan() ? determinant.error() : OutOfDomain);
    if constexpr (Volume)
        return scaled(HMath::abs(determinant) / HNumber(Tetrahedron ? 6 : 1), scale, 3);

    const HNumber threeFaces = norm(ab) + norm(cross(a, c)) + norm(cross(b, c));
    if constexpr (Tetrahedron)
        return scaled((threeFaces + norm(cross(difference(a, b), difference(a, c)))) / HNumber(2),
                      scale, 2);
    return scaled(threeFaces * HNumber(2), scale, 2);
}

struct Triangle {
    Face vertices;
    Point normal;
    Point lower;
    Point upper;
    int projection;
};

Error readMesh(const Quantity& faces, const QVector<Point>& points, QVector<Triangle>& triangles)
{
    if (!faces.isMatrix() || faces.columns() != 3 || faces.rows() < 1)
        return OutOfDomain;

    const auto rows = faces.matrix();
    const HNumber pointCount(static_cast<int>(points.size()));
    triangles.reserve(rows.size());
    for (const auto& row : rows) {
        Face vertices;
        for (int i = 0; i < 3; ++i) {
            const Quantity& value = row[i];
            if (value.isCollection() || value.isNan() || !value.isInteger())
                return OutOfDomain;
            if (value.hasUnit() || !value.isDimensionless())
                return InvalidDimension;
            const HNumber vertex = value.numericValue().real;
            if (vertex < 1 || vertex > pointCount)
                return OutOfDomain;
            vertices[i] = vertex.toInt() - 1;
        }

        const Point& a = points[vertices[0]];
        const Point& b = points[vertices[1]];
        const Point& c = points[vertices[2]];
        const Point normal = cross(difference(b, a), difference(c, a));
        Point lower, upper;
        int projection = 0;
        for (int i = 0; i < 3; ++i) {
            if (normal[i].isNan())
                return normal[i].error();
            if (HMath::abs(normal[i]) > HMath::abs(normal[projection]))
                projection = i;
            lower[i] = HMath::min(a[i], HMath::min(b[i], c[i]));
            upper[i] = HMath::max(a[i], HMath::max(b[i], c[i]));
        }
        if (normal[projection].isZero())
            return OutOfDomain;
        triangles.append({vertices, normal, lower, upper, projection});
    }
    return Success;
}

class Components {
public:
    explicit Components(Index count) : m_parent(count), m_size(count, 1)
    {
        std::iota(m_parent.begin(), m_parent.end(), Index(0));
    }

    Index root(Index node)
    {
        while (node != m_parent[node]) {
            m_parent[node] = m_parent[m_parent[node]];
            node = m_parent[node];
        }
        return node;
    }

    void join(Index a, Index b)
    {
        a = root(a);
        b = root(b);
        if (a == b)
            return;
        if (m_size[a] < m_size[b])
            std::swap(a, b);
        m_parent[b] = a;
        m_size[a] += m_size[b];
    }

private:
    QVector<Index> m_parent;
    QVector<Index> m_size;
};

bool isClosedShell(const QVector<Triangle>& triangles, Index pointCount)
{
    struct EdgeUse {
        Index face;
        int corner;
        bool paired = false;
    };

    std::map<std::pair<Index, Index>, EdgeUse> edges;
    Components faces(triangles.size());
    Components corners(triangles.size() * 3);
    for (Index i = 0; i < triangles.size(); ++i) {
        const Face& face = triangles[i].vertices;
        for (int j = 0; j < 3; ++j) {
            const int next = (j + 1) % 3;
            const auto key = std::minmax(face[j], face[next]);
            auto [it, inserted] = edges.try_emplace(key, EdgeUse {i, j});
            if (inserted)
                continue;

            EdgeUse& first = it->second;
            if (first.paired || triangles[first.face].vertices[first.corner] == face[j])
                return false;
            first.paired = true;
            faces.join(i, first.face);
            corners.join(i * 3 + j, first.face * 3 + (first.corner + 1) % 3);
            corners.join(i * 3 + next, first.face * 3 + first.corner);
        }
    }
    for (const auto& entry : edges)
        if (!entry.second.paired)
            return false;

    // Every vertex must have one connected fan of faces.
    QVector<Index> vertexRoots(pointCount, -1);
    const Index shell = faces.root(0);
    for (Index i = 0; i < triangles.size(); ++i) {
        if (faces.root(i) != shell)
            return false;
        for (int j = 0; j < 3; ++j) {
            Index& vertexRoot = vertexRoots[triangles[i].vertices[j]];
            const Index cornerRoot = corners.root(i * 3 + j);
            if (vertexRoot != -1 && vertexRoot != cornerRoot)
                return false;
            vertexRoot = cornerRoot;
        }
    }
    return true;
}

bool contains(const Face& face, Index vertex)
{
    return std::find(face.begin(), face.end(), vertex) != face.end();
}

bool edgeMeetsTriangle(Index start, Index end, const Triangle& triangle, const QVector<Point>& points)
{
    const Face& face = triangle.vertices;
    const Point& origin = points[face[0]];
    const Point& a = points[start];
    const Point& b = points[end];
    const bool sharesStart = contains(face, start);
    const bool sharesEnd = contains(face, end);
    if (sharesStart && sharesEnd)
        return false;

    const HNumber da = sharesStart ? HNumber(0) : dot(triangle.normal, difference(a, origin));
    const HNumber db = sharesEnd ? HNumber(0) : dot(triangle.normal, difference(b, origin));
    const int sideA = sign(da);
    const int sideB = sign(db);
    if (sideA == sideB && sideA != 0)
        return false;

    const int drop = triangle.projection;
    const int inside = sign(triangle.normal[drop]);
    if (sideA != 0 || sideB != 0) {
        if (sharesStart || sharesEnd)
            return false; // The only contact is the shared vertex.

        for (int i = 0; i < 3; ++i) {
            const Point& p = points[face[i]];
            const Point& q = points[face[(i + 1) % 3]];
            // Test the plane intersection without dividing by its distance.
            const HNumber orientation = turn(p, q, a, drop) * HMath::abs(db)
                                      + turn(p, q, b, drop) * HMath::abs(da);
            if (sign(orientation) == -inside)
                return false;
        }
        return true;
    }

    bool aInside = !sharesStart;
    bool bInside = !sharesEnd;
    for (int i = 0; i < 3; ++i) {
        const Index vertex = face[i];
        const Point& p = points[vertex];
        const Point& q = points[face[(i + 1) % 3]];
        if (properCrossing(a, b, p, q, drop)
            || (vertex != start && vertex != end && onSegment(a, b, p, drop)))
            return true;

        aInside = aInside && sign(turn(p, q, a, drop)) != -inside;
        bInside = bInside && sign(turn(p, q, b, drop)) != -inside;
    }
    return aInside || bInside;
}

bool hasIntersectingFaces(QVector<Triangle>& triangles, const QVector<Point>& points)
{
    std::sort(triangles.begin(), triangles.end(), [](const Triangle& a, const Triangle& b) {
        return a.lower[0] < b.lower[0];
    });
    for (Index i = 0; i < triangles.size(); ++i) {
        const Triangle& a = triangles[i];
        for (Index j = i + 1; j < triangles.size(); ++j) {
            const Triangle& b = triangles[j];
            if (b.lower[0] > a.upper[0])
                break;
            if (a.upper[1] < b.lower[1] || b.upper[1] < a.lower[1]
                || a.upper[2] < b.lower[2] || b.upper[2] < a.lower[2])
                continue;

            if (std::is_permutation(a.vertices.begin(), a.vertices.end(), b.vertices.begin()))
                return true;
            for (int k = 0; k < 3; ++k) {
                const int next = (k + 1) % 3;
                if (edgeMeetsTriangle(a.vertices[k], a.vertices[next], b, points)
                    || edgeMeetsTriangle(b.vertices[k], b.vertices[next], a, points))
                    return true;
            }
        }
    }
    return false;
}

template<bool Volume>
Quantity mesh(const Function::ArgumentList& args)
{
    Quantity scale(1);
    if (const Error error = readScale(args, 2, scale))
        return DMath::nan(error);

    QVector<Point> points;
    if (const Error error = readPoints(args[0], 3, Volume ? 4 : 3, points))
        return DMath::nan(error);

    QVector<Triangle> triangles;
    if (const Error error = readMesh(args[1], points, triangles))
        return DMath::nan(error);

    HNumber sum = 0;
    if constexpr (Volume) {
        if (!isClosedShell(triangles, points.size()) || hasIntersectingFaces(triangles, points))
            return DMath::nan(OutOfDomain);
        const Point& origin = points[triangles[0].vertices[0]];
        for (const Triangle& triangle : triangles)
            sum += dot(difference(points[triangle.vertices[0]], origin), triangle.normal);
        if (sum.isZero())
            return DMath::nan(OutOfDomain);
        return scaled(HMath::abs(sum) / HNumber(6), scale, 3);
    }

    for (const Triangle& triangle : triangles)
        sum += norm(triangle.normal);
    return scaled(sum / HNumber(2), scale, 2);
}

}

namespace GeometryFunctions {

Quantity areaPolygon(Function*, const Function::ArgumentList& args)
{
    Quantity scale(1);
    if (const Error error = readScale(args, 1, scale))
        return DMath::nan(error);

    QVector<Point> points;
    if (const Error error = readPoints(args[0], 2, 3, points))
        return DMath::nan(error);
    if (points.front() == points.back())
        points.removeLast();
    if (points.size() < 3 || !isSimplePolygon(points))
        return DMath::nan(OutOfDomain);

    // Use the first vertex as the origin to avoid large terms that cancel.
    HNumber sum = 0;
    for (Index i = 1; i + 1 < points.size(); ++i)
        sum += turn(points[0], points[i], points[i + 1], 2);
    if (sum.isZero())
        return DMath::nan(OutOfDomain);
    return scaled(HMath::abs(sum) / HNumber(2), scale, 2);
}

Quantity surfaceAreaTetrahedron(Function*, const Function::ArgumentList& args)
{
    return coordinateSolid<false, true>(args);
}

Quantity volumeTetrahedron(Function*, const Function::ArgumentList& args)
{
    return coordinateSolid<true, true>(args);
}

Quantity surfaceAreaParallelepiped(Function*, const Function::ArgumentList& args)
{
    return coordinateSolid<false, false>(args);
}

Quantity volumeParallelepiped(Function*, const Function::ArgumentList& args)
{
    return coordinateSolid<true, false>(args);
}

Quantity surfaceAreaMesh(Function*, const Function::ArgumentList& args)
{
    return mesh<false>(args);
}

Quantity volumeMesh(Function*, const Function::ArgumentList& args)
{
    return mesh<true>(args);
}

}
