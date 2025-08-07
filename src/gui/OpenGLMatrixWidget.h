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

#ifndef OPENGLMATRIXWIDGET_H_
#define OPENGLMATRIXWIDGET_H_

#include "OpenGLPaintWidget.h"
#include "MatrixWidget.h"

/**
 * \class OpenGLMatrixWidget
 *
 * \brief OpenGL-accelerated version of MatrixWidget for hardware-accelerated MIDI editing.
 *
 * OpenGLMatrixWidget provides the exact same functionality as MatrixWidget but with
 * OpenGL hardware acceleration. It's designed as a drop-in replacement that:
 *
 * - **Inherits from OpenGLPaintWidget**: Gets OpenGL acceleration automatically
 * - **Delegates to MatrixWidget**: Uses composition to reuse all existing MatrixWidget logic
 * - **Transparent Integration**: Same API and behavior as original MatrixWidget
 * - **Performance Boost**: GPU-accelerated rendering for better performance
 *
 * The implementation uses composition rather than inheritance to avoid complex
 * multiple inheritance issues while providing seamless OpenGL acceleration.
 */
class OpenGLMatrixWidget : public OpenGLPaintWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new OpenGL-accelerated MatrixWidget.
     * \param settings Application settings
     * \param parent The parent widget
     */
    OpenGLMatrixWidget(QSettings *settings, QWidget *parent = nullptr);

    /**
     * \brief Destructor.
     */
    ~OpenGLMatrixWidget();

    // === Delegate all MatrixWidget methods ===

    /**
     * \brief Gets the internal MatrixWidget instance.
     * \return Pointer to the internal MatrixWidget
     */
    MatrixWidget *getMatrixWidget() const { return _matrixWidget; }

    // === MatrixWidget API Delegation ===
    // All methods delegate to the internal MatrixWidget instance

    /**
     * \brief Sets the MIDI file to display and edit.
     * \param f The MidiFile to load
     */
    void setFile(MidiFile *f);

    /**
     * \brief Gets the currently loaded MIDI file.
     * \return Pointer to the current MidiFile, or nullptr if none
     */
    MidiFile *midiFile() { return _matrixWidget->midiFile(); }

    /**
     * \brief Sets the screen lock state to prevent auto-scrolling.
     * \param b True to lock the screen, false to allow auto-scrolling
     */
    void setScreenLocked(bool b) { _matrixWidget->setScreenLocked(b); }

    /**
     * \brief Gets the current screen lock state.
     * \return True if screen is locked, false otherwise
     */
    bool screenLocked() { return _matrixWidget->screenLocked(); }

    /**
     * \brief Tests if an event is visible in the current viewport.
     * \param event The MidiEvent to test
     * \return True if the event is within the visible area
     */
    bool eventInWidget(MidiEvent *event) { return _matrixWidget->eventInWidget(event); }

    /**
     * \brief Sets the viewport to display a specific time and pitch range.
     * \param startTick Starting MIDI tick
     * \param endTick Ending MIDI tick
     * \param startLine Starting MIDI note line
     * \param endLine Ending MIDI note line
     */
    void setViewport(int startTick, int endTick, int startLine, int endLine) {
        _matrixWidget->setViewport(startTick, endTick, startLine, endLine);
    }

    /**
     * \brief Gets the width of the piano key area.
     * \return Width in pixels of the piano key area
     */
    int getLineNameWidth() const { return _matrixWidget->getLineNameWidth(); }

    /**
     * \brief Converts milliseconds to X coordinate.
     * \param ms Time in milliseconds
     * \return X coordinate in pixels
     */
    int xPosOfMs(int ms) { return _matrixWidget->xPosOfMs(ms); }

    /**
     * \brief Converts MIDI note line to Y coordinate.
     * \param line MIDI note line number
     * \return Y coordinate in pixels
     */
    int yPosOfLine(int line) { return _matrixWidget->yPosOfLine(line); }

    /**
     * \brief Converts X coordinate to milliseconds.
     * \param x X coordinate in pixels
     * \return Time in milliseconds
     */
    int msOfXPos(int x) { return _matrixWidget->msOfXPos(x); }

    /**
     * \brief Converts Y coordinate to MIDI note line.
     * \param y Y coordinate in pixels
     * \return MIDI note line number
     */
    int lineAtY(int y) { return _matrixWidget->lineAtY(y); }

    /**
     * \brief Gets the height of each piano key line.
     * \return Height in pixels per MIDI note line
     */
    double lineHeight() { return _matrixWidget->lineHeight(); }



    /**
     * \brief Updates rendering settings from application preferences.
     */
    void updateRenderingSettings() { _matrixWidget->updateRenderingSettings(); }

    /**
     * \brief Sets the quantization division.
     * \param div The quantization division value
     */
    void setDiv(int div) { _matrixWidget->setDiv(div); }

    /**
     * \brief Gets the current quantization division.
     * \return The quantization division value
     */
    int div() { return _matrixWidget->div(); }
    void takeKeyPressEvent(QKeyEvent *event) { _matrixWidget->takeKeyPressEvent(event); }
    void takeKeyReleaseEvent(QKeyEvent *event) { _matrixWidget->takeKeyReleaseEvent(event); }
    int minVisibleMidiTime() { return _matrixWidget->minVisibleMidiTime(); }
    int maxVisibleMidiTime() { return _matrixWidget->maxVisibleMidiTime(); }
    QList<MidiEvent *> *activeEvents() { return _matrixWidget->activeEvents(); }
    QList<MidiEvent *> *velocityEvents() { return _matrixWidget->velocityEvents(); }
    QList<GraphicObject *> *getObjects() { return _matrixWidget->getObjects(); }
    bool colorsByChannel() { return _matrixWidget->colorsByChannel(); }
    void setColorsByChannel() { _matrixWidget->setColorsByChannel(); }

public slots:
    // === MatrixWidget Slot Delegation ===
    // All slots delegate to the internal MatrixWidget and trigger OpenGL updates

    /**
     * \brief Updates the playback cursor position.
     * \param ms Current playback time in milliseconds
     * \param ignoreLocked If true, updates even when screen is locked
     */
    void timeMsChanged(int ms, bool ignoreLocked = false) {
        _matrixWidget->timeMsChanged(ms, ignoreLocked);
        update(); // Use asynchronous update for consistent behavior with software rendering
    }

    /**
     * \brief Handles horizontal scroll position changes.
     * \param scrollPositionX New horizontal scroll position
     */
    void scrollXChanged(int scrollPositionX) {
        _matrixWidget->scrollXChanged(scrollPositionX);
        // Hidden widget's update() doesn't trigger OpenGL repaint
        update();
    }

    /**
     * \brief Handles vertical scroll position changes.
     * \param scrollPositionY New vertical scroll position
     */
    void scrollYChanged(int scrollPositionY) {
        _matrixWidget->scrollYChanged(scrollPositionY);
        // Hidden widget's update() doesn't trigger OpenGL repaint
        update();
    }

    /**
     * \brief Zooms in horizontally (time axis).
     */
    void zoomHorIn() {
        _matrixWidget->zoomHorIn();
        // Hidden widget's calcSizes()->update() doesn't trigger OpenGL repaint
        update();
    }

    void zoomHorOut() {
        _matrixWidget->zoomHorOut();
        // Hidden widget's calcSizes()->update() doesn't trigger OpenGL repaint
        update();
    }

    void zoomVerIn() {
        _matrixWidget->zoomVerIn();
        // Hidden widget's calcSizes()->update() doesn't trigger OpenGL repaint
        update();
    }

    void zoomVerOut() {
        _matrixWidget->zoomVerOut();
        // Hidden widget's calcSizes()->update() doesn't trigger OpenGL repaint
        update();
    }

    void zoomStd() {
        _matrixWidget->zoomStd();
        // Hidden widget's calcSizes()->update() doesn't trigger OpenGL repaint
        update();
    }

    void resetView() {
        _matrixWidget->resetView();
        // Hidden widget's calcSizes()->update() doesn't trigger OpenGL repaint
        update();
    }

    void calcSizes() {
        _matrixWidget->calcSizes();
        // Hidden widget's update() doesn't trigger OpenGL repaint
        update();
    }

    /**
     * \brief Registers that a layout recalculation is needed.
     */
    void registerRelayout() {
        _matrixWidget->registerRelayout();
        // No immediate update needed - registerRelayout just marks for later recalc
    }

signals:
    // Forward all MatrixWidget signals
    void objectListChanged();

    void sizeChanged(int maxScrollTime, int maxScrollLine, int vX, int vY);

protected:
    /**
     * \brief Implements OpenGL-accelerated painting by delegating to MatrixWidget.
     * \param painter OpenGL-accelerated QPainter
     */
    void paintContent(QPainter *painter) override;

    // === Event Handlers ===
    // All event handlers delegate to the internal MatrixWidget

    /**
     * \brief Handles mouse press events for note editing.
     * \param event The mouse press event
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * \brief Handles mouse release events for note editing.
     * \param event The mouse release event
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /**
     * \brief Handles mouse move events for note editing and selection.
     * \param event The mouse move event
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * \brief Handles mouse double-click events for note editing.
     * \param event The mouse double-click event
     */
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    /**
     * \brief Handles wheel events for zooming and scrolling.
     * \param event The wheel event
     */
    void wheelEvent(QWheelEvent *event) override;

    /**
     * \brief Handles mouse enter events.
     * \param event The enter event
     */
    void enterEvent(QEnterEvent *event) override;

    /**
     * \brief Handles mouse leave events.
     * \param event The leave event
     */
    void leaveEvent(QEvent *event) override;

    /**
     * \brief Handles resize events to update the internal widget.
     * \param event The resize event
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * \brief Handles key press events for shortcuts and piano emulation.
     * \param event The key press event
     */
    void keyPressEvent(QKeyEvent *event) override;

    /**
     * \brief Handles key release events for piano emulation.
     * \param event The key release event
     */
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    // === Signal Forwarding ===

    /**
     * \brief Forwards objectListChanged signal from internal widget.
     */
    void onObjectListChanged() {
        emit objectListChanged();
    }

    /**
     * \brief Forwards sizeChanged signal from internal widget.
     * \param maxScrollTime Maximum scroll time value
     * \param maxScrollLine Maximum scroll line value
     * \param vX Viewport X position
     * \param vY Viewport Y position
     */
    void onSizeChanged(int maxScrollTime, int maxScrollLine, int vX, int vY) {
        emit sizeChanged(maxScrollTime, maxScrollLine, vX, vY);
    }

private:
    // === Internal Components ===

    /** \brief Internal MatrixWidget instance that handles all the logic */
    MatrixWidget *_matrixWidget;
};

#endif // OPENGLMATRIXWIDGET_H_
