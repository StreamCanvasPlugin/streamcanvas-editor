#include "GradientStopBar.h"
#include "ColorUtils.h"
#include "GradientHandlePainter.h"
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPaintEvent>
#include <algorithm>

static QGradientStops toQGradientStops(const QVector<GradientStop>& stops)
{
    QGradientStops r;
    r.reserve(stops.size());
    for (const auto& s : stops)
        r.append({s.position, s.color});
    return r;
}

GradientStopBar::GradientStopBar(QWidget* parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setToolTip(tr("Drag a handle to move a stop. Click the strip to add one. "
                  "Delete removes the selected stop, arrow keys nudge it."));
    setMinimumSize(120, sizeHint().height());
    setStops({{0.0, Qt::black}, {1.0, Qt::white}});
}

QVector<GradientStop> GradientStopBar::stops() const
{
    QVector<GradientStop> r;
    r.reserve(m_stops.size());
    for (const auto& s : m_stops)
        r.append(s.stop);
    return r;
}

void GradientStopBar::setStops(const QVector<GradientStop>& stops)
{
    QVector<GradientStop> src =
        stops.size() >= 2 ? stops : QVector<GradientStop>{{0.0, Qt::black}, {1.0, Qt::white}};

    int prevIdx = indexOfSelected();

    m_stops.clear();
    m_stops.reserve(src.size());
    for (const auto& s : src) {
        GradientStop clamped = s;
        clamped.position = qBound(0.0, s.position, 1.0);
        m_stops.append({m_nextId++, clamped});
    }
    sortByPosition();

    if (prevIdx >= 0 && prevIdx < m_stops.size())
        m_selectedId = m_stops[prevIdx].id;
    else if (!m_stops.isEmpty())
        m_selectedId = m_stops[0].id;
    else
        m_selectedId = -1;

    update();
}

int GradientStopBar::selectedStop() const
{
    return indexOfSelected();
}

void GradientStopBar::setSelectedStop(int index)
{
    if (index < 0 || index >= m_stops.size())
        m_selectedId = -1;
    else
        m_selectedId = m_stops[index].id;
    update();
}

void GradientStopBar::setSelectedStopColor(const QColor& c)
{
    int idx = indexOfSelected();
    if (idx < 0)
        return;
    m_stops[idx].stop.color = c;
    update();
}

void GradientStopBar::setSelectedStopPosition(qreal pos)
{
    int idx = indexOfSelected();
    if (idx < 0)
        return;
    m_stops[idx].stop.position = qBound(0.0, pos, 1.0);
    sortByPosition();
    update();
}

QSize GradientStopBar::sizeHint() const
{
    int h = kMarginY + kStripH + kGap + 2 * kHandleR + kMarginY;
    return {240, h};
}

QRect GradientStopBar::stripRect() const
{
    return {kMarginX, kMarginY, qMax(0, width() - 2 * kMarginX), kStripH};
}

QPointF GradientStopBar::handleCenter(const Stop& s) const
{
    QRect strip = stripRect();
    qreal x = strip.left() + qBound(0.0, s.stop.position, 1.0) * strip.width();
    qreal y = strip.bottom() + kGap + kHandleR + 1;
    return {x, y};
}

qreal GradientStopBar::xToPosition(qreal x) const
{
    QRect strip = stripRect();
    if (strip.width() <= 0)
        return 0.0;
    return qBound(0.0, (x - strip.left()) / qreal(strip.width()), 1.0);
}

int GradientStopBar::hitTestHandle(QPointF p) const
{
    // The selected stop is drawn last (on top), so it wins ties; check it first.
    int selIdx = indexOfSelected();
    if (selIdx >= 0 && GHP::hitCircle(p, handleCenter(m_stops[selIdx]), kHandleR))
        return m_stops[selIdx].id;
    for (int i = m_stops.size() - 1; i >= 0; --i) {
        if (i == selIdx)
            continue;
        if (GHP::hitCircle(p, handleCenter(m_stops[i]), kHandleR))
            return m_stops[i].id;
    }
    return -1;
}

int GradientStopBar::indexOfId(int id) const
{
    for (int i = 0; i < m_stops.size(); ++i)
        if (m_stops[i].id == id)
            return i;
    return -1;
}

int GradientStopBar::indexOfSelected() const
{
    return indexOfId(m_selectedId);
}

void GradientStopBar::selectByIndexInternal(int index, bool emitSignal)
{
    if (index < 0 || index >= m_stops.size()) {
        if (m_selectedId != -1) {
            m_selectedId = -1;
            if (emitSignal)
                emit stopSelected(-1);
        }
        return;
    }
    int newId = m_stops[index].id;
    if (newId != m_selectedId) {
        m_selectedId = newId;
        if (emitSignal)
            emit stopSelected(index);
    }
}

void GradientStopBar::sortByPosition()
{
    // Only reorders — never rewrites positions (no endpoint pinning).
    std::stable_sort(m_stops.begin(), m_stops.end(),
                     [](const Stop& a, const Stop& b) { return a.stop.position < b.stop.position; });
}

void GradientStopBar::addStopAt(qreal t)
{
    t = qBound(0.0, t, 1.0);
    QColor color = sampleGradient(toQGradientStops(stops()), t);
    Stop s{m_nextId++, GradientStop{t, color}};
    m_stops.append(s);
    sortByPosition();
    m_selectedId = s.id;
    emit stopSelected(indexOfId(s.id));
    emit stopsChanged(stops());
    update();
}

void GradientStopBar::drawCheckerboard(QPainter& p, const QRect& r) const
{
    p.save();
    p.setClipRect(r);
    QColor c1 = palette().color(QPalette::Base);
    QColor c2 = palette().color(QPalette::Mid);
    for (int row = 0; row * kCheckerCell < r.height(); ++row)
        for (int col = 0; col * kCheckerCell < r.width(); ++col) {
            const QColor& c = ((row + col) % 2 == 0) ? c1 : c2;
            p.fillRect(r.x() + col * kCheckerCell, r.y() + row * kCheckerCell, kCheckerCell,
                      kCheckerCell, c);
        }
    p.restore();
}

void GradientStopBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect strip = stripRect();
    drawCheckerboard(p, strip);

    if (m_stops.size() >= 2) {
        QLinearGradient grad(strip.left(), 0, strip.right(), 0);
        for (const auto& s : m_stops)
            grad.setColorAt(qBound(0.0, s.stop.position, 1.0), s.stop.color);
        p.fillRect(strip, grad);
    } else if (m_stops.size() == 1) {
        p.fillRect(strip, m_stops.first().stop.color);
    }

    p.setPen(QPen(palette().color(QPalette::Shadow), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(strip.adjusted(0, 0, -1, -1));

    int selIdx = indexOfSelected();
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < m_stops.size(); ++i) {
            bool selected = (i == selIdx);
            if (pass == 0 && selected)
                continue;
            if (pass == 1 && !selected)
                continue;

            QPointF c = handleCenter(m_stops[i]);
            if (!selected && m_stops[i].id == m_hoverId) {
                p.setPen(QPen(palette().color(QPalette::Highlight), 1.5));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(c, kHandleR + 3, kHandleR + 3);
            }
            GHP::paintCircleHandle(p, c, kHandleR, m_stops[i].stop.color, selected, palette());
        }
    }
}

void GradientStopBar::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(e);
        return;
    }
    setFocus(Qt::MouseFocusReason);

    QPointF pos = e->position();
    int hit = hitTestHandle(pos);
    if (hit >= 0) {
        // Select on press, but do NOT mutate the stop yet — dragging only
        // begins once the 4px threshold is crossed in mouseMoveEvent.
        selectByIndexInternal(indexOfId(hit), true);
        m_pressed = true;
        m_dragging = false;
        m_pressId = hit;
        m_pressPos = pos;
        update();
        return;
    }

    if (stripRect().contains(pos.toPoint()))
        addStopAt(xToPosition(pos.x()));
}

void GradientStopBar::mouseMoveEvent(QMouseEvent* e)
{
    QPointF pos = e->position();

    if (m_pressed && (e->buttons() & Qt::LeftButton)) {
        if (!m_dragging) {
            if (QLineF(m_pressPos, pos).length() > kDragThresholdPx) {
                m_dragging = true;
                setCursor(Qt::SplitHCursor);
                emit interactionStarted();
            }
        }
        if (m_dragging) {
            int idx = indexOfId(m_pressId);
            if (idx >= 0) {
                m_stops[idx].stop.position = xToPosition(pos.x());
                sortByPosition();
                emit stopsChanged(stops());
                update();
            }
        }
        return;
    }

    int hover = hitTestHandle(pos);
    if (hover != m_hoverId) {
        m_hoverId = hover;
        update();
    }
    if (hover >= 0)
        setCursor(Qt::PointingHandCursor);
    else if (stripRect().contains(pos.toPoint()))
        setCursor(Qt::CrossCursor);
    else
        unsetCursor();
}

void GradientStopBar::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(e);
        return;
    }

    if (m_dragging) {
        m_dragging = false;
        emit interactionFinished();
    }
    m_pressed = false;
    m_pressId = -1;

    QPointF pos = e->position();
    int hover = hitTestHandle(pos);
    m_hoverId = hover;
    if (hover >= 0)
        setCursor(Qt::PointingHandCursor);
    else if (stripRect().contains(pos.toPoint()))
        setCursor(Qt::CrossCursor);
    else
        unsetCursor();
    update();
}

void GradientStopBar::leaveEvent(QEvent* e)
{
    QWidget::leaveEvent(e);
    if (m_hoverId != -1) {
        m_hoverId = -1;
        update();
    }
    unsetCursor();
}

void GradientStopBar::keyPressEvent(QKeyEvent* e)
{
    if (m_stops.isEmpty()) {
        QWidget::keyPressEvent(e);
        return;
    }

    int idx = indexOfSelected();

    switch (e->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace: {
        if (idx < 0 || m_stops.size() <= 2)
            break;
        m_stops.removeAt(idx);
        selectByIndexInternal(qBound(0, idx, m_stops.size() - 1), true);
        emit stopsChanged(stops());
        update();
        break;
    }
    case Qt::Key_Left:
    case Qt::Key_Right: {
        if (idx < 0)
            break;
        qreal step = (e->modifiers() & Qt::ShiftModifier) ? 0.1 : 0.01;
        if (e->key() == Qt::Key_Left)
            step = -step;
        m_stops[idx].stop.position = qBound(0.0, m_stops[idx].stop.position + step, 1.0);
        sortByPosition();
        emit stopsChanged(stops());
        update();
        break;
    }
    case Qt::Key_Home:
        selectByIndexInternal(0, true);
        update();
        break;
    case Qt::Key_End:
        selectByIndexInternal(m_stops.size() - 1, true);
        update();
        break;
    case Qt::Key_BracketLeft:
        selectByIndexInternal(idx > 0 ? idx - 1 : m_stops.size() - 1, true);
        update();
        break;
    case Qt::Key_BracketRight:
        selectByIndexInternal(idx >= 0 && idx < m_stops.size() - 1 ? idx + 1 : 0, true);
        update();
        break;
    default:
        QWidget::keyPressEvent(e);
        return;
    }
}
