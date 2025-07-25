#ifndef APPLICATIONSETTINGSWIDGET_H
#define APPLICATIONSETTINGSWIDGET_H

#include "SettingsWidget.h"
#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>

class ApplicationSettingsWidget : public SettingsWidget {
    Q_OBJECT

public:
    ApplicationSettingsWidget(QWidget* parent = 0);

public slots:
    void ignoreScalingChanged(bool ignoreScaling);
    void roundedScalingChanged(bool useRounded);

private:
    // No private members needed for this simple widget
};

#endif // APPLICATIONSETTINGSWIDGET_H