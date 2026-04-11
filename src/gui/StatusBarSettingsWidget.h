#ifndef STATUSBARSETTINGSWIDGET_H_
#define STATUSBARSETTINGSWIDGET_H_

#include "SettingsWidget.h"

class QCheckBox;
class QComboBox;
class QSpinBox;

class StatusBarSettingsWidget : public SettingsWidget {
    Q_OBJECT

public:
    StatusBarSettingsWidget(QWidget *parent = 0);

    virtual bool accept() override;

signals:
    void statusBarSettingsChanged();

private slots:
    void strategyChanged(int index);
    void notifyChanged();

private:
    QComboBox *_strategyCombo;
    QSpinBox *_toleranceSpin;
    QCheckBox *_showStatusBar;
    QComboBox *_alignmentCombo;
    QCheckBox *_showTrackChannel;
    QCheckBox *_showNoteName;
    QCheckBox *_showNoteRange;
    QCheckBox *_showChordName;
};

#endif // STATUSBARSETTINGSWIDGET_H_
