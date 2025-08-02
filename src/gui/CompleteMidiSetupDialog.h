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

#ifndef COMPLETEMIDISETUPDIALOG_H_
#define COMPLETEMIDISETUPDIALOG_H_

// Qt includes
#include <QDialog>
#include <QObject>
#include <QWidget>

/**
 * \class CompleteMidiSetupDialog
 *
 * \brief Dialog for guiding users through MIDI setup when no MIDI devices are detected.
 *
 * CompleteMidiSetupDialog appears when the application detects missing or
 * improperly configured MIDI input/output devices. It provides:
 *
 * - **Setup guidance**: Instructions for configuring MIDI devices
 * - **Problem diagnosis**: Information about detected MIDI issues
 * - **Settings access**: Direct link to MIDI settings configuration
 * - **User assistance**: Help for common MIDI setup problems
 *
 * The dialog helps users resolve MIDI connectivity issues and ensures
 * proper MIDI functionality for recording and playback.
 */
class CompleteMidiSetupDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * \brief Creates a new CompleteMidiSetupDialog.
     * \param parent The parent widget
     * \param alertAboutInput True if there are MIDI input issues
     * \param alertAboutOutput True if there are MIDI output issues
     */
    CompleteMidiSetupDialog(QWidget *parent, bool alertAboutInput, bool alertAboutOutput);

public slots:
    /**
     * \brief Opens the MIDI settings dialog.
     */
    void openSettings();
};

#endif // COMPLETEMIDISETUPDIALOG_H_
