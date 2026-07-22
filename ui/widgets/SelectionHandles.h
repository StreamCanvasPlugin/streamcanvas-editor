#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QTransform>

class QPainter;

// Lightweight helper (no QObject) for drawing and hit-testing selection handles.
// The 8 handles are indexed as follows:
//   0=TL, 1=TC, 2=TR, 3=ML, 4=MR, 5=BL, 6=BC, 7=BR
struct SelectionHandles {
    static constexpr int kHandleSize = 8;
    static constexpr int kHandleCount = 8;
    static constexpr int kHitSlack = 4;   // extra px around each handle for hit-testing
    static constexpr int kRotateHandle = 8;    // index of the rotate handle
    static constexpr double kRotateGap = 24.0; // widget px from top edge to rotate knob
    static constexpr double kRotateRadius = 6.0;

    // --- Transform-based (rotation/shear-aware) overloads ---
    // `xf` maps element-local coordinates to widget coordinates. `localRect` is the
    // element's rect in local coordinates, i.e. QRectF(0, 0, width, height).

    // Widget-space center of handle `idx` (0-7 for the corner/edge handles, or
    // kRotateHandle for the rotate knob), given the local->widget transform.
    static QPointF handleCenterT(int idx, const QTransform& xf, const QRectF& localRect);

    // Widget-space rect for handle indices 0-7 only (rotate handle has no rect;
    // use handleCenterT + kRotateRadius for its geometry).
    static QRectF handleRect(int handleIndex, const QTransform& xf, const QRectF& localRect);

    // Return handle index (0-7 or kRotateHandle) if widgetPt hits a handle, -1 otherwise.
    static int hitTest(QPointF widgetPt, const QTransform& xf, const QRectF& localRect);

    // Draw the selection border (as a rotated/sheared quad), the 8 corner/edge
    // handles, and the rotate handle, all mapped through `xf`.
    static void draw(QPainter& p, const QTransform& xf, const QRectF& localRect,
                     int activeHandle = -1, QColor highlight = QColor(0, 120, 215));
};
