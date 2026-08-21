#pragma once
#include <QColor>
#include <QImage>
#include <QWidget>
#include <array>

class ColorWheel : public QWidget {
    Q_OBJECT
    // No NOTIFY clause: colorChanged fires only on user interaction, never from
    // setColor(). Declaring it as the property's NOTIFY signal would promise
    // Qt's meta-object system that every WRITE notifies, which is false here —
    // a QPropertyAnimation or QML binding on "color" would silently miss
    // programmatic writes. Connect to colorChanged directly instead.
    Q_PROPERTY(QColor color READ color WRITE setColor)
public:
    explicit ColorWheel(QWidget* parent = nullptr);
    QColor color() const;
    void setColor(const QColor& c);
    QSize sizeHint() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int w) const override;
signals:
    void colorChanged(const QColor&);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    enum class DragZone { None, Ring, Triangle };

    QColor m_color{Qt::red};
    // Sticky hue, independent of m_color: QColor::hsvHue() returns -1 for any
    // fully desaturated colour (black/white/grey), so recomputing hue from
    // m_color loses the ring position whenever the S/V indicator is dragged
    // into a triangle corner. Updated only when an incoming colour has a
    // valid (>= 0) hue.
    int m_hue{0};
    DragZone m_drag{DragZone::None};
    QImage m_ringImage;
    QImage m_triangleImage;
    bool m_ringDirty{true};
    bool m_triangleDirty{true};
    int m_cachedTriangleHue{-1};

    void rebuildRing();
    void rebuildTriangle();

    QPointF center() const;
    qreal outerRadius() const;
    qreal innerRadius() const;

    bool pointInRing(QPointF p) const;
    bool pointInTriangle(QPointF p) const;

    void handleRingDrag(QPointF p);
    void handleTriangleDrag(QPointF p);

    std::array<QPointF, 3> triangleVertices() const;
    QPointF svPosition() const;
};
