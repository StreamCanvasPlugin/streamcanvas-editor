#include "GraphicTimingEditor.h"
#include "AnimationTimingEditor.h"
#include "icons.h"
#include "model/TitleDocument.h"
#include "engine/title.h"
#include "engine/visual_element.h"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

static constexpr int kNameColWidth = 120;

GraphicTimingEditor::GraphicTimingEditor(TitleDocument* doc, QWidget* parent)
    : QWidget{parent}, m_doc{doc}
{
    m_playBtn = new QPushButton("▶");
    m_playBtn->setFixedSize(26, 26);

    m_speedSlider = new QSlider(Qt::Horizontal);
    m_speedSlider->setRange(0, 100);
    m_speedSlider->setValue(100);
    m_speedSlider->setFixedWidth(80);
    m_speedSlider->setToolTip("Playback speed (0–1×)");

    m_timeLabel = new QLabel("0.00 s");
    m_timeLabel->setFixedWidth(52);
    m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_tabBar = new QTabBar;
    m_tabBar->setIconSize({16, 16});
    m_tabBar->addTab(themedIcon(Icons16::Action_LogIn),    "In");
    m_tabBar->addTab(themedIcon(Icons16::Action_LogOut),   "Out");
    m_tabBar->addTab(themedIcon(Icons16::Action_Download), "Data Change In");
    m_tabBar->addTab(themedIcon(Icons16::Action_Export),   "Data Change Out");

    m_maxDurSpin = new QDoubleSpinBox;
    m_maxDurSpin->setRange(0.1, 60.0);
    m_maxDurSpin->setSingleStep(0.1);
    m_maxDurSpin->setDecimals(2);
    m_maxDurSpin->setSuffix(" s");
    m_maxDurSpin->setValue(1.0);
    m_maxDurSpin->setFixedWidth(80);

    auto* toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(4, 2, 4, 2);
    toolbar->setSpacing(4);
    toolbar->addWidget(m_tabBar);
    toolbar->addWidget(m_playBtn);
    toolbar->addWidget(m_timeLabel);
    toolbar->addWidget(new QLabel("Speed:"));
    toolbar->addWidget(m_speedSlider);
    toolbar->addStretch();
    toolbar->addWidget(new QLabel("Max. Duration:"));
    toolbar->addWidget(m_maxDurSpin);

    m_content = new QWidget;
    m_content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_grid = new QGridLayout(m_content);
    m_grid->setContentsMargins(4, 4, 4, 4);
    m_grid->setHorizontalSpacing(4);
    m_grid->setVerticalSpacing(2);
    m_grid->setColumnMinimumWidth(0, kNameColWidth);
    m_grid->setColumnStretch(1, 1);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(m_content);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addLayout(toolbar);
    root->addWidget(scroll);

    m_playTimer = new QTimer(this);
    m_playTimer->setInterval(16);

    connect(m_tabBar,     &QTabBar::currentChanged,      this, &GraphicTimingEditor::onTabChanged);
    connect(m_maxDurSpin, &QDoubleSpinBox::valueChanged, this, &GraphicTimingEditor::onMaxDurationChanged);
    connect(m_playBtn,    &QPushButton::clicked,         this, [this] {
        m_playState == PlayState::Playing ? onStop() : onPlay();
    });
    connect(m_speedSlider, &QSlider::valueChanged, this, [this](int v) {
        m_playSpeed = v / 100.0f;
    });
    connect(m_playTimer,  &QTimer::timeout,              this, &GraphicTimingEditor::onTick);
}

// ── public ──────────────────────────────────────────────────────────────────

void GraphicTimingEditor::load()
{
    onStop();
    m_maxDurSpin->blockSignals(true);
    m_maxDurSpin->setValue(double(computeAutoMaxDuration()));
    m_maxDurSpin->blockSignals(false);
    rebuild();
}

void GraphicTimingEditor::clear()
{
    onStop();
    rebuild();
}

// ── private slots ────────────────────────────────────────────────────────────

void GraphicTimingEditor::onTabChanged(int)
{
    onStop();
    m_maxDurSpin->blockSignals(true);
    m_maxDurSpin->setValue(double(computeAutoMaxDuration()));
    m_maxDurSpin->blockSignals(false);
    rebuild();
}

void GraphicTimingEditor::onMaxDurationChanged(double value)
{
    const float maxD = float(value);
    for (AnimationTimingEditor* ed : m_editors)
        ed->setMaxDuration(maxD);
}

void GraphicTimingEditor::onPlay()
{
    m_playState = PlayState::Playing;
    m_playBtn->setText("⏹");
    m_playTimer->start();
}

void GraphicTimingEditor::onStop()
{
    m_playState = PlayState::Stopped;
    m_playBtn->setText("▶");
    m_playTimer->stop();
    setScrubTimeOnAll(0.0f);
    emit previewStopped();
}

void GraphicTimingEditor::onTick()
{
    const float maxD = float(m_maxDurSpin->value());
    m_scrubTime += m_playTimer->interval() / 1000.0f * m_playSpeed;
    if (m_scrubTime >= maxD) {
        m_scrubTime = maxD;
        setScrubTimeOnAll(m_scrubTime);
        onStop();
        return;
    }
    setScrubTimeOnAll(m_scrubTime);
}

// ── private ──────────────────────────────────────────────────────────────────

bool GraphicTimingEditor::isIn() const
{
    const int tab = m_tabBar->currentIndex();
    return tab == 0 || tab == 2;
}

bool GraphicTimingEditor::isDataAnim() const
{
    return m_tabBar->currentIndex() >= 2;
}

float GraphicTimingEditor::computeAutoMaxDuration() const
{
    const Title& t = m_doc->title();
    const bool in = isIn();
    const bool data = isDataAnim();
    float maxEnd = 0.1f;
    for (int i = 1; i < (int)t.elements.size(); ++i) {
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[i].get());
        if (!ve) continue;
        const AnimationDef& def = data
            ? (in ? ve->dataInAnimation : ve->dataOutAnimation)
            : (in ? ve->inAnimation     : ve->outAnimation);
        maxEnd = qMax(maxEnd, def.delay + def.duration);
    }
    return std::ceil(maxEnd * 2.0f) / 2.0f;
}

void GraphicTimingEditor::setScrubTimeOnAll(float t)
{
    m_scrubTime = t;
    m_timeLabel->setText(QString("%1 s").arg(double(t), 0, 'f', 2));
    emit scrubTimeChanged(t);
}

void GraphicTimingEditor::rebuild()
{
    while (QLayoutItem* item = m_grid->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    m_editors.clear();

    const Title& t = m_doc->title();
    const float maxD = float(m_maxDurSpin->value());
    const bool in = isIn();
    const bool data = isDataAnim();

    for (int i = 1; i < (int)t.elements.size(); ++i) {
        const auto* ve = dynamic_cast<const VisualElement*>(t.elements[i].get());
        if (!ve) continue;

        auto* label = new QLabel(QString::fromStdString(ve->GetId()));
        label->setFixedWidth(kNameColWidth);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        const AnimationDef& animDef = data
            ? (in ? ve->dataInAnimation : ve->dataOutAnimation)
            : (in ? ve->inAnimation     : ve->outAnimation);

        auto* editor = new AnimationTimingEditor;
        editor->setMaxDuration(maxD);
        editor->load(animDef);

        const int row = static_cast<int>(m_editors.size());
        connect(editor, &AnimationTimingEditor::animationChanged,
                this, [this, i, in, data](AnimationType type, Easing easing, float delay, float duration) {
            emit animationChanged(i, in, data, type, easing, delay, duration);
        });

        m_grid->addWidget(label,  row, 0);
        m_grid->addWidget(editor, row, 1);
        m_editors.append(editor);
    }
}
