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

#ifndef SYSEXEVENT_H_
#define SYSEXEVENT_H_

#include "MidiEvent.h"
#include <QByteArray>

/**
 * \class SysExEvent
 *
 * \brief MIDI System Exclusive event for device-specific data.
 *
 * SysExEvent represents MIDI System Exclusive (SysEx) messages, which are
 * used to transmit device-specific data that doesn't fit into standard
 * MIDI message categories. SysEx messages are commonly used for:
 *
 * - **Parameter changes**: Device-specific settings and configurations
 * - **Patch data**: Sound bank uploads and downloads
 * - **Firmware updates**: Device software updates
 * - **Custom protocols**: Manufacturer-specific communication
 * - **Real-time data**: Continuous controller data streams
 *
 * Key features:
 * - **Variable length**: Can contain any amount of data
 * - **Manufacturer ID**: First bytes identify the device manufacturer
 * - **Raw data storage**: Preserves exact byte sequences
 * - **Device targeting**: Can be directed to specific devices
 * - **Non-standard format**: Outside normal MIDI channel/note structure
 *
 * SysEx messages begin with 0xF0 and end with 0xF7, with manufacturer
 * and device-specific data in between.
 */
class SysExEvent : public MidiEvent {
public:
    /**
     * \brief Creates a new SysExEvent.
     * \param channel MIDI channel (typically 0 for system messages)
     * \param data The SysEx data bytes (without F0/F7 framing)
     * \param track The MIDI track this event belongs to
     */
    SysExEvent(int channel, QByteArray data, MidiTrack *track);

    /**
     * \brief Creates a new SysExEvent copying another instance.
     * \param other The SysExEvent instance to copy
     */
    SysExEvent(SysExEvent &other);

    /**
     * \brief Gets the SysEx data bytes.
     * \return QByteArray containing the raw SysEx data
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
     * \brief Gets the type string for this event.
     * \return String identifying this as a "System Exclusive" event
     */
    QString typeString();

    /**
     * \brief Creates a copy of this event for the protocol system.
     * \return A new ProtocolEntry representing this event's state
     */
    ProtocolEntry *copy();

    /**
     * \brief Reloads the event's state from a protocol entry.
     * \param entry The protocol entry to restore state from
     */
    virtual void reloadState(ProtocolEntry *entry);

    /**
     * \brief Sets the SysEx data bytes.
     * \param d QByteArray containing the new SysEx data
     */
    void setData(QByteArray d);

private:
    /** \brief The raw SysEx data bytes */
    QByteArray _data;
};

#endif // SYSEXEVENT_H_
