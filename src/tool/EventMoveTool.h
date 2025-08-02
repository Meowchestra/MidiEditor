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

#ifndef EVENTMOVETOOL_H_
#define EVENTMOVETOOL_H_

#include "EventTool.h"

// Forward declarations
class MidiEvent;

/**
 * \class EventMoveTool
 *
 * \brief Tool for moving MIDI events by dragging them to new positions.
 *
 * The EventMoveTool allows users to move MIDI events by clicking and dragging
 * them to new positions in time and/or pitch. The tool can be configured to
 * allow movement in different directions:
 *
 * - **Horizontal movement**: Changes event timing (when events occur)
 * - **Vertical movement**: Changes event pitch (for note events)
 * - **Both directions**: Full 2D movement capability
 *
 * Key features:
 * - **Configurable directions**: Can restrict movement to horizontal, vertical, or both
 * - **Multi-selection support**: Moves all selected events together
 * - **Grid snapping**: Respects magnet/grid settings for precise alignment
 * - **Visual feedback**: Shows movement preview during dragging
 * - **Constraint handling**: Prevents invalid moves (negative times, etc.)
 * - **Undo support**: All moves are recorded in the protocol system
 */
class EventMoveTool : public EventTool {
public:
    /**
     * \brief Creates a new EventMoveTool with specified movement directions.
     * \param upDown True to allow vertical movement (pitch changes)
     * \param leftRight True to allow horizontal movement (timing changes)
     */
    EventMoveTool(bool upDown, bool leftRight);

    /**
     * \brief Creates a new EventMoveTool copying another instance.
     * \param other The EventMoveTool instance to copy
     */
    EventMoveTool(EventMoveTool &other);

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
     * \brief Sets the allowed movement directions.
     * \param upDown True to allow vertical movement (pitch changes)
     * \param leftRight True to allow horizontal movement (timing changes)
     */
    void setDirections(bool upDown, bool leftRight);

    /**
     * \brief Draws the tool's visual feedback (movement preview).
     * \param painter The QPainter to draw with
     */
    void draw(QPainter *painter);

    /**
     * \brief Handles mouse press events to start moving.
     * \param leftClick True if left mouse button was pressed
     * \return True if the event was handled
     */
    bool press(bool leftClick);

    /**
     * \brief Handles mouse release events to complete moving.
     * \return True if the event was handled
     */
    bool release();

    /**
     * \brief Handles mouse move events during moving.
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

protected:
    /** \brief Movement direction flags and drag state */
    bool moveUpDown, moveLeftRight, inDrag;

    /** \brief Starting position coordinates for the drag operation */
    int startX, startY;

private:
    /**
     * \brief Computes the grid raster for snapping.
     * \return The grid spacing in pixels
     */
    int computeRaster();
};

#endif // EVENTMOVETOOL_H_
