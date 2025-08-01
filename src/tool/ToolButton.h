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

#ifndef TOOLBUTTON_H_
#define TOOLBUTTON_H_

// Qt includes
#include <QAction>
#include <QKeySequence>

// Forward declarations
class Tool;

/**
 * \class ToolButton
 *
 * \brief UI button widget for tool selection and activation.
 *
 * ToolButton provides the user interface element for selecting and activating
 * tools in the MIDI editor. It extends QAction to provide:
 *
 * - **Tool association**: Links UI buttons to Tool instances
 * - **Keyboard shortcuts**: Supports hotkey activation of tools
 * - **Visual feedback**: Shows tool icons and selection state
 * - **Click handling**: Manages tool activation and deactivation
 * - **Icon management**: Displays appropriate tool icons
 *
 * Each tool can have an associated ToolButton that appears in the toolbar,
 * allowing users to switch between different editing tools quickly.
 */
class ToolButton : public QAction {
    Q_OBJECT

public:
    /**
     * \brief Creates a new ToolButton.
     * \param tool The Tool instance this button controls
     * \param sequence Optional keyboard shortcut for the tool
     * \param parent The parent widget
     */
    ToolButton(Tool *tool, QKeySequence sequence = QKeySequence(), QWidget *parent = 0);

public slots:
    /**
     * \brief Handles button click events to activate the tool.
     */
    void buttonClick();

    /**
     * \brief Handles button release events.
     */
    void releaseButton();

    /**
     * \brief Refreshes the button's icon display.
     */
    void refreshIcon();

private:
    /** \brief The Tool instance controlled by this button */
    Tool *button_tool;
};
#endif // TOOLBUTTON_H_
