#pragma once
#include <QColor>

struct GradientStop {
    qreal position; // 0.0 – 1.0
    QColor color;
};
