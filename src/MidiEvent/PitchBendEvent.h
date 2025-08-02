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

#ifndef PITCHBENDEVENT_H_
#define PITCHBENDEVENT_H_

#include "MidiEvent.h"

/**
 * \class PitchBendEvent
 *
 * \brief MIDI Pitch Bend event for pitch wheel control.
 *
 * PitchBendEvent represents MIDI Pitch Bend messages, which are used to
 * smoothly bend the pitch of notes up or down. This is commonly controlled
 * by a pitch wheel or joystick on MIDI keyboards.
 *
 * Key features:
 * - **14-bit resolution**: Higher precision than most MIDI messages (0-16383)
 * - **Center value**: 8192 represents no bend (neutral position)
 * - **Bend range**: Values below 8192 bend down, above 8192 bend up
 * - **Channel-wide**: Affects all notes currently playing on the channel
 * - **Real-time**: Provides smooth, continuous pitch changes
 *
 * The actual pitch bend range (in semitones) is determined by the receiving
 * instrument's pitch bend sensitivity setting.
 */
class PitchBendEvent : public MidiEvent {
public:
    /**
     * \brief Creates a new PitchBendEvent.
     * \param channel MIDI channel (0-15)
     * \param val Pitch bend value (0-16383, where 8192 = no bend)
     * \param track The MIDI track this event belongs to
     */
    PitchBendEvent(int channel, int val, MidiTrack *track);

    /**
     * \brief Creates a new PitchBendEvent copying another instance.
     * \param other The PitchBendEvent instance to copy
     */
    PitchBendEvent(PitchBendEvent &other);

    /**
     * \brief Gets the display line for this event.
     * \return The line number where this event should be displayed
     */
    virtual int line();

    /**
     * \brief Gets the pitch bend value.
     * \return The pitch bend value (0-16383, where 8192 = no bend)
     */
    int value();

    /**
     * \brief Sets the pitch bend value.
     * \param v The new pitch bend value (0-16383)
     */
    void setValue(int v);

    /**
     * \brief Gets a string representation of this event.
     * \return String message describing the pitch bend event
     */
    QString toMessage();

    /**
     * \brief Saves the event data to a byte array.
     * \return QByteArray containing the serialized event data
     */
    QByteArray save();

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
     * \brief Gets the type string for this event.
     * \return String identifying this as a "Pitch Bend" event
     */
    QString typeString();

    /**
     * \brief Returns whether this is an OnEvent.
     * \return False (this is not an OnEvent)
     */
    virtual bool isOnEvent();

private:
    /** \brief The pitch bend value (0-16383) */
    int _value;
};

#endif // PITCHBENDEVENT_H_
