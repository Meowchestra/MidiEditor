#ifndef APPEARANCESETTINGSWIDGET_H
#define APPEARANCESETTINGSWIDGET_H

#include <QWidget>
#include "SettingsWidget.h"
#include "ColoredWidget.h"
#include <QList>

class QCheckBox;

class NamedColorWidgetItem : public QWidget {

    Q_OBJECT

public:
    NamedColorWidgetItem(int number, QString name, QColor color, QWidget* parent = 0);
    int number();

signals:
    void colorChanged(int number, QColor c);

public slots:
    void colorChanged(QColor color);

protected:
    void mousePressEvent(QMouseEvent* event);

private:
    int _number;
    QColor color;
    ColoredWidget* colored;
};

class AppearanceSettingsWidget : public SettingsWidget {

    Q_OBJECT

public:
    AppearanceSettingsWidget(QWidget* parent = 0);

public slots:
    void channelColorChanged(int channel, QColor c);
    void trackColorChanged(int track, QColor c);
    void resetColors();
    void refreshColors(); // Refresh colors when they're auto-updated
    void opacityChanged(int opacity);
    void stripStyleChanged(int strip);
    void rangeLinesChanged(bool enabled);
    void styleChanged(const QString& style);

private:
    QList<NamedColorWidgetItem*> *_channelItems;
    QList<NamedColorWidgetItem*> *_trackItems;
};

#endif // APPEARANCESETTINGSWIDGET_H
