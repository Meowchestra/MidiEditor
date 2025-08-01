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

#ifndef NOTEONEVENT_H_
#define NOTEONEVENT_H_

#include "OnEvent.h"

// Forward declarations
class OffEvent;

/**
 * \class NoteOnEvent
 *
 * \brief MIDI Note On event representing the start of a musical note.
 *
 * NoteOnEvent represents a MIDI Note On message, which starts a musical note
 * with a specific pitch and velocity. This is the most common type of MIDI
 * event used in musical compositions.
 *
 * Key properties:
 * - **Note**: MIDI note number (0-127, where 60 = Middle C)
 * - **Velocity**: How hard the note was pressed (0-127, affects volume/timbre)
 * - **Duration**: Determined by the corresponding OffEvent
 * - **Channel**: MIDI channel for the note (0-15)
 *
 * The event automatically pairs with a corresponding OffEvent to define
 * the note's duration and provides visual representation in the piano roll editor.
 */
class NoteOnEvent : public OnEvent {
public:
    /**
     * \brief Creates a new NoteOnEvent.
     * \param note MIDI note number (0-127)
     * \param velocity Note velocity (0-127)
     * \param ch MIDI channel (0-15)
     * \param track The MIDI track this event belongs to
     */
    NoteOnEvent(int note, int velocity, int ch, MidiTrack *track);

    /**
     * \brief Creates a new NoteOnEvent copying another instance.
     * \param other The NoteOnEvent instance to copy
     */
    NoteOnEvent(NoteOnEvent &other);

    /**
     * \brief Gets the MIDI note number.
     * \return The note number (0-127, where 60 = Middle C)
     */
    int note();

    /**
     * \brief Gets the note velocity.
     * \return The velocity value (0-127)
     */
    int velocity();

    /**
     * \brief Gets the display line for this note.
     * \return The line number where this note should be displayed
     */
    int line();

    /**
     * \brief Sets the MIDI note number.
     * \param n The new note number (0-127)
     */
    void setNote(int n);

    /**
     * \brief Sets the note velocity.
     * \param v The new velocity value (0-127)
     */
    void setVelocity(int v);

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
     * \brief Gets a string representation of this event.
     * \return String message describing the note on event
     */
    QString toMessage();

    /**
     * \brief Gets a string representation of the corresponding off event.
     * \return String message describing the note off event
     */
    QString offEventMessage();

    /**
     * \brief Saves the event data to a byte array.
     * \return QByteArray containing the serialized event data
     */
    QByteArray save();

    /**
     * \brief Saves the corresponding off event data to a byte array.
     * \return QByteArray containing the serialized off event data
     */
    QByteArray saveOffEvent();

    /**
     * \brief Gets the type string for this event.
     * \return String identifying this as a "Note On" event
     */
    QString typeString();

protected:
    /** \brief MIDI note number and velocity */
    int _note, _velocity;
};

#endif // NOTEONEVENT_H_
