/*
 * MidiEditor
 *
 * Control Change Settings Widget
 */

#ifndef CONTROLCHANGESETTINGSWIDGET_H_
#define CONTROLCHANGESETTINGSWIDGET_H_

#include "SettingsWidget.h"

class QTableWidget;
class QTableWidgetItem;
class QSettings;

class ControlChangeSettingsWidget : public SettingsWidget {
    Q_OBJECT
public:
    ControlChangeSettingsWidget(QSettings *settings, QWidget *parent = 0);

    virtual bool accept();

public slots:
    void refreshColors();
    void onTableItemChanged(QTableWidgetItem *item);
    void clearSettings();

private:
    void populateTable();

    QSettings *_settings;
    QTableWidget *_tableWidget;
    QWidget *_infoBox;
};

#endif // CONTROLCHANGESETTINGSWIDGET_H_
