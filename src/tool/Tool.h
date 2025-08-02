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

#ifndef TOOL_H_
#define TOOL_H_

// Qt includes
#include <QImage>
#include <QString>

// Project includes
#include "../protocol/ProtocolEntry.h"

// Forward declarations
class Protocol;
class MidiFile;
class EditorTool;
class ToolButton;
class StandardTool;

/**
 * \class Tool
 *
 * \brief Tool is the superclass for every Tool in the program.
 *
 * Every Tool can be represented by a ToolButton; for this is has to be set
 * either an image or an imagetext.
 *
 * Tool inherits ProtocolEntry, so every action on a Tool can be written to the
 * history of the program.
 *
 * The method selected() can be used to give the ToolButton another background.
 *
 * A Tool can either be accessed using the Toolbuttons or it may be set by the
 * StandardTool.
 * This Tool decides on every click in the Editor which Tool to take.
 * If a tool has a StandardTool not equal 0, it has to return to this standard
 * tool when its action has been finished.
 */

class Tool : public ProtocolEntry {
public:
    /**
     * \brief Creates a new Tool.
     */
    Tool();

    /**
     * \brief Creates a new Tool copying all data from another instance.
     * \param other The Tool instance to copy
     */
    Tool(Tool &other);

    /**
     * \brief Returns whether the Tool is selected or not.
     * \return True if the tool is currently selected
     */
    virtual bool selected();

    /**
     * \brief Sets the tool's image.
     * \param name The image file name to load
     */
    void setImage(QString name);

    /**
     * \brief Returns the tool's image.
     * \return Pointer to the tool's QImage
     */
    QImage *image();

    /**
     * \brief Sets the tool's tooltip text.
     * \param text The tooltip text to display
     */
    void setToolTipText(QString text);

    /**
     * \brief Returns the tool's tooltip text.
     * \return The tooltip string
     */
    QString toolTip();

    /**
		 * \brief sets the Tools ToolButton.
		 */
    void setButton(ToolButton *b);

    /**
		 * \brief this method is called when the user presses the Tools Button.
		 */
    virtual void buttonClick();

    /**
		 * \brief returns the Tools ToolButton.
		 */
    ToolButton *button();

    /**
		 * \brief sets the static current Tool.
		 *
		 * This method is used by EditorTool
		 */
    static void setCurrentTool(EditorTool *editorTool);

    /**
		 * \brief returns the current Tool.
		 */
    static EditorTool *currentTool();

    /**
		 * \brief sets the static current MidiFile.
		 */
    static void setFile(MidiFile *file);

    /**
		 * \brief returns the currenty opened File.
		 */
    static MidiFile *currentFile();

    /**
		 * \brief returns the Protocol of the currently opened Document.
		 */
    static Protocol *currentProtocol();

    /**
		 * \brief sets the StandardTool. When the StandardTool is set, the Tool
		 * has to set StandardTool as currentTool when its action is finished
		 */
    void setStandardTool(StandardTool *stdTool);

    /**
		 * \brief The following functions are redefinitions from the superclass
		 * ProtocolEntry
		 */
    virtual ProtocolEntry *copy();

    /**
     * \brief Reloads the tool's state from a protocol entry.
     * \param entry The protocol entry to restore state from
     */
    virtual void reloadState(ProtocolEntry *entry);

    /**
     * \brief Gets the current MIDI file.
     * \return Pointer to the current MidiFile
     */
    MidiFile *file();

protected:
    /**
     * \brief The tool's button if existing.
     */
    ToolButton *_button;

    /**
     * \brief The image representing the tool.
     *
     * Used in the protocol list and on the buttons.
     */
    QImage *_image;

    /**
     * \brief The tooltip text the button should display.
     */
    QString _toolTip;

    /**
     * \brief The standard tool reference.
     *
     * If existing, the tool has to set _standardTool as current tool
     * after its action has been finished.
     */
    StandardTool *_standardTool;

    /**
     * \brief The currently opened file.
     */
    static MidiFile *_currentFile;

    /**
     * \brief The active EditorTool.
     *
     * Is not always the selected tool (if the selected tool is the
     * StandardTool).
     */
    static EditorTool *_currentTool;
};

#endif // TOOL_H_
