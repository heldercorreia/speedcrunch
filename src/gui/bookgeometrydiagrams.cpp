// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/bookgeometrydiagrams.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <iterator>

namespace {

constexpr QPointF drawingCenter(60, 45);

class Diagram {
public:
    Diagram(QPainter& painter, const QColor& ink, const QColor& background)
        : m_painter(painter), m_ink(ink), m_background(background), m_labelFont(painter.font())
    {
        m_labelFont.setPixelSize(13);
        m_labelFont.setWeight(QFont::DemiBold);
        m_labelFont.setItalic(true);
    }

    void fill(const QPainterPath& path, qreal opacity = 0.16)
    {
        QColor color = m_ink;
        color.setAlphaF(opacity);
        m_painter.fillPath(path, color);
    }

    void outline(const QPainterPath& path, qreal opacity = 1, qreal width = 1.55)
    {
        QColor color = m_ink;
        color.setAlphaF(opacity);
        const QPen pen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        m_painter.strokePath(path, pen);
    }

    void guide(const QPainterPath& path)
    {
        QColor color = m_ink;
        color.setAlphaF(0.58);
        const QPen pen(color, 1.25, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
        m_painter.strokePath(path, pen);
    }

    void guideOutside(const QPainterPath& path, const QPainterPath& solid)
    {
        QPainterPath canvas;
        const QSizeF size = BookGeometryDiagrams::canvasSize;
        canvas.addRect(QRectF(drawingCenter - QPointF(size.width(), size.height()) / 2, size));
        m_painter.save();
        m_painter.setClipPath(canvas.subtracted(solid), Qt::IntersectClip);
        guide(path);
        m_painter.restore();
    }

    void solid(const QPainterPath& path, qreal opacity = 0.16)
    {
        fill(path, opacity);
        outline(path);
    }

    void mark(const QPainterPath& path, qreal opacity = 0.85)
    {
        QColor color = m_ink;
        color.setAlphaF(opacity);
        m_painter.strokePath(path, QPen(color, 0.95, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    }

    void label(const QPointF& center, QStringView symbol)
    {
        QPainterPath text;
        text.addText(QPointF(), m_labelFont, symbol.toString());
        text.setFillRule(Qt::WindingFill);
        text.translate(center - text.boundingRect().center());
        m_painter.strokePath(text, QPen(m_background, 2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        m_painter.fillPath(text, m_ink);
    }

    void dot(const QPointF& point)
    {
        QPainterPath dot;
        dot.addEllipse(point, 1.4, 1.4);
        m_painter.fillPath(dot, m_ink);
    }

    void dimension(QPointF from, QPointF to, QStringView symbol,
                   QPointF offset = {}, QPointF labelOffset = {})
    {
        measure(from, to, symbol, offset, labelOffset, true);
    }

    void vectorDimension(QPointF from, QPointF to, QStringView symbol, QPointF offset)
    {
        measure(from, to, symbol, offset, {}, false);
    }

    void axis(const QPointF& from, const QPointF& to)
    {
        arrows(from, to, false);
    }

    void arrowArc(const QPainterPath& path)
    {
        mark(path);
        const auto tangent = [&path](qreal position) {
            const qreal angle = qDegreesToRadians(path.angleAtPercent(position));
            return QPointF(qCos(angle), -qSin(angle));
        };
        QPainterPath heads;
        arrowHead(heads, path.pointAtPercent(0), -3.7 * tangent(0));
        arrowHead(heads, path.pointAtPercent(1), 3.7 * tangent(1));
        mark(heads);
    }

    void arrow(const QPointF& from, const QPointF& to, QStringView symbol, QPointF labelOffset = {})
    {
        arrows(from, to, false);
        dot(from);
        label((from + to) / 2 + labelOffset, symbol);
    }

    void leader(const QPointF& tip, const QPointF& text, QStringView symbol)
    {
        arrows(text, tip, false);
        label(text, symbol);
    }

private:
    void measure(QPointF from, QPointF to, QStringView symbol, QPointF offset,
                 QPointF labelOffset, bool bothEnds)
    {
        if (!offset.isNull()) {
            QPainterPath extensions(from);
            extensions.lineTo(from + offset);
            extensions.moveTo(to);
            extensions.lineTo(to + offset);
            mark(extensions, 0.50);
        }
        from += offset;
        to += offset;
        arrows(from, to, bothEnds);
        label((from + to) / 2 + labelOffset, symbol);
    }

    static void arrowHead(QPainterPath& path, const QPointF& tip, const QPointF& head)
    {
        const QPointF wing(-head.y() * 0.46, head.x() * 0.46);
        path.moveTo(tip - head + wing);
        path.lineTo(tip);
        path.lineTo(tip - head - wing);
    }

    void arrows(const QPointF& from, const QPointF& to, bool bothEnds)
    {
        const QPointF delta = to - from;
        const qreal length = qHypot(delta.x(), delta.y());
        if (length == 0)
            return;

        const QPointF head = delta * (qMin(3.7, length / 3.5) / length);
        QPainterPath path(from);
        path.lineTo(to);
        arrowHead(path, to, head);
        if (bothEnds)
            arrowHead(path, from, -head);
        mark(path);
    }

    QPainter& m_painter;
    QColor m_ink;
    QColor m_background;
    QFont m_labelFont;
};

QPainterPath polygon(const QList<QPointF>& points)
{
    QPainterPath path;
    path.addPolygon(QPolygonF(points));
    path.closeSubpath();
    return path;
}

QPainterPath ellipse(const QRectF& bounds)
{
    QPainterPath path;
    path.addEllipse(bounds);
    return path;
}

QPainterPath arc(const QRectF& bounds, qreal start, qreal sweep)
{
    QPainterPath path;
    path.arcMoveTo(bounds, start);
    path.arcTo(bounds, start, sweep);
    return path;
}

QPainterPath line(const QPointF& from, const QPointF& to)
{
    QPainterPath path(from);
    path.lineTo(to);
    return path;
}

QList<QPointF> regularPolygon(int sides, qreal radius, QPointF center = {}, qreal phase = -90)
{
    QList<QPointF> vertices;
    for (int i = 0; i < sides; ++i) {
        const qreal angle = qDegreesToRadians(360.0 * i / sides + phase);
        vertices.append(center + radius * QPointF(qCos(angle), qSin(angle)));
    }
    return vertices;
}

QPointF ellipsePoint(const QRectF& ellipse, qreal degrees)
{
    const qreal angle = qDegreesToRadians(degrees);
    return ellipse.center() + QPointF(ellipse.width() / 2 * qCos(angle), -ellipse.height() / 2 * qSin(angle));
}

void angleMark(Diagram& diagram, const QPointF& center, qreal radius, qreal start, qreal sweep)
{
    const QRectF guide(center - QPointF(radius, radius), QSizeF(2 * radius, 2 * radius));
    diagram.mark(arc(guide, start, sweep));
    const QRectF labelGuide = guide.adjusted(-7, -7, 7, 7);
    diagram.label(ellipsePoint(labelGuide, start + sweep / 2), u"θ");
}

void drawSector(Diagram& diagram, bool annular)
{
    const QRectF outer(22, 10, 76, 76);
    const QRectF inner(38, 26, 44, 44);
    const QPointF center = outer.center();
    QPainterPath area = arc(outer, 13, 128);
    if (annular)
        area.arcTo(inner, 141, -128);
    else
        area.lineTo(center);
    area.closeSubpath();

    diagram.guide(ellipse(outer));
    diagram.solid(area, 0.29);
    if (annular) {
        diagram.guide(line(center, ellipsePoint(inner, 13)));
        diagram.guide(line(center, ellipsePoint(inner, 141)));
        diagram.arrow(center, ellipsePoint(inner, 141), u"r", {-5, 8});
        diagram.arrow(center, ellipsePoint(outer, 29), u"R", {8, -4});
    } else {
        diagram.arrow(center, ellipsePoint(outer, 32), u"r", {8, -5});
        const QRectF lengthGuide = outer.adjusted(-9, -9, 9, 9);
        for (qreal end : {13.0, 141.0})
            diagram.mark(line(ellipsePoint(outer, end), ellipsePoint(lengthGuide, end)), 0.50);
        diagram.arrowArc(arc(lengthGuide, 13, 128));
        diagram.label(ellipsePoint(lengthGuide, 77), u"L");
    }
    angleMark(diagram, center, annular ? 8 : 10, 13, 128);
}

void drawAnnulus(Diagram& diagram)
{
    const QRectF outer(25, 10, 70, 70);
    const QRectF inner(43, 28, 34, 34);
    diagram.solid(ellipse(outer).subtracted(ellipse(inner)));
    diagram.arrow(outer.center(), ellipsePoint(outer, 27), u"R", {4, -6});
    diagram.arrow(inner.center(), ellipsePoint(inner, 181), u"r", {0, -8});
}

void markRadiusAndDiameter(Diagram& diagram, const QRectF& circle)
{
    diagram.arrow(circle.center(), ellipsePoint(circle, 36), u"r", {8, 4});
    const qreal y = circle.center().y();
    diagram.dimension({circle.left(), y}, {circle.right(), y}, u"d", {0, circle.height() / 2 + 9});
}

void drawCircularSegment(Diagram& diagram)
{
    const QRectF circle(25, 10, 70, 70);
    constexpr qreal depth = 15;
    const qreal radius = circle.width() / 2;
    const qreal start = qRadiansToDegrees(qAsin((radius - depth) / radius));
    const qreal sweep = 180 - 2 * start;
    const QPointF center = circle.center();
    QPainterPath area = arc(circle, start, sweep);
    area.closeSubpath();
    diagram.guide(ellipse(circle));
    diagram.solid(area, 0.29);
    diagram.guide(line(center, ellipsePoint(circle, start)));
    diagram.guide(line(center, ellipsePoint(circle, 180 - start)));
    angleMark(diagram, center, 7, start, sweep);
    const qreal chordY = circle.top() + depth;
    diagram.dimension({center.x(), circle.top()}, {center.x(), chordY}, u"d", {50, 0}, {7, 0});
    diagram.arrow(center, ellipsePoint(circle, 325), u"r", {4, 7});
}

void drawCircleOverlap(Diagram& diagram)
{
    const QRectF left(12, 8, 68, 68);
    const QRectF right(52, 16, 52, 52);
    const qreal r1 = left.width() / 2;
    const qreal r2 = right.width() / 2;
    const qreal distance = right.center().x() - left.center().x();
    const qreal chordX = (r1 * r1 - r2 * r2 + distance * distance) / (2 * distance);
    const qreal leftAngle = qRadiansToDegrees(qAcos(chordX / r1));
    const qreal rightAngle = qRadiansToDegrees(qAcos((distance - chordX) / r2));
    QPainterPath overlap = arc(left, -leftAngle, 2 * leftAngle);
    overlap.arcTo(right, 180 - rightAngle, 2 * rightAngle);
    overlap.closeSubpath();
    diagram.outline(ellipse(left), 0.58);
    diagram.outline(ellipse(right), 0.58);
    diagram.solid(overlap, 0.29);
    diagram.arrow(left.center(), ellipsePoint(left, 153), u"r₁", {-5, -7});
    diagram.arrow(right.center(), ellipsePoint(right, 32), u"r₂", {5, -8});
    diagram.dimension(left.center(), right.center(), u"d", {0, 41});
}

void drawRoundedRectangle(Diagram& diagram, qreal radius)
{
    const QRectF bounds(14, 22, 92, 46);
    QPainterPath path;
    path.addRoundedRect(bounds, radius, radius);
    diagram.solid(path);
    const qreal bottom = bounds.bottom();
    const qreal right = bounds.right();
    diagram.dimension({bounds.left(), bottom - radius}, {right, bottom - radius},
                      u"l", {0, radius + 13});
    diagram.dimension({right - radius, bounds.top()}, {right - radius, bottom},
                      u"w", {radius + 13, 0});
    if (radius > 0) {
        const QPointF corner(bounds.left() + radius, bounds.top() + radius);
        diagram.arrow(corner, corner - QPointF(radius, radius) / qSqrt(2.0), u"r", {5, -8});
    }
}

void drawStadium(Diagram& diagram)
{
    const QRectF bounds(14, 22, 92, 46);
    const qreal radius = bounds.height() / 2;
    QPainterPath path;
    path.addRoundedRect(bounds, radius, radius);
    diagram.solid(path);

    const qreal leftCenter = bounds.left() + radius;
    const qreal rightCenter = bounds.right() - radius;
    for (qreal x : {leftCenter, rightCenter})
        diagram.guide(line({x, bounds.top()}, {x, bounds.bottom()}));

    diagram.dimension({leftCenter, bounds.bottom()}, {rightCenter, bounds.bottom()}, u"L", {0, 14});
    diagram.arrow({rightCenter, bounds.center().y()}, {bounds.right(), bounds.center().y()}, u"r", {0, -8});
}

void drawDiagonals(Diagram& diagram, qreal shoulder, qreal halfWidth)
{
    diagram.solid(polygon({{60, 9}, {60 + halfWidth, shoulder}, {60, 81}, {60 - halfWidth, shoulder}}));
    diagram.dimension({60, 9}, {60, 81}, u"d₁", {}, {-10, 12});
    diagram.dimension({60 - halfWidth, shoulder}, {60 + halfWidth, shoulder}, u"d₂", {}, {13, -8});
}

void drawTriangle(Diagram& diagram)
{
    const QPointF left(16, 73), right(104, 73), apex(46, 14);
    diagram.solid(polygon({apex, right, left}));
    diagram.dimension(left, right, u"b", {0, 13});
    diagram.dimension(left, apex, u"a", {-9, -4});
    diagram.dimension(apex, right, u"c", {8, -8});
    diagram.dimension({apex.x(), left.y()}, apex, u"h", {}, {8, 4});
    angleMark(diagram, left, 14, 0, qRadiansToDegrees(qAtan2(left.y() - apex.y(), apex.x() - left.x())));
}

using Vertices = QList<QVector3D>;
using Faces = QList<QList<int>>;

struct View {
    View(qreal azimuth, qreal elevation)
        : sine(qSin(qDegreesToRadians(elevation))), cosine(qCos(qDegreesToRadians(elevation))),
          right(float(qCos(qDegreesToRadians(azimuth))), float(-qSin(qDegreesToRadians(azimuth))), 0),
          forward(-right.y(), right.x(), 0),
          down(forward * float(sine) - QVector3D(0, 0, float(cosine))),
          eye(QVector3D::crossProduct(right, down))
    {}

    QPointF project(const QVector3D& point) const
    {
        return {QVector3D::dotProduct(right, point), QVector3D::dotProduct(down, point)};
    }

    qreal sine;
    qreal cosine;
    QVector3D right;
    QVector3D forward;
    QVector3D down;
    QVector3D eye;
};

const View standardView(37, 30);
const View baseView(38, 35);

struct Projection {
    const View& view;
    qreal scale;
    QPointF origin;

    QPointF operator()(const QVector3D& point) const
    {
        return origin + view.project(point) * scale;
    }

    QPointF operator()(const QPointF& point, float z = 0) const
    {
        return operator()(QVector3D(float(point.x()), float(point.y()), z));
    }
};

Projection drawPolyhedron(Diagram& diagram, const Vertices& vertices, const Faces& faces,
                          const QList<int>& baseGuide = {}, const View& view = standardView,
                          int featuredFace = -1)
{
    const QVector3D light(-0.4f, 0.2f, 0.89f);

    QVector3D center;
    QPolygonF projected;
    for (const QVector3D& vertex : vertices) {
        center += vertex;
        projected.append(view.project(vertex));
    }
    center /= float(vertices.size());

    const QRectF bounds = projected.boundingRect();
    const qreal scale = qMin(100 / bounds.width(), 74 / bounds.height());
    const Projection projection{view, scale, drawingCenter - bounds.center() * scale};
    for (QPointF& point : projected)
        point = projection.origin + point * scale;

    const auto facePath = [&projected](const QList<int>& face) {
        QList<QPointF> points;
        for (int index : face)
            points.append(projected[index]);
        return polygon(points);
    };
    if (!baseGuide.isEmpty())
        diagram.guide(facePath(baseGuide));

    QPainterPath edges;
    for (int i = 0; i < faces.size(); ++i) {
        const QList<int>& face = faces[i];
        const QVector3D& first = vertices[face[0]];
        QVector3D normal = QVector3D::normal(vertices[face[1]] - first, vertices[face[2]] - first);
        if (QVector3D::dotProduct(normal, first - center) < 0)
            normal = -normal;
        if (QVector3D::dotProduct(normal, view.eye) <= 0.0001f)
            continue;

        const QPainterPath surface = facePath(face);
        const qreal shade = 0.20 - 0.12 * QVector3D::dotProduct(normal, light);
        diagram.fill(surface, i == featuredFace ? 0.29 : shade);
        edges.addPath(surface);
    }
    diagram.outline(edges);
    return projection;
}

QList<QPointF> rectangleBase(qreal width, qreal depth)
{
    return {{-width / 2, -depth / 2}, {width / 2, -depth / 2},
            {width / 2, depth / 2}, {-width / 2, depth / 2}};
}

QList<QPointF> scaledBase(QList<QPointF> base, qreal scale, QPointF shift = {})
{
    for (QPointF& point : base)
        point = scale * point + shift;
    return base;
}

Projection drawLayers(Diagram& diagram, const QList<QPointF>& base, const QList<QPointF>& top, float height,
                      bool showBase = false, const View& view = standardView)
{
    const int count = int(base.size());
    const bool apex = top.size() == 1;
    Vertices vertices;
    Faces faces;
    QList<int> baseFace;
    QList<int> topFace;
    for (const QPointF& point : base)
        vertices.append(QVector3D(point));
    for (const QPointF& point : top)
        vertices.append(QVector3D(float(point.x()), float(point.y()), height));

    for (int i = 0; i < count; ++i) {
        baseFace.append(i);
        const int next = (i + 1) % count;
        if (apex) {
            faces.append({i, next, count});
        } else {
            topFace.append(i + count);
            faces.append({i, next, next + count, i + count});
        }
    }
    faces.append(baseFace);
    if (!apex)
        faces.append(topFace);
    return drawPolyhedron(diagram, vertices, faces, apex || showBase ? baseFace : QList<int>(), view);
}

Projection drawPrism(Diagram& diagram, const QList<QPointF>& base, float height)
{
    return drawLayers(diagram, base, base, height);
}

void markVertical(Diagram& diagram, const QPointF& from, const QPointF& to, QStringView symbol, qreal x = 115)
{
    const QPointF first(x, from.y());
    const QPointF second(x, to.y());
    diagram.mark(line(from, first), 0.50);
    diagram.mark(line(to, second), 0.50);
    diagram.dimension(first, second, symbol, {}, {8, 0});
}

void markEdge(Diagram& diagram, const QPointF& first, const QPointF& second, QStringView symbol,
              QPointF inside = {60, 45})
{
    const QPointF delta = second - first;
    const QPointF midpoint = (first + second) / 2;
    QPointF normal(-delta.y(), delta.x());
    if (QPointF::dotProduct(normal, inside - midpoint) > 0)
        normal = -normal;
    diagram.dimension(first, second, symbol, normal * (9 / qHypot(delta.x(), delta.y())));
}

void drawBox(Diagram& diagram, qreal length, qreal depth, float height, bool cube)
{
    const auto base = rectangleBase(length, depth);
    const auto p = drawPrism(diagram, base, height);
    markEdge(diagram, p(base[3]), p(base[2]), u"l");
    if (!cube) {
        markEdge(diagram, p(base[1]), p(base[2]), u"w");
        markVertical(diagram, p(base[1]), p(base[1], height), u"h");
    }
}

void drawGeneralPrism(Diagram& diagram)
{
    const QList<QPointF> base{{-26, -21}, {26, -21}, {26, 9}, {0, 33}, {-26, 9}};
    constexpr float height = 40.5;
    const auto p = drawLayers(diagram, base, base, height, false, baseView);

    QList<QPointF> perimeter;
    for (const QPointF& corner : base)
        perimeter.append(p(corner, height));
    diagram.outline(polygon(perimeter), 1, 2.3);
    diagram.label(p(QPointF(), height) + QPointF(1, 2), u"B");
    const QPointF rim = (perimeter[4] + perimeter[0]) / 2;
    diagram.leader(rim, rim + QPointF(-17, -10), u"P");
    markVertical(diagram, p(base[1]), p(base[1], height), u"h");
}

void drawPrismoid(Diagram& diagram)
{
    const auto base = rectangleBase(60, 46);
    const auto top = rectangleBase(49, 23);
    constexpr float height = 45;
    const auto p = drawLayers(diagram, base, top, height);
    const float midHeight = height / 2;
    const QPointF left = p((base[3] + top[3]) / 2, midHeight);
    const QPointF front = p((base[2] + top[2]) / 2, midHeight);
    const QPointF right = p((base[1] + top[1]) / 2, midHeight);
    QPainterPath section(left);
    section.lineTo(front);
    section.lineTo(right);
    diagram.guide(section);

    markVertical(diagram, p(top[1]), p(top[1], height), u"h");
    diagram.label(p(QPointF(), height), u"B₂");
    diagram.leader(left, {3, left.y() - 5}, u"Bₘ");
    diagram.label(p(QPointF()) + QPointF(0, 3), u"B₁");
}

void drawGeneralPyramid(Diagram& diagram)
{
    const QList<QPointF> base{{-24, -17}, {22, -13}, {27, 17}, {-20, 12}};
    const QPointF apex(12, -8);
    constexpr float height = 46;
    const auto p = drawLayers(diagram, base, {apex}, height);
    diagram.label(p(QPointF()), u"B");
    markVertical(diagram, p(apex), p(apex, height), u"h");
}

void drawPyramidFrustum(Diagram& diagram)
{
    const auto base = rectangleBase(62, 42);
    const QPointF shift(4, -4);
    const auto top = scaledBase(base, 0.5, shift);
    constexpr float height = 35;
    const auto p = drawLayers(diagram, base, top, height, true);
    diagram.label(p(QPointF()), u"B₁");
    diagram.label(p(shift, height), u"B₂");
    markVertical(diagram, p(top[1]), p(top[1], height), u"h");
}

void drawRectangularPyramid(Diagram& diagram, qreal length, qreal depth, bool square)
{
    const auto base = rectangleBase(length, depth);
    constexpr float height = 42;
    const auto p = drawLayers(diagram, base, {{0, 0}}, height, false, square ? standardView : baseView);
    const QPointF apex = p(QPointF(), height);
    markEdge(diagram, p(base[3]), p(base[2]), square ? u"s" : u"l");
    markVertical(diagram, p(QPointF()), apex, u"h");
    if (!square) {
        markEdge(diagram, p(base[1]), p(base[2]), u"w");
        const QPointF lengthMidpoint = p((base[2] + base[3]) / 2);
        const QPointF widthMidpoint = p((base[1] + base[2]) / 2);
        diagram.dimension(apex, lengthMidpoint, u"s₁", {}, {-15, -3});
        diagram.dimension(apex, widthMidpoint, u"s₂", {}, {7, 9});
        diagram.dot(lengthMidpoint);
        diagram.dot(widthMidpoint);
    }
}

void drawRegularPrism(Diagram& diagram)
{
    const auto base = regularPolygon(6, 26);
    constexpr float height = 39;
    const auto p = drawPrism(diagram, base, height);
    markEdge(diagram, p(base[3]), p(base[4]), u"s");
    diagram.label(p(QPointF(), height), u"n = 6");
    markVertical(diagram, p(base[1]), p(base[1], height), u"h");
}

void drawRegularPyramid(Diagram& diagram)
{
    const auto base = regularPolygon(5, 34, {}, -76);
    constexpr float height = 36;
    const auto p = drawLayers(diagram, base, {{0, 0}}, height, true, baseView);
    const QPointF center = p(QPointF());
    const QPointF apex = p(QPointF(), height);
    const QPointF edgeMidpoint = (base[2] + base[3]) / 2;
    const QPointF midpoint = p(edgeMidpoint);
    diagram.mark(line(apex, midpoint));
    diagram.arrow(center, midpoint, u"a", {-6, 6});
    markEdge(diagram, p(base[2]), p(base[3]), u"s");
    diagram.label({13, 12}, u"n = 5");
    markVertical(diagram, center, apex, u"h");
}

void drawRegularPyramidFrustum(Diagram& diagram)
{
    const auto base = regularPolygon(6, 31);
    const auto top = scaledBase(base, 0.62);
    constexpr float height = 31;
    const auto p = drawLayers(diagram, base, top, height, true, baseView);
    markEdge(diagram, p(base[2]), p(base[3]), u"s₁");
    markEdge(diagram, p(top[5], height), p(top[0], height), u"s₂", p(QPointF(), height));
    const QPointF lowerMidpoint = p((base[3] + base[4]) / 2);
    const QPointF upperMidpoint = p((top[3] + top[4]) / 2, height);
    diagram.dimension(lowerMidpoint, upperMidpoint, u"t", {-18, -3});
    diagram.label(p(QPointF(), height) + QPointF(-2, -1), u"n = 6");
    markVertical(diagram, p(top[1]), p(top[1], height), u"h");
}

void drawTriangularPrism(Diagram& diagram)
{
    constexpr float height = 62;
    const Vertices base{{-25, height / 2, -16}, {22, height / 2, -16}, {-8, height / 2, 22}};
    Vertices vertices = base;
    for (const QVector3D& vertex : base)
        vertices.append(vertex - QVector3D(0, height, 0));
    const Faces faces{{0, 1, 2}, {3, 5, 4}, {0, 3, 4, 1}, {1, 4, 5, 2}, {2, 5, 3, 0}};
    const auto p = drawPolyhedron(diagram, vertices, faces, {3, 4, 5});
    const QPointF center = p((base[0] + base[1] + base[2]) / 3);
    markEdge(diagram, p(base[0]), p(base[1]), u"a", center);
    markEdge(diagram, p(base[1]), p(base[2]), u"b", center);
    markEdge(diagram, p(base[2]), p(base[0]), u"c", center);
    markEdge(diagram, p(vertices[2]), p(vertices[5]), u"h");
}

constexpr QRectF unitCircle(-1, -1, 2, 2);

QTransform circleTransform(const QPointF& center, const QPointF& cosineAxis, const QPointF& sineAxis)
{
    return {cosineAxis.x(), cosineAxis.y(), -sineAxis.x(), -sineAxis.y(), center.x(), center.y()};
}

QTransform verticalCircle(const Projection& p, const QVector3D& center, float radius)
{
    const QPointF origin = p(center);
    return circleTransform(origin, p(center + QVector3D(radius, 0, 0)) - origin,
                           p(center + QVector3D(0, 0, radius)) - origin);
}

QRectF horizontalEllipse(const QPointF& center, qreal acrossRadius, qreal forwardRadius,
                         const View& view = standardView)
{
    const qreal depth = forwardRadius * view.sine;
    return QRectF(center - QPointF(acrossRadius, depth), QSizeF(2 * acrossRadius, 2 * depth));
}

QRectF horizontalCircle(const QPointF& center, qreal radius, const View& view = standardView)
{
    return horizontalEllipse(center, radius, radius, view);
}

void drawRoundWall(Diagram& diagram, const QRectF& bottom, const QRectF& top, qreal tangent)
{
    QPainterPath wall = top.isEmpty() ? QPainterPath(top.center()) : arc(top, 180 - tangent, 180 + 2 * tangent);
    wall.arcTo(bottom, tangent, -180 - 2 * tangent);
    wall.closeSubpath();
    diagram.solid(wall, 0.22);
}

qreal frustumTangent(qreal lowerRadius, qreal upperRadius, qreal height, const View& view)
{
    // At the silhouette the side normal is perpendicular to the line of sight.
    const qreal slope = (lowerRadius - upperRadius) / height;
    return qRadiansToDegrees(qAsin(slope * view.sine / view.cosine));
}

QPointF rightOfRim(const QRectF& rim)
{
    return {rim.right(), rim.center().y()};
}

void drawCone(Diagram& diagram)
{
    constexpr float radius = 38;
    constexpr float height = 62;
    const Projection p{standardView, 1, {60, 70}};
    const QPointF apex = p(QVector3D(0, 0, height));
    const QRectF base = horizontalCircle(p.origin, radius);
    const qreal tangent = frustumTangent(radius, 0, height, standardView);
    drawRoundWall(diagram, base, QRectF(apex, QSizeF(0, 0)), tangent);
    diagram.guide(arc(base, tangent, 180 - 2 * tangent));
    markEdge(diagram, apex, ellipsePoint(base, 180 - tangent), u"s");
    markVertical(diagram, base.center(), apex, u"h");
    diagram.arrow(base.center(), rightOfRim(base), u"r", {0, -9});
}

void drawConeFrustum(Diagram& diagram)
{
    constexpr float lowerRadius = 43;
    constexpr float upperRadius = 22;
    constexpr float height = 52;
    const Projection p{standardView, 1, {60, 65}};
    const QRectF bottom = horizontalCircle(p.origin, lowerRadius);
    const QRectF top = horizontalCircle(p(QVector3D(0, 0, height)), upperRadius);
    const qreal tangent = frustumTangent(lowerRadius, upperRadius, height, standardView);
    drawRoundWall(diagram, bottom, top, tangent);
    diagram.solid(ellipse(top), 0.10);
    markEdge(diagram, ellipsePoint(top, 180 - tangent), ellipsePoint(bottom, 180 - tangent), u"s");
    markVertical(diagram, rightOfRim(bottom), rightOfRim(top), u"h");
    diagram.arrow(top.center(), rightOfRim(top), u"r₁", {0, -8});
    diagram.arrow(bottom.center(), rightOfRim(bottom), u"r₂", {0, -8});
}

void drawCircularCylinder(Diagram& diagram)
{
    constexpr float radius = 34;
    constexpr float height = 47;
    const Projection p{standardView, 1, {60, 67}};
    const QRectF bottom = horizontalCircle(p.origin, radius);
    const QRectF top = horizontalCircle(p(QVector3D(0, 0, height)), radius);
    drawRoundWall(diagram, bottom, top, 0);
    diagram.solid(ellipse(top), 0.10);
    markVertical(diagram, rightOfRim(bottom), rightOfRim(top), u"h");
    diagram.arrow(top.center(), rightOfRim(top), u"r", {0, -8});
}

void drawEllipticalCylinder(Diagram& diagram)
{
    constexpr float a = 46;
    constexpr float b = 21;
    constexpr float height = 51;
    const Projection p{standardView, 1, {59, 67}};
    const QRectF bottom = horizontalEllipse(p.origin, a, b);
    const QRectF top = horizontalEllipse(p(QVector3D(0, 0, height)), a, b);
    drawRoundWall(diagram, bottom, top, 0);
    diagram.solid(ellipse(top), 0.10);
    markVertical(diagram, rightOfRim(bottom), rightOfRim(top), u"h");
    diagram.arrow(top.center(), rightOfRim(top), u"a", {0, -8});
    diagram.arrow(top.center(), {top.center().x(), top.bottom()}, u"b", {-9, 0});
}

void drawHollowCylinder(Diagram& diagram)
{
    constexpr float outerRadius = 37;
    constexpr float innerRadius = 21;
    constexpr float height = 44;
    const Projection p{standardView, 1, {60, 65}};
    const QRectF bottom = horizontalCircle(p.origin, outerRadius);
    const QPointF topCenter = p(QVector3D(0, 0, height));
    const QRectF top = horizontalCircle(topCenter, outerRadius);
    const QRectF hole = horizontalCircle(topCenter, innerRadius);
    drawRoundWall(diagram, bottom, top, 0);

    diagram.fill(ellipse(hole), 0.26);
    QPainterPath lid = ellipse(top);
    lid.addEllipse(hole);
    diagram.solid(lid, 0.10);
    markVertical(diagram, rightOfRim(bottom), rightOfRim(top), u"h");
    diagram.arrow(topCenter, ellipsePoint(top, 24), u"R", {5, -7});
    diagram.arrow(topCenter, {hole.left(), hole.center().y()}, u"r", {-3, -7});
}

qreal verticalCircleFacingAngle(const View& view)
{
    return qRadiansToDegrees(qAtan2(view.eye.z(), view.eye.x()));
}

void drawCylindricalSegment(Diagram& diagram)
{
    constexpr float radius = 27;
    constexpr float length = 75;
    constexpr float depth = 22;
    constexpr float cutZ = depth - radius;
    const Projection p{standardView, 1, {59, 43}};
    const QVector3D front(0, length / 2, 0);
    const QVector3D back(0, -length / 2, 0);
    const QTransform nearCircle = verticalCircle(p, front, radius);
    const QTransform farCircle = verticalCircle(p, back, radius);
    const qreal left = 180 - qRadiansToDegrees(qAsin(cutZ / radius));
    const qreal right = 360 + qRadiansToDegrees(qAsin(cutZ / radius));
    const qreal tangent = 270 + verticalCircleFacingAngle(standardView);

    QPainterPath end = nearCircle.map(arc(unitCircle, left, right - left));
    const QPointF nearLeft = end.pointAtPercent(0);
    const QPointF nearRight = end.currentPosition();
    const QPointF axis = p(back) - p(front);
    end.closeSubpath();

    QPainterPath wall(nearRight);
    wall.connectPath(farCircle.map(arc(unitCircle, right, tangent - right)));
    wall.connectPath(nearCircle.map(arc(unitCircle, tangent, right - tangent)));
    wall.closeSubpath();

    const QPainterPath plane = polygon({nearLeft, nearLeft + axis, nearRight + axis, nearRight});
    const QPainterPath solid = wall.united(plane).united(end);
    const QPointF frontTop = p(front + QVector3D(0, 0, radius));
    QPainterPath uncut = nearCircle.map(ellipse(unitCircle));
    uncut.addPath(farCircle.map(ellipse(unitCircle)));
    uncut.addPath(line(frontTop, frontTop + axis));
    diagram.guideOutside(uncut, solid);

    diagram.solid(wall, 0.22);
    diagram.solid(plane, 0.12);
    diagram.solid(end, 0.29);
    diagram.dimension(frontTop, frontTop + axis, u"L", {0, -9});
    diagram.arrow(p(front), nearCircle.map(ellipsePoint(unitCircle, 141)), u"r", {-6, -6});
    diagram.dimension(p(front + QVector3D(0, 0, cutZ)), p(front - QVector3D(0, 0, radius)),
                      u"d", {-28, 0}, {-8, 0});
}

void drawCapsule(Diagram& diagram)
{
    constexpr float radius = 19;
    constexpr float length = 67;
    const Projection p{standardView, 1, {60, 43}};
    const QVector3D front(0, length / 2, 0);
    const QVector3D back(0, -length / 2, 0);
    const QPointF axis = p(back) - p(front);
    const qreal projectedLength = qHypot(axis.x(), axis.y());
    const QPointF normal = QPointF(-axis.y(), axis.x()) / projectedLength;

    // A projected capsule is a line segment with a disk swept along it.
    QTransform align;
    align.translate(p(front).x(), p(front).y());
    align.rotate(qRadiansToDegrees(qAtan2(axis.y(), axis.x())));
    QPainterPath body;
    body.addRoundedRect(QRectF(-radius, -radius, projectedLength + 2 * radius, 2 * radius), radius, radius);
    diagram.solid(align.map(body));

    const qreal frontAngle = verticalCircleFacingAngle(standardView);
    for (const QVector3D& end : {front, back}) {
        const QTransform seam = verticalCircle(p, end, radius);
        diagram.guide(seam.map(arc(unitCircle, frontAngle + 90, 180)));
        diagram.outline(seam.map(arc(unitCircle, frontAngle - 90, 180)), 0.70);
    }
    diagram.dimension(p(front) + normal * radius, p(back) + normal * radius, u"L", normal * 10);
    diagram.arrow(p(back), p(back) - normal * radius, u"r", {8, -1});
}

QRectF sphereBounds(const QPointF& center, qreal radius)
{
    return QRectF(center - QPointF(radius, radius), QSizeF(2 * radius, 2 * radius));
}

void drawGlobe(Diagram& diagram, const QRectF& bounds, bool shell = false)
{
    const QPointF center = bounds.center();
    const qreal radius = bounds.width() / 2;
    diagram.solid(ellipse(bounds), shell ? 0.07 : 0.16);
    const QRectF equator = horizontalCircle(center, radius);
    diagram.guide(arc(equator, 0, 180));
    diagram.outline(arc(equator, 0, -180), 0.80);
    if (shell) {
        const QRectF inner = sphereBounds(center, radius - 11);
        diagram.guide(ellipse(inner));
        diagram.arrow(center, ellipsePoint(bounds, 33), u"R", {8, 0});
        diagram.arrow(center, ellipsePoint(inner, 144), u"r", {-6, -7});
    } else {
        const float r = float(radius);
        const QTransform meridian = circleTransform(center, standardView.project({0, r, 0}),
                                                     standardView.project({0, 0, r}));
        const qreal angle = qRadiansToDegrees(qAtan2(standardView.eye.z(), standardView.eye.y()));
        diagram.outline(meridian.map(arc(unitCircle, angle - 90, 180)), 0.70);
    }
}

void drawEllipsoid(Diagram& diagram, float axis1, float axis2, float axis3, bool spheroid = false)
{
    const QPointF a = standardView.project(QVector3D(axis1, 0, 0));
    const QPointF b = standardView.project(QVector3D(0, axis2, 0));
    const QPointF c = standardView.project(QVector3D(0, 0, axis3));

    // Factor the projected semiaxes to find the ellipse of the outer surface.
    const qreal x = qHypot(a.x(), b.x());
    const qreal crossAxis = (a.x() * a.y() + b.x() * b.y()) / x;
    const qreal y = qSqrt(a.y() * a.y() + b.y() * b.y() + c.y() * c.y() - crossAxis * crossAxis);
    const QTransform silhouette(x, crossAxis, 0, y, drawingCenter.x(), drawingCenter.y());
    const QTransform equator = circleTransform(drawingCenter, a, b);
    diagram.solid(silhouette.map(ellipse(unitCircle)));
    const qreal facing = qRadiansToDegrees(qAtan2(standardView.eye.y() * axis1, standardView.eye.x() * axis2));
    diagram.guide(equator.map(arc(unitCircle, facing + 90, 180)));
    diagram.outline(equator.map(arc(unitCircle, facing - 90, 180)), 0.70);
    diagram.arrow(drawingCenter, drawingCenter + a, u"a", {5, -6});
    if (!spheroid)
        diagram.arrow(drawingCenter, drawingCenter + b, u"b", {-8, 8});
    diagram.arrow(drawingCenter, drawingCenter + c, u"c", {8, -1});
}

struct SphereCut {
    QRectF rim;
    qreal limbAngle;
    qreal rimAngle;
};

SphereCut sphereCut(const Projection& p, qreal radius, qreal z)
{
    if (qAbs(z) == radius)
        return {{}, z > 0 ? 90.0 : -90.0, 0};
    const qreal sectionRadius = qSqrt(radius * radius - z * z);
    // At the limb, the sphere normal is perpendicular to the line of sight.
    const qreal limb = qRadiansToDegrees(qAsin(z / (radius * p.view.cosine)));
    const qreal rim = qRadiansToDegrees(qAsin(z * p.view.sine / (sectionRadius * p.view.cosine)));
    const QPointF center = p(QVector3D(0, 0, float(z)));
    return {horizontalCircle(center, sectionRadius, p.view), limb, rim};
}

void drawSphereSlice(Diagram& diagram, const QRectF& sphere, const SphereCut& top,
                     const SphereCut& bottom, bool showSphere)
{
    const qreal sweep = bottom.limbAngle - top.limbAngle;
    QPainterPath wall = top.rim.isEmpty()
        ? arc(sphere, top.limbAngle, sweep)
        : arc(top.rim, 180 - top.rimAngle, 180 + 2 * top.rimAngle);
    if (!top.rim.isEmpty())
        wall.arcTo(sphere, top.limbAngle, sweep);
    if (!bottom.rim.isEmpty())
        wall.arcTo(bottom.rim, bottom.rimAngle, -180 - 2 * bottom.rimAngle);
    wall.arcTo(sphere, 180 - bottom.limbAngle, sweep);
    wall.closeSubpath();

    const QPainterPath lid = top.rim.isEmpty() ? QPainterPath() : ellipse(top.rim);
    if (showSphere)
        diagram.guideOutside(ellipse(sphere), wall.united(lid));
    diagram.solid(wall, 0.24);
    if (!lid.isEmpty())
        diagram.solid(lid, 0.12);
    if (!bottom.rim.isEmpty())
        diagram.guide(arc(bottom.rim, bottom.rimAngle, 180 - 2 * bottom.rimAngle));
}

void drawHemisphere(Diagram& diagram)
{
    constexpr qreal radius = 39;
    const Projection p{standardView, 1, {60, 37}};
    const QRectF sphere = sphereBounds(p.origin, radius);
    drawSphereSlice(diagram, sphere, sphereCut(p, radius, 0), sphereCut(p, radius, -radius), false);
    diagram.arrow(p.origin, ellipsePoint(sphere, 0), u"r", {0, -8});
}

void drawSphericalCap(Diagram& diagram)
{
    constexpr qreal radius = 41;
    constexpr qreal height = 22;
    const Projection p{standardView, 1, drawingCenter};
    const QRectF sphere = sphereBounds(p.origin, radius);
    const SphereCut cut = sphereCut(p, radius, radius - height);
    drawSphereSlice(diagram, sphere, sphereCut(p, radius, radius), cut, true);
    const QPointF pole = p(QVector3D(0, 0, radius));
    diagram.dot(pole);
    markVertical(diagram, rightOfRim(cut.rim), pole, u"h");
    diagram.arrow(p.origin, ellipsePoint(sphere, 223), u"r", {-9, 5});
}

void drawSphericalSegment(Diagram& diagram)
{
    constexpr qreal radius = 38;
    constexpr qreal z1 = -17;
    constexpr qreal z2 = 24;
    const Projection p{standardView, 1, {51, 47}};
    const QRectF sphere = sphereBounds(p.origin, radius);
    const SphereCut top = sphereCut(p, radius, z2);
    const SphereCut bottom = sphereCut(p, radius, z1);
    drawSphereSlice(diagram, sphere, top, bottom, true);

    // Transfer all three levels to one signed axis.
    constexpr qreal axisX = 97;
    const QPointF zero(axisX, p.origin.y());
    const QPointF upper(axisX, top.rim.center().y());
    const QPointF lower(axisX, bottom.rim.center().y());
    const QPointF tick(3, 0);
    diagram.mark(line(p.origin, zero + tick), 0.50);
    diagram.mark(line(rightOfRim(top.rim), upper + tick), 0.50);
    diagram.mark(line(rightOfRim(bottom.rim), lower + tick), 0.50);
    const QPointF axisTip = upper - QPointF(0, 11);
    diagram.axis(lower + QPointF(0, 6), axisTip);
    diagram.label(axisTip - QPointF(0, 8), u"+z");
    diagram.label(upper + QPointF(11, 0), u"z₂");
    diagram.label(lower + QPointF(11, 0), u"z₁");
    diagram.arrow(p.origin, ellipsePoint(sphere, 187), u"r", {-5, 6});
    diagram.label(p.origin + QPointF(18, 0), u"z = 0");
}

void drawTorus(Diagram& diagram)
{
    constexpr float majorRadius = 39;
    constexpr float tubeRadius = 11;
    const Projection p{baseView, 1, drawingCenter};
    const QRectF centerline = horizontalCircle(p.origin, majorRadius, p.view);
    // A torus is a ball swept along a circle. Its projection sweeps a disk along an ellipse.
    QPainterPathStroker tube;
    tube.setWidth(2 * tubeRadius);
    tube.setCurveThreshold(0.05);
    diagram.solid(tube.createStroke(ellipse(centerline)), 0.22);

    const QPointF tubeCenter = p(p.view.right * majorRadius);
    const qreal sectionHeight = p.view.cosine * tubeRadius;
    const QRectF section(tubeCenter - QPointF(tubeRadius, sectionHeight),
                         QSizeF(2 * tubeRadius, 2 * sectionHeight));
    diagram.guide(ellipse(section));
    diagram.arrow(p.origin, tubeCenter, u"R", {-4, 9});
    diagram.arrow(tubeCenter, tubeCenter - QPointF(0, sectionHeight), u"r", {17, -10});
}

void dodecahedron(Diagram& diagram)
{
    const float phi = (1 + qSqrt(5.0f)) / 2;
    const float inversePhi = 1 / phi;
    const Vertices vertices{
        {-1, -1, -1}, {-1, -1, 1}, {-1, 1, -1},
         {-1, 1, 1}, {1, -1, -1}, {1, -1, 1},
         {1, 1, -1}, {1, 1, 1}, {0, -inversePhi, -phi},
         {-phi, 0, -inversePhi}, {-inversePhi, -phi, 0}, {0, -inversePhi, phi},
         {phi, 0, -inversePhi}, {-inversePhi, phi, 0}, {0, inversePhi, -phi},
         {-phi, 0, inversePhi}, {inversePhi, -phi, 0}, {0, inversePhi, phi},
         {phi, 0, inversePhi}, {inversePhi, phi, 0}};
    const auto p = drawPolyhedron(diagram, vertices,
        {{15, 9, 0, 10, 1}, {14, 8, 0, 9, 2}, {4, 8, 0, 10, 16}, {17, 11, 1, 15, 3},
         {16, 10, 1, 11, 5}, {15, 9, 2, 13, 3}, {19, 13, 2, 14, 6}, {19, 13, 3, 17, 7},
         {18, 12, 4, 16, 5}, {14, 8, 4, 12, 6}, {17, 11, 5, 18, 7}, {18, 12, 6, 19, 7}});
    markEdge(diagram, p(vertices[16]), p(vertices[5]), u"s");
}

void icosahedron(Diagram& diagram)
{
    const float phi = (1 + qSqrt(5.0f)) / 2;
    const Vertices vertices{
        {0, -1, -phi}, {-phi, 0, -1}, {-1, -phi, 0},
         {0, -1, phi}, {phi, 0, -1}, {-1, phi, 0},
         {0, 1, -phi}, {-phi, 0, 1}, {1, -phi, 0},
         {0, 1, phi}, {phi, 0, 1}, {1, phi, 0}};
    const auto p = drawPolyhedron(diagram, vertices,
        {{2, 0, 1}, {6, 0, 1}, {8, 0, 2}, {6, 0, 4},
         {8, 0, 4}, {7, 1, 2}, {6, 1, 5}, {7, 1, 5},
         {7, 2, 3}, {8, 2, 3}, {9, 3, 7}, {10, 3, 8},
         {10, 3, 9}, {11, 4, 6}, {10, 4, 8}, {11, 4, 10},
         {11, 5, 6}, {9, 5, 7}, {11, 5, 9}, {11, 9, 10}});
    markEdge(diagram, p(vertices[8]), p(vertices[10]), u"s");
}

void drawTriangleMesh(Diagram& diagram)
{
    const Vertices vertices{{-29, -9, 0}, {-12, -26, 0}, {26, -19, 0},
                            {34, 12, 0}, {7, 25, 0}, {-23, 19, 0},
                            {-3, 4, 28}, {6, -2, -22}};
    constexpr int rimCount = 6;
    constexpr int top = rimCount;
    constexpr int bottom = rimCount + 1;
    constexpr int featuredSide = 3;
    Faces faces;
    for (int i = 0; i < rimCount; ++i) {
        const int next = (i + 1) % rimCount;
        faces.append({top, i, next});
        faces.append({bottom, next, i});
    }
    const auto p = drawPolyhedron(diagram, vertices, faces, {}, standardView, 2 * featuredSide);
    const QList<int>& face = faces[2 * featuredSide];
    const QPointF first = p(vertices[face[0]]);
    const QPointF second = p(vertices[face[1]]);
    const QPointF third = p(vertices[face[2]]);
    for (const QPointF& vertex : {first, second, third})
        diagram.dot(vertex);
    diagram.leader(first, first + QPointF(-7, -11), u"p");
    diagram.leader(second, second + QPointF(13, -4), u"q");
    diagram.leader(third, third + QPointF(-9, 14), u"r");
}

void drawParallelepiped(Diagram& diagram)
{
    const QPointF a(-57, 0);
    const QPointF b(-8, -41);
    const QPointF shear(-11, 8);
    constexpr float height = 33;
    const QList<QPointF> base{a + b, b, {0, 0}, a};
    const auto top = scaledBase(base, 1, shear);
    const auto p = drawLayers(diagram, base, top, height, false, baseView);
    const QPointF origin = p(QPointF());
    diagram.vectorDimension(origin, p(a), u"a", {-4, 10});
    diagram.vectorDimension(origin, p(b), u"b", {10, 7});
    diagram.vectorDimension(p(a), p(a + shear, height), u"c", {-12, 6});
    diagram.dot(origin);
}

void drawTetrahedron(Diagram& diagram)
{
    const Vertices vertices{{-30, -14, 0}, {26, -16, 8}, {6, 24, 0}, {-5, 0, 47}};
    const auto p = drawPolyhedron(diagram, vertices,
                                 {{0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3}}, {0, 1, 2});
    const QPointF origin = p(vertices[2]);
    diagram.arrow(origin, p(vertices[0]), u"a", {-7, 9});
    diagram.arrow(origin, p(vertices[1]), u"b", {9, 8});
    diagram.arrow(origin, p(vertices[3]), u"c", {-9, 0});
    diagram.label(origin + QPointF(0, 10), u"p");
    diagram.label(p(vertices[1]) + QPointF(10, -4), u"q");
    diagram.label(p(vertices[3]) + QPointF(0, -9), u"r");
}

struct Drawing {
    const char* shape;
    void (*paint)(Diagram&);
};

constexpr Drawing drawings[] = {
    {"annular-sector", [](Diagram& d) { drawSector(d, true); }},
    {"annulus", drawAnnulus},
    {"circle", [](Diagram& d) {
        const QRectF bounds(27, 12, 66, 66);
        d.solid(ellipse(bounds));
        markRadiusAndDiameter(d, bounds);
    }},
    {"circle-overlap", drawCircleOverlap},
    {"sector", [](Diagram& d) { drawSector(d, false); }},
    {"circular-segment", drawCircularSegment},
    {"ellipse", [](Diagram& d) {
        const QRectF bounds(14, 21, 92, 48);
        d.solid(ellipse(bounds));
        d.arrow(bounds.center(), ellipsePoint(bounds, 0), u"a", {0, -8});
        d.arrow(bounds.center(), ellipsePoint(bounds, 90), u"b", {-8, 0});
    }},
    {"kite", [](Diagram& d) { drawDiagonals(d, 37, 28); }},
    {"parabolic-segment", [](Diagram& d) {
        constexpr qreal chord = 86;
        constexpr qreal rise = 41;
        const QPointF center(60, 64);
        const QPointF left = center - QPointF(chord / 2, 0);
        const QPointF right = center + QPointF(chord / 2, 0);
        // Half of the control point displacement gives the rise at the vertex.
        QPainterPath area(left);
        area.quadTo(center - QPointF(0, 2 * rise), right);
        area.closeSubpath();
        d.solid(area);
        d.dimension(left, right, u"b", {0, 14});
        d.dimension(center, center - QPointF(0, rise), u"h", {}, {8, 0});
    }},
    {"parallelogram", [](Diagram& d) {
        d.solid(polygon({{32, 20}, {107, 20}, {88, 70}, {13, 70}}));
        d.dimension({13, 70}, {88, 70}, u"b", {0, 13});
        d.dimension({32, 20}, {32, 70}, u"h", {}, {8, 0});
    }},
    {"polygon", [](Diagram& d) {
        const QPointF first(31, 19), second(74, 14);
        d.solid(polygon({{18, 55}, first, second, {65, 39}, {104, 48}, {85, 75}, {43, 72}}));
        d.leader(first, {23, 0}, u"(x₁, y₁)");
        d.leader(second, {92, -1}, u"(x₂, y₂)");
    }},
    {"rectangle", [](Diagram& d) { drawRoundedRectangle(d, 0); }},
    {"regular-polygon", [](Diagram& d) {
        const auto vertices = regularPolygon(6, 36, {60, 45});
        d.solid(polygon(vertices));
        markEdge(d, vertices[2], vertices[3], u"s");
        d.label({60, 45}, u"n = 6");
    }},
    {"rhombus", [](Diagram& d) { drawDiagonals(d, 45, 45); }},
    {"right-triangle", [](Diagram& d) {
        const QPointF corner(28, 74), right(98, 74), top(28, 18);
        const QPointF inside = (corner + right + top) / 3;
        d.solid(polygon({top, right, corner}));
        QPainterPath square(corner + QPointF(8, 0));
        square.lineTo(corner + QPointF(8, -8));
        square.lineTo(corner + QPointF(0, -8));
        d.mark(square);
        markEdge(d, corner, top, u"a", inside);
        markEdge(d, corner, right, u"b", inside);
        markEdge(d, top, right, u"c", inside);
    }},
    {"rounded-rectangle", [](Diagram& d) { drawRoundedRectangle(d, 9); }},
    {"square", [](Diagram& d) {
        d.solid(polygon(rectangleBase(60, 60)).translated(60, 45));
        d.dimension({30, 75}, {90, 75}, u"s", {0, 12});
    }},
    {"stadium", drawStadium},
    {"trapezoid", [](Diagram& d) {
        d.solid(polygon({{34, 20}, {79, 20}, {106, 70}, {14, 70}}));
        d.dimension({34, 20}, {79, 20}, u"b₁", {0, -13});
        d.dimension({14, 70}, {106, 70}, u"b₂", {0, 13});
        d.dimension({51, 20}, {51, 70}, u"h", {}, {8, 0});
    }},
    {"triangle", drawTriangle},
    {"cube", [](Diagram& d) { drawBox(d, 42, 42, 42, true); }},
    {"cuboid", [](Diagram& d) { drawBox(d, 65, 36, 27, false); }},
    {"prism", drawGeneralPrism},
    {"prismoid", drawPrismoid},
    {"pyramid", drawGeneralPyramid},
    {"pyramid-frustum", drawPyramidFrustum},
    {"rectangular-pyramid", [](Diagram& d) { drawRectangularPyramid(d, 70, 32, false); }},
    {"regular-prism", drawRegularPrism},
    {"regular-pyramid", drawRegularPyramid},
    {"regular-pyramid-frustum", drawRegularPyramidFrustum},
    {"square-pyramid", [](Diagram& d) { drawRectangularPyramid(d, 48, 48, true); }},
    {"triangular-prism", drawTriangularPrism},
    {"capsule", drawCapsule},
    {"cone", drawCone},
    {"cone-frustum", drawConeFrustum},
    {"cylinder", drawCircularCylinder},
    {"cylindrical-segment", drawCylindricalSegment},
    {"ellipsoid", [](Diagram& d) { drawEllipsoid(d, 52, 34, 31); }},
    {"elliptical-cylinder", drawEllipticalCylinder},
    {"hemisphere", drawHemisphere},
    {"hollow-cylinder", drawHollowCylinder},
    {"sphere", [](Diagram& d) {
        const QRectF bounds(23, 8, 74, 74);
        drawGlobe(d, bounds);
        markRadiusAndDiameter(d, bounds);
    }},
    {"spherical-cap", drawSphericalCap},
    {"spherical-segment", drawSphericalSegment},
    {"spherical-shell", [](Diagram& d) { drawGlobe(d, QRectF(23, 8, 74, 74), true); }},
    {"spheroid", [](Diagram& d) { drawEllipsoid(d, 45, 45, 26, true); }},
    {"torus", drawTorus},
    {"parallelepiped", drawParallelepiped},
    {"regular-dodecahedron", dodecahedron},
    {"regular-icosahedron", icosahedron},
    {"regular-octahedron", [](Diagram& d) {
        const Vertices vertices{{1, 0, 0}, {0, 1, 0}, {-1, 0, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        const auto p = drawPolyhedron(d, vertices, {{4, 0, 1}, {4, 1, 2}, {4, 2, 3}, {4, 3, 0},
                                                    {5, 0, 1}, {5, 1, 2}, {5, 2, 3}, {5, 3, 0}});
        markEdge(d, p(vertices[5]), p(vertices[0]), u"s");
    }},
    {"regular-tetrahedron", [](Diagram& d) {
        const auto base = regularPolygon(3, 30);
        const auto p = drawLayers(d, base, {{0, 0}}, 30 * qSqrt(2.0f));
        markEdge(d, p(base[1]), p(base[2]), u"s");
    }},
    {"tetrahedron", drawTetrahedron},
    {"mesh", drawTriangleMesh},
};

} // namespace

void BookGeometryDiagrams::draw(QPainter& painter, const QRectF& rect, QStringView shape,
                                const QColor& foreground, const QColor& background)
{
    const auto drawing = std::find_if(std::begin(drawings), std::end(drawings),
                                      [shape](const Drawing& entry) { return shape == QLatin1String(entry.shape); });
    if (drawing == std::end(drawings))
        return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal scale = qMin(rect.width() / canvasSize.width(), rect.height() / canvasSize.height());
    painter.translate(rect.center());
    painter.scale(scale, scale);
    painter.translate(-drawingCenter);

    Diagram diagram(painter, foreground, background);
    drawing->paint(diagram);
    painter.restore();
}
