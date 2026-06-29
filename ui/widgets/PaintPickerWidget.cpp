#include "PaintPickerWidget.h"
#include "BrandColorSwatchGrid.h"
#include "ColorPicker.h"
#include "icons.h"
#include "model/TitleDocument.h"
#include <QFrame>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

PaintPickerWidget::PaintPickerWidget(TitleDocument* doc, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    auto* noneBtn = new QPushButton(themedIcon(Icons16::Action_Close), "No Styling");
    noneBtn->setAutoDefault(false);
    layout->addWidget(noneBtn);

    auto addSep = [&]() {
        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        layout->addWidget(sep);
    };

    addSep();

    if (doc) {
        m_brandGrid = new BrandColorSwatchGrid(doc, BrandColorSwatchGrid::Mode::Compact);
        layout->addWidget(m_brandGrid);
        addSep();
    }

    m_colorPicker = new ColorPicker;
    layout->addWidget(m_colorPicker);

    addSep();

    auto* moreBtn = new QPushButton(themedIcon(Icons16::Misc_PaintBucketDrop), "More Options…");
    moreBtn->setAutoDefault(false);
    layout->addWidget(moreBtn);

    connect(noneBtn, &QPushButton::clicked, this, [this]() {
        emit paintChanged(Paint{});
    });
    connect(m_colorPicker, &ColorPicker::colorChanged, this, [this](const QColor& c) {
        emit paintChanged(Paint::Solid(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
    });
    connect(moreBtn, &QPushButton::clicked, this, [this]() {
        emit moreOptionsRequested();
    });

    if (m_brandGrid) {
        connect(m_brandGrid, &BrandColorSwatchGrid::colorSelected, this,
                [this](const QColor& c) {
                    QSignalBlocker b(m_colorPicker);
                    m_colorPicker->setColor(c);
                    emit paintChanged(Paint::Solid(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
                });
    }
}

void PaintPickerWidget::setPaint(const Paint& paint)
{
    if (paint.type == Paint::Type::Solid) {
        QSignalBlocker b(m_colorPicker);
        m_colorPicker->setColor(QColor::fromRgbF(
            paint.params[0], paint.params[1], paint.params[2], paint.params[3]));
    }
}
