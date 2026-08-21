#pragma once

#include <QButtonGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMap>
#include <QStackedWidget>
#include <QWidget>

#include "engine/types.hpp"
#include "ui/widgets/ColorPicker.h"
#include "ui/widgets/GradientStopBar.h"
#include "ui/widgets/LinearGradientEditor.h"
#include "ui/widgets/RadialGradientEditor.h"

class BrandColorSwatchGrid;
class TitleDocument;
class QToolButton;
class QLabel;

class PaintEditor : public QWidget {
    Q_OBJECT

public:
    explicit PaintEditor(TitleDocument* doc = nullptr, QWidget* parent = nullptr);

    Paint getPaint() const;
    void setPaint(const Paint& paint);

    // Forwarded to both geometry editors so gradient authoring matches the
    // engine's element-space normalization. Defaults to 1:1 until called.
    void setTargetElementSize(double w, double h);

    bool isInteracting() const
    {
        return m_interactionDepth > 0;
    }

signals:
    void paintChanged(Paint paint);
    void interactionStarted();
    void interactionFinished();

private slots:
    void onTypeButtonClicked(int type);
    void onColorChanged(const QColor& color);
    void onStopsChanged(const QVector<GradientStop>& stops);
    void onStopBarStopSelected(int index);
    void onGeometryStopSelected(int index);

private:
    QButtonGroup* m_typeGroup;
    QStackedWidget* m_mainSelector;
    Paint::Type m_currentType{Paint::Type::None};

    // Cache of the last-authored paint for each type, so switching type away
    // and back restores what was there (None -> Linear -> None -> Linear).
    QMap<Paint::Type, Paint> m_paintByType;

    // Shared colour section — edits solid colour in Solid mode, or the
    // selected gradient stop's colour in Linear/Radial mode.
    ColorPicker* m_colorPicker;

    // shared gradient stop UI
    GradientStopBar* m_stopBar;

    // linear page
    LinearGradientEditor* m_linearEditor;
    QDoubleSpinBox* m_lnAngle{nullptr};
    QDoubleSpinBox* m_lnLength{nullptr};

    // radial page
    RadialGradientEditor* m_radialEditor;
    QDoubleSpinBox* m_radCenterX{nullptr};
    QDoubleSpinBox* m_radCenterY{nullptr};
    QDoubleSpinBox* m_radRadius{nullptr};

    // image page
    QLineEdit* m_imagePathForPaint{nullptr};
    QComboBox* m_paintScaleMode{nullptr};
    QDoubleSpinBox* m_tileScale{nullptr};
    QLabel* m_imageErrorLabel{nullptr};

    // Cache to avoid re-decoding the PNG on every emit; rebuilt only when
    // the path (or scale mode / tile scale) actually changes.
    mutable Paint m_cachedImagePaint;
    mutable QString m_cachedImagePath;
    // Forces the next getPaint() to re-decode even when the path string is
    // unchanged. Set by the two USER-driven path entry points (Browse and
    // editingFinished) so re-picking the same file reloads a PNG that was
    // overwritten on disk. Programmatic setPaint() must not set it.
    mutable bool m_imageCacheDirty{true};

    bool m_updating{false}; // prevent recursion
    BrandColorSwatchGrid* m_brandGrid{nullptr};

    int m_interactionDepth{0};

    void emitPaint();
    void beginInteraction();
    void endInteraction();
    void applyTypeUi(Paint::Type type);
    void syncColorPickerToSelectedStop();
    bool eventFilter(QObject* obj, QEvent* event) override;
};
