#include "GradientGeometryEditor.h"
#include "GradientHandlePainter.h"
#include <QLineF>
#include <QPainter>
#include <QResizeEvent>

GradientGeometryEditor::GradientGeometryEditor(QWidget* parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    m_stops = {{0.0, Qt::black}, {1.0, Qt::white}};
}

void GradientGeometryEditor::setTargetSize(double w, double h)
{
    // Joint fallback, not per-axis: setTargetSize(-5, 100) must not yield a
    // 1:100 letterbox. Either dimension being unusable means the aspect ratio
    // is unknown, so fall back to 1:1.
    const bool ok = w > 0 && h > 0;
    m_targetW = ok ? w : 1.0;
    m_targetH = ok ? h : 1.0;
    update();
}

void GradientGeometryEditor::setStops(const QVector<GradientStop>& stops)
{
    m_stops = stops.size() >= 2 ? stops : QVector<GradientStop>{{0.0, Qt::black}, {1.0, Qt::white}};
    m_selected = qBound(0, m_selected, m_stops.size() - 1);
    update();
}

void GradientGeometryEditor::setSelectedStop(int index)
{
    m_selected = qBound(0, index, m_stops.size() - 1);
    update();
}

QSize GradientGeometryEditor::sizeHint() const
{
    return {300, 200};
}

void GradientGeometryEditor::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    update();
}

QRectF GradientGeometryEditor::previewRect() const
{
    qreal w = width(), h = height();
    if (w <= 0 || h <= 0)
        return QRectF(0, 0, qMax(w, 0.0), qMax(h, 0.0));

    qreal aspect = (m_targetW > 0 && m_targetH > 0) ? (m_targetW / m_targetH) : 1.0;
    qreal rw = w, rh = w / aspect;
    if (rh > h) {
        rh = h;
        rw = h * aspect;
    }
    return QRectF((w - rw) / 2.0, (h - rh) / 2.0, rw, rh);
}

QPointF GradientGeometryEditor::toPixel(QPointF norm) const
{
    QRectF r = previewRect();
    return {r.left() + norm.x() * r.width(), r.top() + norm.y() * r.height()};
}

QPointF GradientGeometryEditor::toNorm(QPointF px) const
{
    QRectF r = previewRect();
    if (r.width() <= 0 || r.height() <= 0)
        return {0.0, 0.0};
    return {(px.x() - r.left()) / r.width(), (px.y() - r.top()) / r.height()};
}

void GradientGeometryEditor::drawBackground(QPainter& p) const
{
    p.fillRect(rect(), palette().color(QPalette::Window));

    QRectF r = previewRect();
    if (r.width() <= 0 || r.height() <= 0)
        return;

    p.save();
    p.setClipRect(r);
    QColor c1 = palette().color(QPalette::Base);
    QColor c2 = palette().color(QPalette::Mid);
    QRect ri = r.toAlignedRect();
    for (int row = 0; row * kCheckerCell < ri.height(); ++row)
        for (int col = 0; col * kCheckerCell < ri.width(); ++col) {
            const QColor& c = ((row + col) % 2 == 0) ? c1 : c2;
            p.fillRect(ri.x() + col * kCheckerCell, ri.y() + row * kCheckerCell, kCheckerCell,
                       kCheckerCell, c);
        }
    p.restore();

    p.setPen(QPen(palette().color(QPalette::Shadow), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r.adjusted(0, 0, -1, -1));
}

void GradientGeometryEditor::drawStopMarkers(QPainter& p,
                                             const std::function<QPointF(int)>& stopPixelPos) const
{
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < m_stops.size(); ++i) {
            bool sel = (i == m_selected);
            if (pass == 0 && sel)
                continue;
            if (pass == 1 && !sel)
                continue;
            GHP::paintDiamondHandle(p, stopPixelPos(i), kMarkerD, m_stops[i].color, sel, palette());
        }
    }
}

int GradientGeometryEditor::hitTestStopMarker(QPointF px,
                                              const std::function<QPointF(int)>& stopPixelPos) const
{
    // The selected stop is drawn last (on top), so it wins ties; check it first.
    if (m_selected >= 0 && m_selected < m_stops.size() &&
        GHP::hitDiamond(px, stopPixelPos(m_selected), kMarkerD))
        return m_selected;
    for (int i = m_stops.size() - 1; i >= 0; --i) {
        if (i == m_selected)
            continue;
        if (GHP::hitDiamond(px, stopPixelPos(i), kMarkerD))
            return i;
    }
    return -1;
}

void GradientGeometryEditor::selectStopAndEmit(int index)
{
    index = qBound(0, index, m_stops.size() - 1);
    if (index == m_selected)
        return;
    m_selected = index;
    emit stopSelected(m_selected);
    update();
}

void GradientGeometryEditor::beginPress(QPointF pos)
{
    m_pressActive = true;
    m_dragStarted = false;
    m_pressPos = pos;
}

void GradientGeometryEditor::crossedDragThreshold(QPointF pos)
{
    if (!m_pressActive || m_dragStarted)
        return;
    if (QLineF(m_pressPos, pos).length() > kDragThresholdPx) {
        m_dragStarted = true;
        emit interactionStarted();
    }
}

void GradientGeometryEditor::endPress()
{
    if (m_dragStarted)
        emit interactionFinished();
    m_pressActive = false;
    m_dragStarted = false;
}

void GradientGeometryEditor::cancelPress()
{
    m_pressActive = false;
    m_dragStarted = false;
}
