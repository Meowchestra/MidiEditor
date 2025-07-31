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

#ifndef GLUETOOL_H_
#define GLUETOOL_H_

#include "EventTool.h"
#include <QList>

class MidiEvent;
class NoteOnEvent;

/**
 * \class GlueTool
 *
 * \brief Tool for merging adjacent notes of the same pitch.
 *
 * The GlueTool allows users to merge adjacent MIDI notes that have the same
 * pitch (note number) and channel. This is similar to the "Glue" functionality
 * found in Cubase and other DAWs.
 *
 * The tool works by:
 * 1. Finding all selected notes or notes in the current view
 * 2. Grouping them by pitch and channel
 * 3. Identifying adjacent notes (notes that touch or overlap)
 * 4. Merging adjacent notes into a single note that spans from the start
 *    of the first note to the end of the last note
 */
class GlueTool : public EventTool {
public:
    /**
     * \brief Creates a new GlueTool.
     */
    GlueTool();

    /**
     * \brief Creates a new GlueTool copying &other.
     */
    GlueTool(GlueTool &other);

    /**
     * \brief Draws the tool's visual feedback.
     */
    void draw(QPainter *painter);

    /**
     * \brief Handles mouse press events.
     */
    bool press(bool leftClick);

    /**
     * \brief Handles mouse release events and performs the glue operation.
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

    /**
     * \brief Performs the glue operation on selected events.
     * \param respectChannels If true, only merge notes within the same channel. If false, merge across all channels on the same track.
     */
    void performGlueOperation(bool respectChannels = true);

private:
    /**
     * \brief Groups notes by pitch and track, optionally by channel.
     * \param events List of events to group
     * \param respectChannels If true, group by channel as well
     * \return Map of grouping keys to lists of NoteOnEvents
     */
    QMap<QString, QList<NoteOnEvent *> > groupNotes(const QList<MidiEvent *> &events, bool respectChannels);

    /**
     * \brief Merges a group of notes into a single note.
     * \param noteGroup List of notes to merge (sorted by start time)
     */
    void mergeNoteGroup(const QList<NoteOnEvent *> &noteGroup);
};

#endif
