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

#ifndef GRAPHICOBJECT_H_
#define GRAPHICOBJECT_H_

// Qt includes
#include <QPainter>

/**
 * \class GraphicObject
 *
 * \brief Base class for objects that can be drawn in the MIDI editor.
 *
 * GraphicObject provides the fundamental interface for all objects that
 * have a visual representation in the MIDI editor. It defines the basic
 * properties and methods needed for graphical display:
 *
 * - **Position**: X and Y coordinates for placement
 * - **Size**: Width and height dimensions
 * - **Visibility**: Show/hide state management
 * - **Drawing**: Virtual draw method for custom rendering
 *
 * This class serves as the base for MIDI events and other visual elements
 * that need to be rendered in the matrix widget and other graphical views.
 * It provides a consistent interface for positioning, sizing, and drawing
 * operations across all graphical components.
 */
class GraphicObject {
public:
    /**
     * \brief Creates a new GraphicObject.
     */
    GraphicObject();

    /**
     * \brief Gets the X coordinate.
     * \return The X position in pixels
     */
    int x();

    /**
     * \brief Gets the Y coordinate.
     * \return The Y position in pixels
     */
    int y();

    /**
     * \brief Gets the width.
     * \return The width in pixels
     */
    int width();

    /**
     * \brief Gets the height.
     * \return The height in pixels
     */
    int height();

    /**
     * \brief Sets the X coordinate.
     * \param x The new X position in pixels
     */
    void setX(int x);

    /**
     * \brief Sets the Y coordinate.
     * \param y The new Y position in pixels
     */
    void setY(int y);

    /**
     * \brief Sets the width.
     * \param w The new width in pixels
     */
    void setWidth(int w);

    /**
     * \brief Sets the height.
     * \param h The new height in pixels
     */
    void setHeight(int h);

    /**
     * \brief Draws the object using the given painter and color.
     * \param p The QPainter to draw with
     * \param c The color to use for drawing
     */
    virtual void draw(QPainter *p, QColor c);

    /**
     * \brief Gets the visibility state.
     * \return True if the object is shown, false if hidden
     */
    bool shown();

    /**
     * \brief Sets the visibility state.
     * \param b True to show the object, false to hide it
     */
    void setShown(bool b);

private:
    /** \brief Position and size coordinates */
    int _x, _y, _width, _height;

    /** \brief Visibility flag */
    bool shownInWidget;
};

#endif // GRAPHICOBJECT_H_
