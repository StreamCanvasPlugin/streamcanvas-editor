#pragma once
#include <QRectF>
#include <QPointF>
#include <vector>
#include <algorithm>

namespace alignmath {

enum class AlignMode { Left, HCenter, Right, Top, VMiddle, Bottom };

// boxes: each element's axis-aligned bounding box in one common (title) space.
// Returns the per-element translation delta that aligns them to the group's
// union bounding box. (QRectF::right()==left()+width(), bottom()==top()+height() —
// no QRect off-by-one.)
inline std::vector<QPointF> alignDeltas(const std::vector<QRectF>& boxes, AlignMode mode)
{
    std::vector<QPointF> deltas(boxes.size(), QPointF(0, 0));
    if (boxes.empty()) return deltas;
    QRectF g = boxes[0];
    for (const auto& b : boxes) g = g.united(b);
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const QRectF& b = boxes[i];
        double dx = 0, dy = 0;
        switch (mode) {
        case AlignMode::Left:    dx = g.left()       - b.left();       break;
        case AlignMode::HCenter: dx = g.center().x() - b.center().x(); break;
        case AlignMode::Right:   dx = g.right()      - b.right();      break;
        case AlignMode::Top:     dy = g.top()        - b.top();        break;
        case AlignMode::VMiddle: dy = g.center().y() - b.center().y(); break;
        case AlignMode::Bottom:  dy = g.bottom()     - b.bottom();     break;
        }
        deltas[i] = QPointF(dx, dy);
    }
    return deltas;
}

// Distribute the interior elements so all centers are evenly spaced between the
// two extreme elements along one axis. horizontal ⇒ X centers, else Y centers.
// Needs >=3 boxes; the two extremes stay put; returns zero deltas otherwise.
inline std::vector<QPointF> distributeDeltas(const std::vector<QRectF>& boxes, bool horizontal)
{
    const std::size_t n = boxes.size();
    std::vector<QPointF> deltas(n, QPointF(0, 0));
    if (n < 3) return deltas;
    auto center = [&](std::size_t i) {
        return horizontal ? boxes[i].center().x() : boxes[i].center().y();
    };
    std::vector<std::size_t> order(n);
    for (std::size_t i = 0; i < n; ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t a, std::size_t b) { return center(a) < center(b); });
    const double first = center(order.front());
    const double last  = center(order.back());
    const double step  = (last - first) / (double)(n - 1);
    for (std::size_t k = 1; k + 1 < n; ++k) {
        const std::size_t idx = order[k];
        const double target = first + step * (double)k;
        const double d = target - center(idx);
        deltas[idx] = horizontal ? QPointF(d, 0) : QPointF(0, d);
    }
    return deltas;
}

} // namespace alignmath
