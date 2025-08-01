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

#ifndef KEYPRESSUREEVENT_H_
#define KEYPRESSUREEVENT_H_

#include "MidiEvent.h"

#include <QLabel>

/**
 * \class KeyPressureEvent
 *
 * \brief MIDI Key Pressure event for polyphonic aftertouch control.
 *
 * KeyPressureEvent represents MIDI Key Pressure messages (also known as
 * Polyphonic Aftertouch), which transmit pressure information for individual
 * keys after the initial key press.
 *
 * Unlike Channel Pressure, Key Pressure allows different pressure values
 * for each note currently playing, enabling more expressive control.
 *
 * Key features:
 * - **Per-note pressure**: Each note can have its own pressure value
 * - **Note-specific**: Identified by both note number and pressure value
 * - **Value range**: 0-127 (0 = no pressure, 127 = maximum pressure)
 * - **Expressive control**: Used for vibrato, timbre changes, volume swells
 */
class KeyPressureEvent : public MidiEvent {
public:
    /**
     * \brief Creates a new KeyPressureEvent.
     * \param channel MIDI channel (0-15)
     * \param value Pressure value (0-127)
     * \param note MIDI note number (0-127)
     * \param track The MIDI track this event belongs to
     */
    KeyPressureEvent(int channel, int value, int note, MidiTrack *track);

    /**
     * \brief Creates a new KeyPressureEvent copying another instance.
     * \param other The KeyPressureEvent instance to copy
     */
    KeyPressureEvent(KeyPressureEvent &other);

    /**
     * \brief Gets the display line for this event.
     * \return The line number where this event should be displayed
     */
    virtual int line();

    /**
     * \brief Gets a string representation of this event.
     * \return String message describing the key pressure event
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
     * \return String identifying this as a "Key Pressure" event
     */
    QString typeString();

    /**
     * \brief Gets the pressure value.
     * \return The pressure value (0-127)
     */
    int value();

    /**
     * \brief Gets the note number.
     * \return The MIDI note number (0-127)
     */
    int note();

    /**
     * \brief Sets the pressure value.
     * \param v The new pressure value (0-127)
     */
    void setValue(int v);

    /**
     * \brief Sets the note number.
     * \param n The new MIDI note number (0-127)
     */
    void setNote(int n);

private:
    /** \brief Pressure value and note number */
    int _value, _note;
};

#endif // KEYPRESSUREEVENT_H_
