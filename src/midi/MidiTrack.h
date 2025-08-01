/*
 * MidiEditor
 * Copyright (C) 2010  Markus Schwenk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.+
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MIDITRACK_H_
#define MIDITRACK_H_

// Project includes
#include "../protocol/ProtocolEntry.h"

// Qt includes
#include <QObject>
#include <QString>

// Forward declarations
class TextEvent;
class MidiFile;
class QColor;

/**
 * \class MidiTrack
 *
 * \brief Represents a single MIDI track within a MIDI file.
 *
 * MidiTrack manages a collection of MIDI events that belong to a specific
 * track in the MIDI file. Each track can contain events for multiple MIDI
 * channels and provides organization and management capabilities:
 *
 * - **Event organization**: Groups related MIDI events together
 * - **Track naming**: User-friendly track names and identification
 * - **Channel assignment**: Default channel assignment for new events
 * - **Visibility control**: Show/hide tracks in the editor
 * - **Mute control**: Enable/disable track playback
 * - **Color coding**: Visual identification in the editor
 *
 * Key features:
 * - Protocol integration for undo/redo support
 * - Track name management with TextEvent integration
 * - Channel assignment for streamlined event creation
 * - Visibility and mute state management
 * - Color customization for visual organization
 * - Integration with the parent MidiFile
 *
 * Tracks provide a logical grouping mechanism that helps organize complex
 * MIDI compositions with multiple instruments or parts.
 */
class MidiTrack : public QObject, public ProtocolEntry {
    Q_OBJECT

public:
    /**
     * \brief Creates a new MidiTrack.
     * \param file The parent MidiFile this track belongs to
     */
    MidiTrack(MidiFile *file);

    /**
     * \brief Creates a new MidiTrack copying another instance.
     * \param other The MidiTrack instance to copy
     */
    MidiTrack(MidiTrack &other);

    /**
     * \brief Destroys the MidiTrack and cleans up resources.
     */
    virtual ~MidiTrack();

    // === Track Identification ===

    /**
     * \brief Gets the track name.
     * \return The user-friendly name of this track
     */
    QString name();

    /**
     * \brief Sets the track name.
     * \param name The new name for this track
     */
    void setName(QString name);

    /**
     * \brief Gets the track number.
     * \return The numeric identifier of this track
     */
    int number();

    /**
     * \brief Sets the track number.
     * \param number The new numeric identifier for this track
     */
    void setNumber(int number);

    /**
     * \brief Sets the TextEvent that contains the track name.
     * \param nameEvent The TextEvent containing the track name
     */
    void setNameEvent(TextEvent *nameEvent);

    /**
     * \brief Gets the TextEvent that contains the track name.
     * \return The TextEvent containing the track name, or nullptr if none
     */
    TextEvent *nameEvent();

    // === File Association ===

    /**
     * \brief Gets the parent MIDI file.
     * \return Pointer to the MidiFile containing this track
     */
    MidiFile *file();

    // === Channel Management ===

    /**
     * \brief Assigns a default MIDI channel to this track.
     * \param ch The MIDI channel (0-15) to assign
     */
    void assignChannel(int ch);

    /**
     * \brief Gets the assigned MIDI channel.
     * \return The default MIDI channel for this track (0-15)
     */
    int assignedChannel();

    // === Visibility and State ===

    /**
     * \brief Sets the hidden state of the track.
     * \param hidden True to hide the track, false to show it
     */
    void setHidden(bool hidden);

    /**
     * \brief Gets the hidden state of the track.
     * \return True if the track is hidden, false if visible
     */
    bool hidden();

    /**
     * \brief Sets the muted state of the track.
     * \param muted True to mute the track, false to unmute it
     */
    void setMuted(bool muted);

    /**
     * \brief Gets the muted state of the track.
     * \return True if the track is muted, false if audible
     */
    bool muted();

    // === Protocol System Integration ===

    /**
     * \brief Creates a copy of this track for the protocol system.
     * \return A new ProtocolEntry representing this track's state
     */
    virtual ProtocolEntry *copy();

    /**
     * \brief Reloads the track's state from a protocol entry.
     * \param entry The protocol entry to restore state from
     */
    virtual void reloadState(ProtocolEntry *entry);

    // === Visual and Utility Methods ===

    /**
     * \brief Gets the track's display color.
     * \return Pointer to the QColor used for visual representation
     */
    QColor *color();

    /**
     * \brief Creates a copy of this track in another MIDI file.
     * \param file The target MidiFile to copy the track to
     * \return Pointer to the newly created MidiTrack copy
     */
    MidiTrack *copyToFile(MidiFile *file);

signals:
    /**
     * \brief Emitted when the track's properties change.
     */
    void trackChanged();

private:
    /** \brief Track number identifier */
    int _number;

    /** \brief TextEvent containing the track name */
    TextEvent *_nameEvent;

    /** \brief Parent MIDI file */
    MidiFile *_file;

    /** \brief Track visibility and mute state */
    bool _hidden, _muted;

    /** \brief Default MIDI channel assignment */
    int _assignedChannel;
};

#endif // MIDITRACK_H_
