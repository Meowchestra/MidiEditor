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

#ifndef PROTOCOLENTRY_H_
#define PROTOCOLENTRY_H_

// Forward declarations
class MidiFile;

/**
 * \class ProtocolEntry
 *
 * \brief Base class for objects that can be recorded in the undo/redo protocol.
 *
 * ProtocolEntry is the base class for all objects that can participate in the
 * MIDI editor's undo/redo system. It provides the fundamental interface for
 * state management and protocol recording.
 *
 * **Protocol System Workflow:**
 * 1. Before modification: Create a copy of the current state using copy()
 * 2. Perform the modification on the object
 * 3. Record the change: Call protocol() with old and new states
 * 4. For undo/redo: Use reloadState() to restore previous states
 *
 * **Key Responsibilities:**
 * - **State Capture**: copy() must save all essential state information
 * - **State Restoration**: reloadState() must restore the object to a previous state
 * - **Protocol Integration**: protocol() records state changes for undo/redo
 * - **File Association**: file() provides context for the protocol system
 *
 * **Implementation Notes:**
 * - Layout information doesn't need to be saved (automatic relayout occurs)
 * - All subclasses must implement copy() and reloadState()
 * - State changes should be atomic and reversible
 */
class ProtocolEntry {
public:
    /**
     * \brief Virtual destructor for proper cleanup of derived classes.
     */
    virtual ~ProtocolEntry();

    /**
     * \brief Creates a copy of this ProtocolEntry for state preservation.
     * \return A new ProtocolEntry containing the current state
     *
     * This method should save all essential state information needed to
     * restore the object later via reloadState(). Layout information
     * doesn't need to be saved as the program will relayout automatically
     * after protocol operations.
     */
    virtual ProtocolEntry *copy();

    /**
     * \brief Reloads the object's state from a previously saved entry.
     * \param entry The ProtocolEntry containing the state to restore
     *
     * This method must restore the object to the exact state that was
     * saved in the provided entry by a previous call to copy().
     */
    virtual void reloadState(ProtocolEntry *entry);

    /**
     * \brief Records a state change in the protocol system.
     * \param oldObj The ProtocolEntry representing the old state
     * \param newObj The ProtocolEntry representing the new state
     *
     * This method writes both the old and new states to the protocol,
     * enabling undo/redo functionality. Typically called with "this"
     * as the newObj parameter.
     */
    virtual void protocol(ProtocolEntry *oldObj, ProtocolEntry *newObj);

    /**
     * \brief Gets the MIDI file associated with this entry.
     * \return Pointer to the MidiFile containing this entry
     */
    virtual MidiFile *file();
};

#endif // PROTOCOLENTRY_H_
