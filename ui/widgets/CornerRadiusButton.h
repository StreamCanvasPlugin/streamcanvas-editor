#pragma once

#include <QDoubleSpinBox>
#include <QToolButton>
#include <QWidget>

class CornerRadiusEditor;

// Compact corner-radius control for the ribbon: shows a uniform-corner spinbox
// plus a "…" button whose dropdown opens a QMenu with the full per-corner editor.
class CornerRadiusButton : public QWidget {
    Q_OBJECT
public:
    explicit CornerRadiusButton(QWidget* parent = nullptr);

    void setValues(float tl, float tr, float br, float bl);
    void getValues(float& tl, float& tr, float& br, float& bl) const;

signals:
    void valuesChanged(float tl, float tr, float br, float bl);

private slots:
    void onUniformChanged(double v);

private:
    QDoubleSpinBox* m_uniform{nullptr};
    QToolButton*    m_moreBtn{nullptr};
    float m_tl{0}, m_tr{0}, m_br{0}, m_bl{0};
    bool  m_updating{false};
};
