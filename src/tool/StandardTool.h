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

#ifndef STANDARDTOOL_H_
#define STANDARDTOOL_H_

#include "EventTool.h"

// Forward declarations
class EventMoveTool;
class SelectTool;
class SizeChangeTool;
class NewNoteTool;

/**
 * \class StandardTool
 *
 * \brief The default multipurpose tool that automatically selects appropriate sub-tools.
 *
 * The StandardTool is the primary tool used in the MIDI editor. It intelligently
 * switches between different specialized tools based on the user's actions and
 * the context of the mouse interaction:
 *
 * - Uses SelectTool for selecting events and regions
 * - Uses EventMoveTool for moving selected events
 * - Uses SizeChangeTool for resizing note events
 * - Uses NewNoteTool for creating new notes
 *
 * This provides a seamless editing experience where users don't need to manually
 * switch between tools for common operations.
 */
class StandardTool : public EventTool {
public:
    /**
     * \brief Creates a new StandardTool.
     */
    StandardTool();

    /**
     * \brief Creates a new StandardTool copying another instance.
     * \param other The StandardTool instance to copy
     */
    StandardTool(StandardTool &other);

    /**
     * \brief Draws the tool's visual feedback by delegating to the active sub-tool.
     * \param painter The QPainter to draw with
     */
    void draw(QPainter *painter);

    /**
     * \brief Handles mouse press events and selects the appropriate sub-tool.
     * \param leftClick True if left mouse button was pressed
     * \return True if the event was handled
     */
    bool press(bool leftClick);

    /**
     * \brief Handles mouse move events by delegating to the active sub-tool.
     * \param mouseX Current mouse X coordinate
     * \param mouseY Current mouse Y coordinate
     * \return True if the event was handled
     */
    bool move(int mouseX, int mouseY);

    /**
     * \brief Handles mouse release events by delegating to the active sub-tool.
     * \return True if the event was handled
     */
    bool release();

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
     * \brief Returns whether this tool shows selection highlights.
     * \return True if selection should be shown
     */
    bool showsSelection();

private:
    /** \brief Tool for moving events */
    EventMoveTool *moveTool;

    /** \brief Tool for selecting events */
    SelectTool *selectTool;

    /** \brief Tool for resizing events */
    SizeChangeTool *sizeChangeTool;

    /** \brief Tool for creating new notes */
    NewNoteTool *newNoteTool;
};
#endif // STANDARDTOOL_H_
