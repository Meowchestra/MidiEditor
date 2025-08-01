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

#ifndef PROTOCOLWIDGET_H_
#define PROTOCOLWIDGET_H_

#include <QListWidget>
#include <QPaintEvent>

// Forward declarations
class MidiFile;
class ProtocolStep;

/**
 * \class ProtocolWidget
 *
 * \brief Widget for displaying and managing the undo/redo history.
 *
 * The ProtocolWidget provides a visual representation of the command history
 * (protocol) for the MIDI editor. It displays a list of all actions that have
 * been performed, allowing users to:
 *
 * - View the complete history of operations
 * - Navigate to any previous state by clicking on a history entry
 * - Understand what changes have been made to the file
 *
 * The widget automatically updates when new actions are performed and provides
 * visual feedback for the current position in the history.
 */
class ProtocolWidget : public QListWidget {
    Q_OBJECT

public:
    /**
     * \brief Creates a new ProtocolWidget.
     * \param parent The parent widget (default: nullptr)
     */
    ProtocolWidget(QWidget *parent = 0);

    /**
     * \brief Sets the MIDI file whose protocol should be displayed.
     * \param f The MidiFile to monitor for protocol changes
     */
    void setFile(MidiFile *f);

public slots:
    /**
     * \brief Called when the protocol (history) has changed.
     * Updates the widget to reflect the new history state.
     */
    void protocolChanged();

    /**
     * \brief Updates the widget display.
     * Refreshes the list of protocol steps.
     */
    void update();

    /**
     * \brief Handles clicks on protocol steps.
     * \param item The list item that was clicked
     */
    void stepClicked(QListWidgetItem *item);

    /**
     * \brief Refreshes colors for theme changes.
     * Updates the widget's appearance when the theme changes.
     */
    void refreshColors();

private:
    /** \brief The MIDI file being monitored */
    MidiFile *file;

    /** \brief Flag indicating if the protocol has changed */
    bool protocolHasChanged;

    /** \brief Flag indicating if the next change comes from the list */
    bool nextChangeFromList;
};

#endif // PROTOCOLWIDGET_H_
