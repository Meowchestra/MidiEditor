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

#ifndef TIMESIGNATUREDIALOG_H_
#define TIMESIGNATUREDIALOG_H_

// Qt includes
#include <QDialog>
#include <QRadioButton>
#include <QSpinBox>
#include <QComboBox>

// Project includes
#include "../midi/MidiFile.h"

/**
 * \class TimeSignatureDialog
 *
 * \brief Dialog for creating and editing time signature events.
 *
 * TimeSignatureDialog provides a user interface for setting time signature
 * parameters when creating or modifying time signature events in the MIDI file.
 * It allows users to specify:
 *
 * - **Beats per measure**: The numerator of the time signature (e.g., 4 in 4/4)
 * - **Beat type**: The denominator of the time signature (e.g., 4 in 4/4)
 * - **Scope**: Whether the change applies until the next change or end of piece
 *
 * The dialog provides intuitive controls for common time signatures while
 * allowing flexibility for custom time signatures.
 */
class TimeSignatureDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * \brief Creates a new TimeSignatureDialog.
     * \param file The MidiFile to add the time signature to
     * \param measure The measure number where the time signature will be placed
     * \param startTickOfMeasure The MIDI tick at the start of the measure
     * \param parent The parent widget
     */
    TimeSignatureDialog(MidiFile *file, int measure, int startTickOfMeasure, QWidget *parent = 0);

public slots:
    /**
     * \brief Accepts the dialog and creates the time signature event.
     */
    void accept();

private:
    /** \brief Spin box for beats per measure (numerator) */
    QSpinBox *_beats;

    /** \brief Combo box for beat type (denominator) */
    QComboBox *_beatType;

    /** \brief Radio buttons for time signature scope */
    QRadioButton *_untilNextMeterChange, *_endOfPiece;

    /** \brief The MIDI file to modify */
    MidiFile *_file;

    /** \brief Measure number and timing information */
    int _measure, _startTickOfMeasure;
};

#endif // TIMESIGNATUREDIALOG_H_
