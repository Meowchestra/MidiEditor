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

#ifndef SINGLENOTEPLAYER_H_
#define SINGLENOTEPLAYER_H_

// Qt includes
#include <QObject>

// Forward declarations
class NoteOnEvent;
class QTimer;

/**
 * \class SingleNotePlayer
 *
 * \brief Simple player for single note preview during editing.
 *
 * SingleNotePlayer provides immediate audio feedback when users interact
 * with individual notes in the editor. It offers:
 *
 * - **Note preview**: Play individual notes for immediate feedback
 * - **Automatic note-off**: Automatically stops notes after a duration
 * - **Non-blocking**: Doesn't interfere with main playback system
 * - **Simple interface**: Easy integration with editing tools
 * - **Timer-based**: Uses QTimer for note duration management
 *
 * The player is commonly used when:
 * - Clicking on notes in the piano roll
 * - Creating new notes with tools
 * - Testing note properties during editing
 * - Providing audio feedback for user interactions
 *
 * It automatically handles note-off messages to prevent stuck notes.
 */
class SingleNotePlayer : public QObject {
    Q_OBJECT

public:
    /**
     * \brief Creates a new SingleNotePlayer.
     */
    SingleNotePlayer();

    /**
     * \brief Plays a single note event for preview.
     * \param event The NoteOnEvent to play
     */
    void play(NoteOnEvent *event);

public slots:
    /**
     * \brief Handles timer timeout to stop the note.
     */
    void timeout();

private:
    /** \brief Timer for note duration */
    QTimer *timer;

    /** \brief Note-off message to send when stopping */
    QByteArray offMessage;

    /** \brief Flag indicating if currently playing */
    bool playing;
};

#endif // SINGLENOTEPLAYER_H_
