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

#ifndef OFFEVENT_H_
#define OFFEVENT_H_

#include "MidiEvent.h"
#include <QList>

// Forward declarations
class OnEvent;

/**
 * \class OffEvent
 *
 * \brief MIDI event that marks the end of a note or other duration-based event.
 *
 * OffEvent represents the "off" portion of MIDI events that have duration,
 * primarily Note Off events. It maintains a bidirectional relationship with
 * its corresponding OnEvent to properly represent note durations.
 *
 * Key features:
 * - Automatic pairing with OnEvent instances
 * - Static management of unpaired OnEvents
 * - Line-based organization for display
 * - Integration with the MIDI file structure
 *
 * The class uses a static system to track OnEvents that haven't been paired
 * with their corresponding OffEvents, helping to identify and resolve
 * corrupted or incomplete event pairs.
 */
class OffEvent : public MidiEvent {
public:
    /**
     * \brief Creates a new OffEvent.
     * \param ch The MIDI channel (0-15)
     * \param line The display line for this event
     * \param track The MIDI track this event belongs to
     */
    OffEvent(int ch, int line, MidiTrack *track);

    /**
     * \brief Creates a new OffEvent copying another instance.
     * \param other The OffEvent instance to copy
     */
    OffEvent(OffEvent &other);

    /**
     * \brief Sets the corresponding on event for this off event.
     * \param event The OnEvent that starts this duration
     */
    void setOnEvent(OnEvent *event);

    /**
     * \brief Gets the corresponding on event for this off event.
     * \return The OnEvent that starts this duration, or nullptr if not set
     */
    OnEvent *onEvent();

    // === Static OnEvent Management ===

    /**
     * \brief Registers an OnEvent as waiting for its OffEvent.
     * \param event The OnEvent to register
     */
    static void enterOnEvent(OnEvent *event);

    /**
     * \brief Clears all registered OnEvents.
     */
    static void clearOnEvents();

    /**
     * \brief Removes an OnEvent from the waiting list.
     * \param event The OnEvent to remove
     */
    static void removeOnEvent(OnEvent *event);

    /**
     * \brief Gets a list of OnEvents that don't have corresponding OffEvents.
     * \return List of corrupted/unpaired OnEvents
     */
    static QList<OnEvent *> corruptedOnEvents();

    // === MidiEvent Interface ===

    /**
     * \brief Draws the event on the given painter.
     * \param p The QPainter to draw with
     * \param c The color to use for drawing
     */
    void draw(QPainter *p, QColor c);

    /**
     * \brief Gets the display line for this event.
     * \return The line number where this event should be displayed
     */
    int line();

    /**
     * \brief Saves the event data to a byte array.
     * \return QByteArray containing the serialized event data
     */
    QByteArray save();

    /**
     * \brief Gets a string representation of this event.
     * \return String message describing the event
     */
    QString toMessage();

    /**
     * \brief Creates a copy of this event for the protocol system.
     * \return A new ProtocolEntry representing this event's state
     */
    ProtocolEntry *copy();

    /**
     * \brief Reloads the event's state from a protocol entry.
     * \param entry The protocol entry to restore state from
     */
    void reloadState(ProtocolEntry *entry);

    /**
     * \brief Sets the MIDI time for this event.
     * \param t The MIDI time in ticks
     * \param toProtocol Whether to record this change in the protocol
     */
    void setMidiTime(int t, bool toProtocol = true);

    /**
     * \brief Returns whether this is an OnEvent.
     * \return False (this is an OffEvent)
     */
    virtual bool isOnEvent();

protected:
    /** \brief Pointer to the corresponding on event */
    OnEvent *_onEvent;

    /**
     * \brief Static map of OnEvents waiting for their OffEvents.
     *
     * Saves all opened and not closed OnEvents. When an OffEvent is created,
     * it searches for its OnEvent in this map and removes it from the map.
     */
    static QMultiMap<int, OnEvent *> *onEvents;

    /**
     * \brief The display line for this event.
     *
     * Needs to save the line because OffEvents are bound to their OnEvents.
     * Setting the line is necessary to find the OnEvent in the QMap.
     */
    int _line;
};

#endif // OFFEVENT_H_
