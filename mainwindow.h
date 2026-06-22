#pragma once

#include "model/EditorScene.h"
#include <SARibbonMainWindow.h>
#include <nlohmann/json.hpp>
#include <optional>

class SceneDocument;
class EditorScene;
class CanvasWidget;
class GraphicTimingEditor;
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

private slots:
    void onSelectionChanged(SelectionId id);
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onCopy();
    void onCut();

private:
    void setupMenuBar();
    void setupRibbon();
    void updateWindowTitle();
    void updateToolBarState(SelectionId sel);
    bool maybeSave();
    void doPaste(bool inPlace);

    SceneDocument* m_doc;
    EditorScene*   m_editorScene;
    CanvasWidget*  m_canvas;

    // Scene ribbon controls
    QLineEdit* m_sceneNameEdit{nullptr};
    QComboBox* m_scenePresetCombo{nullptr};
    QSpinBox*  m_sceneWidthSpin{nullptr};
    QSpinBox*  m_sceneHeightSpin{nullptr};
    bool       m_sceneUpdating{false};

    RibbonFormatSection* m_formatSection{nullptr};

    GraphicTimingEditor* m_timingEditor{nullptr};

    QAction* m_newAction{nullptr};
    QAction* m_openAction{nullptr};
    QAction* m_saveAction{nullptr};
    QAction* m_saveAsAction{nullptr};
    QAction* m_exitAction{nullptr};
    QAction* m_undoAction{nullptr};
    QAction* m_redoAction{nullptr};
    QAction* m_cutAction{nullptr};
    QAction* m_copyAction{nullptr};
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
