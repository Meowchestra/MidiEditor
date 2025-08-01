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

#ifndef PROTOCOLSTEP_H_
#define PROTOCOLSTEP_H_

// Qt includes
#include <QStack>

// Forward declarations
class ProtocolItem;
class QImage;

/**
 * \class ProtocolStep
 *
 * \brief A complete undo/redo step containing multiple related actions.
 *
 * ProtocolStep represents a logical group of related actions that should be
 * undone or redone together as a single unit. It contains a stack of
 * ProtocolItems that represent individual state changes:
 *
 * - **Action grouping**: Multiple related changes in one undo/redo step
 * - **Stack management**: LIFO ordering for proper undo sequence
 * - **Bidirectional**: Can be released to create the reverse step
 * - **Visual identity**: Description and icon for UI display
 * - **Atomic operations**: All items in a step are processed together
 *
 * For example, moving multiple selected notes would create one ProtocolStep
 * containing multiple ProtocolItems (one for each moved note). When undone,
 * all notes return to their original positions in a single operation.
 */
class ProtocolStep {
public:
    /**
     * \brief Creates a new ProtocolStep with the given description.
     * \param description Human-readable description of this step
     * \param img Optional icon image for UI display
     */
    ProtocolStep(QString description, QImage *img = 0);

    /**
     * \brief Destroys the ProtocolStep and cleans up resources.
     */
    ~ProtocolStep();

    /**
     * \brief Adds a ProtocolItem to the step's stack.
     * \param item The ProtocolItem to add
     *
     * Every item added will be released when releaseStep() is called.
     */
    void addItem(ProtocolItem *item);

    /**
     * \brief Gets the number of items in the stack.
     * \return The number of ProtocolItems in this step
     */
    int items();

    /**
     * \brief Gets the step's description.
     * \return Human-readable description of this step
     */
    QString description();

    /**
     * \brief Gets the step's icon image.
     * \return Pointer to the QImage icon, or nullptr if none
     */
    QImage *image();

    /**
     * \brief Releases the ProtocolStep by executing all items.
     * \return A new ProtocolStep representing the reverse operation
     *
     * Every item in the stack will be released (undone) and the reverse
     * actions will be written to the returned ProtocolStep in reverse order.
     * This enables the undo operation to be placed on the redo stack.
     */
    ProtocolStep *releaseStep();

private:
    /** \brief Human-readable description of this step */
    QString _stepDescription;

    /** \brief Optional icon image for UI display */
    QImage *_image;

    /** \brief Stack of ProtocolItems representing individual actions */
    QStack<ProtocolItem *> *_itemStack;
};

#endif // PROTOCOLSTEP_H_
