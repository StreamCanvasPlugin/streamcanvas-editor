#pragma once
#include "GradientGeometryEditor.h"

// Radial gradient centre/radius editor.
//
// The radius handle only moves HORIZONTALLY. Cairo's radial gradient normalizes BOTH
// circles' radii by the element's WIDTH ONLY (see engine/types.hpp, `params[5] * w`),
// so a radius dragged vertically would mean something different from one dragged
// horizontally unless the element happens to be square. Constraining to horizontal
// drag keeps radius() always meaning "fraction of target element width", matching the
// engine exactly.
//
// The focal circle (Cairo's first radial-gradient circle, params[0..2]) is not
// authored here — there is no UI for it — but it must round-trip unchanged from
// whatever was loaded. See setFocalPoint()/focusX()/focusY()/focusR().
//
// Stop VALUES (add/remove/move/colour) are owned by GradientStopBar. This widget only
// renders stop markers along the centre-radius line and reports clicks via
// stopSelected().
class RadialGradientEditor : public GradientGeometryEditor {
    Q_OBJECT
public:
    explicit RadialGradientEditor(QWidget* parent = nullptr);

    QPointF center() const
    {
        return m_center;
    }
    void setCenter(QPointF c); // silent

    // Width-normalized (fraction of the target element's width), matching the engine.
    qreal radius() const
    {
        return m_radius;
    }
    void setRadius(qreal r); // silent

    // Focal circle, stored verbatim (no authoring UI). See class comment.
    // Named setFocalPoint, not setFocus: a setFocus overload here would hide
    // QWidget::setFocus(Qt::FocusReason) by name and break this widget's own
    // focus grab in mousePressEvent.
    void setFocalPoint(double fx, double fy, double fr); // silent
    double focusX() const
    {
        return m_focusX;
    }
    double focusY() const
    {
        return m_focusY;
    }
    double focusR() const
    {
        return m_focusR;
    }

signals:
    void centerChanged(QPointF);
    void radiusChanged(qreal);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    static constexpr int kEndpointR = 10;
    static constexpr qreal kMinRadius = 1e-3;

    QPointF m_center{0.5, 0.5};
    qreal m_radius{0.35};

    // Focal circle — loaded verbatim, written back verbatim. See class comment.
    double m_focusX{0.5};
    double m_focusY{0.5};
    double m_focusR{0.0};

    enum class Handle { None, Center, Radius, Stop };
    struct HitResult {
        Handle type = Handle::None;
        int stopIndex = -1;
    };

    Handle m_pressHandle{Handle::None};
    Handle m_hoverHandle{Handle::None};
    Handle m_activeHandle{Handle::Center}; // last-interacted handle; arrow-key nudge target

    HitResult hitTest(QPointF px) const;
    QPointF stopPixelPos(int i) const;
    QPointF radiusHandleNorm() const
    {
        return {m_center.x() + m_radius, m_center.y()};
    }
    void updateHover(QPointF pos);
};
