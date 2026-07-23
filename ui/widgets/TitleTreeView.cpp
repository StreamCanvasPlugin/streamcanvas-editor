#include "TitleTreeView.h"
#include "TitleTreeModel.h"

#include "engine/title.h"
#include "model/EditorTitle.h"
#include "model/TitleDocument.h"
#include "model/UndoCommands.h"

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMenu>

#include <algorithm>

TitleTreeView::TitleTreeView(TitleDocument* doc, EditorTitle* editorTitle, QWidget* parent)
    : QTreeView(parent), m_doc(doc), m_editorTitle(editorTitle),
      m_model(new TitleTreeModel(doc, this))
{
    setModel(m_model);
    setHeaderHidden(true);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    expandAll();
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() {
        expandAll();
        onEditorSelectionSetChanged();
    });

    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &TitleTreeView::onViewSelectionChanged);

    connect(m_editorTitle, &EditorTitle::selectionSetChanged, this,
            &TitleTreeView::onEditorSelectionSetChanged);
}

void TitleTreeView::setResetsSuppressed(bool suppressed)
{
    m_model->setResetsSuppressed(suppressed);
}

void TitleTreeView::onViewSelectionChanged(const QItemSelection& /*selected*/,
                                           const QItemSelection& /*deselected*/)
{
    if (m_syncingSelection) return;
    m_syncingSelection = true;

    const QModelIndexList idxs = selectionModel()->selectedIndexes();
    std::vector<int> elems;
    for (const QModelIndex& idx : idxs) {
        const SelectionId sid = m_model->selectionIdFor(idx);
        if (sid.level == SelectionId::Level::Element)
            elems.push_back(sid.elementIndex);
    }

    if (elems.empty()) {
        // No element rows selected: fall back to Title/None single behaviour.
        if (idxs.isEmpty())
            m_editorTitle->setSelection(SelectionId{});
        else
            m_editorTitle->setSelection(m_model->selectionIdFor(idxs.first()));
    } else {
        // Active = the current row if it is one of the selected elements, else the last.
        int active = elems.back();
        const SelectionId curSid = m_model->selectionIdFor(currentIndex());
        if (curSid.level == SelectionId::Level::Element &&
            std::find(elems.begin(), elems.end(), curSid.elementIndex) != elems.end())
            active = curSid.elementIndex;
        m_editorTitle->setMultiSelection(elems, active);
    }

    m_syncingSelection = false;
}

void TitleTreeView::onEditorSelectionSetChanged()
{
    if (m_syncingSelection) return;
    m_syncingSelection = true;

    const std::vector<int> elems = m_editorTitle->selectedIndices();
    if (elems.empty()) {
        // Title-level or None: preserve prior single-row behaviour.
        const QModelIndex idx = m_model->indexForSelection(m_editorTitle->selection());
        if (idx.isValid()) {
            selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
            scrollTo(idx);
        } else {
            selectionModel()->clearSelection();
        }
    } else {
        QItemSelection newSel;
        for (int ei : elems) {
            const QModelIndex idx = m_model->indexForSelection({SelectionId::Level::Element, ei});
            if (idx.isValid()) newSel.select(idx, idx);
        }
        selectionModel()->select(newSel, QItemSelectionModel::ClearAndSelect);
        // Set the current/anchor row to the active element and scroll it into view.
        const SelectionId active = m_editorTitle->selection();
        if (active.level == SelectionId::Level::Element) {
            const QModelIndex aidx = m_model->indexForSelection(active);
            if (aidx.isValid()) {
                selectionModel()->setCurrentIndex(aidx, QItemSelectionModel::NoUpdate);
                scrollTo(aidx);
            }
        }
    }

    m_syncingSelection = false;
}

bool TitleTreeView::event(QEvent* e)
{
    if (e->type() == QEvent::ShortcutOverride) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->matches(QKeySequence::Copy) ||
            ke->matches(QKeySequence::Cut)  ||
            ke->matches(QKeySequence::Paste)) {
            ke->ignore();
            return true;
        }
    }
    return QTreeView::event(e);
}

void TitleTreeView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);

    QModelIndex current = currentIndex();
    SelectionId sel = m_model->selectionIdFor(current);
    const bool elementSelected = (sel.level == SelectionId::Level::Element);

    // ── Add Element ──────────────────────────────────────────────────────────
    auto addElem = [this](const std::string& prefix, const std::string& type,
                          nlohmann::json extra = {}) {
        const Title& t = m_doc->title();
        int maxN = 0;
        for (int i = 1; i < (int)t.elements.size(); ++i) {
            const std::string& eid = t.elements[i]->GetId();
            if (eid.rfind(prefix, 0) == 0)
                try { maxN = std::max(maxN, std::stoi(eid.substr(prefix.size()))); } catch (...) {}
        }
        std::string newId = prefix + std::to_string(maxN + 1);
        using json = nlohmann::json;
        json j = {{"id", newId}, {"type", type}, {"x", 100}, {"y", 100},
                  {"w", 200}, {"h", 80}, {"z_order", std::max(0, (int)t.elements.size() - 1)}};
        for (auto& [k, v] : extra.items()) j[k] = v;
        m_doc->undoStack()->push(new AddElementCmd(m_doc, std::move(j)));
    };

    menu.addAction("Add Rectangle", this, [this, addElem]() {
        addElem("element_", "rectangle", {{"fill", {0.5, 0.5, 0.9, 1.0}}, {"h", 80}});
    });
    menu.addAction("Add Text", this, [this, addElem]() {
        addElem("text_", "text",
            {{"w", 300}, {"h", 60}, {"text", "New Text"}, {"fill", {1.0, 1.0, 1.0, 1.0}}});
    });
    menu.addAction("Add Image", this, [this, addElem]() {
        addElem("image_", "image", {{"w", 200}, {"h", 200}, {"scale_mode", "contain"}});
    });
    menu.addAction("Add QR Code", this, [this, addElem]() {
        addElem("qr_", "qr_code", {{"w", 200}, {"h", 200}, {"fill", {0.0, 0.0, 0.0, 1.0}}});
    });

    menu.addSeparator();

    // ── Remove ───────────────────────────────────────────────────────────────
    QAction* removeAction = menu.addAction("Remove");
    removeAction->setEnabled(elementSelected);
    connect(removeAction, &QAction::triggered, this, [this]() {
        const std::vector<int> indices = m_editorTitle->selectedIndices();
        if (indices.empty()) return;
        const Title& t = m_doc->title();
        std::vector<std::string> ids;
        for (int i : indices)
            if (i >= 1 && i < (int)t.elements.size())
                ids.push_back(t.elements[i]->GetId());
        if (ids.empty()) return;
        if (ids.size() == 1) {
            m_doc->undoStack()->push(new RemoveElementCmd(m_doc, ids[0]));
        } else {
            m_doc->undoStack()->beginMacro("Delete elements");
            for (const auto& id : ids) m_doc->undoStack()->push(new RemoveElementCmd(m_doc, id));
            m_doc->undoStack()->endMacro();
        }
    });

    menu.exec(event->globalPos());
}
