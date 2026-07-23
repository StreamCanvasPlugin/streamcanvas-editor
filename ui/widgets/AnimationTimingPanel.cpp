#include "ui/widgets/AnimationTimingPanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>

#include "engine/title.h"
#include "engine/visual_element.h"
#include "model/EditorTitle.h"
#include "model/TitleDocument.h"
#include "model/UndoCommands.h"
#include "ui/UiUtils.h"
#include "ui/widgets/ClipPropertiesPanel.h"

AnimationTimingPanel::AnimationTimingPanel(TitleDocument* doc, EditorTitle* sel, QWidget* parent)
    : QWidget(parent), m_doc(doc), m_sel(sel)
{
    m_timeline = new AnimationTimelineEditor(doc, this);
    m_panel = new ClipPropertiesPanel(this);

    m_slotCombo = new QComboBox(this);
    m_slotCombo->addItem(tr("In"));
    m_slotCombo->addItem(tr("Out"));
    m_slotCombo->addItem(tr("Data In"));
    m_slotCombo->addItem(tr("Data Out"));

    m_playBtn = new QPushButton(QStringLiteral("▶"), this);
    m_playBtn->setFixedWidth(34);

    m_speedSpin = new QSpinBox(this);
    m_speedSpin->setRange(10, 400);
    m_speedSpin->setSuffix(tr(" %"));
    m_speedSpin->setValue(100);
    m_speedSpin->setFixedWidth(70);
    m_speedSpin->setToolTip(tr("Playback speed"));

    m_zoomSpin = new QSpinBox(this);
    m_zoomSpin->setRange(10, 1000);
    m_zoomSpin->setSuffix(tr(" %"));
    m_zoomSpin->setValue(100);
    m_zoomSpin->setFixedWidth(80);
    m_zoomSpin->setToolTip(tr("Zoom"));

    m_fitBtn = new QPushButton(tr("Fit"), this);
    m_fitBtn->setFixedWidth(44);
    m_fitBtn->setToolTip(tr("Zoom to fit all clips"));

    m_playTimer = new QTimer(this);
    m_playTimer->setInterval(16);

    auto* topStrip = new QHBoxLayout();
    topStrip->setContentsMargins(4, 4, 4, 4);
    topStrip->addWidget(new QLabel(tr("Show:"), this));
    topStrip->addWidget(m_slotCombo);
    topStrip->addStretch(1);
    topStrip->addWidget(m_playBtn);
    topStrip->addWidget(new QLabel(tr("Speed"), this));
    topStrip->addWidget(m_speedSpin);
    topStrip->addStretch(1);
    topStrip->addWidget(new QLabel(tr("Zoom"), this));
    topStrip->addWidget(m_zoomSpin);
    topStrip->addWidget(m_fitBtn);
    topStrip->addStretch(1);

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

    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        if (m_playing) {
            stopPlayback();
        } else {
            startPlayback();
        }
    });
    connect(m_playTimer, &QTimer::timeout, this, &AnimationTimingPanel::onPlayTick);

    connect(m_zoomSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int value) { m_timeline->setZoomPercent(value); });
    connect(m_timeline, &AnimationTimelineEditor::zoomChanged, this, [this](int percent) {
        const QSignalBlocker blocker(m_zoomSpin);
        m_zoomSpin->setValue(percent);
    });
    connect(m_fitBtn, &QPushButton::clicked, this, [this]() { m_timeline->zoomToFit(); });

    connect(m_timeline, &AnimationTimelineEditor::playheadMoved, this, [this](double t) {
        bool isIn, isData;
        slotFlags(isIn, isData);
        emit scrubTimeChanged(isIn, isData, t);
    });

    {
        const QSignalBlocker blocker(m_zoomSpin);
        m_zoomSpin->setValue(m_timeline->zoomPercent());
    }

    m_slotCombo->setCurrentIndex(0);
    onSelectionChanged();
}

void AnimationTimingPanel::onDocumentChanged()
{
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
        if (m_playing) stopPlayback();
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
    m_playing = true;
    m_playBtn->setText(QStringLiteral("■"));
    m_scrubTime = 0.0;
    m_playTimer->start();
}

void AnimationTimingPanel::stopPlayback()
{
    m_playing = false;
    m_playBtn->setText(QStringLiteral("▶"));
    m_playTimer->stop();
    m_scrubTime = 0.0;
    m_timeline->setPlayhead(0.0);
    emit previewStopped();
}

void AnimationTimingPanel::onPlayTick()
{
    const double speed = m_speedSpin->value() / 100.0;
    m_scrubTime += (m_playTimer->interval() / 1000.0) * speed;
    const double total = m_timeline->contentDuration();
    bool isIn, isData;
    slotFlags(isIn, isData);
    if (total <= 0.0 || m_scrubTime >= total) {
        m_timeline->setPlayhead(total);
        emit scrubTimeChanged(isIn, isData, total);
        stopPlayback();
        return;
    }
    m_timeline->setPlayhead(m_scrubTime);
    emit scrubTimeChanged(isIn, isData, m_scrubTime);
}
