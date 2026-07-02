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
#include <QPen>
#include <QPixmap>
#include <QResizeEvent>
#include <QTimer>
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
        const Rectangle gb = globalBounds(*ve);
        if (titlePt.x() >= gb.x && titlePt.x() <= gb.x + gb.width &&
            titlePt.y() >= gb.y && titlePt.y() <= gb.y + gb.height) {
            return {SelectionId::Level::Element, ei};
        }
    }
    return {};
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
    return SelectionHandles::hitTest(widgetPt, titleToWidget(globalBounds(*ve)));
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
    const SelectionId sel = m_editorState->selection();
    const Title& t = m_doc->title();
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setBrush(Qt::NoBrush);

    for (int i = 1; i < (int)t.elements.size(); ++i) {
        if (i == sel.elementIndex) continue;
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[i].get());
        if (!ve) continue;
        const QRectF r = titleToWidget(globalBounds(*ve));
        p.setPen(QPen(QColor(0, 0, 0, 84), 3.0, Qt::DashLine));
        p.drawRect(r);
        p.setPen(QPen(QColor(255, 255, 255, 178), 1.0, Qt::DashLine));
        p.drawRect(r);
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

    if (!m_previewTitle) {
        const SelectionId sel = m_editorState->selection();
        if (sel.level == SelectionId::Level::Element) {
            const Title& t = m_doc->title();
            if (sel.elementIndex >= 1 && sel.elementIndex < (int)t.elements.size()) {
                const auto* ve = dynamic_cast<const VisualElement*>(t.elements[sel.elementIndex].get());
                if (ve)
                    SelectionHandles::draw(p, titleToWidget(globalBounds(*ve)),
                                           m_dragging ? m_dragHandle : -1,
                                           palette().highlight().color());
            }
        }
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
    if (m_dragging && m_dragMode == DragMode::Resize) {
        Qt::KeyboardModifiers mods = event->modifiers();
        switch (event->key()) {
        case Qt::Key_Shift:   mods &= ~Qt::ShiftModifier;   break;
        case Qt::Key_Control: mods &= ~Qt::ControlModifier; break;
        case Qt::Key_Alt:     mods &= ~Qt::AltModifier;     break;
        default: break;
        }
        applyResizeDrag(m_lastDragWidgetPos, mods);
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void CanvasWidget::keyPressEvent(QKeyEvent* event)
{
    if (m_dragging && m_dragMode == DragMode::Resize) {
        applyResizeDrag(m_lastDragWidgetPos, event->modifiers());
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

    const int ei = sel.elementIndex;
    const Title& t = m_doc->title();
    if (ei < 1 || ei >= (int)t.elements.size()) return;

    const auto* ve = dynamic_cast<const VisualElement*>(t.elements[ei].get());
    if (!ve) return;

    Rectangle b = ve->GetBounds();
    b.x += dx;
    b.y += dy;
    m_doc->undoStack()->push(new SetElementBoundsCmd(m_doc, ve->GetId(), b, kArrowMergeTag));
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

    // 1. Resize handle hit?
    const int handle = hitHandle(wpos);
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
        return;
    }

    const SelectionId curSel = m_editorState->selection();
    const QPointF sp = widgetToTitle(wpos);

    // 2. Always hit-test (top-most element under cursor wins), then decide
    //    whether to keep dragging the current selection or select the new hit.
    const SelectionId hit = hitTest(sp);

    if (hit.level != SelectionId::Level::Element) {
        m_editorState->setSelection(hit);
        return;
    }

    if (!(hit == curSel))
        m_editorState->setSelection(hit);

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
    }
}

void CanvasWidget::applyResizeDrag(QPointF widgetPos, Qt::KeyboardModifiers mods)
{
    const QPointF titlePt = widgetToTitle(widgetPos);
    const double dx = titlePt.x() - m_dragStartTitle.x();
    const double dy = titlePt.y() - m_dragStartTitle.y();
    const double ox = m_dragOrigBounds.x, oy = m_dragOrigBounds.y;
    const double ow = m_dragOrigBounds.width, oh = m_dragOrigBounds.height;
    Rectangle newBounds = m_dragOrigBounds;

    switch (m_dragHandle) {
    case 0: newBounds.x = ox+dx; newBounds.y = oy+dy; newBounds.width = std::max(1.0,ow-dx); newBounds.height = std::max(1.0,oh-dy); break;
    case 1: newBounds.y = oy+dy; newBounds.height = std::max(1.0,oh-dy); break;
    case 2: newBounds.y = oy+dy; newBounds.width = std::max(1.0,ow+dx); newBounds.height = std::max(1.0,oh-dy); break;
    case 3: newBounds.x = ox+dx; newBounds.width = std::max(1.0,ow-dx); break;
    case 4: newBounds.width = std::max(1.0,ow+dx); break;
    case 5: newBounds.x = ox+dx; newBounds.width = std::max(1.0,ow-dx); newBounds.height = std::max(1.0,oh+dy); break;
    case 6: newBounds.height = std::max(1.0,oh+dy); break;
    case 7: newBounds.width = std::max(1.0,ow+dx); newBounds.height = std::max(1.0,oh+dy); break;
    default: break;
    }

    if ((mods & Qt::ShiftModifier) && oh > 0 &&
        (m_dragHandle == 0 || m_dragHandle == 2 || m_dragHandle == 5 || m_dragHandle == 7)) {
        const double scale = std::min(newBounds.width / ow, newBounds.height / oh);
        newBounds.width = std::max(1.0, ow * scale);
        newBounds.height = std::max(1.0, oh * scale);
        switch (m_dragHandle) {
        case 0: newBounds.x = ox+ow-newBounds.width; newBounds.y = oy+oh-newBounds.height; break;
        case 2: newBounds.x = ox; newBounds.y = oy+oh-newBounds.height; break;
        case 5: newBounds.x = ox+ow-newBounds.width; newBounds.y = oy; break;
        case 7: newBounds.x = ox; newBounds.y = oy; break;
        default: break;
        }
    }

    if (mods & Qt::ControlModifier) {
        const double cx = ox + ow / 2.0, cy = oy + oh / 2.0;
        newBounds.width = std::max(1.0, 2.0*newBounds.width - ow);
        newBounds.height = std::max(1.0, 2.0*newBounds.height - oh);
        newBounds.x = cx - newBounds.width / 2.0;
        newBounds.y = cy - newBounds.height / 2.0;
    }

    if (m_snappingEnabled) {
        const double threshold = 8.0 * titleW() / letterboxRect().width();
        QList<double> candX = {0.0, titleW()/2.0, double(titleW())};
        QList<double> candY = {0.0, titleH()/2.0, double(titleH())};
        appendGuideCandidates(candX, candY);
        const Title& t = m_doc->title();
        for (int i = 1; i < (int)t.elements.size(); ++i) {
            if (i == m_dragEi) continue;
            const auto* ve = dynamic_cast<const VisualElement*>(t.elements[i].get());
            if (!ve) continue;
            const Rectangle& ob = ve->GetBounds();
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
    m_doc->applyMutation([ei, newBounds](Title& t) {
        if (ei >= 1 && ei < (int)t.elements.size()) {
            auto* sp = dynamic_cast<Spatial*>(t.elements[ei].get());
            if (sp) sp->SetBounds(newBounds);
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

    const int ei = m_dragEi;
    m_doc->applyMutation([ei, newBounds](Title& t) {
        if (ei >= 1 && ei < (int)t.elements.size()) {
            auto* sp = dynamic_cast<Spatial*>(t.elements[ei].get());
            if (sp) sp->SetBounds(newBounds);
        }
    });
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton || !m_dragging || m_previewTitle) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_dragging = false;
    m_snapLinesX.clear();
    m_snapLinesY.clear();

    const Title& t = m_doc->title();
    if (m_dragEi < 1 || m_dragEi >= (int)t.elements.size()) {
        m_dragMode = DragMode::None;
        return;
    }

    const auto* ve = dynamic_cast<const VisualElement*>(t.elements[m_dragEi].get());
    if (!ve) {
        m_dragMode = DragMode::None;
        return;
    }

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

    m_dragMode = DragMode::None;
    m_dragHandle = -1;
    m_dragEi = -1;
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

void CanvasWidget::updateCursorForPos(QPointF wpos)
{
    if (m_paintModeActive) return; // cursor already set by setPaintMode(); don't override

    static const Qt::CursorShape kHandleCursors[8] = {
        Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor,
        Qt::SizeHorCursor, Qt::SizeBDiagCursor, Qt::SizeVerCursor, Qt::SizeFDiagCursor};

    const int handle = hitHandle(wpos);
    if (handle >= 0) { setCursor(kHandleCursors[handle]); return; }

    const SelectionId sel = m_editorState->selection();
    if (sel.level == SelectionId::Level::Element) {
        const QPointF sp = widgetToTitle(wpos);
        const Title& t = m_doc->title();
        if (sel.elementIndex >= 1 && sel.elementIndex < (int)t.elements.size()) {
            const auto* ve = dynamic_cast<const VisualElement*>(t.elements[sel.elementIndex].get());
            if (ve) {
                const Rectangle gb = globalBounds(*ve);
                if (sp.x() >= gb.x && sp.x() <= gb.x + gb.width &&
                    sp.y() >= gb.y && sp.y() <= gb.y + gb.height) {
                    setCursor(Qt::SizeAllCursor);
                    return;
                }
            }
        }
    }

    unsetCursor();
}
