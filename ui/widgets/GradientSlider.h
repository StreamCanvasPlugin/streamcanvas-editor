#pragma once
#include <QSlider>

class QWheelEvent;

class GradientSlider : public QSlider {
    Q_OBJECT
public:
    explicit GradientSlider(QWidget* parent = nullptr);
    void setStops(const QGradientStops& stops);

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    QGradientStops m_stops;
};
