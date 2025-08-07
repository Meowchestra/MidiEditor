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

#include "OpenGLPaintWidget.h"
#include "Appearance.h"
#include <QApplication>
#include <QDebug>
#include <QCursor>

OpenGLPaintWidget::OpenGLPaintWidget(QSettings *settings, QWidget *parent)
    : QOpenGLWidget(parent), _settings(settings), _paintDevice(nullptr) {
    // Initialize mouse tracking and state (same as PaintWidget)
    setMouseTracking(true);
    mouseOver = false;
    mousePressed = false;
    mouseReleased = false;
    repaintOnMouseMove = false;
    repaintOnMousePress = false;
    repaintOnMouseRelease = false;
    inDrag = false;
    mousePinned = false;
    mouseX = 0;
    mouseY = 0;
    mouseLastY = 0;
    mouseLastX = 0;
    enabled = true;

    // Configure OpenGL widget for optimal 2D rendering and high DPI support
    // Use NoPartialUpdate for immediate, responsive rendering needed by interactive tools
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);

    // Ensure proper high DPI handling
    setAttribute(Qt::WA_AcceptTouchEvents, false);
    setAttribute(Qt::WA_AlwaysShowToolTips, true);

    qDebug() << "OpenGLPaintWidget: Created with hardware acceleration support";
}

void OpenGLPaintWidget::initializeGL() {
    qDebug() << "OpenGLPaintWidget: Initializing OpenGL for hardware acceleration";

    // Get OpenGL context and functions
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context) {
        qWarning() << "OpenGLPaintWidget: No OpenGL context available";
        return;
    }

    QOpenGLFunctions *f = context->functions();
    QSurfaceFormat format = context->format();

    qDebug() << "OpenGLPaintWidget: OpenGL Version:" << format.majorVersion() << "." << format.minorVersion();
    qDebug() << "OpenGLPaintWidget: OpenGL Profile:" << format.profile();

    // Set up OpenGL state for optimal 2D rendering performance
    f->glEnable(GL_BLEND);
    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    f->glDisable(GL_DEPTH_TEST);
    f->glDisable(GL_CULL_FACE);

    // Enable multisampling if available for better visual quality
    if (format.samples() > 1) {
        f->glEnable(GL_MULTISAMPLE);
    }

    // Optimize for 2D rendering performance
    f->glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    f->glHint(GL_POINT_SMOOTH_HINT, GL_FASTEST);
    f->glHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);

    // Create OpenGL paint device for QPainter acceleration
    _paintDevice = new QOpenGLPaintDevice();
    if (!_paintDevice) {
        qWarning() << "OpenGLPaintWidget: Failed to create QOpenGLPaintDevice";
        return;
    }

    // Configure paint device for optimal performance
    _paintDevice->setSize(size());

    // Use device pixel ratio of 1.0 to avoid coordinate system issues
    // This ensures consistent rendering across different DPI settings
    _paintDevice->setDevicePixelRatio(1.0);

    // Keep Qt widget coordinate system (not flipped) for compatibility
    _paintDevice->setPaintFlipped(false);
}

void OpenGLPaintWidget::paintGL() {
    if (!_paintDevice) {
        // Fallback: clear to background color
        QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
        if (f) {
            f->glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
            f->glClear(GL_COLOR_BUFFER_BIT);
        }
        return;
    }

    // Update paint device size if needed (minimize allocations)
    QSize currentSize = size();
    if (_paintDevice->size() != currentSize) {
        _paintDevice->setSize(currentSize);
        _paintDevice->setDevicePixelRatio(1.0);
        _paintDevice->setPaintFlipped(false);
    }

    // Create OpenGL-accelerated QPainter
    QPainter painter(_paintDevice);
    if (!painter.isActive()) {
        qWarning() << "OpenGLPaintWidget: Failed to create active OpenGL painter";
        return;
    }

    // Configure painter with hardware-specific settings
    // When hardware acceleration is enabled, use hardware-specific settings instead of software ones
    bool hardwareSmoothTransforms = _settings->value("rendering/hardware_smooth_transforms", true).toBool();

    // Hardware antialiasing is handled by MSAA (multisampling) at the OpenGL level
    // Always enable QPainter antialiasing for hardware rendering - MSAA provides the actual antialiasing
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::VerticalSubpixelPositioning, true);

    // Use hardware smooth transforms setting (user preference for GPU texture filtering)
    painter.setRenderHint(QPainter::SmoothPixmapTransform, hardwareSmoothTransforms);

    // Enable high-quality rendering for OpenGL
    painter.setRenderHint(QPainter::LosslessImageRendering, true);

    // Call the subclass's paint implementation with OpenGL-accelerated painter
    paintContent(&painter);

    painter.end();
}

void OpenGLPaintWidget::resizeGL(int w, int h) {
    // Call base class to ensure proper OpenGL setup
    QOpenGLWidget::resizeGL(w, h);

    // Update paint device size for optimal performance
    if (_paintDevice) {
        _paintDevice->setSize(QSize(w, h));
        _paintDevice->setDevicePixelRatio(1.0);
        _paintDevice->setPaintFlipped(false);
    }

    // Set OpenGL viewport for optimal 2D rendering
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    if (f) {
        f->glViewport(0, 0, w, h);
    }
}

// === Mouse Event Handlers (identical to PaintWidget) ===

void OpenGLPaintWidget::mouseMoveEvent(QMouseEvent *event) {
    mouseOver = true;

    if (mousePinned) {
        // do not change mousePosition but lastMousePosition to get the
        // correct move distance
        QCursor::setPos(mapToGlobal(QPoint(mouseX, mouseY)));
        mouseLastX = 2 * mouseX - qRound(event->position().x());
        mouseLastY = 2 * mouseY - qRound(event->position().y());
    } else {
        mouseLastX = mouseX;
        mouseLastY = mouseY;
        mouseX = qRound(event->position().x());
        mouseY = qRound(event->position().y());
    }
    if (mousePressed) {
        inDrag = true;
    }

    if (!enabled) {
        return;
    }

    if (repaintOnMouseMove) {
        update();
    }
}

void OpenGLPaintWidget::enterEvent(QEnterEvent *event) {
    mouseOver = true;

    if (!enabled) {
        return;
    }

    update();
}

void OpenGLPaintWidget::leaveEvent(QEvent *event) {
    mouseOver = false;

    if (!enabled) {
        return;
    }

    update();
}

void OpenGLPaintWidget::mousePressEvent(QMouseEvent *event) {
    mousePressed = true;
    mouseReleased = false;

    if (!enabled) {
        return;
    }

    if (repaintOnMousePress) {
        update();
    }
}

void OpenGLPaintWidget::mouseReleaseEvent(QMouseEvent *event) {
    inDrag = false;
    mouseReleased = true;
    mousePressed = false;

    if (!enabled) {
        return;
    }

    if (repaintOnMouseRelease) {
        update();
    }
}

// === Geometric Testing Methods (identical to PaintWidget) ===

bool OpenGLPaintWidget::mouseInRect(int x, int y, int width, int height) {
    return mouseBetween(x, y, x + width, y + height);
}

bool OpenGLPaintWidget::mouseInRect(QRectF rect) {
    return mouseInRect(rect.x(), rect.y(), rect.width(), rect.height());
}

bool OpenGLPaintWidget::mouseBetween(int x1, int y1, int x2, int y2) {
    int temp;
    if (x1 > x2) {
        temp = x1;
        x1 = x2;
        x2 = temp;
    }
    if (y1 > y2) {
        temp = y1;
        y1 = y2;
        y2 = temp;
    }
    return mouseOver && mouseX >= x1 && mouseX <= x2 && mouseY >= y1 && mouseY <= y2;
}

int OpenGLPaintWidget::draggedX() {
    if (!inDrag) {
        return 0;
    }
    int i = mouseX - mouseLastX;
    mouseLastX = mouseX;
    return i;
}

int OpenGLPaintWidget::draggedY() {
    if (!inDrag) {
        return 0;
    }
    int i = mouseY - mouseLastY;
    mouseLastY = mouseY;
    return i;
}

void OpenGLPaintWidget::setRepaintOnMouseMove(bool b) {
    repaintOnMouseMove = b;
}

void OpenGLPaintWidget::setRepaintOnMousePress(bool b) {
    repaintOnMousePress = b;
}

void OpenGLPaintWidget::setRepaintOnMouseRelease(bool b) {
    repaintOnMouseRelease = b;
}

OpenGLPaintWidget::~OpenGLPaintWidget() {
    // Ensure proper OpenGL resource cleanup to prevent QRhi resource leaks
    qDebug() << "OpenGLPaintWidget: Starting destructor cleanup";

    // Check if we still have a valid OpenGL context
    QOpenGLContext *context = QOpenGLContext::currentContext();
    bool hadContext = (context != nullptr);

    if (!hadContext) {
        // Try to make our context current for cleanup
        try {
            makeCurrent();
            context = QOpenGLContext::currentContext();
        } catch (...) {
            // makeCurrent() failed, context is likely already destroyed
            context = nullptr;
        }
    }

    if (context) {
        qDebug() << "OpenGLPaintWidget: Cleaning up with valid OpenGL context";

        // Clean up OpenGL resources while context is valid
        if (_paintDevice) {
            // Force the paint device to release all its resources
            _paintDevice->setSize(QSize(1, 1)); // Minimize size to reduce resource usage
            delete _paintDevice;
            _paintDevice = nullptr;
        }

        // Ensure all OpenGL operations are completed and flush all commands
        QOpenGLFunctions *f = context->functions();
        if (f) {
            f->glFlush();  // Flush all commands
            f->glFinish(); // Wait for all OpenGL commands to complete
        }

        // Force Qt to clean up any cached OpenGL resources
        context->doneCurrent();

        // Release the context
        doneCurrent();
    } else {
        qDebug() << "OpenGLPaintWidget: No valid OpenGL context for cleanup (normal during application shutdown)";

        // Clean up what we can without OpenGL context
        if (_paintDevice) {
            delete _paintDevice;
            _paintDevice = nullptr;
        }
    }

    qDebug() << "OpenGLPaintWidget: Destructor cleanup completed";
}
