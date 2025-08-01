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

#ifndef MIDIPLAYER_H_
#define MIDIPLAYER_H_

// Qt includes
#include <QObject>

// Forward declarations
class MidiFile;
class NoteOnEvent;
class PlayerThread;
class SingleNotePlayer;

/**
 * \class MidiPlayer
 *
 * \brief Static interface for MIDI playback functionality.
 *
 * MidiPlayer provides a centralized, static interface for all MIDI playback
 * operations in the editor. It manages both file playback and single note
 * preview functionality:
 *
 * - **File playback**: Complete MIDI file playback with timing
 * - **Note preview**: Single note playback for editing feedback
 * - **Playback control**: Start, stop, and status monitoring
 * - **Speed control**: Variable playback speed for practice
 * - **Panic function**: Emergency stop for all MIDI output
 * - **Time tracking**: Current playback position monitoring
 *
 * Key features:
 * - Static interface for global access
 * - Thread-based playback for smooth performance
 * - Speed scaling for tempo adjustment
 * - Emergency panic function for stuck notes
 * - Integration with MIDI output system
 *
 * The class uses separate threads for file and single note playback
 * to ensure responsive user interface during playback operations.
 */
class MidiPlayer : public QObject {
    Q_OBJECT

public:
    // === Playback Control ===

    /**
     * \brief Starts playback of a MIDI file.
     * \param file The MidiFile to play
     */
    static void play(MidiFile *file);

    /**
     * \brief Plays a single note event for preview.
     * \param event The NoteOnEvent to play
     */
    static void play(NoteOnEvent *event);

    /**
     * \brief Stops all MIDI playback.
     */
    static void stop();

    /**
     * \brief Checks if MIDI playback is currently active.
     * \return True if playing, false if stopped
     */
    static bool isPlaying();

    // === Playback Status and Control ===

    /**
     * \brief Gets the current playback time in milliseconds.
     * \return Current playback position in milliseconds
     */
    static int timeMs();

    /**
     * \brief Gets the player thread instance.
     * \return Pointer to the PlayerThread handling file playback
     */
    static PlayerThread *playerThread();

    /**
     * \brief Gets the current playback speed scale.
     * \return Speed multiplier (1.0 = normal speed)
     */
    static double speedScale();

    /**
     * \brief Sets the playback speed scale.
     * \param d Speed multiplier (1.0 = normal, 0.5 = half speed, 2.0 = double speed)
     */
    static void setSpeedScale(double d);

    /**
     * \brief Emergency stop - sends all notes off to every channel.
     *
     * This function immediately stops all MIDI output and sends
     * "All Notes Off" messages to prevent stuck notes.
     */
    static void panic();

private:
    /** \brief Thread handling MIDI file playback */
    static PlayerThread *filePlayer;

    /** \brief Current playing state */
    static bool playing;

    /** \brief Player for single note preview */
    static SingleNotePlayer *singleNotePlayer;

    /** \brief Playback speed multiplier */
    static double _speed;
};

#endif // MIDIPLAYER_H_
