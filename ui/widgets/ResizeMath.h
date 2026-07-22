#pragma once
#include <QPointF>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include "engine/types.hpp"

namespace resizemath {

constexpr double kPi = 3.14159265358979323846;

// Normalized coords (0, 0.5, 1) of the anchor (the handle OPPOSITE the dragged one).
inline void anchorNorm(int handle, double& ax, double& ay)
{
    switch (handle) {
    case 0: ax = 1;   ay = 1;   break; // TL -> anchor BR
    case 1: ax = 0.5; ay = 1;   break; // TC -> BC
    case 2: ax = 0;   ay = 1;   break; // TR -> BL
    case 3: ax = 1;   ay = 0.5; break; // ML -> MR
    case 4: ax = 0;   ay = 0.5; break; // MR -> ML
    case 5: ax = 1;   ay = 0;   break; // BL -> TR
    case 6: ax = 0.5; ay = 0;   break; // BC -> TC
    case 7: ax = 0;   ay = 0;   break; // BR -> TL
    default: ax = 0;  ay = 0;   break;
    }
}

// handle: 0-7. orig: bounds at drag start. R: elementLinear (rotation+shear, no translation).
// dLx,dLy: mouse delta already projected into element-local axes (R^-1 * worldDelta).
// anchorTitle: world pos of the anchor, captured at drag start. origCenterTitle: world pos of
// the element center at drag start. parentOffset: P (parent global offset). shift/ctrl modifiers.
inline Rectangle resizeSolve(int handle, const Rectangle& orig, const QTransform& R,
                             double dLx, double dLy,
                             const QPointF& anchorTitle, const QPointF& origCenterTitle,
                             const QPointF& parentOffset, bool shift, bool ctrl)
{
    const double ow = orig.width, oh = orig.height;
    double w1 = ow, h1 = oh;
    switch (handle) {
    case 0: w1 = ow - dLx; h1 = oh - dLy; break;
    case 1:                h1 = oh - dLy; break;
    case 2: w1 = ow + dLx; h1 = oh - dLy; break;
    case 3: w1 = ow - dLx;                break;
    case 4: w1 = ow + dLx;                break;
    case 5: w1 = ow - dLx; h1 = oh + dLy; break;
    case 6:                h1 = oh + dLy; break;
    case 7: w1 = ow + dLx; h1 = oh + dLy; break;
    default: return orig;
    }
    w1 = std::max(1.0, w1);
    h1 = std::max(1.0, h1);

    // Shift: aspect-lock on corner handles.
    const bool corner = (handle == 0 || handle == 2 || handle == 5 || handle == 7);
    if (shift && corner && ow > 0.0 && oh > 0.0) {
        const double scale = std::min(w1 / ow, h1 / oh);
        w1 = std::max(1.0, ow * scale);
        h1 = std::max(1.0, oh * scale);
    }

    Rectangle nb = orig;
    nb.width = w1;
    nb.height = h1;

    if (ctrl) {
        // Symmetric about the ORIGINAL center: double the size delta, keep center fixed.
        w1 = std::max(1.0, 2.0 * w1 - ow);
        h1 = std::max(1.0, 2.0 * h1 - oh);
        nb.width = w1;
        nb.height = h1;
        // W(center) = P + (nb.x,nb.y) + c1  (R terms cancel) == origCenterTitle
        nb.x = origCenterTitle.x() - parentOffset.x() - w1 / 2.0;
        nb.y = origCenterTitle.y() - parentOffset.y() - h1 / 2.0;
        return nb;
    }

    // Anchor-invariant solve: (nb.x,nb.y) = anchorTitle - P - R*anchorLocal1 - (I-R)*c1
    double ax, ay;
    anchorNorm(handle, ax, ay);
    const QPointF anchorLocal1(ax * w1, ay * h1);
    const QPointF c1(w1 / 2.0, h1 / 2.0);
    // R has no translation, but subtract R.map(origin) defensively.
    const QPointF Rzero = R.map(QPointF(0, 0));
    const QPointF Ra = R.map(anchorLocal1) - Rzero;
    const QPointF Rc = R.map(c1) - Rzero;
    nb.x = anchorTitle.x() - parentOffset.x() - Ra.x() - (c1.x() - Rc.x());
    nb.y = anchorTitle.y() - parentOffset.y() - Ra.y() - (c1.y() - Rc.y());
    return nb;
}

// Normalize degrees to the canonical (-180, 180] range so a rotate drag that crosses the
// atan2 seam (or wraps past a full turn) can't leave the stored angle offset by a multiple
// of 360° — visually identical either way, but keeps the rotation spinbox reading sanely.
inline double normalizeDeg(double d)
{
    d = std::fmod(d, 360.0);
    if (d > 180.0) d -= 360.0;
    else if (d <= -180.0) d += 360.0;
    return d;
}

// Rotation drag: returns the new rotation (degrees). angles in radians (atan2 of the
// cursor about the element center). snap15 → snap the result to the nearest 15°.
inline double rotateSolve(double origRotDeg, double startAngleRad, double curAngleRad, bool snap15)
{
    double deg = origRotDeg + (curAngleRad - startAngleRad) * 180.0 / kPi;
    if (snap15) deg = std::round(deg / 15.0) * 15.0;
    return normalizeDeg(deg);
}

} // namespace resizemath
