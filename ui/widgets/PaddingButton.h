#pragma once

#include <QDoubleSpinBox>
#include <QToolButton>
#include <QWidget>

class PaddingEditor;

// Compact padding control for the ribbon: shows a uniform-padding spinbox
// plus a "…" button whose dropdown opens a QMenu with the full per-side editor.
class PaddingButton : public QWidget {
    Q_OBJECT
public:
    explicit PaddingButton(QWidget* parent = nullptr);

    void setValues(float top, float right, float bottom, float left);
    void getValues(float& top, float& right, float& bottom, float& left) const;

signals:
    void paddingChanged(float top, float right, float bottom, float left);

private slots:
    void onUniformChanged(double v);

private:
    QDoubleSpinBox* m_uniform{nullptr};
    QToolButton*    m_moreBtn{nullptr};
    float m_top{0}, m_right{0}, m_bottom{0}, m_left{0};
    bool  m_updating{false};
};
