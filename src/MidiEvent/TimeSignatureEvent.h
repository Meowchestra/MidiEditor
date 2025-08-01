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

#ifndef TIMESIGNATUREEVENT_H_
#define TIMESIGNATUREEVENT_H_

#include "MidiEvent.h"

/**
 * \class TimeSignatureEvent
 *
 * \brief MIDI Time Signature meta-event for musical meter information.
 *
 * TimeSignatureEvent represents MIDI Time Signature meta-events, which specify
 * the musical meter and timing structure of the composition. This information
 * is crucial for:
 *
 * - **Musical notation**: Proper bar lines and beat grouping
 * - **Quantization**: Beat-aware note alignment
 * - **Metronome**: Click track generation
 * - **Analysis**: Understanding rhythmic structure
 * - **Display**: Measure numbering and grid display
 *
 * Key components:
 * - **Numerator**: Number of beats per measure (e.g., 4 in 4/4 time)
 * - **Denominator**: Note value that gets the beat (e.g., 4 = quarter note)
 * - **MIDI clocks**: Timing resolution for metronome clicks
 * - **32nd notes**: Number of 32nd notes in a quarter note (usually 8)
 *
 * Common time signatures: 4/4, 3/4, 2/4, 6/8, 12/8, etc.
 */
class TimeSignatureEvent : public MidiEvent {
public:
    /**
     * \brief Creates a new TimeSignatureEvent.
     * \param channel MIDI channel (typically 0 for meta-events)
     * \param num Numerator (beats per measure)
     * \param denom Denominator (note value that gets the beat)
     * \param midiClocks MIDI clocks per metronome click
     * \param num32In4 Number of 32nd notes in a quarter note
     * \param track The MIDI track this event belongs to
     */
    TimeSignatureEvent(int channel, int num, int denom, int midiClocks,
                       int num32In4, MidiTrack *track);

    /**
     * \brief Creates a new TimeSignatureEvent copying another instance.
     * \param other The TimeSignatureEvent instance to copy
     */
    TimeSignatureEvent(TimeSignatureEvent &other);

    /**
     * \brief Gets the numerator (beats per measure).
     * \return The numerator of the time signature
     */
    int num();

    /**
     * \brief Gets the denominator (note value for the beat).
     * \return The denominator of the time signature
     */
    int denom();

    /**
     * \brief Gets the MIDI clocks per metronome click.
     * \return Number of MIDI clocks per metronome click
     */
    int midiClocks();

    /**
     * \brief Gets the number of 32nd notes in a quarter note.
     * \return Number of 32nd notes in a quarter note (usually 8)
     */
    int num32In4();

    /**
     * \brief Calculates the measure number for a given tick.
     * \param tick The MIDI tick position
     * \param ticksLeft Optional pointer to receive remaining ticks in measure
     * \return The measure number (0-based)
     */
    int measures(int tick, int *ticksLeft = 0);

    /**
     * \brief Gets the number of ticks per measure.
     * \return Number of MIDI ticks in one complete measure
     */
    int ticksPerMeasure();

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
     * \brief Sets the denominator (note value for the beat).
     * \param d The new denominator value
     */
    void setDenominator(int d);

    /**
     * \brief Sets the numerator (beats per measure).
     * \param n The new numerator value
     */
    void setNumerator(int n);

    /**
     * \brief Gets the type string for this event.
     * \return String identifying this as a "Time Signature" event
     */
    QString typeString();

private:
    /** \brief Time signature components */
    int numerator, denominator, midiClocksPerMetronome, num32In4th;
};

#endif // TIMESIGNATUREEVENT_H_
