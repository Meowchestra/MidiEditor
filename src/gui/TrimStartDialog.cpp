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

#include "TrimStartDialog.h"

#include "../midi/MidiFile.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>

TrimStartDialog::TrimStartDialog(MidiFile *f, QWidget *parent)
    : QDialog(parent) {
    _file = f;

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    QLabel *info = new QLabel(tr("Choose how to trim blank time from the start of the file:"), this);
    info->setWordWrap(true);
    mainLayout->addWidget(info);

    QGroupBox *optionsGroup = new QGroupBox(tr("Trimming Options"), this);
    QVBoxLayout *optionsLayout = new QVBoxLayout(optionsGroup);

    _trimToNote = new QRadioButton(tr("Remove all blank time until first note"), optionsGroup);
    _trimToNote->setChecked(true);
    optionsLayout->addWidget(_trimToNote);

    _trimToMeasure = new QRadioButton(tr("Remove only empty measures at start"), optionsGroup);
    optionsLayout->addWidget(_trimToMeasure);

    mainLayout->addWidget(optionsGroup);

    QLabel *metaInfo = new QLabel(tr("Note: Meta events (Tempo, Time Signature, etc.) before the first note will be moved to the new start time."), this);
    metaInfo->setWordWrap(true);
    metaInfo->setStyleSheet("color: gray; font-size: 10px;");
    mainLayout->addWidget(metaInfo);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *acceptButton = new QPushButton(tr("Accept"));
    connect(acceptButton, SIGNAL(clicked()), this, SLOT(accept()));
    
    QPushButton *cancelButton = new QPushButton(tr("Cancel"));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(hide()));

    buttonLayout->addStretch();
    buttonLayout->addWidget(acceptButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    setWindowTitle(tr("Trim Blank Start"));
    setMinimumWidth(400);
}

void TrimStartDialog::accept() {
    int trimTick = 0;
    if (_trimToNote->isChecked()) {
        trimTick = _file->firstNoteTick();
    } else if (_trimToMeasure->isChecked()) {
        int firstMeasure = _file->firstNonEmptyMeasure();
        trimTick = _file->startTickOfMeasure(firstMeasure);
    }

    if (trimTick > 0) {
        _file->trimStart(trimTick);
    }
    
    hide();
}
