#include "AnimationTimelineEditor.h"

#include "model/TitleDocument.h"
#include "engine/title.h"
#include "engine/visual_element.h"
#include "engine/animation.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QScrollBar>
#include <QMenu>
#include <QFontMetrics>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kRulerH = 22;
constexpr int kGutterW = 140;
constexpr int kRowH = 26;
constexpr int kHandleZoneWidth = 14;
constexpr int kPadPx = 400;
constexpr double kMinPps = 2.0;
constexpr double kMaxPps = 2000.0;

QString animationTypeName(AnimationType t)
{
    switch (t) {
    case AnimationType::None:       return "None";
    case AnimationType::Fade:       return "Fade";
    case AnimationType::SlideUp:    return "Slide Up";
    case AnimationType::SlideDown:  return "Slide Down";
    case AnimationType::SlideLeft:  return "Slide Left";
    case AnimationType::SlideRight: return "Slide Right";
    case AnimationType::ScaleIn:    return "Scale In";
    case AnimationType::WipeUp:     return "Wipe Up";
    case AnimationType::WipeDown:   return "Wipe Down";
    case AnimationType::WipeLeft:   return "Wipe Left";
    case AnimationType::WipeRight:  return "Wipe Right";
    }
    return {};
}

// Cannibalized from AnimationTimingEditor.cpp's qUtilsDrawButton: rounded
// gradient bar with left/right "handle" tick marks and elided label text.
void qUtilsDrawButton(QPainter    *painter,
                      const QRect &rect,
                      const QColor  &base,
                      bool           sunken,
                      const QString &text,
                      int            radius)
{
    const QColor textColor = base.darker(300);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect r = rect.adjusted(1, 1, -1, -1);

    painter->setPen(QPen(base.darker(160), 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(r, radius + 1, radius + 1);

    QLinearGradient grad(r.topLeft(), r.bottomLeft());
    if (sunken) {
        grad.setColorAt(0.0, base.darker(115));
        grad.setColorAt(1.0, base.darker(105));
    } else {
        grad.setColorAt(0.0, base.lighter(120));
        grad.setColorAt(1.0, base.darker(115));
    }

    painter->setPen(QPen(base.darker(140), 1.0));
    painter->setBrush(grad);
    painter->drawRoundedRect(r, radius, radius);

    QRect textRect = r.adjusted(16, 2, -16, -2);
    if (!text.isEmpty() && textRect.width() > 0) {
        if (sunken) textRect.translate(0, 1);

        painter->save();
        painter->setClipRect(textRect);

        QFont font = painter->font();
        font.setBold(true);
        painter->setFont(font);

        QFontMetrics fm(font);
        QString elided = fm.elidedText(text, Qt::ElideRight, textRect.width());

        painter->setPen(textColor);
        painter->drawText(textRect, Qt::AlignLeft, elided);

        painter->restore();
    }

    int lx = rect.left();
    int rx = textRect.right() + 16;
    painter->setPen(textColor);
    painter->drawLine(lx + 5, textRect.y()+1, lx + 5, textRect.bottom()-2);
    painter->drawLine(lx + 8, textRect.y()+1, lx + 8, textRect.bottom()-2);
    painter->drawLine(rx - 5, textRect.y()+1, rx - 5, textRect.bottom()-2);
    painter->drawLine(rx - 8, textRect.y()+1, rx - 8, textRect.bottom()-2);

    painter->restore();
}

constexpr AnimationType kAllTypes[] = {
    AnimationType::None,
    AnimationType::Fade,
    AnimationType::SlideUp,   AnimationType::SlideDown,
    AnimationType::SlideLeft, AnimationType::SlideRight,
    AnimationType::ScaleIn,
    AnimationType::WipeUp,    AnimationType::WipeDown,
    AnimationType::WipeLeft,  AnimationType::WipeRight,
};

struct EasingEntry { Easing easing; const char* name; };
constexpr EasingEntry kAllEasings[] = {
    { Easing::Linear,          "Linear"                },
    { Easing::EaseIn,          "Ease In"               },
    { Easing::EaseOut,         "Ease Out"              },
    { Easing::EaseInOut,       "Ease In/Out"           },
    { Easing::EaseInCubic,     "Ease In (Cubic)"       },
    { Easing::EaseOutCubic,    "Ease Out (Cubic)"      },
    { Easing::EaseInOutCubic,  "Ease In/Out (Cubic)"   },
    { Easing::EaseInExpo,      "Ease In (Expo)"        },
    { Easing::EaseOutExpo,     "Ease Out (Expo)"       },
    { Easing::EaseInOutExpo,   "Ease In/Out (Expo)"    },
    { Easing::EaseInBack,      "Ease In (Back)"        },
    { Easing::EaseOutBack,     "Ease Out (Back)"       },
    { Easing::EaseInOutBack,   "Ease In/Out (Back)"    },
    { Easing::EaseInElastic,   "Ease In (Elastic)"     },
    { Easing::EaseOutElastic,  "Ease Out (Elastic)"    },
    { Easing::EaseInOutElastic,"Ease In/Out (Elastic)" },
};

// Cannibalized from ScrubRuler.cpp: adaptive tick step so major ticks (every
// 5th minor) land ~50px apart.
constexpr float kTickSteps[] = {0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f};

} // namespace

AnimationTimelineEditor::AnimationTimelineEditor(TitleDocument* doc, QWidget* parent)
    : QAbstractScrollArea(parent)
    , m_doc(doc)
{
    viewport()->setMouseTracking(true);
    setFrameStyle(QFrame::NoFrame);
    setMinimumHeight(150);
    horizontalScrollBar()->setSingleStep(20);
    verticalScrollBar()->setSingleStep(kRowH);
    rebuild();
}

void AnimationTimelineEditor::setSlot(Slot s)
{
    if (m_slot == s)
        return;
    m_slot = s;
    rebuild();
}

void AnimationTimelineEditor::rebuild()
{
    // Remember the active row's element so it can be re-resolved by identity
    // after the rows are rebuilt (rows are position-indexed, not id-stable).
    int activeElementIndex = -1;
    if (m_activeRow >= 0 && m_activeRow < int(m_rows.size()))
        activeElementIndex = m_rows[m_activeRow].elementIndex;

    m_rows.clear();

    if (m_doc) {
        const Title& t = m_doc->title();
        for (size_t i = 1; i < t.elements.size(); ++i) {
            const auto* ve = dynamic_cast<const VisualElement*>(t.elements[i].get());
            if (!ve)
                continue;

            AnimationDef def;
            switch (m_slot) {
            case Slot::In:      def = ve->inAnimation;     break;
            case Slot::Out:     def = ve->outAnimation;    break;
            case Slot::DataIn:  def = ve->dataInAnimation; break;
            case Slot::DataOut: def = ve->dataOutAnimation;break;
            }
            m_rows.push_back(Row{int(i), ve->GetId(), def});
        }
    }

    // Re-resolve the active row by element index (may have shifted or vanished).
    m_activeRow = -1;
    if (activeElementIndex >= 0) {
        for (int i = 0; i < int(m_rows.size()); ++i) {
            if (m_rows[i].elementIndex == activeElementIndex) {
                m_activeRow = i;
                break;
            }
        }
    }

    updateScrollRanges();
    viewport()->update();
}

void AnimationTimelineEditor::updateScrollRanges()
{
    double contentEndSec = 0.0;
    for (const auto& r : m_rows)
        contentEndSec = std::max(contentEndSec, double(r.def.delay + r.def.duration));

    const double viewportSec = viewport()->width() / m_pps;
    const double maxSec = std::max({contentEndSec, m_playheadTime, viewportSec});

    horizontalScrollBar()->setRange(0, int(maxSec * m_pps) + kPadPx);
    horizontalScrollBar()->setPageStep(viewport()->width());
    horizontalScrollBar()->setSingleStep(int(m_pps / 10.0) + 1);

    // The ruler band overlays the top kRulerH of the row area, so the content
    // extent must include it for the last rows to be scrollable into view.
    verticalScrollBar()->setRange(0, std::max(0, int(m_rows.size()) * kRowH + kRulerH - viewport()->height()));
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setSingleStep(kRowH);
}

int AnimationTimelineEditor::timeToX(double t) const
{
    return kGutterW + int(t * m_pps) - horizontalScrollBar()->value();
}

double AnimationTimelineEditor::xToTime(int x) const
{
    return (x - kGutterW + horizontalScrollBar()->value()) / m_pps;
}

int AnimationTimelineEditor::rowTopY(int row) const
{
    return kRulerH + row * kRowH - verticalScrollBar()->value();
}

int AnimationTimelineEditor::yToRow(int y) const
{
    int r = (y - kRulerH + verticalScrollBar()->value()) / kRowH;
    return r;
}

void AnimationTimelineEditor::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollRanges();
}

void AnimationTimelineEditor::scrollContentsBy(int dx, int dy)
{
    Q_UNUSED(dx);
    Q_UNUSED(dy);
    viewport()->update();
}

void AnimationTimelineEditor::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, true);

    const int vw = viewport()->width();
    const int vh = viewport()->height();

    p.fillRect(viewport()->rect(), QColor(30, 30, 30));

    // --- 2. clip rows (culled, clipped to x >= kGutterW) ---
    p.save();
    p.setClipRect(kGutterW, 0, vw - kGutterW, vh);
    for (int r = 0; r < int(m_rows.size()); ++r) {
        const int y = rowTopY(r);
        if (y + kRowH < kRulerH || y > vh)
            continue;

        const Row& row = m_rows[r];

        QColor rowBg = (r % 2 == 0) ? QColor(38, 38, 38) : QColor(34, 34, 34);
        if (r == m_activeRow)
            rowBg = QColor(52, 60, 70);
        p.fillRect(QRect(kGutterW, y, vw - kGutterW, kRowH), rowBg);
        if (r == m_activeRow) {
            p.setPen(QPen(QColor(90, 160, 230), 1));
            p.drawRect(QRect(kGutterW, y, vw - kGutterW - 1, kRowH - 1));
        }

        if (row.def.type == AnimationType::None) {
            const int x0 = timeToX(0.0);
            const int x1 = timeToX(0.5);
            QRect ph(x0, y + 3, std::max(4, x1 - x0), kRowH - 6);
            p.save();
            QPen pen(QColor(90, 90, 90));
            pen.setStyle(Qt::DashLine);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawRect(ph);
            p.restore();
        } else {
            const int x0 = timeToX(row.def.delay);
            const int x1 = timeToX(row.def.delay + row.def.duration);
            QRect br(x0, y + 3, x1 - x0, kRowH - 6);
            qUtilsDrawButton(&p, br, QColor(241, 162, 24), false,
                              animationTypeName(row.def.type), 3);
        }
    }
    p.restore();

    // --- 3. name gutter overlay ---
    p.save();
    p.setClipRect(0, kRulerH, kGutterW, vh);
    p.fillRect(QRect(0, kRulerH, kGutterW, vh), QColor(26, 26, 26));
    for (int r = 0; r < int(m_rows.size()); ++r) {
        const int y = rowTopY(r);
        if (y + kRowH < kRulerH || y > vh)
            continue;

        QColor rowBg = (r % 2 == 0) ? QColor(38, 38, 38) : QColor(34, 34, 34);
        if (r == m_activeRow)
            rowBg = QColor(52, 60, 70);
        p.fillRect(QRect(0, y, kGutterW, kRowH), rowBg);

        p.setPen(QColor(220, 220, 220));
        QRect labelRect(4, y, kGutterW - 10, kRowH);
        const QString elided = QFontMetrics(p.font()).elidedText(
            QString::fromStdString(m_rows[r].id), Qt::ElideLeft, labelRect.width());
        p.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, elided);
    }
    p.restore();

    // --- 4. ruler band overlay ---
    p.save();
    p.setClipRect(kGutterW, 0, vw - kGutterW, kRulerH);
    p.fillRect(QRect(kGutterW, 0, vw - kGutterW, kRulerH), QColor(22, 22, 22));

    float minorStep = 10.0f;
    for (float s : kTickSteps) {
        if (m_pps * s * 5.0 >= 50.0) { minorStep = s; break; }
    }

    const double startTime = xToTime(kGutterW);
    const double endTime = xToTime(vw);
    int kStart = int(std::floor(startTime / minorStep)) - 1;
    int kEnd = int(std::ceil(endTime / minorStep)) + 1;
    kStart = std::max(0, kStart);

    for (int k = kStart; k <= kEnd; ++k) {
        const float t = k * minorStep;
        const int x = timeToX(t);
        if (x < kGutterW || x > vw)
            continue;

        const bool isMajor = (k % 5 == 0);
        if (isMajor) {
            p.setPen(QColor(130, 130, 130));
            p.drawLine(x, kRulerH - 9, x, kRulerH);
            QFont f = p.font();
            f.setPointSize(7);
            p.setFont(f);
            p.drawText(QRect(x + 2, 0, 40, kRulerH - 7),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QString("%1s").arg(double(t), 0, 'f', t < 1.0f ? 2 : 1));
        } else {
            p.setPen(QColor(65, 65, 65));
            p.drawLine(x, kRulerH - 4, x, kRulerH);
        }
    }
    p.restore();

    // --- 5. top-left corner ---
    p.fillRect(QRect(0, 0, kGutterW, kRulerH), QColor(26, 26, 26));

    // --- 6. playhead ---
    const int cx = timeToX(m_playheadTime);
    if (cx >= kGutterW) {
        p.save();
        p.setClipRect(kGutterW, 0, vw - kGutterW, vh);
        p.setPen(QPen(QColor(255, 200, 50), 2));
        p.drawLine(cx, 0, cx, vh);
        p.restore();

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 200, 50));
        const QPoint tri[3] = { {cx - 6, 0}, {cx + 6, 0}, {cx, 10} };
        p.drawPolygon(tri, 3);
    }
}

AnimationTimelineEditor::HitResult AnimationTimelineEditor::hitTest(const QPoint& pos) const
{
    HitResult res;

    if (pos.y() < kRulerH) {
        res.zone = HitZone::Ruler;
        return res;
    }

    if (pos.x() < kGutterW) {
        res.zone = HitZone::Gutter;
        const int r = yToRow(pos.y());
        if (r >= 0 && r < int(m_rows.size()))
            res.row = r;
        return res;
    }

    const int r = yToRow(pos.y());
    if (r < 0 || r >= int(m_rows.size())) {
        res.zone = HitZone::Empty;
        return res;
    }

    res.zone = HitZone::Row;
    res.row = r;

    const Row& row = m_rows[r];
    if (row.def.type == AnimationType::None) {
        res.dragZone = DragZone::None;
        return res;
    }

    const int x0 = timeToX(row.def.delay);
    const int x1 = timeToX(row.def.delay + row.def.duration);
    const QRect br(x0, rowTopY(r) + 3, x1 - x0, kRowH - 6);

    if (!br.contains(pos)) {
        res.dragZone = DragZone::None;
        return res;
    }

    if (pos.x() <= br.left() + kHandleZoneWidth)
        res.dragZone = DragZone::LeftHandle;
    else if (pos.x() >= br.right() - kHandleZoneWidth)
        res.dragZone = DragZone::RightHandle;
    else
        res.dragZone = DragZone::Body;

    return res;
}

void AnimationTimelineEditor::updateCursorForPos(const QPoint& pos)
{
    const HitResult hit = hitTest(pos);
    if (hit.zone == HitZone::Ruler) {
        viewport()->setCursor(Qt::SizeHorCursor);
        return;
    }
    switch (hit.dragZone) {
    case DragZone::LeftHandle:
    case DragZone::RightHandle:
        viewport()->setCursor(Qt::SizeHorCursor);
        break;
    case DragZone::Body:
        viewport()->setCursor(Qt::SizeAllCursor);
        break;
    default:
        viewport()->unsetCursor();
        break;
    }
}

void AnimationTimelineEditor::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const HitResult hit = hitTest(event->pos());

    if (hit.zone == HitZone::Ruler) {
        m_draggingPlayhead = true;
        m_playheadTime = std::max(0.0, xToTime(event->pos().x()));
        viewport()->update();
        event->accept();
        return;
    }

    if ((hit.zone == HitZone::Gutter || hit.zone == HitZone::Row) && hit.row >= 0) {
        m_activeRow = hit.row;
        emit rowClicked(m_rows[hit.row].elementIndex);
        emit activeClipChanged(true, m_rows[hit.row].def);
        viewport()->update();

        if (hit.zone == HitZone::Row && hit.dragZone != DragZone::None) {
            m_dragZone = hit.dragZone;
            m_dragRow = hit.row;
            m_dragStartX = event->pos().x();
            m_dragStartDelay = m_rows[hit.row].def.delay;
            m_dragStartDuration = m_rows[hit.row].def.duration;
            m_dragDef = m_rows[hit.row].def;
        }
    }

    event->accept();
}

void AnimationTimelineEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (m_draggingPlayhead) {
        m_playheadTime = std::max(0.0, xToTime(event->pos().x()));
        viewport()->update();
        event->accept();
        return;
    }

    if (m_dragZone != DragZone::None) {
        const float deltaSec = float(event->pos().x() - m_dragStartX) / float(m_pps);
        const float minDur = float(2 * kHandleZoneWidth) / float(m_pps);
        auto snap = [](float v) { return std::round(v * 100.0f) / 100.0f; };

        switch (m_dragZone) {
        case DragZone::LeftHandle: {
            const float maxDelay = m_dragStartDelay + m_dragStartDuration - minDur;
            float delay = std::max(0.0f, std::min(m_dragStartDelay + deltaSec, maxDelay));
            delay = snap(delay);
            m_dragDef.delay = delay;
            m_dragDef.duration = (m_dragStartDelay + m_dragStartDuration) - delay;
            break;
        }
        case DragZone::Body: {
            m_dragDef.delay = snap(std::max(0.0f, m_dragStartDelay + deltaSec));
            break;
        }
        case DragZone::RightHandle: {
            m_dragDef.duration = snap(std::max(minDur, m_dragStartDuration + deltaSec));
            break;
        }
        default:
            break;
        }

        viewport()->update();
        event->accept();
        return;
    }

    updateCursorForPos(event->pos());
    event->accept();
}

void AnimationTimelineEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    if (m_draggingPlayhead) {
        m_draggingPlayhead = false;
        event->accept();
        return;
    }

    if (m_dragZone != DragZone::None && m_dragRow >= 0 && m_dragRow < int(m_rows.size())) {
        const int elementIndex = m_rows[m_dragRow].elementIndex;
        m_rows[m_dragRow].def = m_dragDef;
        m_dragZone = DragZone::None;
        m_dragRow = -1;

        emit clipEdited(elementIndex, m_slot, m_dragDef);
        emit activeClipChanged(true, m_dragDef);

        viewport()->update();
        updateCursorForPos(event->pos());
        event->accept();
        return;
    }

    m_dragZone = DragZone::None;
    m_dragRow = -1;
    event->accept();
}

void AnimationTimelineEditor::wheelEvent(QWheelEvent* event)
{
    const QPoint pos = event->position().toPoint();

    if (event->modifiers() & Qt::ControlModifier) {
        const double tCursor = xToTime(pos.x());
        const double factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        m_pps = std::clamp(m_pps * factor, kMinPps, kMaxPps);
        updateScrollRanges();
        horizontalScrollBar()->setValue(int(tCursor * m_pps) - (pos.x() - kGutterW));
        viewport()->update();
        event->accept();
        return;
    }

    if (event->modifiers() & Qt::ShiftModifier) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - event->angleDelta().y());
        event->accept();
        return;
    }

    verticalScrollBar()->setValue(verticalScrollBar()->value() - event->angleDelta().y());
    event->accept();
}

void AnimationTimelineEditor::contextMenuEvent(QContextMenuEvent* event)
{
    const HitResult hit = hitTest(event->pos());
    if (hit.zone != HitZone::Row || hit.row < 0)
        return;

    const int row = hit.row;
    const int elementIndex = m_rows[row].elementIndex;

    m_activeRow = row;
    emit activeClipChanged(true, m_rows[row].def);
    viewport()->update();

    QMenu menu(this);

    QMenu* typeMenu = menu.addMenu("Set Animation Type");
    for (AnimationType t : kAllTypes) {
        QAction* a = typeMenu->addAction(animationTypeName(t));
        a->setCheckable(true);
        a->setChecked(t == m_rows[row].def.type);
        connect(a, &QAction::triggered, this, [this, row, elementIndex, t] {
            if (row < 0 || row >= int(m_rows.size()))
                return;
            AnimationDef def = m_rows[row].def;
            def.type = t;
            m_rows[row].def = def;
            emit clipEdited(elementIndex, m_slot, def);
            emit activeClipChanged(true, def);
            viewport()->update();
        });
    }

    QMenu* easingMenu = menu.addMenu("Set Easing");
    for (const auto& e : kAllEasings) {
        QAction* a = easingMenu->addAction(e.name);
        a->setCheckable(true);
        a->setChecked(e.easing == m_rows[row].def.easing);
        connect(a, &QAction::triggered, this, [this, row, elementIndex, e] {
            if (row < 0 || row >= int(m_rows.size()))
                return;
            AnimationDef def = m_rows[row].def;
            def.easing = e.easing;
            m_rows[row].def = def;
            emit clipEdited(elementIndex, m_slot, def);
            emit activeClipChanged(true, def);
            viewport()->update();
        });
    }

    menu.exec(event->globalPos());
    event->accept();
}

void AnimationTimelineEditor::focusElement(int elementIndex)
{
    for (int i = 0; i < int(m_rows.size()); ++i) {
        if (m_rows[i].elementIndex == elementIndex) {
            m_activeRow = i;
            scrollRowIntoView(i);
            emit activeClipChanged(true, m_rows[i].def);
            viewport()->update();
            return;
        }
    }
    clearActive();
}

void AnimationTimelineEditor::clearActive()
{
    m_activeRow = -1;
    emit activeClipChanged(false, AnimationDef{});
    viewport()->update();
}

void AnimationTimelineEditor::scrollRowIntoView(int row)
{
    // Visible row window is [kRulerH, viewH] because the ruler overlays the top.
    const int viewH = viewport()->height();
    const int topNeeded = row * kRowH;                          // scroll to put row flush under the ruler
    const int bottomNeeded = (row + 1) * kRowH + kRulerH - viewH; // scroll to reveal the row's bottom
    int v = verticalScrollBar()->value();
    if (v > topNeeded) {
        v = topNeeded;
    } else if (v < bottomNeeded) {
        v = bottomNeeded;
    }
    verticalScrollBar()->setValue(std::max(0, v));
}
