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

#include "OpenGLMatrixWidget.h"
#include "../protocol/Protocol.h"
#include "../midi/MidiFile.h"
#include <QDebug>
#include <QApplication>

OpenGLMatrixWidget::OpenGLMatrixWidget(QSettings *settings, QWidget *parent)
    : OpenGLPaintWidget(settings, parent) {
    // Create internal MatrixWidget instance
    _matrixWidget = new MatrixWidget(settings, this);

    // Hide the internal widget since we'll render its content through OpenGL
    _matrixWidget->hide();

    // Connect signals to forward them
    connect(_matrixWidget, &MatrixWidget::objectListChanged,
            this, &OpenGLMatrixWidget::onObjectListChanged);
    connect(_matrixWidget, &MatrixWidget::sizeChanged,
            this, &OpenGLMatrixWidget::onSizeChanged);

    qDebug() << "OpenGLMatrixWidget: Created with hardware acceleration";
}

OpenGLMatrixWidget::~OpenGLMatrixWidget() {
    // MatrixWidget will be deleted automatically as a child widget
}

void OpenGLMatrixWidget::paintContent(QPainter *painter) {
    // CRITICAL: Ensure the internal MatrixWidget has the same size as us
    QSize logicalSize = size();
    if (_matrixWidget->size() != logicalSize) {
        _matrixWidget->resize(logicalSize);
    }

    // OPTIMIZED DIRECT GPU RENDERING:
    // Use Qt's render() method which provides direct GPU acceleration
    // when used with an OpenGL-backed QPainter. This eliminates any
    // CPU->GPU->CPU->GPU conversions and provides true hardware acceleration.
    // Note: A default empty MIDI file is always loaded, so no null check needed
    _matrixWidget->render(painter, QPoint(0, 0), QRegion(),
                          QWidget::DrawWindowBackground | QWidget::DrawChildren);
}

// === Event Forwarding ===

void OpenGLMatrixWidget::mousePressEvent(QMouseEvent *event) {
    // Call parent to handle OpenGL mouse state
    OpenGLPaintWidget::mousePressEvent(event);

    // Forward to internal MatrixWidget for business logic
    if (_matrixWidget) {
        QApplication::sendEvent(_matrixWidget, event);
        // Use asynchronous update for consistent behavior with software rendering
        // This prevents GPU pipeline stalls during interactive operations
        update();
    }
}

void OpenGLMatrixWidget::mouseReleaseEvent(QMouseEvent *event) {
    // Call parent to handle OpenGL mouse state
    OpenGLPaintWidget::mouseReleaseEvent(event);

    // Forward to internal MatrixWidget for business logic
    if (_matrixWidget) {
        QApplication::sendEvent(_matrixWidget, event);
        // Use asynchronous update for consistent behavior with software rendering
        // This prevents GPU pipeline stalls during interactive operations
        update();
    }
}

void OpenGLMatrixWidget::mouseMoveEvent(QMouseEvent *event) {
    // Call parent to handle OpenGL mouse state
    OpenGLPaintWidget::mouseMoveEvent(event);

    // Forward to internal MatrixWidget for business logic
    if (_matrixWidget) {
        QApplication::sendEvent(_matrixWidget, event);
        // Use asynchronous update for smooth drag operations
        // This prevents GPU pipeline stalls and eliminates flickering during note selection/dragging
        update();
    }
}

void OpenGLMatrixWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    // Forward to internal MatrixWidget for timeline cursor positioning
    if (_matrixWidget) {
        QApplication::sendEvent(_matrixWidget, event);
        // Use asynchronous update for consistent behavior with software rendering
        update();
    }
}

void OpenGLMatrixWidget::resizeEvent(QResizeEvent *event) {
    // Resize internal MatrixWidget to match
    if (_matrixWidget) {
        _matrixWidget->resize(event->size());
    }

    // Let parent handle OpenGL resize
    QOpenGLWidget::resizeEvent(event);
}

void OpenGLMatrixWidget::enterEvent(QEnterEvent *event) {
    // Forward to internal MatrixWidget for piano key hover effects
    if (_matrixWidget) {
        QApplication::sendEvent(_matrixWidget, event);
        // Hidden widget's update() doesn't trigger OpenGL repaint
        update();
    }
}

void OpenGLMatrixWidget::leaveEvent(QEvent *event) {
    // Forward to internal MatrixWidget for piano key hover effects
    if (_matrixWidget) {
        QApplication::sendEvent(_matrixWidget, event);
        // Hidden widget's update() doesn't trigger OpenGL repaint
        update();
    }
}

void OpenGLMatrixWidget::wheelEvent(QWheelEvent *event) {
    // Forward wheel events to internal MatrixWidget for mouse scrolling
    if (_matrixWidget) {
        QApplication::sendEvent(_matrixWidget, event);
        // Hidden widget's zoom/scroll->update() doesn't trigger OpenGL repaint
        update();
    }
}

void OpenGLMatrixWidget::keyPressEvent(QKeyEvent *event) {
    // Forward to internal MatrixWidget
    if (_matrixWidget) {
        _matrixWidget->takeKeyPressEvent(event);
        // Hidden widget's conditional update() doesn't trigger OpenGL repaint
        // Update unconditionally since we can't check the tool's return value
        update();
    }
}

void OpenGLMatrixWidget::keyReleaseEvent(QKeyEvent *event) {
    // Forward to internal MatrixWidget
    if (_matrixWidget) {
        _matrixWidget->takeKeyReleaseEvent(event);
        // Hidden widget's conditional update() doesn't trigger OpenGL repaint
        // Update unconditionally since we can't check the tool's return value
        update();
    }
}

void OpenGLMatrixWidget::setFile(MidiFile *file) {
    // Delegate to internal MatrixWidget
    if (_matrixWidget) {
        // Get the old file to disconnect from its protocol
        MidiFile *oldFile = _matrixWidget->midiFile();
        if (oldFile && oldFile->protocol()) {
            disconnect(oldFile->protocol(), &Protocol::actionFinished, this, &OpenGLMatrixWidget::registerRelayout);
            disconnect(oldFile->protocol(), &Protocol::actionFinished, this, QOverload<>::of(&QWidget::update));
        }

        _matrixWidget->setFile(file);

        // CRITICAL: Connect to protocol actionFinished signal for OpenGL updates
        // The internal MatrixWidget is hidden, so its calls don't trigger OpenGL updates
        // Set up both connections that MatrixWidget has: registerRelayout() and update()
        if (file && file->protocol()) {
            connect(file->protocol(), &Protocol::actionFinished, this, &OpenGLMatrixWidget::registerRelayout);
            connect(file->protocol(), &Protocol::actionFinished, this, QOverload<>::of(&QWidget::update));
        }
    }
}
