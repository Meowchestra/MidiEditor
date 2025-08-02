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

#ifndef NEWNOTETOOL_H_
#define NEWNOTETOOL_H_

#include "EventTool.h"

/**
 * \class NewNoteTool
 *
 * \brief Tool for creating new MIDI note events.
 *
 * The NewNoteTool allows users to create new MIDI notes by clicking and dragging
 * in the matrix editor. The tool provides:
 *
 * - **Click to create**: Single click creates a note with default length
 * - **Drag to size**: Click and drag to set the note duration
 * - **Visual feedback**: Shows preview of the note being created
 * - **Track/channel targeting**: Notes are created on the specified track and channel
 *
 * The tool maintains static settings for the target track and channel,
 * which can be configured through the UI or programmatically.
 */
class NewNoteTool : public EventTool {
public:
    /**
     * \brief Creates a new NewNoteTool.
     */
    NewNoteTool();

    /**
     * \brief Creates a new NewNoteTool copying another instance.
     * \param other The NewNoteTool instance to copy
     */
    NewNoteTool(NewNoteTool &other);

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
     * \brief Draws the tool's visual feedback (note preview).
     * \param painter The QPainter to draw with
     */
    void draw(QPainter *painter);

    /**
     * \brief Handles mouse press events to start note creation.
     * \param leftClick True if left mouse button was pressed
     * \return True if the event was handled
     */
    bool press(bool leftClick);

    /**
     * \brief Handles mouse release events to complete note creation.
     * \return True if the event was handled
     */
    bool release();

    /**
     * \brief Handles mouse move events during note creation.
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

    // === Static Configuration ===

    /**
     * \brief Gets the current edit track.
     * \return The track index where new notes will be created
     */
    static int editTrack();

    /**
     * \brief Gets the current edit channel.
     * \return The channel index where new notes will be created
     */
    static int editChannel();

    /**
     * \brief Sets the edit track for new notes.
     * \param i The track index to use for new notes
     */
    static void setEditTrack(int i);

    /**
     * \brief Sets the edit channel for new notes.
     * \param i The channel index to use for new notes
     */
    static void setEditChannel(int i);

private:
    /** \brief Flag indicating if currently dragging to size a note */
    bool inDrag;

    /** \brief The line (pitch) where the note is being created */
    int line;

    /** \brief The X position where note creation started */
    int xPos;

    /** \brief Static channel and track settings for new notes */
    static int _channel, _track;
};

#endif // NEWNOTETOOL_H_
