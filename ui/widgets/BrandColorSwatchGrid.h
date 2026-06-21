#pragma once

#include <QColor>
#include <QList>
#include <QWidget>

class QGridLayout;
class SceneDocument;

class BrandColorSwatchGrid : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Compact, Full };

    explicit BrandColorSwatchGrid(SceneDocument* doc, Mode mode, QWidget* parent = nullptr);

    void setCurrentColor(const QColor& color);

signals:
    void colorSelected(const QColor& color);

private slots:
    void onDocumentChanged();

private:
    void rebuild();

    SceneDocument* m_doc{nullptr};
    Mode m_mode;
    QGridLayout* m_grid{nullptr};
    QColor m_currentColor{Qt::black};
    QList<QColor> m_cachedColors;
    int m_selectedIndex{-1};

    static constexpr int kCellSize = 16;
    static constexpr int kCellGap  = 2;
    static constexpr int kCols = 8;
    static constexpr int kCompactRows = 2;
};
