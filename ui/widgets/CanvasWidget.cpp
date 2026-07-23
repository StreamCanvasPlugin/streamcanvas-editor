#include "CanvasWidget.h"

#include <algorithm>
#include <cmath>

#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QResizeEvent>
#include <QTimer>
#include <QTransform>
#include <QUrl>
#include <QWheelEvent>

#include "engine/animation.h"
#include "engine/element_image.h"
#include "engine/element_qr.h"
#include "engine/element_rectangle.h"
#include "engine/element_text.h"
#include "engine/title.h"
#include "engine/visual_element.h"

#include "model/EditorTitle.h"
#include "model/TitleDocument.h"
#include "model/UndoCommands.h"

#include "ResizeMath.h"
#include "SelectionHandles.h"
#include "icons.h"

static constexpr int kArrowMergeTag = 9999;

static const QStringList kCanvasImageExts = {
    "png", "jpg", "jpeg", "bmp", "gif", "tiff", "tif", "webp"};

static Rectangle globalBounds(const VisualElement& el)
{
    const Point pos = el.GetGlobalPosition();
    const Rectangle b = el.GetBounds();
    return {pos.x, pos.y, b.width, b.height};
}

// Element's own rect in LOCAL coordinates: (0,0,width,height).
static QRectF localBoundsRect(const VisualElement& el)
{
    const Rectangle b = el.GetBounds();
    return QRectF(0.0, 0.0, b.width, b.height);
}

// Maps element-LOCAL coords (0..w, 0..h) to WIDGET pixels.
// lbX/lbY/lbW/lbH describe the letterbox rect; titleW/titleH the title size.
// Mirrors the engine's Cairo render order exactly (see Transform::Apply in
// engine/types.hpp): translate(cx,cy) -> shear -> rotate -> translate(-cx,-cy),
// applied at the element's global position, itself placed within the
// letterboxed title viewport.
static QTransform makeElementTransform(
    double gposX, double gposY, double w, double h,
    double rotationDeg, double shearX, double shearY,
    double lbX, double lbY, double lbW, double lbH, double titleW, double titleH)
{
    QTransform m;
    m.translate(lbX, lbY);
    m.scale(lbW / titleW, lbH / titleH);  // letterbox scale
    m.translate(gposX, gposY);            // world position
    m.translate(w / 2.0, h / 2.0);         // to center
    m.shear(shearX, shearY);              // == cairo shearMatrix
    m.rotate(rotationDeg);
    m.translate(-w / 2.0, -h / 2.0);
    return m;
}

static constexpr double kZoomMin = 0.05;
static constexpr double kZoomMax = 30.0;
static constexpr double kZoomStep = 1.12;

// ── Lifecycle ─────────────────────────────────────────────────────────────────

CanvasWidget::CanvasWidget(TitleDocument* doc, EditorTitle* editorState, QWidget* parent)
    : QWidget(parent), m_doc(doc), m_editorState(editorState)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);

    setupCairo();

    connect(m_doc, &TitleDocument::documentChanged, this, &CanvasWidget::onDocumentChanged);
    connect(m_editorState, &EditorTitle::selectionChanged, this, &CanvasWidget::onSelectionChanged);
    connect(m_editorState, &EditorTitle::selectionSetChanged, this, [this] { update(); });

    renderStaticTitle();
}

CanvasWidget::~CanvasWidget()
{
    if (m_animTimer)
        m_animTimer->stop();
    m_image = QImage{};
    teardownCairo();
}

// ── Title dimension helpers ───────────────────────────────────────────────────

int CanvasWidget::titleW() const
{
    return m_doc ? m_doc->title().width : 1920;
}

int CanvasWidget::titleH() const
{
    return m_doc ? m_doc->title().height : 1080;
}

// ── Cairo setup/teardown ──────────────────────────────────────────────────────

void CanvasWidget::setupCairo()
{
    const int w = titleW(), h = titleH();
    m_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    m_cr = cairo_create(m_surface);
    m_image = QImage(cairo_image_surface_get_data(m_surface), w, h,
                     cairo_image_surface_get_stride(m_surface), QImage::Format_ARGB32_Premultiplied);
}

void CanvasWidget::teardownCairo()
{
    if (m_cr) { cairo_destroy(m_cr); m_cr = nullptr; }
    if (m_surface) { cairo_surface_destroy(m_surface); m_surface = nullptr; }
}

// ── Coordinate helpers ────────────────────────────────────────────────────────

QRectF CanvasWidget::letterboxRect() const
{
    const double wW = width(), wH = height();
    const double base = std::min(wW / titleW(), wH / titleH()) * m_zoom;
    const double dW = titleW() * base, dH = titleH() * base;
    return {(wW - dW) / 2.0 + m_panOffset.x(), (wH - dH) / 2.0 + m_panOffset.y(), dW, dH};
}

QPointF CanvasWidget::widgetToTitle(QPointF pt) const
{
    const QRectF lb = letterboxRect();
    if (lb.width() <= 0.0 || lb.height() <= 0.0) return {};
    return {(pt.x() - lb.left()) / lb.width() * titleW(),
            (pt.y() - lb.top()) / lb.height() * titleH()};
}

QRectF CanvasWidget::titleToWidget(const Rectangle& r) const
{
    const QRectF lb = letterboxRect();
    const double sx = lb.width() / titleW(), sy = lb.height() / titleH();
    return {lb.left() + r.x * sx, lb.top() + r.y * sy, r.width * sx, r.height * sy};
}

QTransform CanvasWidget::elementToWidgetTransform(const VisualElement& el) const
{
    const Point pos = el.GetGlobalPosition();
    const Rectangle b = el.GetBounds();
    const QRectF lb = letterboxRect();
    return makeElementTransform(pos.x, pos.y, b.width, b.height,
                                 el.GetRotation(), el.GetShearX(), el.GetShearY(),
                                 lb.left(), lb.top(), lb.width(), lb.height(),
                                 titleW(), titleH());
}

QTransform CanvasWidget::elementLinear(const VisualElement& el) const
{
    // Pure linear part (rotation+shear) matching the center-pivot transform's
    // linear component, in title/world space. No translation.
    QTransform r;
    r.shear(el.GetShearX(), el.GetShearY());
    r.rotate(el.GetRotation());
    return r;
}

QTransform CanvasWidget::elementLocalToTitle(const VisualElement& el) const
{
    const Point pos = el.GetGlobalPosition();
    const Rectangle b = el.GetBounds();
    return makeElementTransform(pos.x, pos.y, b.width, b.height,
                                el.GetRotation(), el.GetShearX(), el.GetShearY(),
                                0.0, 0.0, titleW(), titleH(), titleW(), titleH());
}

QRectF CanvasWidget::elementTitleAABB(const VisualElement& el) const
{
    const QTransform xf = elementLocalToTitle(el);
    const QRectF lr = localBoundsRect(el);
    QPolygonF poly;
    poly << xf.map(lr.topLeft()) << xf.map(lr.topRight())
         << xf.map(lr.bottomRight()) << xf.map(lr.bottomLeft());
    return poly.boundingRect();
}

// ── Zoom helpers ──────────────────────────────────────────────────────────────

void CanvasWidget::zoomToward(QPointF cursor, double factor)
{
    const QRectF lb = letterboxRect();
    const double sx = lb.width() > 0 ? (cursor.x() - lb.left()) / lb.width() * titleW() : 0;
    const double sy = lb.height() > 0 ? (cursor.y() - lb.top()) / lb.height() * titleH() : 0;

    m_zoom = std::clamp(m_zoom * factor, kZoomMin, kZoomMax);

    const double wW = width(), wH = height();
    const double base = std::min(wW / titleW(), wH / titleH());
    const double dW = titleW() * base * m_zoom, dH = titleH() * base * m_zoom;

    m_panOffset.setX(cursor.x() - sx * dW / titleW() - (wW - dW) / 2.0);
    m_panOffset.setY(cursor.y() - sy * dH / titleH() - (wH - dH) / 2.0);
    update();
}

void CanvasWidget::zoomIn() { zoomToward(rect().center(), 1.25); }
void CanvasWidget::zoomOut() { zoomToward(rect().center(), 1.0 / 1.25); }
void CanvasWidget::fitToWindow() { m_zoom = 1.0; m_panOffset = {}; update(); }

// ── Paint (copy-style) mode ───────────────────────────────────────────────────

void CanvasWidget::setPaintMode(bool active, const std::string& sourceElementId)
{
    m_paintModeActive = active;
    m_paintSourceEi = active ? sourceElementId : std::string();
    if (active)
        setCursor(QCursor(themedIcon(Icons16::Misc_PaintBrushThin).pixmap(24, 24)));
    else
        unsetCursor();
}

// ── Snapping ──────────────────────────────────────────────────────────────────

void CanvasWidget::setSnapping(bool on)
{
    m_snappingEnabled = on;
    if (!on) { m_snapLinesX.clear(); m_snapLinesY.clear(); }
}

void CanvasWidget::appendGuideCandidates(QList<double>& candX, QList<double>& candY) const
{
    const double W = titleW(), H = titleH();
    if (m_guideFlags & GuideRuleOfThirds) {
        candX << W / 3.0 << 2.0 * W / 3.0;
        candY << H / 3.0 << 2.0 * H / 3.0;
    }
    if (m_guideFlags & GuideTitleSafe) {
        candX << W * 0.05 << W * 0.95;
        candY << H * 0.05 << H * 0.95;
    }
    if (m_guideFlags & GuideActionSafe) {
        candX << W * 0.10 << W * 0.90;
        candY << H * 0.10 << H * 0.90;
    }
}

Rectangle CanvasWidget::applySnapping(Rectangle b, int ei)
{
    m_snapLinesX.clear();
    m_snapLinesY.clear();

    const double threshold = 8.0 * titleW() / letterboxRect().width();

    QList<double> candX = {0.0, titleW() / 2.0, double(titleW())};
    QList<double> candY = {0.0, titleH() / 2.0, double(titleH())};
    appendGuideCandidates(candX, candY);

    const Title& t = m_doc->title();
    for (int i = 1; i < (int)t.elements.size(); ++i) {
        if (i == ei) continue;
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[i].get());
        if (!ve) continue;
        const Rectangle ob = globalBounds(*ve);
        candX << ob.x << ob.x + ob.width / 2.0 << ob.x + ob.width;
        candY << ob.y << ob.y + ob.height / 2.0 << ob.y + ob.height;
    }

    // Compute parent world offset so we can snap in world space
    double parentOffX = 0.0, parentOffY = 0.0;
    if (ei >= 1 && ei < (int)t.elements.size()) {
        const IElement* parent = t.elements[ei]->GetParent();
        if (parent && parent->GetId() != "__root") {
            Point pg = parent->GetGlobalPosition();
            parentOffX = pg.x;
            parentOffY = pg.y;
        }
    }

    Rectangle gb = b;
    gb.x += parentOffX;
    gb.y += parentOffY;

    const double anchX[3] = {gb.x, gb.x + gb.width / 2.0, gb.x + gb.width};
    const double anchY[3] = {gb.y, gb.y + gb.height / 2.0, gb.y + gb.height};

    auto bestSnap = [&](const double anchors[], const QList<double>& cands,
                        double& outDelta, double& outLine) -> bool {
        double best = threshold;
        bool found = false;
        for (double a : {anchors[0], anchors[1], anchors[2]}) {
            for (double c : cands) {
                double d = std::abs(a - c);
                if (d < best) { best = d; outDelta = c - a; outLine = c; found = true; }
            }
        }
        return found;
    };

    double dX = 0, snapX = 0, dY = 0, snapY = 0;
    if (bestSnap(anchX, candX, dX, snapX)) { gb.x += dX; m_snapLinesX << snapX; }
    if (bestSnap(anchY, candY, dY, snapY)) { gb.y += dY; m_snapLinesY << snapY; }

    b.x = gb.x - parentOffX;
    b.y = gb.y - parentOffY;
    return b;
}

static double snapEdge(double edge, const QList<double>& cands, double threshold)
{
    double best = threshold, result = edge;
    for (double c : cands) {
        double d = std::abs(edge - c);
        if (d < best) { best = d; result = c; }
    }
    return result;
}

// ── Hit testing ───────────────────────────────────────────────────────────────

bool CanvasWidget::isActiveSelectionLocked() const
{
    const SelectionId sel = m_editorState->selection();
    if (sel.level != SelectionId::Level::Element) return false;
    const Title& t = m_doc->title();
    if (sel.elementIndex < 1 || sel.elementIndex >= (int)t.elements.size()) return false;
    return m_doc->isElementLocked(t.elements[sel.elementIndex]->GetId());
}

SelectionId CanvasWidget::hitTest(QPointF titlePt) const
{
    const Title& t = m_doc->title();

    std::vector<int> order;
    order.reserve(t.elements.size());
    for (int i = 1; i < (int)t.elements.size(); ++i)
        order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        const auto* va = dynamic_cast<const VisualElement*>(t.elements[a].get());
        const auto* vb = dynamic_cast<const VisualElement*>(t.elements[b].get());
        return (va ? va->zOrder : 0) > (vb ? vb->zOrder : 0);
    });

    for (int ei : order) {
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[ei].get());
        if (!ve) continue;
        if (m_doc->isElementLocked(ve->GetId())) continue;   // locked: not click-selectable on canvas
        bool ok = false;
        const QTransform inv = elementLocalToTitle(*ve).inverted(&ok);
        if (!ok) continue;               // degenerate transform (e.g. shear det 0): skip
        const QPointF lp = inv.map(titlePt);
        const Rectangle b = ve->GetBounds();
        if (lp.x() >= 0.0 && lp.x() <= b.width && lp.y() >= 0.0 && lp.y() <= b.height)
            return {SelectionId::Level::Element, ei};
    }
    return {};
}

std::vector<int> CanvasWidget::elementsInMarquee(const QRectF& widgetRect) const
{
    std::vector<int> result;
    const Title& t = m_doc->title();

    QPainterPath rectPath;
    rectPath.addRect(widgetRect);

    for (int i = 1; i < (int)t.elements.size(); ++i) {
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[i].get());
        if (!ve) continue;
        if (m_doc->isElementLocked(ve->GetId())) continue;   // locked: excluded from marquee selection
        const QTransform xf = elementToWidgetTransform(*ve);
        const QRectF lr = localBoundsRect(*ve);
        QPolygonF poly;
        poly << xf.map(lr.topLeft()) << xf.map(lr.topRight())
             << xf.map(lr.bottomRight()) << xf.map(lr.bottomLeft())
             << xf.map(lr.topLeft());  // close

        QPainterPath polyPath;
        polyPath.addPolygon(poly);

        if (rectPath.intersects(polyPath))
            result.push_back(i);
    }
    return result;
}

int CanvasWidget::hitHandle(QPointF widgetPt) const
{
    const SelectionId sel = m_editorState->selection();
    if (sel.level != SelectionId::Level::Element)
        return -1;
    const Title& t = m_doc->title();
    if (sel.elementIndex < 1 || sel.elementIndex >= (int)t.elements.size())
        return -1;
    const auto* ve = dynamic_cast<const VisualElement*>(t.elements[sel.elementIndex].get());
    if (!ve) return -1;
    return SelectionHandles::hitTest(widgetPt, elementToWidgetTransform(*ve), localBoundsRect(*ve));
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void CanvasWidget::renderStaticTitle()
{
    if (!m_cr || !m_surface) return;

    cairo_save(m_cr);
    cairo_set_operator(m_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m_cr);
    cairo_restore(m_cr);

    // Set title to "fully visible at rest" so animations don't affect the static view
    Title& t = const_cast<Title&>(m_doc->title());
    t.state = TitleState::Visible;
    t.timer = 1e9;

    cairo_set_antialias(m_cr, CAIRO_ANTIALIAS_BEST);
    t.Render(m_cr);
    cairo_surface_flush(m_surface);
    update();
}

void CanvasWidget::renderPreviewTitle()
{
    if (!m_cr || !m_surface || !m_previewTitle) return;

    cairo_save(m_cr);
    cairo_set_operator(m_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m_cr);
    cairo_restore(m_cr);

    m_previewTitle->Render(m_cr);
    cairo_surface_flush(m_surface);
    update();
}

// ── Overlay helpers ───────────────────────────────────────────────────────────

void CanvasWidget::setGuides(GuideFlags flags)
{
    m_guideFlags = flags;
    update();
}

void CanvasWidget::drawCheckerboard(QPainter& p, const QRectF& lb)
{
    static constexpr int sz = 10;
    static QPixmap checker;
    if (checker.isNull()) {
        checker = QPixmap(sz * 2, sz * 2);
        QPainter cp(&checker);
        cp.fillRect(0, 0, sz, sz, QColor(72, 72, 72));
        cp.fillRect(sz, sz, sz, sz, QColor(72, 72, 72));
        cp.fillRect(sz, 0, sz, sz, QColor(56, 56, 56));
        cp.fillRect(0, sz, sz, sz, QColor(56, 56, 56));
    }
    p.save();
    p.setClipRect(lb);
    p.drawTiledPixmap(lb.toRect(), checker);
    p.restore();
}

void CanvasWidget::drawGuides(QPainter& p, const QRectF& lb)
{
    if (m_guideFlags == GuideNone) return;
    p.save();
    p.setClipRect(lb);

    const double guideWidth = std::clamp(lb.width() / titleW(), 1.0, 3.0);

    if (m_guideFlags & GuideRuleOfThirds) {
        p.setPen(QPen(QColor(255, 255, 255, 55), guideWidth));
        for (int i = 1; i <= 2; ++i) {
            double x = lb.left() + lb.width() * i / 3.0;
            double y = lb.top() + lb.height() * i / 3.0;
            p.drawLine(QPointF(x, lb.top()), QPointF(x, lb.bottom()));
            p.drawLine(QPointF(lb.left(), y), QPointF(lb.right(), y));
        }
    }
    if (m_guideFlags & GuideCenterLines) {
        p.setPen(QPen(QColor(255, 255, 255, 38), guideWidth, Qt::DashLine));
        double cx = lb.center().x(), cy = lb.center().y();
        p.drawLine(QPointF(cx, lb.top()), QPointF(cx, lb.bottom()));
        p.drawLine(QPointF(lb.left(), cy), QPointF(lb.right(), cy));
    }
    if (m_guideFlags & GuideTitleSafe) {
        double mx = lb.width() * 0.05, my = lb.height() * 0.05;
        p.setPen(QPen(QColor(255, 210, 50, 80), guideWidth));
        p.drawRect(lb.adjusted(mx, my, -mx, -my));
    }
    if (m_guideFlags & GuideActionSafe) {
        double mx = lb.width() * 0.10, my = lb.height() * 0.10;
        p.setPen(QPen(QColor(255, 100, 50, 80), guideWidth));
        p.drawRect(lb.adjusted(mx, my, -mx, -my));
    }
    p.restore();
}

void CanvasWidget::drawSnapLines(QPainter& p, const QRectF& lb)
{
    if (m_snapLinesX.isEmpty() && m_snapLinesY.isEmpty()) return;
    p.save();
    p.setClipRect(lb);
    const double guideWidth = std::clamp(lb.width() / titleW(), 1.0, 3.0);
    p.setPen(QPen(QColor(0, 150, 255, 210), guideWidth));
    for (double sx : m_snapLinesX) {
        double wx = lb.left() + sx / titleW() * lb.width();
        p.drawLine(QPointF(wx, lb.top()), QPointF(wx, lb.bottom()));
    }
    for (double sy : m_snapLinesY) {
        double wy = lb.top() + sy / titleH() * lb.height();
        p.drawLine(QPointF(lb.left(), wy), QPointF(lb.right(), wy));
    }
    p.restore();
}

void CanvasWidget::drawElementOutlines(QPainter& p)
{
    const Title& t = m_doc->title();
    const bool singleSelect = m_editorState->selectionCount() == 1;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(Qt::NoBrush);

    for (int i = 1; i < (int)t.elements.size(); ++i) {
        const bool selected = m_editorState->isSelected(i);
        if (selected && singleSelect) continue;  // active single selection gets the handle box instead
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[i].get());
        if (!ve) continue;
        const QTransform xf = elementToWidgetTransform(*ve);
        const QRectF lr = localBoundsRect(*ve);
        QPolygonF poly;
        poly << xf.map(lr.topLeft()) << xf.map(lr.topRight())
             << xf.map(lr.bottomRight()) << xf.map(lr.bottomLeft());
        if (selected) {
            p.setPen(QPen(QColor(0, 0, 0, 100), 2.5));
            p.drawPolygon(poly);
            p.setPen(QPen(palette().highlight().color(), 1.5));
            p.drawPolygon(poly);
        } else {
            p.setPen(QPen(QColor(0, 0, 0, 84), 3.0, Qt::DashLine));
            p.drawPolygon(poly);
            p.setPen(QPen(QColor(255, 255, 255, 178), 1.0, Qt::DashLine));
            p.drawPolygon(poly);
        }
    }
    p.restore();
}

// ── paintEvent ────────────────────────────────────────────────────────────────

void CanvasWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(38, 38, 38));

    const QRectF lb = letterboxRect();
    drawCheckerboard(p, lb);

    if (!m_image.isNull())
        p.drawImage(lb, m_image);

    p.setPen(QPen(QColor(0, 0, 0, 120), 1));
    p.drawLine(lb.topLeft(), lb.topRight());
    p.drawLine(lb.topLeft(), lb.bottomLeft());
    p.setPen(QPen(QColor(100, 100, 100, 140), 1));
    p.drawLine(lb.topRight(), lb.bottomRight());
    p.drawLine(lb.bottomLeft(), lb.bottomRight());

    drawGuides(p, lb);

    if (m_dragging)
        drawSnapLines(p, lb);

    if (!m_previewTitle)
        drawElementOutlines(p);

    if (!m_previewTitle && m_editorState->selectionCount() <= 1) {
        const SelectionId sel = m_editorState->selection();
        if (sel.level == SelectionId::Level::Element) {
            const Title& t = m_doc->title();
            if (sel.elementIndex >= 1 && sel.elementIndex < (int)t.elements.size()) {
                const auto* ve = dynamic_cast<const VisualElement*>(t.elements[sel.elementIndex].get());
                if (ve)
                    SelectionHandles::draw(p, elementToWidgetTransform(*ve), localBoundsRect(*ve),
                                           m_dragging ? m_dragHandle : -1,
                                           palette().highlight().color());
            }
        }
    }

    if (m_marqueeActive) {
        const QRectF mr = QRectF(m_marqueeStartWidget, m_marqueeCurWidget).normalized();
        p.save();
        QColor fill = palette().highlight().color();
        fill.setAlpha(40);
        p.setBrush(fill);
        QPen pen(palette().highlight().color(), 1.0, Qt::DashLine);
        p.setPen(pen);
        p.drawRect(mr);
        p.restore();
    }
}

void CanvasWidget::resizeEvent(QResizeEvent*) { update(); }

// ── Slots ─────────────────────────────────────────────────────────────────────

void CanvasWidget::onDocumentChanged()
{
    if (m_surface) {
        int w = titleW(), h = titleH();
        if (cairo_image_surface_get_width(m_surface) != w ||
            cairo_image_surface_get_height(m_surface) != h) {
            m_image = QImage{};
            teardownCairo();
            setupCairo();
        }
    }
    if (!m_previewTitle)
        renderStaticTitle();
}

void CanvasWidget::onSelectionChanged(SelectionId)
{
    if (!m_previewTitle)
        renderStaticTitle();
    else
        update();
}

void CanvasWidget::onAnimTick()
{
    if (!m_previewTitle) return;
    const float delta = static_cast<float>(m_elapsedTimer.restart()) / 1000.0f;
    m_previewTitle->Tick(delta);
    renderPreviewTitle();
}

// ── Animation preview ─────────────────────────────────────────────────────────

void CanvasWidget::startAnimationPreview(bool playIn)
{
    stopAnimationPreview();

    const QString tmpPath = QDir::tempPath() + "/obs_gfx_preview.ogt";
    m_doc->title().Save(tmpPath.toStdString());
    m_previewTitle = std::make_unique<Title>(Title::Load(tmpPath.toStdString()));
    QFile::remove(tmpPath);

    if (playIn)
        m_previewTitle->TriggerIn();
    else
        m_previewTitle->TriggerOut();

    m_elapsedTimer.start();
    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(16);
    connect(m_animTimer, &QTimer::timeout, this, &CanvasWidget::onAnimTick);
    m_animTimer->start();
}

void CanvasWidget::stopAnimationPreview()
{
    if (m_animTimer) {
        m_animTimer->stop();
        m_animTimer->deleteLater();
        m_animTimer = nullptr;
    }
    m_previewTitle.reset();
    renderStaticTitle();
}

void CanvasWidget::previewAtTime(bool isIn, bool isData, double t)
{
    if (m_animTimer) {
        m_animTimer->stop();
        m_animTimer->deleteLater();
        m_animTimer = nullptr;
    }

    if (!m_previewTitle) {
        const QString tmpPath = QDir::tempPath() + "/obs_gfx_preview.ogt";
        m_doc->title().Save(tmpPath.toStdString());
        m_previewTitle = std::make_unique<Title>(Title::Load(tmpPath.toStdString()));
        QFile::remove(tmpPath);
    }

    if (isData) {
        // timer=9999: ensures all main in/out animations evaluate as fully done
        // so elements render at full opacity before data anim is overlaid.
        m_previewTitle->state = TitleState::Visible;
        m_previewTitle->timer = 9999.0;
        for (int i = 1; i < (int)m_previewTitle->elements.size(); ++i) {
            auto* ve = dynamic_cast<VisualElement*>(m_previewTitle->elements[i].get());
            if (!ve) continue;
            const AnimationDef& def = isIn ? ve->dataInAnimation : ve->dataOutAnimation;
            if (def.type != AnimationType::None)
                ve->SetDataPreviewTime(!isIn, t);
        }
        renderPreviewTitle();
        return;
    }

    m_previewTitle->timer = t;

    bool allDone = true;
    for (int i = 1; i < (int)m_previewTitle->elements.size(); ++i) {
        const auto* ve = dynamic_cast<const VisualElement*>(m_previewTitle->elements[i].get());
        if (!ve) continue;
        const AnimationDef& anim = isIn ? ve->inAnimation : ve->outAnimation;
        if (t < anim.delay + anim.duration) { allDone = false; break; }
    }

    if (isIn)
        m_previewTitle->state = allDone ? TitleState::Visible : TitleState::AnimatingIn;
    else
        m_previewTitle->state = allDone ? TitleState::Hidden : TitleState::AnimatingOut;

    renderPreviewTitle();
}

// ── Keyboard ──────────────────────────────────────────────────────────────────

void CanvasWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (m_dragging && (m_dragMode == DragMode::Resize || m_dragMode == DragMode::Rotate)) {
        Qt::KeyboardModifiers mods = event->modifiers();
        switch (event->key()) {
        case Qt::Key_Shift:   mods &= ~Qt::ShiftModifier;   break;
        case Qt::Key_Control: mods &= ~Qt::ControlModifier; break;
        case Qt::Key_Alt:     mods &= ~Qt::AltModifier;     break;
        default: break;
        }
        if (m_dragMode == DragMode::Resize) applyResizeDrag(m_lastDragWidgetPos, mods);
        else applyRotateDrag(m_lastDragWidgetPos, mods);
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void CanvasWidget::keyPressEvent(QKeyEvent* event)
{
    if (m_dragging && (m_dragMode == DragMode::Resize || m_dragMode == DragMode::Rotate)) {
        if (m_dragMode == DragMode::Resize) applyResizeDrag(m_lastDragWidgetPos, event->modifiers());
        else applyRotateDrag(m_lastDragWidgetPos, event->modifiers());
        event->accept();
        return;
    }

    const SelectionId sel = m_editorState->selection();
    if (m_previewTitle || sel.level != SelectionId::Level::Element) {
        QWidget::keyPressEvent(event);
        return;
    }

    double dx = 0, dy = 0;
    const double step = (event->modifiers() & Qt::ShiftModifier) ? 10.0 : 1.0;
    switch (event->key()) {
    case Qt::Key_Left:  dx = -step; break;
    case Qt::Key_Right: dx = +step; break;
    case Qt::Key_Up:    dy = -step; break;
    case Qt::Key_Down:  dy = +step; break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }

    const Title& t = m_doc->title();

    if (m_editorState->selectionCount() <= 1) {
        const int ei = sel.elementIndex;
        if (ei < 1 || ei >= (int)t.elements.size()) return;

        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[ei].get());
        if (!ve) return;
        if (m_doc->isElementLocked(ve->GetId())) { event->accept(); return; }  // locked: not nudgeable

        Rectangle b = ve->GetBounds();
        b.x += dx;
        b.y += dy;
        m_doc->undoStack()->push(new SetElementBoundsCmd(m_doc, ve->GetId(), b, kArrowMergeTag));
        event->accept();
        return;
    }

    // Multi-selection nudge: one undo macro per key press (cross-press
    // coalescing via kArrowMergeTag does not apply to macros). Locked members
    // stay put; only unlocked members move.
    std::vector<int> movable;
    for (int gi : m_editorState->selectedIndices()) {
        if (gi < 1 || gi >= (int)t.elements.size()) continue;
        const auto* gve = dynamic_cast<const VisualElement*>(t.elements[gi].get());
        if (!gve) continue;
        if (m_doc->isElementLocked(gve->GetId())) continue;
        movable.push_back(gi);
    }
    if (movable.empty()) { event->accept(); return; }

    m_doc->undoStack()->beginMacro("Move elements");
    for (int gi : movable) {
        const auto* gve = static_cast<const VisualElement*>(t.elements[gi].get());
        Rectangle b = gve->GetBounds();
        b.x += dx;
        b.y += dy;
        m_doc->undoStack()->push(new SetElementBoundsCmd(m_doc, gve->GetId(), b, kArrowMergeTag));
    }
    m_doc->undoStack()->endMacro();
    event->accept();
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void CanvasWidget::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const double factor = event->angleDelta().y() > 0 ? kZoomStep : 1.0 / kZoomStep;
        zoomToward(event->position(), factor);
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void CanvasWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panStart = event->position();
        m_panStartOffset = m_panOffset;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton || m_previewTitle) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPointF wpos = event->position();

    if (m_paintModeActive) {
        const QPointF sp = widgetToTitle(wpos);
        const SelectionId hit = hitTest(sp);
        if (hit.level == SelectionId::Level::Element) {
            const Title& t = m_doc->title();
            const std::string dstId = t.elements[hit.elementIndex]->GetId();
            if (dstId != m_paintSourceEi)
                m_doc->undoStack()->push(new CopyElementStyleCmd(m_doc, m_paintSourceEi, dstId));
        }
        setPaintMode(false, std::string());
        emit paintModeFinished();
        event->accept();
        return;
    }

    // 1. Rotate/resize handle hit? Handles only exist for a single selection
    //    (2+ selected is move-only), so don't grab an invisible handle otherwise.
    //    A locked active element (e.g. selected via the tree) can't be dragged.
    const int handle = (m_editorState->selectionCount() <= 1 && !isActiveSelectionLocked())
                       ? hitHandle(wpos) : -1;
    if (handle == SelectionHandles::kRotateHandle) {
        const SelectionId sel = m_editorState->selection();
        const Title& t = m_doc->title();
        const auto* ve = (sel.level == SelectionId::Level::Element &&
                          sel.elementIndex >= 1 && sel.elementIndex < (int)t.elements.size())
                         ? dynamic_cast<const VisualElement*>(t.elements[sel.elementIndex].get())
                         : nullptr;
        if (ve) {
            m_dragMode = DragMode::Rotate;
            m_dragHandle = handle;
            m_dragEi = sel.elementIndex;
            m_dragOrigRotation = ve->GetRotation();
            const QTransform x0 = elementLocalToTitle(*ve);
            const Rectangle b = ve->GetBounds();
            m_dragOrigCenterTitle = x0.map(QPointF(b.width / 2.0, b.height / 2.0));
            const QPointF sp = widgetToTitle(wpos);
            m_dragStartAngle = std::atan2(sp.y() - m_dragOrigCenterTitle.y(),
                                          sp.x() - m_dragOrigCenterTitle.x());
            m_lastDragWidgetPos = wpos;
            m_dragging = true;
            emit interactiveEditStarted();
        }
        return;
    }

    // 2. Resize handle hit?
    if (handle >= 0) {
        const SelectionId sel = m_editorState->selection();
        const Title& t = m_doc->title();
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[sel.elementIndex].get());
        m_dragMode = DragMode::Resize;
        m_dragHandle = handle;
        m_dragStartWidget = wpos;
        m_dragStartTitle = widgetToTitle(wpos);
        m_dragOrigBounds = ve ? ve->GetBounds() : Rectangle{};
        m_dragEi = sel.elementIndex;
        m_dragging = true;

        if (ve) {
            const QTransform x0 = elementLocalToTitle(*ve);
            const double ow0 = m_dragOrigBounds.width, oh0 = m_dragOrigBounds.height;
            double ax0, ay0;
            resizemath::anchorNorm(handle, ax0, ay0);
            m_dragAnchorTitle     = x0.map(QPointF(ax0 * ow0, ay0 * oh0));
            m_dragOrigCenterTitle = x0.map(QPointF(ow0 / 2.0, oh0 / 2.0));
            const Point gpos = ve->GetGlobalPosition();
            m_dragParentOffset = QPointF(gpos.x - m_dragOrigBounds.x, gpos.y - m_dragOrigBounds.y);
        }

        emit interactiveEditStarted();
        return;
    }

    const SelectionId curSel = m_editorState->selection();
    const QPointF sp = widgetToTitle(wpos);
    const bool additive = (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier));

    // 3. Always hit-test (top-most element under cursor wins), then decide
    //    whether to keep dragging the current selection or select the new hit.
    const SelectionId hit = hitTest(sp);

    if (hit.level != SelectionId::Level::Element) {
        // Empty canvas: begin a rubber-band marquee. Selection is only
        // cleared on release if the marquee turns out to be a plain click
        // (see mouseReleaseEvent).
        m_marqueeActive = true;
        m_marqueeStartWidget = wpos;
        m_marqueeCurWidget = wpos;
        m_marqueeAdditive = additive;
        update();
        return;
    }

    if (additive) {
        m_editorState->toggleSelection(hit.elementIndex);
        return;
    }

    if (m_editorState->isSelected(hit.elementIndex) && m_editorState->selectionCount() > 1) {
        // Element is already part of a multi-selection: keep the set, just
        // make it the active/anchor element (does not change membership).
        m_editorState->addToSelection(hit.elementIndex);
    } else if (!(hit == curSel)) {
        m_editorState->setSelection(hit);
    }

    const Title& t = m_doc->title();
    const auto* ve = dynamic_cast<const VisualElement*>(t.elements[hit.elementIndex].get());
    if (ve) {
        m_dragMode = DragMode::Move;
        m_dragHandle = -1;
        m_dragStartWidget = wpos;
        m_dragStartTitle = sp;
        m_dragOrigBounds = ve->GetBounds();
        m_dragEi = hit.elementIndex;
        m_dragging = true;

        m_dragGroup.clear();
        if (m_editorState->selectionCount() > 1 && m_editorState->isSelected(hit.elementIndex)) {
            for (int gi : m_editorState->selectedIndices()) {
                if (gi < 1 || gi >= (int)t.elements.size()) continue;
                const auto* gve = dynamic_cast<const VisualElement*>(t.elements[gi].get());
                if (gve && !m_doc->isElementLocked(gve->GetId()))   // locked members stay put
                    m_dragGroup.emplace_back(gi, gve->GetBounds());
            }
        }

        emit interactiveEditStarted();
    }
}

void CanvasWidget::applyResizeDrag(QPointF widgetPos, Qt::KeyboardModifiers mods)
{
    const Title& t = m_doc->title();
    if (m_dragEi < 1 || m_dragEi >= (int)t.elements.size()) return;
    const auto* ve = dynamic_cast<const VisualElement*>(t.elements[m_dragEi].get());
    if (!ve) return;

    const QPointF titlePt = widgetToTitle(widgetPos);
    const QPointF deltaWorld = titlePt - m_dragStartTitle;

    const QTransform R = elementLinear(*ve);
    bool ok = false;
    const QTransform Rinv = R.inverted(&ok);
    const QPointF dl = ok ? (Rinv.map(deltaWorld) - Rinv.map(QPointF(0, 0))) : deltaWorld;

    Rectangle newBounds = resizemath::resizeSolve(
        m_dragHandle, m_dragOrigBounds, R, dl.x(), dl.y(),
        m_dragAnchorTitle, m_dragOrigCenterTitle, m_dragParentOffset,
        (mods & Qt::ShiftModifier) != 0, (mods & Qt::ControlModifier) != 0);

    const bool identity = ve->GetRotation() == 0.0f && ve->GetShearX() == 0.0f && ve->GetShearY() == 0.0f;
    if (identity && m_snappingEnabled && !(mods & Qt::ControlModifier)) {
        const double ox = m_dragOrigBounds.x, oy = m_dragOrigBounds.y;
        const double ow = m_dragOrigBounds.width, oh = m_dragOrigBounds.height;

        const double threshold = 8.0 * titleW() / letterboxRect().width();
        QList<double> candX = {0.0, titleW()/2.0, double(titleW())};
        QList<double> candY = {0.0, titleH()/2.0, double(titleH())};
        appendGuideCandidates(candX, candY);
        for (int i = 1; i < (int)t.elements.size(); ++i) {
            if (i == m_dragEi) continue;
            const auto* ove = dynamic_cast<const VisualElement*>(t.elements[i].get());
            if (!ove) continue;
            const Rectangle& ob = ove->GetBounds();
            candX << ob.x << ob.x+ob.width/2.0 << ob.x+ob.width;
            candY << ob.y << ob.y+ob.height/2.0 << ob.y+ob.height;
        }

        m_snapLinesX.clear();
        m_snapLinesY.clear();
        auto snapX = [&](double& edge) { double s = snapEdge(edge,candX,threshold); if(s!=edge){m_snapLinesX<<s;edge=s;} };
        auto snapY = [&](double& edge) { double s = snapEdge(edge,candY,threshold); if(s!=edge){m_snapLinesY<<s;edge=s;} };

        switch (m_dragHandle) {
        case 0: snapX(newBounds.x); newBounds.width=ox+ow-newBounds.x; snapY(newBounds.y); newBounds.height=oy+oh-newBounds.y; break;
        case 1: snapY(newBounds.y); newBounds.height=oy+oh-newBounds.y; break;
        case 2: { double r=newBounds.x+newBounds.width; snapX(r); newBounds.width=r-newBounds.x; snapY(newBounds.y); newBounds.height=oy+oh-newBounds.y; break; }
        case 3: snapX(newBounds.x); newBounds.width=ox+ow-newBounds.x; break;
        case 4: { double r=newBounds.x+newBounds.width; snapX(r); newBounds.width=r-newBounds.x; break; }
        case 5: { snapX(newBounds.x); newBounds.width=ox+ow-newBounds.x; double b2=newBounds.y+newBounds.height; snapY(b2); newBounds.height=b2-newBounds.y; break; }
        case 6: { double b2=newBounds.y+newBounds.height; snapY(b2); newBounds.height=b2-newBounds.y; break; }
        case 7: { double r=newBounds.x+newBounds.width; snapX(r); newBounds.width=r-newBounds.x; double b2=newBounds.y+newBounds.height; snapY(b2); newBounds.height=b2-newBounds.y; break; }
        default: break;
        }
    } else {
        m_snapLinesX.clear();
        m_snapLinesY.clear();
    }

    const int ei = m_dragEi;
    m_doc->applyMutation([ei, newBounds](Title& tt) {
        if (ei >= 1 && ei < (int)tt.elements.size()) {
            auto* sp = dynamic_cast<Spatial*>(tt.elements[ei].get());
            if (sp) sp->SetBounds(newBounds);
        }
    });
}

void CanvasWidget::applyRotateDrag(QPointF widgetPos, Qt::KeyboardModifiers mods)
{
    const Title& t = m_doc->title();
    if (m_dragEi < 1 || m_dragEi >= (int)t.elements.size()) return;
    const auto* ve = dynamic_cast<const VisualElement*>(t.elements[m_dragEi].get());
    if (!ve) return;
    const QPointF sp = widgetToTitle(widgetPos);
    const double curAngle = std::atan2(sp.y() - m_dragOrigCenterTitle.y(),
                                       sp.x() - m_dragOrigCenterTitle.x());
    const double newRot = resizemath::rotateSolve(m_dragOrigRotation, m_dragStartAngle,
                                                  curAngle, (mods & Qt::ShiftModifier) != 0);
    const int ei = m_dragEi;
    m_doc->applyMutation([ei, newRot](Title& tt) {
        if (ei >= 1 && ei < (int)tt.elements.size()) {
            auto* sp2 = dynamic_cast<Spatial*>(tt.elements[ei].get());
            if (sp2) sp2->SetRotation(static_cast<float>(newRot));
        }
    });
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        m_panOffset = m_panStartOffset + (event->position() - m_panStart);
        update();
        return;
    }

    if (m_marqueeActive) {
        m_marqueeCurWidget = event->position();
        update();
        event->accept();
        return;
    }

    if (!m_dragging || m_previewTitle) {
        if (!m_previewTitle)
            updateCursorForPos(event->position());
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPointF wpos = event->position();
    const QPointF titlePt = widgetToTitle(wpos);
    m_lastDragWidgetPos = wpos;

    if (m_dragMode == DragMode::Resize) {
        applyResizeDrag(wpos, event->modifiers());
        return;
    }

    if (m_dragMode == DragMode::Rotate) {
        applyRotateDrag(wpos, event->modifiers());
        return;
    }

    // Move drag
    Rectangle newBounds = m_dragOrigBounds;
    newBounds.x = m_dragOrigBounds.x + (titlePt.x() - m_dragStartTitle.x());
    newBounds.y = m_dragOrigBounds.y + (titlePt.y() - m_dragStartTitle.y());

    if (m_snappingEnabled)
        newBounds = applySnapping(newBounds, m_dragEi);
    else {
        m_snapLinesX.clear();
        m_snapLinesY.clear();
    }

    if (m_dragGroup.empty()) {
        const int ei = m_dragEi;
        m_doc->applyMutation([ei, newBounds](Title& t) {
            if (ei >= 1 && ei < (int)t.elements.size()) {
                auto* sp = dynamic_cast<Spatial*>(t.elements[ei].get());
                if (sp) sp->SetBounds(newBounds);
            }
        });
    } else {
        // Group move: apply the same (snapped) delta — derived from the
        // active element's actual result — to every member, relative to its
        // own captured orig bounds. The group snaps as a unit relative to
        // the active element.
        const double dxA = newBounds.x - m_dragOrigBounds.x;
        const double dyA = newBounds.y - m_dragOrigBounds.y;
        auto group = m_dragGroup;  // capture by value
        m_doc->applyMutation([group, dxA, dyA](Title& t) {
            for (const auto& [gi, orig] : group) {
                if (gi < 1 || gi >= (int)t.elements.size()) continue;
                auto* sp = dynamic_cast<Spatial*>(t.elements[gi].get());
                if (!sp) continue;
                Rectangle nb = orig;
                nb.x = orig.x + dxA;
                nb.y = orig.y + dyA;
                sp->SetBounds(nb);
            }
        });
    }
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    if (m_marqueeActive) {
        m_marqueeActive = false;
        const QPointF wpos = event->position();
        const QRectF r = QRectF(m_marqueeStartWidget, wpos).normalized();
        const double kClickPx = 3.0;
        if (r.width() < kClickPx && r.height() < kClickPx) {
            // Treated as a plain click on empty canvas.
            if (!m_marqueeAdditive) m_editorState->setSelection(SelectionId{});
        } else {
            std::vector<int> inside = elementsInMarquee(r);
            if (m_marqueeAdditive) {
                // Union with the current selection, preserving current members first.
                std::vector<int> cur = m_editorState->selectedIndices();
                for (int i : inside)
                    if (std::find(cur.begin(), cur.end(), i) == cur.end())
                        cur.push_back(i);
                inside.swap(cur);
            }
            if (inside.empty()) {
                if (!m_marqueeAdditive) m_editorState->setSelection(SelectionId{});
            } else {
                m_editorState->setMultiSelection(inside, inside.back());  // active = a member (contract-safe)
            }
        }
        update();
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton || !m_dragging || m_previewTitle) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_dragging = false;
    emit interactiveEditFinished();
    m_snapLinesX.clear();
    m_snapLinesY.clear();

    const Title& t = m_doc->title();
    if (m_dragEi < 1 || m_dragEi >= (int)t.elements.size()) {
        m_dragMode = DragMode::None;
        m_dragGroup.clear();
        return;
    }

    const auto* ve = dynamic_cast<const VisualElement*>(t.elements[m_dragEi].get());
    if (!ve) {
        m_dragMode = DragMode::None;
        m_dragGroup.clear();
        return;
    }

    if (m_dragMode == DragMode::Rotate) {
        const float finalRot = ve->GetRotation();
        const std::string eid = ve->GetId();
        if (finalRot != m_dragOrigRotation) {
            // revert to orig so SetElementRotationCmd captures the correct before-value
            const int ei = m_dragEi;
            const float orig = m_dragOrigRotation;
            m_doc->applyMutation([ei, orig](Title& tt) {
                if (ei >= 1 && ei < (int)tt.elements.size()) {
                    auto* sp2 = dynamic_cast<Spatial*>(tt.elements[ei].get());
                    if (sp2) sp2->SetRotation(orig);
                }
            });
            m_doc->undoStack()->push(new SetElementRotationCmd(m_doc, eid, finalRot, -1));
        }
        m_dragMode = DragMode::None;
        m_dragHandle = -1;
        m_dragEi = -1;
        m_dragGroup.clear();
        updateCursorForPos(event->position());
        return;
    }

    if (m_dragGroup.empty()) {
        const Rectangle finalBounds = ve->GetBounds();
        const std::string eid = ve->GetId();

        if (finalBounds.x != m_dragOrigBounds.x || finalBounds.y != m_dragOrigBounds.y ||
            finalBounds.width != m_dragOrigBounds.width || finalBounds.height != m_dragOrigBounds.height) {
            // Revert to original so SetElementBoundsCmd captures the correct before-value
            const int ei = m_dragEi;
            const Rectangle orig = m_dragOrigBounds;
            m_doc->applyMutation([ei, orig](Title& t) {
                if (ei >= 1 && ei < (int)t.elements.size()) {
                    auto* sp = dynamic_cast<Spatial*>(t.elements[ei].get());
                    if (sp) sp->SetBounds(orig);
                }
            });
            m_doc->undoStack()->push(new SetElementBoundsCmd(m_doc, eid, finalBounds));
        }
    } else {
        // Group move commit: gather (id, origBounds, finalBounds) for every
        // member whose bounds actually changed, then revert+push each under
        // a single undo macro so the whole group move is ONE undo step.
        struct Member { std::string id; Rectangle orig; Rectangle final_; };
        std::vector<Member> changed;
        for (const auto& [gi, orig] : m_dragGroup) {
            if (gi < 1 || gi >= (int)t.elements.size()) continue;
            const auto* gve = dynamic_cast<const VisualElement*>(t.elements[gi].get());
            if (!gve) continue;
            const Rectangle final_ = gve->GetBounds();
            if (final_.x != orig.x || final_.y != orig.y ||
                final_.width != orig.width || final_.height != orig.height) {
                changed.push_back({gve->GetId(), orig, final_});
            }
        }

        if (!changed.empty()) {
            m_doc->undoStack()->beginMacro("Move elements");
            for (const auto& m : changed) {
                const std::string id = m.id;
                const Rectangle orig = m.orig;
                m_doc->applyMutation([id, orig](Title& tt) {
                    for (int i = 1; i < (int)tt.elements.size(); ++i) {
                        if (tt.elements[i]->GetId() != id) continue;
                        auto* sp = dynamic_cast<Spatial*>(tt.elements[i].get());
                        if (sp) sp->SetBounds(orig);
                        break;
                    }
                });
                m_doc->undoStack()->push(new SetElementBoundsCmd(m_doc, m.id, m.final_));
            }
            m_doc->undoStack()->endMacro();
        }
    }

    m_dragMode = DragMode::None;
    m_dragHandle = -1;
    m_dragEi = -1;
    m_dragGroup.clear();
    updateCursorForPos(event->position());
}

void CanvasWidget::leaveEvent(QEvent* event)
{
    unsetCursor();
    QWidget::leaveEvent(event);
}

// ── Drag and drop ─────────────────────────────────────────────────────────────

static bool mimeHasImageFile(const QMimeData* md)
{
    if (!md || !md->hasUrls()) return false;
    for (const QUrl& url : md->urls()) {
        if (!url.isLocalFile()) continue;
        if (kCanvasImageExts.contains(QFileInfo(url.toLocalFile()).suffix().toLower()))
            return true;
    }
    return false;
}

void CanvasWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (mimeHasImageFile(event->mimeData())) event->acceptProposedAction();
    else event->ignore();
}

void CanvasWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (mimeHasImageFile(event->mimeData())) event->acceptProposedAction();
    else event->ignore();
}

void CanvasWidget::dropEvent(QDropEvent* event)
{
    if (m_previewTitle) { event->ignore(); return; }

    const QMimeData* md = event->mimeData();
    if (!md || !md->hasUrls()) { event->ignore(); return; }

    QStringList imagePaths;
    for (const QUrl& url : md->urls()) {
        if (!url.isLocalFile()) continue;
        QString p = url.toLocalFile();
        if (kCanvasImageExts.contains(QFileInfo(p).suffix().toLower()))
            imagePaths << p;
    }
    if (imagePaths.isEmpty()) { event->ignore(); return; }

    const QPointF titlePt = widgetToTitle(event->position());
    const bool needsMacro = imagePaths.size() > 1;
    if (needsMacro)
        m_doc->undoStack()->beginMacro("Drop Images");

    std::string firstNewId;
    for (int idx = 0; idx < imagePaths.size(); ++idx) {
        const QString& imagePath = imagePaths[idx];
        QImage img(imagePath);

        // Find unique element ID
        int maxN = 0;
        const Title& t = m_doc->title();
        for (int i = 1; i < (int)t.elements.size(); ++i) {
            const std::string& eid = t.elements[i]->GetId();
            if (eid.rfind("image_", 0) == 0)
                try { maxN = std::max(maxN, std::stoi(eid.substr(6))); } catch (...) {}
        }
        const std::string newId = "image_" + std::to_string(maxN + 1);
        if (idx == 0) firstNewId = newId;

        double w = img.isNull() ? 200.0 : img.width();
        double h = img.isNull() ? 200.0 : img.height();
        if (w > titleW()) { h = h * titleW() / w; w = titleW(); }
        if (h > titleH()) { w = w * titleH() / h; h = titleH(); }

        const double x = titlePt.x() - w / 2.0 + idx * 20.0;
        const double y = titlePt.y() - h / 2.0 + idx * 20.0;
        const int zOrder = static_cast<int>(m_doc->title().elements.size()) - 1;

        using json = nlohmann::json;
        json j = {{"id", newId}, {"type", "image"},
                  {"x", x}, {"y", y}, {"w", w}, {"h", h},
                  {"z_order", zOrder},
                  {"image_path", imagePath.toStdString()},
                  {"scale_mode", "contain"}};
        m_doc->undoStack()->push(new AddElementCmd(m_doc, std::move(j)));
    }

    if (needsMacro)
        m_doc->undoStack()->endMacro();

    // Select the first created element
    if (!firstNewId.empty()) {
        const Title& t = m_doc->title();
        for (int i = 1; i < (int)t.elements.size(); ++i) {
            if (t.elements[i]->GetId() == firstNewId) {
                m_editorState->setSelection({SelectionId::Level::Element, i});
                break;
            }
        }
    }

    event->acceptProposedAction();
}

// Pick a bidirectional resize cursor from a handle's outward direction (widget space, y-down).
static Qt::CursorShape resizeCursorForAngle(double dx, double dy)
{
    double a = std::atan2(dy, dx) * 180.0 / M_PI;   // (-180,180]
    a = std::fmod(a + 180.0, 180.0);                 // fold to [0,180) — cursor is bidirectional
    if (a < 22.5 || a >= 157.5) return Qt::SizeHorCursor;
    if (a < 67.5)               return Qt::SizeFDiagCursor;   // down-right / up-left  "\"
    if (a < 112.5)              return Qt::SizeVerCursor;
    return Qt::SizeBDiagCursor;                              // down-left / up-right  "/"
}

void CanvasWidget::updateCursorForPos(QPointF wpos)
{
    if (m_paintModeActive) return; // cursor already set by setPaintMode(); don't override

    const SelectionId sel = m_editorState->selection();
    const VisualElement* selVe = nullptr;
    if (sel.level == SelectionId::Level::Element) {
        const Title& t = m_doc->title();
        if (sel.elementIndex >= 1 && sel.elementIndex < (int)t.elements.size())
            selVe = dynamic_cast<const VisualElement*>(t.elements[sel.elementIndex].get());
    }

    const int handle = (m_editorState->selectionCount() <= 1 && !isActiveSelectionLocked())
                       ? hitHandle(wpos) : -1;
    if (handle == SelectionHandles::kRotateHandle) {
        setCursor(Qt::CrossCursor);   // Qt has no native rotate cursor; CrossCursor is the stand-in
        return;
    }
    if (handle >= 0 && selVe) {
        const QTransform xf = elementToWidgetTransform(*selVe);
        const QPointF center = xf.map(localBoundsRect(*selVe).center());
        const QPointF hc = SelectionHandles::handleCenterT(handle, xf, localBoundsRect(*selVe));
        setCursor(resizeCursorForAngle(hc.x() - center.x(), hc.y() - center.y()));
        return;
    }

    if (selVe) {
        bool ok = false;
        const QTransform inv = elementLocalToTitle(*selVe).inverted(&ok);
        if (ok) {
            const QPointF lp = inv.map(widgetToTitle(wpos));
            const Rectangle b = selVe->GetBounds();
            if (lp.x() >= 0.0 && lp.x() <= b.width && lp.y() >= 0.0 && lp.y() <= b.height) {
                setCursor(Qt::SizeAllCursor);
                return;
            }
        }
    }

    unsetCursor();
}
