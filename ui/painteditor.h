#pragma once

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QWidget>

#include "engine/types.hpp"
#include "ui/adaptivestack.h"
#include "ui/widgets/ColorPicker.h"
#include "ui/widgets/LinearGradientEditor.h"
#include "ui/widgets/RadialGradientEditor.h"

class BrandColorSwatchGrid;
class SceneDocument;

class PaintEditor : public QWidget {
    Q_OBJECT

public:
    explicit PaintEditor(SceneDocument* doc = nullptr, QWidget* parent = nullptr);

    Paint getPaint() const;
    void setPaint(const Paint& paint);

signals:
    void paintChanged(Paint paint);

private slots:
    void onTypeChanged(int index);
    void onColorChanged(const QColor& color);
    void onStopPosChanged(double value);
    void onStopsChanged(const QVector<GradientStop>& stops);
    void onStopSelected(int index);

private:
    QComboBox* m_typeCombo;
    AdaptiveStack* m_mainSelector;

    // solid page
    ColorPicker* m_cpSolid;

    // linear page
    ColorPicker* m_cpLinear;
    QDoubleSpinBox* m_lnStopPosition;
    LinearGradientEditor* m_linearEditor;

    // radial page
    ColorPicker* m_cpRadial;
    QDoubleSpinBox* m_radStopPosition;
    RadialGradientEditor* m_radialEditor;

    // image page
    QLineEdit* m_imagePathForPaint{nullptr};
    QComboBox* m_paintScaleMode{nullptr};

    bool m_updating{false}; // prevent recursion
    BrandColorSwatchGrid* m_brandGrid{nullptr};

    void emitPaint();
    bool eventFilter(QObject* obj, QEvent* event) override;
};
