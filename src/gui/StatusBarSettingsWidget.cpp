#include "StatusBarSettingsWidget.h"

#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QSettings>
#include "SettingsDialog.h"

StatusBarSettingsWidget::StatusBarSettingsWidget(QWidget *parent)
    : SettingsWidget(tr("Status Bar"), parent) {
    QFormLayout *layout = new QFormLayout(this);
    setLayout(layout);

    _showStatusBar = new QCheckBox(tr("Show Status Bar"), this);
    layout->addRow(_showStatusBar);

    layout->addRow(separator());

    _showTrackChannel = new QCheckBox(tr("Show Track and Channel Info"), this);
    layout->addRow(_showTrackChannel);

    _showNoteName = new QCheckBox(tr("Show Note Name (Single Selection)"), this);
    layout->addRow(_showNoteName);

    _showNoteRange = new QCheckBox(tr("Show Note Range"), this);
    layout->addRow(_showNoteRange);

    _showChordName = new QCheckBox(tr("Show Chord Name"), this);
    layout->addRow(_showChordName);

    layout->addRow(separator());

    _strategyCombo = new QComboBox(this);
    _strategyCombo->addItem(tr("Strict (Same Start Time)"), 0);
    _strategyCombo->addItem(tr("Flexible (Entire Selection)"), 1);
    _strategyCombo->addItem(tr("Humanized (Staggered Window)"), 2);
    layout->addRow(tr("Chord Detection Strategy"), _strategyCombo);

    _toleranceSpin = new QSpinBox(this);
    _toleranceSpin->setRange(0, 1000);
    _toleranceSpin->setSuffix(tr(" ticks"));
    layout->addRow(tr("Humanization Tolerance"), _toleranceSpin);

    // Load current values
    SettingsDialog *dialog = qobject_cast<SettingsDialog *>(parent);
    if (dialog) {
        QSettings *settings = dialog->settings();
        settings->beginGroup("status_bar");
        _strategyCombo->setCurrentIndex(settings->value("strategy", 0).toInt());
        _toleranceSpin->setValue(settings->value("tolerance", 10).toInt());
        _showStatusBar->setChecked(settings->value("visible", true).toBool());
        _showTrackChannel->setChecked(settings->value("show_track_channel", true).toBool());
        _showNoteName->setChecked(settings->value("show_note_name", true).toBool());
        _showNoteRange->setChecked(settings->value("show_note_range", true).toBool());
        _showChordName->setChecked(settings->value("show_chord_name", true).toBool());
        settings->endGroup();
    }

    connect(_strategyCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(strategyChanged(int)));
    strategyChanged(_strategyCombo->currentIndex());
}

void StatusBarSettingsWidget::strategyChanged(int index) {
    _toleranceSpin->setEnabled(index == 2);
}

bool StatusBarSettingsWidget::accept() {
    SettingsDialog *dialog = qobject_cast<SettingsDialog *>(parent());
    if (dialog) {
        QSettings *settings = dialog->settings();
        settings->beginGroup("status_bar");
        settings->setValue("strategy", _strategyCombo->currentIndex());
        settings->setValue("tolerance", _toleranceSpin->value());
        settings->setValue("visible", _showStatusBar->isChecked());
        settings->setValue("show_track_channel", _showTrackChannel->isChecked());
        settings->setValue("show_note_name", _showNoteName->isChecked());
        settings->setValue("show_note_range", _showNoteRange->isChecked());
        settings->setValue("show_chord_name", _showChordName->isChecked());
        settings->endGroup();
    }
    return true;
}
