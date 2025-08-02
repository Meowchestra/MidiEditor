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

#ifndef SELECTION_H_
#define SELECTION_H_

// Project includes
#include "../protocol/ProtocolEntry.h"

// Qt includes
#include <QList>

// Forward declarations
class MidiEvent;
class EventWidget;

/**
 * \class Selection
 *
 * \brief Manages the current selection of MIDI events in the editor.
 *
 * Selection is a singleton class that maintains the current set of selected
 * MIDI events across the entire application. It provides centralized selection
 * management with protocol support for undo/redo operations.
 *
 * Key features:
 * - **Singleton pattern**: Global access to the current selection
 * - **Protocol integration**: Selection changes can be undone/redone
 * - **Event management**: Add, remove, and clear selected events
 * - **Widget integration**: Synchronizes with EventWidget display
 * - **Multi-selection**: Supports selecting multiple events simultaneously
 *
 * The selection is used by various tools and widgets to determine which
 * events should be affected by operations like move, copy, delete, etc.
 * It maintains consistency across all views and provides a unified
 * interface for selection management.
 */
class Selection : public ProtocolEntry {
public:
    /**
     * \brief Creates a new Selection for the given file.
     * \param file The MidiFile this selection belongs to
     */
    Selection(MidiFile *file);

    /**
     * \brief Creates a new Selection copying another instance.
     * \param other The Selection instance to copy
     */
    Selection(Selection &other);

    /**
     * \brief Creates a copy of this selection for the protocol system.
     * \return A new ProtocolEntry representing this selection's state
     */
    virtual ProtocolEntry *copy();

    /**
     * \brief Reloads the selection's state from a protocol entry.
     * \param entry The protocol entry to restore state from
     */
    virtual void reloadState(ProtocolEntry *entry);

    /**
     * \brief Gets the MIDI file associated with this selection.
     * \return Pointer to the MidiFile
     */
    virtual MidiFile *file();

    // === Singleton Management ===

    /**
     * \brief Gets the global selection instance.
     * \return Pointer to the singleton Selection instance
     */
    static Selection *instance();

    /**
     * \brief Sets the MIDI file for the global selection.
     * \param file The MidiFile to associate with the selection
     */
    static void setFile(MidiFile *file);

    // === Selection Management ===

    /**
     * \brief Gets the list of currently selected events.
     * \return Reference to the list of selected MidiEvent pointers
     */
    QList<MidiEvent *> &selectedEvents();

    /**
     * \brief Sets the selection to the given list of events.
     * \param selections List of MidiEvent pointers to select
     */
    void setSelection(QList<MidiEvent *> selections);

    /**
     * \brief Clears all selected events.
     */
    void clearSelection();

    /** \brief Static reference to the event widget for display updates */
    static EventWidget *_eventWidget;

private:
    /** \brief List of currently selected events */
    QList<MidiEvent *> _selectedEvents;

    /** \brief Singleton instance pointer */
    static Selection *_selectionInstance;

    /** \brief Associated MIDI file */
    MidiFile *_file;
};

#endif // SELECTION_H_
