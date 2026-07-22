#include "SelectionHandles.h"

#include <QBrush>
#include <QPainter>
#include <QPen>
#include <QPolygonF>

#include <cmath>

// Handle order: TL=0, TC=1, TR=2, ML=3, MR=4, BL=5, BC=6, BR=7

static QPointF handleCenter(int idx, const QRectF& r)
{
    const double l = r.left();
    const double t = r.top();
    const double cx = r.center().x();
    const double cy = r.center().y();
    const double ri = r.right();
    const double bo = r.bottom();

    switch (idx) {
    case 0:
        return {l, t}; // TL
    case 1:
        return {cx, t}; // TC
    case 2:
        return {ri, t}; // TR
    case 3:
        return {l, cy}; // ML
    case 4:
        return {ri, cy}; // MR
    case 5:
        return {l, bo}; // BL
    case 6:
        return {cx, bo}; // BC
    case 7:
        return {ri, bo}; // BR
    default:
        return {};
    }
}

// --- Transform-based (rotation/shear-aware) overloads ---

QPointF SelectionHandles::handleCenterT(int idx, const QTransform& xf, const QRectF& localRect)
{
    if (idx == kRotateHandle) {
        const QPointF topC = xf.map(QPointF(localRect.center().x(), localRect.top()));
        const QPointF center = xf.map(localRect.center());
        QPointF up = topC - center;
        const double len = std::hypot(up.x(), up.y());
        const QPointF n = (len > 1e-6) ? QPointF(up.x() / len, up.y() / len) : QPointF(0.0, -1.0);
        return topC + n * kRotateGap;
    }

    return xf.map(handleCenter(idx, localRect));
}

QRectF SelectionHandles::handleRect(int handleIndex, const QTransform& xf, const QRectF& localRect)
{
    const double half = kHandleSize / 2.0;
    const QPointF c = handleCenterT(handleIndex, xf, localRect);
    return {c.x() - half, c.y() - half, static_cast<double>(kHandleSize),
            static_cast<double>(kHandleSize)};
}

int SelectionHandles::hitTest(QPointF widgetPt, const QTransform& xf, const QRectF& localRect)
{
    int best = -1;
    double bestDist = 0.0;

    for (int i = 0; i < kHandleCount; ++i) {
        const QRectF r = handleRect(i, xf, localRect)
                             .adjusted(-kHitSlack, -kHitSlack, kHitSlack, kHitSlack);
        if (!r.contains(widgetPt))
            continue;
        const QPointF c = handleCenterT(i, xf, localRect);
        const double dx = widgetPt.x() - c.x();
        const double dy = widgetPt.y() - c.y();
        const double d = dx * dx + dy * dy;
        if (best < 0 || d < bestDist) {
            best = i;
            bestDist = d;
        }
    }

    // Rotate handle
    {
        const QPointF c = handleCenterT(kRotateHandle, xf, localRect);
        const double dx = widgetPt.x() - c.x();
        const double dy = widgetPt.y() - c.y();
        const double d = dx * dx + dy * dy;
        const double threshold = kRotateRadius + kHitSlack;
        if (d <= threshold * threshold) {
            if (best < 0 || d < bestDist) {
                best = kRotateHandle;
                bestDist = d;
            }
        }
    }

    return best;
}

void SelectionHandles::draw(QPainter& p, const QTransform& xf, const QRectF& localRect,
                            int activeHandle, QColor highlight)
{
    p.save();

    // Selection border — mapped quad, drawn with AA so rotated edges aren't jagged.
    p.setRenderHint(QPainter::Antialiasing, true);

    QPolygonF poly;
    poly << xf.map(localRect.topLeft()) << xf.map(localRect.topRight())
         << xf.map(localRect.bottomRight()) << xf.map(localRect.bottomLeft());

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 0, 0, 120), 3.0));
    p.drawPolygon(poly);
    p.setPen(QPen(Qt::white, 1.0));
    p.drawPolygon(poly);

    // Rotate handle connector + knob (AA on, matches the border style).
    const QPointF topC = xf.map(QPointF(localRect.center().x(), localRect.top()));
    const QPointF knob = handleCenterT(kRotateHandle, xf, localRect);

    p.setPen(QPen(QColor(0, 0, 0, 100), 3.0));
    p.drawLine(topC, knob);
    p.setPen(QPen(Qt::white, 1.0));
    p.drawLine(topC, knob);

    {
        const bool isActive = (activeHandle == kRotateHandle);
        p.setPen(QPen(QColor(0, 0, 0, 100), 5.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(knob, kRotateRadius, kRotateRadius);

        p.setBrush(isActive ? highlight : Qt::white);
        p.setPen(QPen(Qt::white, 1.5));
        p.drawEllipse(knob, kRotateRadius, kRotateRadius);
    }

    // Square handles 0-7 — AA off to keep the crisp look of the original squares.
    p.setRenderHint(QPainter::Antialiasing, false);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < kHandleCount; ++i) {
            const bool isActive = (i == activeHandle);
            if (pass == 0 && isActive)
                continue;
            if (pass == 1 && !isActive)
                continue;

            const QRectF r = handleRect(i, xf, localRect);

            p.setPen(QPen(QColor(0, 0, 0, 100), 5.0));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r);

            p.setBrush(isActive ? highlight : Qt::white);
            p.setPen(QPen(Qt::white, 1.5));
            p.drawRect(r);
        }
    }

    p.restore();
}
