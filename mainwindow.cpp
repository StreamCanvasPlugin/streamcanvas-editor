#include "mainwindow.h"
#include "icons.h"
#include "model/EditorTitle.h"
#include "model/TitleDocument.h"
#include "model/UndoCommands.h"
#include "ui/widgets/CanvasWidget.h"
#include "ui/widgets/AnimationTimingPanel.h"
#include "ui/widgets/RibbonFormatSection.h"
#include "ui/widgets/TitleTreeView.h"
#include "ui/UiUtils.h"
#include "engine/element_image.h"
#include "engine/element_qr.h"
#include "engine/element_text.h"
#include "engine/title.h"
#include "engine/visual_element.h"
#include <SARibbonMainWindow.h>
#include <SARibbonCategory.h>
#include <SARibbonPanel.h>
#include <SARibbonBar.h>
#include <SAFramelessHelper.h>
#include <SARibbonSystemButtonBar.h>

#include <algorithm>
#include <qcheckbox.h>
#include <string>

#include <QAction>
#include <QClipboard>
#include <QCloseEvent>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
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
#include <QPalette>
#include <QSpinBox>
#include <QStatusBar>
#include <QTextBrowser>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QToolButton>

static const struct { const char* label; int w, h; } kTitlePresets[] = {
    {"1920 × 1080  (16:9 FHD)", 1920, 1080},
    {"1280 × 720   (16:9 HD)",  1280,  720},
    {"3840 × 2160  (4K UHD)",   3840, 2160},
    {"1080 × 1920  (9:16 Vert)",1080, 1920},
    {"2560 × 1440  (16:9 QHD)", 2560, 1440},
    {"Custom", 0, 0},
};
static constexpr int kTitleCustomIdx = 5;

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
    : SARibbonMainWindow(parent), m_doc(new TitleDocument(this)),
      m_editorTitle(new EditorTitle(m_doc, this)),
      m_canvas(new CanvasWidget(m_doc, m_editorTitle, this))
{
#ifdef Q_OS_LINUX
    if (auto* helper = framelessHelper()) {
        helper->removeFrom(this);
    }
    setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
#endif

    showMaximized();

    setRibbonTheme(SARibbonTheme::RibbonThemeDark);
    m_ribbon = ribbonBar();

#ifdef Q_OS_LINUX
    m_ribbon->setTitleBarHeight(0);
    m_ribbon->setTitleVisible(false);
    m_ribbon->setTitleIconVisible(false);
    if (auto* sysBar = windowButtonBar()) {
        sysBar->hide();
    }
#endif

    setupRibbon();

    setCentralWidget(m_canvas);

    // Title tree — left dock
    auto* titleDock = new QDockWidget("Title", this);
    titleDock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
    m_treeView = new TitleTreeView(m_doc, m_editorTitle, titleDock);
    titleDock->setWidget(m_treeView);
    addDockWidget(Qt::LeftDockWidgetArea, titleDock);

    connect(m_treeView, &TitleTreeView::duplicateRequested, this, &MainWindow::onDuplicate);
    connect(m_treeView, &TitleTreeView::cutRequested, this, &MainWindow::onCut);
    connect(m_treeView, &TitleTreeView::copyRequested, this, &MainWindow::onCopy);
    connect(m_treeView, &TitleTreeView::pasteRequested, this, [this]() { doPaste(false); });
    connect(m_treeView, &TitleTreeView::pasteInPlaceRequested, this, [this]() { doPaste(true); });

    setCorner(Qt::BottomLeftCorner,  Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    // Animation timing — bottom dock
    auto* timingDock = new QDockWidget("Animation Timing", this);
    timingDock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
    m_timingPanel = new AnimationTimingPanel(m_doc, m_editorTitle, timingDock);
    timingDock->setWidget(m_timingPanel);
    addDockWidget(Qt::BottomDockWidgetArea, timingDock);

    connect(m_timingPanel, &AnimationTimingPanel::scrubTimeChanged, this,
            [this](bool isIn, bool isData, double t) {
        m_canvas->previewAtTime(isIn, isData, t);
    });
    connect(m_timingPanel, &AnimationTimingPanel::previewStopped,
            m_canvas, &CanvasWidget::stopAnimationPreview);

    setupMenuBar();
    updateWindowTitle();

    connect(QGuiApplication::clipboard(), &QClipboard::changed, this,
        [this](QClipboard::Mode mode) {
            if (mode == QClipboard::Clipboard)
                updateToolBarState(m_editorTitle->selection());
        });

    connect(m_editorTitle, &EditorTitle::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(m_editorTitle, &EditorTitle::selectionSetChanged, this, &MainWindow::updateSelectionStatus);

    connect(m_formatSection, &RibbonFormatSection::elementIdChanged,
            [this](const std::string&) {
                // RibbonFormatSection tracks the new ID internally; nothing to sync.
            });

    connect(m_doc, &TitleDocument::modifiedChanged, this, &MainWindow::setWindowModified);
    connect(m_doc, &TitleDocument::documentChanged, this, &MainWindow::updateWindowTitle);
    connect(m_doc, &TitleDocument::filePathChanged, this, [this](const QString& path) {
        updateWindowTitle();
        statusBar()->showMessage(path.isEmpty() ? "Untitled" : path);
    });

    statusBar()->showMessage("Untitled");
}

// ── Ribbon setup ──────────────────────────────────────────────────────────────

void MainWindow::setupRibbon()
{
    auto padded = [](QWidget* w, int m = 5) -> QWidget* {
        auto* outer = new QWidget;
        auto* l = new QHBoxLayout(outer);
        l->setContentsMargins(m, m, m, m);
        l->setSpacing(0);
        l->addWidget(w);
        return outer;
    };

    auto makeLargeBtn = [&padded](QAction* act) -> QWidget* {
        auto* btn = new QToolButton;
        btn->setDefaultAction(act);
        btn->setIconSize({32, 32});
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setMinimumHeight(56);
        btn->setAutoRaise(true);
        return padded(btn);
    };

    auto makeSmallStack = [](std::initializer_list<QAction*> acts) -> QWidget* {
        auto* col = new QWidget;
        auto* cl = new QVBoxLayout(col);
        cl->setContentsMargins(5, 5, 5, 5);
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
        return col;
    };

    // ── Pre-create shared actions ─────────────────────────────────────────────

    m_newAction = new QAction(themedIcon(Icons16::File_File), "New", this);
    m_newAction->setShortcut(QKeySequence::New);
    connect(m_newAction, &QAction::triggered, this, &MainWindow::onNew);
    addAction(m_newAction);

    m_openAction = new QAction(themedIcon(Icons16::File_FolderOpen), "Open…", this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::onOpen);
    addAction(m_openAction);

    m_saveAction = new QAction(themedIcon(Icons16::Action_Save), "Save", this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSave);
    addAction(m_saveAction);

    m_saveAsAction = new QAction("Save As…", this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::onSaveAs);
    addAction(m_saveAsAction);

    m_exitAction = new QAction("Exit", this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    addAction(m_exitAction);

    m_undoAction = m_doc->undoStack()->createUndoAction(this, "Undo");
    m_undoAction->setIcon(themedIcon(Icons16::Action_Undo));
    m_undoAction->setShortcut(QKeySequence::Undo);
    addAction(m_undoAction);

    m_redoAction = m_doc->undoStack()->createRedoAction(this, "Redo");
    m_redoAction->setIcon(themedIcon(Icons16::Action_Redo));
    m_redoAction->setShortcuts({QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z),
                                QKeySequence(Qt::CTRL | Qt::Key_Y)});
    addAction(m_redoAction);

    m_shortcutsAction = new QAction(themedIcon(Icons16::Misc_Help), "Keyboard Shortcuts…", this);
    m_shortcutsAction->setShortcut(Qt::Key_F1);
    connect(m_shortcutsAction, &QAction::triggered, this, [this]() {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("Keyboard Shortcuts");
        dlg->resize(500, 520);
        auto* browser = new QTextBrowser(dlg);
        const QPalette& pal = dlg->palette();
        const QString cHead   = pal.color(QPalette::PlaceholderText).name(); // h3
        const QString cBorder = pal.color(QPalette::Mid).name();             // td border
        const QString cText   = pal.color(QPalette::Text).name();            // td body
        const QString cKey    = pal.color(QPalette::BrightText).name();      // td:first key
        browser->setHtml(QString(
            "<style>"
            "body { font-family: sans-serif; font-size: 13px; margin: 8px; }"
            "h3 { margin-top: 14px; margin-bottom: 2px; color: %1; font-size: 12px;"
            "     text-transform: uppercase; letter-spacing: 1px; }"
            "table { border-collapse: collapse; width: 100%; margin-bottom: 4px; }"
            "td { padding: 3px 6px; border-bottom: 1px solid %2; color: %3; }"
            "td:first-child { font-family: monospace; font-weight: bold;"
            "                 white-space: nowrap; color: %4; width: 200px; }"
            "</style>"
            "<h3>File</h3><table>"
            "<tr><td>Ctrl+N</td><td>New title</td></tr>"
            "<tr><td>Ctrl+O</td><td>Open .ogt title file</td></tr>"
            "<tr><td>Ctrl+S</td><td>Save</td></tr>"
            "<tr><td>Ctrl+Shift+S</td><td>Save As</td></tr>"
            "<tr><td>Ctrl+Q</td><td>Quit</td></tr>"
            "</table><h3>Edit</h3><table>"
            "<tr><td>Ctrl+Z</td><td>Undo</td></tr>"
            "<tr><td>Ctrl+Shift+Z / Ctrl+Y</td><td>Redo</td></tr>"
            "<tr><td>Ctrl+C</td><td>Copy selected element</td></tr>"
            "<tr><td>Ctrl+X</td><td>Cut selected element</td></tr>"
            "<tr><td>Ctrl+V</td><td>Paste (offset +10 px)</td></tr>"
            "<tr><td>Ctrl+Shift+V</td><td>Paste in place</td></tr>"
            "<tr><td>Ctrl+D</td><td>Duplicate selected element</td></tr>"
            "<tr><td>Delete / Backspace</td><td>Delete selected element</td></tr>"
            "</table><h3>View</h3><table>"
            "<tr><td>Ctrl+=</td><td>Zoom in</td></tr>"
            "<tr><td>Ctrl+−</td><td>Zoom out</td></tr>"
            "<tr><td>Ctrl+0</td><td>Fit canvas to window</td></tr>"
            "<tr><td>;</td><td>Toggle snapping</td></tr>"
            "</table><h3>Canvas</h3><table>"
            "<tr><td>Click</td><td>Select element</td></tr>"
            "<tr><td>Drag element</td><td>Move element</td></tr>"
            "<tr><td>Drag handle</td><td>Resize element</td></tr>"
            "<tr><td>Shift + drag corner</td><td>Resize proportionally</td></tr>"
            "<tr><td>Ctrl + drag corner</td><td>Resize from center</td></tr>"
            "<tr><td>← ↑ → ↓</td><td>Nudge 1 px</td></tr>"
            "<tr><td>Shift + ← ↑ → ↓</td><td>Nudge 10 px</td></tr>"
            "<tr><td>Middle mouse drag</td><td>Pan canvas</td></tr>"
            "<tr><td>Scroll wheel</td><td>Zoom canvas</td></tr>"
            "</table><h3>Insert</h3><table>"
            "<tr><td>Insert → Rectangle / Text / Image / QR Code</td><td>Add element</td></tr>"
            "</table><h3>Misc</h3><table>"
            "<tr><td>F1</td><td>Show this shortcuts reference</td></tr>"
            "</table>"
        ).arg(cHead, cBorder, cText, cKey));
        auto* layout = new QVBoxLayout(dlg);
        layout->addWidget(browser);
        auto* btns = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
        connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::accept);
        layout->addWidget(btns);
        dlg->exec();
    });
    addAction(m_shortcutsAction);

    m_aboutAction = new QAction(themedIcon(Icons16::Misc_Info), "About…", this);
    connect(m_aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About StreamCanvas Title Editor",
            "<b>StreamCanvas Title Editor</b> &mdash; version " APP_VERSION "<br><br>"
            "A standalone desktop editor for authoring animated broadcast graphics titles.<br>"
            "Titles are saved as .ogt archives and consumed by the <i>StreamCanvas</i> OBS Studio plugin.<br><br>"
            "Built with Qt 6 and Cairo.");
    });

    // ── Home tab ───────────────────────────────────────────────────────────────
    auto* homeCat = m_ribbon->addCategoryPage("Home");

    {   // File panel
        auto* panel = homeCat->addPanel("File");
        panel->addLargeWidget(makeLargeBtn(m_newAction));
        panel->addLargeWidget(makeSmallStack({m_openAction, m_saveAction}));
    }

    {   // Edit panel (Undo / Redo)
        auto* panel = homeCat->addPanel("Edit");
        panel->addLargeWidget(makeSmallStack({m_undoAction, m_redoAction}));
    }

    {   // Clipboard panel
        auto* panel = homeCat->addPanel("Clipboard");

        m_pasteAction = new QAction(themedIcon(Icons16::Action_Paste), "Paste", this);
        m_pasteAction->setEnabled(false);
        m_pasteAction->setShortcut(QKeySequence::Paste);
        connect(m_pasteAction, &QAction::triggered, this, [this]() { doPaste(false); });
        addAction(m_pasteAction);

        auto* pasteMenu = new QMenu(this);
        m_pasteInPlaceAction = new QAction("Paste in Place", this);
        m_pasteInPlaceAction->setEnabled(false);
        m_pasteInPlaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
        connect(m_pasteInPlaceAction, &QAction::triggered, this, [this]() { doPaste(true); });
        addAction(m_pasteInPlaceAction);
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
        panel->addLargeWidget(padded(pasteBtn));

        m_cutAction = new QAction(themedIcon(Icons16::Action_Cut), "Cut", this);
        m_cutAction->setEnabled(false);
        m_cutAction->setShortcut(QKeySequence::Cut);
        connect(m_cutAction, &QAction::triggered, this, &MainWindow::onCut);
        addAction(m_cutAction);

        m_copyAction = new QAction(themedIcon(Icons16::Action_Copy), "Copy", this);
        m_copyAction->setEnabled(false);
        m_copyAction->setShortcut(QKeySequence::Copy);
        connect(m_copyAction, &QAction::triggered, this, &MainWindow::onCopy);
        addAction(m_copyAction);

        panel->addLargeWidget(makeSmallStack({m_cutAction, m_copyAction}));

        m_duplicateAction = new QAction(themedIcon(Icons16::Action_Copy), "Duplicate", this);
        m_duplicateAction->setEnabled(false);
        m_duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
        connect(m_duplicateAction, &QAction::triggered, this, &MainWindow::onDuplicate);
        addAction(m_duplicateAction);

        m_deleteAction = new QAction(themedIcon(Icons16::Action_Trash), "Delete", this);
        m_deleteAction->setEnabled(false);
        m_deleteAction->setShortcuts({QKeySequence(Qt::Key_Delete), QKeySequence(Qt::Key_Backspace)});
        connect(m_deleteAction, &QAction::triggered, this, &MainWindow::deleteSelectedElement);
        addAction(m_deleteAction);

        panel->addLargeWidget(makeSmallStack({m_duplicateAction, m_deleteAction}));

        m_selectAllAction = new QAction(themedIcon(Icons16::Action_SelectAll), "Select All", this);
        m_selectAllAction->setShortcut(QKeySequence::SelectAll);
        m_selectAllAction->setEnabled(false);
        connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::selectAllElements);
        addAction(m_selectAllAction);
        panel->addLargeWidget(makeSmallStack({m_selectAllAction}));
    }

    {   // Arrange panel — align / distribute a multi-selection
        auto* panel = homeCat->addPanel("Arrange");

        using AM = alignmath::AlignMode;
        auto mkAlign = [this](const auto& icon, const QString& label, AM mode) {
            auto* a = new QAction(themedIcon(icon), label, this);
            a->setEnabled(false);
            connect(a, &QAction::triggered, this, [this, mode]() { alignSelected(mode); });
            addAction(a);
            return a;
        };

        m_alignLeftAction    = mkAlign(Icons16::Action_AlignLeft,            "Align Left",     AM::Left);
        m_alignHCenterAction = mkAlign(Icons16::Action_AlignCenterHorizontal,"Align Center",   AM::HCenter);
        m_alignRightAction   = mkAlign(Icons16::Action_AlignRight,           "Align Right",    AM::Right);
        m_alignTopAction     = mkAlign(Icons16::Action_AlignTop,             "Align Top",      AM::Top);
        m_alignVMiddleAction = mkAlign(Icons16::Action_AlignCenterVertical,  "Align Middle",   AM::VMiddle);
        m_alignBottomAction  = mkAlign(Icons16::Action_AlignBottom,          "Align Bottom",   AM::Bottom);

        m_distributeHAction = new QAction(themedIcon(Icons16::Action_DistributeHorizontal), "Distribute Horizontally", this);
        m_distributeHAction->setEnabled(false);
        connect(m_distributeHAction, &QAction::triggered, this, [this]() { distributeSelected(true); });
        addAction(m_distributeHAction);

        m_distributeVAction = new QAction(themedIcon(Icons16::Action_DistributeVertical), "Distribute Vertically", this);
        m_distributeVAction->setEnabled(false);
        connect(m_distributeVAction, &QAction::triggered, this, [this]() { distributeSelected(false); });
        addAction(m_distributeVAction);

        panel->addLargeWidget(makeSmallStack({m_alignLeftAction, m_alignHCenterAction, m_alignRightAction}));
        panel->addLargeWidget(makeSmallStack({m_alignTopAction, m_alignVMiddleAction, m_alignBottomAction}));
        panel->addLargeWidget(makeSmallStack({m_distributeHAction, m_distributeVAction}));

        using ZOp = zorderops::ReorderOp;
        auto mkReorder = [this](const auto& icon, const QString& label, ZOp op,
                                const QKeySequence& shortcut = {}) {
            auto* a = new QAction(themedIcon(icon), label, this);
            a->setEnabled(false);
            if (!shortcut.isEmpty()) a->setShortcut(shortcut);
            connect(a, &QAction::triggered, this, [this, op]() { reorderActive(op); });
            addAction(a);
            return a;
        };

        m_bringToFrontAction = mkReorder(Icons16::Action_BringToFront, "Bring to Front", ZOp::ToFront,
                                         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight));
        m_bringForwardAction = mkReorder(Icons16::Navigation_StepFront, "Bring Forward", ZOp::Forward,
                                         QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
        m_sendBackwardAction = mkReorder(Icons16::Navigation_StepBack, "Send Backward", ZOp::Backward,
                                         QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
        m_sendToBackAction   = mkReorder(Icons16::Action_BringToBack, "Send to Back", ZOp::ToBack,
                                         QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft));

        panel->addLargeWidget(makeSmallStack({m_bringToFrontAction, m_bringForwardAction}));
        panel->addLargeWidget(makeSmallStack({m_sendBackwardAction, m_sendToBackAction}));
    }

    // ── Insert tab ─────────────────────────────────────────────────────────────
    auto* insertCat = m_ribbon->addCategoryPage("Insert");

    {   // Elements panel — always available, no graphic prerequisite
        auto* panel = insertCat->addPanel("Elements");

        // Helper: scan title.elements[1..] for max N with given prefix
        auto nextId = [this](const std::string& prefix) -> std::string {
            const Title& t = m_doc->title();
            int maxN = 0;
            for (int i = 1; i < (int)t.elements.size(); ++i) {
                const std::string& id = t.elements[i]->GetId();
                if (id.rfind(prefix, 0) == 0)
                    try { maxN = std::max(maxN, std::stoi(id.substr(prefix.size()))); } catch (...) {}
            }
            return prefix + std::to_string(maxN + 1);
        };

        // After adding, select the newly created element
        auto selectNewElem = [this](const std::string& newId) {
            const Title& t = m_doc->title();
            for (int i = 1; i < (int)t.elements.size(); ++i)
                if (t.elements[i]->GetId() == newId) {
                    m_editorTitle->setSelection({SelectionId::Level::Element, i});
                    return;
                }
        };

        m_addRectAction = new QAction(themedIcon(Icons16::Shape_Square), "Rectangle", this);
        connect(m_addRectAction, &QAction::triggered, this, [this, nextId, selectNewElem]() {
            const std::string newId = nextId("element_");
            const int zOrder = std::max(0, (int)m_doc->title().elements.size() - 1);
            using json = nlohmann::json;
            json j = {{"id", newId}, {"type", "rectangle"}, {"x", 100}, {"y", 100},
                      {"w", 200}, {"h", 80}, {"fill", {0.5, 0.5, 0.9, 1.0}}, {"z_order", zOrder}};
            m_doc->undoStack()->push(new AddElementCmd(m_doc, std::move(j)));
            selectNewElem(newId);
        });
        panel->addLargeWidget(makeLargeBtn(m_addRectAction));

        m_addTextAction = new QAction(themedIcon(Icons16::File_Font), "Text", this);
        connect(m_addTextAction, &QAction::triggered, this, [this, nextId, selectNewElem]() {
            const std::string newId = nextId("text_");
            const int zOrder = std::max(0, (int)m_doc->title().elements.size() - 1);
            using json = nlohmann::json;
            json j = {{"id", newId}, {"type", "text"}, {"x", 100}, {"y", 100},
                      {"w", 300}, {"h", 60}, {"text", "New Text"}, {"z_order", zOrder},
                      {"fill", {1.0, 1.0, 1.0, 1.0}}};
            m_doc->undoStack()->push(new AddElementCmd(m_doc, std::move(j)));
            selectNewElem(newId);
        });
        panel->addLargeWidget(makeLargeBtn(m_addTextAction));

        m_addImageAction = new QAction(themedIcon(Icons16::File_Picture), "Image", this);
        connect(m_addImageAction, &QAction::triggered, this, [this, nextId, selectNewElem]() {
            const std::string newId = nextId("image_");
            const int zOrder = std::max(0, (int)m_doc->title().elements.size() - 1);
            using json = nlohmann::json;
            json j = {{"id", newId}, {"type", "image"}, {"x", 100}, {"y", 100},
                      {"w", 200}, {"h", 200}, {"z_order", zOrder}, {"scale_mode", "contain"}};
            m_doc->undoStack()->push(new AddElementCmd(m_doc, std::move(j)));
            selectNewElem(newId);
        });
        panel->addLargeWidget(makeLargeBtn(m_addImageAction));

        m_addQrCodeAction = new QAction(qrCodeIcon(), "QR Code", this);
        connect(m_addQrCodeAction, &QAction::triggered, this, [this, nextId, selectNewElem]() {
            const std::string newId = nextId("qr_");
            const int zOrder = std::max(0, (int)m_doc->title().elements.size() - 1);
            using json = nlohmann::json;
            json j = {{"id", newId}, {"type", "qr_code"}, {"x", 100}, {"y", 100},
                      {"w", 200}, {"h", 200}, {"z_order", zOrder}, {"fill", {0.0, 0.0, 0.0, 1.0}}};
            m_doc->undoStack()->push(new AddElementCmd(m_doc, std::move(j)));
            selectNewElem(newId);
        });
        panel->addLargeWidget(makeLargeBtn(m_addQrCodeAction));
    }

    // ── Title tab — always visible, holds title name + canvas dimensions ────────
    {
        auto* titleCat = m_ribbon->addCategoryPage("Title");

        auto titleLabel = [](const QString& text) -> QLabel* {
            auto* lbl = new QLabel(text);
            lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            lbl->setStyleSheet("font-size: 12px;");
            return lbl;
        };

        auto makeTitlePanel = [&](const QString& name) -> QGridLayout* {
            auto* panel = titleCat->addPanel(name);
            auto* container = new QWidget;
            auto* gl = new QGridLayout(container);
            gl->setContentsMargins(0, 0, 0, 0);
            gl->setSpacing(4);
            panel->addLargeWidget(padded(container));
            return gl;
        };

        {   // Info panel: title name
            auto* gl = makeTitlePanel("Info");
            m_titleNameEdit = new QLineEdit;
            m_titleNameEdit->setFixedWidth(160);
            gl->addWidget(titleLabel("Name"), 0, 0, 2, 1);
            gl->addWidget(m_titleNameEdit, 0, 1, 2, 1, Qt::AlignVCenter);
        }

        {   // Canvas panel: preset + W × H
            auto* gl = makeTitlePanel("Canvas");
            m_titlePresetCombo = new QComboBox;
            m_titlePresetCombo->setFixedWidth(180);
            for (const auto& p : kTitlePresets)
                m_titlePresetCombo->addItem(p.label);

            m_titleWidthSpin = new QSpinBox;
            m_titleWidthSpin->setRange(1, 7680);
            m_titleWidthSpin->setSuffix(" px");
            m_titleWidthSpin->setMinimumWidth(80);

            m_titleHeightSpin = new QSpinBox;
            m_titleHeightSpin->setRange(1, 7680);
            m_titleHeightSpin->setSuffix(" px");
            m_titleHeightSpin->setMinimumWidth(80);

            gl->addWidget(titleLabel("Preset"), 0, 0);
            gl->addWidget(m_titlePresetCombo, 0, 1, 1, 3);
            gl->addWidget(titleLabel("W"), 1, 0);
            gl->addWidget(m_titleWidthSpin, 1, 1);
            gl->addWidget(new QLabel("×"), 1, 2, Qt::AlignCenter);
            gl->addWidget(m_titleHeightSpin, 1, 3);
        }

        // Sync title ribbon from document
        auto updateTitleRibbon = [this]() {
            if (m_titleUpdating) return;
            m_titleUpdating = true;
            const Title& t = m_doc->title();
            m_titleNameEdit->setText(m_doc->titleName());
            m_titleWidthSpin->setValue(t.width);
            m_titleHeightSpin->setValue(t.height);
            int idx = kTitleCustomIdx;
            for (int i = 0; i < kTitleCustomIdx; ++i)
                if (kTitlePresets[i].w == t.width && kTitlePresets[i].h == t.height)
                    { idx = i; break; }
            m_titlePresetCombo->setCurrentIndex(idx);
            m_titleUpdating = false;
        };
        connect(m_doc, &TitleDocument::documentChanged, this, updateTitleRibbon);

        connect(m_titleNameEdit, &QLineEdit::editingFinished, this, [this]() {
            if (m_titleUpdating) return;
            QString name = m_titleNameEdit->text().trimmed();
            if (name == m_doc->titleName()) return;
            m_doc->undoStack()->push(new SetTitleNameCmd(m_doc, name));
        });

        connect(m_titlePresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            if (m_titleUpdating || index < 0 || index >= kTitleCustomIdx) return;
            int w = kTitlePresets[index].w, h = kTitlePresets[index].h;
            if (w == m_doc->title().width && h == m_doc->title().height) return;
            m_titleUpdating = true;
            m_titleWidthSpin->setValue(w);
            m_titleHeightSpin->setValue(h);
            m_titleUpdating = false;
            m_doc->undoStack()->push(new SetTitleDimensionsCmd(m_doc, w, h));
        });

        connect(m_titleWidthSpin, &QSpinBox::editingFinished, this, [this]() {
            if (m_titleUpdating) return;
            int w = m_titleWidthSpin->value(), h = m_titleHeightSpin->value();
            if (w == m_doc->title().width) return;
            m_doc->undoStack()->push(new SetTitleDimensionsCmd(m_doc, w, h));
        });

        connect(m_titleHeightSpin, &QSpinBox::editingFinished, this, [this]() {
            if (m_titleUpdating) return;
            int w = m_titleWidthSpin->value(), h = m_titleHeightSpin->value();
            if (h == m_doc->title().height) return;
            m_doc->undoStack()->push(new SetTitleDimensionsCmd(m_doc, w, h));
        });

        updateTitleRibbon();
    }

    // ── View tab ──────────────────────────────────────────────────────────────
    auto* viewCat = m_ribbon->addCategoryPage("View");

    {   // Zoom panel
        auto* panel = viewCat->addPanel("Zoom");
        auto* zoomInAct = new QAction(themedIcon(Icons16::Action_ZoomIn), "Zoom In", this);
        zoomInAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal));
        connect(zoomInAct, &QAction::triggered, m_canvas, &CanvasWidget::zoomIn);
        addAction(zoomInAct);

        auto* zoomOutAct = new QAction(themedIcon(Icons16::Action_ZoomOut), "Zoom Out", this);
        zoomOutAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
        connect(zoomOutAct, &QAction::triggered, m_canvas, &CanvasWidget::zoomOut);
        addAction(zoomOutAct);

        auto* fitAct = new QAction(themedIcon(Icons16::Action_ZoomFit), "Fit", this);
        fitAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
        connect(fitAct, &QAction::triggered, m_canvas, &CanvasWidget::fitToWindow);
        addAction(fitAct);

        panel->addLargeWidget(makeLargeBtn(zoomInAct));
        panel->addLargeWidget(makeSmallStack({zoomOutAct, fitAct}));
    }

    {   // Snapping panel
        auto* panel = viewCat->addPanel("Snapping");
        auto* snapAct = new QAction(themedIcon(Icons16::Misc_Grid), "Snapping", this);
        snapAct->setCheckable(true);
        snapAct->setChecked(true);
        snapAct->setShortcut(Qt::Key_Semicolon);
        connect(snapAct, &QAction::toggled, m_canvas, &CanvasWidget::setSnapping);
        addAction(snapAct);
        panel->addLargeWidget(makeLargeBtn(snapAct));
    }

    {   // Guides panel
        auto* panel = viewCat->addPanel("Guides");

        auto addGuideToggle = [&](const QString& label, CanvasWidget::GuideFlag flag, bool on) {
            auto* btn = new QCheckBox;
            btn->setText(label);
            btn->setCheckable(true);
            btn->setChecked(on);
            connect(btn, &QCheckBox::toggled, this, [this, flag](bool checked) {
                CanvasWidget::GuideFlags f = m_canvas->guides();
                if (checked) f |= flag; else f &= ~flag;
                m_canvas->setGuides(f);
            });
            return btn;
        };

        auto* col = new QWidget;
        auto* cl  = new QVBoxLayout(col);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(2);
        cl->addWidget(addGuideToggle("Rule of Thirds",    CanvasWidget::GuideRuleOfThirds, true));
        cl->addWidget(addGuideToggle("Center Lines",      CanvasWidget::GuideCenterLines,  true));
        cl->addWidget(addGuideToggle("Title Safe (90%)",  CanvasWidget::GuideTitleSafe,    false));
        cl->addWidget(addGuideToggle("Action Safe (80%)", CanvasWidget::GuideActionSafe,   false));
        panel->addLargeWidget(padded(col));
    }

    // Contextual Element + Style + Text + Image + QR Code tabs
    m_formatSection = new RibbonFormatSection(m_doc, m_ribbon, this);

    // ── Help tab ──────────────────────────────────────────────────────────────
    {
        auto* helpCat = m_ribbon->addCategoryPage("Help");
        auto* panel   = helpCat->addPanel("Help");
        panel->addLargeWidget(makeLargeBtn(m_shortcutsAction));
        panel->addLargeWidget(makeLargeBtn(m_aboutAction));
    }

    m_ribbon->hideCategory(m_formatSection->elementCategory());
    m_ribbon->hideCategory(m_formatSection->styleCategory());
    m_ribbon->hideCategory(m_formatSection->textCategory());
    m_ribbon->hideCategory(m_formatSection->imageCategory());
    m_ribbon->hideCategory(m_formatSection->qrCategory());

    connect(m_formatSection, &RibbonFormatSection::deleteElementRequested, this, &MainWindow::deleteSelectedElement);

    connect(m_formatSection, &RibbonFormatSection::copyStyleModeToggled, this,
            [this](bool on, const std::string& srcEi) { m_canvas->setPaintMode(on, srcEi); });
    connect(m_canvas, &CanvasWidget::paintModeFinished, this, [this]() {
        m_formatSection->setPaintModeActive(false);
    });

    connect(m_canvas, &CanvasWidget::interactiveEditStarted, this,
            [this]() { m_treeView->setResetsSuppressed(true); });
    connect(m_canvas, &CanvasWidget::interactiveEditFinished, this,
            [this]() { m_treeView->setResetsSuppressed(false); });
}

// ── Menu bar ──────────────────────────────────────────────────────────────────

void MainWindow::setupMenuBar()
{
    auto* appMenu = new QMenu(this);
    appMenu->addAction(m_newAction);
    appMenu->addAction(m_openAction);
    appMenu->addSeparator();
    appMenu->addAction(m_saveAction);
    appMenu->addAction(m_saveAsAction);
    appMenu->addSeparator();
    appMenu->addAction(m_shortcutsAction);
    appMenu->addAction(m_aboutAction);
    appMenu->addSeparator();
    appMenu->addAction(m_exitAction);

    if (auto* appBtn = qobject_cast<QToolButton*>(m_ribbon->applicationButton())) {
        appBtn->setText("File");
        appBtn->setPopupMode(QToolButton::InstantPopup);
        appBtn->setMenu(appMenu);
    }
}

// ── File operations ───────────────────────────────────────────────────────────

void MainWindow::onNew()
{
    if (!maybeSave()) return;
    m_doc->reset();
    m_editorTitle->setSelection(SelectionId{});
}

void MainWindow::onOpen()
{
    if (!maybeSave()) return;
    QString path = QFileDialog::getOpenFileName(this, "Open Title", QString(),
                                                "StreamCanvas Title (*.ogt);;All Files (*)");
    if (path.isEmpty()) return;
    bool ok;
    { WaitCursor wc; ok = m_doc->load(path); }
    if (!ok) {
        const QString detail = m_doc->lastError();
        QMessageBox::warning(this, "Open Failed",
            detail.isEmpty() ? QString("Could not open file:\n%1").arg(path)
                             : QString("Could not open file:\n%1\n\n%2").arg(path, detail));
    } else {
        m_editorTitle->setSelection(SelectionId{});

        const Title::LoadDiagnostic& diag = m_doc->lastLoadDiagnostic();
        if (diag.severity != Title::LoadDiagnostic::Severity::None) {
            const bool isWarning = diag.severity == Title::LoadDiagnostic::Severity::Warning;
            auto* msgBox = new QMessageBox(
                isWarning ? QMessageBox::Warning : QMessageBox::Information,
                "Schema version", QString::fromStdString(diag.message), QMessageBox::Ok, this);
            msgBox->setAttribute(Qt::WA_DeleteOnClose);
            msgBox->show();
        }
    }
}

void MainWindow::onSave()
{
    if (m_doc->filePath().isEmpty()) { onSaveAs(); return; }
    bool ok;
    { WaitCursor wc; ok = m_doc->save(); }
    if (!ok) {
        const QString detail = m_doc->lastError();
        QMessageBox::warning(this, "Save Failed",
            detail.isEmpty() ? QString("Could not save the file.")
                             : QString("Could not save the file.\n\n%1").arg(detail));
    }
}

void MainWindow::onSaveAs()
{
    QString path = QFileDialog::getSaveFileName(this, "Save Title As", QString(),
                                                "StreamCanvas Title (*.ogt);;All Files (*)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".ogt", Qt::CaseInsensitive))
        path += ".ogt";
    bool ok;
    { WaitCursor wc; ok = m_doc->saveAs(path); }
    if (!ok) {
        const QString detail = m_doc->lastError();
        QMessageBox::warning(this, "Save Failed",
            detail.isEmpty() ? QString("Could not save file:\n%1").arg(path)
                             : QString("Could not save file:\n%1\n\n%2").arg(path, detail));
    } else {
        updateWindowTitle();
    }
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
        QString("The title \"%1\" has unsaved changes.\nSave before continuing?")
            .arg(m_doc->titleName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (result == QMessageBox::Save) { onSave(); return !m_doc->isModified(); }
    return result == QMessageBox::Discard;
}

void MainWindow::showEvent(QShowEvent* event)
{
    SARibbonMainWindow::showEvent(event);
    QTimer::singleShot(0, this, [this]() {
        for (auto* splitter : findChildren<QSplitter*>()) {
            for (int i = 1; i < splitter->count(); ++i) {
                auto* h = splitter->handle(i);
                h->setCursor(splitter->orientation() == Qt::Horizontal
                    ? Qt::SplitHCursor : Qt::SplitVCursor);
            }
        }
    });
}

void MainWindow::updateWindowTitle()
{
    setWindowTitle(QString("%1[*] — StreamCanvas Title Editor").arg(m_doc->titleName()));
}

// ── Selection ─────────────────────────────────────────────────────────────────

void MainWindow::updateToolBarState(SelectionId id)
{
    const bool hasElement = (m_editorTitle->selectionCount() > 0);

    if (m_cutAction)  m_cutAction->setEnabled(hasElement);
    if (m_copyAction) m_copyAction->setEnabled(hasElement);
    if (m_duplicateAction) m_duplicateAction->setEnabled(hasElement);
    if (m_deleteAction)    m_deleteAction->setEnabled(hasElement);
    if (m_selectAllAction)
        m_selectAllAction->setEnabled((int)m_doc->title().elements.size() > 1);

    bool canPaste = m_clipboard.has_value();
    if (!canPaste)
        canPaste = clipboardHasExternalImage();
    if (m_pasteAction)        m_pasteAction->setEnabled(canPaste);
    if (m_pasteInPlaceAction) m_pasteInPlaceAction->setEnabled(canPaste);

    const int selCount = m_editorTitle->selectionCount();
    const bool canAlign = selCount >= 2;
    const bool canDistribute = selCount >= 3;
    if (m_alignLeftAction)    m_alignLeftAction->setEnabled(canAlign);
    if (m_alignHCenterAction) m_alignHCenterAction->setEnabled(canAlign);
    if (m_alignRightAction)   m_alignRightAction->setEnabled(canAlign);
    if (m_alignTopAction)     m_alignTopAction->setEnabled(canAlign);
    if (m_alignVMiddleAction) m_alignVMiddleAction->setEnabled(canAlign);
    if (m_alignBottomAction)  m_alignBottomAction->setEnabled(canAlign);
    if (m_distributeHAction)  m_distributeHAction->setEnabled(canDistribute);
    if (m_distributeVAction)  m_distributeVAction->setEnabled(canDistribute);

    // Reorder actions: single active element with >=2 siblings (same sibling
    // definition as TitleTreeModel::childrenOf).
    bool canReorder = false;
    if (id.level == SelectionId::Level::Element) {
        const Title& t = m_doc->title();
        if (id.elementIndex >= 1 && id.elementIndex < (int)t.elements.size()) {
            const IElement* activeEl = t.elements[id.elementIndex].get();
            if (const IElement* parentEl = activeEl->GetParent()) {
                int siblingCount = 0;
                for (const IElement* child : parentEl->GetChildren())
                    if (dynamic_cast<const VisualElement*>(child))
                        ++siblingCount;
                canReorder = siblingCount >= 2;
            }
        }
    }
    if (m_bringToFrontAction) m_bringToFrontAction->setEnabled(canReorder);
    if (m_bringForwardAction) m_bringForwardAction->setEnabled(canReorder);
    if (m_sendBackwardAction) m_sendBackwardAction->setEnabled(canReorder);
    if (m_sendToBackAction)   m_sendToBackAction->setEnabled(canReorder);
}

void MainWindow::onSelectionChanged(SelectionId id)
{
    updateToolBarState(id);

    if (id.level == SelectionId::Level::Element) {
        const Title& t = m_doc->title();
        if (id.elementIndex < 1 || id.elementIndex >= (int)t.elements.size()) return;
        const auto* el = dynamic_cast<const VisualElement*>(t.elements[id.elementIndex].get());
        if (!el) return;

        const bool isText  = dynamic_cast<const TextElement*>(el)  != nullptr;
        const bool isImage = dynamic_cast<const ImageElement*>(el) != nullptr;
        const bool isQr    = dynamic_cast<const QrElement*>(el)    != nullptr;

        m_formatSection->setSelection(el->GetId());

        m_ribbon->showCategory(m_formatSection->elementCategory());
        m_ribbon->showCategory(m_formatSection->styleCategory());
        isText  ? m_ribbon->showCategory(m_formatSection->textCategory())
                : m_ribbon->hideCategory(m_formatSection->textCategory());
        isImage ? m_ribbon->showCategory(m_formatSection->imageCategory())
                : m_ribbon->hideCategory(m_formatSection->imageCategory());
        isQr    ? m_ribbon->showCategory(m_formatSection->qrCategory())
                : m_ribbon->hideCategory(m_formatSection->qrCategory());
        m_ribbon->setCurrentIndex(m_ribbon->categoryIndex(m_formatSection->elementCategory()));

    } else {
        m_formatSection->clearSelection();
        m_ribbon->hideCategory(m_formatSection->elementCategory());
        m_ribbon->hideCategory(m_formatSection->styleCategory());
        m_ribbon->hideCategory(m_formatSection->textCategory());
        m_ribbon->hideCategory(m_formatSection->imageCategory());
        m_ribbon->hideCategory(m_formatSection->qrCategory());
    }
}

void MainWindow::selectAllElements()
{
    const Title& t = m_doc->title();
    std::vector<int> indices;
    for (int i = 1; i < (int)t.elements.size(); ++i)
        if (dynamic_cast<const VisualElement*>(t.elements[i].get()))
            indices.push_back(i);
    if (indices.empty()) return;
    m_editorTitle->setMultiSelection(indices, indices.back());
}

void MainWindow::updateSelectionStatus()
{
    const int n = m_editorTitle->selectionCount();
    if (n > 1)
        statusBar()->showMessage(QString("%1 elements selected").arg(n));
    else
        statusBar()->clearMessage();
}

// ── Clipboard ─────────────────────────────────────────────────────────────────

void MainWindow::onCopy()
{
    const SelectionId sel = m_editorTitle->selection();
    const Title& t = m_doc->title();

    if (sel.level == SelectionId::Level::Element &&
        sel.elementIndex >= 1 && sel.elementIndex < (int)t.elements.size()) {
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[sel.elementIndex].get());
        if (ve) {
            nlohmann::json ej = TitleDocument::elementToJson(*ve);
            ej["_clipboard_type"] = "element";
            m_clipboard = std::move(ej);
        }
    }
    updateToolBarState(m_editorTitle->selection());
}

void MainWindow::onCut()
{
    const SelectionId sel = m_editorTitle->selection();
    onCopy();
    if (!m_clipboard.has_value()) return;

    if (sel.level == SelectionId::Level::Element) {
        const Title& t = m_doc->title();
        if (sel.elementIndex < 1 || sel.elementIndex >= (int)t.elements.size()) return;
        const std::string ei = t.elements[sel.elementIndex]->GetId();
        m_doc->undoStack()->push(new RemoveElementCmd(m_doc, ei));
    }
}

void MainWindow::deleteSelectedElement()
{
    const std::vector<int> indices = m_editorTitle->selectedIndices();
    if (indices.empty()) return;
    const Title& t = m_doc->title();
    // Resolve to string ids FIRST — removals shift positional indices.
    std::vector<std::string> ids;
    for (int i : indices)
        if (i >= 1 && i < (int)t.elements.size())
            ids.push_back(t.elements[i]->GetId());
    if (ids.empty()) return;
    if (ids.size() == 1) {
        m_doc->undoStack()->push(new RemoveElementCmd(m_doc, ids[0]));
    } else {
        m_doc->undoStack()->beginMacro("Delete elements");
        for (const auto& id : ids)
            m_doc->undoStack()->push(new RemoveElementCmd(m_doc, id));
        m_doc->undoStack()->endMacro();
    }
}

std::string MainWindow::insertElementCopyImpl(nlohmann::json cb, double offset)
{
    const Title& t = m_doc->title();
    std::string origType = cb.value("type", "rectangle");
    std::string prefix = "element_";
    if (origType == "text")       prefix = "text_";
    else if (origType == "image") prefix = "image_";
    else if (origType == "qr_code") prefix = "qr_";

    int maxN = 0;
    for (int i = 1; i < (int)t.elements.size(); ++i) {
        const std::string& eid = t.elements[i]->GetId();
        if (eid.rfind(prefix, 0) == 0)
            try { maxN = std::max(maxN, std::stoi(eid.substr(prefix.size()))); } catch (...) {}
    }
    std::string newId = prefix + std::to_string(maxN + 1);

    cb["id"] = newId;
    cb["z_order"] = std::max(0, (int)t.elements.size() - 1);
    if (offset != 0.0) {
        if (cb.contains("x")) cb["x"] = cb["x"].get<double>() + offset;
        if (cb.contains("y")) cb["y"] = cb["y"].get<double>() + offset;
    }

    m_doc->undoStack()->push(new AddElementCmd(m_doc, std::move(cb)));
    return newId;
}

void MainWindow::insertElementCopy(nlohmann::json cb, double offset)
{
    std::string newId = insertElementCopyImpl(std::move(cb), offset);

    const Title& t2 = m_doc->title();
    for (int i = 1; i < (int)t2.elements.size(); ++i)
        if (t2.elements[i]->GetId() == newId) {
            m_editorTitle->setSelection({SelectionId::Level::Element, i});
            break;
        }
    updateToolBarState(m_editorTitle->selection());
}

void MainWindow::onDuplicate()
{
    const std::vector<int> indices = m_editorTitle->selectedIndices();
    if (indices.empty()) return;
    const Title& t = m_doc->title();
    std::vector<nlohmann::json> sources;
    for (int i : indices) {
        if (i < 1 || i >= (int)t.elements.size()) continue;
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[i].get());
        if (!ve) continue;
        nlohmann::json cb = TitleDocument::elementToJson(*ve);
        cb.erase("mask");
        cb.erase("parent");
        sources.push_back(std::move(cb));
    }
    if (sources.empty()) return;
    if (sources.size() == 1) { insertElementCopy(std::move(sources[0]), 10.0); return; }

    m_doc->undoStack()->beginMacro("Duplicate elements");
    std::vector<std::string> newIds;
    for (auto& src : sources)
        newIds.push_back(insertElementCopyImpl(std::move(src), 10.0));
    m_doc->undoStack()->endMacro();

    const Title& t2 = m_doc->title();
    std::vector<int> newIndices;
    for (const auto& id : newIds)
        for (int i = 1; i < (int)t2.elements.size(); ++i)
            if (t2.elements[i]->GetId() == id) { newIndices.push_back(i); break; }
    if (!newIndices.empty())
        m_editorTitle->setMultiSelection(newIndices, newIndices.back());
    updateToolBarState(m_editorTitle->selection());
}

void MainWindow::doPaste(bool inPlace)
{
    using json = nlohmann::json;

    if (m_clipboard.has_value()) {
        json cb = *m_clipboard;
        std::string clipType = cb.value("_clipboard_type", "");
        cb.erase("_clipboard_type");
        cb.erase("mask");
        cb.erase("parent");
        const double offset = inPlace ? 0.0 : 10.0;

        if (clipType == "element") {
            insertElementCopy(std::move(cb), offset);
        }

        updateToolBarState(m_editorTitle->selection());
        return;
    }

    // ── System clipboard image ─────────────────────────────────────────────────
    const QMimeData* md = QGuiApplication::clipboard()->mimeData();
    if (!md) return;

    QString imagePath;
    QImage img;
    if (md->hasUrls()) {
        WaitCursor wc;
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
    if (imagePath.isEmpty()) {
        if (md->hasImage())
            img = qvariant_cast<QImage>(md->imageData());
        if (img.isNull()) {
            for (const QString& fmt : md->formats()) {
                if (!fmt.startsWith("image/")) continue;
                QByteArray raw = md->data(fmt);
                if (!raw.isEmpty() && img.loadFromData(raw)) break;
            }
        }
    }

    if (img.isNull() && imagePath.isEmpty()) return;

    // Raw pixel data must be saved to disk before embedding
    if (imagePath.isEmpty()) {
        QString defaultDir = m_doc->filePath().isEmpty()
            ? QDir::homePath()
            : QFileInfo(m_doc->filePath()).absolutePath();
        imagePath = QFileDialog::getSaveFileName(
            this, "Save Clipboard Image",
            defaultDir + "/clipboard_image.png",
            "PNG Images (*.png);;JPEG Images (*.jpg *.jpeg);;All Files (*)");
        if (imagePath.isEmpty()) return;
        bool saved;
        { WaitCursor wc; saved = img.save(imagePath); }
        if (!saved) {
            QMessageBox::warning(this, "Save Failed",
                "Could not save the clipboard image.");
            return;
        }
    }

    {
        WaitCursor wc;
        if (img.isNull()) img.load(imagePath);
    }

    double w = img.isNull() ? 200.0 : img.width();
    double h = img.isNull() ? 200.0 : img.height();
    const Title& t = m_doc->title();
    if (w > t.width)  { h = h * t.width  / w; w = t.width; }
    if (h > t.height) { w = w * t.height / h; h = t.height; }

    int maxN = 0;
    for (int i = 1; i < (int)t.elements.size(); ++i) {
        const std::string& eid = t.elements[i]->GetId();
        if (eid.rfind("image_", 0) == 0)
            try { maxN = std::max(maxN, std::stoi(eid.substr(6))); } catch (...) {}
    }
    std::string newId = "image_" + std::to_string(maxN + 1);

    json j = {{"id", newId}, {"type", "image"},
              {"x", inPlace ? 0.0 : 100.0},
              {"y", inPlace ? 0.0 : 100.0},
              {"w", w}, {"h", h},
              {"z_order", std::max(0, (int)t.elements.size() - 1)},
              {"image_path", imagePath.toStdString()},
              {"scale_mode", "contain"}};

    m_doc->undoStack()->push(new AddElementCmd(m_doc, std::move(j)));

    const Title& t2 = m_doc->title();
    for (int i = 1; i < (int)t2.elements.size(); ++i)
        if (t2.elements[i]->GetId() == newId) {
            m_editorTitle->setSelection({SelectionId::Level::Element, i});
            break;
        }

    updateToolBarState(m_editorTitle->selection());
}

void MainWindow::alignSelected(alignmath::AlignMode mode)
{
    const std::vector<int> indices = m_editorTitle->selectedIndices();
    if (indices.size() < 2) return;
    const Title& t = m_doc->title();
    std::vector<QRectF> boxes;
    std::vector<std::string> ids;
    std::vector<Rectangle> bounds;
    for (int i : indices) {
        if (i < 1 || i >= (int)t.elements.size()) continue;
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[i].get());
        if (!ve) continue;
        boxes.push_back(m_canvas->elementTitleAABB(*ve));
        ids.push_back(ve->GetId());
        bounds.push_back(ve->GetBounds());
    }
    if (boxes.size() < 2) return;
    const std::vector<QPointF> deltas = alignmath::alignDeltas(boxes, mode);

    // Collect the actual changes first so we never open an empty macro.
    struct Change { std::string id; Rectangle b; };
    std::vector<Change> changes;
    for (std::size_t k = 0; k < ids.size(); ++k) {
        if (deltas[k].isNull()) continue;
        Rectangle b = bounds[k];
        b.x += deltas[k].x();
        b.y += deltas[k].y();
        changes.push_back({ids[k], b});
    }
    if (changes.empty()) return;
    if (changes.size() == 1) {
        m_doc->undoStack()->push(new SetElementBoundsCmd(m_doc, changes[0].id, changes[0].b));
    } else {
        m_doc->undoStack()->beginMacro("Align elements");
        for (const auto& c : changes)
            m_doc->undoStack()->push(new SetElementBoundsCmd(m_doc, c.id, c.b));
        m_doc->undoStack()->endMacro();
    }
}

void MainWindow::distributeSelected(bool horizontal)
{
    const std::vector<int> indices = m_editorTitle->selectedIndices();
    if (indices.size() < 3) return;
    const Title& t = m_doc->title();
    std::vector<QRectF> boxes;
    std::vector<std::string> ids;
    std::vector<Rectangle> bounds;
    for (int i : indices) {
        if (i < 1 || i >= (int)t.elements.size()) continue;
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[i].get());
        if (!ve) continue;
        boxes.push_back(m_canvas->elementTitleAABB(*ve));
        ids.push_back(ve->GetId());
        bounds.push_back(ve->GetBounds());
    }
    if (boxes.size() < 3) return;
    const std::vector<QPointF> deltas = alignmath::distributeDeltas(boxes, horizontal);

    struct Change { std::string id; Rectangle b; };
    std::vector<Change> changes;
    for (std::size_t k = 0; k < ids.size(); ++k) {
        if (deltas[k].isNull()) continue;
        Rectangle b = bounds[k];
        b.x += deltas[k].x();
        b.y += deltas[k].y();
        changes.push_back({ids[k], b});
    }
    if (changes.empty()) return;
    if (changes.size() == 1) {
        m_doc->undoStack()->push(new SetElementBoundsCmd(m_doc, changes[0].id, changes[0].b));
    } else {
        m_doc->undoStack()->beginMacro("Distribute elements");
        for (const auto& c : changes)
            m_doc->undoStack()->push(new SetElementBoundsCmd(m_doc, c.id, c.b));
        m_doc->undoStack()->endMacro();
    }
}

void MainWindow::reorderActive(zorderops::ReorderOp op)
{
    const SelectionId sel = m_editorTitle->selection();
    if (sel.level != SelectionId::Level::Element) return;
    const Title& t = m_doc->title();
    if (sel.elementIndex < 1 || sel.elementIndex >= (int)t.elements.size()) return;
    zorderops::applyReorder(m_doc, t.elements[sel.elementIndex].get(), op);
}
