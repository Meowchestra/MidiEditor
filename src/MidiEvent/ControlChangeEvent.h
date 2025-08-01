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

#ifndef CONTROLCHANGEEVENT_H_
#define CONTROLCHANGEEVENT_H_

#include "MidiEvent.h"

/**
 * \class ControlChangeEvent
 *
 * \brief MIDI Control Change event for modifying controller parameters.
 *
 * ControlChangeEvent represents MIDI Control Change messages (CC), which are
 * used to modify various parameters of MIDI instruments and effects. These
 * events control aspects like:
 *
 * - **Volume** (CC 7): Channel volume level
 * - **Pan** (CC 10): Stereo positioning
 * - **Expression** (CC 11): Expression pedal
 * - **Sustain** (CC 64): Sustain pedal on/off
 * - **Modulation** (CC 1): Modulation wheel
 * - And many other controllers (0-127)
 *
 * Each Control Change event specifies a controller number (0-127) and
 * a value (0-127) to set for that controller.
 */
class ControlChangeEvent : public MidiEvent {
public:
    /**
     * \brief Creates a new ControlChangeEvent.
     * \param channel MIDI channel (0-15)
     * \param contr Controller number (0-127)
     * \param val Controller value (0-127)
     * \param track The MIDI track this event belongs to
     */
    ControlChangeEvent(int channel, int contr, int val, MidiTrack *track);

    /**
     * \brief Creates a new ControlChangeEvent copying another instance.
     * \param other The ControlChangeEvent instance to copy
     */
    ControlChangeEvent(ControlChangeEvent &other);

    /**
     * \brief Gets the display line for this event.
     * \return The line number where this event should be displayed
     */
    virtual int line();

    /**
     * \brief Gets the controller number.
     * \return The controller number (0-127)
     */
    int control();

    /**
     * \brief Gets the controller value.
     * \return The controller value (0-127)
     */
    int value();

    /**
     * \brief Sets the controller value.
     * \param v The new controller value (0-127)
     */
    void setValue(int v);

    /**
     * \brief Sets the controller number.
     * \param c The new controller number (0-127)
     */
    void setControl(int c);

    /**
     * \brief Gets a string representation of this event.
     * \return String message describing the control change event
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
     * \return String identifying this as a "Control Change" event
     */
    QString typeString();

    /**
     * \brief Returns whether this is an OnEvent.
     * \return False (this is not an OnEvent)
     */
    virtual bool isOnEvent();

private:
    /** \brief Controller number and value */
    int _control, _value;
};

#endif // CONTROLCHANGEEVENT_H_
