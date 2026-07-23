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
    return elementForIndex(id.elementIndex);
}

const IElement* EditorTitle::elementForIndex(int elementIndex) const
{
    const Title& t = m_doc->title();
    if (elementIndex < 1 || elementIndex >= (int)t.elements.size())
        return nullptr;
    return t.elements[elementIndex].get();
}

void EditorTitle::validateSelection()
{
    if (m_selection.level != SelectionId::Level::Element) {
        m_selectedElements.clear();
        return;
    }

    const Title& t = m_doc->title();

    // Prune m_selectedElements to only pointers still present in elements[1..].
    bool setChanged = false;
    {
        std::vector<const IElement*> survivors;
        survivors.reserve(m_selectedElements.size());
        for (const IElement* p : m_selectedElements) {
            bool found = false;
            for (int i = 1; i < (int)t.elements.size(); ++i) {
                if (t.elements[i].get() == p) {
                    found = true;
                    break;
                }
            }
            if (found)
                survivors.push_back(p);
            else
                setChanged = true;
        }
        m_selectedElements = std::move(survivors);
    }

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
            setChanged = true;
        } else if (curId != m_selectedElementId) {
            m_selectedElementId = curId;
            emit selectionChanged(m_selection);
            setChanged = true;
        }
    } else if (!m_selectedElements.empty()) {
        // Active element is gone but survivors remain: promote the first survivor.
        const IElement* promoted = m_selectedElements.front();
        int promotedIndex = -1;
        for (int i = 1; i < (int)t.elements.size(); ++i) {
            if (t.elements[i].get() == promoted) {
                promotedIndex = i;
                break;
            }
        }
        m_selection = SelectionId{SelectionId::Level::Element, promotedIndex};
        m_selectedElement = promoted;
        m_selectedElementId = promoted->GetId();
        emit selectionChanged(m_selection);
        setChanged = true;
    } else {
        m_selection = SelectionId{};
        m_selectedElement = nullptr;
        m_selectedElementId.clear();
        emit selectionChanged(m_selection);
        setChanged = true;
    }

    if (setChanged)
        emit selectionSetChanged();
}

void EditorTitle::setSelection(const SelectionId& id)
{
    if (m_selection == id && selectionCount() <= 1)
        return;
    m_selection = id;
    m_selectedElement = elementForSelection(id);
    m_selectedElementId = m_selectedElement ? m_selectedElement->GetId() : std::string();
    if (m_selectedElement)
        m_selectedElements = {m_selectedElement};
    else
        m_selectedElements.clear();
    emit selectionChanged(m_selection);
    emit selectionSetChanged();
}

std::vector<int> EditorTitle::selectedIndices() const
{
    std::vector<int> result;
    const Title& t = m_doc->title();
    for (const IElement* p : m_selectedElements) {
        for (int i = 1; i < (int)t.elements.size(); ++i) {
            if (t.elements[i].get() == p) {
                result.push_back(i);
                break;
            }
        }
    }
    return result;
}

bool EditorTitle::isSelected(int elementIndex) const
{
    const IElement* el = elementForIndex(elementIndex);
    if (!el)
        return false;
    for (const IElement* p : m_selectedElements) {
        if (p == el)
            return true;
    }
    return false;
}

int EditorTitle::selectionCount() const
{
    return (int)m_selectedElements.size();
}

void EditorTitle::setMultiSelection(const std::vector<int>& indices, int activeIndex)
{
    std::vector<const IElement*> resolved;
    resolved.reserve(indices.size());
    for (int idx : indices) {
        const IElement* el = elementForIndex(idx);
        if (!el)
            continue;
        bool alreadyPresent = false;
        for (const IElement* p : resolved) {
            if (p == el) {
                alreadyPresent = true;
                break;
            }
        }
        if (!alreadyPresent)
            resolved.push_back(el);
    }
    m_selectedElements = std::move(resolved);

    const IElement* activeEl = elementForIndex(activeIndex);
    bool activeValid = false;
    if (activeEl) {
        for (const IElement* p : m_selectedElements) {
            if (p == activeEl) {
                activeValid = true;
                break;
            }
        }
    }

    if (activeValid) {
        m_selection = SelectionId{SelectionId::Level::Element, activeIndex};
        m_selectedElement = activeEl;
        m_selectedElementId = activeEl->GetId();
    } else if (!m_selectedElements.empty()) {
        m_selectedElement = m_selectedElements.front();
        m_selection = SelectionId{SelectionId::Level::Element, -1};
        const Title& t = m_doc->title();
        for (int i = 1; i < (int)t.elements.size(); ++i) {
            if (t.elements[i].get() == m_selectedElement) {
                m_selection.elementIndex = i;
                break;
            }
        }
        m_selectedElementId = m_selectedElement->GetId();
    } else {
        m_selection = SelectionId{};
        m_selectedElement = nullptr;
        m_selectedElementId.clear();
    }

    emit selectionChanged(m_selection);
    emit selectionSetChanged();
}

void EditorTitle::toggleSelection(int elementIndex)
{
    const IElement* el = elementForIndex(elementIndex);
    if (!el)
        return;

    auto it = m_selectedElements.begin();
    for (; it != m_selectedElements.end(); ++it) {
        if (*it == el)
            break;
    }

    if (it != m_selectedElements.end()) {
        bool wasActive = (m_selectedElement == el);
        m_selectedElements.erase(it);
        if (wasActive) {
            if (!m_selectedElements.empty()) {
                m_selectedElement = m_selectedElements.front();
                const Title& t = m_doc->title();
                int newIndex = -1;
                for (int i = 1; i < (int)t.elements.size(); ++i) {
                    if (t.elements[i].get() == m_selectedElement) {
                        newIndex = i;
                        break;
                    }
                }
                m_selection = SelectionId{SelectionId::Level::Element, newIndex};
                m_selectedElementId = m_selectedElement->GetId();
            } else {
                m_selection = SelectionId{};
                m_selectedElement = nullptr;
                m_selectedElementId.clear();
            }
        }
    } else {
        m_selectedElements.push_back(el);
        m_selectedElement = el;
        m_selection = SelectionId{SelectionId::Level::Element, elementIndex};
        m_selectedElementId = el->GetId();
    }

    emit selectionChanged(m_selection);
    emit selectionSetChanged();
}

void EditorTitle::addToSelection(int elementIndex)
{
    const IElement* el = elementForIndex(elementIndex);
    if (!el)
        return;

    bool alreadyPresent = false;
    for (const IElement* p : m_selectedElements) {
        if (p == el) {
            alreadyPresent = true;
            break;
        }
    }
    if (!alreadyPresent)
        m_selectedElements.push_back(el);

    m_selectedElement = el;
    m_selection = SelectionId{SelectionId::Level::Element, elementIndex};
    m_selectedElementId = el->GetId();

    emit selectionChanged(m_selection);
    emit selectionSetChanged();
}
