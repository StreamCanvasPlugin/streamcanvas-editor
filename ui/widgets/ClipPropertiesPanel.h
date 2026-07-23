#pragma once

#include <QWidget>

#include "engine/animation.h"

class QComboBox;
class QDoubleSpinBox;

// Pure view widget showing the fields of ONE AnimationDef for precise editing.
// No document/undo/selection logic -- the container widget owns that.
class ClipPropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit ClipPropertiesPanel(QWidget* parent = nullptr);

    // Populate the form. If valid==false, blank + disable the form controls.
    // Does NOT emit clipChanged() as a side effect.
    void setClip(bool valid, const AnimationDef& def);

signals:
    // Emitted whenever the user edits any field. Carries the FULL current
    // AnimationDef (all four fields read from the controls).
    void clipChanged(AnimationDef def);

private slots:
    void onControlChanged();

private:
    AnimationDef currentDef() const;

    QComboBox*      m_typeCombo{nullptr};
    QComboBox*      m_easingCombo{nullptr};
    QDoubleSpinBox* m_delaySpin{nullptr};
    QDoubleSpinBox* m_durationSpin{nullptr};
    bool            m_updating{false};
};
