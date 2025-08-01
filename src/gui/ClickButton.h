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

#ifndef CLICKBUTTON_H_
#define CLICKBUTTON_H_

// Qt includes
#include <QPushButton>
#include <QEnterEvent>

/**
 * \class ClickButton
 *
 * \brief Custom push button with image support and hover effects.
 *
 * ClickButton extends QPushButton to provide enhanced visual feedback
 * and image-based button display. It offers:
 *
 * - **Image display**: Shows custom images instead of text
 * - **Hover effects**: Visual feedback when mouse enters/leaves
 * - **Click feedback**: Visual indication of button presses
 * - **Dynamic images**: Ability to change button images at runtime
 * - **Custom painting**: Specialized rendering for better appearance
 *
 * The button is commonly used in toolbars and control panels where
 * iconic buttons provide better user experience than text buttons.
 * It automatically handles mouse state changes and provides appropriate
 * visual feedback for user interactions.
 */
class ClickButton : public QPushButton {
    Q_OBJECT

public:
    /**
     * \brief Creates a new ClickButton with the specified image.
     * \param imageName Name of the image file to display on the button
     * \param parent The parent widget
     */
    ClickButton(QString imageName, QWidget *parent = 0);

    /**
     * \brief Sets the button's image.
     * \param imageName Name of the new image file to display
     */
    void setImageName(QString imageName);

public slots:
    /**
     * \brief Handles button click events.
     */
    void buttonClick();

protected:
    /**
     * \brief Handles paint events to draw the button with custom appearance.
     * \param event The paint event
     */
    void paintEvent(QPaintEvent *event);

    /**
     * \brief Handles mouse enter events for hover effects.
     * \param event The enter event
     */
    void enterEvent(QEnterEvent *event);

    /**
     * \brief Handles mouse leave events for hover effects.
     * \param event The leave event
     */
    void leaveEvent(QEvent *event);

private:
    /** \brief Mouse state tracking flags */
    bool button_mouseInButton, button_mouseClicked;

    /** \brief The image displayed on the button */
    QImage *image;
};

#endif // CLICKBUTTON_H_
