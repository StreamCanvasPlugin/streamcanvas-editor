#pragma once

#include <functional>
#include <string>

#include <QColor>
#include <QList>
#include <QString>
#include <QUndoCommand>

#include <nlohmann/json.hpp>

#include "engine/title.h"
#include "engine/visual_element.h"

#include "TitleDocument.h"

// ── SetTitleDimensionsCmd ─────────────────────────────────────────────────────

class SetTitleDimensionsCmd : public QUndoCommand {
public:
    SetTitleDimensionsCmd(TitleDocument* doc, int width, int height,
                          QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    TitleDocument* m_doc;
    int m_beforeW, m_beforeH;
    int m_afterW, m_afterH;
};

// ── SetTitleNameCmd ───────────────────────────────────────────────────────────

class SetTitleNameCmd : public QUndoCommand {
public:
    SetTitleNameCmd(TitleDocument* doc, const QString& after, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    TitleDocument* m_doc;
    QString m_before;
    QString m_after;
};

// ── SetBrandColorsCmd ─────────────────────────────────────────────────────────

class SetBrandColorsCmd : public QUndoCommand {
public:
    SetBrandColorsCmd(TitleDocument* doc, QList<QColor> after, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    TitleDocument* m_doc;
    QList<QColor> m_before;
    QList<QColor> m_after;
};

// ── ElemMergeTag — unique ids for collapsing rapid spinbox edits ──────────────

namespace ElemMergeTag {
enum : int {
    ZOrder   = 1000,
    X        = 1001,
    Y        = 1002,
    W        = 1003,
    H        = 1004,
    Rotation = 1005,
    Opacity  = 1006,
    StrokeW  = 1007,
    ShearX      = 1008,
    ShearY      = 1009,
    LineSpacing = 1010,
    CharSpacing = 1011,
};
}

// ── SetElementFieldCmd ────────────────────────────────────────────────────────
// Generic command for a single scalar field on a VisualElement.
// Accessor returns a T& from the VisualElement (direct field or cast to subclass).
// T must be copyable.

template <typename T>
class SetElementFieldCmd : public QUndoCommand {
public:
    SetElementFieldCmd(TitleDocument* doc, std::string ei, T after,
                       std::function<T&(VisualElement&)> accessor, const QString& fieldName,
                       int mergeTag = -1, QUndoCommand* parent = nullptr)
        : QUndoCommand(parent), m_doc(doc), m_ei(std::move(ei)), m_after(std::move(after)),
          m_accessor(std::move(accessor)), m_mergeTag(mergeTag)
    {
        setText(QString("Set element %1").arg(fieldName));
        try {
            m_before = m_accessor(m_doc->getElement(m_ei));
        } catch (const std::runtime_error&) {}
    }

    void undo() override
    {
        m_doc->applyMutation([&](Title&) {
            try { m_accessor(m_doc->getElement(m_ei)) = m_before; }
            catch (const std::runtime_error&) {}
        });
    }

    void redo() override
    {
        m_doc->applyMutation([&](Title&) {
            try { m_accessor(m_doc->getElement(m_ei)) = m_after; }
            catch (const std::runtime_error&) {}
        });
    }

    int id() const override { return m_mergeTag; }

    bool mergeWith(const QUndoCommand* other) override
    {
        if (other->id() != id() || id() == -1) return false;
        const auto* o = static_cast<const SetElementFieldCmd<T>*>(other);
        if (o->m_ei != m_ei) return false;
        m_after = o->m_after;
        return true;
    }

private:
    TitleDocument* m_doc;
    std::string    m_ei;
    T              m_before;
    T              m_after;
    std::function<T&(VisualElement&)> m_accessor;
    int            m_mergeTag;
};

// ── SetElementBoundsCmd ───────────────────────────────────────────────────────
// Stores a full Rectangle; merges when the same element and same merge tag.

class SetElementBoundsCmd : public QUndoCommand {
public:
    SetElementBoundsCmd(TitleDocument* doc, std::string ei, Rectangle after,
                        int mergeTag = -1, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
    int  id() const override;
    bool mergeWith(const QUndoCommand* other) override;

private:
    TitleDocument* m_doc;
    std::string    m_ei;
    Rectangle      m_before;
    Rectangle      m_after;
    int            m_mergeTag;
};

// ── SetElementRotationCmd ─────────────────────────────────────────────────────

class SetElementRotationCmd : public QUndoCommand {
public:
    SetElementRotationCmd(TitleDocument* doc, std::string ei, float after,
                          QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
    int  id() const override;
    bool mergeWith(const QUndoCommand* other) override;

private:
    TitleDocument* m_doc;
    std::string    m_ei;
    float          m_before;
    float          m_after;
};

// ── SetElementShearCmd ────────────────────────────────────────────────────────
// Stores shearX and shearY together; separate merge tags for X vs Y.

class SetElementShearCmd : public QUndoCommand {
public:
    SetElementShearCmd(TitleDocument* doc, std::string ei,
                       float shearX, float shearY, int mergeTag,
                       QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
    int  id() const override;
    bool mergeWith(const QUndoCommand* other) override;

private:
    TitleDocument* m_doc;
    std::string    m_ei;
    float m_beforeX, m_beforeY;
    float m_afterX,  m_afterY;
    int   m_mergeTag;
};

// ── SetElementPaintCmd ────────────────────────────────────────────────────────

class SetElementPaintCmd : public QUndoCommand {
public:
    enum class Target { Fill, Stroke };

    SetElementPaintCmd(TitleDocument* doc, std::string ei, Target target,
                       const Paint& after, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    TitleDocument* m_doc;
    std::string    m_ei;
    Target         m_target;
    Paint          m_before;
    Paint          m_after;

    Paint& paintRef(VisualElement& el) const;
};

// ── SetElementAnimCmd ─────────────────────────────────────────────────────────

class SetElementAnimCmd : public QUndoCommand {
public:
    enum class Target { AnimIn, AnimOut, DataAnimIn, DataAnimOut };

    SetElementAnimCmd(TitleDocument* doc, std::string ei, Target target,
                      const AnimationDef& after, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    TitleDocument* m_doc;
    std::string    m_ei;
    Target         m_target;
    AnimationDef   m_before;
    AnimationDef   m_after;

    AnimationDef& animRef(VisualElement& el) const;
};

// ── SetChildrenPaddingCmd ─────────────────────────────────────────────────────

class SetChildrenPaddingCmd : public QUndoCommand {
public:
    SetChildrenPaddingCmd(TitleDocument* doc, std::string ei, float top,
                          float right, float bottom, float left, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    TitleDocument* m_doc;
    std::string    m_ei;
    float m_before[4];
    float m_after[4];
};

// ── SetElementShadowCmd ───────────────────────────────────────────────────────

class SetElementShadowCmd : public QUndoCommand {
public:
    SetElementShadowCmd(TitleDocument* doc, std::string ei,
                        const VisualElement::DropShadow& after, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    TitleDocument*          m_doc;
    std::string             m_ei;
    VisualElement::DropShadow m_before, m_after;
};

// ── SetCornerRadiusCmd ────────────────────────────────────────────────────────

class SetCornerRadiusCmd : public QUndoCommand {
public:
    SetCornerRadiusCmd(TitleDocument* doc, std::string ei, float tl, float tr,
                       float br, float bl, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    TitleDocument* m_doc;
    std::string    m_ei;
    float m_before[4];
    float m_after[4];
};

// ── Structural: Add / Remove Element ─────────────────────────────────────────

class AddElementCmd : public QUndoCommand {
public:
    AddElementCmd(TitleDocument* doc, nlohmann::json elementJson,
                  QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    TitleDocument* m_doc;
    nlohmann::json m_json;
    std::string    m_id;
};

class RemoveElementCmd : public QUndoCommand {
public:
    RemoveElementCmd(TitleDocument* doc, std::string ei, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    TitleDocument* m_doc;
    std::string    m_ei;
    nlohmann::json m_snapshot;
    int            m_savedPosition;
};

// ── Structural: Set Element Parent ───────────────────────────────────────────

class SetElementParentCmd : public QUndoCommand {
public:
    SetElementParentCmd(TitleDocument* doc, std::string elementId,
                        std::string newParentId,   // empty string = root
                        QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    TitleDocument* m_doc;
    std::string    m_elementId;
    std::string    m_oldParentId;
    std::string    m_newParentId;

    void doReparent(const std::string& fromId, const std::string& toId);
};

// ── Structural: Move Element ──────────────────────────────────────────────────

class MoveElementCmd : public QUndoCommand {
public:
    MoveElementCmd(TitleDocument* doc, int fromIndex, int toIndex, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    TitleDocument* m_doc;
    int m_from;
    int m_to;

    void doMove(int from, int to);
};
