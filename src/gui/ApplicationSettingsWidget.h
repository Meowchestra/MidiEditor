#ifndef APPLICATIONSETTINGSWIDGET_H
#define APPLICATIONSETTINGSWIDGET_H

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>

class ApplicationSettingsWidget : public QWidget {
    Q_OBJECT

public:
    ApplicationSettingsWidget(QWidget* parent = 0);

public slots:
    void ignoreScalingChanged(bool ignore);
    void roundedScalingChanged(bool useRounded);

private:
    // No private members needed for this simple widget
};

#endif // APPLICATIONSETTINGSWIDGET_H
