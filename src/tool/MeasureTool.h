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

#ifndef MEASURETOOL_H_
#define MEASURETOOL_H_

#include "EventTool.h"

/**
 * \class MeasureTool
 *
 * \brief Tool for selecting and highlighting measures in the MIDI editor.
 *
 * The MeasureTool allows users to select one or more measures in the MIDI editor
 * by clicking and dragging. Selected measures are visually highlighted, making it
 * easier to work with specific sections of the composition.
 *
 * Features:
 * - Click to select a single measure
 * - Drag to select multiple consecutive measures
 * - Visual feedback with highlighted measure regions
 * - Integration with the editor's selection system
 */
class MeasureTool : public EventTool {
public:
    /**
     * \brief Creates a new MeasureTool.
     */
    MeasureTool();

    /**
     * \brief Creates a new MeasureTool copying another instance.
     * \param other The MeasureTool instance to copy
     */
    MeasureTool(MeasureTool &other);

    /**
     * \brief Draws the tool's visual feedback and measure highlights.
     * \param painter The QPainter to draw with
     */
    void draw(QPainter *painter);

    /**
     * \brief Handles mouse press events to start measure selection.
     * \param leftClick True if left mouse button was pressed
     * \return True if the event was handled
     */
    bool press(bool leftClick);

    /**
     * \brief Handles mouse release events to complete measure selection.
     * \return True if the event was handled
     */
    bool release();

    /**
     * \brief Handles release-only events.
     * \return True if the event was handled
     */
    bool releaseOnly();

    /**
     * \brief Handles key release events.
     * \param key The key code that was released
     * \return True if the event was handled
     */
    bool releaseKey(int key);

    /**
     * \brief Handles mouse move events during measure selection.
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
    /** \brief Index of the first selected measure */
    int _firstSelectedMeasure;

    /** \brief Index of the second selected measure (for range selection) */
    int _secondSelectedMeasure;

    /**
     * \brief Finds the closest measure start position to the given coordinates.
     * \param distX Pointer to store the distance to the closest measure
     * \param measureX Pointer to store the X coordinate of the measure start
     * \return The index of the closest measure
     */
    int closestMeasureStart(int *distX, int *measureX);

    /**
     * \brief Fills the specified measure range with highlight color.
     * \param painter The QPainter to draw with
     * \param start The starting measure index
     * \param end The ending measure index
     */
    void fillMeasures(QPainter *painter, int start, int end);
};

#endif // MEASURETOOL_H_
