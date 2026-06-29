#include "TitleTreeModel.h"

#include <QDataStream>
#include <QIcon>
#include <QIODevice>
#include <algorithm>
#include <numeric>

#include "engine/element_image.h"
#include "engine/element_qr.h"
#include "engine/element_text.h"
#include "engine/visual_element.h"
#include "icons.h"
#include "model/TitleDocument.h"
#include "model/UndoCommands.h"

TitleTreeModel::TitleTreeModel(TitleDocument* doc, QObject* parent)
    : QAbstractItemModel(parent), m_doc(doc)
{
    connect(m_doc, &TitleDocument::documentChanged, this, [this]() {
        beginResetModel();
        endResetModel();
    });
}

// ── Z-Order sort helpers ──────────────────────────────────────────────────────

QVector<int> TitleTreeModel::sortedElementIndices() const
{
    const Title& t = m_doc->title();
    int n = static_cast<int>(t.elements.size());
    // Skip index 0 (root); collect 1..n-1
    QVector<int> idx;
    idx.reserve(n - 1);
    for (int i = 1; i < n; ++i)
        idx.push_back(i);
    // Descending: highest zOrder → row 0 (topmost in the layer list)
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
        const auto* va = dynamic_cast<const VisualElement*>(t.elements[a].get());
        const auto* vb = dynamic_cast<const VisualElement*>(t.elements[b].get());
        int za = va ? va->zOrder : 0;
        int zb = vb ? vb->zOrder : 0;
        return za > zb;
    });
    return idx;
}

int TitleTreeModel::sortedElementRow(int ei) const
{
    auto sorted = sortedElementIndices();
    for (int r = 0; r < sorted.size(); ++r)
        if (sorted[r] == ei)
            return r;
    return -1;
}

// ── QAbstractItemModel overrides ──────────────────────────────────────────────

QModelIndex TitleTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    if (!parent.isValid()) {
        // Root level: only the title node
        if (row == 0)
            return createIndex(row, column, makeId(LEVEL_SCENE, -1));
        return {};
    }

    quintptr pid = parent.internalId();
    if (levelOf(pid) == LEVEL_SCENE) {
        auto sorted = sortedElementIndices();
        if (row >= sorted.size())
            return {};
        return createIndex(row, column, makeId(LEVEL_ELEMENT, sorted[row]));
    }

    return {};
}

QModelIndex TitleTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return {};

    quintptr id = child.internalId();
    if (levelOf(id) == LEVEL_SCENE)
        return {};

    // All elements are children of the title node
    return createIndex(0, 0, makeId(LEVEL_SCENE, -1));
}

int TitleTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return 1; // just the title node

    quintptr id = parent.internalId();
    if (levelOf(id) == LEVEL_SCENE) {
        const Title& t = m_doc->title();
        return std::max(0, static_cast<int>(t.elements.size()) - 1); // exclude root
    }

    return 0; // elements have no children in the tree
}

int TitleTreeModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 1;
}

QVariant TitleTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    quintptr id = index.internalId();
    quintptr level = levelOf(id);

    if (role == Qt::DisplayRole) {
        if (level == LEVEL_SCENE)
            return m_doc->titleName();

        if (level == LEVEL_ELEMENT) {
            int ei = eiOf(id);
            const Title& t = m_doc->title();
            if (ei < 1 || ei >= static_cast<int>(t.elements.size()))
                return {};
            return QString::fromStdString(t.elements[ei]->GetId());
        }
    }

    if (role == Qt::DecorationRole) {
        if (level == LEVEL_SCENE)
            return themedIcon(Icons16::Misc_FilmRoll);

        if (level == LEVEL_ELEMENT) {
            int ei = eiOf(id);
            const Title& t = m_doc->title();
            if (ei < 1 || ei >= static_cast<int>(t.elements.size()))
                return {};
            const IElement* el = t.elements[ei].get();
            if (dynamic_cast<const TextElement*>(el))
                return themedIcon(Icons16::File_Font);
            if (dynamic_cast<const ImageElement*>(el))
                return themedIcon(Icons16::File_Picture);
            if (dynamic_cast<const QrElement*>(el))
                return themedIcon(Icons16::Hardware_Scanner);
            return themedIcon(Icons16::Shape_Square);
        }
    }

    return {};
}

Qt::ItemFlags TitleTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;

    quintptr id = index.internalId();
    Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (levelOf(id) == LEVEL_ELEMENT)
        f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

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
    if (indexes.isEmpty())
        return nullptr;

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
    if (action != Qt::MoveAction)
        return false;
    if (!data->hasFormat("application/x-obs-title-node"))
        return false;

    QByteArray encoded = data->data("application/x-obs-title-node");
    if (encoded.size() < 8)
        return false;

    quintptr srcId = 0;
    for (int i = 0; i < 8; ++i)
        srcId = (srcId << 8) | static_cast<unsigned char>(encoded[i]);

    if (levelOf(srcId) != LEVEL_ELEMENT)
        return false;

    int srcEi = eiOf(srcId);
    auto sorted = sortedElementIndices();
    int n = sorted.size();
    if (n < 2)
        return false;

    int srcRow = sortedElementRow(srcEi);
    if (srcRow < 0)
        return false;

    int destRow = row;
    if (destRow < 0) {
        if (parent.isValid() && levelOf(parent.internalId()) == LEVEL_ELEMENT)
            destRow = sortedElementRow(eiOf(parent.internalId()));
        else
            destRow = n - 1;
    }
    destRow = std::clamp(destRow, 0, n - 1);

    if (destRow == srcRow)
        return false;

    // Build new zOrder assignment
    QVector<int> newOrder = sorted;
    newOrder.remove(srcRow);
    int insertPos = (destRow > srcRow) ? destRow - 1 : destRow;
    insertPos = std::clamp(insertPos, 0, n - 1);
    newOrder.insert(insertPos, srcEi);

    const Title& t = m_doc->title();
    auto* macro = new QUndoCommand("reorder element");
    for (int r = 0; r < n; ++r) {
        int ei = newOrder[r];
        int newZ = n - 1 - r;
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[ei].get());
        if (ve && ve->zOrder != newZ) {
            const std::string eid = t.elements[ei]->GetId();
            new SetElementFieldCmd<int>(
                m_doc, eid, newZ,
                [](VisualElement& e) -> int& { return e.zOrder; },
                "zOrder", -1, macro);
        }
    }
    m_doc->undoStack()->push(macro);
    return true;
}

SelectionId TitleTreeModel::selectionIdFor(const QModelIndex& index) const
{
    if (!index.isValid())
        return SelectionId{};

    quintptr id = index.internalId();
    quintptr level = levelOf(id);

    if (level == LEVEL_SCENE)
        return SelectionId{SelectionId::Level::Title, -1};

    if (level == LEVEL_ELEMENT)
        return SelectionId{SelectionId::Level::Element, eiOf(id)};

    return SelectionId{};
}

QModelIndex TitleTreeModel::indexForSelection(const SelectionId& sel) const
{
    switch (sel.level) {
    case SelectionId::Level::Title:
        return createIndex(0, 0, makeId(LEVEL_SCENE, -1));

    case SelectionId::Level::Element: {
        int row = sortedElementRow(sel.elementIndex);
        if (row < 0)
            return {};
        return createIndex(row, 0, makeId(LEVEL_ELEMENT, sel.elementIndex));
    }

    default:
        return {};
    }
}
