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

#ifndef UNKNOWNEVENT_H_
#define UNKNOWNEVENT_H_

#include "MidiEvent.h"
#include <QByteArray>

/**
 * \class UnknownEvent
 *
 * \brief MIDI event for unrecognized or unsupported MIDI message types.
 *
 * UnknownEvent serves as a fallback container for MIDI messages that are
 * not recognized by the editor or don't have specific event classes. This
 * ensures that all MIDI data is preserved, even if it can't be interpreted.
 *
 * Key features:
 * - **Data preservation**: Stores raw MIDI bytes without interpretation
 * - **Type tracking**: Records the original MIDI message type
 * - **Fallback handling**: Prevents data loss from unsupported messages
 * - **Future compatibility**: Allows handling of new MIDI message types
 * - **Debug support**: Helps identify unimplemented MIDI features
 *
 * Common uses:
 * - Proprietary MIDI extensions
 * - Newer MIDI specifications not yet supported
 * - Corrupted or malformed MIDI data
 * - Experimental MIDI message types
 * - Manufacturer-specific messages outside SysEx
 *
 * The event preserves the original data exactly as it appeared in the
 * MIDI file, allowing for round-trip compatibility.
 */
class UnknownEvent : public MidiEvent {
public:
    /**
     * \brief Creates a new UnknownEvent.
     * \param channel MIDI channel (0-15)
     * \param type The MIDI message type byte
     * \param data The raw MIDI data bytes
     * \param track The MIDI track this event belongs to
     */
    UnknownEvent(int channel, int type, QByteArray data, MidiTrack *track);

    /**
     * \brief Creates a new UnknownEvent copying another instance.
     * \param other The UnknownEvent instance to copy
     */
    UnknownEvent(UnknownEvent &other);

    /**
     * \brief Gets the raw MIDI data bytes.
     * \return QByteArray containing the uninterpreted MIDI data
     */
    QByteArray data();

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
     * \brief Gets the MIDI message type.
     * \return The original MIDI message type byte
     */
    int type();

    /**
     * \brief Sets the MIDI message type.
     * \param type The MIDI message type byte
     */
    void setType(int type);

    /**
     * \brief Sets the raw MIDI data bytes.
     * \param d QByteArray containing the new MIDI data
     */
    void setData(QByteArray d);

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

private:
    /** \brief The raw MIDI data bytes */
    QByteArray _data;

    /** \brief The MIDI message type byte */
    int _type;
};

#endif // UNKNOWNEVENT_H_
