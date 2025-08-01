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

#ifndef MATRIXRENDERDATA_H_
#define MATRIXRENDERDATA_H_

// Qt includes
#include <QList>
#include <QRectF>
#include <QMap>

// Forward declarations
class MidiEvent;
class TimeSignatureEvent;

/**
 * \struct MatrixRenderData
 *
 * \brief Data structure for passing rendering information between matrix widgets.
 *
 * MatrixRenderData serves as a communication bridge between HybridMatrixWidget
 * and its rendering backends (MatrixWidget and AcceleratedMatrixWidget). It
 * contains all the information needed to render the MIDI matrix view:
 *
 * - **Viewport bounds**: Visible area and coordinate ranges
 * - **Event data**: Lists of MIDI events to render
 * - **Scaling information**: Zoom levels and pixel ratios
 * - **UI state**: Tool selection, mouse position, display options
 * - **Layout data**: Area definitions and piano key mappings
 *
 * This structure enables the hybrid widget to prepare all rendering data
 * once and pass it to either rendering backend without duplication.
 */
struct MatrixRenderData {
    // === Viewport Bounds ===
    int startTick, endTick, startLine, endLine;      ///< Visible time and line ranges
    int startTimeX, endTimeX, startLineY, endLineY;  ///< Screen coordinate bounds
    int timeHeight, lineNameWidth;                   ///< UI area dimensions
    double scaleX, scaleY, lineHeight;               ///< Scaling factors

    // === Event Data ===
    QList<MidiEvent *> *objects;                       ///< Main MIDI events to render
    QList<MidiEvent *> *velocityObjects;               ///< Events for velocity display
    QList<MidiEvent *> *tempoEvents;                   ///< Tempo change events
    QList<TimeSignatureEvent *> *timeSignatureEvents;  ///< Time signature events
    QList<QPair<int, int> > divs;                      ///< Division markers
    int msOfFirstEventInList;                          ///< Timing reference

    // === Display State ===
    bool colorsByChannels;       ///< Color coding mode
    bool screenLocked;           ///< Screen lock state
    bool pianoEmulationEnabled;  ///< Piano emulation mode
    int div, measure, tool;      ///< Current division, measure, and tool

    // === UI Layout Areas ===
    QRectF toolArea, pianoArea, timeLineArea;  ///< UI area definitions
    QMap<int, QRect> pianoKeys;                ///< Piano key rectangles

    // === Rendering Constants ===
    int numLines, pixelPerS, pixelPerLine, pixelPerEvent; ///< Pixel scaling constants

    // === Mouse State ===
    int mouseX, mouseY;  ///< Current mouse position
    bool mouseOver;      ///< Mouse hover state

    // === File Reference ===
    class MidiFile *file; ///< MIDI file for coordinate calculations
};

#endif // MATRIXRENDERDATA_H_
