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

#ifndef SCISSORSTOOL_H_
#define SCISSORSTOOL_H_

#include "EventTool.h"
#include <QList>

// Forward declarations
class MidiEvent;
class NoteOnEvent;

/**
 * \class ScissorsTool
 *
 * \brief Tool for splitting notes at a specific time position.
 *
 * The ScissorsTool allows users to split MIDI notes at the cursor position,
 * similar to the "Scissors" functionality found in Cubase and other DAWs.
 *
 * The tool works by:
 * 1. Displaying a red vertical line that follows the mouse cursor
 * 2. When clicked, finding all notes that span across the cursor position
 * 3. Splitting those notes into two separate notes at the cursor position
 * 4. Working across all tracks and channels (like Cubase scissors)
 */
class ScissorsTool : public EventTool {
public:
    /**
     * \brief Creates a new ScissorsTool.
     */
    ScissorsTool();

    /**
     * \brief Creates a new ScissorsTool copying another instance.
     * \param other The ScissorsTool instance to copy
     */
    ScissorsTool(ScissorsTool &other);

    /**
     * \brief Draws the tool's visual feedback (red vertical line).
     */
    void draw(QPainter *painter);

    /**
     * \brief Handles mouse press events.
     */
    bool press(bool leftClick);

    /**
     * \brief Handles mouse release events and performs the split operation.
     */
    bool release();

    /**
     * \brief Creates a copy of this tool for the protocol system.
     */
    ProtocolEntry *copy();

    /**
     * \brief Reloads the tool's state from a protocol entry.
     */
    void reloadState(ProtocolEntry *entry);

    /**
     * \brief Returns whether this tool shows selection.
     */
    bool showsSelection();

private:
    /**
     * \brief Performs the split operation at the current cursor position.
     */
    void performSplitOperation();

    /**
     * \brief Finds all notes that span across the given tick position.
     * \param splitTick The tick position where to split
     * \return List of NoteOnEvents that need to be split
     */
    QList<NoteOnEvent *> findNotesToSplit(int splitTick);

    /**
     * \brief Splits a single note at the given position.
     * \param note The note to split
     * \param splitTick The tick position where to split
     */
    void splitNote(NoteOnEvent *note, int splitTick);

    /**
     * \brief Checks if a note spans across the given tick position.
     * \param note The note to check
     * \param tick The tick position to check
     * \return True if the note spans across the tick position
     */
    bool noteSpansAcrossTick(NoteOnEvent *note, int tick);

    int _splitTick; ///< The tick position where the split will occur
};

#endif // SCISSORSTOOL_H_
