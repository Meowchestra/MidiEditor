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

#ifndef PAINTWIDGET_H_
#define PAINTWIDGET_H_

// Qt includes
#include <QMouseEvent>
#include <QWidget>

/**
 * \class PaintWidget
 *
 * \brief Base widget class with enhanced mouse event handling and painting support.
 *
 * PaintWidget extends QWidget to provide specialized mouse event handling
 * and painting capabilities commonly needed in the MIDI editor. It offers:
 *
 * - **Enhanced mouse tracking**: Detailed mouse position and movement tracking
 * - **Configurable repainting**: Control when repaints occur based on mouse events
 * - **Drag detection**: Built-in support for drag operations
 * - **Mouse state management**: Comprehensive mouse state tracking
 * - **Geometric utilities**: Helper methods for mouse position testing
 *
 * Key features:
 * - Tracks current and previous mouse positions
 * - Provides movement and drag distance calculations
 * - Supports mouse pinning for constrained operations
 * - Configurable repaint triggers for performance optimization
 * - Rectangle and line intersection testing for mouse interactions
 *
 * This class serves as the foundation for interactive widgets like MatrixWidget
 * that require precise mouse handling and custom painting.
 */
class PaintWidget : public QWidget {
public:
    /**
     * \brief Creates a new PaintWidget.
     * \param parent The parent widget
     */
    PaintWidget(QWidget *parent = 0);

    /**
     * \brief Sets whether to repaint on mouse move events.
     * \param b True to enable repainting on mouse moves
     */
    void setRepaintOnMouseMove(bool b);

    /**
     * \brief Sets whether to repaint on mouse press events.
     * \param b True to enable repainting on mouse presses
     */
    void setRepaintOnMousePress(bool b);

    /**
     * \brief Sets whether to repaint on mouse release events.
     * \param b True to enable repainting on mouse releases
     */
    void setRepaintOnMouseRelease(bool b);

    /**
     * \brief Sets the enabled state of the widget.
     * \param b True to enable the widget, false to disable
     */
    void setEnabled(bool b);

protected:
    // === Event Handlers ===

    /**
     * \brief Handles mouse move events.
     * \param event The mouse move event
     */
    void mouseMoveEvent(QMouseEvent *event);

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
     * \brief Handles mouse press events.
     * \param event The mouse press event
     */
    void mousePressEvent(QMouseEvent *event);

    /**
     * \brief Handles mouse release events.
     * \param event The mouse release event
     */
    void mouseReleaseEvent(QMouseEvent *event);

    // === Mouse Position Utilities ===

    /**
     * \brief Gets the X distance moved since last mouse event.
     * \return Horizontal movement in pixels
     */
    int movedX() { return mouseX - mouseLastX; }

    /**
     * \brief Gets the Y distance moved since last mouse event.
     * \return Vertical movement in pixels
     */
    int movedY() { return mouseY - mouseLastY; }

    /**
     * \brief Gets the total X distance dragged since drag started.
     * \return Total horizontal drag distance in pixels
     */
    int draggedX();

    /**
     * \brief Gets the total Y distance dragged since drag started.
     * \return Total vertical drag distance in pixels
     */
    int draggedY();

    // === Geometric Testing ===

    /**
     * \brief Tests if mouse is within a rectangle.
     * \param x Rectangle X coordinate
     * \param y Rectangle Y coordinate
     * \param width Rectangle width
     * \param height Rectangle height
     * \return True if mouse is within the rectangle
     */
    bool mouseInRect(int x, int y, int width, int height);

    /**
     * \brief Tests if mouse is within a rectangle.
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

    // === State Variables ===

    /** \brief Mouse and widget state flags */
    bool mouseOver, mousePressed, mouseReleased, repaintOnMouseMove,
            repaintOnMousePress, repaintOnMouseRelease, inDrag, mousePinned,
            enabled;

    /** \brief Mouse position tracking */
    int mouseX, mouseY, mouseLastX, mouseLastY;
};
#endif // PAINTWIDGET_H_
