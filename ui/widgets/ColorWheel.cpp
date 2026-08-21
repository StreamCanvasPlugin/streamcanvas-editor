#include "ColorWheel.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QtMath>
#include <array>
#include <cmath>

// Angle convention throughout:
//   0° = east (right), increases clockwise on screen (Qt y-down).
//   To convert degrees → screen offset: dx = r*cos(rad), dy = r*sin(rad)
//   with rad = deg * π/180.  This matches atan2(dy, dx) for mouse positions.

static constexpr qreal kRingFraction = 0.22; // ring width as fraction of outer radius

ColorWheel::ColorWheel(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(100, 100);
}

QColor ColorWheel::color() const
{
    return m_color;
}

void ColorWheel::setColor(const QColor& c)
{
    if (c == m_color)
        return;
    int oldHue = m_hue;
    m_color = c;
    if (c.hsvHue() >= 0)
        m_hue = c.hsvHue();
    if (m_hue != oldHue)
        m_triangleDirty = true;
    update();
}

QSize ColorWheel::sizeHint() const
{
    return {200, 200};
}
bool ColorWheel::hasHeightForWidth() const
{
    return true;
}
int ColorWheel::heightForWidth(int w) const
{
    return w;
}

QPointF ColorWheel::center() const
{
    return {width() / 2.0, height() / 2.0};
}

qreal ColorWheel::outerRadius() const
{
    return qMin(width(), height()) / 2.0 - 1.0;
}

qreal ColorWheel::innerRadius() const
{
    return outerRadius() * (1.0 - kRingFraction);
}

// Returns the 3 vertices of the HSV triangle inscribed in the inner circle:
//   [0] = pure hue  (S=1, V=1)
//   [1] = white     (S=0, V=1) — 120° clockwise from hue
//   [2] = black     (S=0, V=0) — 240° clockwise from hue
std::array<QPointF, 3> ColorWheel::triangleVertices() const
{
    QPointF c = center();
    qreal r = innerRadius();
    int hue = m_hue;

    auto pt = [&](qreal deg) -> QPointF {
        qreal rad = -qDegreesToRadians(deg);
        return c + QPointF(r * std::cos(rad), r * std::sin(rad));
    };

    qreal base = (qreal)hue;
    return {pt(base), pt(base + 120.0), pt(base + 240.0)};
}

// Screen-space position corresponding to the current S/V values.
QPointF ColorWheel::svPosition() const
{
    qreal S = m_color.hsvSaturationF();
    qreal V = m_color.valueF();
    // Barycentric: wHue = S*V, wWhite = V*(1-S), wBlack = 1-V
    qreal wHue = S * V;
    qreal wWhite = V * (1.0 - S);
    qreal wBlack = 1.0 - V;
    auto verts = triangleVertices();
    return verts[0] * wHue + verts[1] * wWhite + verts[2] * wBlack;
}

bool ColorWheel::pointInRing(QPointF p) const
{
    QPointF d = p - center();
    qreal distSq = d.x() * d.x() + d.y() * d.y();
    qreal rI = innerRadius();
    qreal rO = outerRadius();
    return distSq >= rI * rI && distSq <= rO * rO;
}

bool ColorWheel::pointInTriangle(QPointF p) const
{
    auto v = triangleVertices();
    QPointF e0 = v[1] - v[0], e1 = v[2] - v[0], ep = p - v[0];
    qreal denom = e0.x() * e1.y() - e0.y() * e1.x();
    if (qAbs(denom) < 1e-8)
        return false;
    qreal u = (ep.x() * e1.y() - ep.y() * e1.x()) / denom;
    qreal vc = (e0.x() * ep.y() - e0.y() * ep.x()) / denom;
    return u >= -1e-6 && vc >= -1e-6 && (u + vc) <= 1.0 + 1e-6;
}

void ColorWheel::handleRingDrag(QPointF p)
{
    QPointF d = p - center();
    qreal angleDeg = qRadiansToDegrees(-std::atan2(d.y(), d.x()));
    if (angleDeg < 0.0)
        angleDeg += 360.0;
    int hue = qRound(angleDeg) % 360;

    if (hue != m_hue) {
        m_hue = hue;
        m_triangleDirty = true;
    }

    QColor c = m_color;
    c.setHsv(hue, c.hsvSaturation(), c.value(), c.alpha());
    if (c == m_color) {
        update();
        return;
    }
    m_color = c;
    update();
    emit colorChanged(m_color);
}

void ColorWheel::handleTriangleDrag(QPointF p)
{
    auto v = triangleVertices();
    QPointF e0 = v[1] - v[0], e1 = v[2] - v[0], ep = p - v[0];
    qreal denom = e0.x() * e1.y() - e0.y() * e1.x();
    if (qAbs(denom) < 1e-8)
        return;

    qreal u = (ep.x() * e1.y() - ep.y() * e1.x()) / denom;  // weight of v[1]=white
    qreal vc = (e0.x() * ep.y() - e0.y() * ep.x()) / denom; // weight of v[2]=black
    qreal w = 1.0 - u - vc;                                 // weight of v[0]=hue

    // Clamp to triangle
    u = qMax(0.0, u);
    vc = qMax(0.0, vc);
    w = qMax(0.0, w);
    qreal sum = u + vc + w;
    if (sum > 1e-8) {
        u /= sum;
        vc /= sum;
        w /= sum;
    }

    qreal V_val = w + u; // wHue + wWhite
    qreal S_val = (V_val > 1e-8) ? w / V_val : 0.0;

    QColor nc = QColor::fromHsvF(m_hue / 360.0, S_val, V_val, m_color.alphaF());
    if (nc == m_color)
        return;
    m_color = nc;
    update();
    emit colorChanged(m_color);
}

void ColorWheel::rebuildRing()
{
    QSize sz = size();
    const qreal dpr = devicePixelRatioF();
    m_ringImage = QImage(QSize(qRound(sz.width() * dpr), qRound(sz.height() * dpr)),
                         QImage::Format_ARGB32);
    m_ringImage.setDevicePixelRatio(dpr);
    m_ringImage.fill(Qt::transparent);

    QPainter p(&m_ringImage);
    p.setRenderHint(QPainter::Antialiasing);

    QPointF c = center();
    qreal rO = outerRadius();
    qreal rI = innerRadius();

    // Full-spectrum conical gradient
    QConicalGradient grad(c, 0.0);
    for (int h = 0; h <= 360; h += 3)
        grad.setColorAt(h / 360.0, QColor::fromHsv(h % 360, 255, 255));

    QPainterPath ring;
    ring.addEllipse(c, rO, rO);
    QPainterPath inner;
    inner.addEllipse(c, rI, rI);
    ring -= inner;

    p.fillPath(ring, grad);
    m_ringDirty = false;
}

// HSV-triangle identity used to avoid per-pixel QColor::fromHsvF calls:
// for barycentric weights (w = hue corner, u = white corner, vc = black
// corner, w+u+vc=1) the resulting HSV colour with V=w+u, S=w/(w+u) equals
// the plain linear blend  w*hueRGB + u*white + vc*black  in RGB space
// (white=(255,255,255), black=(0,0,0), so the vc term drops out). This is
// the standard construction used by HSV-triangle colour pickers and is
// exact, not an approximation — it lets the inner loop do a couple of
// integer multiply-adds per pixel instead of constructing a QColor.
void ColorWheel::rebuildTriangle()
{
    QSize sz = size();
    const qreal dpr = devicePixelRatioF();
    const QSize physSz(qRound(sz.width() * dpr), qRound(sz.height() * dpr));

    // Cache hit: hue, physical size and DPR are all unchanged, so the
    // previously rendered image is still correct — skip the rebuild.
    if (!m_triangleImage.isNull() && m_cachedTriangleHue == m_hue
        && m_triangleImage.size() == physSz
        && qFuzzyCompare(m_triangleImage.devicePixelRatio(), dpr)) {
        m_triangleDirty = false;
        return;
    }

    m_triangleImage = QImage(physSz, QImage::Format_ARGB32);
    m_triangleImage.setDevicePixelRatio(dpr);
    m_triangleImage.fill(Qt::transparent);

    auto verts = triangleVertices();
    QPointF v0 = verts[0] * dpr; // hue    (physical coords)
    QPointF v1 = verts[1] * dpr; // white
    QPointF v2 = verts[2] * dpr; // black

    // Pure-hue corner colour, computed once (not per pixel).
    const QColor hueColor = QColor::fromHsv(m_hue, 255, 255);
    const int hr = hueColor.red();
    const int hg = hueColor.green();
    const int hb = hueColor.blue();

    // Precompute edge vectors and inverse denominator for barycentric coords
    QPointF e0 = v1 - v0, e1 = v2 - v0;
    qreal denom = e0.x() * e1.y() - e0.y() * e1.x();
    if (qAbs(denom) < 1e-8) {
        m_triangleDirty = false;
        m_cachedTriangleHue = m_hue;
        return;
    }
    qreal invD = 1.0 / denom;

    int x0 = qMax(0, (int)std::floor(std::min({v0.x(), v1.x(), v2.x()})));
    int x1 = qMin(physSz.width() - 1, (int)std::ceil(std::max({v0.x(), v1.x(), v2.x()})));
    int y0 = qMax(0, (int)std::floor(std::min({v0.y(), v1.y(), v2.y()})));
    int y1 = qMin(physSz.height() - 1, (int)std::ceil(std::max({v0.y(), v1.y(), v2.y()})));

    for (int y = y0; y <= y1; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(m_triangleImage.scanLine(y));
        for (int x = x0; x <= x1; ++x) {
            QPointF ep((qreal)x - v0.x(), (qreal)y - v0.y());
            qreal u = (ep.x() * e1.y() - ep.y() * e1.x()) * invD;  // weight v1 (white)
            qreal vc = (e0.x() * ep.y() - e0.y() * ep.x()) * invD; // weight v2 (black)
            qreal w = 1.0 - u - vc;                                // weight v0 (hue)
            if (u < -1e-4 || vc < -1e-4 || w < -1e-4)
                continue;
            u = qBound(0.0, u, 1.0);
            w = qBound(0.0, w, 1.0);

            // Integer blend: RGB = w*hueRGB + u*white (vc*black contributes 0).
            const int r = qBound(0, qRound(w * hr + u * 255.0), 255);
            const int g = qBound(0, qRound(w * hg + u * 255.0), 255);
            const int b = qBound(0, qRound(w * hb + u * 255.0), 255);
            line[x] = qRgba(r, g, b, 255);
        }
    }

    m_triangleDirty = false;
    m_cachedTriangleHue = m_hue;
}

void ColorWheel::paintEvent(QPaintEvent*)
{
    if (m_ringDirty)
        rebuildRing();
    if (m_triangleDirty)
        rebuildTriangle();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.drawImage(0, 0, m_ringImage);
    p.drawImage(0, 0, m_triangleImage);

    QPointF c = center();

    // Hue indicator on the ring
    {
        qreal rad = -qDegreesToRadians((qreal)m_hue);
        qreal rMid = (innerRadius() + outerRadius()) / 2.0;
        QPointF ind = c + QPointF(rMid * std::cos(rad), rMid * std::sin(rad));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::white, 2));
        p.drawEllipse(ind, 5, 5);
        p.setPen(QPen(Qt::black, 1));
        p.drawEllipse(ind, 6, 6);
    }

    // S/V indicator inside the triangle
    {
        QPointF svPos = svPosition();
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::white, 2));
        p.drawEllipse(svPos, 5, 5);
        p.setPen(QPen(Qt::black, 1));
        p.drawEllipse(svPos, 6, 6);
    }
}

void ColorWheel::resizeEvent(QResizeEvent*)
{
    m_ringDirty = true;
    m_triangleDirty = true;
}

void ColorWheel::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return;
    QPointF pos = QPointF(e->pos());
    if (pointInRing(pos)) {
        m_drag = DragZone::Ring;
        handleRingDrag(pos);
    } else if (pointInTriangle(pos)) {
        m_drag = DragZone::Triangle;
        handleTriangleDrag(pos);
    }
}

void ColorWheel::mouseMoveEvent(QMouseEvent* e)
{
    if (!(e->buttons() & Qt::LeftButton))
        return;
    QPointF pos = QPointF(e->pos());
    if (m_drag == DragZone::Ring)
        handleRingDrag(pos);
    else if (m_drag == DragZone::Triangle)
        handleTriangleDrag(pos);
}

void ColorWheel::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
        m_drag = DragZone::None;
}
