#include "TitleTreeModel.h"

#include <QDataStream>
#include <QIcon>
#include <QIODevice>
#include <algorithm>

#include <QPainter>
#include <QPixmap>

#include "engine/element_image.h"
#include "engine/element_qr.h"
#include "engine/element_text.h"
#include "engine/visual_element.h"
#include "icons.h"
#include "model/TitleDocument.h"
#include "model/UndoCommands.h"

namespace {
// A dimmed variant of a themed icon, used for the "unlocked" affordance so it
// reads as a subtle hint rather than competing visually with the lock icon.
QIcon fadedIcon(Icons16 id, qreal opacity)
{
    const QIcon base = themedIcon(id);
    const QPixmap src = base.pixmap(16, 16);
    QPixmap faded(src.size());
    faded.fill(Qt::transparent);
    QPainter p(&faded);
    p.setOpacity(opacity);
    p.drawPixmap(0, 0, src);
    p.end();
    return QIcon(faded);
}
} // namespace

TitleTreeModel::TitleTreeModel(TitleDocument* doc, QObject* parent)
    : QAbstractItemModel(parent), m_doc(doc)
{
    connect(m_doc, &TitleDocument::documentChanged, this, &TitleTreeModel::onDocumentChanged);
}

void TitleTreeModel::onDocumentChanged()
{
    if (m_suppressResets) {
        m_resetPending = true;
        return;
    }
    beginResetModel();
    endResetModel();
}

void TitleTreeModel::setResetsSuppressed(bool suppressed)
{
    if (m_suppressResets == suppressed)
        return;
    m_suppressResets = suppressed;
    if (!suppressed && m_resetPending) {
        m_resetPending = false;
        beginResetModel();
        endResetModel();
    }
}

// ── Tree helpers ──────────────────────────────────────────────────────────────

QVector<int> TitleTreeModel::childrenOf(int parentEi) const
{
    const Title& t = m_doc->title();
    const IElement* parentEl = (parentEi == -1) ? t.GetRoot()
                                                 : t.elements[parentEi].get();
    if (!parentEl) return {};

    QVector<int> result;
    for (const IElement* child : parentEl->GetChildren()) {
        for (int i = 1; i < static_cast<int>(t.elements.size()); ++i) {
            if (t.elements[i].get() == child) {
                result.push_back(i);
                break;
            }
        }
    }

    std::stable_sort(result.begin(), result.end(), [&](int a, int b) {
        const auto* va = dynamic_cast<const VisualElement*>(t.elements[a].get());
        const auto* vb = dynamic_cast<const VisualElement*>(t.elements[b].get());
        int za = va ? va->zOrder : 0;
        int zb = vb ? vb->zOrder : 0;
        return za > zb;
    });
    return result;
}

int TitleTreeModel::rowOfElement(int ei) const
{
    const Title& t = m_doc->title();
    if (ei < 1 || ei >= static_cast<int>(t.elements.size())) return -1;
    IElement* parentEl = t.elements[ei]->GetParent();
    int parentEi = -1;
    if (parentEl && parentEl != t.GetRoot()) {
        for (int i = 1; i < static_cast<int>(t.elements.size()); ++i) {
            if (t.elements[i].get() == parentEl) { parentEi = i; break; }
        }
    }
    QVector<int> siblings = childrenOf(parentEi);
    for (int r = 0; r < siblings.size(); ++r)
        if (siblings[r] == ei) return r;
    return -1;
}

int TitleTreeModel::parentEiOf(int ei) const
{
    const Title& t = m_doc->title();
    if (ei < 1 || ei >= static_cast<int>(t.elements.size())) return -1;
    IElement* p = t.elements[ei]->GetParent();
    if (!p || p == t.GetRoot()) return -1;
    for (int i = 1; i < static_cast<int>(t.elements.size()); ++i)
        if (t.elements[i].get() == p) return i;
    return -1;
}

// ── QAbstractItemModel overrides ──────────────────────────────────────────────

QModelIndex TitleTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    if (!parent.isValid()) {
        if (row == 0)
            return createIndex(0, column, makeId(LEVEL_SCENE, -1));
        return {};
    }

    quintptr pid = parent.internalId();
    if (levelOf(pid) == LEVEL_SCENE) {
        QVector<int> kids = childrenOf(-1);
        if (row >= kids.size()) return {};
        return createIndex(row, column, makeId(LEVEL_ELEMENT, kids[row]));
    }

    if (levelOf(pid) == LEVEL_ELEMENT) {
        int pei = eiOf(pid);
        QVector<int> kids = childrenOf(pei);
        if (row >= kids.size()) return {};
        return createIndex(row, column, makeId(LEVEL_ELEMENT, kids[row]));
    }

    return {};
}

QModelIndex TitleTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid()) return {};

    quintptr id = child.internalId();
    if (levelOf(id) == LEVEL_SCENE) return {};

    int ei = eiOf(id);
    int pei = parentEiOf(ei);

    if (pei == -1)
        return createIndex(0, 0, makeId(LEVEL_SCENE, -1));

    int grandRow = rowOfElement(pei);
    if (grandRow < 0) grandRow = 0;
    return createIndex(grandRow, 0, makeId(LEVEL_ELEMENT, pei));
}

int TitleTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) return 1;

    quintptr id = parent.internalId();
    if (levelOf(id) == LEVEL_SCENE)
        return childrenOf(-1).size();

    if (levelOf(id) == LEVEL_ELEMENT)
        return childrenOf(eiOf(id)).size();

    return 0;
}

int TitleTreeModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 2;
}

QVariant TitleTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) return {};

    quintptr id = index.internalId();
    quintptr level = levelOf(id);

    // Column 1 is the lock column: decoration only, no text, on element rows.
    if (index.column() == 1) {
        if (role == Qt::DecorationRole && level == LEVEL_ELEMENT) {
            int ei = eiOf(id);
            const Title& t = m_doc->title();
            if (ei < 1 || ei >= static_cast<int>(t.elements.size())) return {};
            const std::string elId = t.elements[ei]->GetId();
            if (m_doc->isElementLocked(elId))
                return themedIcon(Icons16::Action_Lock);
            return fadedIcon(Icons16::Action_Unlock, 0.35);
        }
        return {};
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (level == LEVEL_SCENE)
            return m_doc->titleName();

        if (level == LEVEL_ELEMENT) {
            int ei = eiOf(id);
            const Title& t = m_doc->title();
            if (ei < 1 || ei >= static_cast<int>(t.elements.size())) return {};
            return QString::fromStdString(t.elements[ei]->GetId());
        }
    }

    if (role == Qt::DecorationRole) {
        if (level == LEVEL_SCENE)
            return themedIcon(Icons16::Misc_FilmRoll);

        if (level == LEVEL_ELEMENT) {
            int ei = eiOf(id);
            const Title& t = m_doc->title();
            if (ei < 1 || ei >= static_cast<int>(t.elements.size())) return {};
            const IElement* el = t.elements[ei].get();
            if (dynamic_cast<const TextElement*>(el))
                return themedIcon(Icons16::File_Font);
            if (dynamic_cast<const ImageElement*>(el))
                return themedIcon(Icons16::File_Picture);
            if (dynamic_cast<const QrElement*>(el))
                return qrCodeIcon();
            return themedIcon(Icons16::Shape_Square);
        }
    }

    return {};
}

bool TitleTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;

    quintptr id = index.internalId();
    if (levelOf(id) != LEVEL_ELEMENT)
        return false;

    int ei = eiOf(id);
    const Title& t = m_doc->title();
    if (ei < 1 || ei >= static_cast<int>(t.elements.size()))
        return false;

    const std::string oldId = t.elements[ei]->GetId();
    const std::string newId = value.toString().trimmed().toStdString();

    if (newId.empty())
        return false;
    if (newId == oldId)
        return true;

    for (const auto& up : t.elements) {
        if (up && up->GetId() == newId)
            return false;
    }

    m_doc->undoStack()->push(new SetElementIdCmd(m_doc, oldId, newId));
    return true;
}

Qt::ItemFlags TitleTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;

    quintptr id = index.internalId();

    // Column 1 (lock column): clickable via the view, but not selectable,
    // editable, or a drag/drop target — it's a standalone icon toggle.
    if (index.column() == 1) {
        if (levelOf(id) == LEVEL_ELEMENT)
            return Qt::ItemIsEnabled;
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
    if (levelOf(id) == LEVEL_ELEMENT) {
        f |= Qt::ItemIsDragEnabled;

        int ei = eiOf(id);
        const Title& t = m_doc->title();
        if (ei >= 1 && ei < static_cast<int>(t.elements.size()) &&
            t.elements[ei]->GetChildren().empty()) {
            f |= Qt::ItemIsEditable;
        }
    }

    return f;
}

Qt::DropActions TitleTreeModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

QStringList TitleTreeModel::mimeTypes() const
{
    return {"application/x-obs-title-node"};
}

QMimeData* TitleTreeModel::mimeData(const QModelIndexList& indexes) const
{
    if (indexes.isEmpty()) return nullptr;

    auto* mime = new QMimeData;
    QByteArray bytes;
    bytes.resize(8);

    quintptr id = indexes.first().internalId();
    for (int i = 7; i >= 0; --i) {
        bytes[i] = static_cast<char>(id & 0xFF);
        id >>= 8;
    }
    mime->setData("application/x-obs-title-node", bytes);
    return mime;
}

bool TitleTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action,
                                  int row, int /*column*/, const QModelIndex& parent)
{
    if (action != Qt::MoveAction) return false;
    if (!data->hasFormat("application/x-obs-title-node")) return false;

    QByteArray encoded = data->data("application/x-obs-title-node");
    if (encoded.size() < 8) return false;

    quintptr srcId = 0;
    for (int i = 0; i < 8; ++i)
        srcId = (srcId << 8) | static_cast<unsigned char>(encoded[i]);

    if (levelOf(srcId) != LEVEL_ELEMENT) return false;

    int srcEi = eiOf(srcId);
    const Title& t = m_doc->title();
    if (srcEi < 1 || srcEi >= static_cast<int>(t.elements.size())) return false;

    const std::string srcId_str = t.elements[srcEi]->GetId();

    // ── Determine destination parent ──────────────────────────────────────────
    // row == -1 means drop ON the parent item (reparent to it).
    // row >= 0 means drop BETWEEN children of parent (reorder / cross-reparent).

    int destParentEi = -1;  // -1 = root
    if (parent.isValid()) {
        quintptr pid = parent.internalId();
        if (levelOf(pid) == LEVEL_ELEMENT)
            destParentEi = eiOf(pid);
        // LEVEL_SCENE → destParentEi stays -1 (root)
    }

    const std::string destParentId = (destParentEi == -1)
        ? "" : t.elements[destParentEi]->GetId();

    // Prevent dropping an element onto itself or onto one of its own descendants
    {
        IElement* probe = (destParentEi == -1) ? t.GetRoot() : t.elements[destParentEi].get();
        while (probe) {
            if (probe == t.elements[srcEi].get()) return false;
            probe = probe->GetParent();
        }
    }

    auto* macro = new QUndoCommand(row == -1 ? "reparent element" : "reorder element");

    // ── Reparent if parent changes ────────────────────────────────────────────
    const int curParentEi = parentEiOf(srcEi);
    const bool needsReparent = (curParentEi != destParentEi);

    if (row == -1) {
        // Drop ON: just reparent (append to dest's children)
        if (!needsReparent) { delete macro; return false; }
        m_doc->undoStack()->push(
            new SetElementParentCmd(m_doc, srcId_str, destParentId));
        return true;
    }

    // Drop BETWEEN rows — may need reparent + zOrder assignment.
    if (needsReparent) {
        new SetElementParentCmd(m_doc, srcId_str, destParentId, macro);
    }

    // After (potential) reparent all siblings share destParentEi.
    // Re-fetch children in the order they *will* be after reparenting.
    QVector<int> siblings = childrenOf(destParentEi);
    if (needsReparent) {
        // srcEi may not be in the list yet if reparent hasn't run; add it.
        if (!siblings.contains(srcEi))
            siblings.append(srcEi);
    }

    int n = siblings.size();
    if (n < 1) { delete macro; return false; }

    // Remove src from its current position in the list (if already there)
    int srcRow = -1;
    for (int r = 0; r < n; ++r) if (siblings[r] == srcEi) { srcRow = r; break; }
    if (srcRow >= 0) siblings.remove(srcRow);

    int insertPos = std::clamp(row, 0, static_cast<int>(siblings.size()));
    siblings.insert(insertPos, srcEi);

    n = siblings.size();
    for (int r = 0; r < n; ++r) {
        int ei = siblings[r];
        int newZ = n - 1 - r;
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[ei].get());
        if (ve && ve->zOrder != newZ) {
            new SetElementFieldCmd<int>(
                m_doc, t.elements[ei]->GetId(), newZ,
                [](VisualElement& e) -> int& { return e.zOrder; },
                "zOrder", -1, macro);
        }
    }

    if (macro->childCount() == 0) { delete macro; return false; }
    m_doc->undoStack()->push(macro);
    return true;
}

// ── Selection helpers ─────────────────────────────────────────────────────────

SelectionId TitleTreeModel::selectionIdFor(const QModelIndex& index) const
{
    if (!index.isValid()) return SelectionId{};

    quintptr id = index.internalId();
    if (levelOf(id) == LEVEL_SCENE)
        return SelectionId{SelectionId::Level::Title, -1};

    if (levelOf(id) == LEVEL_ELEMENT)
        return SelectionId{SelectionId::Level::Element, eiOf(id)};

    return SelectionId{};
}

QModelIndex TitleTreeModel::indexForSelection(const SelectionId& sel) const
{
    switch (sel.level) {
    case SelectionId::Level::Title:
        return createIndex(0, 0, makeId(LEVEL_SCENE, -1));

    case SelectionId::Level::Element: {
        int ei = sel.elementIndex;
        int row = rowOfElement(ei);
        if (row < 0) return {};
        return createIndex(row, 0, makeId(LEVEL_ELEMENT, ei));
    }

    default:
        return {};
    }
}
