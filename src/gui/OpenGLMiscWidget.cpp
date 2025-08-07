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

#include "OpenGLMiscWidget.h"
#include "Appearance.h"
#include <QDebug>
#include <QApplication>

OpenGLMiscWidget::OpenGLMiscWidget(MatrixWidget *matrixWidget, QSettings *settings, QWidget *parent)
    : OpenGLPaintWidget(settings, parent) {
    // Create internal MiscWidget instance
    _miscWidget = new MiscWidget(matrixWidget, this);

    // Hide the internal widget since we'll render its content through OpenGL
    _miscWidget->hide();

    // Also connect to the MatrixWidget's objectListChanged signal for updates
    connect(matrixWidget, &MatrixWidget::objectListChanged,
            this, QOverload<>::of(&QWidget::update));

    qDebug() << "OpenGLMiscWidget: Created with hardware acceleration";
}

OpenGLMiscWidget::~OpenGLMiscWidget() {
    // MiscWidget will be deleted automatically as a child widget
}

void OpenGLMiscWidget::paintContent(QPainter *painter) {
    // CRITICAL: Ensure the internal MiscWidget has the same size as us
    QSize logicalSize = size();
    if (_miscWidget->size() != logicalSize) {
        _miscWidget->resize(logicalSize);
    }

    // OPTIMIZED DIRECT GPU RENDERING:
    // Use Qt's render() method which provides direct GPU acceleration
    // when used with an OpenGL-backed QPainter. This eliminates any
    // CPU->GPU->CPU->GPU conversions and provides true hardware acceleration.
    // Note: A default empty MIDI file is always loaded, so no null check needed
    _miscWidget->render(painter, QPoint(0, 0), QRegion(),
                        QWidget::DrawWindowBackground | QWidget::DrawChildren);
}

// === Event Forwarding ===

void OpenGLMiscWidget::mousePressEvent(QMouseEvent *event) {
    // Call parent to handle OpenGL mouse state
    OpenGLPaintWidget::mousePressEvent(event);

    // Forward to internal MiscWidget for business logic
    if (_miscWidget) {
        QApplication::sendEvent(_miscWidget, event);
        // Use asynchronous update for consistent behavior with software rendering
        // This prevents GPU pipeline stalls during interactive operations
        update();
    }
}

void OpenGLMiscWidget::mouseReleaseEvent(QMouseEvent *event) {
    // Call parent to handle OpenGL mouse state
    OpenGLPaintWidget::mouseReleaseEvent(event);

    // Forward to internal MiscWidget for business logic
    if (_miscWidget) {
        QApplication::sendEvent(_miscWidget, event);
        // Use asynchronous update for consistent behavior with software rendering
        // This prevents GPU pipeline stalls during interactive operations
        update();
    }
}

void OpenGLMiscWidget::mouseMoveEvent(QMouseEvent *event) {
    // Call parent to handle OpenGL mouse state
    OpenGLPaintWidget::mouseMoveEvent(event);

    // Forward to internal MiscWidget for business logic
    if (_miscWidget) {
        QApplication::sendEvent(_miscWidget, event);
        // Use asynchronous update for smooth drag operations
        // This prevents GPU pipeline stalls and eliminates flickering during editing
        update();
    }
}

void OpenGLMiscWidget::wheelEvent(QWheelEvent *event) {
    // Forward wheel events to internal MiscWidget
    if (_miscWidget) {
        QApplication::sendEvent(_miscWidget, event);
        update(); // Smooth scrolling
    }
}

void OpenGLMiscWidget::enterEvent(QEnterEvent *event) {
    // Forward to internal MiscWidget for hover effects
    if (_miscWidget) {
        QApplication::sendEvent(_miscWidget, event);
        // Hidden widget's update() doesn't trigger OpenGL repaint
        update();
    }
}

void OpenGLMiscWidget::leaveEvent(QEvent *event) {
    // Forward to internal MiscWidget for hover effects
    if (_miscWidget) {
        QApplication::sendEvent(_miscWidget, event);
        // Hidden widget's update() doesn't trigger OpenGL repaint
        update();
    }
}

void OpenGLMiscWidget::keyPressEvent(QKeyEvent *event) {
    // Forward to internal MiscWidget for keyboard shortcuts
    if (_miscWidget) {
        QApplication::sendEvent(_miscWidget, event);
        // Hidden widget's conditional update() doesn't trigger OpenGL repaint
        // Update unconditionally since we can't check the tool's return value
        update();
    }
}

void OpenGLMiscWidget::keyReleaseEvent(QKeyEvent *event) {
    // Forward to internal MiscWidget for keyboard shortcuts
    if (_miscWidget) {
        QApplication::sendEvent(_miscWidget, event);
        // Hidden widget's conditional update() doesn't trigger OpenGL repaint
        // Update unconditionally since we can't check the tool's return value
        update();
    }
}

void OpenGLMiscWidget::resizeEvent(QResizeEvent *event) {
    // Resize internal MiscWidget to match
    if (_miscWidget) {
        _miscWidget->resize(event->size());
    }

    // Let parent handle OpenGL resize
    QOpenGLWidget::resizeEvent(event);
}
