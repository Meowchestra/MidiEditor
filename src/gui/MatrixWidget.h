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

#ifndef MATRIXWIDGET_H_
#define MATRIXWIDGET_H_

// Project includes
#include "PaintWidget.h"
#include "MatrixRenderData.h"

// Qt includes
#include <QApplication>
#include <QMap>
#include <QAtomicInt>
#include <QMouseEvent>
#include <QMutex>
#include <QPainter>
#include <QWidget>

// Forward declarations
class MidiFile;
class TempoChangeEvent;
class TimeSignatureEvent;
class MidiEvent;
class GraphicObject;
class NoteOnEvent;
class QSettings;
class MatrixRenderTask;

/**
 * \class MatrixWidget
 *
 * \brief The main visual editor widget for MIDI notes and events.
 *
 * MatrixWidget is the core visual component of the MIDI editor, providing:
 *
 * - **Piano roll display**: Visual representation of MIDI notes as rectangles
 * - **Multi-track support**: Display and editing of multiple MIDI tracks
 * - **Real-time rendering**: Efficient drawing of large MIDI files
 * - **Tool integration**: Support for various editing tools
 * - **Coordinate system**: Conversion between screen coordinates and MIDI data
 * - **Viewport management**: Scrolling and zooming capabilities
 *
 * The widget works in conjunction with HybridMatrixWidget to provide both
 * software and hardware-accelerated rendering paths. It receives render data
 * and focuses on the visual presentation of MIDI content.
 *
 * Key features:
 * - Asynchronous rendering for smooth performance
 * - Customizable appearance and colors
 * - Grid and measure line display
 * - Event selection and highlighting
 * - Integration with the tool system
 */
class MatrixWidget : public PaintWidget {
    Q_OBJECT
    friend class HybridMatrixWidget; // Allow access to private methods
    friend class MatrixRenderTask; // Allow access to async rendering members

public:
    /**
     * \brief Creates a new MatrixWidget.
     * \param parent The parent widget
     */
    MatrixWidget(QWidget *parent = 0);

    /**
     * \brief Destroys the MatrixWidget and cleans up resources.
     */
    ~MatrixWidget();

    // === Pure Renderer Interface ===

    /**
     * \brief Sets the render data for the matrix display.
     * \param data The MatrixRenderData containing all rendering information
     *
     * This is the primary interface for updating the widget's display.
     * All rendering information is passed through this structure.
     */
    void setRenderData(const MatrixRenderData &data);

    // === Legacy Interface ===

    /**
     * \brief Sets the MIDI file for the widget (legacy interface).
     * \param file The MidiFile to display
     */
    void setFile(MidiFile *file);

    /**
     * \brief Gets the current MIDI file.
     * \return Pointer to the current MidiFile
     */
    MidiFile *midiFile();

    // Tool interface methods handled by HybridMatrixWidget

    // === Coordinate Methods (using cached render data) ===

    /**
     * \brief Gets the height of each line in pixels.
     * \return Line height in pixels
     */
    double lineHeight();

    /**
     * \brief Gets the MIDI line number at a given Y coordinate.
     * \param y The Y coordinate in pixels
     * \return The MIDI line number (note number)
     */
    int lineAtY(int y);

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

    /**
     * \brief Gets the Y coordinate for a given MIDI line.
     * \param line The MIDI line number (note number)
     * \return The Y coordinate in pixels
     */
    int yPosOfLine(int line);

    /**
     * \brief Sets the screen locked state.
     * \param b True to lock the screen, false to unlock
     */
    void setScreenLocked(bool b);

    /**
     * \brief Gets the screen locked state.
     * \return True if screen is locked
     */
    bool screenLocked();

    // Coordinate methods for tools handled by HybridMatrixWidget

    // === Pure Renderer Interface ===

    /**
     * \brief Gets the width of the line name area.
     * \return Width in pixels of the line name area
     */
    int getLineNameWidth() const { return lineNameWidth; }

    /**
     * \brief Gets the list of graphical objects for rendering.
     * \return Pointer to list of GraphicObject instances
     */
    QList<GraphicObject *> *getObjects() { return reinterpret_cast<QList<GraphicObject *> *>(objects); }

    /**
     * \brief Converts MIDI ticks to milliseconds.
     * \param tick Time in MIDI ticks
     * \return Time in milliseconds
     */
    int msOfTick(int tick);

    /**
     * \brief Converts milliseconds to X position (needed for rendering calculations).
     * \param ms Time in milliseconds
     * \return X coordinate in pixels
     */
    int xPosOfMs(int ms);

    /**
     * \brief Converts X position to milliseconds (needed for rendering calculations).
     * \param x X coordinate in pixels
     * \return Time in milliseconds
     */
    int msOfXPos(int x);

public slots:
    // === Rendering-Specific Methods ===

    /**
     * \brief Registers a request for layout recalculation.
     */
    void registerRelayout();

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

private:
    /**
     * \brief Calculates UI area sizes from render data.
     */
    void calcSizes();

    /**
     * \brief Forces a complete redraw of the widget.
     */
    void forceCompleteRedraw();

    // === Async Rendering Methods ===

    /**
     * \brief Starts asynchronous rendering process.
     */
    void startAsyncRendering();

    /**
     * \brief Renders the pixmap asynchronously.
     */
    void renderPixmapAsync();

signals:
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

protected:
    /**
     * \brief Handles paint events for widget rendering.
     * \param event The paint event
     */
    void paintEvent(QPaintEvent *event);

    /**
     * \brief Handles resize events to update layout.
     * \param event The resize event
     */
    void resizeEvent(QResizeEvent *event);

    /**
     * \brief Handles mouse enter events.
     * \param event The enter event
     */
    void enterEvent(QEvent *event);

    /**
     * \brief Handles mouse leave events.
     * \param event The leave event
     */
    void leaveEvent(QEvent *event);

    /**
     * \brief Handles mouse double-click events.
     * \param event The mouse double-click event
     */
    void mouseDoubleClickEvent(QMouseEvent *event);

private:
    // === PURE RENDERER: Rendering Methods ===

    /**
     * \brief Paints all events for a specific channel.
     * \param painter The QPainter to render with
     * \param channel The MIDI channel to paint
     */
    void paintChannel(QPainter *painter, int channel);

    /**
     * \brief Paints a single piano key.
     * \param painter The QPainter to render with
     * \param number The MIDI note number
     * \param x X coordinate of the key
     * \param y Y coordinate of the key
     * \param width Width of the key
     * \param height Height of the key
     */
    void paintPianoKey(QPainter *painter, int number, int x, int y,
                       int width, int height);

    // === Performance Optimization Methods ===

    /**
     * \brief Batch draws multiple events with the same color.
     * \param painter The QPainter to render with
     * \param events List of events to draw
     * \param color The color to use for all events
     */
    void batchDrawEvents(QPainter *painter, const QList<MidiEvent *> &events, const QColor &color);

    /**
     * \brief Clears the software rendering cache.
     */
    void clearSoftwareCache();

    // === Viewport Caching for Performance ===

    /** \brief Cached viewport bounds for performance optimization */
    int _lastStartTick, _lastEndTick, _lastStartLineY, _lastEndLineY;
    double _lastScaleX, _lastScaleY;

    // === Qt6 Threading Optimizations ===

    /** \brief Whether to use asynchronous rendering */
    bool _useAsyncRendering;

    /** \brief Mutex for thread-safe rendering */
    QMutex _renderMutex;

    /** \brief Thread-safe flag for rendering state */
    QAtomicInt _renderingInProgress;

    // === Performance Settings ===

    /** \brief Application settings for performance configuration */
    QSettings *_settings;

    // === Cached Render Data from HybridMatrixWidget ===

    /** \brief Render data received from HybridMatrixWidget */
    MatrixRenderData *_renderData;

    // === Rendering-Specific State ===

    /** \brief MIDI file (kept for signal connections) */
    MidiFile *file;

    /** \brief Pixmap cache for software rendering */
    QPixmap *pixmap;

    /** \brief UI areas from render data */
    QRectF ToolArea, PianoArea, TimeLineArea;

    /** \brief Piano key areas from render data */
    QMap<int, QRect> pianoKeys;

    // === Legacy State for Compatibility (from render data) ===

    /** \brief Viewport and coordinate information */
    int startTick, endTick, startTimeX, endTimeX, startLineY, endLineY,
            lineNameWidth, timeHeight, msOfFirstEventInList;

    /** \brief Scaling factors */
    double scaleX, scaleY;

    /** \brief Display state flags */
    bool screen_locked;
    bool _colorsByChannels;
    int _div;

    // === Event Lists from Render Data ===

    /** \brief Main event lists for rendering */
    QList<MidiEvent *> *objects, *velocityObjects;

    /** \brief Tempo events for timeline */
    QList<MidiEvent *> *currentTempoEvents;

    /** \brief Time signature events for timeline */
    QList<TimeSignatureEvent *> *currentTimeSignatureEvents;

    /** \brief Division markers for grid display */
    QList<QPair<int, int> > currentDivs;

    /** \brief Piano emulation (kept for compatibility) */
    NoteOnEvent *pianoEvent;

    //here num 0 is key E.
    static const unsigned sharp_strip_mask = (1 << 4) | (1 << 6) | (1 << 9) | (1 << 11) | (1 << 1);
};

#endif // MATRIXWIDGET_H_