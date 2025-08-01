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

#ifndef TEXTEVENT_H_
#define TEXTEVENT_H_

#include "MidiEvent.h"
#include <QByteArray>

/**
 * \class TextEvent
 *
 * \brief MIDI text event for storing textual information in MIDI files.
 *
 * TextEvent represents various types of text meta-events that can be embedded
 * in MIDI files. These events carry textual information such as:
 *
 * - Song lyrics
 * - Track names
 * - Copyright notices
 * - Instrument names
 * - Markers and comments
 *
 * Each text event has a specific type that determines how the text should be
 * interpreted and displayed.
 */
class TextEvent : public MidiEvent {
public:
    /**
     * \brief Creates a new TextEvent.
     * \param channel The MIDI channel (0-15)
     * \param track The MIDI track this event belongs to
     */
    TextEvent(int channel, MidiTrack *track);

    /**
     * \brief Creates a new TextEvent copying another instance.
     * \param other The TextEvent instance to copy
     */
    TextEvent(TextEvent &other);

    /**
     * \brief Gets the text content of this event.
     * \return The text string stored in this event
     */
    QString text();

    /**
     * \brief Sets the text content of this event.
     * \param text The text string to store in this event
     */
    void setText(QString text);

    /**
     * \brief Gets the type of this text event.
     * \return The text event type (see enum values)
     */
    int type();

    /**
     * \brief Sets the type of this text event.
     * \param type The text event type (see enum values)
     */
    void setType(int type);

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
     * \brief Text event type constants.
     */
    enum {
        TEXT = 0x01,       ///< General text
        COPYRIGHT,         ///< Copyright notice
        TRACKNAME,         ///< Track name
        INSTRUMENT_NAME,   ///< Instrument name
        LYRIK,             ///< Lyrics
        MARKER,            ///< Marker text
        COMMENT            ///< Comment text
    };

    /**
     * \brief Gets a string representation of this event's type.
     * \return Human-readable string describing the event type
     */
    QString typeString();

    /**
     * \brief Gets a string representation of the given text type.
     * \param type The text event type
     * \return Human-readable string describing the event type
     */
    static QString textTypeString(int type);

    /**
     * \brief Gets the default type for new text events.
     * \return The default text event type
     */
    static int getTypeForNewEvents();

    /**
     * \brief Sets the default type for new text events.
     * \param type The default text event type to use
     */
    static void setTypeForNewEvents(int type);

private:
    /** \brief The type of this text event */
    int _type;

    /** \brief The text content of this event */
    QString _text;

    /** \brief Default type for new text events */
    static int typeForNewEvents;
};

#endif // TEXTEVENT_H_
