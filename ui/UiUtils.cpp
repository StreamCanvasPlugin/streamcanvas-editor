#include "UiUtils.h"
#include <QVBoxLayout>
#include <QWidgetAction>

// ── GridBuilder ───────────────────────────────────────────────────────────────

GridBuilder::GridBuilder(QWidget* parent)
{
    m_layout = new QGridLayout(parent);
}

GridBuilder& GridBuilder::spacing(int hor, int ver)
{
    m_layout->setHorizontalSpacing(hor);
    m_layout->setVerticalSpacing(ver);
    return *this;
}

GridBuilder& GridBuilder::stretch(int row, int col, int rowStretch, int colStretch)
{
    if (rowStretch >= 0) m_layout->setRowStretch(row, rowStretch);
    if (colStretch >= 0) m_layout->setColumnStretch(col, colStretch);
    return *this;
}

GridBuilder& GridBuilder::addWidget(QWidget* widget, int row, int col,
                                     int rowSpan, int colSpan)
{
    m_layout->addWidget(widget, row, col, rowSpan, colSpan);
    m_lastWidget = widget;
    return *this;
}

GridBuilder& GridBuilder::align(Qt::Alignment alignment)
{
    if (!m_lastWidget) return *this;
    m_layout->setAlignment(m_lastWidget, alignment);
    return *this;
}

GridBuilder& GridBuilder::sizePolicy(QSizePolicy::Policy xSizePolicy,
                                      QSizePolicy::Policy ySizePolicy)
{
    if (!m_lastWidget) return *this;
    m_lastWidget->setSizePolicy(xSizePolicy, ySizePolicy);
    return *this;
}

// ── Widget factories ──────────────────────────────────────────────────────────

QDoubleSpinBox* makeSpinBox(double lo, double hi, int decimals,
                             const QString& suffix, int fixedWidth)
{
    auto* s = new QDoubleSpinBox;
    s->setRange(lo, hi);
    s->setDecimals(decimals);
    s->setSuffix(suffix);
    if (fixedWidth > 0) s->setMinimumWidth(fixedWidth);
    return s;
}

QLabel* makeTinyLabel(const QString& text)
{
    auto* lbl = new QLabel(text);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("font-size: 10px;");
    return lbl;
}

QLabel* makeRibbonLabel(const QString& text)
{
    auto* lbl = new QLabel(text);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lbl->setStyleSheet("font-size: 12px;");
    return lbl;
}

QToolButton* makeIconToolButton(const QIcon& icon, const QString& tip, bool checkable)
{
    auto* btn = new QToolButton;
    btn->setIcon(icon);
    btn->setIconSize({16, 16});
    btn->setCheckable(checkable);
    btn->setToolTip(tip);
    btn->setAutoRaise(true);
    return btn;
}

PopupButton makePopupButton(const QString& text, QWidget* editorWidget, QWidget* parent)
{
    auto* btn = new QToolButton(parent);
    btn->setText(text);
    btn->setPopupMode(QToolButton::MenuButtonPopup);
    btn->setAutoRaise(true);

    auto* menu = new QMenu(btn);

    auto* container = new QWidget;
    auto* cl = new QVBoxLayout(container);
    cl->setContentsMargins(8, 8, 8, 8);
    cl->addWidget(editorWidget);

    auto* action = new QWidgetAction(menu);
    action->setDefaultWidget(container);
    menu->addAction(action);

    btn->setMenu(menu);

    // Main-area click also opens the menu (not just the arrow).
    QObject::connect(btn, &QToolButton::clicked, btn, &QToolButton::showMenu);

    return {btn, menu};
}

QLabel* makeIconLabel(const QIcon &icon)
{
    auto* lbl = new QLabel();
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lbl->setPixmap(icon.pixmap(16, 16));
    return lbl;
}
