#pragma once
#include "GradientGeometryEditor.h"

// Linear gradient endpoint editor. p1/p2 are normalized (0-1) relative to the TARGET
// ELEMENT (see GradientGeometryEditor::setTargetSize()/previewRect()), matching the
// normalized coordinates cairo_pattern_create_linear() multiplies by the element's
// own width/height at render time (engine/types.hpp).
//
// Stop VALUES (add/remove/move/colour) are owned by GradientStopBar. This widget only
// renders stop markers along the p1-p2 line and reports clicks via stopSelected().
class LinearGradientEditor : public GradientGeometryEditor {
    Q_OBJECT
public:
    explicit LinearGradientEditor(QWidget* parent = nullptr);

    QPointF p1() const
    {
        return m_p1;
    }
    QPointF p2() const
    {
        return m_p2;
    }
    void setP1(QPointF p); // silent
    void setP2(QPointF p); // silent

    // Derived numeric accessors for the panel's spin boxes. Both are computed in
    // ELEMENT space (i.e. after scaling the normalized delta by targetWidth()/
    // targetHeight()) so they mean what they say regardless of the element's aspect
    // ratio.
    qreal angleDegrees() const;   // atan2 of the element-space p2-p1 vector, degrees
    qreal lengthFraction() const; // |element-space p2-p1| / element diagonal

    void setAngleDegrees(qreal deg);   // silent; keeps current length, pivots around p1
    void setLengthFraction(qreal len); // silent; keeps current angle, moves p2

    // Presets — all silent.
    void setHorizontal();
    void setVertical();
    void setDiagonalDown(); // top-left -> bottom-right
    void setDiagonalUp();   // bottom-left -> top-right
    void reverse();         // swaps p1 and p2

signals:
    void p1Changed(QPointF);
    void p2Changed(QPointF);

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

    QPointF m_p1{0.2, 0.5};
    QPointF m_p2{0.8, 0.5};

    enum class Handle { None, P1, P2, Stop };
    struct HitResult {
        Handle type = Handle::None;
        int stopIndex = -1;
    };

    Handle m_pressHandle{Handle::None};
    Handle m_hoverHandle{Handle::None};
    Handle m_activeHandle{Handle::P2}; // last-interacted endpoint; arrow-key nudge target

    HitResult hitTest(QPointF px) const;
    QPointF stopPixelPos(int i) const;
    // Snaps the dragged endpoint so the ELEMENT-space angle from anchor to the raw
    // drag point lands on a 15-degree increment, preserving the element-space length
    // of the raw drag vector.
    QPointF snapAngleForDrag(QPointF anchorNorm, QPointF rawNorm) const;
    void updateHover(QPointF pos);
};
