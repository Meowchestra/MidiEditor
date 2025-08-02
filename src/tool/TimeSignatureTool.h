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

#ifndef TIMESIGNATURETOOL_H_
#define TIMESIGNATURETOOL_H_

#include "EventTool.h"

/**
 * \class TimeSignatureTool
 *
 * \brief Tool for creating and editing time signature events.
 *
 * The TimeSignatureTool allows users to add time signature change events
 * to the MIDI sequence by clicking at specific time positions. This enables
 * changes in musical meter throughout the composition.
 *
 * Key features:
 * - **Click to create**: Single click creates a time signature event
 * - **Visual feedback**: Shows where the time signature change will be placed
 * - **Measure alignment**: Time signatures typically align to measure boundaries
 * - **Meter changes**: Supports common time signatures (4/4, 3/4, 6/8, etc.)
 * - **Musical structure**: Affects measure lines and beat grouping
 *
 * The tool creates TimeSignatureEvent instances that control the musical
 * meter and measure structure from their position onward.
 */
class TimeSignatureTool : public EventTool {
public:
    /**
     * \brief Creates a new TimeSignatureTool.
     */
    TimeSignatureTool();

    /**
     * \brief Creates a new TimeSignatureTool copying another instance.
     * \param other The TimeSignatureTool instance to copy
     */
    TimeSignatureTool(TimeSignatureTool &other);

    /**
     * \brief Draws the tool's visual feedback.
     * \param painter The QPainter to draw with
     */
    void draw(QPainter *painter);

    /**
     * \brief Handles mouse press events to start time signature event creation.
     * \param leftClick True if left mouse button was pressed
     * \return True if the event was handled
     */
    bool press(bool leftClick);

    /**
     * \brief Handles mouse release events to complete time signature event creation.
     * \return True if the event was handled
     */
    bool release();

    /**
     * \brief Handles release-only events.
     * \return True if the event was handled
     */
    bool releaseOnly();

    /**
     * \brief Handles mouse move events.
     * \param mouseX Current mouse X coordinate
     * \param mouseY Current mouse Y coordinate
     * \return True if the event was handled
     */
    bool move(int mouseX, int mouseY);

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

private:
    // No additional private members needed
};

#endif // TIMESIGNATURETOOL_H_
