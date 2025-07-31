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
    int l = width() - 1;
    int x = 0;
    int y = (height() - 1 - l) / 2;
    if (l > height() - 1) {
        l = height() - 1;
        y = 0;
        x = (width() - 1 - l) / 2;
    }

    p.begin(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Don't fill background - let it be transparent to match container
    p.setPen(Appearance::borderColor());
    p.setBrush(_color);
    p.drawRoundedRect(x, y, l, l, 30, 30);
    p.end();
}
