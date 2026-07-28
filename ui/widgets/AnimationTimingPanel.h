#pragma once

#include <QElapsedTimer>
#include <QWidget>

#include "ui/widgets/AnimationTimelineEditor.h"

class TitleDocument;
class EditorTitle;
class ClipPropertiesPanel;
class QComboBox;
class QSpinBox;
class QTimer;
class QToolButton;

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
    void pausePlayback();
    void stopPlayback();
    void resetToVisible();
    void updateTransportState();
    void emitScrub(double t);
    void emitPreviewStopped();

    TitleDocument* m_doc;
    EditorTitle* m_sel;
    AnimationTimelineEditor* m_timeline;
    ClipPropertiesPanel* m_panel;
    QComboBox* m_slotCombo;
    QToolButton* m_playBtn;
    QToolButton* m_stopBtn;
    QToolButton* m_resetBtn;
    QSpinBox* m_speedSpin;
    QSpinBox* m_zoomSpin;
    QToolButton* m_fitBtn;
    QTimer* m_playTimer;
    QElapsedTimer m_playClock;
    double m_scrubTime{0.0};
    bool m_playing{false};
    bool m_previewActive{false};
    int m_activeElementIndex{-1};
    bool m_updating{false};
};
