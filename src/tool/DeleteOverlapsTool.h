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

#ifndef DELETEOVERLAPSTOOL_H_
#define DELETEOVERLAPSTOOL_H_

#include "EventTool.h"
#include <QList>

class MidiEvent;
class NoteOnEvent;

/**
 * \class DeleteOverlapsTool
 *
 * \brief Tool for deleting overlapping notes in different modes.
 *
 * The DeleteOverlapsTool provides three different modes for handling overlapping notes:
 * 1. Delete Overlaps (Mono) - Delete overlapping notes on the same pitch
 * 2. Delete Overlaps (Poly) - Make the part monophonic by shortening all overlapping notes
 * 3. Delete Doubles - Remove notes that are on the same pitch at the exact same position
 *
 * This is similar to the functionality found in Cubase and other DAWs.
 */
class DeleteOverlapsTool : public EventTool {

public:
    enum OverlapMode {
        MONO_MODE,      // Delete overlaps on same pitch only
        POLY_MODE,      // Delete all overlaps regardless of pitch
        DOUBLES_MODE    // Delete exact duplicates only
    };

    /**
     * \brief Creates a new DeleteOverlapsTool.
     */
    DeleteOverlapsTool();

    /**
     * \brief Creates a new DeleteOverlapsTool copying &other.
     */
    DeleteOverlapsTool(DeleteOverlapsTool& other);

    /**
     * \brief Draws the tool's visual feedback.
     */
    void draw(QPainter* painter);

    /**
     * \brief Handles mouse press events.
     */
    bool press(bool leftClick);

    /**
     * \brief Handles mouse release events and performs the delete operation.
     */
    bool release();

    /**
     * \brief Creates a copy of this tool for the protocol system.
     */
    ProtocolEntry* copy();

    /**
     * \brief Reloads the tool's state from a protocol entry.
     */
    void reloadState(ProtocolEntry* entry);

    /**
     * \brief Returns whether this tool shows selection.
     */
    bool showsSelection();

    /**
     * \brief Performs the delete overlaps operation on selected events.
     * \param mode The overlap deletion mode to use
     * \param respectChannels If true, only process overlaps within the same channel. If false, process across all channels.
     * \param respectTracks If true, only process overlaps within the same track. If false, process across all tracks.
     */
    void performDeleteOverlapsOperation(OverlapMode mode, bool respectChannels = true, bool respectTracks = true);

private:
    /**
     * \brief Deletes overlapping notes on the same pitch (mono mode).
     * \param notes List of notes to process
     * \param respectChannels If true, only process overlaps within the same channel
     * \param respectTracks If true, only process overlaps within the same track
     */
    void deleteOverlapsMono(const QList<NoteOnEvent*>& notes, bool respectChannels = true, bool respectTracks = true);

    /**
     * \brief Makes the part monophonic by deleting all overlaps (poly mode).
     * \param notes List of notes to process
     * \param respectTracks If true, only process overlaps within the same track
     */
    void deleteOverlapsPoly(const QList<NoteOnEvent*>& notes, bool respectTracks = true);

    /**
     * \brief Deletes exact duplicate notes (doubles mode).
     * \param notes List of notes to process
     * \param respectChannels If true, only process duplicates within the same channel
     * \param respectTracks If true, only process duplicates within the same track
     */
    void deleteDoubles(const QList<NoteOnEvent*>& notes, bool respectChannels = true, bool respectTracks = true);

    /**
     * \brief Checks if two notes overlap in time.
     * \param note1 First note
     * \param note2 Second note
     * \return True if the notes overlap
     */
    bool notesOverlap(NoteOnEvent* note1, NoteOnEvent* note2);

    /**
     * \brief Checks if two notes are exact duplicates.
     * \param note1 First note
     * \param note2 Second note
     * \param respectChannels If true, notes must be on same channel to be considered duplicates
     * \param respectTracks If true, notes must be on same track to be considered duplicates
     * \return True if the notes are duplicates
     */
    bool notesAreDuplicates(NoteOnEvent* note1, NoteOnEvent* note2, bool respectChannels = true, bool respectTracks = true);

    /**
     * \brief Removes a note from the MIDI file.
     * \param note Note to remove
     */
    void removeNote(NoteOnEvent* note);

    /**
     * \brief Helper method to process poly overlaps for a group of notes.
     * \param notes List of notes to process
     */
    void processPolyOverlaps(const QList<NoteOnEvent*>& notes);
};

#endif
