#ifndef INSTRUMENTSETTINGSWIDGET_H_
#define INSTRUMENTSETTINGSWIDGET_H_

#include "SettingsWidget.h"

class QSettings;
class QLineEdit;
class QComboBox;

class QTableWidget;
class QTableWidgetItem;

class InstrumentSettingsWidget : public SettingsWidget {
    Q_OBJECT

public:
    InstrumentSettingsWidget(QSettings *settings, QWidget *parent = 0);
    bool accept();

public slots:
    void browseFile();
    void clearSettings();
    void loadFile();
    void instrumentChanged(int index);
    void refreshColors();
    void onTableItemChanged(QTableWidgetItem *item);

private:
    void populateTable();

    QSettings *_settings;
    QLineEdit *_fileEdit;
    QComboBox *_instrumentBox;
    QTableWidget *_tableWidget;
    QWidget *_infoBox;
};

#endif // INSTRUMENTSETTINGSWIDGET_H_
