#include "mainwindow.h"
#include "engine/graphic.h"
#include "engine/scene.h"
#include "icons.h"
#include "model/EditorScene.h"
#include "model/SceneDocument.h"
#include "model/UndoCommands.h"
#include "ui/widgets/CanvasWidget.h"
#include "ui/widgets/GraphicTimingEditor.h"
#include "ui/widgets/RibbonFormatSection.h"
#include "ui/widgets/SceneTreeView.h"
#include <RibbonGroup>

#include <algorithm>
#include <string>

#include <QAction>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QSpinBox>
#include <QStatusBar>
#include <QTextBrowser>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QToolButton>

static const struct { const char* label; int w, h; } kScenePresets[] = {
    {"1920 × 1080  (16:9 FHD)", 1920, 1080},
    {"1280 × 720   (16:9 HD)",  1280,  720},
    {"3840 × 2160  (4K UHD)",   3840, 2160},
    {"1080 × 1920  (9:16 Vert)",1080, 1920},
    {"2560 × 1440  (16:9 QHD)", 2560, 1440},
    {"Custom", 0, 0},
};
static constexpr int kSceneCustomIdx = 5;

static const QStringList kImageExts = {
    "png","jpg","jpeg","bmp","gif","tiff","tif","webp"};

static bool clipboardHasExternalImage()
{
    const QMimeData* md = QGuiApplication::clipboard()->mimeData();
    if (!md) return false;
    if (md->hasImage()) return true;
    for (const QString& fmt : md->formats())
        if (fmt.startsWith("image/")) return true;
    if (md->hasUrls()) {
        for (const QUrl& url : md->urls()) {
            if (!url.isLocalFile()) continue;
            if (kImageExts.contains(QFileInfo(url.toLocalFile()).suffix().toLower()))
                return true;
        }
    }
    return false;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_doc(new SceneDocument(this)),
      m_editorScene(new EditorScene(m_doc, this)),
      m_canvas(new CanvasWidget(m_doc, m_editorScene, this))
{
    showMaximized();

    // Ribbon bar
    m_ribbon = new Nedrysoft::Ribbon::RibbonWidget(this);

    auto* ribbonDock = new QDockWidget("Ribbon", this);
    ribbonDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    ribbonDock->setTitleBarWidget(new QWidget(ribbonDock));
    ribbonDock->setWidget(m_ribbon);
    addDockWidget(Qt::TopDockWidgetArea, ribbonDock);

    setupRibbon();

    // Central widget
    setCentralWidget(m_canvas);

    // Scene tree — left dock
    auto* sceneDock = new QDockWidget("Scene", this);
    sceneDock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
    sceneDock->setWidget(new SceneTreeView(m_doc, m_editorScene, sceneDock));
    addDockWidget(Qt::LeftDockWidgetArea, sceneDock);

    // Give side docks ownership of the bottom corners
    setCorner(Qt::BottomLeftCorner,  Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    // Animation timing — bottom dock
    auto* timingDock = new QDockWidget("Animation Timing", this);
    timingDock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
    m_timingEditor = new GraphicTimingEditor(m_doc, timingDock);
    timingDock->setWidget(m_timingEditor);
    addDockWidget(Qt::BottomDockWidgetArea, timingDock);

    connect(m_timingEditor, &GraphicTimingEditor::animationChanged,
            [this](int elementIndex, bool isIn, AnimationType type, Easing easing,
                   float delay, float duration) {
        const Scene& s = m_doc->scene();
        const SelectionId sel = m_editorScene->selection();
        if (sel.graphicIndex < 0 || sel.graphicIndex >= (int)s.graphics.size())
            return;
        const Graphic& g = s.graphics[sel.graphicIndex];
        if (elementIndex < 0 || elementIndex >= (int)g.elements.size())
            return;
        const std::string gi = g.id;
        const std::string ei = g.elements[elementIndex].id;
        AnimationDef def = isIn ? g.elements[elementIndex].inAnimation
                                : g.elements[elementIndex].outAnimation;
        def.type     = type;
        def.easing   = easing;
        def.delay    = delay;
        def.duration = duration;
        auto target = isIn ? SetElementAnimCmd::Target::AnimIn
                           : SetElementAnimCmd::Target::AnimOut;
        m_doc->undoStack()->push(new SetElementAnimCmd(m_doc, gi, ei, target, def));
    });

    connect(m_timingEditor, &GraphicTimingEditor::scrubTimeChanged,
            [this](float t) {
        const SelectionId sel = m_editorScene->selection();
        if (sel.graphicIndex >= 0)
            m_canvas->previewAtTime(sel.graphicIndex, m_timingEditor->isIn(), double(t));
    });

    connect(m_timingEditor, &GraphicTimingEditor::previewStopped,
            m_canvas, &CanvasWidget::stopAnimationPreview);

    setupMenuBar();
    updateWindowTitle();

    connect(QGuiApplication::clipboard(), &QClipboard::changed, this,
        [this](QClipboard::Mode mode) {
            if (mode == QClipboard::Clipboard)
                updateToolBarState(m_editorScene->selection());
        });

    connect(m_editorScene, &EditorScene::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(m_formatSection, &RibbonFormatSection::elementIdChanged,
            [this](const std::string& /*gi*/, const std::string& /*ei*/) {
                // RibbonFormatSection already holds the new IDs internally; nothing else to sync.
            });
    connect(m_doc, &SceneDocument::modifiedChanged, this, &MainWindow::setWindowModified);
    connect(m_doc, &SceneDocument::filePathChanged, this, [this](const QString& path) {
        updateWindowTitle();
        statusBar()->showMessage(path.isEmpty() ? "Untitled" : path);
    });

    statusBar()->showMessage("Untitled");
}

// ── Ribbon setup ──────────────────────────────────────────────────────────────

void MainWindow::setupRibbon()
{
    // ── Shared helpers ─────────────────────────────────────────────────────────
    auto makeGroup = [](QWidget* page, QHBoxLayout* pageLayout, const QString& name) {
        auto* grp = new Nedrysoft::Ribbon::RibbonGroup(page);
        grp->setGroupName(name);
        grp->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        auto* gl = new QHBoxLayout(grp);
        gl->setContentsMargins(4, 2, 4, 2);
        gl->setSpacing(4);
        pageLayout->insertWidget(pageLayout->count()-1, grp);
        return gl;
    };

    auto makeLargeBtn = [](QAction* act) -> QToolButton* {
        auto* btn = new QToolButton;
        btn->setDefaultAction(act);
        btn->setIconSize({32, 32});
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setMinimumHeight(56);
        btn->setAutoRaise(true);
        return btn;
    };

    // Creates a vertical stack of small icon+text buttons and appends it to gl.
    auto makeSmallStack = [](QHBoxLayout* gl, std::initializer_list<QAction*> acts) {
        auto* col = new QWidget;
        auto* cl = new QVBoxLayout(col);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(2);
        cl->addStretch();
        for (auto* act : acts) {
            auto* btn = new QToolButton;
            btn->setDefaultAction(act);
            btn->setIconSize({16, 16});
            btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            btn->setMinimumHeight(22);
            btn->setAutoRaise(true);
            btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            cl->addWidget(btn);
        }
        cl->addStretch();
        gl->insertWidget(gl->count()-1, col);
    };

    // ── Home tab ───────────────────────────────────────────────────────────────
    auto* homePage = new QWidget;
    auto* homeLayout = new QHBoxLayout(homePage);
    homeLayout->setContentsMargins(0, 0, 0, 0);
    homeLayout->setSpacing(0);
    homeLayout->addStretch();
    m_ribbon->addTab(homePage, "Home");

    {   // File group
        auto* gl = makeGroup(homePage, homeLayout, "File");

        auto* newAct = new QAction(themedIcon(Icons16::File_File), "New", this);
        connect(newAct, &QAction::triggered, this, &MainWindow::onNew);
        gl->insertWidget(gl->count()-1, makeLargeBtn(newAct));

        auto* openAct = new QAction(themedIcon(Icons16::File_FolderOpen), "Open", this);
        connect(openAct, &QAction::triggered, this, &MainWindow::onOpen);
        auto* saveAct = new QAction(themedIcon(Icons16::Action_Save), "Save", this);
        connect(saveAct, &QAction::triggered, this, &MainWindow::onSave);
        makeSmallStack(gl, {openAct, saveAct});
    }

    {   // Clipboard group
        auto* gl = makeGroup(homePage, homeLayout, "Clipboard");

        m_pasteAction = new QAction(themedIcon(Icons16::Action_Paste), "Paste", this);
        m_pasteAction->setEnabled(false);
        connect(m_pasteAction, &QAction::triggered, this, [this]() { doPaste(false); });

        auto* pasteMenu = new QMenu(this);
        m_pasteInPlaceAction = new QAction("Paste in Place", this);
        m_pasteInPlaceAction->setEnabled(false);
        connect(m_pasteInPlaceAction, &QAction::triggered, this, [this]() { doPaste(true); });
        pasteMenu->addAction(m_pasteInPlaceAction);
        pasteMenu->addAction("Paste special…")->setEnabled(false);

        auto* pasteBtn = new QToolButton;
        pasteBtn->setDefaultAction(m_pasteAction);
        pasteBtn->setIconSize({32, 32});
        pasteBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        pasteBtn->setMinimumHeight(56);
        pasteBtn->setAutoRaise(true);
        pasteBtn->setMenu(pasteMenu);
        pasteBtn->setPopupMode(QToolButton::MenuButtonPopup);
        gl->insertWidget(gl->count()-1, pasteBtn);

        m_cutAction = new QAction(themedIcon(Icons16::Action_Cut), "Cut", this);
        m_cutAction->setEnabled(false);
        connect(m_cutAction, &QAction::triggered, this, &MainWindow::onCut);

        m_copyAction = new QAction(themedIcon(Icons16::Action_Copy), "Copy", this);
        m_copyAction->setEnabled(false);
        connect(m_copyAction, &QAction::triggered, this, &MainWindow::onCopy);

        makeSmallStack(gl, {m_cutAction, m_copyAction});
    }

    {   // View group
        auto* gl = makeGroup(homePage, homeLayout, "View");
        auto* zoomInAct  = new QAction(themedIcon(Icons16::Action_ZoomIn),  "Zoom In",  this);
        auto* zoomOutAct = new QAction(themedIcon(Icons16::Action_ZoomOut), "Zoom Out", this);
        auto* fitAct     = new QAction(themedIcon(Icons16::Action_ZoomFit), "Fit",      this);
        connect(zoomInAct,  &QAction::triggered, m_canvas, &CanvasWidget::zoomIn);
        connect(zoomOutAct, &QAction::triggered, m_canvas, &CanvasWidget::zoomOut);
        connect(fitAct,     &QAction::triggered, m_canvas, &CanvasWidget::fitToWindow);
        makeSmallStack(gl, {zoomInAct, zoomOutAct, fitAct});
    }

    // ── Insert tab ─────────────────────────────────────────────────────────────
    {
        auto* insertPage = new QWidget;
        auto* insertLayout = new QHBoxLayout(insertPage);
        insertLayout->setContentsMargins(0, 0, 0, 0);
        insertLayout->setSpacing(0);
        insertLayout->addStretch();
        m_ribbon->addTab(insertPage, "Insert");

        {   // Graphic group
            auto* gl = makeGroup(insertPage, insertLayout, "Graphic");

            auto* addGraphicAct = new QAction(themedIcon(Icons16::Navigation_Layers1), "Graphic", this);
            connect(addGraphicAct, &QAction::triggered, this, [this]() {
                using json = nlohmann::json;
                int maxN = 0;
                for (const auto& g : m_doc->scene().graphics)
                    if (g.id.rfind("graphic_", 0) == 0)
                        try { maxN = std::max(maxN, std::stoi(g.id.substr(8))); } catch (...) {}
                std::string newId = "graphic_" + std::to_string(maxN + 1);
                int newZ = (int)m_doc->scene().graphics.size();
                json j = {{"id", newId}, {"z_order", newZ}, {"elements", json::array()}};
                m_doc->undoStack()->push(new AddGraphicCmd(m_doc, std::move(j)));
                const auto& gs = m_doc->scene().graphics;
                for (int i = 0; i < (int)gs.size(); ++i)
                    if (gs[i].id == newId)
                        { m_editorScene->setSelection({SelectionId::Level::Graphic, i, -1}); break; }
            });
            gl->insertWidget(gl->count()-1, makeLargeBtn(addGraphicAct));
        }

        auto* gl = makeGroup(insertPage, insertLayout, "Elements");

        m_addRectAction = new QAction(themedIcon(Icons16::Shape_Square), "Rectangle", this);
        m_addRectAction->setEnabled(false);
        connect(m_addRectAction, &QAction::triggered, this, [this]() {
            const SelectionId sel = m_editorScene->selection();
            int gi = (sel.level == SelectionId::Level::Graphic || sel.level == SelectionId::Level::Element)
                     ? sel.graphicIndex : -1;
            if (gi < 0 || gi >= (int)m_doc->scene().graphics.size()) return;
            const Graphic& graphic = m_doc->scene().graphics[gi];
            const std::string gid = graphic.id;
            int maxN = 0;
            for (const auto& el : graphic.elements)
                if (el.id.rfind("element_", 0) == 0)
                    try { maxN = std::max(maxN, std::stoi(el.id.substr(8))); } catch (...) {}
            std::string newId = "element_" + std::to_string(maxN + 1);
            using json = nlohmann::json;
            json j = {{"id", newId}, {"type", "rectangle"}, {"x", 100}, {"y", 100},
                      {"w", 200}, {"h", 80}, {"fill", {0.5, 0.5, 0.9, 1.0}},
                      {"z_order", (int)graphic.elements.size()}};
            m_doc->undoStack()->push(new AddElementCmd(m_doc, gid, std::move(j)));
            const auto& els = m_doc->scene().graphics[gi].elements;
            for (int ei = 0; ei < (int)els.size(); ++ei)
                if (els[ei].id == newId)
                    { m_editorScene->setSelection({SelectionId::Level::Element, gi, ei}); break; }
        });
        gl->insertWidget(gl->count()-1, makeLargeBtn(m_addRectAction));

        m_addTextAction = new QAction(themedIcon(Icons16::File_Font), "Text", this);
        m_addTextAction->setEnabled(false);
        connect(m_addTextAction, &QAction::triggered, this, [this]() {
            const SelectionId sel = m_editorScene->selection();
            int gi = (sel.level == SelectionId::Level::Graphic || sel.level == SelectionId::Level::Element)
                     ? sel.graphicIndex : -1;
            if (gi < 0 || gi >= (int)m_doc->scene().graphics.size()) return;
            const Graphic& graphic = m_doc->scene().graphics[gi];
            const std::string gid = graphic.id;
            int maxN = 0;
            for (const auto& el : graphic.elements)
                if (el.id.rfind("text_", 0) == 0)
                    try { maxN = std::max(maxN, std::stoi(el.id.substr(5))); } catch (...) {}
            std::string newId = "text_" + std::to_string(maxN + 1);
            using json = nlohmann::json;
            json j = {{"id", newId}, {"type", "text"}, {"x", 100}, {"y", 100},
                      {"w", 300}, {"h", 60}, {"text", "New Text"}, {"font_family", "Sans"},
                      {"font_size", 36}, {"color", {1.0, 1.0, 1.0, 1.0}},
                      {"z_order", (int)graphic.elements.size()}};
            m_doc->undoStack()->push(new AddElementCmd(m_doc, gid, std::move(j)));
            const auto& els = m_doc->scene().graphics[gi].elements;
            for (int ei = 0; ei < (int)els.size(); ++ei)
                if (els[ei].id == newId)
                    { m_editorScene->setSelection({SelectionId::Level::Element, gi, ei}); break; }
        });
        gl->insertWidget(gl->count()-1, makeLargeBtn(m_addTextAction));

        auto makeElementAction = [&](const std::string& prefix, const std::string& type,
                                     QIcon icon, const QString& label) -> QAction* {
            auto* act = new QAction(std::move(icon), label, this);
            act->setEnabled(false);
            connect(act, &QAction::triggered, this, [this, prefix, type]() {
                const SelectionId sel = m_editorScene->selection();
                int gi = (sel.level == SelectionId::Level::Graphic || sel.level == SelectionId::Level::Element)
                         ? sel.graphicIndex : -1;
                if (gi < 0 || gi >= (int)m_doc->scene().graphics.size()) return;
                const Graphic& graphic = m_doc->scene().graphics[gi];
                const std::string gid = graphic.id;
                int maxN = 0;
                for (const auto& el : graphic.elements)
                    if (el.id.rfind(prefix, 0) == 0)
                        try { maxN = std::max(maxN, std::stoi(el.id.substr(prefix.size()))); } catch (...) {}
                std::string newId = prefix + std::to_string(maxN + 1);
                using json = nlohmann::json;
                json j = {{"id", newId}, {"type", type}, {"x", 100}, {"y", 100},
                          {"w", 200}, {"h", 200}, {"z_order", (int)graphic.elements.size()}};
                m_doc->undoStack()->push(new AddElementCmd(m_doc, gid, std::move(j)));
                const auto& els = m_doc->scene().graphics[gi].elements;
                for (int ei = 0; ei < (int)els.size(); ++ei)
                    if (els[ei].id == newId)
                        { m_editorScene->setSelection({SelectionId::Level::Element, gi, ei}); break; }
            });
            gl->insertWidget(gl->count()-1, makeLargeBtn(act));
            return act;
        };

        m_addImageAction  = makeElementAction("image_",  "image",   themedIcon(Icons16::File_Picture), "Image");
        m_addQrCodeAction = makeElementAction("qr_",     "qr_code", qrCodeIcon(),                     "QR Code");
    }

    // ── Scene tab — always visible, holds scene name + canvas dimensions ────────
    {
        auto* scenePage = new QWidget;
        auto* sceneLayout = new QHBoxLayout(scenePage);
        sceneLayout->setContentsMargins(0, 0, 0, 0);
        sceneLayout->setSpacing(0);
        sceneLayout->addStretch();
        m_ribbon->addTab(scenePage, "Scene");

        auto sceneLabel = [](const QString& text) -> QLabel* {
            auto* lbl = new QLabel(text);
            lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            lbl->setStyleSheet("font-size: 12px;");
            return lbl;
        };

        auto makeSceneGroup = [&](const QString& name) -> QGridLayout* {
            auto* grp = new Nedrysoft::Ribbon::RibbonGroup(scenePage);
            grp->setGroupName(name);
            grp->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
            auto* gl = new QGridLayout(grp);
            gl->setContentsMargins(4, 2, 4, 2);
            gl->setSpacing(4);
            sceneLayout->insertWidget(sceneLayout->count() - 1, grp);
            return gl;
        };

        {   // Info group: scene name
            auto* gl = makeSceneGroup("Info");
            m_sceneNameEdit = new QLineEdit;
            m_sceneNameEdit->setFixedWidth(160);
            gl->addWidget(sceneLabel("Name"), 0, 0, 2, 1);
            gl->addWidget(m_sceneNameEdit, 0, 1, 2, 1, Qt::AlignVCenter);
        }

        {   // Canvas group: preset + W × H
            auto* gl = makeSceneGroup("Canvas");
            m_scenePresetCombo = new QComboBox;
            m_scenePresetCombo->setFixedWidth(180);
            for (const auto& p : kScenePresets)
                m_scenePresetCombo->addItem(p.label);

            m_sceneWidthSpin = new QSpinBox;
            m_sceneWidthSpin->setRange(1, 7680);
            m_sceneWidthSpin->setSuffix(" px");
            m_sceneWidthSpin->setFixedWidth(80);

            m_sceneHeightSpin = new QSpinBox;
            m_sceneHeightSpin->setRange(1, 7680);
            m_sceneHeightSpin->setSuffix(" px");
            m_sceneHeightSpin->setFixedWidth(80);

            gl->addWidget(sceneLabel("Preset"), 0, 0);
            gl->addWidget(m_scenePresetCombo, 0, 1, 1, 3);
            gl->addWidget(sceneLabel("W"), 1, 0);
            gl->addWidget(m_sceneWidthSpin, 1, 1);
            gl->addWidget(new QLabel("×"), 1, 2, Qt::AlignCenter);
            gl->addWidget(m_sceneHeightSpin, 1, 3);
        }

        // Sync scene ribbon from document
        auto updateSceneRibbon = [this]() {
            if (m_sceneUpdating) return;
            m_sceneUpdating = true;
            const Scene& s = m_doc->scene();
            m_sceneNameEdit->setText(m_doc->sceneName());
            m_sceneWidthSpin->setValue(s.width);
            m_sceneHeightSpin->setValue(s.height);
            int idx = kSceneCustomIdx;
            for (int i = 0; i < kSceneCustomIdx; ++i)
                if (kScenePresets[i].w == s.width && kScenePresets[i].h == s.height)
                    { idx = i; break; }
            m_scenePresetCombo->setCurrentIndex(idx);
            m_sceneUpdating = false;
        };
        connect(m_doc, &SceneDocument::documentChanged, this, updateSceneRibbon);

        connect(m_sceneNameEdit, &QLineEdit::editingFinished, this, [this]() {
            if (m_sceneUpdating) return;
            QString name = m_sceneNameEdit->text().trimmed();
            if (name == m_doc->sceneName()) return;
            m_doc->undoStack()->push(new SetSceneNameCmd(m_doc, name));
        });

        connect(m_scenePresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            if (m_sceneUpdating || index < 0 || index >= kSceneCustomIdx) return;
            int w = kScenePresets[index].w, h = kScenePresets[index].h;
            if (w == m_doc->scene().width && h == m_doc->scene().height) return;
            m_sceneUpdating = true;
            m_sceneWidthSpin->setValue(w);
            m_sceneHeightSpin->setValue(h);
            m_sceneUpdating = false;
            m_doc->undoStack()->push(new SetSceneDimensionsCmd(m_doc, w, h));
        });

        connect(m_sceneWidthSpin, &QSpinBox::editingFinished, this, [this]() {
            if (m_sceneUpdating) return;
            int w = m_sceneWidthSpin->value(), h = m_sceneHeightSpin->value();
            if (w == m_doc->scene().width) return;
            m_doc->undoStack()->push(new SetSceneDimensionsCmd(m_doc, w, h));
        });

        connect(m_sceneHeightSpin, &QSpinBox::editingFinished, this, [this]() {
            if (m_sceneUpdating) return;
            int w = m_sceneWidthSpin->value(), h = m_sceneHeightSpin->value();
            if (h == m_doc->scene().height) return;
            m_doc->undoStack()->push(new SetSceneDimensionsCmd(m_doc, w, h));
        });

        updateSceneRibbon();
    }

    // Contextual Graphic + Element + Style + Text tabs created by RibbonFormatSection
    m_formatSection = new RibbonFormatSection(m_doc, m_ribbon, this);
    m_ribbon->tabBar()->setTabVisible(m_formatSection->graphicTabIndex(), false);
    m_ribbon->tabBar()->setTabVisible(m_formatSection->elementTabIndex(), false);
    m_ribbon->tabBar()->setTabVisible(m_formatSection->styleTabIndex(),   false);
    m_ribbon->tabBar()->setTabVisible(m_formatSection->textTabIndex(),    false);

    connect(m_formatSection, &RibbonFormatSection::deleteGraphicRequested, this, [this]() {
        const SelectionId sel = m_editorScene->selection();
        if (sel.level != SelectionId::Level::Graphic) return;
        const std::string gid = m_doc->scene().graphics[sel.graphicIndex].id;
        m_doc->undoStack()->push(new RemoveGraphicCmd(m_doc, gid));
    });

    connect(m_formatSection, &RibbonFormatSection::deleteElementRequested, this, [this]() {
        const SelectionId sel = m_editorScene->selection();
        if (sel.level != SelectionId::Level::Element) return;
        const auto& g = m_doc->scene().graphics[sel.graphicIndex];
        m_doc->undoStack()->push(
            new RemoveElementCmd(m_doc, g.id, g.elements[sel.elementIndex].id));
    });
}

// ── Menu bar ──────────────────────────────────────────────────────────────────

void MainWindow::setupMenuBar()
{
    // File
    auto* fileMenu = menuBar()->addMenu("&File");

    auto* newAct = fileMenu->addAction("&New");
    newAct->setShortcut(QKeySequence::New);
    connect(newAct, &QAction::triggered, this, &MainWindow::onNew);

    auto* openAct = fileMenu->addAction("&Open...");
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpen);

    fileMenu->addSeparator();

    auto* saveAct = fileMenu->addAction("&Save");
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::onSave);

    auto* saveAsAct = fileMenu->addAction("Save &As...");
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::onSaveAs);

    fileMenu->addSeparator();

    auto* exitAct = fileMenu->addAction("E&xit");
    exitAct->setShortcut(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // Edit
    auto* editMenu = menuBar()->addMenu("&Edit");

    m_undoAction = m_doc->undoStack()->createUndoAction(this, "&Undo");
    m_undoAction->setShortcut(QKeySequence::Undo);
    editMenu->addAction(m_undoAction);

    m_redoAction = m_doc->undoStack()->createRedoAction(this, "&Redo");
    m_redoAction->setShortcuts({QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z),
                                QKeySequence(Qt::CTRL | Qt::Key_Y)});
    editMenu->addAction(m_redoAction);

    editMenu->addSeparator();

    m_cutAction->setShortcut(QKeySequence::Cut);
    editMenu->addAction(m_cutAction);

    m_copyAction->setShortcut(QKeySequence::Copy);
    editMenu->addAction(m_copyAction);

    m_pasteAction->setShortcut(QKeySequence::Paste);
    editMenu->addAction(m_pasteAction);

    m_pasteInPlaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    editMenu->addAction(m_pasteInPlaceAction);

    // View
    auto* viewMenu = menuBar()->addMenu("&View");

    auto* snapAct = viewMenu->addAction("Snapping");
    snapAct->setCheckable(true);
    snapAct->setChecked(true);
    snapAct->setShortcut(QKeySequence(Qt::Key_Semicolon));
    connect(snapAct, &QAction::toggled, m_canvas, &CanvasWidget::setSnapping);

    viewMenu->addSeparator();

    auto* zoomInAct  = viewMenu->addAction("Zoom In",      m_canvas, &CanvasWidget::zoomIn);
    auto* zoomOutAct = viewMenu->addAction("Zoom Out",     m_canvas, &CanvasWidget::zoomOut);
    auto* fitAct     = viewMenu->addAction("Fit to Window",m_canvas, &CanvasWidget::fitToWindow);
    zoomInAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal));
    zoomOutAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    fitAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    viewMenu->addSeparator();

    auto* guidesMenu = viewMenu->addMenu("Guides");
    auto makeGuide = [&](const QString& label, CanvasWidget::GuideFlag flag, bool on) {
        auto* act = guidesMenu->addAction(label);
        act->setCheckable(true);
        act->setChecked(on);
        connect(act, &QAction::toggled, this, [this, flag](bool checked) {
            CanvasWidget::GuideFlags f = m_canvas->guides();
            if (checked) f |= flag; else f &= ~flag;
            m_canvas->setGuides(f);
        });
    };
    makeGuide("Rule of Thirds",   CanvasWidget::GuideRuleOfThirds, true);
    makeGuide("Center Lines",     CanvasWidget::GuideCenterLines,  true);
    makeGuide("Title Safe (90%)", CanvasWidget::GuideTitleSafe,    false);
    makeGuide("Action Safe (80%)",CanvasWidget::GuideActionSafe,   false);

    // Help
    auto* helpMenu = menuBar()->addMenu("&Help");

    auto* shortcutsAct = helpMenu->addAction("Keyboard &Shortcuts...");
    shortcutsAct->setShortcut(QKeySequence(Qt::Key_F1));
    connect(shortcutsAct, &QAction::triggered, this, [this]() {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("Keyboard Shortcuts");
        dlg->resize(500, 520);
        auto* browser = new QTextBrowser(dlg);
        browser->setHtml(
            "<style>"
            "body { font-family: sans-serif; font-size: 13px; margin: 8px; }"
            "h3 { margin-top: 14px; margin-bottom: 2px; color: #555; font-size: 12px;"
            "     text-transform: uppercase; letter-spacing: 1px; }"
            "table { border-collapse: collapse; width: 100%; margin-bottom: 4px; }"
            "td { padding: 3px 6px; border-bottom: 1px solid #eee; }"
            "td:first-child { font-family: monospace; font-weight: bold;"
            "                 white-space: nowrap; color: #333; width: 200px; }"
            "</style>"
            "<h3>File</h3>"
            "<table>"
            "<tr><td>Ctrl+N</td><td>New scene</td></tr>"
            "<tr><td>Ctrl+O</td><td>Open scene file</td></tr>"
            "<tr><td>Ctrl+S</td><td>Save</td></tr>"
            "<tr><td>Ctrl+Shift+S</td><td>Save As</td></tr>"
            "<tr><td>Ctrl+Q</td><td>Quit</td></tr>"
            "</table>"
            "<h3>Edit</h3>"
            "<table>"
            "<tr><td>Ctrl+Z</td><td>Undo</td></tr>"
            "<tr><td>Ctrl+Shift+Z &nbsp;/&nbsp; Ctrl+Y</td><td>Redo</td></tr>"
            "<tr><td>Ctrl+C</td><td>Copy selected graphic or element</td></tr>"
            "<tr><td>Ctrl+X</td><td>Cut selected graphic or element</td></tr>"
            "<tr><td>Ctrl+V</td><td>Paste (offset +10 px)</td></tr>"
            "<tr><td>Ctrl+Shift+V</td><td>Paste in place</td></tr>"
            "</table>"
            "<h3>View</h3>"
            "<table>"
            "<tr><td>Ctrl+=</td><td>Zoom in</td></tr>"
            "<tr><td>Ctrl+−</td><td>Zoom out</td></tr>"
            "<tr><td>Ctrl+0</td><td>Fit canvas to window</td></tr>"
            "<tr><td>;</td><td>Toggle snapping</td></tr>"
            "</table>"
            "<h3>Canvas</h3>"
            "<table>"
            "<tr><td>Click</td><td>Select element or graphic</td></tr>"
            "<tr><td>Drag element</td><td>Move element</td></tr>"
            "<tr><td>Drag handle</td><td>Resize element</td></tr>"
            "<tr><td>Shift + drag corner</td><td>Resize proportionally</td></tr>"
            "<tr><td>Ctrl + drag corner</td><td>Resize from center</td></tr>"
            "<tr><td>← ↑ → ↓</td><td>Nudge element / graphic 1 px</td></tr>"
            "<tr><td>Shift + ← ↑ → ↓</td><td>Nudge element / graphic 10 px</td></tr>"
            "<tr><td>Middle mouse drag</td><td>Pan canvas</td></tr>"
            "<tr><td>Scroll wheel</td><td>Zoom canvas</td></tr>"
            "</table>"
            "<h3>Insert (requires a graphic selected)</h3>"
            "<table>"
            "<tr><td>Insert tab → Graphic</td><td>Add new graphic layer</td></tr>"
            "<tr><td>Insert tab → Rectangle</td><td>Add rectangle element</td></tr>"
            "<tr><td>Insert tab → Text</td><td>Add text element</td></tr>"
            "<tr><td>Insert tab → Image</td><td>Add image element</td></tr>"
            "<tr><td>Insert tab → QR Code</td><td>Add QR code element</td></tr>"
            "</table>"
            "<h3>Misc</h3>"
            "<table>"
            "<tr><td>F1</td><td>Show this shortcuts reference</td></tr>"
            "</table>"
        );
        auto* layout = new QVBoxLayout(dlg);
        layout->addWidget(browser);
        auto* btns = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
        connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::accept);
        layout->addWidget(btns);
        dlg->exec();
    });

    helpMenu->addSeparator();

    auto* aboutAct = helpMenu->addAction("&About...");
    connect(aboutAct, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About obs-graphics Scene Editor",
            "<b>obs-graphics Scene Editor</b> &mdash; version 0.1<br><br>"
            "A standalone desktop editor for authoring animated broadcast graphics scenes.<br>"
            "Scenes are saved as JSON and consumed by the <i>obs-graphics</i> OBS Studio plugin.<br><br>"
            "Built with Qt 6 and Cairo.");
    });
}

// ── File operations ───────────────────────────────────────────────────────────

void MainWindow::onNew()
{
    if (!maybeSave()) return;
    m_doc->reset();
    m_editorScene->setSelection(SelectionId{});
}

void MainWindow::onOpen()
{
    if (!maybeSave()) return;
    QString path = QFileDialog::getOpenFileName(this, "Open Scene", QString(),
                                                "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;
    if (!m_doc->load(path))
        QMessageBox::warning(this, "Open Failed",
                             QString("Could not open file:\n%1").arg(path));
    else
        m_editorScene->setSelection(SelectionId{});
}

void MainWindow::onSave()
{
    if (m_doc->filePath().isEmpty()) { onSaveAs(); return; }
    if (!m_doc->save())
        QMessageBox::warning(this, "Save Failed", "Could not save the file.");
}

void MainWindow::onSaveAs()
{
    QString path = QFileDialog::getSaveFileName(this, "Save Scene As", QString(),
                                                "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;
    if (!m_doc->saveAs(path))
        QMessageBox::warning(this, "Save Failed",
                             QString("Could not save file:\n%1").arg(path));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    maybeSave() ? event->accept() : event->ignore();
}

bool MainWindow::maybeSave()
{
    if (!m_doc->isModified()) return true;
    auto result = QMessageBox::question(
        this, "Unsaved Changes",
        QString("The scene \"%1\" has unsaved changes.\nSave before continuing?")
            .arg(m_doc->sceneName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (result == QMessageBox::Save) { onSave(); return !m_doc->isModified(); }
    return result == QMessageBox::Discard;
}

void MainWindow::updateWindowTitle()
{
    setWindowTitle(QString("%1[*] — obs-graphics Scene Editor").arg(m_doc->sceneName()));
}

// ── Selection ─────────────────────────────────────────────────────────────────

void MainWindow::updateToolBarState(SelectionId id)
{
    bool hasGraphic = (id.level == SelectionId::Level::Graphic ||
                       id.level == SelectionId::Level::Element);

    if (m_addRectAction)   m_addRectAction->setEnabled(hasGraphic);
    if (m_addTextAction)   m_addTextAction->setEnabled(hasGraphic);
    if (m_addImageAction)  m_addImageAction->setEnabled(hasGraphic);
    if (m_addQrCodeAction) m_addQrCodeAction->setEnabled(hasGraphic);

    bool hasSelection = (id.level == SelectionId::Level::Graphic ||
                         id.level == SelectionId::Level::Element);
    if (m_cutAction)  m_cutAction->setEnabled(hasSelection);
    if (m_copyAction) m_copyAction->setEnabled(hasSelection);

    bool canPaste = false;
    if (m_clipboard.has_value()) {
        std::string clipType = m_clipboard->value("_clipboard_type", "");
        canPaste = (clipType == "graphic") ||
                   (clipType == "element" && hasGraphic);
    }
    if (!canPaste && hasGraphic)
        canPaste = clipboardHasExternalImage();
    if (m_pasteAction)        m_pasteAction->setEnabled(canPaste);
    if (m_pasteInPlaceAction) m_pasteInPlaceAction->setEnabled(canPaste);
}

void MainWindow::onSelectionChanged(SelectionId id)
{
    updateToolBarState(id);

    if (id.level == SelectionId::Level::Element) {
        const Scene& s = m_doc->scene();
        if (id.graphicIndex < 0 || id.graphicIndex >= (int)s.graphics.size()) return;
        const Graphic& g = s.graphics[id.graphicIndex];
        if (id.elementIndex < 0 || id.elementIndex >= (int)g.elements.size()) return;

        const std::string gi = g.id;
        const std::string ei = g.elements[id.elementIndex].id;
        const auto elType = g.elements[id.elementIndex].type;
        const bool isText  = elType == ElementType::Text;
        const bool isImage = elType == ElementType::Image;
        const bool isQr    = elType == ElementType::QrCode;

        m_formatSection->setSelection(gi, ei);
        m_timingEditor->loadGraphic(id.graphicIndex);

        m_ribbon->tabBar()->setTabVisible(m_formatSection->graphicTabIndex(), false);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->elementTabIndex(), true);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->styleTabIndex(),   true);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->textTabIndex(),  isText);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->imageTabIndex(), isImage);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->qrTabIndex(),    isQr);
        m_ribbon->setCurrentIndex(m_formatSection->elementTabIndex());

    } else if (id.level == SelectionId::Level::Graphic) {
        const Scene& s = m_doc->scene();
        if (id.graphicIndex < 0 || id.graphicIndex >= (int)s.graphics.size()) return;

        const std::string gid = s.graphics[id.graphicIndex].id;
        m_formatSection->setGraphicSelection(gid);
        m_timingEditor->loadGraphic(id.graphicIndex);

        m_ribbon->tabBar()->setTabVisible(m_formatSection->graphicTabIndex(), true);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->elementTabIndex(), false);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->styleTabIndex(),   false);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->textTabIndex(),    false);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->imageTabIndex(),   false);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->qrTabIndex(),      false);
        m_ribbon->setCurrentIndex(m_formatSection->graphicTabIndex());

    } else {
        m_formatSection->clearSelection();
        m_ribbon->tabBar()->setTabVisible(m_formatSection->graphicTabIndex(), false);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->elementTabIndex(), false);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->styleTabIndex(),   false);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->textTabIndex(),    false);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->imageTabIndex(),   false);
        m_ribbon->tabBar()->setTabVisible(m_formatSection->qrTabIndex(),      false);

        m_timingEditor->clear();
    }
}

// ── Clipboard ─────────────────────────────────────────────────────────────────

void MainWindow::onCopy()
{
    const SelectionId sel = m_editorScene->selection();
    const Scene& s = m_doc->scene();

    if (sel.level == SelectionId::Level::Graphic &&
        sel.graphicIndex >= 0 && sel.graphicIndex < (int)s.graphics.size()) {
        nlohmann::json full = SceneDocument::sceneToJson(s);
        nlohmann::json gj = full["graphics"][sel.graphicIndex];
        gj["_clipboard_type"] = "graphic";
        m_clipboard = std::move(gj);

    } else if (sel.level == SelectionId::Level::Element &&
               sel.graphicIndex >= 0 && sel.graphicIndex < (int)s.graphics.size()) {
        const Graphic& g = s.graphics[sel.graphicIndex];
        if (sel.elementIndex >= 0 && sel.elementIndex < (int)g.elements.size()) {
            nlohmann::json full = SceneDocument::sceneToJson(s);
            nlohmann::json ej = full["graphics"][sel.graphicIndex]["elements"][sel.elementIndex];
            ej["_clipboard_type"] = "element";
            m_clipboard = std::move(ej);
        }
    }
    updateToolBarState(m_editorScene->selection());
}

void MainWindow::onCut()
{
    const SelectionId sel = m_editorScene->selection();
    onCopy();
    if (!m_clipboard.has_value()) return;

    if (sel.level == SelectionId::Level::Graphic) {
        if (sel.graphicIndex < 0 || sel.graphicIndex >= (int)m_doc->scene().graphics.size()) return;
        const std::string gid = m_doc->scene().graphics[sel.graphicIndex].id;
        m_doc->undoStack()->push(new RemoveGraphicCmd(m_doc, gid));
    } else if (sel.level == SelectionId::Level::Element) {
        if (sel.graphicIndex < 0 || sel.graphicIndex >= (int)m_doc->scene().graphics.size()) return;
        const auto& g = m_doc->scene().graphics[sel.graphicIndex];
        if (sel.elementIndex < 0 || sel.elementIndex >= (int)g.elements.size()) return;
        m_doc->undoStack()->push(new RemoveElementCmd(m_doc, g.id, g.elements[sel.elementIndex].id));
    }
}

void MainWindow::doPaste(bool inPlace)
{
    using json = nlohmann::json;

    // ── Internal clipboard (app-internal cut/copy takes priority) ────────────
    if (m_clipboard.has_value()) {
        json cb = *m_clipboard;
        std::string clipType = cb.value("_clipboard_type", "");
        cb.erase("_clipboard_type");
        cb.erase("mask");
        cb.erase("parent");
        const double offset = inPlace ? 0.0 : 10.0;

        if (clipType == "graphic") {
            int maxN = 0;
            for (const auto& g : m_doc->scene().graphics)
                if (g.id.rfind("graphic_", 0) == 0)
                    try { maxN = std::max(maxN, std::stoi(g.id.substr(8))); } catch (...) {}
            std::string newId = "graphic_" + std::to_string(maxN + 1);
            cb["id"] = newId;
            cb["z_order"] = (int)m_doc->scene().graphics.size();

            if (cb.contains("elements") && cb["elements"].is_array()) {
                for (auto& ej : cb["elements"]) {
                    ej.erase("mask");
                    ej.erase("parent");
                    if (offset != 0.0) {
                        if (ej.contains("x")) ej["x"] = ej["x"].get<double>() + offset;
                        if (ej.contains("y")) ej["y"] = ej["y"].get<double>() + offset;
                    }
                }
            }

            m_doc->undoStack()->push(new AddGraphicCmd(m_doc, std::move(cb)));

            const auto& gs = m_doc->scene().graphics;
            for (int i = 0; i < (int)gs.size(); ++i)
                if (gs[i].id == newId)
                    { m_editorScene->setSelection({SelectionId::Level::Graphic, i, -1}); break; }

        } else if (clipType == "element") {
            const SelectionId sel = m_editorScene->selection();
            int gi = -1;
            if (sel.level == SelectionId::Level::Graphic || sel.level == SelectionId::Level::Element)
                gi = sel.graphicIndex;
            if (gi < 0 || gi >= (int)m_doc->scene().graphics.size()) {
                updateToolBarState(m_editorScene->selection());
                return;
            }

            const Graphic& graphic = m_doc->scene().graphics[gi];
            const std::string gid = graphic.id;

            std::string origType = cb.value("type", "rectangle");
            std::string prefix = "element_";
            if (origType == "text")     prefix = "text_";
            else if (origType == "image")    prefix = "image_";
            else if (origType == "qr_code")  prefix = "qr_";

            int maxN = 0;
            for (const auto& el : graphic.elements)
                if (el.id.rfind(prefix, 0) == 0)
                    try { maxN = std::max(maxN, std::stoi(el.id.substr(prefix.size()))); } catch (...) {}
            std::string newId = prefix + std::to_string(maxN + 1);

            cb["id"] = newId;
            cb["z_order"] = (int)graphic.elements.size();
            if (offset != 0.0) {
                if (cb.contains("x")) cb["x"] = cb["x"].get<double>() + offset;
                if (cb.contains("y")) cb["y"] = cb["y"].get<double>() + offset;
            }

            m_doc->undoStack()->push(new AddElementCmd(m_doc, gid, std::move(cb)));

            const auto& els = m_doc->scene().graphics[gi].elements;
            for (int ei = 0; ei < (int)els.size(); ++ei)
                if (els[ei].id == newId)
                    { m_editorScene->setSelection({SelectionId::Level::Element, gi, ei}); break; }
        }

        updateToolBarState(m_editorScene->selection());
        return;
    }

    // ── System clipboard image ────────────────────────────────────────────────
    const QMimeData* md = QGuiApplication::clipboard()->mimeData();
    if (!md) return;

    // Prefer a local file URL so we don't need to save anything
    QString imagePath;
    QImage img;
    if (md->hasUrls()) {
        for (const QUrl& url : md->urls()) {
            if (!url.isLocalFile()) continue;
            QString p = url.toLocalFile();
            if (kImageExts.contains(QFileInfo(p).suffix().toLower())) {
                imagePath = p;
                img.load(imagePath);
                break;
            }
        }
    }
    // Fall back to raw pixel data
    if (imagePath.isEmpty()) {
        if (md->hasImage())
            img = qvariant_cast<QImage>(md->imageData());
        // hasImage() can return false even when image/* formats are present (Linux/Wayland)
        if (img.isNull()) {
            for (const QString& fmt : md->formats()) {
                if (!fmt.startsWith("image/")) continue;
                QByteArray raw = md->data(fmt);
                if (!raw.isEmpty() && img.loadFromData(raw)) break;
            }
        }
    }

    if (img.isNull() && imagePath.isEmpty()) return;

    // Need a target graphic
    const SelectionId sel = m_editorScene->selection();
    int gi = -1;
    if (sel.level == SelectionId::Level::Graphic || sel.level == SelectionId::Level::Element)
        gi = sel.graphicIndex;
    if (gi < 0 || gi >= (int)m_doc->scene().graphics.size()) {
        QMessageBox::information(this, "Paste Image",
            "Select a graphic layer first to paste an image into.");
        return;
    }

    // Raw pixel data needs to be saved to disk first
    if (imagePath.isEmpty()) {
        QString defaultDir = m_doc->filePath().isEmpty()
            ? QDir::homePath()
            : QFileInfo(m_doc->filePath()).absolutePath();
        imagePath = QFileDialog::getSaveFileName(
            this, "Save Clipboard Image",
            defaultDir + "/clipboard_image.png",
            "PNG Images (*.png);;JPEG Images (*.jpg *.jpeg);;All Files (*)");
        if (imagePath.isEmpty()) return;
        if (!img.save(imagePath)) {
            QMessageBox::warning(this, "Save Failed",
                "Could not save the clipboard image.");
            return;
        }
    }

    // Load dimensions if we only resolved a URL (img still null)
    if (img.isNull()) img.load(imagePath);

    // Build element, scale down to fit scene if necessary
    double w = img.isNull() ? 200.0 : img.width();
    double h = img.isNull() ? 200.0 : img.height();
    const Scene& sc = m_doc->scene();
    if (w > sc.width)  { h = h * sc.width  / w; w = sc.width; }
    if (h > sc.height) { w = w * sc.height / h; h = sc.height; }

    const Graphic& graphic = m_doc->scene().graphics[gi];
    const std::string gid = graphic.id;

    int maxN = 0;
    for (const auto& el : graphic.elements)
        if (el.id.rfind("image_", 0) == 0)
            try { maxN = std::max(maxN, std::stoi(el.id.substr(6))); } catch (...) {}
    std::string newId = "image_" + std::to_string(maxN + 1);

    json j = {{"id", newId}, {"type", "image"},
              {"x", inPlace ? 0.0 : 100.0},
              {"y", inPlace ? 0.0 : 100.0},
              {"w", w}, {"h", h},
              {"z_order", (int)graphic.elements.size()},
              {"image_path", imagePath.toStdString()},
              {"scale_mode", "contain"}};

    m_doc->undoStack()->push(new AddElementCmd(m_doc, gid, std::move(j)));

    const auto& els = m_doc->scene().graphics[gi].elements;
    for (int ei = 0; ei < (int)els.size(); ++ei)
        if (els[ei].id == newId)
            { m_editorScene->setSelection({SelectionId::Level::Element, gi, ei}); break; }

    updateToolBarState(m_editorScene->selection());
}
