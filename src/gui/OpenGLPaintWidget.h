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

#ifndef OPENGLPAINTWIDGET_H_
#define OPENGLPAINTWIDGET_H_

// Qt includes
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLPaintDevice>
#include <QPainter>
#include <QSettings>

/**
 * \class OpenGLPaintWidget
 *
 * \brief OpenGL-accelerated version of PaintWidget for hardware-accelerated rendering.
 *
 * OpenGLPaintWidget provides the same interface as PaintWidget but with OpenGL
 * hardware acceleration. It's designed as a drop-in replacement that provides:
 *
 * - **Hardware Acceleration**: Uses QOpenGLWidget for GPU-accelerated rendering
 * - **Compatible Interface**: Same API as PaintWidget for easy migration
 * - **Enhanced Performance**: GPU-accelerated QPainter operations
 * - **Automatic Fallback**: Graceful fallback to software rendering if needed
 *
 * Key features:
 * - Identical mouse event handling to PaintWidget
 * - OpenGL-accelerated QPainter through QOpenGLPaintDevice
 * - Configurable render hints for optimal quality/performance balance
 * - Seamless integration with existing widget code
 */
class OpenGLPaintWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new OpenGL-accelerated PaintWidget.
     * \param settings Application settings for configuration
     * \param parent The parent widget
     */
    OpenGLPaintWidget(QSettings *settings, QWidget *parent = nullptr);

    /**
     * \brief Destructor.
     */
    virtual ~OpenGLPaintWidget();

    /**
     * \brief Sets whether to repaint on mouse move events.
     * \param b True to enable repainting on mouse moves
     */
    void setRepaintOnMouseMove(bool b);

    /**
     * \brief Sets whether to repaint on mouse press events.
     * \param b True to enable repainting on mouse press
     */
    void setRepaintOnMousePress(bool b);

    /**
     * \brief Sets whether to repaint on mouse release events.
     * \param b True to enable repainting on mouse release
     */
    void setRepaintOnMouseRelease(bool b);

    // === Mouse Position and State ===

    /**
     * \brief Gets the current mouse X coordinate.
     * \return Mouse X position in widget coordinates
     */
    int getMouseX() const { return mouseX; }

    /**
     * \brief Gets the current mouse Y coordinate.
     * \return Mouse Y position in widget coordinates
     */
    int getMouseY() const { return mouseY; }

    /**
     * \brief Gets the previous mouse X coordinate.
     * \return Previous mouse X position
     */
    int getMouseLastX() const { return mouseLastX; }

    /**
     * \brief Gets the previous mouse Y coordinate.
     * \return Previous mouse Y position
     */
    int getMouseLastY() const { return mouseLastY; }

    /**
     * \brief Checks if mouse is currently over the widget.
     * \return True if mouse is over the widget
     */
    bool isMouseOver() const { return mouseOver; }

    /**
     * \brief Checks if mouse button is currently pressed.
     * \return True if mouse is pressed
     */
    bool isMousePressed() const { return mousePressed; }

    /**
     * \brief Checks if mouse was recently released.
     * \return True if mouse was released
     */
    bool isMouseReleased() const { return mouseReleased; }

    /**
     * \brief Checks if a drag operation is in progress.
     * \return True if dragging
     */
    bool isInDrag() const { return inDrag; }

    /**
     * \brief Gets the distance dragged in X direction since last call.
     * \return X drag distance in pixels
     */
    int draggedX();

    /**
     * \brief Gets the distance dragged in Y direction since last call.
     * \return Y drag distance in pixels
     */
    int draggedY();

    // === Geometric Testing ===

    /**
     * \brief Tests if mouse is within a rectangular area.
     * \param x Rectangle X coordinate
     * \param y Rectangle Y coordinate
     * \param width Rectangle width
     * \param height Rectangle height
     * \return True if mouse is within the rectangle
     */
    bool mouseInRect(int x, int y, int width, int height);

    /**
     * \brief Tests if mouse is within a rectangular area.
     * \param rect The rectangle to test
     * \return True if mouse is within the rectangle
     */
    bool mouseInRect(QRectF rect);

    /**
     * \brief Tests if mouse is between two points.
     * \param x1 First point X coordinate
     * \param y1 First point Y coordinate
     * \param x2 Second point X coordinate
     * \param y2 Second point Y coordinate
     * \return True if mouse is between the points
     */
    bool mouseBetween(int x1, int y1, int x2, int y2);

    /**
     * \brief Sets mouse pinning state for constrained operations.
     * \param b True to pin the mouse, false to unpin
     */
    void setMousePinned(bool b) { mousePinned = b; }

    /**
     * \brief Sets the widget enabled state.
     * \param enabled True to enable the widget
     */
    void setEnabled(bool enabled) { this->enabled = enabled; }

    /**
     * \brief Gets the widget enabled state.
     * \return True if widget is enabled
     */
    bool isEnabled() const { return enabled; }

protected:
    // === OpenGL Methods ===

    /**
     * \brief Initializes OpenGL context and resources.
     */
    void initializeGL() override;

    /**
     * \brief Handles OpenGL rendering.
     */
    void paintGL() override;

    /**
     * \brief Handles OpenGL viewport resizing.
     * \param w New width
     * \param h New height
     */
    void resizeGL(int w, int h) override;

    // === Mouse Event Handlers ===

    /**
     * \brief Handles mouse move events with OpenGL acceleration.
     * \param event The mouse move event
     */
    void mouseMoveEvent(QMouseEvent *event) override;

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
     * \brief Handles mouse press events.
     * \param event The mouse press event
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * \brief Handles mouse release events.
     * \param event The mouse release event
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    // === Virtual Methods for Subclasses ===

    /**
     * \brief Virtual method for subclasses to implement custom OpenGL painting.
     * \param painter OpenGL-accelerated QPainter
     * 
     * Subclasses should override this method to implement their custom rendering
     * using the provided OpenGL-accelerated QPainter.
     */
    virtual void paintContent(QPainter *painter) = 0;

    // === State Variables ===

    /** \brief Application settings */
    QSettings *_settings;

    /** \brief OpenGL paint device for hardware acceleration */
    QOpenGLPaintDevice *_paintDevice;

    /** \brief Mouse and widget state flags */
    bool mouseOver, mousePressed, mouseReleased, repaintOnMouseMove,
            repaintOnMousePress, repaintOnMouseRelease, inDrag, mousePinned,
            enabled;

    /** \brief Mouse position tracking */
    int mouseX, mouseY, mouseLastX, mouseLastY;
};

#endif // OPENGLPAINTWIDGET_H_
