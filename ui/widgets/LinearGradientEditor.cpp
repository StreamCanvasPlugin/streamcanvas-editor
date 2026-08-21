#include "LinearGradientEditor.h"
#include "ColorUtils.h"
#include "GradientHandlePainter.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <cmath>

static QGradientStops toQGradientStops(const QVector<GradientStop>& stops)
{
    QGradientStops r;
    r.reserve(stops.size());
    for (const auto& s : stops)
        r.append({s.position, s.color});
    return r;
}

static QPointF clampNorm(QPointF p)
{
    return {qBound(0.0, p.x(), 1.0), qBound(0.0, p.y(), 1.0)};
}

LinearGradientEditor::LinearGradientEditor(QWidget* parent) : GradientGeometryEditor(parent)
{
    setMinimumSize(100, 100);
}

void LinearGradientEditor::setP1(QPointF p)
{
    m_p1 = p;
    update();
}

void LinearGradientEditor::setP2(QPointF p)
{
    m_p2 = p;
    update();
}

qreal LinearGradientEditor::angleDegrees() const
{
    QPointF d = m_p2 - m_p1;
    qreal ex = d.x() * targetWidth();
    qreal ey = d.y() * targetHeight();
    return std::atan2(ey, ex) * 180.0 / M_PI;
}

qreal LinearGradientEditor::lengthFraction() const
{
    QPointF d = m_p2 - m_p1;
    qreal ex = d.x() * targetWidth();
    qreal ey = d.y() * targetHeight();
    qreal diag = std::hypot(targetWidth(), targetHeight());
    return diag > 1e-9 ? std::hypot(ex, ey) / diag : 0.0;
}

// Both setters below pivot about the MIDPOINT of the gradient line, keeping it
// fixed and moving p1 and p2 symmetrically. Pinning p1 instead (the earlier
// behaviour) makes typing an angle slide the whole gradient across the element
// rather than rotating it in place. Centre-pivot matches Figma, Illustrator and
// CSS linear-gradient.
//
// Endpoints are deliberately not clamped to 0..1: clamping would silently
// change the angle the user asked for, and out-of-range stops are valid — the
// engine's Cairo pattern extends past them.
void LinearGradientEditor::setAngleDegrees(qreal deg)
{
    qreal tw = targetWidth() > 0 ? targetWidth() : 1.0;
    qreal th = targetHeight() > 0 ? targetHeight() : 1.0;

    const QPointF mid = (m_p1 + m_p2) / 2.0;

    QPointF d = m_p2 - m_p1;
    qreal elemLen = std::hypot(d.x() * tw, d.y() * th);

    qreal rad = deg * M_PI / 180.0;
    qreal hx = 0.5 * elemLen * std::cos(rad);
    qreal hy = 0.5 * elemLen * std::sin(rad);

    const QPointF half(hx / tw, hy / th);
    m_p1 = mid - half;
    m_p2 = mid + half;
    update();
}

void LinearGradientEditor::setLengthFraction(qreal len)
{
    qreal tw = targetWidth() > 0 ? targetWidth() : 1.0;
    qreal th = targetHeight() > 0 ? targetHeight() : 1.0;
    qreal diag = std::hypot(tw, th);

    const QPointF mid = (m_p1 + m_p2) / 2.0;

    qreal angle = angleDegrees() * M_PI / 180.0;
    qreal elemLen = qMax(0.0, len) * diag;
    qreal hx = 0.5 * elemLen * std::cos(angle);
    qreal hy = 0.5 * elemLen * std::sin(angle);

    const QPointF half(hx / tw, hy / th);
    m_p1 = mid - half;
    m_p2 = mid + half;
    update();
}

void LinearGradientEditor::setHorizontal()
{
    m_p1 = {0.1, 0.5};
    m_p2 = {0.9, 0.5};
    update();
}

void LinearGradientEditor::setVertical()
{
    m_p1 = {0.5, 0.1};
    m_p2 = {0.5, 0.9};
    update();
}

void LinearGradientEditor::setDiagonalDown()
{
    m_p1 = {0.1, 0.1};
    m_p2 = {0.9, 0.9};
    update();
}

void LinearGradientEditor::setDiagonalUp()
{
    m_p1 = {0.1, 0.9};
    m_p2 = {0.9, 0.1};
    update();
}

void LinearGradientEditor::reverse()
{
    std::swap(m_p1, m_p2);
    update();
}

QPointF LinearGradientEditor::stopPixelPos(int i) const
{
    QPointF norm = m_p1 + m_stops[i].position * (m_p2 - m_p1);
    return toPixel(norm);
}

QPointF LinearGradientEditor::snapAngleForDrag(QPointF anchorNorm, QPointF rawNorm) const
{
    qreal tw = targetWidth() > 0 ? targetWidth() : 1.0;
    qreal th = targetHeight() > 0 ? targetHeight() : 1.0;

    QPointF dNorm = rawNorm - anchorNorm;
    qreal ex = dNorm.x() * tw;
    qreal ey = dNorm.y() * th;
    qreal elemLen = std::hypot(ex, ey);
    if (elemLen < 1e-9)
        return rawNorm;

    const qreal step = M_PI / 12.0; // 15 degrees
    qreal angle = std::round(std::atan2(ey, ex) / step) * step;

    QPointF snappedNorm(elemLen * std::cos(angle) / tw, elemLen * std::sin(angle) / th);
    return anchorNorm + snappedNorm;
}

LinearGradientEditor::HitResult LinearGradientEditor::hitTest(QPointF px) const
{
    QPointF p1px = toPixel(m_p1), p2px = toPixel(m_p2);
    if (GHP::hitCircle(px, p2px, kEndpointR))
        return {Handle::P2, -1};
    if (GHP::hitCircle(px, p1px, kEndpointR))
        return {Handle::P1, -1};
    int stopIdx = hitTestStopMarker(px, [this](int i) { return stopPixelPos(i); });
    if (stopIdx >= 0)
        return {Handle::Stop, stopIdx};
    return {Handle::None, -1};
}

void LinearGradientEditor::updateHover(QPointF pos)
{
    HitResult hit = hitTest(pos);
    bool changed = hit.type != m_hoverHandle;
    m_hoverHandle = hit.type;
    if (changed)
        update();

    switch (hit.type) {
    case Handle::P1:
        setCursor(Qt::OpenHandCursor);
        setToolTip(tr("Gradient start — drag to move, hold Shift to snap the angle"));
        break;
    case Handle::P2:
        setCursor(Qt::OpenHandCursor);
        setToolTip(tr("Gradient end — drag to move, hold Shift to snap the angle"));
        break;
    case Handle::Stop:
        setCursor(Qt::PointingHandCursor);
        setToolTip(tr("Click to select this stop"));
        break;
    default:
        unsetCursor();
        setToolTip(QString());
        break;
    }
}

void LinearGradientEditor::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    drawBackground(p);

    QRectF preview = previewRect();
    QPointF p1px = toPixel(m_p1), p2px = toPixel(m_p2);

    if (preview.width() > 0 && preview.height() > 0 && m_stops.size() >= 2) {
        p.save();
        p.setClipRect(preview);
        QLinearGradient grad(p1px, p2px);
        for (const auto& s : m_stops)
            grad.setColorAt(qBound(0.0, s.position, 1.0), s.color);
        p.fillRect(preview, grad);
        p.restore();
    }

    p.setPen(QPen(QColor(0, 0, 0, 100), 1.5));
    p.drawLine(p1px, p2px);

    drawStopMarkers(p, [this](int i) { return stopPixelPos(i); });

    QGradientStops qstops = toQGradientStops(m_stops);
    QColor startColor = m_stops.isEmpty() ? Qt::black : sampleGradient(qstops, 0.0);
    QColor endColor = m_stops.isEmpty() ? Qt::white : sampleGradient(qstops, 1.0);
    GHP::paintCircleHandle(p, p1px, kEndpointR, startColor,
                           m_hoverHandle == Handle::P1 || m_pressHandle == Handle::P1, palette());
    GHP::paintCircleHandle(p, p2px, kEndpointR, endColor,
                           m_hoverHandle == Handle::P2 || m_pressHandle == Handle::P2, palette());
}

void LinearGradientEditor::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(e);
        return;
    }
    setFocus(Qt::MouseFocusReason);

    QPointF pos = e->position();
    HitResult hit = hitTest(pos);

    if (hit.type == Handle::Stop) {
        selectStopAndEmit(hit.stopIndex);
        m_pressHandle = Handle::None;
        return;
    }
    if (hit.type == Handle::P1 || hit.type == Handle::P2) {
        m_pressHandle = hit.type;
        m_activeHandle = hit.type;
        beginPress(pos);
        update();
        return;
    }
    m_pressHandle = Handle::None;
}

void LinearGradientEditor::mouseMoveEvent(QMouseEvent* e)
{
    QPointF pos = e->position();

    if ((e->buttons() & Qt::LeftButton) && m_pressHandle != Handle::None) {
        crossedDragThreshold(pos);
        if (isDragging()) {
            QPointF raw = clampNorm(toNorm(pos));
            bool snap = e->modifiers() & Qt::ShiftModifier;

            if (m_pressHandle == Handle::P1) {
                m_p1 = clampNorm(snap ? snapAngleForDrag(m_p2, raw) : raw);
                emit p1Changed(m_p1);
            } else if (m_pressHandle == Handle::P2) {
                m_p2 = clampNorm(snap ? snapAngleForDrag(m_p1, raw) : raw);
                emit p2Changed(m_p2);
            }
            update();
        }
        return;
    }

    updateHover(pos);
}

void LinearGradientEditor::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(e);
        return;
    }
    endPress();
    m_pressHandle = Handle::None;
    updateHover(e->position());
    update();
}

void LinearGradientEditor::mouseDoubleClickEvent(QMouseEvent* e)
{
    HitResult hit = hitTest(e->position());
    if (hit.type == Handle::Stop)
        selectStopAndEmit(hit.stopIndex);
}

void LinearGradientEditor::leaveEvent(QEvent* e)
{
    QWidget::leaveEvent(e);
    m_hoverHandle = Handle::None;
    unsetCursor();
    setToolTip(QString());
    update();
}

void LinearGradientEditor::keyPressEvent(QKeyEvent* e)
{
    qreal step = (e->modifiers() & Qt::ShiftModifier) ? 0.02 : 0.005;
    QPointF delta(0, 0);
    switch (e->key()) {
    case Qt::Key_Left:
        delta = {-step, 0};
        break;
    case Qt::Key_Right:
        delta = {step, 0};
        break;
    case Qt::Key_Up:
        delta = {0, -step};
        break;
    case Qt::Key_Down:
        delta = {0, step};
        break;
    default:
        QWidget::keyPressEvent(e);
        return;
    }

    if (m_activeHandle == Handle::P1) {
        m_p1 = clampNorm(m_p1 + delta);
        emit p1Changed(m_p1);
    } else {
        m_p2 = clampNorm(m_p2 + delta);
        emit p2Changed(m_p2);
    }
    update();
}
