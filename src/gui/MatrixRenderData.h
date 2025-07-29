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

#ifndef MATRIXRENDERDATA_H
#define MATRIXRENDERDATA_H

#include <QList>
#include <QRectF>
#include <QMap>
#include <QRect>

class MidiEvent;
class TimeSignatureEvent;

// Structure to pass rendering data to both widgets
struct MatrixRenderData {
    // Viewport bounds
    int startTick, endTick, startLine, endLine;
    int startTimeX, endTimeX, startLineY, endLineY;
    int timeHeight, lineNameWidth;
    double scaleX, scaleY, lineHeight;

    // Event lists
    QList<MidiEvent*>* objects;
    QList<MidiEvent*>* velocityObjects;
    QList<MidiEvent*>* tempoEvents;
    QList<TimeSignatureEvent*>* timeSignatureEvents;
    QList<QPair<int, int>> divs;
    int msOfFirstEventInList;

    // State
    bool colorsByChannels;
    bool screenLocked;
    bool pianoEmulationEnabled;
    int div, measure, tool;

    // UI Areas
    QRectF toolArea, pianoArea, timeLineArea;
    QMap<int, QRect> pianoKeys;

    // Shared constants (defined once in HybridMatrixWidget)
    int numLines, pixelPerS, pixelPerLine, pixelPerEvent;

    // Mouse state for rendering (since MatrixWidget is now a pure renderer)
    int mouseX, mouseY;
    bool mouseOver;

    // File reference for coordinate calculations
    class MidiFile* file;
};

#endif // MATRIXRENDERDATA_H
