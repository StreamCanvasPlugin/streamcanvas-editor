#include "GradientSlider.h"
#include "ColorUtils.h"
#include <QPainter>
#include <QStyleOptionSlider>
#include <QWheelEvent>

GradientSlider::GradientSlider(QWidget* parent) : QSlider(Qt::Horizontal, parent)
{
    setMinimumWidth(24);
    setMinimumHeight(24);
    setStops({{0.0, Qt::transparent}, {1.0, Qt::black}});
}

void GradientSlider::setStops(const QGradientStops& stops)
{
    m_stops = stops;
    update();
}

void GradientSlider::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const int kHandleR = 8;
    const int hPad = kHandleR + 2;

    // Handle rect from the style, so the drawn knob lines up with what
    // QStyle::SC_SliderHandle hit-tests for mouse presses.
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect styleHandleRect =
        style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
    const QPoint handleCenter = styleHandleRect.center();

    const bool horiz = (orientation() == Qt::Horizontal);
    QRect groove = horiz ? QRect(hPad, 1, width() - 2 * hPad, height() - 2)
                         : QRect(1, hPad, width() - 2, height() - 2 * hPad);

    // Checkerboard for transparency indication
    p.save();
    p.setClipRegion(groove);
    const int cell = 4;
    for (int row = 0; row * cell < groove.height(); ++row) {
        for (int col = 0; col * cell < groove.width(); ++col) {
            QColor c = ((row + col) % 2 == 0) ? QColor(180, 180, 180) : Qt::white;
            p.fillRect(groove.x() + col * cell, groove.y() + row * cell, cell, cell, c);
        }
    }
    p.restore();

    QLinearGradient grad = horiz ? QLinearGradient(groove.topLeft(), groove.topRight())
                                 : QLinearGradient(groove.bottomLeft(), groove.topLeft());
    for (const auto& stop : m_stops)
        grad.setColorAt(stop.first, stop.second);
    p.fillRect(groove, grad);

    p.setPen(palette().dark().color());
    p.drawRect(groove);

    const int range = maximum() - minimum();
    const qreal t = (range != 0) ? (qreal)(value() - minimum()) / range : 0.0;

    p.save();
    p.setRenderHint(QPainter::RenderHint::Antialiasing);

    p.save();
    p.setBrush(QBrush(sampleGradient(m_stops, t)));
    p.drawEllipse(handleCenter, kHandleR, kHandleR);
    p.restore();

    p.setPen(QPen(QColor(0, 0, 0, 80), 5));
    p.drawEllipse(handleCenter, kHandleR, kHandleR);
    p.setPen(QPen(Qt::white, 2));
    p.drawEllipse(handleCenter, kHandleR, kHandleR);
    p.restore();
}

void GradientSlider::wheelEvent(QWheelEvent* e)
{
    // Don't steal scroll events from a panel this slider happens to sit in
    // unless it actually has keyboard focus.
    if (!hasFocus()) {
        e->ignore();
        return;
    }
    QSlider::wheelEvent(e);
}
