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

signals:
    void duplicateRequested();
    void cutRequested();
    void copyRequested();
    void pasteRequested();
    void pasteInPlaceRequested();
    // Requests a new element be added. `extraJson` is the per-type extra
    // fields (everything except id/type/x/y/z_order) serialised as JSON.
    void addElementRequested(const QString& prefix, const QString& type, const QString& extraJson);

protected:
    bool event(QEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onViewSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void onEditorSelectionSetChanged();

private:
    TitleDocument* m_doc;
    EditorTitle* m_editorTitle;
    TitleTreeModel* m_model;
    bool m_syncingSelection{false};
};
