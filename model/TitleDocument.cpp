#include "TitleDocument.h"

#include <stdexcept>

#include <QFileInfo>
#include <QColor>

#include "engine/element_image.h"
#include "engine/element_qr.h"
#include "engine/element_rectangle.h"
#include "engine/element_text.h"
#include "engine/title.h"

using json = nlohmann::json;

// ── TitleDocument ─────────────────────────────────────────────────────────────

TitleDocument::TitleDocument(QObject* parent) : QObject(parent)
{
    m_brandColors = defaultBrandColors();
}

bool TitleDocument::load(const QString& path)
{
    m_lastError.clear();
    try {
        m_title = Title::Load(path.toStdString());
    } catch (const std::exception& e) {
        m_lastError = QString::fromStdString(e.what());
        return false;
    }
    m_lastLoadDiagnostic = m_title.GetLoadDiagnostic();

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
    m_lastError.clear();
    try {
        m_title.Save(path.toStdString());
    } catch (const std::exception& e) {
        m_lastError = QString::fromStdString(e.what());
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

bool TitleDocument::isElementLocked(const std::string& id) const
{
    if (!m_title.metadata.contains("locked_ids") || !m_title.metadata["locked_ids"].is_array())
        return false;
    for (const auto& v : m_title.metadata["locked_ids"]) {
        if (v.is_string() && v.get<std::string>() == id)
            return true;
    }
    return false;
}

void TitleDocument::setElementLocked(const std::string& id, bool locked)
{
    // Lock is persisted (metadata) but intentionally not on the undo stack.
    applyMutation([&](Title& t) {
        nlohmann::json& arr = t.metadata["locked_ids"];
        if (!arr.is_array())
            arr = nlohmann::json::array();

        bool present = false;
        for (const auto& v : arr) {
            if (v.is_string() && v.get<std::string>() == id) { present = true; break; }
        }

        if (locked && !present) {
            arr.push_back(id);
        } else if (!locked && present) {
            nlohmann::json filtered = nlohmann::json::array();
            for (const auto& v : arr) {
                if (!(v.is_string() && v.get<std::string>() == id))
                    filtered.push_back(v);
            }
            arr = filtered;
        }
    });
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
    const IElement* root = &el;
    while (root->GetParent()) root = root->GetParent();   // reach "__root"
    return ogt::SerializeElement(&el, root);
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
