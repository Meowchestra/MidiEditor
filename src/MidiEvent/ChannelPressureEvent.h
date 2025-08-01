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

#ifndef CHANNELPRESSUREEVENT_H_
#define CHANNELPRESSUREEVENT_H_

#include "MidiEvent.h"

/**
 * \class ChannelPressureEvent
 *
 * \brief MIDI Channel Pressure event for aftertouch control.
 *
 * ChannelPressureEvent represents MIDI Channel Pressure messages (also known
 * as Channel Aftertouch), which transmit pressure information applied to the
 * entire channel after the initial key press.
 *
 * Unlike Key Pressure (Polyphonic Aftertouch), Channel Pressure affects all
 * notes currently playing on the channel with the same pressure value.
 *
 * Key features:
 * - **Single pressure value**: Applies to all notes on the channel
 * - **Real-time control**: Often used for expression and vibrato
 * - **Value range**: 0-127 (0 = no pressure, 127 = maximum pressure)
 * - **Common usage**: Breath controllers, pressure-sensitive keyboards
 */
class ChannelPressureEvent : public MidiEvent {
public:
    /**
     * \brief Creates a new ChannelPressureEvent.
     * \param channel MIDI channel (0-15)
     * \param value Pressure value (0-127)
     * \param track The MIDI track this event belongs to
     */
    ChannelPressureEvent(int channel, int value, MidiTrack *track);

    /**
     * \brief Creates a new ChannelPressureEvent copying another instance.
     * \param other The ChannelPressureEvent instance to copy
     */
    ChannelPressureEvent(ChannelPressureEvent &other);

    /**
     * \brief Gets the display line for this event.
     * \return The line number where this event should be displayed
     */
    virtual int line();

    /**
     * \brief Gets a string representation of this event.
     * \return String message describing the channel pressure event
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
     * \return String identifying this as a "Channel Pressure" event
     */
    QString typeString();

    /**
     * \brief Gets the pressure value.
     * \return The pressure value (0-127)
     */
    int value();

    /**
     * \brief Sets the pressure value.
     * \param v The new pressure value (0-127)
     */
    void setValue(int v);

private:
    /** \brief The pressure value (0-127) */
    int _value;
};

#endif // CHANNELPRESSUREEVENT_H_
