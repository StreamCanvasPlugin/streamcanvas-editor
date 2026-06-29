#include "TitleDocument.h"

#include <stdexcept>

#include <QFileInfo>
#include <QColor>

#include "engine/element_image.h"
#include "engine/element_qr.h"
#include "engine/element_rectangle.h"
#include "engine/element_text.h"

using json = nlohmann::json;

// ── Serialization helpers ────────────────────────────────────────────────────

static std::string easingToString(Easing e)
{
    switch (e) {
    case Easing::EaseIn:          return "ease_in";
    case Easing::EaseOut:         return "ease_out";
    case Easing::EaseInOut:       return "ease_in_out";
    case Easing::EaseInCubic:     return "ease_in_cubic";
    case Easing::EaseOutCubic:    return "ease_out_cubic";
    case Easing::EaseInOutCubic:  return "ease_in_out_cubic";
    case Easing::EaseInExpo:      return "ease_in_expo";
    case Easing::EaseOutExpo:     return "ease_out_expo";
    case Easing::EaseInOutExpo:   return "ease_in_out_expo";
    case Easing::EaseInBack:      return "ease_in_back";
    case Easing::EaseOutBack:     return "ease_out_back";
    case Easing::EaseInOutBack:   return "ease_in_out_back";
    case Easing::EaseInElastic:   return "ease_in_elastic";
    case Easing::EaseOutElastic:  return "ease_out_elastic";
    case Easing::EaseInOutElastic:return "ease_in_out_elastic";
    default:                      return "linear";
    }
}

static std::string animTypeToString(AnimationType t)
{
    switch (t) {
    case AnimationType::Fade:       return "fade";
    case AnimationType::SlideUp:    return "slide_up";
    case AnimationType::SlideDown:  return "slide_down";
    case AnimationType::SlideLeft:  return "slide_left";
    case AnimationType::SlideRight: return "slide_right";
    case AnimationType::ScaleIn:    return "scale_in";
    case AnimationType::WipeUp:     return "wipe_up";
    case AnimationType::WipeDown:   return "wipe_down";
    case AnimationType::WipeLeft:   return "wipe_left";
    case AnimationType::WipeRight:  return "wipe_right";
    default:                        return "none";
    }
}

static std::string fontWeightToString(FontWeight w)
{
    switch (static_cast<int>(w)) {
    case PANGO_WEIGHT_THIN:       return "thin";
    case PANGO_WEIGHT_ULTRALIGHT: return "ultralight";
    case PANGO_WEIGHT_SEMILIGHT:  return "semilight";
    case PANGO_WEIGHT_LIGHT:      return "light";
    case PANGO_WEIGHT_BOOK:       return "book";
    case PANGO_WEIGHT_MEDIUM:     return "medium";
    case PANGO_WEIGHT_SEMIBOLD:   return "semibold";
    case PANGO_WEIGHT_BOLD:       return "bold";
    case PANGO_WEIGHT_ULTRABOLD:  return "ultrabold";
    case PANGO_WEIGHT_HEAVY:      return "heavy";
    case PANGO_WEIGHT_ULTRAHEAVY: return "ultraheavy";
    default:                      return "normal";
    }
}

static std::string alignmentHToString(HorizontalAlignment a)
{
    switch (a) {
    case HorizontalAlignment::Justify: return "justify";
    case HorizontalAlignment::Center:  return "center";
    case HorizontalAlignment::Right:   return "right";
    default:                           return "left";
    }
}

static std::string alignmentVToString(VerticalAlignment a)
{
    switch (a) {
    case VerticalAlignment::Bottom: return "bottom";
    case VerticalAlignment::Middle: return "middle";
    default:                        return "top";
    }
}

static std::string ellipsizeToString(Ellipsize e)
{
    switch (e) {
    case Ellipsize::Start:  return "start";
    case Ellipsize::Middle: return "middle";
    case Ellipsize::End:    return "end";
    default:                return "none";
    }
}

static std::string wrapModeToString(WrapMode w)
{
    switch (w) {
    case WrapMode::Char:     return "char";
    case WrapMode::WordChar: return "word_char";
    default:                 return "word";
    }
}

static std::string textTransformToString(TextTransform t)
{
    switch (t) {
    case TextTransform::Capitalize: return "capitalize";
    case TextTransform::Uppercase:  return "uppercase";
    case TextTransform::Lowercase:  return "lowercase";
    default:                        return "none";
    }
}

static json paintToJson(const Paint& p)
{
    if (p.type == Paint::Type::None)
        return json(nullptr);

    if (p.type == Paint::Type::Solid)
        return json::array({p.params[0], p.params[1], p.params[2], p.params[3]});

    if (p.type == Paint::Type::Image)
        return json(p.imagePath);

    json obj;
    if (p.type == Paint::Type::Linear) {
        obj["type"] = "linear";
        obj["x0"] = p.params[0]; obj["y0"] = p.params[1];
        obj["x1"] = p.params[2]; obj["y1"] = p.params[3];
    } else {
        obj["type"] = "radial";
        obj["cx0"] = p.params[0]; obj["cy0"] = p.params[1]; obj["r0"] = p.params[2];
        obj["cx1"] = p.params[3]; obj["cy1"] = p.params[4]; obj["r1"] = p.params[5];
    }
    json stops = json::array();
    for (const auto& s : p.stops) {
        json stop;
        stop["offset"] = s.offset;
        stop["color"] = json::array({s.r, s.g, s.b, s.a});
        stops.push_back(stop);
    }
    obj["stops"] = stops;
    return obj;
}

static json animDefToJson(const AnimationDef& a)
{
    return {{"type", animTypeToString(a.type)},
            {"easing", easingToString(a.easing)},
            {"duration", a.duration},
            {"delay", a.delay}};
}

// ── TitleDocument ─────────────────────────────────────────────────────────────

TitleDocument::TitleDocument(QObject* parent) : QObject(parent)
{
    m_brandColors = defaultBrandColors();
}

bool TitleDocument::load(const QString& path)
{
    try {
        m_title = Title::Load(path.toStdString());
    } catch (const std::exception&) {
        return false;
    }

    // Load brand colors from metadata, fall back to defaults
    if (m_title.metadata.contains("brand_colors") && m_title.metadata["brand_colors"].is_array()) {
        m_brandColors.clear();
        for (const auto& c : m_title.metadata["brand_colors"]) {
            if (c.is_string()) {
                QColor color(QString::fromStdString(c.get<std::string>()));
                if (color.isValid()) m_brandColors.append(color);
            }
        }
        if (m_brandColors.isEmpty())
            m_brandColors = defaultBrandColors();
    } else {
        m_brandColors = defaultBrandColors();
    }

    m_filePath = path;
    m_undoStack.clear();
    setModified(false);

    emit filePathChanged(m_filePath);
    emit documentChanged();
    return true;
}

bool TitleDocument::save()
{
    if (m_filePath.isEmpty())
        return false;
    return saveAs(m_filePath);
}

bool TitleDocument::saveAs(const QString& path)
{
    try {
        m_title.Save(path.toStdString());
    } catch (...) {
        return false;
    }

    if (path != m_filePath) {
        m_filePath = path;
        emit filePathChanged(m_filePath);
    }
    setModified(false);
    return true;
}

VisualElement& TitleDocument::getElement(const std::string& elementId)
{
    return m_title.GetById(elementId);
}

QString TitleDocument::titleName() const
{
    if (!m_title.id.empty())
        return QString::fromStdString(m_title.id);
    if (!m_filePath.isEmpty())
        return QFileInfo(m_filePath).baseName();
    return QString("Untitled");
}

void TitleDocument::setTitleName(const QString& name)
{
    m_title.id = name.toStdString();
    setModified(true);
    emit documentChanged();
}

void TitleDocument::setBrandColors(const QList<QColor>& colors)
{
    m_brandColors = colors;
    nlohmann::json arr = nlohmann::json::array();
    for (const QColor& c : colors)
        arr.push_back(c.name().toStdString());
    m_title.metadata["brand_colors"] = arr;
    setModified(true);
    emit documentChanged();
}

// static
QList<QColor> TitleDocument::defaultBrandColors()
{
    return {
        QColor(255, 255, 255),
        QColor(0,   0,   0),
        QColor(50,  80,  180),
        QColor(233, 30,  35),
        QColor(245, 165, 0),
        QColor(39,  174, 96),
        QColor(245, 245, 245),
        QColor(51,  51,  51),
        QColor(255, 255, 0),
        QColor(0,   193, 233),
    };
}

void TitleDocument::reset()
{
    m_title = Title{};
    m_title.id = "new_title";
    m_title.width = 1920;
    m_title.height = 1080;
    m_brandColors = defaultBrandColors();
    m_filePath.clear();
    m_undoStack.clear();
    setModified(false);
    emit filePathChanged(m_filePath);
    emit documentChanged();
}

// static
nlohmann::json TitleDocument::elementToJson(const VisualElement& el)
{
    json j;
    j["id"] = el.GetId();

    const Rectangle& b = el.GetBounds();
    j["x"] = b.x;
    j["y"] = b.y;
    j["w"] = b.width;
    j["h"] = b.height;
    j["z_order"] = el.zOrder;
    j["opacity"] = el.opacity;
    j["rotation"] = el.GetRotation();
    if (el.GetShearX() != 0.0f) j["shear_x"] = el.GetShearX();
    if (el.GetShearY() != 0.0f) j["shear_y"] = el.GetShearY();
    j["stroke_width"] = el.strokeWidth;

    bool allEqual = el.cornerRadius[0] == el.cornerRadius[1] &&
                    el.cornerRadius[1] == el.cornerRadius[2] &&
                    el.cornerRadius[2] == el.cornerRadius[3];
    if (allEqual)
        j["corner_radius"] = el.cornerRadius[0];
    else
        j["corner_radius"] = json::array({el.cornerRadius[0], el.cornerRadius[1],
                                          el.cornerRadius[2], el.cornerRadius[3]});

    if (el.fill.Valid())   j["fill"]   = paintToJson(el.fill);
    if (el.stroke.Valid()) j["stroke"] = paintToJson(el.stroke);

    if (el.inAnimation.type != AnimationType::None)
        j["anim_in"] = animDefToJson(el.inAnimation);
    if (el.outAnimation.type != AnimationType::None)
        j["anim_out"] = animDefToJson(el.outAnimation);

    if (el.shadow.enabled) {
        j["shadow"] = {{"enabled",  el.shadow.enabled},
                       {"offset_x", el.shadow.offsetX},
                       {"offset_y", el.shadow.offsetY},
                       {"blur",     el.shadow.blur},
                       {"color",    json::array({el.shadow.color[0], el.shadow.color[1],
                                                 el.shadow.color[2], el.shadow.color[3]})}};
    }

    if (el.clipChildren)
        j["clip_children"] = true;
    if (el.GetParent() && el.GetParent()->GetId() != "__root")
        j["parent"] = el.GetParent()->GetId();

    if (el.fitToChildren) {
        j["fit_to_children"] = true;
        j["children_padding"] = json::array({el.childrenPadding[0], el.childrenPadding[1],
                                             el.childrenPadding[2], el.childrenPadding[3]});
    }

    if (const auto* te = dynamic_cast<const TextElement*>(&el)) {
        j["type"] = "text";
        j["text"] = te->text;
        j["font_family"] = te->font.family;
        j["font_size"] = te->font.size;
        j["font_italic"] = te->font.isItalic;
        j["font_underline"] = te->font.isUnderline;
        j["font_strikethrough"] = te->font.isStrikethrough;
        j["font_weight"] = fontWeightToString(te->font.weight);
        j["auto_scale"] = te->textStyle.autoScale;
        j["text_align_x"] = alignmentHToString(te->textStyle.alignX);
        j["text_align_y"] = alignmentVToString(te->textStyle.alignY);
        j["ellipsize"] = ellipsizeToString(te->textStyle.ellipsize);
        j["wrap"] = wrapModeToString(te->textStyle.wrapMode);
        j["text_transform"] = textTransformToString(te->textStyle.transform);
        if (te->textStyle.lineSpacing != 0.0f) j["line_spacing"] = te->textStyle.lineSpacing;
        if (te->textStyle.charSpacing != 0.0f) j["char_spacing"] = te->textStyle.charSpacing;
    } else if (const auto* ie = dynamic_cast<const ImageElement*>(&el)) {
        j["type"] = "image";
        if (!ie->imagePath.empty()) j["image_path"] = ie->imagePath;
        static const char* kScaleModeStr[] = {"stretch","contain","cover","fit_width","fit_height","none"};
        j["scale_mode"] = kScaleModeStr[static_cast<int>(ie->imageScaleMode)];
    } else if (const auto* qe = dynamic_cast<const QrElement*>(&el)) {
        j["type"] = "qr_code";
        j["text"] = qe->text;
    } else {
        j["type"] = "rectangle";
    }

    return j;
}

void TitleDocument::applyMutation(std::function<void(Title&)> fn)
{
    fn(m_title);
    applyFitToChildren();
    setModified(true);
    emit documentChanged();
}

// private

void TitleDocument::applyFitToChildren()
{
    for (size_t i = 1; i < m_title.elements.size(); ++i) {
        auto* ve = dynamic_cast<VisualElement*>(m_title.elements[i].get());
        if (!ve || !ve->fitToChildren) continue;

        // Collect direct children (local coords relative to ve)
        std::vector<VisualElement*> children;
        for (const auto* child : ve->GetChildren()) {
            auto* vce = const_cast<VisualElement*>(dynamic_cast<const VisualElement*>(child));
            if (vce) children.push_back(vce);
        }
        if (children.empty()) continue;

        const Rectangle& cb0 = children[0]->GetBounds();
        double minX = cb0.x, minY = cb0.y;
        double maxX = minX + cb0.width, maxY = minY + cb0.height;
        for (auto* c : children) {
            const Rectangle& cb = c->GetBounds();
            minX = std::min(minX, cb.x);
            minY = std::min(minY, cb.y);
            maxX = std::max(maxX, cb.x + cb.width);
            maxY = std::max(maxY, cb.y + cb.height);
        }

        const float pT = ve->childrenPadding[0], pR = ve->childrenPadding[1];
        const float pB = ve->childrenPadding[2], pL = ve->childrenPadding[3];

        const double dx = minX - pL;
        const double dy = minY - pT;

        Rectangle vb = ve->GetBounds();
        vb.x += dx;
        vb.y += dy;
        vb.width  = maxX - minX + pL + pR;
        vb.height = maxY - minY + pT + pB;
        ve->SetBounds(vb);

        for (auto* c : children) {
            Rectangle cb = c->GetBounds();
            cb.x -= dx;
            cb.y -= dy;
            c->SetBounds(cb);
        }
    }
}

void TitleDocument::setModified(bool modified)
{
    if (m_modified == modified) return;
    m_modified = modified;
    emit modifiedChanged(m_modified);
}
