#pragma once

#include "engine/types.hpp"
#include <QWidget>

class BrandColorSwatchGrid;
class ColorPicker;
class TitleDocument;

// Compact paint picker embedded in the fill/stroke dropdown menu.
// Solid colors are picked inline via a ColorPicker.
// Full paint editing (gradient types etc.) is delegated to a dialog via moreOptionsRequested().
class PaintPickerWidget : public QWidget {
    Q_OBJECT
public:
    explicit PaintPickerWidget(TitleDocument* doc, QWidget* parent = nullptr);

    // Syncs the inline ColorPicker to the current element's paint (called on aboutToShow).
    void setPaint(const Paint& paint);

signals:
    void paintChanged(const Paint& paint);
    void moreOptionsRequested();

private:
    ColorPicker* m_colorPicker{nullptr};
    BrandColorSwatchGrid* m_brandGrid{nullptr};
};
