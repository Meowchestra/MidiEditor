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

#ifndef METRONOME_H_
#define METRONOME_H_

// Qt includes
#include <QSoundEffect>

// Forward declarations
class MidiFile;

/**
 * \class Metronome
 *
 * \brief Audio metronome for providing timing reference during playback.
 *
 * Metronome provides an audio click track that helps users maintain timing
 * during MIDI playback and recording. It features:
 *
 * - **Beat tracking**: Clicks on beats according to time signature
 * - **Measure awareness**: Different sounds for downbeats and regular beats
 * - **Volume control**: Adjustable loudness for the click sound
 * - **Enable/disable**: Can be turned on/off as needed
 * - **Automatic sync**: Follows tempo and time signature changes
 *
 * The metronome uses QSoundEffect for audio playback and automatically
 * tracks the current playback position to provide accurate timing clicks.
 * It integrates with the PlayerThread to receive timing updates.
 */
class Metronome : public QObject {
    Q_OBJECT

public:
    /**
     * \brief Creates a new Metronome.
     * \param parent The parent QObject
     */
    Metronome(QObject *parent = 0);

    /**
     * \brief Sets the MIDI file for timing reference.
     * \param file The MidiFile to track for timing
     */
    void setFile(MidiFile *file);

    /**
     * \brief Gets the singleton metronome instance.
     * \return Pointer to the global Metronome instance
     */
    static Metronome *instance();

    /**
     * \brief Checks if the metronome is enabled.
     * \return True if metronome is enabled
     */
    static bool enabled();

    /**
     * \brief Enables or disables the metronome.
     * \param b True to enable, false to disable
     */
    static void setEnabled(bool b);

    /**
     * \brief Sets the metronome volume.
     * \param value Volume level (0-100)
     */
    static void setLoudness(int value);

    /**
     * \brief Gets the current metronome volume.
     * \return Volume level (0-100)
     */
    static int loudness();

public slots:
    /**
     * \brief Handles measure position updates from playback.
     * \param measure Current measure number
     * \param tickInMeasure Current tick within the measure
     */
    void measureUpdate(int measure, int tickInMeasure);

    /**
     * \brief Handles time signature changes.
     * \param n Numerator of time signature
     * \param d Denominator of time signature
     */
    void meterChanged(int n, int d);

    /**
     * \brief Handles playback start events.
     */
    void playbackStarted();

    /**
     * \brief Handles playback stop events.
     */
    void playbackStopped();

private:
    /** \brief Singleton instance */
    static Metronome *_instance;

    /** \brief Associated MIDI file */
    MidiFile *_file;

    /**
     * \brief Resets the metronome state.
     */
    void reset();

    /** \brief Time signature and position tracking */
    int num, denom, lastPos, lastMeasure;

    /**
     * \brief Plays a metronome click sound.
     */
    void click();

    /** \brief Enable/disable flag */
    static bool _enable;

    /** \brief Audio player for click sounds */
    QSoundEffect *_player;
};

#endif // METRONOME_H_
