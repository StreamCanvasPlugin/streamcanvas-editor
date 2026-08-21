#include "TitleTreeView.h"
#include "TitleTreeModel.h"
#include "ZOrderOps.h"

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
#include <QMouseEvent>

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
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    // Column 1 is the narrow, fixed-width lock column; column 0 (name) stretches.
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::Fixed);
    setColumnWidth(1, 28);

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

void TitleTreeView::mousePressEvent(QMouseEvent* event)
{
    // A left-click in the lock column toggles that element's locked state
    // WITHOUT disturbing the row selection. Column 1 is non-selectable, so
    // letting the base view process the click would issue a ClearAndSelect
    // against a non-selectable index and drop the current selection.
    if (event->button() == Qt::LeftButton) {
        const QModelIndex idx = indexAt(event->pos());
        if (idx.isValid() && idx.column() == 1) {
            const SelectionId sid = m_model->selectionIdFor(idx);
            if (sid.level == SelectionId::Level::Element) {
                const Title& t = m_doc->title();
                if (sid.elementIndex >= 1 && sid.elementIndex < (int)t.elements.size()) {
                    const std::string id = t.elements[sid.elementIndex]->GetId();
                    m_doc->setElementLocked(id, !m_doc->isElementLocked(id));
                }
            }
            event->accept();
            return;
        }
    }
    QTreeView::mousePressEvent(event);
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
    // Id generation, z_order, and canvas-viewport placement all live in
    // MainWindow::makeNewElementJson (this view has no access to
    // CanvasWidget), so we just emit the per-type extras as a JSON string.
    auto emitAddElem = [this](const std::string& prefix, const std::string& type,
                              nlohmann::json extra = {}) {
        emit addElementRequested(QString::fromStdString(prefix), QString::fromStdString(type),
                                 QString::fromStdString(extra.dump()));
    };

    menu.addAction("Add Rectangle", this, [this, emitAddElem]() {
        emitAddElem("element_", "rectangle", {{"fill", {0.5, 0.5, 0.9, 1.0}}, {"h", 80}});
    });
    menu.addAction("Add Text", this, [this, emitAddElem]() {
        emitAddElem("text_", "text",
            {{"w", 300}, {"h", 60}, {"text", "New Text"}, {"fill", {1.0, 1.0, 1.0, 1.0}}});
    });
    menu.addAction("Add Image", this, [this, emitAddElem]() {
        emitAddElem("image_", "image", {{"w", 200}, {"h", 200}, {"scale_mode", "contain"}});
    });
    menu.addAction("Add QR Code", this, [this, emitAddElem]() {
        emitAddElem("qr_", "qr_code", {{"w", 200}, {"h", 200}, {"fill", {0.0, 0.0, 0.0, 1.0}}});
    });

    menu.addSeparator();

    // ── Order ────────────────────────────────────────────────────────────────
    {
        // Sibling count computed the same way TitleTreeModel::childrenOf does:
        // direct children of the same parent, VisualElements only.
        int siblingCount = 0;
        if (elementSelected && sel.elementIndex >= 1 &&
            sel.elementIndex < (int)m_doc->title().elements.size()) {
            const IElement* activeEl = m_doc->title().elements[sel.elementIndex].get();
            if (const IElement* parentEl = activeEl->GetParent()) {
                for (const IElement* child : parentEl->GetChildren())
                    if (dynamic_cast<const VisualElement*>(child))
                        ++siblingCount;
            }
        }
        const bool canReorder = elementSelected && siblingCount >= 2;

        QMenu* orderMenu = menu.addMenu("Order");
        orderMenu->setEnabled(canReorder);

        auto addOrderAction = [this, orderMenu](const QString& label, zorderops::ReorderOp op) {
            QAction* a = orderMenu->addAction(label);
            connect(a, &QAction::triggered, this, [this, op]() {
                const SelectionId s = m_editorTitle->selection();
                if (s.level != SelectionId::Level::Element) return;
                const Title& t = m_doc->title();
                if (s.elementIndex < 1 || s.elementIndex >= (int)t.elements.size()) return;
                zorderops::applyReorder(m_doc, t.elements[s.elementIndex].get(), op);
            });
        };
        addOrderAction("Bring to Front", zorderops::ReorderOp::ToFront);
        addOrderAction("Bring Forward",  zorderops::ReorderOp::Forward);
        addOrderAction("Send Backward",  zorderops::ReorderOp::Backward);
        addOrderAction("Send to Back",   zorderops::ReorderOp::ToBack);
    }

    menu.addSeparator();

    // ── Duplicate / Remove ───────────────────────────────────────────────────
    QAction* duplicateAction = menu.addAction("Duplicate");
    duplicateAction->setEnabled(elementSelected);
    connect(duplicateAction, &QAction::triggered, this, [this]() { emit duplicateRequested(); });

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

    menu.addSeparator();

    // ── Clipboard ────────────────────────────────────────────────────────────
    QAction* cutAction = menu.addAction("Cut");
    cutAction->setEnabled(elementSelected);
    connect(cutAction, &QAction::triggered, this, [this]() { emit cutRequested(); });

    QAction* copyAction = menu.addAction("Copy");
    copyAction->setEnabled(elementSelected);
    connect(copyAction, &QAction::triggered, this, [this]() { emit copyRequested(); });

    QAction* pasteAction = menu.addAction("Paste");
    connect(pasteAction, &QAction::triggered, this, [this]() { emit pasteRequested(); });

    QAction* pasteInPlaceAction = menu.addAction("Paste in Place");
    connect(pasteInPlaceAction, &QAction::triggered, this, [this]() { emit pasteInPlaceRequested(); });

    menu.exec(event->globalPos());
}
