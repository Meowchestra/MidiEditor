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

#ifndef TEMPOCHANGEEVENT_H_
#define TEMPOCHANGEEVENT_H_

#include "MidiEvent.h"

/**
 * \class TempoChangeEvent
 *
 * \brief MIDI Tempo Change event for controlling playback speed.
 *
 * TempoChangeEvent represents MIDI Tempo Change meta-events, which control
 * the playback tempo of the MIDI sequence. These events are crucial for
 * musical timing and expression.
 *
 * Key features:
 * - **BPM control**: Sets the beats per minute for playback
 * - **Timing precision**: Affects the duration of all subsequent events
 * - **Meta-event**: Not channel-specific, affects the entire sequence
 * - **Real-time changes**: Allows tempo changes during playback
 * - **Musical expression**: Enables rubato, accelerando, ritardando
 *
 * The tempo is stored internally as microseconds per quarter note,
 * which provides high precision for timing calculations.
 */
class TempoChangeEvent : public MidiEvent {
public:
    /**
     * \brief Creates a new TempoChangeEvent.
     * \param channel MIDI channel (typically 0 for meta-events)
     * \param value Tempo value in microseconds per quarter note
     * \param track The MIDI track this event belongs to
     */
    TempoChangeEvent(int channel, int value, MidiTrack *track);

    /**
     * \brief Creates a new TempoChangeEvent copying another instance.
     * \param other The TempoChangeEvent instance to copy
     */
    TempoChangeEvent(TempoChangeEvent &other);

    /**
     * \brief Gets the tempo in beats per quarter note.
     * \return The tempo in BPM (beats per minute)
     */
    int beatsPerQuarter();

    /**
     * \brief Gets the duration of one MIDI tick in milliseconds.
     * \return Milliseconds per tick at this tempo
     */
    double msPerTick();

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
     * \return String identifying this as a "Tempo Change" event
     */
    QString typeString();

    /**
     * \brief Sets the tempo in beats per minute.
     * \param beats The new tempo in BPM
     */
    void setBeats(int beats);

private:
    /** \brief Tempo value in microseconds per quarter note */
    int _beats;
};

#endif // TEMPOCHANGEEVENT_H_
