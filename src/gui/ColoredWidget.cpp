#include "ColoredWidget.h"
#include "Appearance.h"

#include <QPainter>

ColoredWidget::ColoredWidget(QColor color, QWidget *parent)
    : QWidget(parent) {
    _color = color;
    setFixedWidth(30);
    setContentsMargins(0, 0, 0, 0);
}

void ColoredWidget::paintEvent(QPaintEvent *event) {
    QPainter p;
    // Leave 1px margin on each side for the pen stroke to prevent clipping
    int l = width() - 3;
    int x = 1;
    int y = (height() - l) / 2;
    if (l > height() - 3) {
        l = height() - 3;
        y = 1;
        x = (width() - l) / 2;
    }

    p.begin(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Don't fill background - let it be transparent to match container
    p.setPen(Appearance::borderColor());
    p.setBrush(_color);
    p.drawRoundedRect(x, y, l, l, 30, 30);
    p.end();
}
