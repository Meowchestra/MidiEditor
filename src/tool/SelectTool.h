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

#ifndef SELECTTOOL_H_
#define SELECTTOOL_H_

#include "EventTool.h"

// Selection type constants
#define SELECTION_TYPE_RIGHT 0    ///< Right-click selection
#define SELECTION_TYPE_LEFT 1     ///< Left-click selection
#define SELECTION_TYPE_BOX 2      ///< Box/rectangle selection
#define SELECTION_TYPE_SINGLE 3   ///< Single event selection

// Forward declarations
class MidiEvent;

/**
 * \class SelectTool
 *
 * \brief Tool for selecting MIDI events using various selection methods.
 *
 * The SelectTool provides different ways to select MIDI events in the editor:
 *
 * - **Single selection**: Click on individual events
 * - **Box selection**: Drag to create a selection rectangle
 * - **Left-click selection**: Standard left-click selection behavior
 * - **Right-click selection**: Right-click selection behavior
 *
 * The tool supports both additive selection (holding modifiers) and
 * replacement selection (clearing previous selections). It provides
 * visual feedback during box selection operations.
 */
class SelectTool : public EventTool {
public:
    /**
     * \brief Creates a new SelectTool with the specified selection type.
     * \param type The selection type (see SELECTION_TYPE_* constants)
     */
    SelectTool(int type);

    /**
     * \brief Creates a new SelectTool copying another instance.
     * \param other The SelectTool instance to copy
     */
    SelectTool(SelectTool &other);

    /**
     * \brief Draws the tool's visual feedback (selection rectangle).
     * \param painter The QPainter to draw with
     */
    void draw(QPainter *painter);

    /**
     * \brief Handles mouse press events to start selection.
     * \param leftClick True if left mouse button was pressed
     * \return True if the event was handled
     */
    bool press(bool leftClick);

    /**
     * \brief Handles mouse release events to complete selection.
     * \return True if the event was handled
     */
    bool release();

    /**
     * \brief Handles release-only events.
     * \return True if the event was handled
     */
    bool releaseOnly();

    /**
     * \brief Handles mouse move events during selection.
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

    /**
     * \brief Checks if an event is within the selection rectangle.
     * \param event The event to check
     * \param x_start Rectangle start X coordinate
     * \param y_start Rectangle start Y coordinate
     * \param x_end Rectangle end X coordinate
     * \param y_end Rectangle end Y coordinate
     * \return True if the event is within the rectangle
     */
    bool inRect(MidiEvent *event, int x_start, int y_start, int x_end, int y_end);

    /**
     * \brief Returns whether this tool shows selection highlights.
     * \return True if selection should be shown
     */
    bool showsSelection();

protected:
    /** \brief The selection type for this tool instance */
    int stool_type;

    /** \brief Selection rectangle coordinates */
    int x_rect, y_rect;
};

#endif // SELECTTOOL_H_
