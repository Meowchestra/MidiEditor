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

#include "TransposeDialog.h"
#include "MainWindow.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QRadioButton>
#include <QSpinBox>

#include "../MidiEvent/NoteOnEvent.h"
#include "../midi/MidiFile.h"
#include "../protocol/Protocol.h"

TransposeDialog::TransposeDialog(QList<NoteOnEvent *> toTranspose, MidiFile *file, QWidget *parent) {
    setWindowTitle(tr("Transpose Selection"));

    _toTranspose = toTranspose;
    _file = file;

    QLabel *text = new QLabel(tr("Number of semitones: "), this);
    _valueBox = new QSpinBox(this);
    _valueBox->setMinimum(0);
    _valueBox->setMaximum(127);
    _valueBox->setValue(0);

    QButtonGroup *group = new QButtonGroup(this);
    _up = new QRadioButton(tr("Up"), this);
    _down = new QRadioButton(tr("Down"), this);
    _up->setChecked(true);
    group->addButton(_up);
    group->addButton(_down);

    QPushButton *breakButton = new QPushButton(tr("Cancel"), this);
    connect(breakButton, SIGNAL(clicked()), this, SLOT(hide()));
    QPushButton *acceptButton = new QPushButton(tr("Accept"), this);
    connect(acceptButton, SIGNAL(clicked()), this, SLOT(accept()));

    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(text, 0, 0, 1, 1);
    layout->addWidget(_valueBox, 0, 1, 1, 2);
    layout->addWidget(_up, 1, 0, 1, 1);
    layout->addWidget(_down, 1, 2, 1, 1);
    layout->addWidget(acceptButton, 2, 0, 1, 1);
    layout->addWidget(breakButton, 2, 2, 1, 1);
    layout->setColumnStretch(1, 1);

    _valueBox->setFocus();
}

void TransposeDialog::accept() {
    int semitones = _valueBox->value();
    if (_down->isChecked()) {
        semitones *= -1;
    }

    if (semitones != 0) {
        QString actionName = tr("Transpose Selection");
        if (semitones == 12) actionName = tr("Transpose Octave Up");
        else if (semitones == -12) actionName = tr("Transpose Octave Down");

        _file->protocol()->startNewAction(actionName, new QImage(":/run_environment/graphics/tool/transpose.png"));
        foreach(NoteOnEvent* e, _toTranspose) {
            e->setKey(e->key() + semitones);
        }
        _file->protocol()->endAction();
    }
    hide();
}
