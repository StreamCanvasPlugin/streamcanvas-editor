#pragma once

#include <QWidget>

#include "ui/widgets/AnimationTimelineEditor.h"

class TitleDocument;
class EditorTitle;
class ClipPropertiesPanel;
class QComboBox;
class QPushButton;
class QSpinBox;
class QTimer;

// Dock-level container composing AnimationTimelineEditor + ClipPropertiesPanel,
// wired to the document + selection model. Pushes SetElementAnimCmd on edits.
class AnimationTimingPanel : public QWidget {
    Q_OBJECT
public:
    explicit AnimationTimingPanel(TitleDocument* doc, EditorTitle* sel, QWidget* parent = nullptr);

signals:
    void scrubTimeChanged(bool isIn, bool isData, double t);
    void previewStopped();

private slots:
    void onDocumentChanged();
    void onSelectionChanged();
    void onSlotComboChanged(int index);
    void onTimelineRowClicked(int elementIndex);
    void onTimelineActiveClipChanged(bool valid, AnimationDef def);
    void onPanelClipChanged(AnimationDef def);
    void onPlayTick();

private:
    void pushAnimCmd(int elementIndex, AnimationDef def);
    void slotFlags(bool& isIn, bool& isData) const;
    void startPlayback();
    void stopPlayback();

    TitleDocument* m_doc;
    EditorTitle* m_sel;
    AnimationTimelineEditor* m_timeline;
    ClipPropertiesPanel* m_panel;
    QComboBox* m_slotCombo;
    QPushButton* m_playBtn;
    QSpinBox* m_speedSpin;
    QSpinBox* m_zoomSpin;
    QPushButton* m_fitBtn;
    QTimer* m_playTimer;
    double m_scrubTime{0.0};
    bool m_playing{false};
    int m_activeElementIndex{-1};
    bool m_updating{false};
};
