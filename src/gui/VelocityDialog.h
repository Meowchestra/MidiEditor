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

#ifndef VELOCITYDIALOG_H_
#define VELOCITYDIALOG_H_

#include <QDialog>
#include <QList>

class NoteOnEvent;
class MidiFile;
class QSpinBox;

/**
 * \class VelocityDialog
 *
 * \brief Provides a user interface for setting the velocity of MIDI notes.
 */
class VelocityDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * \brief Creates a new VelocityDialog.
     * \param toUpdate List of NoteOnEvents to update
     * \param file The MIDI file containing the events
     * \param parent The parent widget
     */
    VelocityDialog(QList<NoteOnEvent *> toUpdate, MidiFile *file, QWidget *parent = 0);

public slots:
    /**
     * \brief Applies the velocity change and closes the dialog.
     */
    void accept() override;

private:
    /** \brief The list of NoteOnEvents to update */
    QList<NoteOnEvent *> _toUpdate;

    /** \brief The parent MIDI file */
    MidiFile *_file;

    /** \brief Spin box for entering the velocity value */
    QSpinBox *_valueBox;
};

#endif // VELOCITYDIALOG_H_
