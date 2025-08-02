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

#ifndef MIDISETTINGSWIDGET_H_
#define MIDISETTINGSWIDGET_H_

// Project includes
#include "SettingsWidget.h"

// Forward declarations
class QWidget;
class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QSettings;

/**
 * \class AdditionalMidiSettingsWidget
 *
 * \brief Settings widget for advanced MIDI configuration options.
 *
 * AdditionalMidiSettingsWidget provides configuration for advanced MIDI
 * settings that don't fit in the main MIDI settings panel:
 *
 * - **Alternative player mode**: Manual MIDI command configuration
 * - **Timing settings**: Ticks per quarter note configuration
 * - **Metronome settings**: Volume and behavior options
 * - **External commands**: Custom MIDI player command setup
 *
 * These settings are typically used by advanced users who need specific
 * MIDI configurations or want to use external MIDI players.
 */
class AdditionalMidiSettingsWidget : public SettingsWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new AdditionalMidiSettingsWidget.
     * \param settings QSettings instance for configuration storage
     * \param parent The parent widget
     */
    AdditionalMidiSettingsWidget(QSettings *settings, QWidget *parent = 0);

    /**
     * \brief Validates and applies the settings changes.
     * \return True if settings are valid and applied successfully
     */
    bool accept();

public slots:
    /**
     * \brief Handles manual mode toggle changes.
     * \param enable True to enable manual mode
     */
    void manualModeToggled(bool enable);

    /**
     * \brief Sets the default ticks per quarter note.
     * \param value The new TPQ value
     */
    void setDefaultTimePerQuarter(int value);

    /**
     * \brief Sets the metronome loudness.
     * \param value The new loudness value
     */
    void setMetronomeLoudness(int value);

    /**
     * \brief Refreshes colors when theme changes.
     */
    void refreshColors();

private:
    /** \brief Alternative player mode checkbox */
    QCheckBox *_alternativePlayerModeBox;

    /** \brief Settings storage */
    QSettings *_settings;

    /** \brief Start command line edit */
    QLineEdit *startCmd;

    /** \brief Ticks per quarter spin box */
    QSpinBox *_tpqBox;

    /** \brief Metronome loudness spin box */
    QSpinBox *_metronomeLoudnessBox;

    /** \brief Info box widgets */
    QWidget *_tpqInfoBox;
    QWidget *_startCmdInfoBox;
    QWidget *_playerModeInfoBox;
};

/**
 * \class MidiSettingsWidget
 *
 * \brief Main MIDI settings widget for input/output port configuration.
 *
 * MidiSettingsWidget provides the primary interface for configuring MIDI
 * input and output devices:
 *
 * - **Input ports**: Selection of available MIDI input devices
 * - **Output ports**: Selection of available MIDI output devices
 * - **Port detection**: Automatic detection and refresh of MIDI ports
 * - **Connection status**: Visual feedback for port connections
 *
 * The widget automatically detects available MIDI devices and allows
 * users to select the appropriate ports for recording and playback.
 */
class MidiSettingsWidget : public SettingsWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new MidiSettingsWidget.
     * \param parent The parent widget
     */
    MidiSettingsWidget(QWidget *parent = 0);

public slots:
    /**
     * \brief Reloads the list of available input ports.
     */
    void reloadInputPorts();

    /**
     * \brief Reloads the list of available output ports.
     */
    void reloadOutputPorts();

    /**
     * \brief Handles input port selection changes.
     * \param item The selected list widget item
     */
    void inputChanged(QListWidgetItem *item);

    /**
     * \brief Handles output port selection changes.
     * \param item The selected list widget item
     */
    void outputChanged(QListWidgetItem *item);

    /**
     * \brief Refreshes colors when theme changes.
     */
    void refreshColors();

private:
    /** \brief Lists of available ports */
    QStringList *_inputPorts, *_outputPorts;

    /** \brief Port selection list widgets */
    QListWidget *_inList, *_outList;

    /** \brief Player mode info box */
    QWidget *_playerModeInfoBox;
};

#endif // MIDISETTINGSWIDGET_H_
