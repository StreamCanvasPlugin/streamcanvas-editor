#pragma once

#include "model/EditorTitle.h"
#include <SARibbonMainWindow.h>
#include <nlohmann/json.hpp>
#include <optional>

class TitleDocument;
class EditorTitle;
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

private:
    void setupMenuBar();
    void setupRibbon();
    void updateWindowTitle();
    void updateToolBarState(SelectionId sel);
    bool maybeSave();
    void doPaste(bool inPlace);
    void deleteSelectedElement();
    void insertElementCopy(nlohmann::json cb, double offset);

    TitleDocument* m_doc;
    EditorTitle*   m_editorTitle;
    CanvasWidget*  m_canvas;

    // Title ribbon controls
    QLineEdit* m_titleNameEdit{nullptr};
    QComboBox* m_titlePresetCombo{nullptr};
    QSpinBox*  m_titleWidthSpin{nullptr};
    QSpinBox*  m_titleHeightSpin{nullptr};
    bool       m_titleUpdating{false};

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
    QAction* m_duplicateAction{nullptr};
    QAction* m_deleteAction{nullptr};
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
