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

#include "VelocityDialog.h"
#include "MainWindow.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QSpinBox>

#include "../MidiEvent/NoteOnEvent.h"
#include "../midi/MidiFile.h"
#include "../protocol/Protocol.h"

VelocityDialog::VelocityDialog(QList<NoteOnEvent *> toUpdate, MidiFile *file, QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Set Velocity"));

    _toUpdate = toUpdate;
    _file = file;

    QLabel *text = new QLabel(tr("Velocity (0-127): "), this);
    _valueBox = new QSpinBox(this);
    _valueBox->setMinimum(0);
    _valueBox->setMaximum(127);
    
    // Set default value to the velocity of the first note in selection if available
    if (!toUpdate.isEmpty()) {
        _valueBox->setValue(toUpdate.first()->velocity());
    } else {
        _valueBox->setValue(100);
    }

    QPushButton *breakButton = new QPushButton(tr("Cancel"), this);
    connect(breakButton, SIGNAL(clicked()), this, SLOT(hide()));
    QPushButton *acceptButton = new QPushButton(tr("Accept"), this);
    connect(acceptButton, SIGNAL(clicked()), this, SLOT(accept()));

    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(text, 0, 0, 1, 1);
    layout->addWidget(_valueBox, 0, 1, 1, 2);
    layout->addWidget(acceptButton, 1, 0, 1, 1);
    layout->addWidget(breakButton, 1, 2, 1, 1);
    layout->setColumnStretch(1, 1);

    _valueBox->setFocus();
    _valueBox->selectAll();
}

void VelocityDialog::accept() {
    int velocity = _valueBox->value();

    if (!_toUpdate.isEmpty()) {
        _file->protocol()->startNewAction(tr("Set Velocity"));
        foreach(NoteOnEvent* e, _toUpdate) {
            e->setVelocity(velocity);
        }
        _file->protocol()->endAction();
    }
    hide();
}
