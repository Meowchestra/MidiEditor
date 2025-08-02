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

#ifndef FILELENGTHDIALOG_H_
#define FILELENGTHDIALOG_H_

// Qt includes
#include <QDialog>

// Forward declarations
class MidiFile;
class QSpinBox;

/**
 * \class FileLengthDialog
 *
 * \brief Dialog for setting the maximum length of a MIDI file.
 *
 * FileLengthDialog allows users to specify the maximum duration of a MIDI file
 * in milliseconds. This setting affects:
 *
 * - **File duration**: Sets the total playback length
 * - **Timeline display**: Determines the extent of the timeline
 * - **Export behavior**: Controls the length of exported files
 * - **Memory allocation**: Affects internal buffer sizes
 *
 * The dialog provides a simple spin box interface for entering the desired
 * length in milliseconds, with validation to ensure reasonable values.
 */
class FileLengthDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * \brief Creates a new FileLengthDialog.
     * \param f The MidiFile to set the length for
     * \param parent The parent widget
     */
    FileLengthDialog(MidiFile *f, QWidget *parent = 0);

public slots:
    /**
     * \brief Accepts the dialog and applies the new file length.
     */
    void accept();

private:
    /** \brief The MIDI file to modify */
    MidiFile *_file;

    /** \brief Spin box for entering the length in milliseconds */
    QSpinBox *_box;
};

#endif // FILELENGTHDIALOG_H_
