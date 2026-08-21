#pragma once
#include "GradientStop.h"
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWidget>
#include <functional>

class QPainter;

// Shared base for LinearGradientEditor and RadialGradientEditor.
//
// Owns:
//  - Target-size-aware preview geometry. The widget letterboxes a rect matching the
//    TARGET ELEMENT's aspect ratio (see setTargetSize()) inside itself, and all
//    normalized (0-1) coordinates are relative to that rect, not to the widget. This
//    keeps what the user draws consistent with what the engine renders, since the
//    engine multiplies normalized gradient params by the element's own width/height
//    (see engine/types.hpp), not by this widget's size.
//  - Stop MARKER rendering and selection. Stop VALUES (position, color, add/remove)
//    are owned by GradientStopBar; this widget only draws position indicators along
//    its geometry line and reports which one was clicked via stopSelected(). Markers
//    are not draggable here.
//  - Drag-threshold + interactionStarted()/interactionFinished() bookkeeping shared by
//    both subclasses' endpoint-handle dragging.
class GradientGeometryEditor : public QWidget {
    Q_OBJECT
public:
    explicit GradientGeometryEditor(QWidget* parent = nullptr);

    // Size (in element/document units) of the target element this gradient will be
    // painted on. The preview rect is letterboxed to this aspect ratio inside the
    // widget. Defaults to 1:1 (square) until the panel calls this with the selected
    // element's real bounds.
    void setTargetSize(double w, double h);
    double targetWidth() const
    {
        return m_targetW;
    }
    double targetHeight() const
    {
        return m_targetH;
    }

    QVector<GradientStop> stops() const
    {
        return m_stops;
    }
    void setStops(const QVector<GradientStop>& stops); // silent

    int selectedStop() const
    {
        return m_selected;
    }
    // Silent — for the panel to push GradientStopBar's selection into this view for
    // highlighting. User clicks on a marker go through selectStopAndEmit() instead.
    void setSelectedStop(int index);

    QSize sizeHint() const override;

signals:
    void stopSelected(int index);
    void interactionStarted();
    void interactionFinished();

protected:
    void resizeEvent(QResizeEvent*) override;

    // Element-aspect rect, letterboxed inside the widget. All geometry drawing and
    // toPixel()/toNorm() conversions must stay relative to this rect.
    QRectF previewRect() const;
    QPointF toPixel(QPointF norm) const;
    QPointF toNorm(QPointF px) const;

    // Fills the whole widget with the panel background, then draws a checkerboard
    // (using palette roles, no hardcoded colours) clipped to previewRect(), with a
    // border around it.
    void drawBackground(QPainter& p) const;

    // Draws a diamond marker per stop at the pixel position returned by
    // stopPixelPos(index), with the standard two-pass selected-last convention.
    void drawStopMarkers(QPainter& p, const std::function<QPointF(int)>& stopPixelPos) const;
    int hitTestStopMarker(QPointF px, const std::function<QPointF(int)>& stopPixelPos) const;

    // Called by a subclass when the user clicks a stop marker. Updates m_selected and
    // emits stopSelected() if it changed.
    void selectStopAndEmit(int index);

    // Drag-threshold bookkeeping shared by both subclasses' endpoint-handle dragging.
    // A press that never crosses the threshold is treated as a click, not a drag, so
    // it must not mutate geometry.
    void beginPress(QPointF pos);
    void crossedDragThreshold(QPointF pos); // call from mouseMoveEvent while pressed
    bool isDragging() const
    {
        return m_dragStarted;
    }
    void endPress(); // call from mouseReleaseEvent; emits interactionFinished() if dragged
    void cancelPress(); // call when a press turns out not to be on a draggable handle

    static constexpr qreal kDragThresholdPx = 4.0;
    static constexpr int kMarkerD = 7; // stop marker diamond half-size
    static constexpr int kCheckerCell = 6;

    QVector<GradientStop> m_stops;
    int m_selected{0};

private:
    double m_targetW{1.0};
    double m_targetH{1.0};
    bool m_pressActive{false};
    bool m_dragStarted{false};
    QPointF m_pressPos;
};
