#include "EditorTitle.h"
#include "TitleDocument.h"

EditorTitle::EditorTitle(TitleDocument* doc, QObject* parent) : QObject(parent), m_doc(doc)
{
    connect(m_doc, &TitleDocument::documentChanged, this, &EditorTitle::validateSelection);
}

const IElement* EditorTitle::elementForSelection(const SelectionId& id) const
{
    if (id.level != SelectionId::Level::Element)
        return nullptr;
    const Title& t = m_doc->title();
    if (id.elementIndex < 1 || id.elementIndex >= (int)t.elements.size())
        return nullptr;
    return t.elements[id.elementIndex].get();
}

void EditorTitle::validateSelection()
{
    if (m_selection.level != SelectionId::Level::Element)
        return;

    const Title& t = m_doc->title();

    int foundIndex = -1;
    if (m_selectedElement) {
        for (int i = 1; i < (int)t.elements.size(); ++i) {
            if (t.elements[i].get() == m_selectedElement) {
                foundIndex = i;
                break;
            }
        }
    }

    if (foundIndex >= 0) {
        std::string curId = m_selectedElement->GetId();
        if (foundIndex != m_selection.elementIndex) {
            m_selection.elementIndex = foundIndex;
            m_selectedElementId = curId;
            emit selectionChanged(m_selection);
        } else if (curId != m_selectedElementId) {
            m_selectedElementId = curId;
            emit selectionChanged(m_selection);
        }
    } else {
        m_selection = SelectionId{};
        m_selectedElement = nullptr;
        m_selectedElementId.clear();
        emit selectionChanged(m_selection);
    }
}

void EditorTitle::setSelection(const SelectionId& id)
{
    if (m_selection == id)
        return;
    m_selection = id;
    m_selectedElement = elementForSelection(id);
    m_selectedElementId = m_selectedElement ? m_selectedElement->GetId() : std::string();
    emit selectionChanged(m_selection);
}
