#include "ClipPropertiesPanel.h"

#include "ui/UiUtils.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>

namespace {

QString animationTypeName(AnimationType t)
{
    switch (t) {
    case AnimationType::None:       return "None";
    case AnimationType::Fade:       return "Fade";
    case AnimationType::SlideUp:    return "Slide Up";
    case AnimationType::SlideDown:  return "Slide Down";
    case AnimationType::SlideLeft:  return "Slide Left";
    case AnimationType::SlideRight: return "Slide Right";
    case AnimationType::ScaleIn:    return "Scale In";
    case AnimationType::WipeUp:     return "Wipe Up";
    case AnimationType::WipeDown:   return "Wipe Down";
    case AnimationType::WipeLeft:   return "Wipe Left";
    case AnimationType::WipeRight:  return "Wipe Right";
    }
    return {};
}

struct EasingEntry { Easing easing; const char* name; };

// Copied verbatim from AnimationTimingEditor.cpp's kAllEasings[] table.
constexpr EasingEntry kAllEasings[] = {
    { Easing::Linear,          "Linear"                },
    { Easing::EaseIn,          "Ease In"               },
    { Easing::EaseOut,         "Ease Out"              },
    { Easing::EaseInOut,       "Ease In/Out"           },
    { Easing::EaseInCubic,     "Ease In (Cubic)"       },
    { Easing::EaseOutCubic,    "Ease Out (Cubic)"      },
    { Easing::EaseInOutCubic,  "Ease In/Out (Cubic)"   },
    { Easing::EaseInExpo,      "Ease In (Expo)"        },
    { Easing::EaseOutExpo,     "Ease Out (Expo)"       },
    { Easing::EaseInOutExpo,   "Ease In/Out (Expo)"    },
    { Easing::EaseInBack,      "Ease In (Back)"        },
    { Easing::EaseOutBack,     "Ease Out (Back)"       },
    { Easing::EaseInOutBack,   "Ease In/Out (Back)"    },
    { Easing::EaseInElastic,   "Ease In (Elastic)"     },
    { Easing::EaseOutElastic,  "Ease Out (Elastic)"    },
    { Easing::EaseInOutElastic,"Ease In/Out (Elastic)" },
};

constexpr AnimationType kAllTypes[] = {
    AnimationType::None,       AnimationType::Fade,
    AnimationType::SlideUp,    AnimationType::SlideDown,
    AnimationType::SlideLeft,  AnimationType::SlideRight,
    AnimationType::ScaleIn,    AnimationType::WipeUp,
    AnimationType::WipeDown,   AnimationType::WipeLeft,
    AnimationType::WipeRight,
};

} // namespace

ClipPropertiesPanel::ClipPropertiesPanel(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(200);

    m_typeCombo = new QComboBox(this);
    for (auto t : kAllTypes)
        m_typeCombo->addItem(animationTypeName(t), int(t));

    m_easingCombo = new QComboBox(this);
    for (auto& e : kAllEasings)
        m_easingCombo->addItem(e.name, int(e.easing));

    m_delaySpin = makeSpinBox(0.0, 600.0, 2, " s");
    m_delaySpin->setSingleStep(0.05);

    m_durationSpin = makeSpinBox(0.01, 600.0, 2, " s");
    m_durationSpin->setSingleStep(0.05);

    auto* form = new QFormLayout(this);
    form->addRow(tr("Type"), m_typeCombo);
    form->addRow(tr("Easing"), m_easingCombo);
    form->addRow(tr("Delay"), m_delaySpin);
    form->addRow(tr("Duration"), m_durationSpin);

    connect(m_typeCombo, &QComboBox::currentIndexChanged,
            this, &ClipPropertiesPanel::onControlChanged);
    connect(m_easingCombo, &QComboBox::currentIndexChanged,
            this, &ClipPropertiesPanel::onControlChanged);
    connect(m_delaySpin, &QDoubleSpinBox::valueChanged,
            this, &ClipPropertiesPanel::onControlChanged);
    connect(m_durationSpin, &QDoubleSpinBox::valueChanged,
            this, &ClipPropertiesPanel::onControlChanged);

    setClip(false, AnimationDef{});
}

void ClipPropertiesPanel::setClip(bool valid, const AnimationDef& def)
{
    UpdateGuard guard(m_updating);

    setEnabled(valid);

    if (!valid) {
        m_typeCombo->setCurrentIndex(0);
        m_easingCombo->setCurrentIndex(0);
        m_delaySpin->setValue(0.0);
        m_durationSpin->setValue(m_durationSpin->minimum());
        return;
    }

    int typeIdx = m_typeCombo->findData(int(def.type));
    m_typeCombo->setCurrentIndex(typeIdx >= 0 ? typeIdx : 0);

    int easingIdx = m_easingCombo->findData(int(def.easing));
    m_easingCombo->setCurrentIndex(easingIdx >= 0 ? easingIdx : 0);

    m_delaySpin->setValue(def.delay);
    m_durationSpin->setValue(def.duration);
}

AnimationDef ClipPropertiesPanel::currentDef() const
{
    AnimationDef def;
    def.type     = AnimationType(m_typeCombo->currentData().toInt());
    def.easing   = Easing(m_easingCombo->currentData().toInt());
    def.delay    = float(m_delaySpin->value());
    def.duration = float(m_durationSpin->value());
    return def;
}

void ClipPropertiesPanel::onControlChanged()
{
    if (m_updating)
        return;
    emit clipChanged(currentDef());
}
