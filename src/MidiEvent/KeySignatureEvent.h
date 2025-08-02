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

#ifndef KEYSIGNATUREEVENT_H_
#define KEYSIGNATUREEVENT_H_

#include "MidiEvent.h"

/**
 * \class KeySignatureEvent
 *
 * \brief MIDI Key Signature meta-event for musical key information.
 *
 * KeySignatureEvent represents MIDI Key Signature meta-events, which specify
 * the musical key of the composition. This information is used for:
 *
 * - **Musical notation**: Proper display of sharps and flats
 * - **Transposition**: Key-aware pitch shifting
 * - **Analysis**: Understanding the harmonic context
 * - **Display**: Showing key signatures in musical notation
 *
 * Key features:
 * - **Tonality**: Number of sharps (positive) or flats (negative)
 * - **Mode**: Major or minor key
 * - **Standard keys**: Support for all 24 major and minor keys
 * - **Meta-event**: Affects the entire sequence, not channel-specific
 *
 * Examples: C major (0, false), G major (1, false), F major (-1, false),
 * A minor (0, true), E minor (1, true), D minor (-1, true)
 */
class KeySignatureEvent : public MidiEvent {
public:
    /**
     * \brief Creates a new KeySignatureEvent.
     * \param channel MIDI channel (typically 0 for meta-events)
     * \param tonality Number of sharps (positive) or flats (negative)
     * \param minor True for minor key, false for major key
     * \param track The MIDI track this event belongs to
     */
    KeySignatureEvent(int channel, int tonality, bool minor, MidiTrack *track);

    /**
     * \brief Creates a new KeySignatureEvent copying another instance.
     * \param other The KeySignatureEvent instance to copy
     */
    KeySignatureEvent(KeySignatureEvent &other);

    /**
     * \brief Gets the display line for this event.
     * \return The line number where this event should be displayed
     */
    virtual int line();

    /**
     * \brief Gets a string representation of this event.
     * \return String message describing the key signature event
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
     * \return String identifying this as a "Key Signature" event
     */
    QString typeString();

    /**
     * \brief Gets the tonality (number of sharps/flats).
     * \return Positive for sharps, negative for flats, 0 for C major/A minor
     */
    int tonality();

    /**
     * \brief Gets whether this is a minor key.
     * \return True for minor key, false for major key
     */
    bool minor();

    /**
     * \brief Sets the tonality (number of sharps/flats).
     * \param t Positive for sharps, negative for flats
     */
    void setTonality(int t);

    /**
     * \brief Sets whether this is a minor key.
     * \param minor True for minor key, false for major key
     */
    void setMinor(bool minor);

    /**
     * \brief Converts tonality and mode to a readable string.
     * \param tonality Number of sharps (positive) or flats (negative)
     * \param minor True for minor key, false for major key
     * \return Human-readable key signature string (e.g., "G major", "E minor")
     */
    static QString toString(int tonality, bool minor);

private:
    /** \brief Number of sharps (positive) or flats (negative) */
    int _tonality;

    /** \brief True for minor key, false for major key */
    bool _minor;
};

#endif // KEYSIGNATUREEVENT_H_
