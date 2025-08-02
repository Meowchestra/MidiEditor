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

#ifndef RECORDDIALOG_H_
#define RECORDDIALOG_H_

// Qt includes
#include <QDialog>
#include <QMultiMap>

// Forward declarations
class MidiFile;
class MidiEvent;
class QCheckBox;
class QComboBox;
class QListWidget;
class QSettings;

/**
 * \class RecordDialog
 *
 * \brief Dialog for configuring MIDI recording settings and importing recorded data.
 *
 * RecordDialog appears after MIDI recording is complete and allows users to
 * configure how the recorded MIDI data should be imported into the project:
 *
 * - **Channel assignment**: Select which MIDI channel to use for recorded events
 * - **Track assignment**: Choose which track to add the recorded events to
 * - **Event filtering**: Select which types of events to import
 * - **Settings persistence**: Remember user preferences for future recordings
 *
 * The dialog provides a user-friendly interface for processing recorded MIDI
 * data and integrating it into the existing project structure.
 */
class RecordDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * \brief Creates a new RecordDialog.
     * \param file The MidiFile to add recorded events to
     * \param data The recorded MIDI event data
     * \param settings QSettings instance for storing preferences
     * \param parent The parent widget
     */
    RecordDialog(MidiFile *file, QMultiMap<int, MidiEvent *> data, QSettings *settings,
                 QWidget *parent = 0);

public slots:
    /**
     * \brief Accepts the dialog and imports the recorded data.
     */
    void enter();

    /**
     * \brief Cancels the dialog without importing data.
     */
    void cancel();

private:
    /** \brief The target MIDI file */
    MidiFile *_file;

    /** \brief The recorded MIDI event data */
    QMultiMap<int, MidiEvent *> _data;

    /** \brief Channel and track selection combo boxes */
    QComboBox *_channelBox;
    QComboBox *_trackBox;

    /** \brief List widget for event type selection */
    QListWidget *addTypes;

    /** \brief Settings storage */
    QSettings *_settings;

    /**
     * \brief Adds an item to a list widget.
     * \param w The list widget to add to
     * \param title The item title
     * \param line The line number
     * \param enabled Whether the item is enabled
     */
    void addListItem(QListWidget *w, QString title, int line, bool enabled);
};

#endif // RECORDDIALOG_H_
