#pragma once

#include "engine/types.hpp"
#include <QObject>
#include <QSize>
#include <string>

class SceneDocument;
namespace Nedrysoft { namespace Ribbon { class RibbonWidget; } }
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QToolButton;
class QDialog;
class QPlainTextEdit;
class PaintEditor;
class PaintPickerWidget;

// Creates and manages the contextual "Graphic", "Element", "Style", and "Text" ribbon tabs.
// Graphic tab is visible when a graphic is selected. Element/Style/Text tabs are visible
// when an element is selected (Text only for text elements).
class RibbonFormatSection : public QObject {
    Q_OBJECT
public:
    explicit RibbonFormatSection(SceneDocument* doc, Nedrysoft::Ribbon::RibbonWidget* ribbon, QObject* parent = nullptr);

    int graphicTabIndex() const { return m_graphicTabIdx; }
    int elementTabIndex() const { return m_elemTabIdx; }
    int styleTabIndex()   const { return m_styleTabIdx; }
    int textTabIndex()    const { return m_textTabIdx; }
    int imageTabIndex()   const { return m_imageTabIdx; }
    int qrTabIndex()      const { return m_qrTabIdx; }

    void setGraphicSelection(const std::string& gi);
    void setSelection(const std::string& gi, const std::string& ei);
    void clearSelection();

signals:
    void elementIdChanged(const std::string& gi, const std::string& ei);
    void deleteGraphicRequested();
    void deleteElementRequested();

private slots:
    void onDocumentChanged();

    // Graphic tab slots
    void onGraphicIdEditingFinished();

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
    void buildGraphicTab(Nedrysoft::Ribbon::RibbonWidget* ribbon);
    void buildElementTab(Nedrysoft::Ribbon::RibbonWidget* ribbon);
    void buildStyleTab(Nedrysoft::Ribbon::RibbonWidget* ribbon);
    void buildTextTab(Nedrysoft::Ribbon::RibbonWidget* ribbon);
    void buildImageTab(Nedrysoft::Ribbon::RibbonWidget* ribbon);
    void buildQrTab(Nedrysoft::Ribbon::RibbonWidget* ribbon);
    void openContentEditor(const QString& title);

    QPixmap  makePaintSwatch(const Paint& paint, QSize size = QSize(36, 20));
    void updateFillSwatch();
    void updateStrokeSwatch();
    void populateRefCombos();
    void loadFonts();

    SceneDocument* m_doc{nullptr};
    std::string    m_graphicId;
    std::string    m_elementId;
    bool           m_updating{false};

    int m_graphicTabIdx{-1};
    int m_elemTabIdx{-1};
    int m_styleTabIdx{-1};
    int m_textTabIdx{-1};
    int m_imageTabIdx{-1};
    int m_qrTabIdx{-1};

    // Graphic tab
    QLineEdit* m_graphicIdEdit{nullptr};

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
    PaintEditor*        m_fillEditor{nullptr};    // gradient dialog editor
    PaintEditor*        m_strokeEditor{nullptr};  // gradient dialog editor
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

    // Shared text content editor (Text and QrCode elements)
    QDialog*        m_contentDialog{nullptr};
    QPlainTextEdit* m_contentEdit{nullptr};
    int             m_contentSavedCursor{0};

    // Image tab
    QLineEdit* m_imagePathEdit{nullptr};
    QComboBox* m_scaleMode{nullptr};
};
