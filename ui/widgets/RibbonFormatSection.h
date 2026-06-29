#pragma once

#include "engine/types.hpp"
#include <QObject>
#include <QSize>
#include <string>

class TitleDocument;
class SARibbonBar;
class SARibbonCategory;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QToolButton;
class QDialog;
class QPlainTextEdit;
class ColorPicker;
class PaintEditor;
class PaintPickerWidget;

// Creates and manages the contextual "Element", "Style", "Text", "Image", and "QR Code"
// ribbon categories. Categories shown/hidden in MainWindow::onSelectionChanged.
class RibbonFormatSection : public QObject {
    Q_OBJECT
public:
    explicit RibbonFormatSection(TitleDocument* doc, SARibbonBar* ribbon, QObject* parent = nullptr);

    SARibbonCategory* elementCategory() const { return m_elemCategory; }
    SARibbonCategory* styleCategory()   const { return m_styleCategory; }
    SARibbonCategory* textCategory()    const { return m_textCategory; }
    SARibbonCategory* imageCategory()   const { return m_imageCategory; }
    SARibbonCategory* qrCategory()      const { return m_qrCategory; }

    void setSelection(const std::string& ei);
    void clearSelection();

signals:
    void elementIdChanged(const std::string& ei);
    void deleteElementRequested();

private slots:
    void onDocumentChanged();

    // Element tab slots
    void onXChanged(double v);
    void onYChanged(double v);
    void onWChanged(double v);
    void onHChanged(double v);
    void onRotChanged(double v);
    void onZOrderChanged(int v);
    void onOpacityChanged(double v);
    void onFillPaintChanged(const Paint& p);
    void onStrokePaintChanged(const Paint& p);
    void onStrokeWidthChanged(double v);
    void onCornerRadiusChanged();
    void onIdEditingFinished();
    void onMaskChanged(int idx);
    void onParentChanged(int idx);
    void onFitToChildrenToggled(bool checked);
    void onChildrenPaddingChanged();
    void onShearXChanged(double v);
    void onShearYChanged(double v);
    void onShadowChanged();

    // Text tab slots
    void onFontFamilyChanged(int idx);
    void onFontSizeChanged(double v);
    void onFontWeightChanged(int idx);
    void onItalicToggled(bool checked);
    void onUnderlineToggled(bool checked);
    void onStrikethroughToggled(bool checked);
    void onAlignXChanged(int idx);
    void onAlignYChanged(int idx);
    void onAutoScaleToggled(bool checked);
    void onEllipsizeChanged(int idx);
    void onWrapChanged(int idx);
    void onTextTransformChanged(int idx);

    // Image tab slots
    void onImagePathChanged();
    void onScaleModeChanged(int idx);

    // Shared text content slot (Text and QrCode elements)
    void onContentChanged();

private:
    void buildElementTab(SARibbonBar* ribbon);
    void buildStyleTab(SARibbonBar* ribbon);
    void buildTextTab(SARibbonBar* ribbon);
    void buildImageTab(SARibbonBar* ribbon);
    void buildQrTab(SARibbonBar* ribbon);
    void openContentEditor(const QString& title);

    QPixmap makePaintSwatch(const Paint& paint, QSize size = QSize(36, 20));
    void updateFillSwatch();
    void updateStrokeSwatch();
    void populateRefCombos();
    void loadFonts();

    TitleDocument* m_doc{nullptr};
    std::string    m_elementId;
    bool           m_updating{false};

    SARibbonCategory* m_elemCategory{nullptr};
    SARibbonCategory* m_styleCategory{nullptr};
    SARibbonCategory* m_textCategory{nullptr};
    SARibbonCategory* m_imageCategory{nullptr};
    SARibbonCategory* m_qrCategory{nullptr};

    // Transform group
    QDoubleSpinBox* m_spinX{nullptr};
    QDoubleSpinBox* m_spinY{nullptr};
    QDoubleSpinBox* m_spinW{nullptr};
    QDoubleSpinBox* m_spinH{nullptr};
    QDoubleSpinBox* m_spinRot{nullptr};
    QDoubleSpinBox* m_spinShearX{nullptr};
    QDoubleSpinBox* m_spinShearY{nullptr};

    // Appearance group
    QSpinBox*       m_spinZ{nullptr};
    QDoubleSpinBox* m_spinOpacity{nullptr};

    // Style tab controls
    QToolButton*        m_fillBtn{nullptr};
    QToolButton*        m_strokeBtn{nullptr};
    PaintPickerWidget*  m_fillPicker{nullptr};
    PaintPickerWidget*  m_strokePicker{nullptr};
    PaintEditor*        m_fillEditor{nullptr};
    PaintEditor*        m_strokeEditor{nullptr};
    QDoubleSpinBox*     m_strokeWidth{nullptr};

    // Border group (corner radii)
    QDoubleSpinBox* m_spinTL{nullptr};
    QDoubleSpinBox* m_spinTR{nullptr};
    QDoubleSpinBox* m_spinBR{nullptr};
    QDoubleSpinBox* m_spinBL{nullptr};

    // Shadow group (Style tab)
    QCheckBox*      m_shadowEnabled{nullptr};
    QWidget*        m_shadowControls{nullptr};
    QDoubleSpinBox* m_spinShadowOffX{nullptr};
    QDoubleSpinBox* m_spinShadowOffY{nullptr};
    QDoubleSpinBox* m_spinShadowBlur{nullptr};
    QToolButton*    m_shadowColorBtn{nullptr};
    ColorPicker*    m_shadowColorPicker{nullptr};

    // Info group (element)
    QLineEdit* m_idEdit{nullptr};

    // Layout group
    QComboBox* m_maskCombo{nullptr};
    QComboBox* m_parentCombo{nullptr};
    QCheckBox* m_fitCheck{nullptr};
    QWidget*   m_paddingContainer{nullptr};
    QDoubleSpinBox* m_spinPadTop{nullptr};
    QDoubleSpinBox* m_spinPadRight{nullptr};
    QDoubleSpinBox* m_spinPadBottom{nullptr};
    QDoubleSpinBox* m_spinPadLeft{nullptr};

    // Font group
    QComboBox*      m_fontFamily{nullptr};
    QDoubleSpinBox* m_fontSize{nullptr};
    QComboBox*      m_fontWeight{nullptr};
    QToolButton*    m_italicBtn{nullptr};
    QToolButton*    m_underlineBtn{nullptr};
    QToolButton*    m_strikeBtn{nullptr};

    // Paragraph group
    QToolButton* m_alignLeft{nullptr};
    QToolButton* m_alignCenter{nullptr};
    QToolButton* m_alignJustify{nullptr};
    QToolButton* m_alignRight{nullptr};
    QToolButton* m_alignTop{nullptr};
    QToolButton* m_alignMiddle{nullptr};
    QToolButton* m_alignBottom{nullptr};
    QCheckBox*   m_autoScale{nullptr};

    // Text group
    QComboBox*   m_ellipsize{nullptr};
    QComboBox*   m_wrap{nullptr};
    QComboBox*   m_textTransform{nullptr};
    QToolButton* m_textBtn{nullptr};

    // Shared text content editor
    QDialog*        m_contentDialog{nullptr};
    QPlainTextEdit* m_contentEdit{nullptr};
    int             m_contentSavedCursor{0};

    // Image tab
    QLineEdit* m_imagePathEdit{nullptr};
    QComboBox* m_scaleMode{nullptr};
};
