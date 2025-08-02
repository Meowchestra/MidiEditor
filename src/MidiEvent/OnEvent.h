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

#ifndef ONEVENT_H_
#define ONEVENT_H_

#include "MidiEvent.h"

// Forward declarations
class OffEvent;

/**
 * \class OnEvent
 *
 * \brief Base class for MIDI events that have a corresponding "off" event.
 *
 * OnEvent represents MIDI events that have a duration and require a corresponding
 * "off" event to mark their end. This is primarily used for note events where
 * a Note On event is paired with a Note Off event.
 *
 * The class maintains a bidirectional relationship with its corresponding OffEvent,
 * allowing for proper event pairing and duration calculations.
 */
class OnEvent : public MidiEvent {
public:
    /**
     * \brief Creates a new OnEvent.
     * \param ch The MIDI channel (0-15)
     * \param track The MIDI track this event belongs to
     */
    OnEvent(int ch, MidiTrack *track);

    /**
     * \brief Creates a new OnEvent copying another instance.
     * \param other The OnEvent instance to copy
     */
    OnEvent(OnEvent &other);

    /**
     * \brief Sets the corresponding off event for this on event.
     * \param event The OffEvent that ends this OnEvent
     */
    void setOffEvent(OffEvent *event);

    /**
     * \brief Gets the corresponding off event for this on event.
     * \return The OffEvent that ends this OnEvent, or nullptr if not set
     */
    OffEvent *offEvent();

    /**
     * \brief Saves the off event data to a byte array.
     * \return QByteArray containing the serialized off event data
     */
    virtual QByteArray saveOffEvent();

    /**
     * \brief Gets a string representation of the off event.
     * \return String message describing the off event
     */
    virtual QString offEventMessage();

    /**
     * \brief Creates a copy of this event for the protocol system.
     * \return A new ProtocolEntry representing this event's state
     */
    virtual ProtocolEntry *copy();

    /**
     * \brief Reloads the event's state from a protocol entry.
     * \param entry The protocol entry to restore state from
     */
    virtual void reloadState(ProtocolEntry *entry);

    /**
     * \brief Moves this event and its off event to a different channel.
     * \param channel The target MIDI channel (0-15)
     */
    void moveToChannel(int channel);

protected:
    /** \brief Pointer to the corresponding off event */
    OffEvent *_offEvent;
};

#endif // ONEVENT_H_
