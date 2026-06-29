#include "EditorTitle.h"
#include "TitleDocument.h"

EditorTitle::EditorTitle(TitleDocument* doc, QObject* parent) : QObject(parent), m_doc(doc)
{
    connect(m_doc, &TitleDocument::documentChanged, this, &EditorTitle::validateSelection);
}

void EditorTitle::validateSelection()
{
    const SelectionId& cur = m_selection;

    if (cur.level == SelectionId::Level::Element) {
        const Title& t = m_doc->title();
        if (cur.elementIndex < 1 || cur.elementIndex >= (int)t.elements.size())
            setSelection(SelectionId{});
    }
}

void EditorTitle::setSelection(const SelectionId& id)
{
    if (m_selection == id)
        return;
    m_selection = id;
    emit selectionChanged(m_selection);
}
