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

#ifndef IMATRIXWIDGET_H
#define IMATRIXWIDGET_H

#include <QList>
#include <QRectF>

class MidiFile;
class MidiEvent;
class GraphicObject;
class QKeyEvent;

/**
 * \class IMatrixWidget
 *
 * \brief Interface for MatrixWidget implementations.
 *
 * This interface defines the common API that both software (MatrixWidget) and
 * hardware-accelerated (RhiMatrixWidget) implementations must provide. It allows
 * tools and other components to work with either implementation transparently.
 *
 * **Key Design Principles:**
 * - **Unified API**: Both software and hardware widgets implement this interface
 * - **Tool Compatibility**: All editing tools work with either implementation
 * - **State Management**: Common methods for viewport, zoom, and file management
 * - **Event Handling**: Consistent coordinate conversion and event testing
 */
class IMatrixWidget {
public:
    virtual ~IMatrixWidget() = default;

    // === File Management ===
    virtual void setFile(MidiFile *file) = 0;
    virtual MidiFile *midiFile() = 0;

    // === Event Lists ===
    virtual QList<MidiEvent *> *activeEvents() = 0;
    virtual QList<MidiEvent *> *velocityEvents() = 0;
    virtual QList<GraphicObject *> *getObjects() = 0;

    // === Coordinate Conversion ===
    virtual double lineHeight() = 0;
    virtual int lineAtY(int y) = 0;
    virtual int yPosOfLine(int line) = 0;
    virtual int msOfXPos(int x) = 0;
    virtual int xPosOfMs(int ms) = 0;
    virtual int msOfTick(int tick) = 0;
    virtual int timeMsOfWidth(int w) = 0;

    // === Viewport ===
    virtual int minVisibleMidiTime() = 0;
    virtual int maxVisibleMidiTime() = 0;
    virtual bool eventInWidget(MidiEvent *event) = 0;

    // === Layout ===
    virtual void setLineNameWidth(int width) = 0;
    virtual int getLineNameWidth() = 0;
    virtual void setTimeHeight(int height) = 0;
    virtual int getTimeHeight() = 0;

    // === Scaling ===
    virtual void setScaleX(double scale) = 0;
    virtual void setScaleY(double scale) = 0;
    virtual double getScaleX() = 0;
    virtual double getScaleY() = 0;

    // === Configuration ===
    virtual void setDiv(int div) = 0;
    virtual int getDiv() = 0;
    virtual int div() = 0;
    virtual QList<QPair<int, int> > divs() = 0;
    virtual void setColorsByChannels(bool byChannels) = 0;
    virtual bool colorsByChannels() = 0;
    virtual void setColorsByChannel() = 0;
    virtual void setColorsByTracks() = 0;
    virtual bool colorsByChannel() = 0;

    // === Piano Emulation ===
    virtual bool getPianoEmulation() = 0;
    virtual void setPianoEmulation(bool mode) = 0;

    // === State ===
    virtual void setScreenLocked(bool locked) = 0;
    virtual bool screenLocked() = 0;
    virtual bool isEnabled() = 0;

    // === View Control ===
    virtual void resetView() = 0;
    virtual void zoomHorIn() = 0;
    virtual void zoomHorOut() = 0;
    virtual void zoomVerIn() = 0;
    virtual void zoomVerOut() = 0;
    virtual void zoomStd() = 0;

    // === Event Handling ===
    virtual void takeKeyPressEvent(QKeyEvent *event) = 0;
    virtual void takeKeyReleaseEvent(QKeyEvent *event) = 0;

    // === Widget Interface ===
    virtual void update() = 0;
    virtual void setEnabled(bool enabled) = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;

    // === Time and Layout ===
    virtual void timeMsChanged(int ms, bool ignoreLocked) = 0;
    virtual void registerRelayout() = 0;
};

#endif // IMATRIXWIDGET_H
