#include "RadialGradientEditor.h"
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

RadialGradientEditor::RadialGradientEditor(QWidget* parent) : GradientGeometryEditor(parent)
{
    setMinimumSize(120, 100);
}

void RadialGradientEditor::setCenter(QPointF c)
{
    m_center = c;
    update();
}

void RadialGradientEditor::setRadius(qreal r)
{
    m_radius = qMax(kMinRadius, r);
    update();
}

void RadialGradientEditor::setFocalPoint(double fx, double fy, double fr)
{
    m_focusX = fx;
    m_focusY = fy;
    m_focusR = fr;
}

QPointF RadialGradientEditor::stopPixelPos(int i) const
{
    QPointF norm = {m_center.x() + m_stops[i].position * m_radius, m_center.y()};
    return toPixel(norm);
}

RadialGradientEditor::HitResult RadialGradientEditor::hitTest(QPointF px) const
{
    QPointF centerPx = toPixel(m_center);
    QPointF radiusPx = toPixel(radiusHandleNorm());
    if (GHP::hitCircle(px, radiusPx, kEndpointR))
        return {Handle::Radius, -1};
    if (GHP::hitCircle(px, centerPx, kEndpointR))
        return {Handle::Center, -1};
    int stopIdx = hitTestStopMarker(px, [this](int i) { return stopPixelPos(i); });
    if (stopIdx >= 0)
        return {Handle::Stop, stopIdx};
    return {Handle::None, -1};
}

void RadialGradientEditor::updateHover(QPointF pos)
{
    HitResult hit = hitTest(pos);
    bool changed = hit.type != m_hoverHandle;
    m_hoverHandle = hit.type;
    if (changed)
        update();

    switch (hit.type) {
    case Handle::Center:
        setCursor(Qt::OpenHandCursor);
        setToolTip(tr("Gradient centre — drag to move"));
        break;
    case Handle::Radius:
        setCursor(Qt::SizeHorCursor);
        setToolTip(tr("Gradient radius — drag horizontally to resize"));
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

void RadialGradientEditor::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    drawBackground(p);

    QRectF preview = previewRect();
    QPointF centerPx = toPixel(m_center);
    QPointF radiusPx = toPixel(radiusHandleNorm());
    // Width-normalized radius, scaled by the PREVIEW's width — which represents the
    // target element's width — so the drawn circle matches params[5] * elementWidth.
    qreal pixelR = m_radius * preview.width();

    if (preview.width() > 0 && preview.height() > 0) {
        p.save();
        p.setClipRect(preview);
        if (m_stops.size() >= 2) {
            QRadialGradient grad(centerPx, pixelR, centerPx);
            for (const auto& s : m_stops)
                grad.setColorAt(qBound(0.0, s.position, 1.0), s.color);
            p.fillRect(preview, grad);
        }
        p.setPen(QPen(QColor(220, 220, 220, 160), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(centerPx, pixelR, pixelR);
        p.restore();
    }

    p.setPen(QPen(QColor(0, 0, 0, 80), 1.2));
    p.drawLine(centerPx, radiusPx);

    drawStopMarkers(p, [this](int i) { return stopPixelPos(i); });

    QGradientStops qstops = toQGradientStops(m_stops);
    QColor centerColor = m_stops.isEmpty() ? Qt::black : sampleGradient(qstops, 0.0);
    QColor edgeColor = m_stops.isEmpty() ? Qt::white : sampleGradient(qstops, 1.0);
    GHP::paintCircleHandle(p, centerPx, kEndpointR, centerColor,
                           m_hoverHandle == Handle::Center || m_pressHandle == Handle::Center,
                           palette());
    GHP::paintCircleHandle(p, radiusPx, kEndpointR, edgeColor,
                           m_hoverHandle == Handle::Radius || m_pressHandle == Handle::Radius,
                           palette());
}

void RadialGradientEditor::mousePressEvent(QMouseEvent* e)
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
    if (hit.type == Handle::Center || hit.type == Handle::Radius) {
        m_pressHandle = hit.type;
        m_activeHandle = hit.type;
        beginPress(pos);
        update();
        return;
    }
    m_pressHandle = Handle::None;
}

void RadialGradientEditor::mouseMoveEvent(QMouseEvent* e)
{
    QPointF pos = e->position();

    if ((e->buttons() & Qt::LeftButton) && m_pressHandle != Handle::None) {
        crossedDragThreshold(pos);
        if (isDragging()) {
            if (m_pressHandle == Handle::Center) {
                m_center = clampNorm(toNorm(pos));
                emit centerChanged(m_center);
            } else if (m_pressHandle == Handle::Radius) {
                qreal nx = toNorm(pos).x();
                m_radius = qMax(kMinRadius, qAbs(nx - m_center.x()));
                emit radiusChanged(m_radius);
            }
            update();
        }
        return;
    }

    updateHover(pos);
}

void RadialGradientEditor::mouseReleaseEvent(QMouseEvent* e)
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

void RadialGradientEditor::mouseDoubleClickEvent(QMouseEvent* e)
{
    HitResult hit = hitTest(e->position());
    if (hit.type == Handle::Stop)
        selectStopAndEmit(hit.stopIndex);
}

void RadialGradientEditor::leaveEvent(QEvent* e)
{
    QWidget::leaveEvent(e);
    m_hoverHandle = Handle::None;
    unsetCursor();
    setToolTip(QString());
    update();
}

void RadialGradientEditor::keyPressEvent(QKeyEvent* e)
{
    qreal step = (e->modifiers() & Qt::ShiftModifier) ? 0.02 : 0.005;

    if (m_activeHandle == Handle::Radius) {
        if (e->key() == Qt::Key_Left) {
            m_radius = qMax(kMinRadius, m_radius - step);
            emit radiusChanged(m_radius);
            update();
            return;
        }
        if (e->key() == Qt::Key_Right) {
            m_radius = qMax(kMinRadius, m_radius + step);
            emit radiusChanged(m_radius);
            update();
            return;
        }
        QWidget::keyPressEvent(e);
        return;
    }

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
    m_center = clampNorm(m_center + delta);
    emit centerChanged(m_center);
    update();
}
