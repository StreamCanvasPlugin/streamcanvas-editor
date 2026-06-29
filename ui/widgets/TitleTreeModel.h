#pragma once

#include <QAbstractItemModel>
#include <QMimeData>
#include <QVector>

#include "model/EditorTitle.h"

class TitleDocument;

class TitleTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit TitleTreeModel(TitleDocument* doc, QObject* parent = nullptr);

    // QAbstractItemModel overrides
    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // Drag-and-drop
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;

    // Selection helpers
    SelectionId selectionIdFor(const QModelIndex& index) const;
    QModelIndex indexForSelection(const SelectionId& sel) const;

private:
    TitleDocument* m_doc;

    // Returns title.elements indices [1..] sorted descending by zOrder (highest = row 0)
    QVector<int> sortedElementIndices() const;
    int sortedElementRow(int ei) const;  // ei = index into title.elements

    static constexpr quintptr LEVEL_SCENE   = 0;
    static constexpr quintptr LEVEL_ELEMENT = 2;

    static quintptr makeId(quintptr level, int ei)
    {
        return (level << 60) | quintptr(quint32(ei));
    }
    static quintptr levelOf(quintptr id) { return (id >> 60) & 0x3; }
    static int eiOf(quintptr id) { return int(qint32(id & 0xFFFFFFFF)); }
};
