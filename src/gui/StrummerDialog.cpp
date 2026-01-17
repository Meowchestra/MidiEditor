/*
 * MidiEditor
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "StrummerDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

StrummerDialog::StrummerDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("Strummer"));
    setModal(true);

    setupUI();
    setupConnections();
    
    // Set defaults
    _startStrengthSpin->setValue(30); // Default small strum
    _startTensionSpin->setValue(0.0); // Linear
    _endStrengthSpin->setValue(0);
    _endTensionSpin->setValue(0.0);
    _velocityStrengthSpin->setValue(0);
    _velocityTensionSpin->setValue(0.0);
    _preserveEndCheck->setChecked(false);
    _alternateDirectionCheck->setChecked(false);
    _useStepStrengthCheck->setChecked(false);
    _ignoreTrackCheck->setChecked(false);
    
    setFixedSize(sizeHint());
}

void StrummerDialog::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Start Timing Group
    QGroupBox *startGroup = new QGroupBox(tr("Start (Timing)"), this);
    QGridLayout *startLayout = new QGridLayout(startGroup);
    
    startLayout->addWidget(new QLabel(tr("Strength (ms):"), startGroup), 0, 0);
    _startStrengthSpin = new QSpinBox(startGroup);
    _startStrengthSpin->setRange(-5000, 5000); // Range in ms
    _startStrengthSpin->setToolTip(tr("Offset in milliseconds applied to the start of the notes.\nPositive values strum from low to high pitch.\nNegative values strum from high to low pitch."));
    startLayout->addWidget(_startStrengthSpin, 0, 1);
    
    startLayout->addWidget(new QLabel(tr("Tension (-1.0 to 1.0):"), startGroup), 1, 0);
    _startTensionSpin = new QDoubleSpinBox(startGroup);
    _startTensionSpin->setRange(-1.0, 1.0);
    _startTensionSpin->setSingleStep(0.1);
    _startTensionSpin->setToolTip(tr("Controls the acceleration of the strum.\n0 is linear.\nPositive values accelerate (starts slow, ends fast).\nNegative values decelerate (starts fast, ends slow)."));
    startLayout->addWidget(_startTensionSpin, 1, 1);
    
    mainLayout->addWidget(startGroup);
    
    // End Timing Group
    QGroupBox *endGroup = new QGroupBox(tr("End (Timing)"), this);
    QGridLayout *endLayout = new QGridLayout(endGroup);
    
    endLayout->addWidget(new QLabel(tr("Strength (ms):"), endGroup), 0, 0);
    _endStrengthSpin = new QSpinBox(endGroup);
    _endStrengthSpin->setRange(-5000, 5000); // Range in ms
    _endStrengthSpin->setToolTip(tr("Offset in milliseconds applied to the end of the notes.\nPositive values strum from low to high pitch.\nNegative values strum from high to low pitch."));
    endLayout->addWidget(_endStrengthSpin, 0, 1);
    
    endLayout->addWidget(new QLabel(tr("Tension (-1.0 to 1.0):"), endGroup), 1, 0);
    _endTensionSpin = new QDoubleSpinBox(endGroup);
    _endTensionSpin->setRange(-1.0, 1.0);
    _endTensionSpin->setSingleStep(0.1);
    _endTensionSpin->setToolTip(tr("Controls the acceleration of the end offset.\n0 is linear.\nPositive values accelerate (starts slow, ends fast).\nNegative values decelerate (starts fast, ends slow)."));
    endLayout->addWidget(_endTensionSpin, 1, 1);
    
    mainLayout->addWidget(endGroup);

    // Velocity Group
    QGroupBox *velocityGroup = new QGroupBox(tr("Velocity"), this);
    QGridLayout *velocityLayout = new QGridLayout(velocityGroup);
    
    velocityLayout->addWidget(new QLabel(tr("Strength:"), velocityGroup), 0, 0);
    _velocityStrengthSpin = new QSpinBox(velocityGroup);
    _velocityStrengthSpin->setRange(-127, 127);
    _velocityStrengthSpin->setToolTip(tr("Velocity change applied across the strum.\nPositive: increases velocity (crescendo).\nNegative: decreases velocity (diminuendo)."));
    velocityLayout->addWidget(_velocityStrengthSpin, 0, 1);
    
    velocityLayout->addWidget(new QLabel(tr("Tension:"), velocityGroup), 1, 0);
    _velocityTensionSpin = new QDoubleSpinBox(velocityGroup);
    _velocityTensionSpin->setRange(-1.0, 1.0);
    _velocityTensionSpin->setSingleStep(0.1);
    _velocityTensionSpin->setToolTip(tr("Controls the curve of velocity change."));
    velocityLayout->addWidget(_velocityTensionSpin, 1, 1);
    
    mainLayout->addWidget(velocityGroup);
    
    // Options Group
    QGroupBox *optionsGroup = new QGroupBox(tr("Options"), this);
    QVBoxLayout *optionsLayout = new QVBoxLayout(optionsGroup);
    
    _preserveEndCheck = new QCheckBox(tr("Preserve end"), optionsGroup);
    _preserveEndCheck->setToolTip(tr("If checked, note endings are fixed, so changing start time changes note duration.\nIf unchecked, note duration is preserved (end moves with start)."));
    optionsLayout->addWidget(_preserveEndCheck);
    
    _alternateDirectionCheck = new QCheckBox(tr("Alternate direction"), optionsGroup);
    _alternateDirectionCheck->setToolTip(tr("If checked, strum direction alternates (Up, Down, Up...) for consecutive chords."));
    optionsLayout->addWidget(_alternateDirectionCheck);
    
    _useStepStrengthCheck = new QCheckBox(tr("Relative strength (per note)"), optionsGroup);
    _useStepStrengthCheck->setToolTip(tr("If checked, the strength value is applied per note step (e.g. 0, 1s, 2s...).\nIf unchecked, strength is the total range (e.g. 0, 0.33s, 0.66s, 1s)."));
    optionsLayout->addWidget(_useStepStrengthCheck);

    _ignoreTrackCheck = new QCheckBox(tr("Strum across tracks"), optionsGroup);
    _ignoreTrackCheck->setToolTip(tr("If checked, selected notes from different tracks are treated as a single chord.\nNotes keep their original track assignment."));
    optionsLayout->addWidget(_ignoreTrackCheck);
    
    mainLayout->addWidget(optionsGroup);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    _cancelButton = new QPushButton(tr("Cancel"), this);
    _okButton = new QPushButton(tr("OK"), this);
    _okButton->setDefault(true);

    buttonLayout->addWidget(_okButton);
    buttonLayout->addWidget(_cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void StrummerDialog::setupConnections() {
    connect(_okButton, SIGNAL(clicked()), this, SLOT(onOkClicked()));
    connect(_cancelButton, SIGNAL(clicked()), this, SLOT(onCancelClicked()));
}

int StrummerDialog::getStartStrength() const {
    return _startStrengthSpin->value();
}

double StrummerDialog::getStartTension() const {
    return _startTensionSpin->value();
}

int StrummerDialog::getEndStrength() const {
    return _endStrengthSpin->value();
}

double StrummerDialog::getEndTension() const {
    return _endTensionSpin->value();
}

int StrummerDialog::getVelocityStrength() const {
    return _velocityStrengthSpin->value();
}

double StrummerDialog::getVelocityTension() const {
    return _velocityTensionSpin->value();
}

bool StrummerDialog::getPreserveEnd() const {
    return _preserveEndCheck->isChecked();
}

bool StrummerDialog::getAlternateDirection() const {
    return _alternateDirectionCheck->isChecked();
}

bool StrummerDialog::getUseStepStrength() const {
    return _useStepStrengthCheck->isChecked();
}

bool StrummerDialog::getIgnoreTrack() const {
    return _ignoreTrackCheck->isChecked();
}

void StrummerDialog::onOkClicked() {
    accept();
}

void StrummerDialog::onCancelClicked() {
    reject();
}
