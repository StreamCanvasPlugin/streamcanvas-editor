#pragma once

#include <QAbstractScrollArea>
#include <string>
#include <vector>
#include "engine/animation.h"

class TitleDocument;

class AnimationTimelineEditor : public QAbstractScrollArea {
    Q_OBJECT
public:
    enum class Slot { In, Out, DataIn, DataOut };

    explicit AnimationTimelineEditor(TitleDocument* doc, QWidget* parent = nullptr);

    void setSlot(Slot s);              // switch which AnimationDef field is shown; rebuild()
    Slot slot() const { return m_slot; }

    void rebuild();                    // re-read title().elements, recompute scroll ranges, repaint
    void focusElement(int elementIndex); // set active row + scroll it into view + emit activeClipChanged
    void clearActive();                // no active row; emit activeClipChanged(false, {})

    int  zoomPercent() const;              // round(m_pps / kBasePps * 100)
    void setZoomPercent(int percent);      // clamp so m_pps stays in [kMinPps,kMaxPps]; keep left edge fixed; update ranges; repaint; emit zoomChanged(actualPercent)
    void zoomToFit();                      // fit all clips (contentDuration) into the viewport clip area; reset pan to 0; emit zoomChanged
    double contentDuration() const;        // max(delay+duration) over m_rows, >= 0
    void setPlayhead(double t);            // set m_playheadTime = max(0,t); viewport()->update(); NO signal
    double playhead() const { return m_playheadTime; }

signals:
    void activeClipChanged(bool valid, AnimationDef def);   // active row's clip -> side panel
    void rowClicked(int elementIndex);                       // user clicked a row/clip
    void clipEdited(int elementIndex, AnimationTimelineEditor::Slot slot, AnimationDef def);
    void zoomChanged(int percent);
    void playheadMoved(double t);          // emitted only when the USER drags the playhead (not from setPlayhead)

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    struct Row {
        int elementIndex;
        std::string id;
        AnimationDef def;
    };

    enum class DragZone { None, LeftHandle, Body, RightHandle };
    enum class HitZone { Ruler, Gutter, Row, Empty };
    struct HitResult {
        HitZone zone{HitZone::Empty};
        int row{-1};
        DragZone dragZone{DragZone::None};
    };

    TitleDocument* m_doc;
    Slot m_slot{Slot::In};
    std::vector<Row> m_rows;

    double m_pps{120.0};          // pixels per second (zoom)
    double m_playheadTime{0.0};
    int m_activeRow{-1};          // index into m_rows, or -1

    // drag state
    DragZone m_dragZone{DragZone::None};
    int m_dragRow{-1};
    int m_dragStartX{0};
    float m_dragStartDelay{0.0f};
    float m_dragStartDuration{0.0f};
    AnimationDef m_dragDef;       // live local copy during drag

    bool m_draggingPlayhead{false};

    // coordinate model
    int timeToX(double t) const;
    double xToTime(int x) const;
    int rowTopY(int row) const;
    int yToRow(int y) const;

    void updateScrollRanges();
    HitResult hitTest(const QPoint& pos) const;
    void updateCursorForPos(const QPoint& pos);
    void scrollRowIntoView(int row);
};
