#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QElapsedTimer>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <QWidget>

#ifdef __APPLE__
#include <cairo.h>
#else
#include <cairo/cairo.h>
#endif

#include "engine/types.hpp"
#include "model/EditorTitle.h"

class QTimer;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QKeyEvent;
class QPaintEvent;
class QPainter;
class QResizeEvent;
class QMouseEvent;
class QWheelEvent;

class TitleDocument;
class EditorTitle;
class VisualElement;
struct Title;

class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    enum GuideFlag {
        GuideNone = 0,
        GuideRuleOfThirds = 1 << 0,
        GuideCenterLines = 1 << 1,
        GuideTitleSafe = 1 << 2,
        GuideActionSafe = 1 << 3,
    };
    Q_DECLARE_FLAGS(GuideFlags, GuideFlag)

    explicit CanvasWidget(TitleDocument* doc, EditorTitle* editorState, QWidget* parent = nullptr);
    ~CanvasWidget() override;

    void startAnimationPreview(bool playIn);
    void stopAnimationPreview();
    void previewAtTime(bool isIn, bool isData, double t);

    // Guides
    void setGuides(GuideFlags flags);
    GuideFlags guides() const { return m_guideFlags; }

    // Snapping
    void setSnapping(bool on);
    bool snapping() const { return m_snappingEnabled; }

    // Zoom / pan
    void zoomIn();
    void zoomOut();
    void fitToWindow();
    double zoom() const { return m_zoom; }

    // Paint (copy-style) mode
    void setPaintMode(bool active, const std::string& sourceElementId);

    // Axis-aligned bounding box of the element in TITLE/world space, accounting
    // for rotation/shear (used by align/distribute math).
    QRectF elementTitleAABB(const VisualElement& el) const;

signals:
    void paintModeFinished();
    void interactiveEditStarted();
    void interactiveEditFinished();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onDocumentChanged();
    void onSelectionChanged(SelectionId id);
    void onAnimTick();

private:
    // Coordinate helpers
    QRectF letterboxRect() const;
    QPointF widgetToTitle(QPointF pt) const;
    QRectF titleToWidget(const Rectangle& r) const;
    // Full element-local (0..w, 0..h) -> widget-pixel transform, including
    // rotation/shear about the element center. Reproduces the engine's
    // Cairo render order exactly (see Transform::Apply in engine/types.hpp).
    QTransform elementToWidgetTransform(const VisualElement& el) const;
    // Pure linear (rotation+shear) part of the element's transform, in
    // title/world space, with no translation or letterbox scale. Used to
    // project a world-space delta into the element's local axes via
    // .inverted().map(...).
    QTransform elementLinear(const VisualElement& el) const;
    // Element-LOCAL (0..w, 0..h) -> TITLE/world space transform (no letterbox
    // scale/offset). Used to inverse-map a title-space point into local space.
    QTransform elementLocalToTitle(const VisualElement& el) const;

    // Hit testing
    SelectionId hitTest(QPointF titlePt) const;
    int hitHandle(QPointF widgetPt) const;
    // Elements (indices into title.elements) whose widget-space quad
    // intersects the given widget-space rubber-band rect.
    std::vector<int> elementsInMarquee(const QRectF& widgetRect) const;

    // Rendering
    void renderStaticTitle();
    void renderPreviewTitle();
    void setupCairo();
    void teardownCairo();

    // Overlay drawing
    void drawCheckerboard(QPainter& p, const QRectF& lb);
    void drawGuides(QPainter& p, const QRectF& lb);
    void drawSnapLines(QPainter& p, const QRectF& lb);
    void drawElementOutlines(QPainter& p);

    // Title dimension helpers
    int titleW() const;
    int titleH() const;

    // Zoom helper
    void zoomToward(QPointF widgetCenter, double factor);

    // Snapping
    Rectangle applySnapping(Rectangle b, int ei);
    void appendGuideCandidates(QList<double>& candX, QList<double>& candY) const;

    // Cursor
    void updateCursorForPos(QPointF widgetPos);

    // Resize drag
    void applyResizeDrag(QPointF widgetPos, Qt::KeyboardModifiers mods);
    // Rotate drag
    void applyRotateDrag(QPointF widgetPos, Qt::KeyboardModifiers mods);

    // ── Document / selection ──────────────────────────────────────────────────
    TitleDocument* m_doc;
    EditorTitle* m_editorState;

    // ── Cairo offscreen surface ───────────────────────────────────────────────
    cairo_surface_t* m_surface{nullptr};
    cairo_t* m_cr{nullptr};
    QImage m_image;

    // ── Animation preview ─────────────────────────────────────────────────────
    QTimer* m_animTimer{nullptr};
    QElapsedTimer m_elapsedTimer;
    std::unique_ptr<Title> m_previewTitle;

    // ── Guides ───────────────────────────────────────────────────────────────
    GuideFlags m_guideFlags{GuideRuleOfThirds | GuideCenterLines};

    // ── Snapping ─────────────────────────────────────────────────────────────
    bool m_snappingEnabled{true};
    QList<double> m_snapLinesX;
    QList<double> m_snapLinesY;

    // ── Zoom + pan ────────────────────────────────────────────────────────────
    double m_zoom{1.0};
    QPointF m_panOffset{0.0, 0.0};

    bool m_panning{false};
    QPointF m_panStart;
    QPointF m_panStartOffset;

    // ── Element drag / resize ─────────────────────────────────────────────────
    enum class DragMode { None, Move, Resize, Rotate };
    DragMode m_dragMode{DragMode::None};
    int m_dragHandle{-1};
    QPointF m_dragStartWidget;
    QPointF m_dragStartTitle;
    QPointF m_lastDragWidgetPos;
    Rectangle m_dragOrigBounds;
    int m_dragEi{-1};  // index into title.elements (1-based)
    bool m_dragging{false};

    // Group-move support: (elementIndex, origBounds) for every member of a
    // multi-selection move, INCLUDING the active element (m_dragEi). Empty
    // means an ordinary single-element move.
    std::vector<std::pair<int, Rectangle>> m_dragGroup;

    QPointF m_dragAnchorTitle;      // world pos of the resize anchor (opposite handle), captured at press
    QPointF m_dragOrigCenterTitle;  // world pos of the element center at press (for Ctrl-symmetric resize / rotate)
    QPointF m_dragParentOffset;     // P: parent global offset (gpos - local bounds x/y)

    double m_dragStartAngle{0.0};   // cursor angle about element center at rotate-drag start (radians)
    float  m_dragOrigRotation{0.0f};// element rotation at rotate-drag start (degrees)

    // ── Rubber-band marquee selection ─────────────────────────────────────────
    bool m_marqueeActive{false};
    QPointF m_marqueeStartWidget;
    QPointF m_marqueeCurWidget;
    bool m_marqueeAdditive{false};

    // ── Paint (copy-style) mode ───────────────────────────────────────────────
    bool m_paintModeActive{false};
    std::string m_paintSourceEi;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CanvasWidget::GuideFlags)
