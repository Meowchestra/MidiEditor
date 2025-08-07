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

    // Forward all public MatrixWidget methods
    void setFile(MidiFile *f) { _matrixWidget->setFile(f); }
    MidiFile *midiFile() { return _matrixWidget->midiFile(); }
    void setScreenLocked(bool b) { _matrixWidget->setScreenLocked(b); }
    bool screenLocked() { return _matrixWidget->screenLocked(); }
    bool eventInWidget(MidiEvent *event) { return _matrixWidget->eventInWidget(event); }

    void setViewport(int startTick, int endTick, int startLine, int endLine) {
        _matrixWidget->setViewport(startTick, endTick, startLine, endLine);
    }

    void setLineNameWidth(int width) { _matrixWidget->setLineNameWidth(width); }
    int getLineNameWidth() const { return _matrixWidget->getLineNameWidth(); }
    int xPosOfMs(int ms) { return _matrixWidget->xPosOfMs(ms); }
    int yPosOfLine(int line) { return _matrixWidget->yPosOfLine(line); }
    int msOfXPos(int x) { return _matrixWidget->msOfXPos(x); }
    int lineAtY(int y) { return _matrixWidget->lineAtY(y); }
    double lineHeight() { return _matrixWidget->lineHeight(); }
    void registerRelayout() { _matrixWidget->registerRelayout(); }
    void updateRenderingSettings() { _matrixWidget->updateRenderingSettings(); }
    void setDiv(int div) { _matrixWidget->setDiv(div); }
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
    // Forward all MatrixWidget slots with appropriate update mechanism
    void timeMsChanged(int ms) {
        _matrixWidget->timeMsChanged(ms);
        repaint(); // Use immediate repaint for smooth playback cursor
    }

    void scrollXChanged(int scrollPositionX) {
        _matrixWidget->scrollXChanged(scrollPositionX);
        // Hidden widget's update() doesn't trigger OpenGL repaint
        update();
    }

    void scrollYChanged(int scrollPositionY) {
        _matrixWidget->scrollYChanged(scrollPositionY);
        // Hidden widget's update() doesn't trigger OpenGL repaint
        update();
    }

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
    void mousePressEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

    void enterEvent(QEnterEvent *event) override;

    void leaveEvent(QEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    // Forward MatrixWidget signals to our signals
    void onObjectListChanged() {
        emit objectListChanged();
    }

    void onSizeChanged(int maxScrollTime, int maxScrollLine, int vX, int vY) {
        emit sizeChanged(maxScrollTime, maxScrollLine, vX, vY);
    }

private:
    /** \brief Internal MatrixWidget instance that handles all the logic */
    MatrixWidget *_matrixWidget;
};

#endif // OPENGLMATRIXWIDGET_H_
