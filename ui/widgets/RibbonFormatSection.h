#pragma once

#include "engine/types.hpp"
#include <QObject>
#include <QPointer>
#include <QSize>
#include <QTimer>
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
class QAction;
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
    void copyStyleModeToggled(bool on, const std::string& sourceElementId);

public slots:
    void setPaintModeActive(bool active);

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
    void onClipChildrenToggled(bool checked);
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
    void onLineSpacingChanged(double v);
    void onCharSpacingChanged(double v);

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

    // Updates m_imagePathEdit's warning visual state for the given path.
    // Empty path = neutral (no styling). Non-empty but missing/unreadable =
    // red border + warning tooltip. Valid = cleared styling, tooltip = the path.
    // Returns true if the path is empty or valid, false if invalid.
    bool updateImagePathValidity(const QString& path);

    // Folds m_mergeGen into `base` and restarts the idle timer. Used to bound
    // how long consecutive spinbox/text edits merge into a single undo step:
    // a pause > 600ms bumps m_mergeGen, starting a fresh undo entry.
    int mergeTag(int base);

    TitleDocument* m_doc{nullptr};
    std::string    m_elementId;
    bool           m_updating{false};

    int     m_mergeGen{0};
    QTimer* m_mergeIdleTimer{nullptr};

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

    // Paint Style group (Style tab)
    QAction* m_copyStyleAct{nullptr};

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
    QCheckBox* m_clipChildrenCheck{nullptr};
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
    QComboBox*      m_ellipsize{nullptr};
    QComboBox*      m_wrap{nullptr};
    QComboBox*      m_textTransform{nullptr};
    QToolButton*    m_textBtn{nullptr};

    // Spacing group
    QDoubleSpinBox* m_lineSpacing{nullptr};
    QDoubleSpinBox* m_charSpacing{nullptr};

    // Shared text content editor
    QDialog*        m_contentDialog{nullptr};
    QPlainTextEdit* m_contentEdit{nullptr};
    int             m_contentSavedCursor{0};

    // Focus restoration for non-modal Qt::Tool popup dialogs
    QPointer<QWidget> m_focusBeforeFillGradDlg;
    QPointer<QWidget> m_focusBeforeStrokeGradDlg;
    QPointer<QWidget> m_focusBeforeContentDlg;

    // Image tab
    QLineEdit* m_imagePathEdit{nullptr};
    QComboBox* m_scaleMode{nullptr};
};
