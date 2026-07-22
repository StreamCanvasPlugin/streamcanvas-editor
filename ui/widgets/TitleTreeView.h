#pragma once

#include <QTreeView>

#include "model/EditorTitle.h"

class TitleDocument;
class EditorTitle;
class TitleTreeModel;

class TitleTreeView : public QTreeView {
    Q_OBJECT
public:
    explicit TitleTreeView(TitleDocument* doc, EditorTitle* editorTitle, QWidget* parent = nullptr);

    void setResetsSuppressed(bool suppressed);

protected:
    bool event(QEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onViewSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void onEditorSelectionChanged(SelectionId id);

private:
    TitleDocument* m_doc;
    EditorTitle* m_editorTitle;
    TitleTreeModel* m_model;
    bool m_syncingSelection{false};
};
