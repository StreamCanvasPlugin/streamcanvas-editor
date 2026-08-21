#pragma once
#include "GradientStop.h"
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QVector>
#include <QWidget>

// Horizontal gradient strip with stop handles below it. This is the primary
// stop-editing surface for the paint editor. It knows only about
// QVector<GradientStop> — no Paint, TitleDocument, or undo commands. The
// panel that owns this widget is responsible for that conversion.
class GradientStopBar : public QWidget {
    Q_OBJECT
public:
    explicit GradientStopBar(QWidget* parent = nullptr);

    QVector<GradientStop> stops() const;
    void setStops(const QVector<GradientStop>& stops);

    int selectedStop() const; // index into stops(), or -1
    void setSelectedStop(int index);

    void setSelectedStopColor(const QColor& c); // used by the shared colour picker
    void setSelectedStopPosition(qreal pos);

    QSize sizeHint() const override;

signals:
    void stopsChanged(const QVector<GradientStop>& stops);
    void stopSelected(int index);
    void interactionStarted();
    void interactionFinished();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    // Stops keep a stable internal id so a drag/sort/keyboard action can keep
    // tracking the SAME stop even after positions move past each other.
    struct Stop {
        int id;
        GradientStop stop;
    };

    static constexpr int kStripH = 24;
    static constexpr int kHandleR = 10; // 20px diameter handle, satisfies the 20px hit-target minimum
    static constexpr int kGap = 4;
    static constexpr int kMarginY = 4;
    static constexpr int kMarginX = kHandleR + 2;
    static constexpr int kCheckerCell = 6;
    static constexpr qreal kDragThresholdPx = 4.0;

    QVector<Stop> m_stops;
    int m_nextId{0};
    int m_selectedId{-1};
    int m_hoverId{-1};
    bool m_pressed{false};
    bool m_dragging{false};
    int m_pressId{-1};
    QPointF m_pressPos;

    QRect stripRect() const;
    QPointF handleCenter(const Stop& s) const;
    qreal xToPosition(qreal x) const;
    int hitTestHandle(QPointF p) const;
    int indexOfId(int id) const;
    int indexOfSelected() const;
    void selectByIndexInternal(int index, bool emitSignal);
    void sortByPosition();
    void addStopAt(qreal t);
    void drawCheckerboard(QPainter& p, const QRect& r) const;
};
