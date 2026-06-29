#include "UndoCommands.h"

#include <algorithm>
#include <stdexcept>

#include "engine/animation.h"
#include "engine/element_image.h"
#include "engine/element_qr.h"
#include "engine/element_rectangle.h"
#include "engine/element_text.h"
#include "engine/spatial.h"

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// insertElementFromJson — re-parse a single element JSON blob into the title.
// JSON x,y are LOCAL coords; converts to world before AddChild so SetParent
// preserves the correct local position.
// ─────────────────────────────────────────────────────────────────────────────

static void insertElementFromJson(Title& t, const json& ej)
{
    const std::string type = ej.value("type", "rectangle");
    const std::string id   = ej.value("id", "");

    std::unique_ptr<VisualElement> el;
    if (type == "text")      el = std::make_unique<TextElement>();
    else if (type == "image")     el = std::make_unique<ImageElement>();
    else if (type == "qr_code")   el = std::make_unique<QrElement>();
    else                          el = std::make_unique<RectangleElement>();

    el->SetId(id);

    // Bounds (local coords from JSON)
    double x = ej.value("x", 0.0),  y = ej.value("y", 0.0);
    double w = ej.value("w", 100.0), h = ej.value("h", 100.0);

    el->zOrder      = ej.value("z_order", 0);
    el->opacity     = static_cast<float>(ej.value("opacity", 1.0));
    el->strokeWidth = static_cast<float>(ej.value("stroke_width", 0.0));

    if (ej.contains("corner_radius")) {
        const auto& cr = ej["corner_radius"];
        if (cr.is_number()) {
            float v = cr.get<float>();
            el->cornerRadius[0] = el->cornerRadius[1] = el->cornerRadius[2] = el->cornerRadius[3] = v;
        } else if (cr.is_array() && cr.size() == 4) {
            for (int k = 0; k < 4; ++k) el->cornerRadius[k] = cr[k].get<float>();
        }
    }

    if (ej.value("fit_to_children", false)) {
        el->fitToChildren = true;
        if (ej.contains("children_padding") && ej["children_padding"].is_array() &&
            ej["children_padding"].size() == 4) {
            for (int k = 0; k < 4; ++k)
                el->childrenPadding[k] = ej["children_padding"][k].get<float>();
        }
    }

    if (ej.contains("shadow") && ej["shadow"].is_object()) {
        const auto& sh = ej["shadow"];
        el->shadow.enabled = sh.value("enabled", false);
        el->shadow.offsetX = sh.value("offset_x", 4.0);
        el->shadow.offsetY = sh.value("offset_y", 4.0);
        el->shadow.blur    = sh.value("blur", 8.0);
        if (sh.contains("color") && sh["color"].is_array() && sh["color"].size() == 4) {
            for (int k = 0; k < 4; ++k) el->shadow.color[k] = sh["color"][k].get<float>();
        }
    }

    // Type-specific fields
    if (auto* te = dynamic_cast<TextElement*>(el.get())) {
        te->text                = ej.value("text", "");
        te->font.family         = ej.value("font_family", "");
        te->font.size           = static_cast<float>(ej.value("font_size", 36.0));
        te->font.isItalic       = ej.value("font_italic", false);
        te->font.isUnderline    = ej.value("font_underline", false);
        te->font.isStrikethrough= ej.value("font_strikethrough", false);
    } else if (auto* ie = dynamic_cast<ImageElement*>(el.get())) {
        ie->imagePath      = ej.value("image_path", "");
        const std::string sm = ej.value("scale_mode", "stretch");
        if      (sm == "contain")    ie->imageScaleMode = ScaleMode::Contain;
        else if (sm == "cover")      ie->imageScaleMode = ScaleMode::Cover;
        else if (sm == "fit_width")  ie->imageScaleMode = ScaleMode::FitWidth;
        else if (sm == "fit_height") ie->imageScaleMode = ScaleMode::FitHeight;
        else if (sm == "none")       ie->imageScaleMode = ScaleMode::None;
        else                         ie->imageScaleMode = ScaleMode::Stretch;
    } else if (auto* qe = dynamic_cast<QrElement*>(el.get())) {
        qe->text = ej.value("text", "");
    }

    // Find parent element (by id); default to root
    const std::string parentId = ej.value("parent", "");
    IElement* parentEl = t.GetRoot();
    if (!parentId.empty()) {
        for (auto& pe : t.elements) {
            if (pe->GetId() == parentId) {
                parentEl = pe.get();
                break;
            }
        }
    }

    // Convert local coords to world coords so SetParent in AddChild is correct
    if (parentEl && parentEl != t.GetRoot()) {
        Point pw = parentEl->GetGlobalPosition();
        x += pw.x;
        y += pw.y;
    }
    el->SetBounds({x, y, w, h});

    // Rotation and shear must be set before AddChild
    el->SetRotation(static_cast<float>(ej.value("rotation", 0.0)));
    el->SetShearX(static_cast<float>(ej.value("shear_x", 0.0)));
    el->SetShearY(static_cast<float>(ej.value("shear_y", 0.0)));

    VisualElement* rawEl = el.get();
    t.elements.push_back(std::move(el));

    if (parentEl)
        parentEl->AddChild(rawEl);
}

// ─────────────────────────────────────────────────────────────────────────────
// SetTitleDimensionsCmd
// ─────────────────────────────────────────────────────────────────────────────

SetTitleDimensionsCmd::SetTitleDimensionsCmd(TitleDocument* doc, int width, int height,
                                             QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_beforeW(doc->title().width),
      m_beforeH(doc->title().height), m_afterW(width), m_afterH(height)
{
    setText("Set title dimensions");
}

void SetTitleDimensionsCmd::undo()
{
    m_doc->applyMutation([&](Title& t) { t.width = m_beforeW; t.height = m_beforeH; });
}

void SetTitleDimensionsCmd::redo()
{
    m_doc->applyMutation([&](Title& t) { t.width = m_afterW; t.height = m_afterH; });
}

// ─────────────────────────────────────────────────────────────────────────────
// SetTitleNameCmd
// ─────────────────────────────────────────────────────────────────────────────

SetTitleNameCmd::SetTitleNameCmd(TitleDocument* doc, const QString& after, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_before(doc->titleName()), m_after(after)
{
    setText("Set title name");
}

void SetTitleNameCmd::undo() { m_doc->setTitleName(m_before); }
void SetTitleNameCmd::redo() { m_doc->setTitleName(m_after); }

// ─────────────────────────────────────────────────────────────────────────────
// SetBrandColorsCmd
// ─────────────────────────────────────────────────────────────────────────────

SetBrandColorsCmd::SetBrandColorsCmd(TitleDocument* doc, QList<QColor> after, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_before(doc->brandColors()), m_after(std::move(after))
{
    setText("Set brand colors");
}

void SetBrandColorsCmd::undo() { m_doc->setBrandColors(m_before); }
void SetBrandColorsCmd::redo() { m_doc->setBrandColors(m_after); }

// ─────────────────────────────────────────────────────────────────────────────
// SetElementBoundsCmd
// ─────────────────────────────────────────────────────────────────────────────

SetElementBoundsCmd::SetElementBoundsCmd(TitleDocument* doc, std::string ei, Rectangle after,
                                         int mergeTag, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_ei(std::move(ei)), m_after(after), m_mergeTag(mergeTag)
{
    setText("Set element bounds");
    try { m_before = m_doc->getElement(m_ei).GetBounds(); } catch (...) { m_before = after; }
}

void SetElementBoundsCmd::undo()
{
    m_doc->applyMutation([&](Title&) {
        try { m_doc->getElement(m_ei).SetBounds(m_before); } catch (...) {}
    });
}

void SetElementBoundsCmd::redo()
{
    m_doc->applyMutation([&](Title&) {
        try { m_doc->getElement(m_ei).SetBounds(m_after); } catch (...) {}
    });
}

int SetElementBoundsCmd::id() const { return m_mergeTag; }

bool SetElementBoundsCmd::mergeWith(const QUndoCommand* other)
{
    if (other->id() != id() || id() == -1) return false;
    const auto* o = static_cast<const SetElementBoundsCmd*>(other);
    if (o->m_ei != m_ei) return false;
    m_after = o->m_after;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SetElementRotationCmd
// ─────────────────────────────────────────────────────────────────────────────

SetElementRotationCmd::SetElementRotationCmd(TitleDocument* doc, std::string ei, float after,
                                             QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_ei(std::move(ei)), m_after(after)
{
    setText("Set rotation");
    try { m_before = m_doc->getElement(m_ei).GetRotation(); } catch (...) { m_before = after; }
}

void SetElementRotationCmd::undo()
{
    m_doc->applyMutation([&](Title&) {
        try { m_doc->getElement(m_ei).SetRotation(m_before); } catch (...) {}
    });
}

void SetElementRotationCmd::redo()
{
    m_doc->applyMutation([&](Title&) {
        try { m_doc->getElement(m_ei).SetRotation(m_after); } catch (...) {}
    });
}

int  SetElementRotationCmd::id() const { return ElemMergeTag::Rotation; }

bool SetElementRotationCmd::mergeWith(const QUndoCommand* other)
{
    if (other->id() != id()) return false;
    const auto* o = static_cast<const SetElementRotationCmd*>(other);
    if (o->m_ei != m_ei) return false;
    m_after = o->m_after;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SetElementShearCmd
// ─────────────────────────────────────────────────────────────────────────────

SetElementShearCmd::SetElementShearCmd(TitleDocument* doc, std::string ei,
                                       float shearX, float shearY, int mergeTag,
                                       QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_ei(std::move(ei)),
      m_afterX(shearX), m_afterY(shearY), m_mergeTag(mergeTag)
{
    setText("Set shear");
    try {
        m_beforeX = m_doc->getElement(m_ei).GetShearX();
        m_beforeY = m_doc->getElement(m_ei).GetShearY();
    } catch (...) { m_beforeX = shearX; m_beforeY = shearY; }
}

void SetElementShearCmd::undo()
{
    m_doc->applyMutation([&](Title&) {
        try {
            auto& e = m_doc->getElement(m_ei);
            e.SetShearX(m_beforeX);
            e.SetShearY(m_beforeY);
        } catch (...) {}
    });
}

void SetElementShearCmd::redo()
{
    m_doc->applyMutation([&](Title&) {
        try {
            auto& e = m_doc->getElement(m_ei);
            e.SetShearX(m_afterX);
            e.SetShearY(m_afterY);
        } catch (...) {}
    });
}

int  SetElementShearCmd::id() const { return m_mergeTag; }

bool SetElementShearCmd::mergeWith(const QUndoCommand* other)
{
    if (other->id() != id() || id() == -1) return false;
    const auto* o = static_cast<const SetElementShearCmd*>(other);
    if (o->m_ei != m_ei) return false;
    m_afterX = o->m_afterX;
    m_afterY = o->m_afterY;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SetElementPaintCmd
// ─────────────────────────────────────────────────────────────────────────────

SetElementPaintCmd::SetElementPaintCmd(TitleDocument* doc, std::string ei,
                                       Target target, const Paint& after, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_ei(std::move(ei)), m_target(target), m_after(after)
{
    setText(target == Target::Fill ? "Set fill" : "Set stroke");
    try { m_before = paintRef(m_doc->getElement(m_ei)); } catch (...) {}
}

Paint& SetElementPaintCmd::paintRef(VisualElement& el) const
{
    return (m_target == Target::Fill) ? el.fill : el.stroke;
}

void SetElementPaintCmd::undo()
{
    m_doc->applyMutation([&](Title&) {
        try { paintRef(m_doc->getElement(m_ei)) = m_before; } catch (...) {}
    });
}

void SetElementPaintCmd::redo()
{
    m_doc->applyMutation([&](Title&) {
        try { paintRef(m_doc->getElement(m_ei)) = m_after; } catch (...) {}
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// SetElementAnimCmd
// ─────────────────────────────────────────────────────────────────────────────

SetElementAnimCmd::SetElementAnimCmd(TitleDocument* doc, std::string ei,
                                     Target target, const AnimationDef& after, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_ei(std::move(ei)), m_target(target), m_after(after)
{
    setText(target == Target::AnimIn ? "Set anim in" : "Set anim out");
    try { m_before = animRef(m_doc->getElement(m_ei)); } catch (...) {}
}

AnimationDef& SetElementAnimCmd::animRef(VisualElement& el) const
{
    return (m_target == Target::AnimIn) ? el.inAnimation : el.outAnimation;
}

void SetElementAnimCmd::undo()
{
    m_doc->applyMutation([&](Title&) {
        try { animRef(m_doc->getElement(m_ei)) = m_before; } catch (...) {}
    });
}

void SetElementAnimCmd::redo()
{
    m_doc->applyMutation([&](Title&) {
        try { animRef(m_doc->getElement(m_ei)) = m_after; } catch (...) {}
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// SetChildrenPaddingCmd
// ─────────────────────────────────────────────────────────────────────────────

SetChildrenPaddingCmd::SetChildrenPaddingCmd(TitleDocument* doc, std::string ei,
                                             float top, float right, float bottom, float left,
                                             QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_ei(std::move(ei))
{
    setText("Set children padding");
    m_after[0] = top; m_after[1] = right; m_after[2] = bottom; m_after[3] = left;
    try {
        const VisualElement& el = m_doc->getElement(m_ei);
        for (int i = 0; i < 4; ++i) m_before[i] = el.childrenPadding[i];
    } catch (...) {
        for (int i = 0; i < 4; ++i) m_before[i] = 0.f;
    }
}

void SetChildrenPaddingCmd::undo()
{
    m_doc->applyMutation([&](Title&) {
        try {
            VisualElement& el = m_doc->getElement(m_ei);
            for (int i = 0; i < 4; ++i) el.childrenPadding[i] = m_before[i];
        } catch (...) {}
    });
}

void SetChildrenPaddingCmd::redo()
{
    m_doc->applyMutation([&](Title&) {
        try {
            VisualElement& el = m_doc->getElement(m_ei);
            for (int i = 0; i < 4; ++i) el.childrenPadding[i] = m_after[i];
        } catch (...) {}
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// SetElementShadowCmd
// ─────────────────────────────────────────────────────────────────────────────

SetElementShadowCmd::SetElementShadowCmd(TitleDocument* doc, std::string ei,
                                         const VisualElement::DropShadow& after, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_ei(std::move(ei)), m_after(after)
{
    setText("Set shadow");
    try { m_before = m_doc->getElement(m_ei).shadow; } catch (...) {}
}

void SetElementShadowCmd::undo()
{
    m_doc->applyMutation([&](Title&) {
        try { m_doc->getElement(m_ei).shadow = m_before; } catch (...) {}
    });
}

void SetElementShadowCmd::redo()
{
    m_doc->applyMutation([&](Title&) {
        try { m_doc->getElement(m_ei).shadow = m_after; } catch (...) {}
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// SetCornerRadiusCmd
// ─────────────────────────────────────────────────────────────────────────────

SetCornerRadiusCmd::SetCornerRadiusCmd(TitleDocument* doc, std::string ei,
                                       float tl, float tr, float br, float bl, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_ei(std::move(ei))
{
    setText("Set corner radius");
    m_after[0] = tl; m_after[1] = tr; m_after[2] = br; m_after[3] = bl;
    try {
        const VisualElement& el = m_doc->getElement(m_ei);
        for (int i = 0; i < 4; ++i) m_before[i] = el.cornerRadius[i];
    } catch (...) {
        for (int i = 0; i < 4; ++i) m_before[i] = 0.f;
    }
}

void SetCornerRadiusCmd::undo()
{
    m_doc->applyMutation([&](Title&) {
        try {
            VisualElement& el = m_doc->getElement(m_ei);
            for (int i = 0; i < 4; ++i) el.cornerRadius[i] = m_before[i];
        } catch (...) {}
    });
}

void SetCornerRadiusCmd::redo()
{
    m_doc->applyMutation([&](Title&) {
        try {
            VisualElement& el = m_doc->getElement(m_ei);
            for (int i = 0; i < 4; ++i) el.cornerRadius[i] = m_after[i];
        } catch (...) {}
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// AddElementCmd
// ─────────────────────────────────────────────────────────────────────────────

AddElementCmd::AddElementCmd(TitleDocument* doc, json elementJson, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_json(std::move(elementJson)),
      m_id(m_json.value("id", ""))
{
    setText("Add element");
}

void AddElementCmd::undo()
{
    m_doc->applyMutation([&](Title& t) {
        auto it = std::find_if(t.elements.begin(), t.elements.end(),
                               [&](const auto& e) { return e->GetId() == m_id; });
        if (it != t.elements.end()) {
            IElement* parent = (*it)->GetParent();
            if (parent) parent->RemoveChild(it->get());
            t.elements.erase(it);
        }
    });
}

void AddElementCmd::redo()
{
    m_doc->applyMutation([&](Title& t) { insertElementFromJson(t, m_json); });
}

// ─────────────────────────────────────────────────────────────────────────────
// RemoveElementCmd
// ─────────────────────────────────────────────────────────────────────────────

RemoveElementCmd::RemoveElementCmd(TitleDocument* doc, std::string ei, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_ei(std::move(ei)), m_savedPosition(-1)
{
    setText("Remove element");

    Title& t = m_doc->title();
    for (int i = 1; i < (int)t.elements.size(); ++i) {
        if (t.elements[i]->GetId() == m_ei) {
            m_savedPosition = i;
            auto* ve = dynamic_cast<VisualElement*>(t.elements[i].get());
            if (ve) m_snapshot = TitleDocument::elementToJson(*ve);
            break;
        }
    }
}

void RemoveElementCmd::undo()
{
    m_doc->applyMutation([&](Title& t) {
        insertElementFromJson(t, m_snapshot);
        // Restore element to original position in the vector
        if (m_savedPosition >= 0 && m_savedPosition < (int)t.elements.size()) {
            int last = static_cast<int>(t.elements.size()) - 1;
            if (last > m_savedPosition) {
                std::rotate(t.elements.begin() + m_savedPosition,
                            t.elements.end() - 1,
                            t.elements.end());
            }
        }
    });
    // Restore mask ref if present
    const std::string maskId = m_snapshot.value("mask", "");
    if (!maskId.empty())
        m_doc->setElementMaskRef(m_ei, maskId);
}

void RemoveElementCmd::redo()
{
    m_doc->applyMutation([&](Title& t) {
        auto it = std::find_if(t.elements.begin(), t.elements.end(),
                               [&](const auto& e) { return e->GetId() == m_ei; });
        if (it != t.elements.end()) {
            IElement* parent = (*it)->GetParent();
            if (parent) parent->RemoveChild(it->get());
            t.elements.erase(it);
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// MoveElementCmd
// ─────────────────────────────────────────────────────────────────────────────

MoveElementCmd::MoveElementCmd(TitleDocument* doc, int fromIndex, int toIndex, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_from(fromIndex), m_to(toIndex)
{
    setText("Move element");
}

void MoveElementCmd::doMove(int from, int to)
{
    m_doc->applyMutation([&](Title& t) {
        int n = static_cast<int>(t.elements.size());
        if (from < 1 || from >= n || to < 1 || to >= n || from == to) return;
        if (from < to)
            std::rotate(t.elements.begin() + from, t.elements.begin() + from + 1,
                        t.elements.begin() + to + 1);
        else
            std::rotate(t.elements.begin() + to, t.elements.begin() + from,
                        t.elements.begin() + from + 1);
    });
}

void MoveElementCmd::undo() { doMove(m_to, m_from); }
void MoveElementCmd::redo() { doMove(m_from, m_to); }
