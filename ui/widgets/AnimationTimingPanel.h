#pragma once

#include <QWidget>

#include "ui/widgets/AnimationTimelineEditor.h"

class TitleDocument;
class EditorTitle;
class ClipPropertiesPanel;
class QComboBox;

// Dock-level container composing AnimationTimelineEditor + ClipPropertiesPanel,
// wired to the document + selection model. Pushes SetElementAnimCmd on edits.
class AnimationTimingPanel : public QWidget {
    Q_OBJECT
public:
    explicit AnimationTimingPanel(TitleDocument* doc, EditorTitle* sel, QWidget* parent = nullptr);

private slots:
    void onDocumentChanged();
    void onSelectionChanged();
    void onSlotComboChanged(int index);
    void onTimelineRowClicked(int elementIndex);
    void onTimelineActiveClipChanged(bool valid, AnimationDef def);
    void onPanelClipChanged(AnimationDef def);

private:
    void pushAnimCmd(int elementIndex, AnimationDef def);

    TitleDocument* m_doc;
    EditorTitle* m_sel;
    AnimationTimelineEditor* m_timeline;
    ClipPropertiesPanel* m_panel;
    QComboBox* m_slotCombo;
    int m_activeElementIndex{-1};
    bool m_updating{false};
};
