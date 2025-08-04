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

#ifndef MISCWIDGET_H_
#define MISCWIDGET_H_

// Project includes
#include "PaintWidget.h"

// Qt includes
#include <QList>

// Forward declarations
class MatrixWidget;
class MidiEvent;
class SelectTool;

// Edit mode constants
#define SINGLE_MODE 0            ///< Single point editing mode
#define LINE_MODE 1              ///< Line drawing mode
#define MOUSE_MODE 2             ///< Mouse-based editing mode

// Editor type constants
#define VelocityEditor 0         ///< Velocity editor mode
#define ControllEditor 1         ///< Control change editor mode
#define PitchBendEditor 2        ///< Pitch bend editor mode
#define KeyPressureEditor 3      ///< Key pressure editor mode
#define ChannelPressureEditor 4  ///< Channel pressure editor mode
#define TempoEditor 5            ///< Tempo editor mode
#define MiscModeEnd 6            ///< End marker for mode enumeration

/**
 * \class MiscWidget
 *
 * \brief Widget for editing various MIDI event properties and controllers.
 *
 * MiscWidget provides a graphical interface for editing different types of
 * MIDI event data that don't fit into the main piano roll view. It supports:
 *
 * - **Velocity editing**: Modify note velocities graphically
 * - **Controller editing**: Edit control change values (CC messages)
 * - **Pitch bend editing**: Modify pitch bend curves
 * - **Pressure editing**: Edit key and channel pressure values
 * - **Tempo editing**: Modify tempo change events
 *
 * Key features:
 * - Multiple editing modes (single point, line drawing, mouse-based)
 * - Real-time visual feedback during editing
 * - Integration with the main matrix widget
 * - Channel and controller selection
 * - Graphical representation of MIDI data values
 *
 * The widget displays MIDI data as graphical elements that can be
 * manipulated directly, providing an intuitive interface for detailed
 * MIDI event editing.
 */
class MiscWidget : public PaintWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new MiscWidget.
     * \param mw The parent MatrixWidget
     * \param parent The parent widget
     */
    MiscWidget(MatrixWidget *mw, QWidget *parent = 0);

    /**
     * \brief Converts a mode constant to a human-readable string.
     * \param mode The mode constant to convert
     * \return String representation of the mode
     */
    static QString modeToString(int mode);

    /**
     * \brief Sets the editor mode (velocity, controller, etc.).
     * \param mode The editor mode constant
     */
    void setMode(int mode);

    /**
     * \brief Sets the editing interaction mode.
     * \param mode The edit mode (SINGLE_MODE, LINE_MODE, MOUSE_MODE)
     */
    void setEditMode(int mode);

public slots:
    /**
     * \brief Sets the active MIDI channel for editing.
     * \param channel The MIDI channel (0-15)
     */
    void setChannel(int channel);

    /**
     * \brief Sets the active controller number for controller editing.
     * \param ctrl The controller number (0-127)
     */
    void setControl(int ctrl);

    // === Widget Integration Support ===

    /**
     * \brief Gets the associated MatrixWidget.
     * \return Pointer to the MatrixWidget
     */
    MatrixWidget *getMatrixWidget() const { return matrixWidget; }

protected:
    /**
     * \brief Handles paint events to draw the editor interface.
     * \param event The paint event
     */
    void paintEvent(QPaintEvent *event);

    /**
     * \brief Handles key press events for editor shortcuts.
     * \param e The key press event
     */
    void keyPressEvent(QKeyEvent *e);

    /**
     * \brief Handles key release events.
     * \param event The key release event
     */
    void keyReleaseEvent(QKeyEvent *event);

    /**
     * \brief Handles mouse release events for editing operations.
     * \param event The mouse release event
     */
    void mouseReleaseEvent(QMouseEvent *event);

    /**
     * \brief Handles mouse press events for editing operations.
     * \param event The mouse press event
     */
    void mousePressEvent(QMouseEvent *event);

    /**
     * \brief Handles mouse leave events.
     * \param event The leave event
     */
    void leaveEvent(QEvent *event);

    /**
     * \brief Handles mouse move events for editing operations.
     * \param event The mouse move event
     */
    void mouseMoveEvent(QMouseEvent *event);

private:
    /** \brief Reference to the parent matrix widget */
    MatrixWidget *matrixWidget;

    /** \brief Edit mode (SINGLE_MODE or LINE_MODE) */
    int edit_mode;

    /** \brief Current editor mode (velocity, controller, etc.) */
    int mode;

    /** \brief Current MIDI channel */
    int channel;

    /** \brief Current controller number */
    int controller;

    /**
     * \brief Resets the editor state.
     */
    void resetState();

    /**
     * \brief Gets the track data for the current mode.
     * \param accordingEvents Optional list of events to consider
     * \return List of time-value pairs representing the track
     */
    QList<QPair<int, int> > getTrack(QList<MidiEvent *> *accordingEvents = 0);

    /**
     * \brief Computes minimum and maximum values for display.
     */
    void computeMinMax();

    /**
     * \brief Processes a MIDI event to extract relevant data.
     * \param e The MIDI event to process
     * \param ok Pointer to receive success flag
     * \return Pair containing time and value data
     */
    QPair<int, int> processEvent(MidiEvent *e, bool *ok);

    /**
     * \brief Interpolates a value from track data at a given position.
     * \param track The track data to interpolate from
     * \param x The X position to interpolate at
     * \return Interpolated value
     */
    double interpolate(QList<QPair<int, int> > track, int x);

    /**
     * \brief Converts X position to MIDI tick.
     * \param x X coordinate in pixels
     * \return MIDI tick value
     */
    int tickOfXPos(int x);

    /**
     * \brief Converts MIDI tick to X position.
     * \param tick MIDI tick value
     * \return X coordinate in pixels
     */
    int xPosOfTick(int tick);

    /**
     * \brief Converts milliseconds to MIDI tick.
     * \param ms Time in milliseconds
     * \return MIDI tick value
     */
    int tickOfMs(int ms);

    /**
     * \brief Converts MIDI tick to milliseconds.
     * \param tick MIDI tick value
     * \return Time in milliseconds
     */
    int msOfTick(int tick);

    /**
     * \brief Converts X position to milliseconds.
     * \param x X coordinate in pixels
     * \return Time in milliseconds
     */
    int msOfXPos(int x);

    /**
     * \brief Converts milliseconds to X position.
     * \param ms Time in milliseconds
     * \return X coordinate in pixels
     */
    int xPosOfMs(int ms);

    /**
     * \brief Converts Y position to value.
     * \param y Y coordinate in pixels
     * \return Value corresponding to the Y position
     */
    int value(double y);

    /**
     * \brief Filters events based on current mode and settings.
     * \param e The MidiEvent to filter
     * \return True if the event should be included
     */
    bool filter(MidiEvent *e);

    // === Value Range ===

    /** \brief Maximum and default values for the current mode */
    int _max, _default;

    // === Single Point Editing ===

    /** \brief Y position for dragging operations */
    int dragY;

    /** \brief Flag indicating if currently dragging */
    bool dragging;

    /** \brief Dummy tool for selection operations */
    SelectTool *_dummyTool;

    /** \brief Current track index */
    int trackIndex;

    // === Free Hand Drawing ===

    /** \brief Curve points for free hand drawing */
    QList<QPair<int, int> > freeHandCurve;

    /** \brief Flag indicating if currently drawing freehand */
    bool isDrawingFreehand;

    // === Line Drawing ===

    /** \brief Line start and end coordinates */
    int lineX, lineY;

    /** \brief Flag indicating if currently drawing a line */
    bool isDrawingLine;
};

#endif // MISCWIDGET_H_
