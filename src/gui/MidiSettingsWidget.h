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
class QProgressBar;
class MainWindow;
class QListWidget;
class QListWidgetItem;
class QTableWidget;
class QTableWidgetItem;
class SoundFontDragController;
class SoundFontSeparatorDelegate;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QSettings;
class QComboBox;
class QSlider;
class QGroupBox;
class QLabel;
class QPushButton;

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
     * \brief Sets the metronome velocity.
     * \param value The new velocity value (0-127)
     */
    void setMetronomeVelocity(int value);

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

    /** \brief Metronome loudness setting */
    QSlider *_metronomeLoudnessSlider;
    QLabel *_metronomeLoudnessLabel;

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
     * \param mainWindow The main window
     * \param parent The parent widget
     */
    MidiSettingsWidget(MainWindow *mainWindow, QWidget *parent = 0);

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

    /** \brief Main window reference */
    MainWindow *_mainWindow;

#ifdef FLUIDSYNTH_SUPPORT
    friend class SoundFontDragController;
private:
    // FluidSynth settings UI widgets (greyed out unless FluidSynth selected)
    QGroupBox *_fluidSynthSettingsGroup;
    QTableWidget *_soundFontList;
    SoundFontDragController *_dragController;
    SoundFontSeparatorDelegate *_separatorDelegate;
    int _separatorRow;
    QPushButton *_addSoundFontBtn;
    QPushButton *_removeSoundFontBtn;
    QPushButton *_moveSoundFontUpBtn;
    QPushButton *_moveSoundFontDownBtn;
    QPushButton *_downloadDefaultSoundFontBtn;
    QComboBox *_audioDriverCombo;
    QSlider *_gainSlider;
    QLabel *_gainValueLabel;
    QPushButton *_gainResetBtn;
    QComboBox *_sampleRateCombo;
    QComboBox *_reverbEngineCombo;
    QCheckBox *_reverbCheckBox;
    QCheckBox *_chorusCheckBox;
    QComboBox *_sampleFormatCombo;
    QComboBox *_polyphonyCombo;
    QPushButton *_exportWavBtn;

private slots:
    void onSoundFontTableDropped();
    void onSoundFontToggled();
    void updateFluidSynthSettingsEnabled();
    void addSoundFont();
    void removeSoundFont();
    void moveSoundFontUp();
    void moveSoundFontDown();
    void onAudioDriverChanged(int index);
    void onGainChanged(int value);
    void onGainReset();
    void onSampleRateChanged(const QString &rate);
    void onReverbEngineChanged(int index);
    void onReverbToggled(bool enabled);
    void onChorusToggled(bool enabled);
    void onSampleFormatChanged(int index);
    void onPolyphonyChanged(int index);
    void refreshSoundFontList();
    void reorderSoundFont(int fromRow, int toRow);
    void showDownloadSoundFontDialog();

    void onExportToWav();
    void onExportProgress(int percent);
    void onExportFinished(bool success, const QString &path);

private:
#endif
};

#endif // MIDISETTINGSWIDGET_H_
