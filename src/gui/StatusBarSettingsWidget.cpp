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

    _showStatusBar = new QCheckBox(tr("Status Bar"), this);

    _alignmentCombo = new QComboBox(this);
    _alignmentCombo->addItem(tr("Align Left"), 0);
    _alignmentCombo->addItem(tr("Align Right"), 1);

    layout->addRow(_showStatusBar, _alignmentCombo);

    layout->addRow(separator());

    _showTrackChannel = new QCheckBox(tr("Show Track / Channel Info"), this);
    layout->addRow(_showTrackChannel);

    _showNoteName = new QCheckBox(tr("Show Note Info (Name / Count)"), this);
    layout->addRow(_showNoteName);

    _showNoteRange = new QCheckBox(tr("Show Selection Range"), this);
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

    // Load current values directly from QSettings (not through dialog cast,
    // since widget may not be fully parented to the dialog yet at construction time)
    QSettings settings("MidiEditor", "NONE");
    settings.beginGroup("status_bar");
    _strategyCombo->setCurrentIndex(settings.value("strategy", 0).toInt());
    _toleranceSpin->setValue(settings.value("tolerance", 10).toInt());
    // Default to true to match MainWindow's default
    _showStatusBar->setChecked(settings.value("visible", true).toBool());
    _alignmentCombo->setCurrentIndex(settings.value("alignment", 0).toInt());
    _showTrackChannel->setChecked(settings.value("show_track_channel", true).toBool());
    _showNoteName->setChecked(settings.value("show_note_name", true).toBool());
    _showNoteRange->setChecked(settings.value("show_note_range", true).toBool());
    _showChordName->setChecked(settings.value("show_chord_name", true).toBool());
    settings.endGroup();

    connect(_strategyCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(strategyChanged(int)));
    connect(_strategyCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(notifyChanged()));
    connect(_toleranceSpin, SIGNAL(valueChanged(int)), this, SLOT(notifyChanged()));
    connect(_showStatusBar, SIGNAL(toggled(bool)), this, SLOT(notifyChanged()));
    connect(_alignmentCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(notifyChanged()));
    connect(_showTrackChannel, SIGNAL(toggled(bool)), this, SLOT(notifyChanged()));
    connect(_showNoteName, SIGNAL(toggled(bool)), this, SLOT(notifyChanged()));
    connect(_showNoteRange, SIGNAL(toggled(bool)), this, SLOT(notifyChanged()));
    connect(_showChordName, SIGNAL(toggled(bool)), this, SLOT(notifyChanged()));

    strategyChanged(_strategyCombo->currentIndex());
}

void StatusBarSettingsWidget::notifyChanged() {
    // Write settings directly to QSettings (same instance used by the app)
    QSettings settings("MidiEditor", "NONE");
    settings.beginGroup("status_bar");
    settings.setValue("strategy", _strategyCombo->currentIndex());
    settings.setValue("tolerance", _toleranceSpin->value());
    settings.setValue("visible", _showStatusBar->isChecked());
    settings.setValue("alignment", _alignmentCombo->currentIndex());
    settings.setValue("show_track_channel", _showTrackChannel->isChecked());
    settings.setValue("show_note_name", _showNoteName->isChecked());
    settings.setValue("show_note_range", _showNoteRange->isChecked());
    settings.setValue("show_chord_name", _showChordName->isChecked());
    settings.endGroup();
    settings.sync(); // Force write to registry/disk immediately

    emit statusBarSettingsChanged();
}

void StatusBarSettingsWidget::strategyChanged(int index) {
    _toleranceSpin->setEnabled(index == 2);
}

bool StatusBarSettingsWidget::accept() {
    // Write settings on accept as well
    QSettings settings("MidiEditor", "NONE");
    settings.beginGroup("status_bar");
    settings.setValue("strategy", _strategyCombo->currentIndex());
    settings.setValue("tolerance", _toleranceSpin->value());
    settings.setValue("visible", _showStatusBar->isChecked());
    settings.setValue("alignment", _alignmentCombo->currentIndex());
    settings.setValue("show_track_channel", _showTrackChannel->isChecked());
    settings.setValue("show_note_name", _showNoteName->isChecked());
    settings.setValue("show_note_range", _showNoteRange->isChecked());
    settings.setValue("show_chord_name", _showChordName->isChecked());
    settings.endGroup();
    settings.sync(); // Force write to registry/disk immediately

    return true;
}
