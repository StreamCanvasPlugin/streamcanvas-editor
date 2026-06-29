#pragma once

#include <functional>
#include <string>

#include <QColor>
#include <QList>
#include <QObject>
#include <QString>
#include <QUndoStack>
#include <nlohmann/json.hpp>

#include "engine/title.h"
#include "engine/visual_element.h"

class TitleDocument : public QObject {
    Q_OBJECT
public:
    explicit TitleDocument(QObject* parent = nullptr);

    // File operations
    bool load(const QString& path);
    bool save();
    bool saveAs(const QString& path);

    // Accessors
    Title& title() { return m_title; }
    const Title& title() const { return m_title; }
    VisualElement& getElement(const std::string& elementId);
    QString filePath() const { return m_filePath; }
    bool isModified() const { return m_modified; }

    // Display name (maps to title.id for persistence)
    QString titleName() const;

    QUndoStack* undoStack() { return &m_undoStack; }

    // Reset to an empty title, clearing file path and undo stack
    void reset();

    // All mutations go through here; fn may freely modify m_title
    void applyMutation(std::function<void(Title&)> fn);

    // Expose title name mutation (used by SetTitleNameCmd)
    void setTitleName(const QString& name);

    // Brand colors — persisted in title.metadata["brand_colors"]
    QList<QColor> brandColors() const { return m_brandColors; }
    void setBrandColors(const QList<QColor>& colors);
    static QList<QColor> defaultBrandColors();

    // Serialization of a single element to JSON (for clipboard/undo snapshots)
    static nlohmann::json elementToJson(const VisualElement& el);

signals:
    void documentChanged();
    void filePathChanged(const QString& path);
    void modifiedChanged(bool modified);

private:
    Title m_title;
    QUndoStack m_undoStack;
    QString m_filePath;
    bool m_modified{false};
    QList<QColor> m_brandColors;

    void applyFitToChildren();
    void setModified(bool modified);
};
