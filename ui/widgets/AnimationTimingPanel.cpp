#include "ui/widgets/AnimationTimingPanel.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

#include "engine/title.h"
#include "engine/visual_element.h"
#include "icons.h"
#include "model/EditorTitle.h"
#include "model/TitleDocument.h"
#include "model/UndoCommands.h"
#include "ui/UiUtils.h"
#include "ui/widgets/ClipPropertiesPanel.h"

namespace {
QFrame* makeSeparator(QWidget* parent)
{
    auto* sep = new QFrame(parent);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setFixedWidth(1);
    return sep;
}
}  // namespace

AnimationTimingPanel::AnimationTimingPanel(TitleDocument* doc, EditorTitle* sel, QWidget* parent)
    : QWidget(parent), m_doc(doc), m_sel(sel)
{
    m_timeline = new AnimationTimelineEditor(doc, this);
    m_panel = new ClipPropertiesPanel(this);

    m_slotCombo = new QComboBox(this);
    m_slotCombo->setObjectName("slotCombo");
    m_slotCombo->addItem(tr("In"));
    m_slotCombo->addItem(tr("Out"));
    m_slotCombo->addItem(tr("Data In"));
    m_slotCombo->addItem(tr("Data Out"));

    m_playBtn = makeIconToolButton(themedIcon(Icons16::Media_Play), tr("Play"));
    m_playBtn->setObjectName("playBtn");
    m_playBtn->setAccessibleName(tr("Play"));
    m_stopBtn = makeIconToolButton(themedIcon(Icons16::Media_Stop),
        tr("Stop — rewind to the start and return the canvas to the editable state"));
    m_stopBtn->setObjectName("stopBtn");
    m_stopBtn->setAccessibleName(tr("Stop"));
    m_resetBtn = makeIconToolButton(themedIcon(Icons16::Action_Reset),
        tr("Reset to Visible — return the canvas to the editable state, keeping the playhead where it is"));
    m_resetBtn->setObjectName("resetBtn");
    m_resetBtn->setAccessibleName(tr("Reset to Visible"));

    m_speedSpin = new QSpinBox(this);
    m_speedSpin->setObjectName("speedSpin");
    m_speedSpin->setRange(10, 400);
    m_speedSpin->setSuffix(tr(" %"));
    m_speedSpin->setValue(100);
    m_speedSpin->setFixedWidth(70);
    m_speedSpin->setToolTip(tr("Playback speed"));

    m_zoomSpin = new QSpinBox(this);
    m_zoomSpin->setObjectName("zoomSpin");
    m_zoomSpin->setRange(2, 1667);
    m_zoomSpin->setSuffix(tr(" %"));
    m_zoomSpin->setValue(100);
    m_zoomSpin->setFixedWidth(80);
    m_zoomSpin->setToolTip(tr("Zoom"));

    m_fitBtn = makeIconToolButton(themedIcon(Icons16::Action_ZoomFit), tr("Zoom to fit all clips"));
    m_fitBtn->setObjectName("fitBtn");
    m_fitBtn->setAccessibleName(tr("Zoom to fit"));

    m_playTimer = new QTimer(this);
    m_playTimer->setInterval(16);

    auto* topStrip = new QHBoxLayout();
    topStrip->setContentsMargins(8, 4, 8, 4);
    topStrip->setSpacing(6);
    topStrip->addWidget(makeRibbonLabel(tr("Show")));
    topStrip->addWidget(m_slotCombo);
    topStrip->addWidget(makeSeparator(this));
    topStrip->addWidget(m_playBtn);
    topStrip->addWidget(m_stopBtn);
    topStrip->addWidget(m_resetBtn);
    topStrip->addWidget(makeSeparator(this));
    topStrip->addWidget(makeRibbonLabel(tr("Speed")));
    topStrip->addWidget(m_speedSpin);
    topStrip->addStretch(1);
    topStrip->addWidget(makeRibbonLabel(tr("Zoom")));
    topStrip->addWidget(m_zoomSpin);
    topStrip->addWidget(m_fitBtn);

    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(m_timeline, 1);
    row->addWidget(m_panel, 0);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addLayout(topStrip);
    root->addLayout(row, 1);

    connect(m_slotCombo, &QComboBox::currentIndexChanged, this,
            &AnimationTimingPanel::onSlotComboChanged);

    connect(m_timeline, &AnimationTimelineEditor::clipEdited, this,
            [this](int elementIndex, AnimationTimelineEditor::Slot, AnimationDef def) {
                pushAnimCmd(elementIndex, def);
            });
    connect(m_timeline, &AnimationTimelineEditor::rowClicked, this,
            &AnimationTimingPanel::onTimelineRowClicked);
    connect(m_timeline, &AnimationTimelineEditor::activeClipChanged, this,
            &AnimationTimingPanel::onTimelineActiveClipChanged);

    connect(m_panel, &ClipPropertiesPanel::clipChanged, this,
            &AnimationTimingPanel::onPanelClipChanged);

    connect(m_sel, &EditorTitle::selectionChanged, this, &AnimationTimingPanel::onSelectionChanged);
    connect(m_sel, &EditorTitle::selectionSetChanged, this,
            &AnimationTimingPanel::onSelectionChanged);

    connect(m_doc, &TitleDocument::documentChanged, this, &AnimationTimingPanel::onDocumentChanged);

    connect(m_playBtn, &QToolButton::clicked, this, [this]() {
        if (m_playing) {
            pausePlayback();
        } else {
            startPlayback();
        }
    });
    connect(m_stopBtn, &QToolButton::clicked, this, &AnimationTimingPanel::stopPlayback);
    connect(m_resetBtn, &QToolButton::clicked, this, &AnimationTimingPanel::resetToVisible);
    connect(m_playTimer, &QTimer::timeout, this, &AnimationTimingPanel::onPlayTick);

    connect(m_zoomSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int value) { m_timeline->setZoomPercent(value); });
    connect(m_timeline, &AnimationTimelineEditor::zoomChanged, this, [this](int percent) {
        const QSignalBlocker blocker(m_zoomSpin);
        m_zoomSpin->setValue(percent);
    });
    connect(m_fitBtn, &QToolButton::clicked, this, [this]() { m_timeline->zoomToFit(); });

    connect(m_timeline, &AnimationTimelineEditor::playheadMoved, this, [this](double t) {
        if (m_playing) pausePlayback();
        m_scrubTime = t;
        emitScrub(t);
    });

    {
        const QSignalBlocker blocker(m_zoomSpin);
        m_zoomSpin->setValue(m_timeline->zoomPercent());
    }

    m_slotCombo->setCurrentIndex(0);
    onSelectionChanged();
    updateTransportState();
}

void AnimationTimingPanel::onDocumentChanged()
{
    if (m_previewActive) {
        if (m_playing) pausePlayback();
        emitPreviewStopped();
    }

    UpdateGuard guard(m_updating);
    m_timeline->rebuild();
    if (m_activeElementIndex >= 1) {
        m_timeline->focusElement(m_activeElementIndex);
    } else {
        m_panel->setClip(false, {});
    }
}

void AnimationTimingPanel::onSelectionChanged()
{
    if (m_sel->selectionCount() > 1) {
        if (m_playing || m_previewActive) stopPlayback();
        setEnabled(false);
        return;
    }
    setEnabled(true);

    UpdateGuard guard(m_updating);
    const SelectionId sid = m_sel->selection();
    if (sid.level == SelectionId::Level::Element && sid.elementIndex >= 1) {
        m_activeElementIndex = sid.elementIndex;
        m_timeline->focusElement(m_activeElementIndex);
    } else {
        m_activeElementIndex = -1;
        m_timeline->clearActive();
        m_panel->setClip(false, {});
    }
}

void AnimationTimingPanel::onSlotComboChanged(int index)
{
    if (m_playing) pausePlayback();
    emitPreviewStopped();

    m_timeline->setSlot(static_cast<AnimationTimelineEditor::Slot>(index));
    if (m_activeElementIndex >= 1) {
        m_timeline->focusElement(m_activeElementIndex);
    } else {
        m_panel->setClip(false, {});
    }
}

void AnimationTimingPanel::onTimelineRowClicked(int elementIndex)
{
    m_activeElementIndex = elementIndex;
    UpdateGuard guard(m_updating);
    SelectionId sid;
    sid.level = SelectionId::Level::Element;
    sid.elementIndex = elementIndex;
    m_sel->setSelection(sid);
}

void AnimationTimingPanel::onTimelineActiveClipChanged(bool valid, AnimationDef def)
{
    m_panel->setClip(valid, def);
}

void AnimationTimingPanel::onPanelClipChanged(AnimationDef def)
{
    if (m_activeElementIndex >= 1) {
        pushAnimCmd(m_activeElementIndex, def);
    }
}

void AnimationTimingPanel::pushAnimCmd(int elementIndex, AnimationDef def)
{
    const auto& elements = m_doc->title().elements;
    if (elementIndex < 1 || static_cast<size_t>(elementIndex) >= elements.size()) return;

    const auto* ve = dynamic_cast<const VisualElement*>(elements[elementIndex].get());
    if (!ve) return;
    std::string id = ve->GetId();

    SetElementAnimCmd::Target target;
    switch (m_timeline->slot()) {
        case AnimationTimelineEditor::Slot::In: target = SetElementAnimCmd::Target::AnimIn; break;
        case AnimationTimelineEditor::Slot::Out: target = SetElementAnimCmd::Target::AnimOut; break;
        case AnimationTimelineEditor::Slot::DataIn:
            target = SetElementAnimCmd::Target::DataAnimIn;
            break;
        case AnimationTimelineEditor::Slot::DataOut:
            target = SetElementAnimCmd::Target::DataAnimOut;
            break;
    }

    m_doc->undoStack()->push(new SetElementAnimCmd(m_doc, id, target, def));
}

void AnimationTimingPanel::slotFlags(bool& isIn, bool& isData) const
{
    switch (m_timeline->slot()) {
        case AnimationTimelineEditor::Slot::In: isIn = true; isData = false; break;
        case AnimationTimelineEditor::Slot::Out: isIn = false; isData = false; break;
        case AnimationTimelineEditor::Slot::DataIn: isIn = true; isData = true; break;
        case AnimationTimelineEditor::Slot::DataOut: isIn = false; isData = true; break;
    }
}

void AnimationTimingPanel::startPlayback()
{
    const double total = m_timeline->contentDuration();
    if (total <= 0.0) return;
    if (m_scrubTime >= total) {
        m_scrubTime = 0.0;
    }
    m_timeline->setPlayhead(m_scrubTime);
    m_playing = true;
    m_playBtn->setIcon(themedIcon(Icons16::Media_Pause));
    m_playBtn->setToolTip(tr("Pause"));
    m_playClock.start();
    m_playTimer->start();
    updateTransportState();
}

void AnimationTimingPanel::pausePlayback()
{
    m_playTimer->stop();
    m_playing = false;
    m_playBtn->setIcon(themedIcon(Icons16::Media_Play));
    m_playBtn->setToolTip(tr("Play"));
    updateTransportState();
}

void AnimationTimingPanel::stopPlayback()
{
    pausePlayback();
    m_scrubTime = 0.0;
    m_timeline->setPlayhead(0.0);
    emitPreviewStopped();
}

void AnimationTimingPanel::resetToVisible()
{
    pausePlayback();
    emitPreviewStopped();
}

void AnimationTimingPanel::updateTransportState()
{
    m_stopBtn->setEnabled(m_playing || m_previewActive);
    m_resetBtn->setEnabled(m_previewActive);
}

void AnimationTimingPanel::emitScrub(double t)
{
    bool isIn, isData;
    slotFlags(isIn, isData);
    m_previewActive = true;
    emit scrubTimeChanged(isIn, isData, t);
    updateTransportState();
}

void AnimationTimingPanel::emitPreviewStopped()
{
    if (!m_previewActive) return;
    m_previewActive = false;
    emit previewStopped();
    updateTransportState();
}

void AnimationTimingPanel::onPlayTick()
{
    const double speed = m_speedSpin->value() / 100.0;
    m_scrubTime += (m_playClock.restart() / 1000.0) * speed;
    const double total = m_timeline->contentDuration();
    if (total <= 0.0) {
        stopPlayback();
        return;
    }
    if (m_scrubTime >= total) {
        m_scrubTime = total;
        m_timeline->setPlayhead(total);
        emitScrub(total);
        pausePlayback();
        return;
    }
    m_timeline->setPlayhead(m_scrubTime);
    emitScrub(m_scrubTime);
}
