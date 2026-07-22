#pragma once

#include <string>

#include <QObject>

class TitleDocument;
class IElement;

struct SelectionId {
    enum class Level { None, Title, Element };

    Level level{Level::None};
    int elementIndex{-1};  // direct index into title.elements (1-based; [0] is root)

    bool operator==(const SelectionId&) const = default;
};

class EditorTitle : public QObject {
    Q_OBJECT
public:
    explicit EditorTitle(TitleDocument* doc, QObject* parent = nullptr);

    SelectionId selection() const
    {
        return m_selection;
    }
    void setSelection(const SelectionId& id);

signals:
    void selectionChanged(SelectionId id);

private slots:
    void validateSelection();

private:
    const IElement* elementForSelection(const SelectionId& id) const;

    TitleDocument* m_doc;
    SelectionId m_selection;
    const IElement* m_selectedElement = nullptr;
    std::string m_selectedElementId;
};
