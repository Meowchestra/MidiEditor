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

#ifndef PROGCHANGEEVENT_H_
#define PROGCHANGEEVENT_H_

#include "MidiEvent.h"

/**
 * \class ProgChangeEvent
 *
 * \brief MIDI Program Change event for instrument selection.
 *
 * ProgChangeEvent represents MIDI Program Change messages, which are used to
 * select different instruments or sounds on a MIDI channel. This allows
 * switching between different patches, voices, or presets.
 *
 * Key features:
 * - **Instrument selection**: Changes the active sound/patch on a channel
 * - **Program range**: 0-127 (128 different programs available)
 * - **General MIDI**: Standard program numbers for common instruments
 * - **Channel-specific**: Each channel can have its own program
 * - **Instant change**: Takes effect immediately for new notes
 *
 * Common General MIDI programs include Piano (0), Guitar (24-31),
 * Bass (32-39), Strings (40-47), Brass (56-63), etc.
 */
class ProgChangeEvent : public MidiEvent {
public:
    /**
     * \brief Creates a new ProgChangeEvent.
     * \param channel MIDI channel (0-15)
     * \param prog Program number (0-127)
     * \param track The MIDI track this event belongs to
     */
    ProgChangeEvent(int channel, int prog, MidiTrack *track);

    /**
     * \brief Creates a new ProgChangeEvent copying another instance.
     * \param other The ProgChangeEvent instance to copy
     */
    ProgChangeEvent(ProgChangeEvent &other);

    /**
     * \brief Gets the display line for this event.
     * \return The line number where this event should be displayed
     */
    virtual int line();

    /**
     * \brief Gets a string representation of this event.
     * \return String message describing the program change event
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
     * \return String identifying this as a "Program Change" event
     */
    QString typeString();

    /**
     * \brief Gets the program number.
     * \return The program number (0-127)
     */
    int program();

    /**
     * \brief Sets the program number.
     * \param prog The new program number (0-127)
     */
    void setProgram(int prog);

private:
    /** \brief The program number (0-127) */
    int _program;
};

#endif // PROGCHANGEEVENT_H_
