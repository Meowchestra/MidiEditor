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

#ifndef MIDICHANNEL_H_
#define MIDICHANNEL_H_

// Project includes
#include "../protocol/ProtocolEntry.h"

// Qt includes
#include <QMultiMap>

// Forward declarations
class MidiFile;
class MidiEvent;
class QColor;
class MidiTrack;
class NoteOnEvent;

/**
 * \class MidiChannel
 *
 * \brief Represents a MIDI channel containing events for a specific instrument.
 *
 * MidiChannel manages all MIDI events for a specific channel within the MIDI file.
 * The MIDI editor uses 18 channels in total:
 *
 * - **Channels 0-15**: Standard MIDI channels for instruments
 * - **Channel 16**: General events (like PitchBend)
 * - **Channel 17**: Tempo change events
 * - **Channel 18**: Time signature events
 *
 * Key features:
 * - **Event management**: Stores and organizes all events for the channel
 * - **Visibility control**: Show/hide channel in the matrix widget
 * - **Mute control**: Enable/disable channel playback
 * - **Solo mode**: Play only this channel (mutes all others)
 * - **Color coding**: Visual identification in the editor
 * - **Program tracking**: Maintains current instrument program
 * - **Note insertion**: Convenient methods for adding notes
 *
 * Events are stored in a QMultiMap organized by MIDI tick time, allowing
 * efficient time-based access and manipulation.
 */
class MidiChannel : public ProtocolEntry {
public:
    /**
     * \brief Creates a new MidiChannel with the specified number.
     * \param f The MidiFile this channel belongs to
     * \param num The channel number (0-18)
     */
    MidiChannel(MidiFile *f, int num);

    /**
     * \brief Creates a new MidiChannel copying another instance.
     * \param other The MidiChannel instance to copy
     */
    MidiChannel(MidiChannel &other);

    // === Basic Properties ===

    /**
     * \brief Gets the parent MIDI file.
     * \return Pointer to the MidiFile containing this channel
     */
    MidiFile *file();

    /**
     * \brief Gets the channel number.
     * \return Channel number: 0-15 for MIDI channels, 16 for general events,
     *         17 for tempo changes, 18 for time signature events
     */
    int number();

    /**
     * \brief Gets the channel's display color.
     * \return Pointer to the QColor used for visual representation
     *
     * The color is determined by the channel number and provides
     * consistent visual identification across the editor.
     */
    QColor *color();

    // === Event Management ===

    /**
     * \brief Gets the event map containing all channel events.
     * \return Pointer to QMultiMap organized by MIDI tick time
     */
    QMultiMap<int, MidiEvent *> *eventMap();

    /**
     * \brief Inserts a new note into this channel.
     * \param note MIDI note number (0-127)
     * \param startTick Start time in MIDI ticks
     * \param endTick End time in MIDI ticks
     * \param velocity Note velocity (0-127)
     * \param track The MIDI track to associate with the note
     * \return Pointer to the created NoteOnEvent
     */
    NoteOnEvent *insertNote(int note, int startTick, int endTick, int velocity, MidiTrack *track);

    /**
     * \brief Inserts an event into the channel's event map.
     * \param event The MIDI event to insert
     * \param tick The time position in MIDI ticks
     * \param toProtocol Whether to record this change in the protocol
     */
    void insertEvent(MidiEvent *event, int tick, bool toProtocol = true);

    /**
     * \brief Removes an event from the channel's event map.
     * \param event The MIDI event to remove
     * \return True if the event was found and removed
     */
    bool removeEvent(MidiEvent *event);

    /**
     * \brief Gets the program number active at the specified tick.
     * \param tick The time position to query
     * \return The MIDI program number (0-127) active at that time
     */
    int progAtTick(int tick);

    /**
     * \brief Removes all events from the channel.
     */
    void deleteAllEvents();

    // === Display and Playback Control ===

    /**
     * \brief Gets the channel's visibility state.
     * \return True if the channel is visible in the MatrixWidget
     */
    bool visible();

    /**
     * \brief Sets the channel's visibility state.
     * \param b True to show the channel, false to hide it
     */
    void setVisible(bool b);

    /**
     * \brief Gets the channel's mute state.
     * \return True if the channel is muted (no sound output)
     */
    bool mute();

    /**
     * \brief Sets the channel's mute state.
     * \param b True to mute the channel, false to unmute it
     */
    void setMute(bool b);

    /**
     * \brief Gets the channel's solo state.
     * \return True if the channel is in solo mode
     *
     * When a channel is in solo mode, all other channels are effectively muted.
     */
    bool solo();

    /**
     * \brief Sets the channel's solo state.
     * \param b True to enable solo mode, false to disable it
     */
    void setSolo(bool b);

    // === Protocol System Integration ===

    /**
     * \brief Creates a copy of this channel for the protocol system.
     * \return A new ProtocolEntry representing this channel's state
     */
    ProtocolEntry *copy();

    /**
     * \brief Reloads the channel's state from a protocol entry.
     * \param entry The protocol entry to restore state from
     */
    void reloadState(ProtocolEntry *entry);

protected:
    /** \brief The parent MIDI file */
    MidiFile *_midiFile;

    /** \brief Channel state flags */
    bool _visible, _mute, _solo;

    /** \brief Event map organized by MIDI tick time */
    QMultiMap<int, MidiEvent *> *_events;

    /** \brief The channel number (0-18) */
    int _num;
};

#endif // MIDICHANNEL_H_
