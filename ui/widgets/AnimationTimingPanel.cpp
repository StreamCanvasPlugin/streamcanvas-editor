#include "ui/widgets/AnimationTimingPanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
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

    auto* topStrip = new QHBoxLayout();
    topStrip->setContentsMargins(4, 4, 4, 4);
    topStrip->addWidget(new QLabel(tr("Show:"), this));
    topStrip->addWidget(m_slotCombo);
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
