#include "CornerRadiusButton.h"
#include "ui/cornerradiuseditor.h"
#include "ui/UiUtils.h"
#include <QHBoxLayout>
#include <QMenu>
#include <QVBoxLayout>
#include <QWidgetAction>

CornerRadiusButton::CornerRadiusButton(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_uniform = new QDoubleSpinBox;
    m_uniform->setRange(0.0, 9999.0);
    m_uniform->setDecimals(1);
    m_uniform->setSuffix(" px");
    m_uniform->setMinimumWidth(72);
    m_uniform->setToolTip("Corner Radius (uniform)");

    m_moreBtn = new QToolButton;
    m_moreBtn->setText("…");
    m_moreBtn->setFixedSize(22, 22);
    m_moreBtn->setPopupMode(QToolButton::InstantPopup);
    m_moreBtn->setAutoRaise(true);
    m_moreBtn->setToolTip("Per-corner radius");

    auto* menu = new QMenu(m_moreBtn);

    auto* editor = new CornerRadiusEditor;
    auto* container = new QWidget;
    auto* cl = new QVBoxLayout(container);
    cl->setContentsMargins(8, 8, 8, 8);
    cl->addWidget(editor);

    auto* wa = new QWidgetAction(menu);
    wa->setDefaultWidget(container);
    menu->addAction(wa);
    m_moreBtn->setMenu(menu);

    connect(menu, &QMenu::aboutToShow, this, [this, editor]() {
        editor->setValues(m_tl, m_tr, m_br, m_bl);
    });
    connect(editor, &CornerRadiusEditor::valuesChanged,
            this, [this](float tl, float tr, float br, float bl) {
        m_tl = tl; m_tr = tr; m_br = br; m_bl = bl;
        UpdateGuard g(m_updating);
        m_uniform->setValue(static_cast<double>((tl + tr + br + bl) / 4.0f));
        emit valuesChanged(tl, tr, br, bl);
    });

    layout->addWidget(m_uniform);
    layout->addWidget(m_moreBtn);

    connect(m_uniform, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CornerRadiusButton::onUniformChanged);
}

void CornerRadiusButton::setValues(float tl, float tr, float br, float bl)
{
    m_tl = tl; m_tr = tr; m_br = br; m_bl = bl;
    UpdateGuard g(m_updating);
    m_uniform->setValue(static_cast<double>((tl + tr + br + bl) / 4.0f));
}

void CornerRadiusButton::getValues(float& tl, float& tr, float& br, float& bl) const
{
    tl = m_tl; tr = m_tr; br = m_br; bl = m_bl;
}

void CornerRadiusButton::onUniformChanged(double v)
{
    if (m_updating) return;
    m_tl = m_tr = m_br = m_bl = static_cast<float>(v);
    emit valuesChanged(m_tl, m_tr, m_br, m_bl);
}
