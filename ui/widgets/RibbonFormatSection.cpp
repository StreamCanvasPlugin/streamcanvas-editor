#include "RibbonFormatSection.h"
#include "PaintPickerWidget.h"
#include <SARibbonBar.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>
#include "engine/element.h"
#include "engine/graphic.h"
#include "engine/scene.h"
#include "icons.h"
#include "model/SceneDocument.h"
#include "model/UndoCommands.h"
#include "ui/painteditor.h"

#include <QAbstractItemView>
#include "ColorPicker.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QCompleter>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include "../UiUtils.h"

// Renders each font family item in its own typeface (lazy — only when visible).
class FontItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override
    {
        QStyleOptionViewItem o = opt;
        initStyleOption(&o, idx);
        const QString family = idx.data(Qt::UserRole).toString();
        if (!family.isEmpty())
            o.font.setFamily(family);
        QStyledItemDelegate::paint(p, o, idx);
    }
};

static int weightToIndex(FontWeight w)
{
    switch (static_cast<int>(w)) {
    case PANGO_WEIGHT_THIN:       return 0;
    case PANGO_WEIGHT_ULTRALIGHT: return 1;
    case PANGO_WEIGHT_LIGHT:      return 2;
    case PANGO_WEIGHT_NORMAL:     return 3;
    case PANGO_WEIGHT_MEDIUM:     return 4;
    case PANGO_WEIGHT_SEMIBOLD:   return 5;
    case PANGO_WEIGHT_BOLD:       return 6;
    case PANGO_WEIGHT_ULTRABOLD:  return 7;
    case PANGO_WEIGHT_HEAVY:      return 8;
    case PANGO_WEIGHT_ULTRAHEAVY: return 9;
    default:                      return 3;
    }
}

static FontWeight indexToWeight(int i)
{
    switch (i) {
    case 0: return FontWeight::Thin;
    case 1: return FontWeight::UltraLight;
    case 2: return FontWeight::Light;
    case 3: return FontWeight::Normal;
    case 4: return FontWeight::Medium;
    case 5: return FontWeight::SemiBold;
    case 6: return FontWeight::Bold;
    case 7: return FontWeight::UltraBold;
    case 8: return FontWeight::Heavy;
    case 9: return FontWeight::UltraHeavy;
    default: return FontWeight::Normal;
    }
}

// ── helpers ────────────────────────────────────────────────────────────────────

QPixmap RibbonFormatSection::makePaintSwatch(const Paint& paint, QSize size)
{
    QPixmap px(size);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    if (paint.type == Paint::Type::None) {
        p.fillRect(px.rect(), QColor(80, 80, 80));
        p.setPen(QPen(QColor(200, 60, 60), 2));
        p.drawLine(px.rect().topLeft(), px.rect().bottomRight());
    } else if (paint.type == Paint::Type::Solid) {
        QColor c(
            static_cast<int>(paint.params[0] * 255),
            static_cast<int>(paint.params[1] * 255),
            static_cast<int>(paint.params[2] * 255),
            static_cast<int>(paint.params[3] * 255));
        p.fillRect(px.rect(), QColor(180, 180, 180));
        p.fillRect(0, 0, size.width() / 2, size.height() / 2, QColor(220, 220, 220));
        p.fillRect(size.width() / 2, size.height() / 2, size.width() / 2, size.height() / 2,
                   QColor(220, 220, 220));
        p.fillRect(px.rect(), c);
    } else {
        QLinearGradient grad(0, size.height() / 2.0, size.width(), size.height() / 2.0);
        for (const auto& stop : paint.stops) {
            QColor sc(
                static_cast<int>(stop.r * 255),
                static_cast<int>(stop.g * 255),
                static_cast<int>(stop.b * 255),
                static_cast<int>(stop.a * 255));
            grad.setColorAt(stop.offset, sc);
        }
        p.fillRect(px.rect(), grad);
    }
    p.setPen(QColor(100, 100, 100));
    p.drawRect(px.rect().adjusted(0, 0, -1, -1));
    return px;
}

void RibbonFormatSection::updateFillSwatch()
{
    if (!m_fillBtn || m_graphicId.empty() || m_elementId.empty())
        return;
    try {
        const Element& el = m_doc->scene().GetById(m_graphicId).GetById(m_elementId);
        m_fillBtn->setIcon(makePaintSwatch(el.fill, m_fillBtn->iconSize()));
    } catch (...) {}
}

void RibbonFormatSection::updateStrokeSwatch()
{
    if (!m_strokeBtn || m_graphicId.empty() || m_elementId.empty())
        return;
    try {
        const Element& el = m_doc->scene().GetById(m_graphicId).GetById(m_elementId);
        m_strokeBtn->setIcon(makePaintSwatch(el.stroke, m_strokeBtn->iconSize()));
    } catch (...) {}
}

// ── construction ───────────────────────────────────────────────────────────────

RibbonFormatSection::RibbonFormatSection(SceneDocument* doc, SARibbonBar* ribbon, QObject* parent)
    : QObject(parent), m_doc(doc)
{
    buildGraphicTab(ribbon);
    buildElementTab(ribbon);
    buildStyleTab(ribbon);
    buildTextTab(ribbon);
    buildImageTab(ribbon);
    buildQrTab(ribbon);

    connect(m_doc, &SceneDocument::documentChanged, this, &RibbonFormatSection::onDocumentChanged);
}

// ── helper: create a panel on a category and return its container grid layout ──
static QGridLayout* addGroup(SARibbonCategory* category, const QString& name)
{
    auto* panel = category->addPanel(name);
    // Outer container provides uniform padding around the grid content.
    auto* outer = new QWidget;
    auto* ol = new QHBoxLayout(outer);
    ol->setContentsMargins(5, 5, 5, 5);
    ol->setSpacing(0);
    auto* container = new QWidget;
    auto* gl = new QGridLayout(container);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(6);
    ol->addWidget(container);
    panel->addLargeWidget(outer);
    return gl;
}

// ── Graphic tab ────────────────────────────────────────────────────────────────

void RibbonFormatSection::buildGraphicTab(SARibbonBar* ribbon)
{
    m_graphicCategory = ribbon->addCategoryPage("Graphic");

    // ── Info group ─────────────────────────────────────────────────────────────
    {
        auto* gl = addGroup(m_graphicCategory, "Info");

        m_graphicIdEdit = new QLineEdit;
        m_graphicIdEdit->setFixedWidth(120);
        m_graphicIdEdit->setPlaceholderText("ID");

        gl->addWidget(makeRibbonLabel("ID "), 0, 0, 2, 1);
        gl->addWidget(m_graphicIdEdit,  0, 1, 2, 1);

        connect(m_graphicIdEdit, &QLineEdit::editingFinished,
                this, &RibbonFormatSection::onGraphicIdEditingFinished);
    }

    // ── Actions group ──────────────────────────────────────────────────────────
    {
        auto* gl = addGroup(m_graphicCategory, "Actions");

        auto* deleteAct = new QAction(themedIcon(Icons16::Action_Trash), "Delete", nullptr);
        auto* deleteBtn = new QToolButton;
        deleteBtn->setDefaultAction(deleteAct);
        deleteBtn->setIconSize({32, 32});
        deleteBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        deleteBtn->setMinimumHeight(56);
        deleteBtn->setAutoRaise(true);

        gl->addWidget(deleteBtn, 0, 0, 2, 1, Qt::AlignCenter);

        connect(deleteAct, &QAction::triggered, this, &RibbonFormatSection::deleteGraphicRequested);
    }
}

// ── Element tab ────────────────────────────────────────────────────────────────

void RibbonFormatSection::buildElementTab(SARibbonBar* ribbon)
{
    m_elemCategory = ribbon->addCategoryPage("Element");

    // ── Info group ─────────────────────────────────────────────────────────────
    {
        auto* gl = addGroup(m_elemCategory, "Info");

        m_idEdit = new QLineEdit;
        m_idEdit->setFixedWidth(120);
        m_idEdit->setPlaceholderText("ID");

        gl->addWidget(makeRibbonLabel("ID "), 0, 0, 2, 1);
        gl->addWidget(m_idEdit,         0, 1, 2, 1);

        connect(m_idEdit, &QLineEdit::editingFinished, this, &RibbonFormatSection::onIdEditingFinished);
    }

    // ── Transform group ────────────────────────────────────────────────────────
    {
        auto* gl = addGroup(m_elemCategory, "Transform");

        auto mkSpin = [](const QString& suffix, double lo, double hi) {
            auto* s = makeSpinBox(lo, hi, 1, suffix, 78);
            return s;
        };

        m_spinX   = mkSpin(" px", -9999, 9999);
        m_spinY   = mkSpin(" px", -9999, 9999);
        m_spinW   = mkSpin(" px", -9999, 9999);
        m_spinH   = mkSpin(" px", -9999, 9999);
        m_spinRot = mkSpin("°",   -9999, 9999);

        m_spinShearX = mkSpin("", -10, 10);
        m_spinShearX->setDecimals(3);
        m_spinShearX->setSingleStep(0.01);
        m_spinShearX->setToolTip("Horizontal shear");

        m_spinShearY = mkSpin("", -10, 10);
        m_spinShearY->setDecimals(3);
        m_spinShearY->setSingleStep(0.01);
        m_spinShearY->setToolTip("Vertical shear");

        gl->addWidget(makeRibbonLabel("X "),              0, 0);
        gl->addWidget(m_spinX,                            0, 1);
        gl->addWidget(makeRibbonLabel("Width "),          0, 2);
        gl->addWidget(m_spinW,                            0, 3);
        gl->addWidget(makeRibbonLabel("Shear X "),        0, 4);
        gl->addWidget(m_spinShearX,                       0, 5);

        gl->addWidget(makeRibbonLabel("Y "),              1, 0);
        gl->addWidget(m_spinY,                            1, 1);
        gl->addWidget(makeRibbonLabel("Height "),         1, 2);
        gl->addWidget(m_spinH,                            1, 3);
        gl->addWidget(makeRibbonLabel("Shear Y "),        1, 4);
        gl->addWidget(m_spinShearY,                       1, 5);

        gl->addWidget(makeRibbonLabel("Rotation (deg.) "),2, 0);
        gl->addWidget(m_spinRot,                    2, 1, 1, 3);

        gl->setColumnStretch(1, 1);
        gl->setColumnStretch(3, 1);
        gl->setColumnStretch(5, 1);

        connect(m_spinX,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RibbonFormatSection::onXChanged);
        connect(m_spinY,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RibbonFormatSection::onYChanged);
        connect(m_spinW,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RibbonFormatSection::onWChanged);
        connect(m_spinH,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RibbonFormatSection::onHChanged);
        connect(m_spinRot, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RibbonFormatSection::onRotChanged);
        connect(m_spinShearX, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &RibbonFormatSection::onShearXChanged);
        connect(m_spinShearY, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &RibbonFormatSection::onShearYChanged);
    }

    // ── Appearance group ───────────────────────────────────────────────────────
    {
        auto* gl = addGroup(m_elemCategory, "Appearance");

        m_spinZ = new QSpinBox;
        m_spinZ->setRange(-9999, 9999);
        m_spinZ->setFixedWidth(64);
        m_spinZ->setToolTip("Z Order");

        m_spinOpacity = makeSpinBox(0.0, 100.0, 1, "%", 74);
        m_spinOpacity->setSingleStep(0.5);

        gl->addWidget(makeRibbonLabel("Z-Order "), 0, 0);
        gl->addWidget(m_spinZ,               0, 1);
        gl->addWidget(makeRibbonLabel("Opacity "), 1, 0);
        gl->addWidget(m_spinOpacity,         1, 1);

        gl->setColumnStretch(0, 0);
        gl->setColumnStretch(1, 0);

        connect(m_spinZ,       QOverload<int>::of(&QSpinBox::valueChanged),         this, &RibbonFormatSection::onZOrderChanged);
        connect(m_spinOpacity, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &RibbonFormatSection::onOpacityChanged);
    }

    // ── Layout group ───────────────────────────────────────────────────────────
    {
        auto* gl = addGroup(m_elemCategory, "Layout");

        m_maskCombo = new QComboBox;
        m_maskCombo->setFixedWidth(100);

        gl->addWidget(makeRibbonLabel("Mask "),   0, 0);
        gl->addWidget(m_maskCombo,          0, 1);

        m_parentCombo = new QComboBox;
        m_parentCombo->setFixedWidth(100);

        gl->addWidget(makeRibbonLabel("Parent "), 1, 0);
        gl->addWidget(m_parentCombo,        1, 1);

        m_fitCheck = new QCheckBox("Fit to Children");

        // Padding spinboxes (shown only when element has children and fitToChildren)
        m_paddingContainer = new QWidget;
        auto* padGrid = new QGridLayout(m_paddingContainer);
        padGrid->setContentsMargins(0, 0, 0, 0);
        padGrid->setSpacing(4);

        auto mkPadSpin = []() {
            auto* s = makeSpinBox(0.0, 9999.0, 1, " px", 64);
            return s;
        };
        m_spinPadTop    = mkPadSpin();
        m_spinPadRight  = mkPadSpin();
        m_spinPadBottom = mkPadSpin();
        m_spinPadLeft   = mkPadSpin();

        padGrid->addWidget(makeRibbonLabel("T "), 0, 0);
        padGrid->addWidget(m_spinPadTop,    0, 1);
        padGrid->addWidget(makeRibbonLabel("R "), 0, 2);
        padGrid->addWidget(m_spinPadRight,  0, 3);
        padGrid->addWidget(makeRibbonLabel("B "), 1, 0);
        padGrid->addWidget(m_spinPadBottom, 1, 1);
        padGrid->addWidget(makeRibbonLabel("L "), 1, 2);
        padGrid->addWidget(m_spinPadLeft,   1, 3);

        auto* fitContainer = new QWidget;
        auto* fcl = new QVBoxLayout(fitContainer);
        fcl->setContentsMargins(0, 0, 0, 0);
        fcl->setSpacing(4);
        fcl->addWidget(m_fitCheck);
        fcl->addWidget(m_paddingContainer);
        gl->addWidget(fitContainer, 0, 2, 2, 1);

        gl->setColumnStretch(0, 0);
        gl->setColumnStretch(1, 0);
        gl->setColumnStretch(2, 0);

        connect(m_maskCombo,   QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RibbonFormatSection::onMaskChanged);
        connect(m_parentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RibbonFormatSection::onParentChanged);
        connect(m_fitCheck,    &QCheckBox::toggled,                                 this, &RibbonFormatSection::onFitToChildrenToggled);

        auto connectPad = [this](QDoubleSpinBox* s) {
            connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &RibbonFormatSection::onChildrenPaddingChanged);
        };
        connectPad(m_spinPadTop);
        connectPad(m_spinPadRight);
        connectPad(m_spinPadBottom);
        connectPad(m_spinPadLeft);
    }

    // ── Actions group ──────────────────────────────────────────────────────────
    {
        auto* gl = addGroup(m_elemCategory, "Actions");

        auto* deleteAct = new QAction(themedIcon(Icons16::Action_Trash), "Delete", nullptr);
        auto* deleteBtn = new QToolButton;
        deleteBtn->setDefaultAction(deleteAct);
        deleteBtn->setIconSize({32, 32});
        deleteBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        deleteBtn->setMinimumHeight(56);
        deleteBtn->setAutoRaise(true);

        gl->addWidget(deleteBtn, 0, 0, 2, 1, Qt::AlignCenter);

        connect(deleteAct, &QAction::triggered, this, &RibbonFormatSection::deleteElementRequested);
    }
}

void RibbonFormatSection::buildStyleTab(SARibbonBar* ribbon)
{
    m_styleCategory = ribbon->addCategoryPage("Style");

    // ── Paint group (Fill + Stroke) ────────────────────────────────────────────
    {
        auto* gl = addGroup(m_styleCategory, "Paint");

        m_fillPicker   = new PaintPickerWidget(m_doc);
        m_strokePicker = new PaintPickerWidget(m_doc);

        auto makeGradDialog = [this](const QString& title, PaintEditor*& editor) -> QDialog* {
            auto* dlg = new QDialog(nullptr, Qt::Tool);
            dlg->setAttribute(Qt::WA_DeleteOnClose, false);
            dlg->setWindowTitle(title);
            dlg->setMinimumWidth(250);
            auto* l = new QVBoxLayout(dlg);
            editor = new PaintEditor(m_doc, dlg);
            l->addWidget(editor);
            dlg->adjustSize();
            return dlg;
        };
        auto* fillGradDlg   = makeGradDialog("Fill",   m_fillEditor);
        auto* strokeGradDlg = makeGradDialog("Stroke", m_strokeEditor);

        auto [fillBtn,   fillMenu]   = makePopupButton("Fill",   m_fillPicker);
        auto [strokeBtn, strokeMenu] = makePopupButton("Stroke", m_strokePicker);
        m_fillBtn   = fillBtn;
        m_strokeBtn = strokeBtn;

        for (auto* btn : {m_fillBtn, m_strokeBtn}) {
            btn->setIconSize({32, 32});
            btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            btn->setMinimumHeight(56);
            btn->setAutoRaise(true);
        }
        m_fillBtn->setToolTip("Fill paint");
        m_strokeBtn->setToolTip("Stroke paint");

        gl->addWidget(m_fillBtn,   0, 0, Qt::AlignCenter);
        gl->addWidget(m_strokeBtn, 0, 1, Qt::AlignCenter);

        auto wire = [&](PaintPickerWidget* picker, QMenu* menu, QDialog* gradDlg,
                        PaintEditor* editor, bool isFill) {
            connect(menu, &QMenu::aboutToShow, this, [this, picker, isFill]() {
                if (m_graphicId.empty()) return;
                try {
                    const Element& el =
                        m_doc->scene().GetById(m_graphicId).GetById(m_elementId);
                    picker->setPaint(isFill ? el.fill : el.stroke);
                } catch (...) {}
            });

            if (isFill)
                connect(picker, &PaintPickerWidget::paintChanged,
                        this,   &RibbonFormatSection::onFillPaintChanged);
            else
                connect(picker, &PaintPickerWidget::paintChanged,
                        this,   &RibbonFormatSection::onStrokePaintChanged);

            connect(picker, &PaintPickerWidget::moreOptionsRequested,
                    this, [this, menu, gradDlg, editor, isFill]() {
                menu->close();
                if (m_graphicId.empty()) return;
                try {
                    const Element& el =
                        m_doc->scene().GetById(m_graphicId).GetById(m_elementId);
                    QSignalBlocker b(editor);
                    editor->setPaint(isFill ? el.fill : el.stroke);
                } catch (...) {}
                gradDlg->show();
                gradDlg->raise();
                gradDlg->activateWindow();
            });

            if (isFill)
                connect(editor, &PaintEditor::paintChanged,
                        this,   &RibbonFormatSection::onFillPaintChanged);
            else
                connect(editor, &PaintEditor::paintChanged,
                        this,   &RibbonFormatSection::onStrokePaintChanged);
        };

        wire(m_fillPicker,   fillMenu,   fillGradDlg,   m_fillEditor,   true);
        wire(m_strokePicker, strokeMenu, strokeGradDlg, m_strokeEditor, false);
    }

    // ── Stroke group ───────────────────────────────────────────────────────────
    {
        auto* gl = addGroup(m_styleCategory, "Stroke");

        m_strokeWidth = makeSpinBox(0.0, 999.0, 1, " px", 80);
        m_strokeWidth->setSingleStep(0.5);
        m_strokeWidth->setToolTip("Stroke width");

        gl->addWidget(makeRibbonLabel("Width "), 0, 0);
        gl->addWidget(m_strokeWidth,       0, 1);

        gl->setColumnStretch(0, 0);
        gl->setColumnStretch(1, 0);

        connect(m_strokeWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &RibbonFormatSection::onStrokeWidthChanged);
    }

    // ── Border group ───────────────────────────────────────────────────────────
    {
        auto* gl = addGroup(m_styleCategory, "Border");

        auto mkRadiusSpin = []() {
            auto* s = makeSpinBox(0.0, 9999.0, 1, " px", 72);
            s->setSingleStep(0.1);
            return s;
        };
        m_spinTL = mkRadiusSpin(); m_spinTL->setToolTip("Top-Left radius");
        m_spinTR = mkRadiusSpin(); m_spinTR->setToolTip("Top-Right radius");
        m_spinBR = mkRadiusSpin(); m_spinBR->setToolTip("Bottom-Right radius");
        m_spinBL = mkRadiusSpin(); m_spinBL->setToolTip("Bottom-Left radius");

        gl->addWidget(makeIconLabel(themedIcon(Icons16::Action_AnchorTopLeft)), 0, 0);
        gl->addWidget(m_spinTL,         0, 1);
        gl->addWidget(makeIconLabel(themedIcon(Icons16::Action_AnchorTopRight)), 0, 2);
        gl->addWidget(m_spinTR,         0, 3);
        gl->addWidget(makeIconLabel(themedIcon(Icons16::Action_AnchorBottomLeft)), 1, 0);
        gl->addWidget(m_spinBL,         1, 1);
        gl->addWidget(makeIconLabel(themedIcon(Icons16::Action_AnchorBottomRight)), 1, 2);
        gl->addWidget(m_spinBR,         1, 3);

        gl->setColumnStretch(0, 0);
        gl->setColumnStretch(1, 0);
        gl->setColumnStretch(2, 0);
        gl->setColumnStretch(3, 0);

        auto connectRadius = [this](QDoubleSpinBox* s) {
            connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &RibbonFormatSection::onCornerRadiusChanged);
        };
        connectRadius(m_spinTL);
        connectRadius(m_spinTR);
        connectRadius(m_spinBR);
        connectRadius(m_spinBL);
    }

    // ── Shadow group ───────────────────────────────────────────────────────────
    {
        auto* gl = addGroup(m_styleCategory, "Shadow");

        m_shadowEnabled = new QCheckBox("Enable");
        gl->addWidget(m_shadowEnabled, 0, 0, 1, 4);

        m_shadowControls = new QWidget;
        auto* scGrid = new QGridLayout(m_shadowControls);
        scGrid->setContentsMargins(0, 0, 0, 0);
        scGrid->setSpacing(4);

        m_spinShadowOffX = makeSpinBox(-9999, 9999, 1, " px", 72);
        m_spinShadowOffX->setSingleStep(0.5);
        m_spinShadowOffY = makeSpinBox(-9999, 9999, 1, " px", 72);
        m_spinShadowOffY->setSingleStep(0.5);
        m_spinShadowBlur = makeSpinBox(0, 200, 1, " px", 72);
        m_spinShadowBlur->setSingleStep(0.5);

        m_shadowColorPicker = new ColorPicker;
        auto [shadowColorBtn, shadowColorMenu] = makePopupButton("", m_shadowColorPicker);
        m_shadowColorBtn = shadowColorBtn;
        m_shadowColorBtn->setIconSize({24, 16});
        m_shadowColorBtn->setToolTip("Shadow color");
        m_shadowColorBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);

        scGrid->addWidget(makeRibbonLabel("Off X "), 0, 0);
        scGrid->addWidget(m_spinShadowOffX,           0, 1);
        scGrid->addWidget(makeRibbonLabel("Off Y "), 0, 2);
        scGrid->addWidget(m_spinShadowOffY,           0, 3);
        scGrid->addWidget(makeRibbonLabel("Blur "),   1, 0);
        scGrid->addWidget(m_spinShadowBlur,           1, 1);
        scGrid->addWidget(makeRibbonLabel("Color "),  1, 2);
        scGrid->addWidget(m_shadowColorBtn,           1, 3);

        gl->addWidget(m_shadowControls, 1, 0, 1, 4);
        gl->setColumnStretch(0, 0);

        connect(m_shadowEnabled, &QCheckBox::toggled, this, &RibbonFormatSection::onShadowChanged);
        auto connectShadow = [this](QDoubleSpinBox* s) {
            connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &RibbonFormatSection::onShadowChanged);
        };
        connectShadow(m_spinShadowOffX);
        connectShadow(m_spinShadowOffY);
        connectShadow(m_spinShadowBlur);

        connect(shadowColorMenu, &QMenu::aboutToShow, this, [this]() {
            if (m_graphicId.empty()) return;
            try {
                const Element& el = m_doc->scene().GetById(m_graphicId).GetById(m_elementId);
                const auto& c = el.shadow.color;
                QSignalBlocker b(m_shadowColorPicker);
                m_shadowColorPicker->setColor(QColor::fromRgbF(c[0], c[1], c[2], c[3]));
            } catch (...) {}
        });

        connect(m_shadowColorPicker, &ColorPicker::colorChanged, this, [this](const QColor& picked) {
            if (m_graphicId.empty()) return;
            try {
                Element::DropShadow sh =
                    m_doc->scene().GetById(m_graphicId).GetById(m_elementId).shadow;
                sh.color[0] = static_cast<float>(picked.redF());
                sh.color[1] = static_cast<float>(picked.greenF());
                sh.color[2] = static_cast<float>(picked.blueF());
                sh.color[3] = static_cast<float>(picked.alphaF());
                m_doc->undoStack()->push(
                    new SetElementShadowCmd(m_doc, m_graphicId, m_elementId, sh));
            } catch (...) {}
        });
    }
}

void RibbonFormatSection::buildTextTab(SARibbonBar* ribbon)
{
    m_textCategory = ribbon->addCategoryPage("Text");

    // ── Font group ─────────────────────────────────────────────────────────────
    // Row 0: Family (full width, no label)
    // Row 1: Weight (expanding) + Size (fixed, no label)
    // Row 2: Style buttons (I U S, no label)
    {
        auto* gl = addGroup(m_textCategory, "Font");

        auto* fontContainer = new QWidget;
        auto* fcl = new QVBoxLayout(fontContainer);
        fcl->setContentsMargins(0, 0, 0, 0);
        fcl->setSpacing(3);

        // Row 0 — family
        m_fontFamily = new QComboBox;
        m_fontFamily->setMaxVisibleItems(20);
        m_fontFamily->setEditable(true);
        m_fontFamily->setInsertPolicy(QComboBox::NoInsert);
        m_fontFamily->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_fontFamily->setMinimumContentsLength(16);
        m_fontFamily->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_fontFamily->view()->setTextElideMode(Qt::ElideRight);
        if (auto* c = m_fontFamily->completer()) {
            c->setFilterMode(Qt::MatchContains);
            c->setCaseSensitivity(Qt::CaseInsensitive);
        }
        loadFonts();
        fcl->addWidget(m_fontFamily);

        // Row 1 — weight + size
        auto* row1 = new QWidget;
        auto* r1l = new QHBoxLayout(row1);
        r1l->setContentsMargins(0, 0, 0, 0);
        r1l->setSpacing(4);

        m_fontWeight = new QComboBox;
        for (const char* w : {"Thin", "Ultra Light", "Light", "Regular", "Medium",
                              "Semi Bold", "Bold", "Ultra Bold", "Heavy", "Ultra Heavy"})
            m_fontWeight->addItem(w);
        m_fontWeight->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        m_fontSize = makeSpinBox(1.0, 9999.0, 1, " px", 76);
        m_fontSize->setSingleStep(1.0);

        r1l->addWidget(m_fontWeight, 1);
        r1l->addWidget(m_fontSize, 0);
        fcl->addWidget(row1);

        // Row 2 — style buttons
        auto* row2 = new QWidget;
        auto* r2l = new QHBoxLayout(row2);
        r2l->setContentsMargins(0, 0, 0, 0);
        r2l->setSpacing(2);
        r2l->setAlignment(Qt::AlignLeft);

        m_italicBtn    = makeIconToolButton(themedIcon(Icons16::Text_FormatItalic),         "Italic",         true);
        m_underlineBtn = makeIconToolButton(themedIcon(Icons16::Text_FormatUnderline),      "Underline",      true);
        m_strikeBtn    = makeIconToolButton(themedIcon(Icons16::Text_FormatStrikethrough),  "Strikethrough",  true);
        r2l->addWidget(m_italicBtn);
        r2l->addWidget(m_underlineBtn);
        r2l->addWidget(m_strikeBtn);
        r2l->addStretch();
        fcl->addWidget(row2);

        gl->addWidget(fontContainer, 0, 0);

        connect(m_fontFamily, QOverload<int>::of(&QComboBox::activated),
                this, &RibbonFormatSection::onFontFamilyChanged);
        connect(m_fontSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &RibbonFormatSection::onFontSizeChanged);
        connect(m_fontWeight, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &RibbonFormatSection::onFontWeightChanged);
        connect(m_italicBtn,    &QToolButton::toggled, this, &RibbonFormatSection::onItalicToggled);
        connect(m_underlineBtn, &QToolButton::toggled, this, &RibbonFormatSection::onUnderlineToggled);
        connect(m_strikeBtn,    &QToolButton::toggled, this, &RibbonFormatSection::onStrikethroughToggled);
    }

    // ── Paragraph group ────────────────────────────────────────────────────────
    {
        auto* gl = addGroup(m_textCategory, "Paragraph");

        auto* hGroup = new QButtonGroup(nullptr);
        hGroup->setExclusive(true);
        auto* vGroup = new QButtonGroup(nullptr);
        vGroup->setExclusive(true);

        auto mkAlignBtn = [&](Icons16 icon, const QString& tip, QButtonGroup* bg) {
            auto* btn = makeIconToolButton(themedIcon(icon), tip, true);
            bg->addButton(btn);
            return btn;
        };

        m_alignLeft    = mkAlignBtn(Icons16::Text_JustifyLeft,            "Align Left",    hGroup);
        m_alignCenter  = mkAlignBtn(Icons16::Text_JustifyCenter,          "Align Center",  hGroup);
        m_alignJustify = mkAlignBtn(Icons16::Text_JustifyFill,            "Align Justify", hGroup);
        m_alignRight   = mkAlignBtn(Icons16::Text_JustifyRight,           "Align Right",   hGroup);
        m_alignTop     = mkAlignBtn(Icons16::Action_AlignTop,             "Align Top",     vGroup);
        m_alignMiddle  = mkAlignBtn(Icons16::Action_AlignCenterHorizontal,"Align Middle",  vGroup);
        m_alignBottom  = mkAlignBtn(Icons16::Action_AlignBottom,          "Align Bottom",  vGroup);

        auto* btnRow = new QWidget;
        auto* btnRowLayout = new QHBoxLayout(btnRow);
        btnRowLayout->setContentsMargins(0, 0, 0, 0);
        btnRowLayout->setSpacing(2);
        btnRowLayout->setAlignment(Qt::AlignCenter);
        for (auto* btn : {m_alignLeft, m_alignCenter, m_alignJustify, m_alignRight})
            btnRowLayout->addWidget(btn);
        btnRowLayout->addSpacing(6);
        for (auto* btn : {m_alignTop, m_alignMiddle, m_alignBottom})
            btnRowLayout->addWidget(btn);

        m_autoScale = new QCheckBox("Auto scale");
        m_autoScale->setToolTip("Auto scale text to fit bounds");

        gl->addWidget(btnRow,      0, 0);
        gl->addWidget(m_autoScale, 1, 0);

        connect(m_alignLeft,    &QToolButton::clicked, this, [this] { onAlignXChanged(0); });
        connect(m_alignCenter,  &QToolButton::clicked, this, [this] { onAlignXChanged(1); });
        connect(m_alignJustify, &QToolButton::clicked, this, [this] { onAlignXChanged(2); });
        connect(m_alignRight,   &QToolButton::clicked, this, [this] { onAlignXChanged(3); });
        connect(m_alignTop,     &QToolButton::clicked, this, [this] { onAlignYChanged(0); });
        connect(m_alignMiddle,  &QToolButton::clicked, this, [this] { onAlignYChanged(1); });
        connect(m_alignBottom,  &QToolButton::clicked, this, [this] { onAlignYChanged(2); });
        connect(m_autoScale, &QCheckBox::toggled, this, &RibbonFormatSection::onAutoScaleToggled);
    }

    // ── Text group ─────────────────────────────────────────────────────────────
    // Ellipsize / Wrap / Case: label on left, combo expands
    // Edit Text: big button below
    {
        auto* gl = addGroup(m_textCategory, "Text");

        auto* textContainer = new QWidget;
        auto* tcGrid = new QGridLayout(textContainer);
        tcGrid->setContentsMargins(0, 0, 0, 0);
        tcGrid->setHorizontalSpacing(4);
        tcGrid->setVerticalSpacing(3);

        m_ellipsize = new QComboBox;
        m_ellipsize->addItems({"None", "Start", "Middle", "End"});
        m_ellipsize->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_ellipsize->setToolTip("Text ellipsize mode");

        m_wrap = new QComboBox;
        m_wrap->addItems({"Word", "Char", "Word+Char"});
        m_wrap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_wrap->setToolTip("Text wrap mode");

        m_textTransform = new QComboBox;
        m_textTransform->addItems({"None", "Capitalize", "Uppercase", "Lowercase"});
        m_textTransform->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_textTransform->setToolTip("Text transform");

        tcGrid->addWidget(makeRibbonLabel("Ellipsize "), 0, 0);
        tcGrid->addWidget(m_ellipsize,                   0, 1);
        tcGrid->addWidget(makeRibbonLabel("Wrap "),      1, 0);
        tcGrid->addWidget(m_wrap,                        1, 1);
        tcGrid->addWidget(makeRibbonLabel("Case "),      2, 0);
        tcGrid->addWidget(m_textTransform,               2, 1);
        tcGrid->setColumnStretch(0, 0);
        tcGrid->setColumnStretch(1, 1);

        m_textBtn = new QToolButton;
        m_textBtn->setIcon(themedIcon(Icons16::Software_TextEditor));
        m_textBtn->setText("Edit Text…");
        m_textBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_textBtn->setIconSize({32, 32});
        m_textBtn->setMinimumHeight(40);
        m_textBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_textBtn->setAutoRaise(true);
        tcGrid->addWidget(m_textBtn, 0, 2, 3, 1);

        gl->addWidget(textContainer, 0, 0);
        gl->setColumnStretch(0, 1);

        connect(m_ellipsize, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &RibbonFormatSection::onEllipsizeChanged);
        connect(m_wrap, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &RibbonFormatSection::onWrapChanged);
        connect(m_textTransform, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &RibbonFormatSection::onTextTransformChanged);
        connect(m_textBtn, &QToolButton::clicked, this,
                [this]() { openContentEditor("Edit Text"); });
    }
}

// ── Image tab ──────────────────────────────────────────────────────────────────

void RibbonFormatSection::buildImageTab(SARibbonBar* ribbon)
{
    m_imageCategory = ribbon->addCategoryPage("Image");

    {
        auto* gl = addGroup(m_imageCategory, "Source");

        auto* container = new QWidget;
        auto* grid = new QGridLayout(container);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(4);
        grid->setVerticalSpacing(3);

        m_imagePathEdit = new QLineEdit;
        m_imagePathEdit->setPlaceholderText("Image path…");
        m_imagePathEdit->setMinimumWidth(180);
        m_imagePathEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto* browseBtn = new QToolButton;
        browseBtn->setText("…");
        browseBtn->setToolTip("Browse for image file");

        m_scaleMode = new QComboBox;
        m_scaleMode->addItems({"Stretch", "Contain", "Cover", "Fit Width", "Fit Height", "None"});
        m_scaleMode->setToolTip("Scale mode");
        m_scaleMode->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        grid->addWidget(makeRibbonLabel("Path "),  0, 0);
        grid->addWidget(m_imagePathEdit,            0, 1);
        grid->addWidget(browseBtn,                  0, 2);
        grid->addWidget(makeRibbonLabel("Scale "), 1, 0);
        grid->addWidget(m_scaleMode,                1, 1, 1, 2);
        grid->setColumnStretch(1, 1);

        gl->addWidget(container, 0, 0);
        gl->setColumnStretch(0, 1);

        connect(m_imagePathEdit, &QLineEdit::editingFinished,
                this, &RibbonFormatSection::onImagePathChanged);
        connect(browseBtn, &QToolButton::clicked, this, [this]() {
            const QString path = QFileDialog::getOpenFileName(
                nullptr, "Select Image", QString(), "Images (*.png *.jpg *.jpeg)");
            if (path.isEmpty()) return;
            m_imagePathEdit->setText(path);
            onImagePathChanged();
        });
        connect(m_scaleMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &RibbonFormatSection::onScaleModeChanged);
    }
}

// ── QR Code tab ────────────────────────────────────────────────────────────────

void RibbonFormatSection::buildQrTab(SARibbonBar* ribbon)
{
    m_qrCategory = ribbon->addCategoryPage("QR Code");

    {
        auto* gl = addGroup(m_qrCategory, "Content");

        auto* qrBtn = new QToolButton;
        qrBtn->setIcon(qrCodeIcon());
        qrBtn->setText("Edit Content…");
        qrBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        qrBtn->setIconSize({32, 32});
        qrBtn->setMinimumHeight(40);
        qrBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        qrBtn->setAutoRaise(true);

        gl->addWidget(qrBtn, 0, 0);
        gl->setColumnStretch(0, 1);

        connect(qrBtn, &QToolButton::clicked, this,
                [this]() { openContentEditor("Edit QR Content"); });
    }
}

void RibbonFormatSection::openContentEditor(const QString& title)
{
    if (!m_contentDialog) {
        m_contentDialog = new QDialog(nullptr, Qt::Tool);
        m_contentDialog->resize(360, 180);
        auto* l = new QVBoxLayout(m_contentDialog);
        m_contentEdit = new QPlainTextEdit(m_contentDialog);
        l->addWidget(m_contentEdit);
        connect(m_contentEdit, &QPlainTextEdit::textChanged,
                this, &RibbonFormatSection::onContentChanged);
    }
    m_contentDialog->setWindowTitle(title);
    if (!m_graphicId.empty()) {
        try {
            const Element& el = m_doc->scene().GetById(m_graphicId).GetById(m_elementId);
            QSignalBlocker blocker(m_contentEdit);
            m_contentEdit->setPlainText(QString::fromStdString(el.text));
            m_contentSavedCursor = 0;
        } catch (...) {}
    }
    m_contentDialog->show();
    m_contentDialog->raise();
    m_contentDialog->activateWindow();
}

// ── loadFonts ──────────────────────────────────────────────────────────────────

void RibbonFormatSection::loadFonts()
{
    auto* model = new QStandardItemModel(m_fontFamily);

    auto* browseItem = new QStandardItem("Open font from file…");
    browseItem->setData(QString(), Qt::UserRole);
    model->appendRow(browseItem);

    for (const QString& family : QFontDatabase::families()) {
        auto* item = new QStandardItem(family);
        item->setData(family, Qt::UserRole);
        model->appendRow(item);
    }

    m_fontFamily->setModel(model);
    m_fontFamily->view()->setItemDelegate(new FontItemDelegate(m_fontFamily));
}

// ── setSelection / clearSelection ─────────────────────────────────────────────

void RibbonFormatSection::setGraphicSelection(const std::string& gi)
{
    m_graphicId = gi;
    m_elementId.clear();
    if (m_graphicIdEdit) {
        try {
            const Graphic& g = m_doc->scene().GetById(gi);
            QSignalBlocker b(m_graphicIdEdit);
            m_graphicIdEdit->setText(QString::fromStdString(g.id));
        } catch (...) {}
    }
}

void RibbonFormatSection::setSelection(const std::string& gi, const std::string& ei)
{
    m_graphicId = gi;
    m_elementId = ei;
    onDocumentChanged();
}

void RibbonFormatSection::clearSelection()
{
    m_graphicId.clear();
    m_elementId.clear();
}

// ── onDocumentChanged ──────────────────────────────────────────────────────────

void RibbonFormatSection::populateRefCombos()
{
    try {
        Graphic& g = m_doc->scene().GetById(m_graphicId);
        Element& el = g.GetById(m_elementId);

        m_maskCombo->blockSignals(true);
        m_maskCombo->clear();
        m_maskCombo->addItem("None", QString());
        int maskIdx = 0;
        for (const auto& other : g.elements) {
            if (other.id == m_elementId) continue;
            m_maskCombo->addItem(QString::fromStdString(other.id),
                                 QString::fromStdString(other.id));
            if (el.mask && el.mask->id == other.id)
                maskIdx = m_maskCombo->count() - 1;
        }
        m_maskCombo->setCurrentIndex(maskIdx);
        m_maskCombo->blockSignals(false);

        m_parentCombo->blockSignals(true);
        m_parentCombo->clear();
        m_parentCombo->addItem("None", QString());
        int parentIdx = 0;
        for (const auto& other : g.elements) {
            if (other.id == m_elementId) continue;
            m_parentCombo->addItem(QString::fromStdString(other.id),
                                   QString::fromStdString(other.id));
            if (el.parent && el.parent->id == other.id)
                parentIdx = m_parentCombo->count() - 1;
        }
        m_parentCombo->setCurrentIndex(parentIdx);
        m_parentCombo->blockSignals(false);
    } catch (...) {}
}

void RibbonFormatSection::onDocumentChanged()
{
    if (m_updating)
        return;

    // Sync the Graphic tab if only a graphic is selected.
    if (!m_graphicId.empty() && m_elementId.empty() && m_graphicIdEdit) {
        try {
            const Graphic& g = m_doc->scene().GetById(m_graphicId);
            QSignalBlocker b(m_graphicIdEdit);
            m_graphicIdEdit->setText(QString::fromStdString(g.id));
        } catch (...) {}
        return;
    }

    if (m_graphicId.empty() || m_elementId.empty())
        return;

    Scene& s = m_doc->scene();
    const Element* el = nullptr;
    try {
        el = &s.GetById(m_graphicId).GetById(m_elementId);
    } catch (...) {
        return;
    }

    const Graphic& g = s.GetById(m_graphicId);

    {
    UpdateGuard ug(m_updating);

    // Transform
    m_spinX->setValue(el->bounds.x);
    m_spinY->setValue(el->bounds.y);
    m_spinW->setValue(el->bounds.width);
    m_spinH->setValue(el->bounds.height);
    m_spinRot->setValue(static_cast<double>(el->rotation));

    // Appearance
    m_spinZ->setValue(el->zOrder);
    m_spinOpacity->setValue(static_cast<double>(el->opacity) * 100.0);

    // Style tab
    m_strokeWidth->setValue(static_cast<double>(el->strokeWidth));
    m_spinTL->setValue(static_cast<double>(el->cornerRadius[0]));
    m_spinTR->setValue(static_cast<double>(el->cornerRadius[1]));
    m_spinBR->setValue(static_cast<double>(el->cornerRadius[2]));
    m_spinBL->setValue(static_cast<double>(el->cornerRadius[3]));

    // Shear
    m_spinShearX->setValue(static_cast<double>(el->shearX));
    m_spinShearY->setValue(static_cast<double>(el->shearY));

    // Shadow
    m_shadowEnabled->setChecked(el->shadow.enabled);
    m_spinShadowOffX->setValue(el->shadow.offsetX);
    m_spinShadowOffY->setValue(el->shadow.offsetY);
    m_spinShadowBlur->setValue(el->shadow.blur);
    {
        const auto& c = el->shadow.color;
        QPixmap px(24, 14);
        px.fill(QColor::fromRgbF(c[0], c[1], c[2], c[3]));
        m_shadowColorBtn->setIcon(QIcon(px));
    }
    m_shadowControls->setEnabled(el->shadow.enabled);

    // ID field
    bool usedAsParent = false;
    for (const auto& other : g.elements)
        if (other.parent == el) { usedAsParent = true; break; }
    m_idEdit->setReadOnly(usedAsParent);
    m_idEdit->setStyleSheet(usedAsParent ? "QLineEdit { color: #888; font-style: italic; }" : "");
    m_idEdit->setText(QString::fromStdString(el->id));

    // Fit-to-children + padding
    bool hasChildren = false;
    for (const auto& other : g.elements)
        if (other.parent == el) { hasChildren = true; break; }
    m_fitCheck->setChecked(el->fitToChildren);
    m_paddingContainer->setEnabled(el->fitToChildren);
    m_spinPadTop->setValue(static_cast<double>(el->childrenPadding[0]));
    m_spinPadRight->setValue(static_cast<double>(el->childrenPadding[1]));
    m_spinPadBottom->setValue(static_cast<double>(el->childrenPadding[2]));
    m_spinPadLeft->setValue(static_cast<double>(el->childrenPadding[3]));
    auto* fitParent = m_fitCheck->parentWidget();
    if (fitParent) fitParent->setVisible(hasChildren);

    populateRefCombos();

    // Text properties (only when text element)
    if (el->type == ElementType::Text) {
        const QString family = QString::fromStdString(el->font.family);
        auto* model = qobject_cast<QStandardItemModel*>(m_fontFamily->model());
        int idx = -1;
        for (int i = 1; i < model->rowCount(); ++i) {
            if (model->item(i)->data(Qt::UserRole).toString() == family) {
                idx = i; break;
            }
        }
        m_fontFamily->setCurrentIndex(idx >= 0 ? idx : 0);
        m_fontSize->setValue(static_cast<double>(el->font.size));
        m_fontWeight->setCurrentIndex(weightToIndex(el->font.weight));
        m_italicBtn->setChecked(el->font.isItalic);
        m_underlineBtn->setChecked(el->font.isUnderline);
        m_strikeBtn->setChecked(el->font.isStrikethrough);
        m_autoScale->setChecked(el->textStyle.autoScale);

        m_alignLeft->setChecked(el->textStyle.alignX    == HorizontalAlignment::Left);
        m_alignCenter->setChecked(el->textStyle.alignX  == HorizontalAlignment::Center);
        m_alignJustify->setChecked(el->textStyle.alignX == HorizontalAlignment::Justify);
        m_alignRight->setChecked(el->textStyle.alignX   == HorizontalAlignment::Right);
        m_alignTop->setChecked(el->textStyle.alignY    == VerticalAlignment::Top);
        m_alignMiddle->setChecked(el->textStyle.alignY == VerticalAlignment::Middle);
        m_alignBottom->setChecked(el->textStyle.alignY == VerticalAlignment::Bottom);

        switch (el->textStyle.ellipsize) {
        case Ellipsize::Start:  m_ellipsize->setCurrentIndex(1); break;
        case Ellipsize::Middle: m_ellipsize->setCurrentIndex(2); break;
        case Ellipsize::End:    m_ellipsize->setCurrentIndex(3); break;
        default:                m_ellipsize->setCurrentIndex(0); break;
        }
        switch (el->textStyle.wrapMode) {
        case WrapMode::Char:     m_wrap->setCurrentIndex(1); break;
        case WrapMode::WordChar: m_wrap->setCurrentIndex(2); break;
        default:                 m_wrap->setCurrentIndex(0); break;
        }
        switch (el->textStyle.transform) {
        case TextTransform::Capitalize: m_textTransform->setCurrentIndex(1); break;
        case TextTransform::Uppercase:  m_textTransform->setCurrentIndex(2); break;
        case TextTransform::Lowercase:  m_textTransform->setCurrentIndex(3); break;
        default:                        m_textTransform->setCurrentIndex(0); break;
        }

        if (m_contentEdit) {
            QSignalBlocker blocker(m_contentEdit);
            m_contentEdit->setPlainText(QString::fromStdString(el->text));
            QTextCursor cursor(m_contentEdit->document());
            cursor.setPosition(m_contentSavedCursor);
            m_contentEdit->setTextCursor(cursor);
        }
    }

    // Image properties
    if (el->type == ElementType::Image) {
        m_imagePathEdit->setText(QString::fromStdString(el->imagePath));
        m_scaleMode->setCurrentIndex(static_cast<int>(el->imageScaleMode));
    }

    // QR content — restore cursor same as text
    if (el->type == ElementType::QrCode && m_contentEdit) {
        QSignalBlocker blocker(m_contentEdit);
        m_contentEdit->setPlainText(QString::fromStdString(el->text));
        QTextCursor cursor(m_contentEdit->document());
        cursor.setPosition(m_contentSavedCursor);
        m_contentEdit->setTextCursor(cursor);
    }

    } // UpdateGuard released here

    updateFillSwatch();
    updateStrokeSwatch();
}

// ── Graphic tab slot ──────────────────────────────────────────────────────────

void RibbonFormatSection::onGraphicIdEditingFinished()
{
    if (m_updating || m_graphicId.empty()) return;
    std::string newId = m_graphicIdEdit->text().toStdString();
    if (m_graphicId == newId) return;
    m_doc->undoStack()->push(new SetGraphicFieldCmd<std::string>(
        m_doc, m_graphicId, newId,
        [](Graphic& g) -> std::string& { return g.id; }, "id"));
    m_graphicId = newId;
}

// ── Element tab slot implementations ──────────────────────────────────────────

void RibbonFormatSection::onXChanged(double v)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<double>(
        m_doc, m_graphicId, m_elementId, v,
        [](Element& e) -> double& { return e.bounds.x; }, "x", ElemMergeTag::X));
}

void RibbonFormatSection::onYChanged(double v)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<double>(
        m_doc, m_graphicId, m_elementId, v,
        [](Element& e) -> double& { return e.bounds.y; }, "y", ElemMergeTag::Y));
}

void RibbonFormatSection::onWChanged(double v)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<double>(
        m_doc, m_graphicId, m_elementId, v,
        [](Element& e) -> double& { return e.bounds.width; }, "width", ElemMergeTag::W));
}

void RibbonFormatSection::onHChanged(double v)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<double>(
        m_doc, m_graphicId, m_elementId, v,
        [](Element& e) -> double& { return e.bounds.height; }, "height", ElemMergeTag::H));
}

void RibbonFormatSection::onRotChanged(double v)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_graphicId, m_elementId, static_cast<float>(v),
        [](Element& e) -> float& { return e.rotation; }, "rotation", ElemMergeTag::Rotation));
}

void RibbonFormatSection::onZOrderChanged(int v)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<int>(
        m_doc, m_graphicId, m_elementId, v,
        [](Element& e) -> int& { return e.zOrder; }, "zOrder", ElemMergeTag::ZOrder));
}

void RibbonFormatSection::onOpacityChanged(double v)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_graphicId, m_elementId, static_cast<float>(v / 100.0),
        [](Element& e) -> float& { return e.opacity; }, "opacity", ElemMergeTag::Opacity));
}

void RibbonFormatSection::onFillPaintChanged(const Paint& p)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementPaintCmd(
        m_doc, m_graphicId, m_elementId, SetElementPaintCmd::Target::Fill, p));
}

void RibbonFormatSection::onStrokePaintChanged(const Paint& p)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementPaintCmd(
        m_doc, m_graphicId, m_elementId, SetElementPaintCmd::Target::Stroke, p));
}

void RibbonFormatSection::onStrokeWidthChanged(double v)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_graphicId, m_elementId, static_cast<float>(v),
        [](Element& e) -> float& { return e.strokeWidth; }, "strokeWidth",
        ElemMergeTag::StrokeW));
}

void RibbonFormatSection::onCornerRadiusChanged()
{
    if (m_updating || m_graphicId.empty() || !m_spinTL) return;
    float tl = static_cast<float>(m_spinTL->value());
    float tr = static_cast<float>(m_spinTR->value());
    float br = static_cast<float>(m_spinBR->value());
    float bl = static_cast<float>(m_spinBL->value());
    m_doc->undoStack()->push(
        new SetCornerRadiusCmd(m_doc, m_graphicId, m_elementId, tl, tr, br, bl));
}

void RibbonFormatSection::onIdEditingFinished()
{
    if (m_updating || m_graphicId.empty()) return;
    std::string newId = m_idEdit->text().toStdString();
    if (m_elementId == newId) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<std::string>(
        m_doc, m_graphicId, m_elementId, newId,
        [](Element& e) -> std::string& { return e.id; }, "id"));
    m_elementId = newId;
    emit elementIdChanged(m_graphicId, m_elementId);
}

void RibbonFormatSection::onMaskChanged(int)
{
    if (m_updating || m_graphicId.empty()) return;
    try {
        const Element& el = m_doc->scene().GetById(m_graphicId).GetById(m_elementId);
        std::string maskId   = m_maskCombo->currentData().toString().toStdString();
        std::string parentId = el.parent ? el.parent->id : "";
        m_doc->setElementRef(m_graphicId, m_elementId, maskId, parentId);
    } catch (...) {}
}

void RibbonFormatSection::onParentChanged(int)
{
    if (m_updating || m_graphicId.empty()) return;
    try {
        const Element& el = m_doc->scene().GetById(m_graphicId).GetById(m_elementId);
        std::string maskId      = el.mask ? el.mask->id : "";
        std::string newParentId = m_parentCombo->currentData().toString().toStdString();
        std::string oldParentId = el.parent ? el.parent->id : "";
        if (newParentId == oldParentId) return;

        Point globalPos = el.GetGlobalPosition();
        m_doc->setElementRef(m_graphicId, m_elementId, maskId, newParentId);

        double newX = globalPos.x, newY = globalPos.y;
        if (!newParentId.empty()) {
            try {
                Point pg = m_doc->scene().GetById(m_graphicId).GetById(newParentId).GetGlobalPosition();
                newX = globalPos.x - pg.x;
                newY = globalPos.y - pg.y;
            } catch (...) {}
        }
        const std::string gi = m_graphicId, ei = m_elementId;
        m_doc->applyMutation([gi, ei, newX, newY](Scene& s) {
            Element& e = s.GetById(gi).GetById(ei);
            e.bounds.x = newX;
            e.bounds.y = newY;
        });
    } catch (...) {}
}

void RibbonFormatSection::onFitToChildrenToggled(bool checked)
{
    if (m_updating || m_graphicId.empty()) return;
    m_paddingContainer->setEnabled(checked);
    m_doc->undoStack()->push(new SetElementFieldCmd<bool>(
        m_doc, m_graphicId, m_elementId, checked,
        [](Element& e) -> bool& { return e.fitToChildren; }, "fitToChildren"));
}

void RibbonFormatSection::onChildrenPaddingChanged()
{
    if (m_updating || m_graphicId.empty() || !m_spinPadTop) return;
    float top    = static_cast<float>(m_spinPadTop->value());
    float right  = static_cast<float>(m_spinPadRight->value());
    float bottom = static_cast<float>(m_spinPadBottom->value());
    float left   = static_cast<float>(m_spinPadLeft->value());
    m_doc->undoStack()->push(
        new SetChildrenPaddingCmd(m_doc, m_graphicId, m_elementId, top, right, bottom, left));
}

// ── Text tab slot implementations ──────────────────────────────────────────────

void RibbonFormatSection::onFontFamilyChanged(int index)
{
    if (m_updating || m_graphicId.empty()) return;
    if (index == 0) {
        const QString path = QFileDialog::getOpenFileName(
            nullptr, "Open Font File", QString(),
            "Font files (*.ttf *.otf *.woff *.woff2);;All files (*)");
        if (path.isEmpty()) return;
        int id = QFontDatabase::addApplicationFont(path);
        if (id < 0) return;
        const QStringList loaded = QFontDatabase::applicationFontFamilies(id);
        if (loaded.isEmpty()) return;
        const QString family = loaded.first();
        auto* model = qobject_cast<QStandardItemModel*>(m_fontFamily->model());
        for (int i = 1; i < model->rowCount(); ++i) {
            if (model->item(i)->data(Qt::UserRole).toString() == family) {
                m_fontFamily->setCurrentIndex(i);
                onFontFamilyChanged(i);
                return;
            }
        }
        auto* item = new QStandardItem(family + " (file)");
        item->setData(family, Qt::UserRole);
        model->insertRow(1, item);
        m_fontFamily->setCurrentIndex(1);
        return;
    }
    auto* model = qobject_cast<QStandardItemModel*>(m_fontFamily->model());
    const std::string family =
        model->item(index)->data(Qt::UserRole).toString().toStdString();
    m_doc->undoStack()->push(new SetElementFieldCmd<std::string>(
        m_doc, m_graphicId, m_elementId, family,
        [](Element& e) -> std::string& { return e.font.family; }, "font_family"));
}

void RibbonFormatSection::onFontSizeChanged(double v)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_graphicId, m_elementId, static_cast<float>(v),
        [](Element& e) -> float& { return e.font.size; }, "font_size",
        ElemMergeTag::StrokeW + 10));
}

void RibbonFormatSection::onFontWeightChanged(int index)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<FontWeight>(
        m_doc, m_graphicId, m_elementId, indexToWeight(index),
        [](Element& e) -> FontWeight& { return e.font.weight; }, "font_weight"));
}

void RibbonFormatSection::onItalicToggled(bool checked)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<bool>(
        m_doc, m_graphicId, m_elementId, checked,
        [](Element& e) -> bool& { return e.font.isItalic; }, "font_italic"));
}

void RibbonFormatSection::onUnderlineToggled(bool checked)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<bool>(
        m_doc, m_graphicId, m_elementId, checked,
        [](Element& e) -> bool& { return e.font.isUnderline; }, "font_underline"));
}

void RibbonFormatSection::onStrikethroughToggled(bool checked)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<bool>(
        m_doc, m_graphicId, m_elementId, checked,
        [](Element& e) -> bool& { return e.font.isStrikethrough; }, "font_strikethrough"));
}

void RibbonFormatSection::onAlignXChanged(int index)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<HorizontalAlignment>(
        m_doc, m_graphicId, m_elementId, static_cast<HorizontalAlignment>(index),
        [](Element& e) -> HorizontalAlignment& { return e.textStyle.alignX; }, "text_align_x"));
}

void RibbonFormatSection::onAlignYChanged(int index)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<VerticalAlignment>(
        m_doc, m_graphicId, m_elementId, static_cast<VerticalAlignment>(index),
        [](Element& e) -> VerticalAlignment& { return e.textStyle.alignY; }, "text_align_y"));
}

void RibbonFormatSection::onAutoScaleToggled(bool checked)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<bool>(
        m_doc, m_graphicId, m_elementId, checked,
        [](Element& e) -> bool& { return e.textStyle.autoScale; }, "auto_scale"));
}

void RibbonFormatSection::onEllipsizeChanged(int index)
{
    if (m_updating || m_graphicId.empty()) return;
    static const Ellipsize kMap[] = {Ellipsize::None, Ellipsize::Start,
                                      Ellipsize::Middle, Ellipsize::End};
    m_doc->undoStack()->push(new SetElementFieldCmd<Ellipsize>(
        m_doc, m_graphicId, m_elementId, kMap[index],
        [](Element& el) -> Ellipsize& { return el.textStyle.ellipsize; }, "ellipsize"));
}

void RibbonFormatSection::onWrapChanged(int index)
{
    if (m_updating || m_graphicId.empty()) return;
    static const WrapMode kMap[] = {WrapMode::Word, WrapMode::Char, WrapMode::WordChar};
    m_doc->undoStack()->push(new SetElementFieldCmd<WrapMode>(
        m_doc, m_graphicId, m_elementId, kMap[index],
        [](Element& e) -> WrapMode& { return e.textStyle.wrapMode; }, "wrap"));
}

void RibbonFormatSection::onTextTransformChanged(int index)
{
    if (m_updating || m_graphicId.empty()) return;
    static const TextTransform kMap[] = {TextTransform::None, TextTransform::Capitalize,
                                          TextTransform::Uppercase, TextTransform::Lowercase};
    m_doc->undoStack()->push(new SetElementFieldCmd<TextTransform>(
        m_doc, m_graphicId, m_elementId, kMap[index],
        [](Element& e) -> TextTransform& { return e.textStyle.transform; }, "text_transform"));
}

void RibbonFormatSection::onContentChanged()
{
    if (m_updating || !m_contentEdit || m_graphicId.empty()) return;
    const std::string text = m_contentEdit->toPlainText().toStdString();
    m_contentSavedCursor = m_contentEdit->textCursor().position();
    m_doc->undoStack()->push(new SetElementFieldCmd<std::string>(
        m_doc, m_graphicId, m_elementId, text,
        [](Element& e) -> std::string& { return e.text; }, "text"));
}

void RibbonFormatSection::onImagePathChanged()
{
    if (m_updating || m_graphicId.empty()) return;
    const std::string path = m_imagePathEdit->text().toStdString();
    m_doc->undoStack()->push(new SetElementFieldCmd<std::string>(
        m_doc, m_graphicId, m_elementId, path,
        [](Element& e) -> std::string& { return e.imagePath; }, "image_path"));
}

void RibbonFormatSection::onScaleModeChanged(int index)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<ScaleMode>(
        m_doc, m_graphicId, m_elementId, static_cast<ScaleMode>(index),
        [](Element& e) -> ScaleMode& { return e.imageScaleMode; }, "scale_mode"));
}

void RibbonFormatSection::onShearXChanged(double v)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_graphicId, m_elementId, static_cast<float>(v),
        [](Element& e) -> float& { return e.shearX; }, "shearX", ElemMergeTag::ShearX));
}

void RibbonFormatSection::onShearYChanged(double v)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_graphicId, m_elementId, static_cast<float>(v),
        [](Element& e) -> float& { return e.shearY; }, "shearY", ElemMergeTag::ShearY));
}

void RibbonFormatSection::onShadowChanged()
{
    if (m_updating || m_graphicId.empty()) return;
    try {
        Element::DropShadow sh = m_doc->scene().GetById(m_graphicId).GetById(m_elementId).shadow;
        sh.enabled = m_shadowEnabled->isChecked();
        sh.offsetX = m_spinShadowOffX->value();
        sh.offsetY = m_spinShadowOffY->value();
        sh.blur    = m_spinShadowBlur->value();
        m_shadowControls->setEnabled(sh.enabled);
        m_doc->undoStack()->push(
            new SetElementShadowCmd(m_doc, m_graphicId, m_elementId, sh));
    } catch (...) {}
}

