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

#ifndef SIZECHANGETOOL_H_
#define SIZECHANGETOOL_H_

#include "EventTool.h"

/**
 * \class SizeChangeTool
 *
 * \brief Tool for resizing MIDI note events by dragging their edges.
 *
 * The SizeChangeTool allows users to change the duration of MIDI notes by
 * clicking and dragging the start or end edges of note events. This provides
 * an intuitive way to adjust note lengths directly in the piano roll view.
 *
 * Key features:
 * - **Edge detection**: Automatically detects which edge of a note is being dragged
 * - **Start/end resize**: Can resize from either the beginning or end of notes
 * - **Visual feedback**: Shows resize cursor and preview during dragging
 * - **Grid snapping**: Respects magnet/grid settings for precise alignment
 * - **Multi-selection**: Can resize multiple selected notes simultaneously
 * - **Minimum duration**: Prevents notes from becoming too short or negative length
 *
 * The tool distinguishes between dragging the start (OnEvent) and end (OffEvent)
 * of notes to provide appropriate resizing behavior.
 */
class SizeChangeTool : public EventTool {
public:
    /**
     * \brief Creates a new SizeChangeTool.
     */
    SizeChangeTool();

    /**
     * \brief Creates a new SizeChangeTool copying another instance.
     * \param other The SizeChangeTool instance to copy
     */
    SizeChangeTool(SizeChangeTool &other);

    /**
     * \brief Creates a copy of this tool for the protocol system.
     * \return A new ProtocolEntry representing this tool's state
     */
    ProtocolEntry *copy();

    /**
     * \brief Reloads the tool's state from a protocol entry.
     * \param entry The protocol entry to restore state from
     */
    void reloadState(ProtocolEntry *entry);

    /**
     * \brief Draws the tool's visual feedback (resize cursor, preview).
     * \param painter The QPainter to draw with
     */
    void draw(QPainter *painter);

    /**
     * \brief Handles mouse press events to start resizing.
     * \param leftClick True if left mouse button was pressed
     * \return True if the event was handled
     */
    bool press(bool leftClick);

    /**
     * \brief Handles mouse release events to complete resizing.
     * \return True if the event was handled
     */
    bool release();

    /**
     * \brief Handles mouse move events during resizing.
     * \param mouseX Current mouse X coordinate
     * \param mouseY Current mouse Y coordinate
     * \return True if the event was handled
     */
    bool move(int mouseX, int mouseY);

    /**
     * \brief Handles release-only events.
     * \return True if the event was handled
     */
    bool releaseOnly();

    /**
     * \brief Returns whether this tool shows selection highlights.
     * \return True if selection should be shown
     */
    bool showsSelection();

private:
    /** \brief Flag indicating if currently dragging to resize */
    bool inDrag;

    /** \brief Flag indicating if dragging an OnEvent (start) vs OffEvent (end) */
    bool dragsOnEvent;

    /** \brief X position where resize operation started */
    int xPos;
};

#endif // SIZECHANGETOOL_H_
