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

#ifndef TEMPOTOOL_H_
#define TEMPOTOOL_H_

#include "EventTool.h"

/**
 * \class TempoTool
 *
 * \brief Tool for creating and editing tempo change events.
 *
 * The TempoTool allows users to add tempo change events to the MIDI sequence
 * by clicking at specific time positions. This enables dynamic tempo changes
 * throughout the composition.
 *
 * Key features:
 * - **Click to create**: Single click creates a tempo change event
 * - **Visual feedback**: Shows where the tempo change will be placed
 * - **Time-based**: Tempo changes affect playback from that point forward
 * - **BPM editing**: Integrates with tempo dialogs for precise BPM values
 * - **Musical expression**: Enables accelerando, ritardando, and tempo variations
 *
 * The tool creates TempoChangeEvent instances that control the playback
 * speed of the MIDI sequence from their position onward.
 */
class TempoTool : public EventTool {
public:
    /**
     * \brief Creates a new TempoTool.
     */
    TempoTool();

    /**
     * \brief Creates a new TempoTool copying another instance.
     * \param other The TempoTool instance to copy
     */
    TempoTool(TempoTool &other);

    /**
     * \brief Draws the tool's visual feedback.
     * \param painter The QPainter to draw with
     */
    void draw(QPainter *painter);

    /**
     * \brief Handles mouse press events to start tempo event creation.
     * \param leftClick True if left mouse button was pressed
     * \return True if the event was handled
     */
    bool press(bool leftClick);

    /**
     * \brief Handles mouse release events to complete tempo event creation.
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
    /** \brief Starting X position for tempo event creation */
    int _startX;
};

#endif // TEMPOTOOL_H_
