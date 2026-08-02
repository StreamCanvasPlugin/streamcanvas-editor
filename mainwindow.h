#pragma once

#include "model/EditorTitle.h"
#include "ui/widgets/AlignMath.h"
#include "ui/widgets/ZOrderOps.h"
#include <SARibbonMainWindow.h>
#include <nlohmann/json.hpp>
#include <optional>

class TitleDocument;
class EditorTitle;
class CanvasWidget;
class TitleTreeView;
class AnimationTimingPanel;
class RibbonFormatSection;
class QAction;
class QCloseEvent;
class QLineEdit;
class QComboBox;
class QSpinBox;

class MainWindow : public SARibbonMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onSelectionChanged(SelectionId id);
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onCopy();
    void onCut();
    void onDuplicate();
    void selectAllElements();
    void updateSelectionStatus();
    void alignSelected(alignmath::AlignMode mode);
    void distributeSelected(bool horizontal);
    void reorderActive(zorderops::ReorderOp op);

private:
    void setupMenuBar();
    void setupRibbon();
    void updateWindowTitle();
    void updateToolBarState(SelectionId sel);
    bool maybeSave();
    void doPaste(bool inPlace);
    void deleteSelectedElement();
    void insertElementCopy(nlohmann::json cb, double offset);
    std::string insertElementCopyImpl(nlohmann::json cb, double offset);

    TitleDocument* m_doc;
    EditorTitle*   m_editorTitle;
    CanvasWidget*  m_canvas;
    TitleTreeView* m_treeView{nullptr};

    // Title ribbon controls
    QLineEdit* m_titleNameEdit{nullptr};
    QComboBox* m_titlePresetCombo{nullptr};
    QSpinBox*  m_titleWidthSpin{nullptr};
    QSpinBox*  m_titleHeightSpin{nullptr};
    bool       m_titleUpdating{false};

    RibbonFormatSection* m_formatSection{nullptr};

    AnimationTimingPanel* m_timingPanel{nullptr};

    QAction* m_newAction{nullptr};
    QAction* m_openAction{nullptr};
    QAction* m_saveAction{nullptr};
    QAction* m_saveAsAction{nullptr};
    QAction* m_exitAction{nullptr};
    QAction* m_undoAction{nullptr};
    QAction* m_redoAction{nullptr};
    QAction* m_cutAction{nullptr};
    QAction* m_copyAction{nullptr};
    QAction* m_duplicateAction{nullptr};
    QAction* m_deleteAction{nullptr};
    QAction* m_selectAllAction{nullptr};
    QAction* m_alignLeftAction{nullptr};
    QAction* m_alignHCenterAction{nullptr};
    QAction* m_alignRightAction{nullptr};
    QAction* m_alignTopAction{nullptr};
    QAction* m_alignVMiddleAction{nullptr};
    QAction* m_alignBottomAction{nullptr};
    QAction* m_distributeHAction{nullptr};
    QAction* m_distributeVAction{nullptr};
    QAction* m_bringToFrontAction{nullptr};
    QAction* m_bringForwardAction{nullptr};
    QAction* m_sendBackwardAction{nullptr};
    QAction* m_sendToBackAction{nullptr};
    QAction* m_pasteAction{nullptr};
    QAction* m_pasteInPlaceAction{nullptr};
    QAction* m_shortcutsAction{nullptr};
    QAction* m_aboutAction{nullptr};

    std::optional<nlohmann::json> m_clipboard;

    QAction* m_addRectAction{nullptr};
    QAction* m_addTextAction{nullptr};
    QAction* m_addImageAction{nullptr};
    QAction* m_addQrCodeAction{nullptr};

    SARibbonBar* m_ribbon{nullptr};
};
