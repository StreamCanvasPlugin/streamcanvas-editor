#ifndef COLORPICKER_H
#define COLORPICKER_H

#include "ui/widgets/ColorLineEdit.h"
#include "ui/widgets/ColorWheel.h"
#include "ui/widgets/GradientSlider.h"
#include <QWidget>

class ColorPicker : public QWidget {
    Q_OBJECT
public:
    explicit ColorPicker(QWidget* parent = nullptr);

    QColor color() const;
    void setColor(const QColor& color);

    static void showPopup(QWidget* at);

signals:
    void colorChanged(const QColor& color);

private slots:
    void onColorWheelChanged(const QColor& color);
    void onAlphaSliderChanged(int value);
    void onColorEditChanged(const QColor& color);

private:
    ColorWheel* m_colorWheel;
    GradientSlider* m_alphaSlider;
    ColorLineEdit* m_colorEdit;

    // Suppresses ColorPicker::colorChanged entirely; held for the duration
    // of a programmatic setColor() call.
    bool m_updating{false};
    // Suppresses the nested colorChanged emitted by onAlphaSliderChanged
    // when onColorEditChanged() programmatically calls m_alphaSlider->setValue();
    // onColorEditChanged() still emits its own single colorChanged afterwards.
    bool m_syncingFromEdit{false};

    QColor computeColor() const;

    // Repaints the alpha slider's track for the given opaque RGB. ColorWheel's
    // setColor() is deliberately silent, so this is NOT reached as a side
    // effect of a programmatic colour change and must be called explicitly.
    void updateAlphaStops(const QColor& rgb);

    // Drives every child widget from one colour. writeHex is false only when
    // the hex edit itself originated the change, so the user's own text is not
    // reformatted underneath them.
    void syncWidgets(const QColor& color, bool writeHex);
};

#endif // COLORPICKER_H
