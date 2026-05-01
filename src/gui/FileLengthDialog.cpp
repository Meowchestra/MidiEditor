/*
 * MidiEditor
 * Copyright (C) 2010  Markus Schwenk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "FileLengthDialog.h"

#include "../midi/MidiFile.h"
#include "../protocol/Protocol.h"
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

FileLengthDialog::FileLengthDialog(MidiFile *f, QWidget *parent)
    : QDialog(parent) {
    _file = f;
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Milliseconds Group
    QGroupBox *msGroup = new QGroupBox(tr("Total Duration"), this);
    QGridLayout *msLayout = new QGridLayout(msGroup);
    msLayout->addWidget(new QLabel(tr("Milliseconds: ")), 0, 0);
    _msBox = new QSpinBox(this);
    _msBox->setRange(1, 2147483647);
    _msBox->setValue(_file->maxTime());
    _msBox->setSuffix(" ms");
    _msBox->setMinimumHeight(28);
    _msBox->setMinimumWidth(150);
    msLayout->addWidget(_msBox, 0, 1);
    mainLayout->addWidget(msGroup);
    
    // Human Readable Group
    QGroupBox *hmsGroup = new QGroupBox(tr("Human Readable (HH:MM:SS.mmm)"), this);
    QHBoxLayout *hmsLayout = new QHBoxLayout(hmsGroup);
    
    _hBox = new QSpinBox(this);
    _hBox->setRange(0, 999);
    _hBox->setSuffix(" h");
    _hBox->setMinimumHeight(28);
    _hBox->setMinimumWidth(70);
    
    _mBox = new QSpinBox(this);
    _mBox->setRange(0, 59);
    _mBox->setSuffix(" m");
    _mBox->setMinimumHeight(28);
    _mBox->setMinimumWidth(70);
    
    _sBox = new QSpinBox(this);
    _sBox->setRange(0, 59);
    _sBox->setSuffix(" s");
    _sBox->setMinimumHeight(28);
    _sBox->setMinimumWidth(70);
    
    _msSubBox = new QSpinBox(this);
    _msSubBox->setRange(0, 999);
    _msSubBox->setSuffix(" ms");
    _msSubBox->setMinimumHeight(28);
    _msSubBox->setMinimumWidth(80);
    
    hmsLayout->addWidget(_hBox);
    hmsLayout->addWidget(new QLabel(":"));
    hmsLayout->addWidget(_mBox);
    hmsLayout->addWidget(new QLabel(":"));
    hmsLayout->addWidget(_sBox);
    hmsLayout->addWidget(new QLabel("."));
    hmsLayout->addWidget(_msSubBox);
    mainLayout->addWidget(hmsGroup);

    // Fit Buttons
    QHBoxLayout *fitLayout = new QHBoxLayout();
    QPushButton *fitEventButton = new QPushButton(tr("Fit to Last Event"), this);
    fitEventButton->setMinimumHeight(30);
    connect(fitEventButton, SIGNAL(clicked()), this, SLOT(fitToEvents()));
    
    QPushButton *fitMeasureButton = new QPushButton(tr("Fit to Last Measure"), this);
    fitMeasureButton->setMinimumHeight(30);
    connect(fitMeasureButton, SIGNAL(clicked()), this, SLOT(fitToLastMeasure()));
    
    fitLayout->addWidget(fitEventButton);
    fitLayout->addWidget(fitMeasureButton);
    mainLayout->addLayout(fitLayout);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *acceptButton = new QPushButton(tr("Accept"));
    connect(acceptButton, SIGNAL(clicked()), this, SLOT(accept()));
    QPushButton *breakButton = new QPushButton(tr("Cancel"));
    connect(breakButton, SIGNAL(clicked()), this, SLOT(hide()));
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(acceptButton);
    buttonLayout->addWidget(breakButton);
    mainLayout->addLayout(buttonLayout);

    // Sync connections
    connect(_msBox, SIGNAL(valueChanged(int)), this, SLOT(updateFromMs()));
    connect(_hBox, SIGNAL(valueChanged(int)), this, SLOT(updateFromHMS()));
    connect(_mBox, SIGNAL(valueChanged(int)), this, SLOT(updateFromHMS()));
    connect(_sBox, SIGNAL(valueChanged(int)), this, SLOT(updateFromHMS()));
    connect(_msSubBox, SIGNAL(valueChanged(int)), this, SLOT(updateFromHMS()));

    // Initialize HMS from MS
    updateFromMs();

    _msBox->setFocus();
    setWindowTitle(tr("File Duration"));
    setMinimumWidth(480);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
}

void FileLengthDialog::updateFromMs() {
    int totalMs = _msBox->value();
    int h = totalMs / 3600000;
    int m = (totalMs % 3600000) / 60000;
    int s = (totalMs % 60000) / 1000;
    int ms = totalMs % 1000;
    
    _hBox->blockSignals(true);
    _mBox->blockSignals(true);
    _sBox->blockSignals(true);
    _msSubBox->blockSignals(true);
    
    _hBox->setValue(h);
    _mBox->setValue(m);
    _sBox->setValue(s);
    _msSubBox->setValue(ms);
    
    _hBox->blockSignals(false);
    _mBox->blockSignals(false);
    _sBox->blockSignals(false);
    _msSubBox->blockSignals(false);
}

void FileLengthDialog::updateFromHMS() {
    long long totalMs = (long long)_hBox->value() * 3600000 +
                        (long long)_mBox->value() * 60000 +
                        (long long)_sBox->value() * 1000 +
                        _msSubBox->value();
    
    if (totalMs > 2147483647) totalMs = 2147483647;
    if (totalMs < 1) totalMs = 1;
    
    _msBox->blockSignals(true);
    _msBox->setValue((int)totalMs);
    _msBox->blockSignals(false);
}

void FileLengthDialog::fitToEvents() {
    int lastTick = _file->lastEventTick();
    int lastMs = _file->timeMS(lastTick);
    if (lastMs < 1) lastMs = 1;
    _msBox->setValue(lastMs);
}

void FileLengthDialog::fitToLastMeasure() {
    int lastTick = _file->lastEventTick();
    int start, end;
    _file->measure(lastTick, &start, &end);
    int lastMs = _file->timeMS(end);
    if (lastMs < 1) lastMs = 1;
    _msBox->setValue(lastMs);
}

void FileLengthDialog::accept() {
    _file->protocol()->startNewAction(tr("Changed File Duration"));
    _file->setMaxLengthMs(_msBox->value());
    _file->protocol()->endAction();
    hide();
}
