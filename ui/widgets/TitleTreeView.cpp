#include "TitleTreeView.h"
#include "TitleTreeModel.h"

#include "engine/title.h"
#include "model/EditorTitle.h"
#include "model/TitleDocument.h"
#include "model/UndoCommands.h"

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMenu>

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
    setSelectionMode(QAbstractItemView::SingleSelection);

    expandAll();
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { expandAll(); });

    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &TitleTreeView::onViewSelectionChanged);

    connect(m_editorTitle, &EditorTitle::selectionChanged, this,
            &TitleTreeView::onEditorSelectionChanged);
}

void TitleTreeView::onViewSelectionChanged(const QItemSelection& selected,
                                           const QItemSelection& /*deselected*/)
{
    if (m_syncingSelection) return;
    m_syncingSelection = true;

    QModelIndexList indexes = selected.indexes();
    if (indexes.isEmpty()) {
        m_editorTitle->setSelection(SelectionId{});
    } else {
        SelectionId sid = m_model->selectionIdFor(indexes.first());
        m_editorTitle->setSelection(sid);
    }

    m_syncingSelection = false;
}

void TitleTreeView::onEditorSelectionChanged(SelectionId id)
{
    if (m_syncingSelection) return;
    m_syncingSelection = true;

    QModelIndex idx = m_model->indexForSelection(id);
    if (idx.isValid()) {
        selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
        scrollTo(idx);
    } else {
        selectionModel()->clearSelection();
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
        addElem("qr_", "qr_code", {{"w", 200}, {"h", 200}});
    });

    menu.addSeparator();

    // ── Remove ───────────────────────────────────────────────────────────────
    QAction* removeAction = menu.addAction("Remove");
    removeAction->setEnabled(elementSelected);
    connect(removeAction, &QAction::triggered, this, [this, sel]() {
        if (sel.level != SelectionId::Level::Element) return;
        const Title& t = m_doc->title();
        if (sel.elementIndex < 1 || sel.elementIndex >= (int)t.elements.size()) return;
        const std::string ei = t.elements[sel.elementIndex]->GetId();
        m_doc->undoStack()->push(new RemoveElementCmd(m_doc, ei));
    });

    menu.exec(event->globalPos());
}
