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

#ifndef PROTOCOLITEM_H_
#define PROTOCOLITEM_H_

// Forward declarations
class ProtocolEntry;

/**
 * \class ProtocolItem
 *
 * \brief Single undo/redo action storing old and new object states.
 *
 * ProtocolItem represents a single reversible action in the undo/redo system.
 * It stores both the old and new states of a ProtocolEntry, enabling
 * bidirectional state transitions:
 *
 * - **State preservation**: Stores both before and after states
 * - **Reversible actions**: Can undo and redo operations
 * - **Efficient storage**: Only stores the changed object states
 * - **Protocol integration**: Works with the Protocol system
 *
 * The item can be "released" to restore the old state and create
 * a reverse ProtocolItem for redo functionality.
 */
class ProtocolItem {
public:
    /**
     * \brief Creates a new ProtocolItem.
     * \param oldObj The ProtocolEntry representing the old state
     * \param newObj The ProtocolEntry representing the new state
     */
    ProtocolItem(ProtocolEntry *oldObj, ProtocolEntry *newObj);

    /**
     * \brief Releases the item by restoring the old state.
     * \return A new ProtocolItem with reversed state order for redo
     *
     * This method calls newObj.reloadState(oldObj) to restore the old state
     * and returns a new ProtocolItem with the states reversed, enabling
     * redo functionality.
     */
    ProtocolItem *release();

private:
    /** \brief The old and new states of the object */
    ProtocolEntry *_oldObject, *_newObject;
};

#endif // PROTOCOLITEM_H_
