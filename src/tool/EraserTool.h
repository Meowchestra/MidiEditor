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

#ifndef ERASERTOOL_H_
#define ERASERTOOL_H_

#include "EventTool.h"

/**
 * \class EraserTool
 *
 * \brief Tool for deleting MIDI events by clicking or dragging over them.
 *
 * The EraserTool allows users to delete MIDI events by simply clicking on them
 * or dragging over multiple events. It provides an intuitive way to remove
 * unwanted notes and events from the composition.
 *
 * Key features:
 * - **Click to delete**: Single click removes individual events
 * - **Drag to delete**: Drag over multiple events to delete them all
 * - **Visual feedback**: Shows which events will be deleted
 * - **Undo support**: All deletions can be undone through the protocol system
 * - **Safe operation**: Only deletes events that are directly clicked/dragged over
 */
class EraserTool : public EventTool {
public:
    /**
     * \brief Creates a new EraserTool.
     */
    EraserTool();

    /**
     * \brief Creates a new EraserTool copying another instance.
     * \param other The EraserTool instance to copy
     */
    EraserTool(EraserTool &other);

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
     * \brief Handles mouse move events to delete events under the cursor.
     * \param mouseX Current mouse X coordinate
     * \param mouseY Current mouse Y coordinate
     * \return True if the widget needs to be repainted
     */
    bool move(int mouseX, int mouseY);

    /**
     * \brief Draws the tool's visual feedback.
     * \param painter The QPainter to draw with
     */
    void draw(QPainter *painter);

    /**
     * \brief Handles mouse release events.
     * \return True if the widget needs to be repainted
     */
    bool release();
};

#endif // ERASERTOOL_H_
