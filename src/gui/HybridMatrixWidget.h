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

#ifndef HYBRIDMATRIXWIDGET_H_
#define HYBRIDMATRIXWIDGET_H_

// Qt includes
#include <QWidget>
#include <QStackedWidget>
#include <QSettings>
#include <QEnterEvent>
#include <QApplication>

// Project includes
#include "MatrixRenderData.h"

// Forward declarations
class MatrixWidget;
class AcceleratedMatrixWidget;
class MidiFile;
class MidiEvent;
class GraphicObject;
class NoteOnEvent;
class TempoChangeEvent;
class TimeSignatureEvent;

/**
 * \class HybridMatrixWidget
 *
 * \brief Hybrid widget that manages both software and hardware-accelerated matrix rendering.
 *
 * HybridMatrixWidget provides a unified interface for MIDI matrix editing while
 * automatically switching between different rendering backends based on user
 * preferences and hardware capabilities:
 *
 * - **MatrixWidget**: QPainter-based software rendering (compatible, stable)
 * - **AcceleratedMatrixWidget**: Qt RHI-based hardware rendering (fast, modern)
 *
 * Key features:
 * - Automatic backend switching based on settings
 * - Seamless data transfer between rendering modes
 * - Unified API regardless of active backend
 * - Performance optimization for large MIDI files
 * - Fallback support for systems without GPU drivers / OpenGL
 *
 * The widget acts as a proxy, forwarding all matrix operations to the
 * currently active backend while maintaining state consistency.
 */
class HybridMatrixWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new HybridMatrixWidget.
     * \param parent The parent widget
     */
    explicit HybridMatrixWidget(QWidget *parent = nullptr);

    /**
     * \brief Destroys the HybridMatrixWidget and cleans up resources.
     */
    ~HybridMatrixWidget();

    // === MatrixWidget Interface Forwarding ===

    /**
     * \brief Sets the MIDI file to display.
     * \param file The MidiFile to display
     */
    void setFile(MidiFile *file);

    /**
     * \brief Gets the current MIDI file.
     * \return Pointer to the current MidiFile
     */
    MidiFile *midiFile() const;

    // === View Control ===

    /**
     * \brief Sets the viewport bounds for the matrix display.
     * \param startTick Start time in MIDI ticks
     * \param endTick End time in MIDI ticks
     * \param startLine Start MIDI line (note number)
     * \param endLine End MIDI line (note number)
     */
    void setViewport(int startTick, int endTick, int startLine, int endLine);

    /**
     * \brief Sets the height of each line in pixels.
     * \param height Line height in pixels
     */
    void setLineHeight(double height);

    // Note: lineHeight() declared below in coordinate methods section

    // === Hardware Acceleration Control ===

    /**
     * \brief Checks if hardware acceleration is currently active.
     * \return True if using hardware acceleration
     */
    bool isHardwareAccelerated() const;

    /**
     * \brief Checks if hardware acceleration is available on this system.
     * \return True if hardware acceleration can be used
     */
    bool canUseHardwareAcceleration() const;

    /**
     * \brief Gets the hardware acceleration enabled setting.
     * \return True if hardware acceleration is enabled in settings
     */
    bool getHardwareAccelerationEnabled() const { return _hardwareAccelerationEnabled; }

    /**
     * \brief Enables or disables hardware acceleration.
     * \param enabled True to enable hardware acceleration
     */
    void setHardwareAcceleration(bool enabled);

    // === Performance Information ===

    /**
     * \brief Gets performance information about the current backend.
     * \return String containing performance metrics and backend info
     */
    QString getPerformanceInfo() const;

    // === Complete MatrixWidget Interface Forwarding ===

    /**
     * \brief Sets the screen locked state.
     * \param locked True to lock the screen, false to unlock
     */
    void setScreenLocked(bool locked);

    /**
     * \brief Gets the screen locked state.
     * \return True if screen is locked
     */
    bool screenLocked();

    /**
     * \brief Gets the minimum visible MIDI time.
     * \return Minimum visible time in MIDI ticks
     */
    int minVisibleMidiTime();

    /**
     * \brief Gets the maximum visible MIDI time.
     * \return Maximum visible time in MIDI ticks
     */
    int maxVisibleMidiTime();

    /**
     * \brief Sets the width of the line name area.
     * \param width Width in pixels
     */
    void setLineNameWidth(int width);

    /**
     * \brief Gets the width of the line name area.
     * \return Width in pixels
     */
    int getLineNameWidth() const;

    /**
     * \brief Gets the list of graphical objects.
     * \return Pointer to list of GraphicObject instances
     */
    QList<GraphicObject *> *getObjects();

    /**
     * \brief Gets the MIDI file (duplicate method for compatibility).
     * \return Pointer to the current MidiFile
     */
    MidiFile *midiFile();

    // === Color Management ===

    /**
     * \brief Sets whether to color events by channels.
     * \param enabled True to color by channels, false otherwise
     */
    void setColorsByChannels(bool enabled);

    /**
     * \brief Gets whether events are colored by channels.
     * \return True if coloring by channels
     */
    bool colorsByChannels() const;

    /**
     * \brief Sets coloring mode to color by channels.
     */
    void setColorsByChannel();

    /**
     * \brief Sets coloring mode to color by tracks.
     */
    void setColorsByTracks();

    /**
     * \brief Gets whether events are colored by channel (singular).
     * \return True if coloring by channel
     */
    bool colorsByChannel() const;

    // === Display Settings ===

    /**
     * \brief Gets the current division setting.
     * \return Current division value
     */
    int div() const;

    /**
     * \brief Sets the current measure.
     * \param measure The measure number to set
     */
    void setMeasure(int measure);

    /**
     * \brief Gets the current measure.
     * \return Current measure number
     */
    int measure() const;

    /**
     * \brief Sets the current tool.
     * \param tool The tool identifier to set
     */
    void setTool(int tool);

    /**
     * \brief Gets the current tool.
     * \return Current tool identifier
     */
    int tool() const;

    // === Piano Emulation ===

    /**
     * \brief Gets the piano emulation state.
     * \return True if piano emulation is enabled
     */
    bool getPianoEmulation() const;

    /**
     * \brief Sets the piano emulation state.
     * \param enabled True to enable piano emulation
     */
    void setPianoEmulation(bool enabled);

    // === Core Business Logic Methods (moved from MatrixWidget) ===

    /**
     * \brief Gets the list of velocity events.
     * \return Pointer to list of velocity events
     */
    QList<MidiEvent *> *velocityEvents() { return _velocityObjects; }

    /**
     * \brief Gets the list of active events.
     * \return Pointer to list of active events
     */
    QList<MidiEvent *> *activeEvents() { return _objects; }

    /**
     * \brief Gets the current division markers.
     * \return List of division marker pairs
     */
    QList<QPair<int, int> > divs() { return _currentDivs; }

    // === UI Areas (moved from MatrixWidget) ===

    /**
     * \brief Gets the tool area rectangle.
     * \return QRectF defining the tool area
     */
    QRectF toolArea() const { return _toolArea; }

    /**
     * \brief Gets the piano area rectangle.
     * \return QRectF defining the piano area
     */
    QRectF pianoArea() const { return _pianoArea; }

    /**
     * \brief Gets the timeline area rectangle.
     * \return QRectF defining the timeline area
     */
    QRectF timeLineArea() const { return _timeLineArea; }

    /**
     * \brief Gets the piano key rectangles.
     * \return QMap of piano key rectangles indexed by note number
     */
    QMap<int, QRect> pianoKeys() const { return _pianoKeys; }

    // === Coordinate Conversion Methods (business logic) ===

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
     * \brief Converts MIDI ticks to milliseconds.
     * \param tick Time in MIDI ticks
     * \return Time in milliseconds
     */
    int msOfTick(int tick);

    /**
     * \brief Converts MIDI line to Y position.
     * \param line MIDI line number (note number)
     * \return Y coordinate in pixels
     */
    int yPosOfLine(int line);

    /**
     * \brief Converts Y position to MIDI line.
     * \param y Y coordinate in pixels
     * \return MIDI line number (note number)
     */
    int lineAtY(int y);

    /**
     * \brief Gets the calculated line height based on viewport.
     * \return Line height in pixels
     */
    double lineHeight() const;

    /**
     * \brief Gets the stored line height value.
     * \return Stored line height in pixels
     */
    double getStoredLineHeight() const;

    /**
     * \brief Converts width to time in milliseconds.
     * \param w Width in pixels
     * \return Time duration in milliseconds
     */
    int timeMsOfWidth(int w);

    /**
     * \brief Checks if an event is visible in the widget.
     * \param event The MidiEvent to check
     * \return True if the event is visible
     */
    bool eventInWidget(MidiEvent *event);

    // === UI Methods for Tools ===

    /**
     * \brief Sets the cursor for the widget.
     * \param cursor The cursor to set
     */
    void setCursor(const QCursor &cursor) { QWidget::setCursor(cursor); }

    // === Piano Emulation ===

    /**
     * \brief Plays a note for preview.
     * \param note The MIDI note number to play
     */
    void playNote(int note);

    // === Tool Interaction (business logic for both renderers) ===

    /**
     * \brief Handles tool mouse press events.
     * \param event The mouse press event
     */
    void handleToolPress(QMouseEvent *event);

    /**
     * \brief Handles tool mouse release events.
     * \param event The mouse release event
     */
    void handleToolRelease(QMouseEvent *event);

    /**
     * \brief Handles tool mouse move events.
     * \param event The mouse move event
     */
    void handleToolMove(QMouseEvent *event);

    /**
     * \brief Handles tool enter events.
     */
    void handleToolEnter();

    /**
     * \brief Handles tool exit events.
     */
    void handleToolExit();

    // === Additional Methods from MatrixWidget ===

    /**
     * \brief Forces a complete redraw of the widget.
     */
    void forceCompleteRedraw();

protected:
    // === CRITICAL: Mouse and Keyboard Event Handling (moved from MatrixWidget) ===

    /**
     * \brief Handles paint events for widget rendering.
     * \param event The paint event
     */
    void paintEvent(QPaintEvent *event) override;

    /**
     * \brief Handles mouse move events.
     * \param event The mouse move event
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * \brief Handles resize events to update layout.
     * \param event The resize event
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * \brief Handles mouse enter events (Qt6 uses QEnterEvent).
     * \param event The enter event
     */
    void enterEvent(QEnterEvent *event) override;

    /**
     * \brief Handles mouse leave events.
     * \param event The leave event
     */
    void leaveEvent(QEvent *event) override;

    /**
     * \brief Handles mouse press events.
     * \param event The mouse press event
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * \brief Handles mouse double-click events.
     * \param event The mouse double-click event
     */
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    /**
     * \brief Handles mouse release events.
     * \param event The mouse release event
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /**
     * \brief Handles key press events.
     * \param event The key press event
     */
    void keyPressEvent(QKeyEvent *event) override;

    /**
     * \brief Handles key release events.
     * \param event The key release event
     */
    void keyReleaseEvent(QKeyEvent *event) override;

    /**
     * \brief Handles mouse wheel events.
     * \param event The wheel event
     */
    void wheelEvent(QWheelEvent *event) override;

public slots:
    // === Scroll and Zoom Methods (must be slots for signal connections) ===

    /**
     * \brief Handles horizontal scroll position changes.
     * \param scrollPositionX The new horizontal scroll position
     */
    void scrollXChanged(int scrollPositionX);

    /**
     * \brief Handles vertical scroll position changes.
     * \param scrollPositionY The new vertical scroll position
     */
    void scrollYChanged(int scrollPositionY);

    /**
     * \brief Zooms in horizontally.
     */
    void zoomHorIn();

    /**
     * \brief Zooms out horizontally.
     */
    void zoomHorOut();

    /**
     * \brief Zooms in vertically.
     */
    void zoomVerIn();

    /**
     * \brief Zooms out vertically.
     */
    void zoomVerOut();

    /**
     * \brief Resets zoom to standard level.
     */
    void zoomStd();

    /**
     * \brief Resets the view to default settings.
     */
    void resetView();

    /**
     * \brief Handles time position changes.
     * \param ms The new time in milliseconds
     * \param ignoreLocked Whether to ignore screen lock state
     */
    void timeMsChanged(int ms, bool ignoreLocked = false);

    /**
     * \brief Registers a request for layout recalculation.
     */
    void registerRelayout();

    /**
     * \brief Sets the division setting.
     * \param div The new division value
     */
    void setDiv(int div);

    /**
     * \brief Calculates UI area sizes.
     */
    void calcSizes();

    /**
     * \brief Handles key press events from external sources.
     * \param event The key press event
     */
    void takeKeyPressEvent(QKeyEvent *event);

    /**
     * \brief Handles key release events from external sources.
     * \param event The key release event
     */
    void takeKeyReleaseEvent(QKeyEvent *event);

signals:
    /**
     * \brief Emitted when a MIDI event is clicked.
     * \param event The clicked MidiEvent
     */
    void eventClicked(MidiEvent *event);

    /**
     * \brief Emitted when the viewport changes.
     * \param startTick The new start tick
     * \param endTick The new end tick
     */
    void viewportChanged(int startTick, int endTick);

    /**
     * \brief Emitted when hardware acceleration status changes.
     * \param isAccelerated True if hardware acceleration is active
     */
    void accelerationStatusChanged(bool isAccelerated);

    // === MatrixWidget Signals Forwarding ===

    /**
     * \brief Emitted when the widget size changes.
     * \param maxScrollTime Maximum scroll time value
     * \param maxScrollLine Maximum scroll line value
     * \param valueX Current X scroll value
     * \param valueY Current Y scroll value
     */
    void sizeChanged(int maxScrollTime, int maxScrollLine, int valueX, int valueY);

    /**
     * \brief Emitted when the object list changes.
     */
    void objectListChanged();

    /**
     * \brief Emitted when scroll position changes.
     * \param startMs Start time in milliseconds
     * \param maxMs Maximum time in milliseconds
     * \param startLine Start line number
     * \param maxLine Maximum line number
     */
    void scrollChanged(int startMs, int maxMs, int startLine, int maxLine);

private slots:
    /**
     * \brief Handles hardware acceleration widget failures.
     */
    void onAcceleratedWidgetFailed();

    /**
     * \brief Handles viewport changes from child widgets.
     * \param startTick The new start tick
     * \param endTick The new end tick
     */
    void onViewportChanged(int startTick, int endTick);

private:
    // === Widget Management ===

    /**
     * \brief Sets up the child widgets (software and hardware renderers).
     */
    void setupWidgets();

    /**
     * \brief Connects signals from child widgets.
     */
    void connectWidgetSignals();

    /**
     * \brief Switches to software rendering mode.
     */
    void switchToSoftwareRendering();

    /**
     * \brief Switches to hardware rendering mode.
     */
    void switchToHardwareRendering();

    /**
     * \brief Syncs viewport from the active child widget.
     */
    void syncViewportFromActiveWidget();

    /**
     * \brief Creates render data for widget switching.
     * \return MatrixRenderData structure with current state
     */
    MatrixRenderData createRenderData();

    /**
     * \brief Forwards signals from child widgets.
     */
    void forwardSignals();

    // === Business Logic Helper Methods (moved from MatrixWidget) ===

    /**
     * \brief Updates the event lists for display.
     */
    void updateEventLists();

    /**
     * \brief Updates event lists for a specific channel.
     * \param channel The MIDI channel to update
     */
    void updateEventListsForChannel(int channel);

    /**
     * \brief Processes an event for display purposes.
     * \param event The MidiEvent to process
     */
    void processEventForDisplay(MidiEvent *event);

    /**
     * \brief Updates the division markers.
     */
    void updateDivisions();

    /**
     * \brief Updates the tempo event list.
     */
    void updateTempoEvents();

    /**
     * \brief Updates the time signature event list.
     */
    void updateTimeSignatureEvents();

    /**
     * \brief Handles piano emulation from keyboard events.
     * \param event The key event to process
     */
    void pianoEmulator(QKeyEvent *event);

    /**
     * \brief Calculates viewport bounds for rendering.
     */
    void calculateViewportBounds();

    /**
     * \brief Calculates piano key rectangles.
     */
    void calculatePianoKeys();

    /**
     * \brief Emits the sizeChanged signal with current values.
     */
    void emitSizeChanged();

    /**
     * \brief Internal implementation of time position changes.
     * \param ms The new time in milliseconds
     * \param ignoreLocked Whether to ignore screen lock state
     */
    void timeMsChangedInternal(int ms, bool ignoreLocked);

    // === Helper Methods for Mouse Interaction ===

    /**
     * \brief Checks if mouse is within a rectangle.
     * \param rect The rectangle to check
     * \return True if mouse is within the rectangle
     */
    bool mouseInRect(const QRectF &rect);

    // === Smart Update Methods ===

    /**
     * \brief Performs async update for non-critical updates.
     */
    void smartUpdate();

    /**
     * \brief Performs immediate update for real-time updates.
     */
    void smartRepaint();

    // === Widget Management ===

    /** \brief Stacked widget containing both renderers */
    QStackedWidget *_stackedWidget;

    /** \brief Software-based matrix renderer */
    MatrixWidget *_softwareWidget;

    /** \brief Hardware-accelerated matrix renderer */
    AcceleratedMatrixWidget *_hardwareWidget;

    // === State Management ===

    /** \brief Settings storage */
    QSettings *_settings;

    /** \brief Whether hardware acceleration is enabled in settings */
    bool _hardwareAccelerationEnabled;

    /** \brief Whether hardware acceleration is available on this system */
    bool _hardwareAccelerationAvailable;

    /** \brief Whether currently using hardware acceleration */
    bool _currentlyUsingHardware;

    /** \brief Current MIDI file */
    MidiFile *_currentFile;

    // === Core Business Logic State (moved from MatrixWidget) ===

    /** \brief Viewport bounds in ticks and lines */
    int _startTick, _endTick, _startLine, _endLine;

    /** \brief Viewport bounds in screen coordinates */
    int _startTimeX, _endTimeX, _startLineY, _endLineY;

    /** \brief Timeline height and timing reference */
    int _timeHeight, _msOfFirstEventInList;

    /** \brief Line height in pixels */
    double _lineHeight;

    /** \brief Width of the line name area */
    int _lineNameWidth;

    /** \brief Scaling factors for coordinate conversion */
    double _scaleX, _scaleY;

    /** \brief Whether the screen is locked */
    bool _screenLocked;

    // === Additional State for MatrixWidget Compatibility ===

    /** \brief Whether to color events by channels */
    bool _colorsByChannels;

    /** \brief Current division setting */
    int _div;

    /** \brief Current measure number */
    int _measure;

    /** \brief Current tool identifier */
    int _tool;

    /** \brief Whether piano emulation is enabled */
    bool _pianoEmulationEnabled;

    // === Mouse State for Rendering ===

    /** \brief Current mouse coordinates */
    int _mouseX, _mouseY;

    /** \brief Whether mouse is over the widget */
    bool _mouseOver;

    // === Event Management ===

    /** \brief List of main MIDI events for display */
    QList<MidiEvent *> *_objects;

    /** \brief List of velocity events for display */
    QList<MidiEvent *> *_velocityObjects;

    /** \brief List of current tempo events */
    QList<MidiEvent *> *_currentTempoEvents;

    /** \brief List of current time signature events */
    QList<class TimeSignatureEvent *> *_currentTimeSignatureEvents;

    /** \brief List of current division markers */
    QList<QPair<int, int> > _currentDivs;

    // === Piano Emulation ===

    /** \brief Current piano emulation event */
    class NoteOnEvent *_pianoEvent;

    /** \brief Map of piano key rectangles */
    QMap<int, QRect> _pianoKeys;

    // === UI Areas (moved from MatrixWidget) ===

    /** \brief UI area rectangles */
    QRectF _toolArea, _pianoArea, _timeLineArea;

    // === Performance Monitoring ===

    /** \brief Last performance information string */
    QString _lastPerformanceInfo;

    // === Initialization State ===

    /** \brief Whether currently initializing */
    bool _initializing;

    /** \brief Whether constructor has completed */
    bool _constructorComplete;
};

#endif // HYBRIDMATRIXWIDGET_H_
