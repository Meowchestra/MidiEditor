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

#ifndef COLOREDWIDGET_H_
#define COLOREDWIDGET_H_

// Qt includes
#include <QWidget>
#include <QColor>

/**
 * \class ColoredWidget
 *
 * \brief Simple widget that displays a solid color background.
 *
 * ColoredWidget is a basic utility widget that fills its entire area
 * with a single solid color. It's commonly used for:
 *
 * - **Color indicators**: Visual representation of channel/track colors
 * - **Status displays**: Color-coded status information
 * - **UI accents**: Colored separators or highlights
 * - **Color swatches**: Showing selected or available colors
 *
 * The widget automatically repaints when the color is changed and
 * provides a simple interface for dynamic color updates.
 */
class ColoredWidget : public QWidget {
public:
    /**
     * \brief Creates a new ColoredWidget with the specified color.
     * \param color The color to fill the widget with
     * \param parent The parent widget
     */
    ColoredWidget(QColor color, QWidget *parent = 0);

    /**
     * \brief Sets the widget's color and triggers a repaint.
     * \param c The new color to display
     */
    void setColor(QColor c) {
        _color = c;
        update();
    }

protected:
    /**
     * \brief Handles paint events to draw the colored background.
     * \param event The paint event
     */
    void paintEvent(QPaintEvent *event);

private:
    /** \brief The color to fill the widget with */
    QColor _color;
};

#endif // COLOREDWIDGET_H_
