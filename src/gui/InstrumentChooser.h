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

#ifndef INSTRUMENTCHOOSER_H_
#define INSTRUMENTCHOOSER_H_

// Qt includes
#include <QDialog>

// Forward declarations
class MidiFile;
class QComboBox;
class QCheckBox;

/**
 * \class InstrumentChooser
 *
 * \brief Dialog for selecting MIDI instruments for channels.
 *
 * InstrumentChooser provides a user-friendly interface for selecting
 * General MIDI instruments for specific MIDI channels. It features:
 *
 * - **Instrument selection**: Dropdown list of all 128 General MIDI instruments
 * - **Channel assignment**: Associates the selected instrument with a specific channel
 * - **Program change creation**: Automatically creates program change events
 * - **Cleanup option**: Option to remove other program changes on the channel
 * - **Preview support**: Can preview the selected instrument
 *
 * The dialog displays instrument names in a logical order and provides
 * options for managing existing program changes on the channel.
 */
class InstrumentChooser : public QDialog {
    Q_OBJECT

public:
    /**
     * \brief Creates a new InstrumentChooser.
     * \param f The MidiFile to add the program change to
     * \param channel The MIDI channel to set the instrument for
     * \param parent The parent widget
     */
    InstrumentChooser(MidiFile *f, int channel, QWidget *parent = 0);

public slots:
    /**
     * \brief Accepts the dialog and applies the selected instrument.
     */
    void accept();

private:
    /** \brief The MIDI file to modify */
    MidiFile *_file;

    /** \brief Combo box for instrument selection */
    QComboBox *_box;

    /** \brief Checkbox for removing other program changes */
    QCheckBox *_removeOthers;

    /** \brief The MIDI channel number */
    int _channel;
};

#endif // INSTRUMENTCHOOSER_H_
