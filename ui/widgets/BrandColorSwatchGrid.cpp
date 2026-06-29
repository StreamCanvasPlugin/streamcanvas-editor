#include "BrandColorSwatchGrid.h"

#include <QGridLayout>
#include <QPainter>
#include <QToolButton>

#include "model/TitleDocument.h"
#include "model/UndoCommands.h"

// ── SwatchCell ────────────────────────────────────────────────────────────────

class SwatchCell : public QWidget {
    Q_OBJECT
public:
    explicit SwatchCell(const QColor& color, bool selected, QWidget* parent = nullptr)
        : QWidget(parent), m_color(color), m_selected(selected)
    {
        setFixedSize(16, 16);
        setCursor(Qt::PointingHandCursor);
    }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const QRect r = rect();

        // Checkerboard for alpha
        const int cs = 4;
        for (int ry = 0; ry < r.height(); ry += cs)
            for (int rx = 0; rx < r.width(); rx += cs) {
                bool dark = ((rx / cs + ry / cs) % 2) == 0;
                p.fillRect(rx, ry, cs, cs, dark ? QColor(180, 180, 180) : Qt::white);
            }

        p.fillRect(r, m_color);

        if (m_selected) {
            p.setPen(QPen(QColor(64, 160, 255), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r.adjusted(1, 1, -1, -1));
        } else {
            p.setPen(QColor(100, 100, 100));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r.adjusted(0, 0, -1, -1));
        }
    }

    void mousePressEvent(QMouseEvent*) override { emit clicked(); }

private:
    QColor m_color;
    bool m_selected;
};

#include "BrandColorSwatchGrid.moc"

// ── BrandColorSwatchGrid ──────────────────────────────────────────────────────

BrandColorSwatchGrid::BrandColorSwatchGrid(TitleDocument* doc, Mode mode, QWidget* parent)
    : QWidget(parent), m_doc(doc), m_mode(mode)
{
    m_grid = new QGridLayout(this);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setSpacing(kCellGap);

    connect(m_doc, &TitleDocument::documentChanged, this, &BrandColorSwatchGrid::onDocumentChanged);

    rebuild();
}

void BrandColorSwatchGrid::setCurrentColor(const QColor& color)
{
    m_currentColor = color;
}

void BrandColorSwatchGrid::onDocumentChanged()
{
    const QList<QColor> current = m_doc->brandColors();
    if (current != m_cachedColors)
        rebuild();
}

void BrandColorSwatchGrid::rebuild()
{
    m_cachedColors = m_doc->brandColors();

    // Clear all existing items
    while (QLayoutItem* item = m_grid->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }

    const QList<QColor>& colors = m_cachedColors;
    const int total = colors.size();
    const int maxShow = (m_mode == Mode::Compact) ? kCols * kCompactRows : total;
    const int count = std::min(total, maxShow);

    for (int i = 0; i < count; ++i) {
        auto* cell = new SwatchCell(colors[i], i == m_selectedIndex);
        connect(cell, &SwatchCell::clicked, this, [this, i, color = colors[i]]() {
            m_selectedIndex = i;
            emit colorSelected(color);
            if (m_mode == Mode::Full)
                rebuild();
        });
        m_grid->addWidget(cell, i / kCols, i % kCols);
    }

    if (m_mode == Mode::Full) {
        const int nextPos = count;
        const int addRow = nextPos / kCols;
        const int addCol = nextPos % kCols;

        auto* addBtn = new QToolButton;
        addBtn->setText("+");
        addBtn->setFixedSize(kCellSize, kCellSize);
        addBtn->setToolTip("Add current color to brand palette");
        connect(addBtn, &QToolButton::clicked, this, [this]() {
            QList<QColor> newColors = m_doc->brandColors();
            newColors.append(m_currentColor);
            m_doc->undoStack()->push(new SetBrandColorsCmd(m_doc, newColors));
        });
        m_grid->addWidget(addBtn, addRow, addCol);

        const int removeCol = addCol + 1;
        const int removeRow = (removeCol >= kCols) ? addRow + 1 : addRow;
        const int removeColActual = removeCol % kCols;

        auto* removeBtn = new QToolButton;
        removeBtn->setText("−");
        removeBtn->setFixedSize(kCellSize, kCellSize);
        removeBtn->setToolTip("Remove selected color from brand palette");
        removeBtn->setEnabled(m_selectedIndex >= 0 && m_selectedIndex < total);
        connect(removeBtn, &QToolButton::clicked, this, [this]() {
            if (m_selectedIndex < 0 || m_selectedIndex >= m_doc->brandColors().size())
                return;
            QList<QColor> newColors = m_doc->brandColors();
            newColors.removeAt(m_selectedIndex);
            m_selectedIndex = -1;
            m_doc->undoStack()->push(new SetBrandColorsCmd(m_doc, newColors));
        });
        m_grid->addWidget(removeBtn, removeRow, removeColActual);
    }
}
