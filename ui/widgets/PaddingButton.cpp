#include "PaddingButton.h"
#include "ui/paddingeditor.h"
#include "ui/UiUtils.h"
#include <QHBoxLayout>
#include <QMenu>
#include <QVBoxLayout>
#include <QWidgetAction>

PaddingButton::PaddingButton(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_uniform = new QDoubleSpinBox;
    m_uniform->setRange(0.0, 9999.0);
    m_uniform->setDecimals(1);
    m_uniform->setSuffix(" px");
    m_uniform->setFixedWidth(72);
    m_uniform->setToolTip("Padding (uniform)");

    m_moreBtn = new QToolButton;
    m_moreBtn->setText("…");
    m_moreBtn->setFixedSize(22, 22);
    m_moreBtn->setPopupMode(QToolButton::InstantPopup);
    m_moreBtn->setAutoRaise(true);
    m_moreBtn->setToolTip("Per-side padding");

    auto* menu = new QMenu(m_moreBtn);

    auto* editor = new PaddingEditor;
    auto* container = new QWidget;
    auto* cl = new QVBoxLayout(container);
    cl->setContentsMargins(8, 8, 8, 8);
    cl->addWidget(editor);

    auto* wa = new QWidgetAction(menu);
    wa->setDefaultWidget(container);
    menu->addAction(wa);
    m_moreBtn->setMenu(menu);

    connect(menu, &QMenu::aboutToShow, this, [this, editor]() {
        editor->setValues(m_top, m_right, m_bottom, m_left);
    });
    connect(editor, &PaddingEditor::paddingChanged,
            this, [this](float top, float right, float bottom, float left) {
        m_top = top; m_right = right; m_bottom = bottom; m_left = left;
        UpdateGuard g(m_updating);
        m_uniform->setValue(
            static_cast<double>((top + right + bottom + left) / 4.0f));
        emit paddingChanged(top, right, bottom, left);
    });

    layout->addWidget(m_uniform);
    layout->addWidget(m_moreBtn);

    connect(m_uniform, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PaddingButton::onUniformChanged);
}

void PaddingButton::setValues(float top, float right, float bottom, float left)
{
    m_top = top; m_right = right; m_bottom = bottom; m_left = left;
    UpdateGuard g(m_updating);
    m_uniform->setValue(
        static_cast<double>((top + right + bottom + left) / 4.0f));
}

void PaddingButton::getValues(float& top, float& right, float& bottom, float& left) const
{
    top = m_top; right = m_right; bottom = m_bottom; left = m_left;
}

void PaddingButton::onUniformChanged(double v)
{
    if (m_updating) return;
    m_top = m_right = m_bottom = m_left = static_cast<float>(v);
    emit paddingChanged(m_top, m_right, m_bottom, m_left);
}
