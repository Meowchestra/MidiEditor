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

#ifndef TEMPODIALOG_H_
#define TEMPODIALOG_H_

// Qt includes
#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>

// Project includes
#include "../midi/MidiFile.h"

/**
 * \class TempoDialog
 *
 * \brief Dialog for creating and editing tempo change events.
 *
 * TempoDialog provides a user interface for setting tempo parameters
 * when creating or modifying tempo change events in the MIDI file.
 * It supports:
 *
 * - **Start tempo**: Initial BPM value
 * - **End tempo**: Final BPM value (for gradual tempo changes)
 * - **Smooth transition**: Gradual tempo change vs. instant change
 * - **Time range**: Specify the duration of tempo changes
 *
 * The dialog can create either instant tempo changes (single tempo event)
 * or gradual tempo changes (multiple tempo events over time) for musical
 * effects like accelerando and ritardando.
 */
class TempoDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * \brief Creates a new TempoDialog.
     * \param file The MidiFile to add tempo changes to
     * \param startTick The MIDI tick where the tempo change begins
     * \param endTick The MIDI tick where the tempo change ends (-1 for instant change)
     * \param parent The parent widget
     */
    TempoDialog(MidiFile *file, int startTick, int endTick = -1, QWidget *parent = 0);

public slots:
    /**
     * \brief Accepts the dialog and creates the tempo change event(s).
     */
    void accept();

private:
    /** \brief Spin boxes for start and end tempo values */
    QSpinBox *_endBeats, *_startBeats;

    /** \brief Checkbox for smooth tempo transition */
    QCheckBox *_smoothTransition;

    /** \brief Time range for the tempo change */
    int _startTick, _endTick;

    /** \brief The MIDI file to modify */
    MidiFile *_file;
};

#endif // TEMPODIALOG_H_
