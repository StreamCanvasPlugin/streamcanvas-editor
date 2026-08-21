#include "painteditor.h"
#include "ui/widgets/BrandColorSwatchGrid.h"
#include "ui/UiUtils.h"
#include "icons.h"
#include "model/TitleDocument.h"
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

// ScaleMode combo display order.
static const ScaleMode kScaleModeOrder[] = {
    ScaleMode::None, ScaleMode::Contain,  ScaleMode::Cover, ScaleMode::FitHeight,
    ScaleMode::FitWidth, ScaleMode::Stretch, ScaleMode::Tile,
};
static const QStringList kScaleModeLabels = {
    "None", "Contain", "Cover", "Fit Height", "Fit Width", "Stretch", "Tile",
};
static int scaleModeToComboIndex(ScaleMode m)
{
    for (int i = 0; i < (int)std::size(kScaleModeOrder); ++i)
        if (kScaleModeOrder[i] == m) return i;
    return 0;
}
static ScaleMode comboIndexToScaleMode(int i)
{
    if (i >= 0 && i < (int)std::size(kScaleModeOrder)) return kScaleModeOrder[i];
    return ScaleMode::None;
}

// Converts a Paint's engine-side stops to the Qt-side stop list.
static QVector<GradientStop> stopsFromPaint(const Paint& paint)
{
    QVector<GradientStop> stops;
    stops.reserve((int)paint.stops.size());
    for (const auto& st : paint.stops)
        stops.push_back(GradientStop{st.offset, QColor::fromRgbF(st.r, st.g, st.b, st.a)});
    return stops;
}

// Sensible starting point for a type that has never been authored in this session.
static Paint defaultPaintForType(Paint::Type type)
{
    Paint p;
    switch (type) {
    case Paint::Type::None:
        break;
    case Paint::Type::Solid:
        p = Paint::Solid(1.0, 1.0, 1.0, 1.0);
        break;
    case Paint::Type::Linear:
        p = Paint::Linear(0.1, 0.5, 0.9, 0.5);
        p.AddStop(0.0, 0.0, 0.0, 0.0, 1.0);
        p.AddStop(1.0, 1.0, 1.0, 1.0, 1.0);
        break;
    case Paint::Type::Radial:
        p = Paint::Radial(0.5, 0.5, 0.0, 0.5, 0.5, 0.5);
        p.AddStop(0.0, 1.0, 1.0, 1.0, 1.0);
        p.AddStop(1.0, 0.0, 0.0, 0.0, 1.0);
        break;
    case Paint::Type::Image:
        p.type = Paint::Type::Image;
        break;
    }
    return p;
}

// Pushes the linear editor's derived angle/length into the spin boxes without
// re-triggering their valueChanged handlers.
static void syncLinearNumericFromEditor(QDoubleSpinBox* angle, QDoubleSpinBox* length,
                                         LinearGradientEditor* editor)
{
    QSignalBlocker ba(angle);
    QSignalBlocker bl(length);
    angle->setValue(editor->angleDegrees());
    length->setValue(editor->lengthFraction() * 100.0);
}

// Pushes the radial editor's center/radius into the spin boxes without
// re-triggering their valueChanged handlers.
static void syncRadialNumericFromEditor(QDoubleSpinBox* cx, QDoubleSpinBox* cy,
                                         QDoubleSpinBox* radius, RadialGradientEditor* editor)
{
    QSignalBlocker bx(cx);
    QSignalBlocker by(cy);
    QSignalBlocker br(radius);
    cx->setValue(editor->center().x() * 100.0);
    cy->setValue(editor->center().y() * 100.0);
    radius->setValue(editor->radius() * 100.0);
}

// Shows/hides the "path does not exist" warning under the image path field.
static void refreshImageErrorLabel(QLineEdit* pathEdit, QLabel* errorLabel)
{
    const QString path = pathEdit->text();
    errorLabel->setVisible(!path.isEmpty() && !QFileInfo::exists(path));
}

static QWidget* makePlaceholderPage(const QString& text)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(
        QString("color: %1;").arg(label->palette().color(QPalette::PlaceholderText).name()));
    layout->addWidget(label);
    layout->addStretch();
    return page;
}

PaintEditor::PaintEditor(TitleDocument* doc, QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ── Type selector ────────────────────────────────────────────────────
    auto* typeRow = new QWidget;
    auto* typeLayout = new QHBoxLayout(typeRow);
    typeLayout->setContentsMargins(0, 0, 0, 0);
    typeLayout->setSpacing(2);

    m_typeGroup = new QButtonGroup(this);
    m_typeGroup->setExclusive(true);

    struct TypeButtonSpec {
        Paint::Type type;
        Icons16 icon;
        QString label;
    };
    const TypeButtonSpec kTypeButtons[] = {
        {Paint::Type::None, Icons16::Action_Close, "None"},
        {Paint::Type::Solid, Icons16::Shape_SquareFilled, "Solid"},
        {Paint::Type::Linear, Icons16::Navigation_ArrowRight, "Linear"},
        {Paint::Type::Radial, Icons16::Shape_CircleFilled, "Radial"},
        {Paint::Type::Image, Icons16::File_Picture, "Image"},
    };
    for (const auto& spec : kTypeButtons) {
        auto* btn = makeIconToolButton(themedIcon(spec.icon), spec.label, /*checkable*/ true);
        btn->setText(spec.label);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_typeGroup->addButton(btn, static_cast<int>(spec.type));
        typeLayout->addWidget(btn);
    }
    typeLayout->addStretch();
    mainLayout->addWidget(typeRow);

    connect(m_typeGroup, &QButtonGroup::idClicked, this, &PaintEditor::onTypeButtonClicked);

    // ── Main stack: one page per type ───────────────────────────────────
    m_mainSelector = new QStackedWidget;

    m_mainSelector->addWidget(makePlaceholderPage("No fill or stroke is applied."));  // None
    m_mainSelector->addWidget(makePlaceholderPage("Choose a solid color below."));    // Solid

    // Linear page
    {
        auto* page = new QWidget;
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);

        m_linearEditor = new LinearGradientEditor;
        pageLayout->addWidget(m_linearEditor, 1);

        auto* presetRow = new QWidget;
        auto* presetLayout = new QHBoxLayout(presetRow);
        presetLayout->setContentsMargins(0, 0, 0, 0);
        auto* hBtn = makeIconToolButton(themedIcon(Icons16::Navigation_ArrowRight), "Horizontal");
        auto* vBtn = makeIconToolButton(themedIcon(Icons16::Navigation_ArrowDown), "Vertical");
        auto* ddBtn =
            makeIconToolButton(themedIcon(Icons16::Navigation_ArrowDownRight), "Diagonal Down");
        auto* duBtn =
            makeIconToolButton(themedIcon(Icons16::Navigation_ArrowUpRight), "Diagonal Up");
        auto* revBtn = makeIconToolButton(themedIcon(Icons16::Action_Swap), "Reverse");
        for (auto* b : {hBtn, vBtn, ddBtn, duBtn, revBtn})
            presetLayout->addWidget(b);
        presetLayout->addStretch();
        pageLayout->addWidget(presetRow);

        auto* numRow = new QWidget;
        GridBuilder gb(numRow);
        m_lnAngle = makeSpinBox(0.0, 360.0, 1, QStringLiteral("°"));
        m_lnAngle->setWrapping(true);
        m_lnLength = makeSpinBox(0.0, 200.0, 1, QStringLiteral("%"));
        gb.addWidget(makeTinyLabel("Angle"), 0, 0)
            .addWidget(m_lnAngle, 1, 0)
            .addWidget(makeTinyLabel("Length"), 0, 1)
            .addWidget(m_lnLength, 1, 1);
        pageLayout->addWidget(numRow);

        m_mainSelector->addWidget(page);  // Linear

        connect(hBtn, &QToolButton::clicked, this, [this]() {
            m_linearEditor->setHorizontal();
            syncLinearNumericFromEditor(m_lnAngle, m_lnLength, m_linearEditor);
            emitPaint();
        });
        connect(vBtn, &QToolButton::clicked, this, [this]() {
            m_linearEditor->setVertical();
            syncLinearNumericFromEditor(m_lnAngle, m_lnLength, m_linearEditor);
            emitPaint();
        });
        connect(ddBtn, &QToolButton::clicked, this, [this]() {
            m_linearEditor->setDiagonalDown();
            syncLinearNumericFromEditor(m_lnAngle, m_lnLength, m_linearEditor);
            emitPaint();
        });
        connect(duBtn, &QToolButton::clicked, this, [this]() {
            m_linearEditor->setDiagonalUp();
            syncLinearNumericFromEditor(m_lnAngle, m_lnLength, m_linearEditor);
            emitPaint();
        });
        connect(revBtn, &QToolButton::clicked, this, [this]() {
            m_linearEditor->reverse();
            syncLinearNumericFromEditor(m_lnAngle, m_lnLength, m_linearEditor);
            emitPaint();
        });

        connect(m_lnAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double v) {
                    if (m_updating) return;
                    m_linearEditor->setAngleDegrees(v);
                    emitPaint();
                });
        connect(m_lnLength, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double v) {
                    if (m_updating) return;
                    m_linearEditor->setLengthFraction(v / 100.0);
                    emitPaint();
                });

        connect(m_linearEditor, &LinearGradientEditor::p1Changed, this, [this](QPointF) {
            syncLinearNumericFromEditor(m_lnAngle, m_lnLength, m_linearEditor);
            emitPaint();
        });
        connect(m_linearEditor, &LinearGradientEditor::p2Changed, this, [this](QPointF) {
            syncLinearNumericFromEditor(m_lnAngle, m_lnLength, m_linearEditor);
            emitPaint();
        });
        connect(m_linearEditor, &LinearGradientEditor::stopSelected, this,
                &PaintEditor::onGeometryStopSelected);
        connect(m_linearEditor, &LinearGradientEditor::interactionStarted, this,
                &PaintEditor::beginInteraction);
        connect(m_linearEditor, &LinearGradientEditor::interactionFinished, this,
                &PaintEditor::endInteraction);
    }

    // Radial page
    {
        auto* page = new QWidget;
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);

        m_radialEditor = new RadialGradientEditor;
        pageLayout->addWidget(m_radialEditor, 1);

        auto* numRow = new QWidget;
        GridBuilder gb(numRow);
        m_radCenterX = makeSpinBox(0.0, 100.0, 1, QStringLiteral("%"));
        m_radCenterY = makeSpinBox(0.0, 100.0, 1, QStringLiteral("%"));
        m_radRadius = makeSpinBox(0.0, 200.0, 1, QStringLiteral("%"));
        gb.addWidget(makeTinyLabel("Center X"), 0, 0)
            .addWidget(m_radCenterX, 1, 0)
            .addWidget(makeTinyLabel("Center Y"), 0, 1)
            .addWidget(m_radCenterY, 1, 1)
            .addWidget(makeTinyLabel("Radius"), 0, 2)
            .addWidget(m_radRadius, 1, 2);
        pageLayout->addWidget(numRow);

        m_mainSelector->addWidget(page);  // Radial

        auto onCenterSpinChanged = [this](double) {
            if (m_updating) return;
            m_radialEditor->setCenter(
                QPointF(m_radCenterX->value() / 100.0, m_radCenterY->value() / 100.0));
            emitPaint();
        };
        connect(m_radCenterX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                onCenterSpinChanged);
        connect(m_radCenterY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                onCenterSpinChanged);
        connect(m_radRadius, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double v) {
                    if (m_updating) return;
                    m_radialEditor->setRadius(v / 100.0);
                    emitPaint();
                });

        connect(m_radialEditor, &RadialGradientEditor::centerChanged, this, [this](QPointF) {
            syncRadialNumericFromEditor(m_radCenterX, m_radCenterY, m_radRadius, m_radialEditor);
            emitPaint();
        });
        connect(m_radialEditor, &RadialGradientEditor::radiusChanged, this, [this](qreal) {
            syncRadialNumericFromEditor(m_radCenterX, m_radCenterY, m_radRadius, m_radialEditor);
            emitPaint();
        });
        connect(m_radialEditor, &RadialGradientEditor::stopSelected, this,
                &PaintEditor::onGeometryStopSelected);
        connect(m_radialEditor, &RadialGradientEditor::interactionStarted, this,
                &PaintEditor::beginInteraction);
        connect(m_radialEditor, &RadialGradientEditor::interactionFinished, this,
                &PaintEditor::endInteraction);
    }

    // Image page
    {
        auto* page = new QWidget;
        auto* gl = new QGridLayout(page);
        gl->setColumnStretch(1, 1);

        m_imagePathForPaint = new QLineEdit;
        m_imagePathForPaint->setPlaceholderText("Image path (PNG)…");

        auto* browseBtn = new QToolButton;
        browseBtn->setText("…");
        browseBtn->setToolTip("Browse for image file");

        m_paintScaleMode = new QComboBox;
        m_paintScaleMode->addItems(kScaleModeLabels);

        m_tileScale = makeSpinBox(0.01, 100.0, 2, QString());
        m_tileScale->setValue(1.0);
        m_tileScale->setEnabled(false);

        m_imageErrorLabel = new QLabel("Image file not found.");
        m_imageErrorLabel->setStyleSheet(
            QString("color: %1;").arg(m_imageErrorLabel->palette().color(QPalette::Text).name()));
        m_imageErrorLabel->hide();

        gl->addWidget(new QLabel("Path"), 0, 0);
        gl->addWidget(m_imagePathForPaint, 0, 1);
        gl->addWidget(browseBtn, 0, 2);
        gl->addWidget(new QLabel("Scale"), 1, 0);
        gl->addWidget(m_paintScaleMode, 1, 1, 1, 2);
        gl->addWidget(new QLabel("Tile Scale"), 2, 0);
        gl->addWidget(m_tileScale, 2, 1, 1, 2);
        gl->addWidget(m_imageErrorLabel, 3, 0, 1, 3);

        m_mainSelector->addWidget(page);  // Image

        connect(m_imagePathForPaint, &QLineEdit::editingFinished, this, [this]() {
            m_imageCacheDirty = true;   // user re-committed a path: always reload
            refreshImageErrorLabel(m_imagePathForPaint, m_imageErrorLabel);
            emitPaint();
        });
        connect(browseBtn, &QToolButton::clicked, this, [this]() {
            const QString path = QFileDialog::getOpenFileName(
                this, "Select Image", QString(), "PNG Images (*.png);;All Files (*)");
            if (path.isEmpty()) return;
            m_imageCacheDirty = true;   // user picked a file: always reload
            m_imagePathForPaint->setText(path);
            refreshImageErrorLabel(m_imagePathForPaint, m_imageErrorLabel);
            emitPaint();
        });
        connect(m_paintScaleMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int idx) {
                    if (m_updating) return;
                    m_tileScale->setEnabled(comboIndexToScaleMode(idx) == ScaleMode::Tile);
                    emitPaint();
                });
        connect(m_tileScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double) {
                    if (m_updating) return;
                    emitPaint();
                });
    }

    mainLayout->addWidget(m_mainSelector, 1);

    // ── Shared stop bar (Linear/Radial only) ────────────────────────────
    m_stopBar = new GradientStopBar;
    mainLayout->addWidget(m_stopBar);
    connect(m_stopBar, &GradientStopBar::stopsChanged, this, &PaintEditor::onStopsChanged);
    connect(m_stopBar, &GradientStopBar::stopSelected, this, &PaintEditor::onStopBarStopSelected);
    connect(m_stopBar, &GradientStopBar::interactionStarted, this, &PaintEditor::beginInteraction);
    connect(m_stopBar, &GradientStopBar::interactionFinished, this, &PaintEditor::endInteraction);

    // ── Shared colour section (Solid, or selected gradient stop) ───────
    m_colorPicker = new ColorPicker;
    mainLayout->addWidget(m_colorPicker);
    connect(m_colorPicker, &ColorPicker::colorChanged, this, &PaintEditor::onColorChanged);
    connect(m_colorPicker, &ColorPicker::colorChanged, this, [this](const QColor& c) {
        if (m_brandGrid) m_brandGrid->setCurrentColor(c);
    });

    if (doc) {
        auto* brandLabel = new QLabel("Brand Colors");
        brandLabel->setStyleSheet(
            QString("font-size: 10px; color: %1;")
                .arg(brandLabel->palette().color(QPalette::PlaceholderText).name()));
        mainLayout->addWidget(brandLabel);

        m_brandGrid = new BrandColorSwatchGrid(doc, BrandColorSwatchGrid::Mode::Full);
        mainLayout->addWidget(m_brandGrid);

        connect(m_brandGrid, &BrandColorSwatchGrid::colorSelected, this, [this](const QColor& c) {
            // setColor() is silent, so route the color into the current target
            // (solid fill or selected gradient stop) explicitly.
            m_colorPicker->setColor(c);
            onColorChanged(c);
        });
    }

    // Initial state: None, nothing checked-in yet.
    if (auto* noneBtn = m_typeGroup->button(static_cast<int>(Paint::Type::None)))
        noneBtn->setChecked(true);
    applyTypeUi(Paint::Type::None);
}

Paint PaintEditor::getPaint() const
{
    Paint paint{};

    switch (m_currentType) {
    case Paint::Type::None:
        break;
    case Paint::Type::Solid: {
        const QColor c = m_colorPicker->color();
        paint = Paint::Solid(c.redF(), c.greenF(), c.blueF(), c.alphaF());
        break;
    }
    case Paint::Type::Linear: {
        const QPointF p1 = m_linearEditor->p1();
        const QPointF p2 = m_linearEditor->p2();
        paint = Paint::Linear(p1.x(), p1.y(), p2.x(), p2.y());
        for (const auto& st : m_stopBar->stops())
            paint.AddStop(st.position, st.color.redF(), st.color.greenF(), st.color.blueF(),
                           st.color.alphaF());
        break;
    }
    case Paint::Type::Radial: {
        const QPointF c = m_radialEditor->center();
        paint = Paint::Radial(m_radialEditor->focusX(), m_radialEditor->focusY(),
                               m_radialEditor->focusR(), c.x(), c.y(), m_radialEditor->radius());
        for (const auto& st : m_stopBar->stops())
            paint.AddStop(st.position, st.color.redF(), st.color.greenF(), st.color.blueF(),
                           st.color.alphaF());
        break;
    }
    case Paint::Type::Image: {
        const QString path = m_imagePathForPaint->text();
        if (m_imageCacheDirty || path != m_cachedImagePath) {
            m_cachedImagePaint = Paint::Image(path.toStdString());
            m_cachedImagePath = path;
            m_imageCacheDirty = false;
        }
        paint = m_cachedImagePaint;
        paint.imageScaleMode = comboIndexToScaleMode(m_paintScaleMode->currentIndex());
        paint.tileScale = m_tileScale->value();
        break;
    }
    }

    return paint;
}

void PaintEditor::setPaint(const Paint& paint)
{
    UpdateGuard guard(m_updating);

    m_currentType = paint.type;
    m_paintByType[paint.type] = paint;

    if (auto* btn = m_typeGroup->button(static_cast<int>(paint.type))) btn->setChecked(true);

    switch (paint.type) {
    case Paint::Type::None:
        break;
    case Paint::Type::Solid: {
        const QColor c = QColor::fromRgbF(paint.params[0], paint.params[1], paint.params[2],
                                           paint.params[3]);
        QSignalBlocker block(m_colorPicker);
        m_colorPicker->setColor(c);
        break;
    }
    case Paint::Type::Linear: {
        m_linearEditor->setP1(QPointF(paint.params[0], paint.params[1]));
        m_linearEditor->setP2(QPointF(paint.params[2], paint.params[3]));
        const QVector<GradientStop> stops = stopsFromPaint(paint);
        m_linearEditor->setStops(stops);
        m_linearEditor->setSelectedStop(0);
        m_stopBar->setStops(stops);
        m_stopBar->setSelectedStop(0);
        syncLinearNumericFromEditor(m_lnAngle, m_lnLength, m_linearEditor);
        syncColorPickerToSelectedStop();
        break;
    }
    case Paint::Type::Radial: {
        m_radialEditor->setCenter(QPointF(paint.params[3], paint.params[4]));
        m_radialEditor->setRadius(paint.params[5]);
        m_radialEditor->setFocalPoint(paint.params[0], paint.params[1], paint.params[2]);
        const QVector<GradientStop> stops = stopsFromPaint(paint);
        m_radialEditor->setStops(stops);
        m_radialEditor->setSelectedStop(0);
        m_stopBar->setStops(stops);
        m_stopBar->setSelectedStop(0);
        syncRadialNumericFromEditor(m_radCenterX, m_radCenterY, m_radRadius, m_radialEditor);
        syncColorPickerToSelectedStop();
        break;
    }
    case Paint::Type::Image: {
        const QString path = QString::fromStdString(paint.imagePath);
        m_imagePathForPaint->setText(path);
        m_cachedImagePaint = paint;
        m_cachedImagePath = path;
        // The incoming paint already carries a decoded surface, and this is a
        // programmatic load, not a user file pick — no forced reload pending.
        m_imageCacheDirty = false;
        m_paintScaleMode->setCurrentIndex(scaleModeToComboIndex(paint.imageScaleMode));
        m_tileScale->setValue(paint.tileScale > 0.0 ? paint.tileScale : 1.0);
        m_tileScale->setEnabled(paint.imageScaleMode == ScaleMode::Tile);
        refreshImageErrorLabel(m_imagePathForPaint, m_imageErrorLabel);
        break;
    }
    }

    applyTypeUi(paint.type);
}

void PaintEditor::onTypeButtonClicked(int type)
{
    if (m_updating) return;
    const Paint::Type newType = static_cast<Paint::Type>(type);
    if (newType == m_currentType) return;

    // Stash the outgoing type's live state before switching away from it.
    m_paintByType[m_currentType] = getPaint();

    Paint restored;
    auto it = m_paintByType.constFind(newType);
    if (it != m_paintByType.constEnd()) {
        restored = it.value();
    } else {
        restored = defaultPaintForType(newType);
        m_paintByType[newType] = restored;
    }

    setPaint(restored);
    emitPaint();
}

void PaintEditor::onColorChanged(const QColor& color)
{
    if (m_updating) return;

    if (m_currentType == Paint::Type::Linear || m_currentType == Paint::Type::Radial) {
        m_stopBar->setSelectedStopColor(color);
        onStopsChanged(m_stopBar->stops());
    } else if (m_currentType == Paint::Type::Solid) {
        emitPaint();
    }
}

void PaintEditor::onStopsChanged(const QVector<GradientStop>& stops)
{
    if (m_updating) return;

    if (m_currentType == Paint::Type::Linear) {
        m_linearEditor->setStops(stops);
        m_linearEditor->setSelectedStop(m_stopBar->selectedStop());
    } else if (m_currentType == Paint::Type::Radial) {
        m_radialEditor->setStops(stops);
        m_radialEditor->setSelectedStop(m_stopBar->selectedStop());
    }

    emitPaint();
}

void PaintEditor::onStopBarStopSelected(int index)
{
    if (m_updating) return;

    if (m_currentType == Paint::Type::Linear)
        m_linearEditor->setSelectedStop(index);
    else if (m_currentType == Paint::Type::Radial)
        m_radialEditor->setSelectedStop(index);

    syncColorPickerToSelectedStop();
}

void PaintEditor::onGeometryStopSelected(int index)
{
    if (m_updating) return;

    m_stopBar->setSelectedStop(index);
    syncColorPickerToSelectedStop();
}

void PaintEditor::syncColorPickerToSelectedStop()
{
    if (m_currentType != Paint::Type::Linear && m_currentType != Paint::Type::Radial) return;

    const QVector<GradientStop> stops = m_stopBar->stops();
    const int idx = m_stopBar->selectedStop();
    if (idx < 0 || idx >= stops.size()) return;

    QSignalBlocker block(m_colorPicker);
    m_colorPicker->setColor(stops[idx].color);
}

void PaintEditor::emitPaint()
{
    if (m_updating) return;
    const Paint paint = getPaint();
    m_paintByType[m_currentType] = paint;
    emit paintChanged(paint);
}

void PaintEditor::applyTypeUi(Paint::Type type)
{
    const int index = static_cast<int>(type);
    m_mainSelector->setCurrentIndex(index);

    // A QStackedWidget's sizeHint is the MAXIMUM over all its pages, so the
    // tall Linear/Radial pages reserve their height in None/Solid/Image mode
    // and leave a large empty gap under the current page. Make the pages that
    // are not current contribute nothing to the hint, then re-fit the window.
    for (int i = 0; i < m_mainSelector->count(); ++i) {
        QWidget* page = m_mainSelector->widget(i);
        if (!page) continue;
        QSizePolicy sp = page->sizePolicy();
        const auto policy = (i == index) ? QSizePolicy::Preferred : QSizePolicy::Ignored;
        sp.setHorizontalPolicy(policy);
        sp.setVerticalPolicy(policy);
        page->setSizePolicy(sp);
    }
    m_mainSelector->updateGeometry();

    const bool isGradient = (type == Paint::Type::Linear || type == Paint::Type::Radial);
    m_stopBar->setVisible(isGradient);

    const bool showColorPicker = isGradient || type == Paint::Type::Solid;
    m_colorPicker->setVisible(showColorPicker);

    if (m_brandGrid) m_brandGrid->setVisible(showColorPicker);

    // Shrink the hosting tool dialog back to the content it now shows. Without
    // this the window keeps whatever height the tallest previously-shown page
    // needed. Deferred so it runs after the layout has re-hinted.
    if (QWidget* win = window(); win && win != this) {
        QMetaObject::invokeMethod(win, "adjustSize", Qt::QueuedConnection);
    }
}

void PaintEditor::setTargetElementSize(double w, double h)
{
    if (w <= 0 || h <= 0) {
        w = 1.0;
        h = 1.0;
    }
    m_linearEditor->setTargetSize(w, h);
    m_radialEditor->setTargetSize(w, h);
}

void PaintEditor::beginInteraction()
{
    if (m_interactionDepth == 0) emit interactionStarted();
    ++m_interactionDepth;
}

void PaintEditor::endInteraction()
{
    if (m_interactionDepth == 0) return;
    --m_interactionDepth;
    if (m_interactionDepth == 0) emit interactionFinished();
}

bool PaintEditor::eventFilter(QObject* obj, QEvent* event)
{
    return QWidget::eventFilter(obj, event);
}
