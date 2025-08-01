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

#ifndef MIDIFILE_H_
#define MIDIFILE_H_

// Project includes
#include "../protocol/ProtocolEntry.h"

// Qt includes
#include <QDataStream>
#include <QMultiMap>
#include <QObject>

// Forward declarations
class MidiEvent;
class TimeSignatureEvent;
class TempoChangeEvent;
class Protocol;
class MidiChannel;
class MidiTrack;

/**
 * \class MidiFile
 *
 * \brief Core class representing a complete MIDI file with all its data and functionality.
 *
 * MidiFile is the central class of the MIDI editor, managing all aspects of MIDI
 * file handling, playback, and editing. It provides comprehensive functionality for:
 *
 * - **File I/O**: Loading and saving MIDI files in standard format
 * - **Track management**: Multiple MIDI tracks with independent content
 * - **Channel management**: 16 MIDI channels per track
 * - **Event management**: All types of MIDI events and meta-events
 * - **Timing**: Tempo changes, time signatures, and timing calculations
 * - **Protocol integration**: Full undo/redo support for all operations
 * - **Playback support**: Integration with MIDI playback systems
 *
 * Key features:
 * - Standard MIDI file format support (Type 0 and Type 1)
 * - Real-time event manipulation and editing
 * - Comprehensive timing and measure calculations
 * - Multi-track and multi-channel organization
 * - Protocol-based undo/redo for all changes
 * - Event filtering and selection capabilities
 * - Tempo and time signature management
 *
 * The class serves as both a data container and a controller, managing
 * the relationships between tracks, channels, events, and timing information.
 */
class MidiFile : public QObject, public ProtocolEntry {
    Q_OBJECT

public:
    /**
     * \brief Creates a new MidiFile by loading from a file path.
     * \param path Path to the MIDI file to load
     * \param ok Pointer to bool indicating success/failure
     * \param log Optional string list to receive loading messages
     */
    MidiFile(QString path, bool *ok, QStringList *log = 0);

    /**
     * \brief Creates a new empty MidiFile.
     */
    MidiFile();

    /**
     * \brief Creates a new MidiFile for protocol operations.
     * \param maxTime Maximum time in ticks
     * \param p Protocol instance for undo/redo support
     */
    MidiFile(int maxTime, Protocol *p);

    /**
     * \brief Destroys the MidiFile and cleans up all resources.
     */
    ~MidiFile();

    // === File I/O Operations ===

    /**
     * \brief Saves the MIDI file to the specified path.
     * \param path File path to save to
     * \return True if save was successful, false otherwise
     */
    bool save(QString path);

    /**
     * \brief Writes a delta time value to a byte array.
     * \param time The time value to encode
     * \return QByteArray containing the encoded delta time
     */
    QByteArray writeDeltaTime(int time);

    // === Timing and Measurement ===

    /**
     * \brief Gets the maximum time of all events in the file.
     * \return Maximum time in MIDI ticks
     */
    int maxTime();

    /**
     * \brief Gets the end tick of the file (last event + duration).
     * \return End time in MIDI ticks
     */
    int endTick();

    /**
     * \brief Converts MIDI time to milliseconds.
     * \param midiTime Time in MIDI ticks
     * \return Time in milliseconds
     */
    int timeMS(int midiTime);

    /**
     * \brief Gets measure information for a given tick position.
     * \param startTick The tick position to query
     * \param startTickOfMeasure Pointer to receive measure start tick
     * \param endTickOfMeasure Pointer to receive measure end tick
     * \return The measure number (0-based)
     */
    int measure(int startTick, int *startTickOfMeasure, int *endTickOfMeasure);

    // === Event Access and Management ===

    /**
     * \brief Gets the map of tempo change events.
     * \return Pointer to QMap containing tempo events organized by tick
     */
    QMap<int, MidiEvent *> *tempoEvents();

    /**
     * \brief Gets the map of time signature events.
     * \return Pointer to QMap containing time signature events organized by tick
     */
    QMap<int, MidiEvent *> *timeSignatureEvents();

    /**
     * \brief Recalculates the maximum time of all events in the file.
     */
    void calcMaxTime();

    // === Time Conversion Methods ===

    /**
     * \brief Converts milliseconds to MIDI ticks.
     * \param ms Time in milliseconds
     * \return Time in MIDI ticks
     */
    int tick(int ms);

    /**
     * \brief Gets events and timing information for a time range.
     * \param startms Start time in milliseconds
     * \param endms End time in milliseconds
     * \param events Pointer to receive list of events in range
     * \param endTick Pointer to receive end tick
     * \param msOfFirstEvent Pointer to receive timing of first event
     * \return Start tick of the time range
     */
    int tick(int startms, int endms, QList<MidiEvent *> **events, int *endTick, int *msOfFirstEvent);

    /**
     * \brief Gets measure information for a tick range.
     * \param startTick Start tick position
     * \param endTick End tick position
     * \param eventList Pointer to receive list of time signature events
     * \param tickInMeasure Optional pointer to receive tick within measure
     * \return The measure number
     */
    int measure(int startTick, int endTick, QList<TimeSignatureEvent *> **eventList, int *tickInMeasure = 0);

    /**
     * \brief Converts MIDI ticks to milliseconds with event context.
     * \param tick Time in MIDI ticks
     * \param events Optional list of events for timing context
     * \param msOfFirstEventInList Timing reference for first event
     * \return Time in milliseconds
     */
    int msOfTick(int tick, QList<MidiEvent *> *events = 0, int msOfFirstEventInList = 0);

    /**
     * \brief Gets all events between two tick positions.
     * \param start Start tick position
     * \param end End tick position
     * \return Pointer to list of events in the specified range
     */
    QList<MidiEvent *> *eventsBetween(int start, int end);

    /**
     * \brief Gets the ticks per quarter note resolution.
     * \return Number of ticks per quarter note
     */
    int ticksPerQuarter();

    // === Channel and Protocol Access ===

    /**
     * \brief Gets all events for a specific MIDI channel.
     * \param channel The MIDI channel number (0-15)
     * \return Pointer to QMultiMap containing channel events organized by tick
     */
    QMultiMap<int, MidiEvent *> *channelEvents(int channel);

    /**
     * \brief Gets the protocol system for undo/redo operations.
     * \return Pointer to the Protocol instance
     */
    Protocol *protocol();

    /**
     * \brief Gets a specific MIDI channel.
     * \param i The channel number (0-18, where 16-18 are special channels)
     * \return Pointer to the MidiChannel instance
     */
    MidiChannel *channel(int i);

    // === Playback Support ===

    /**
     * \brief Prepares player data starting from a specific tick.
     * \param tickFrom The starting tick position for playback preparation
     */
    void preparePlayerData(int tickFrom);

    /**
     * \brief Gets the prepared player data.
     * \return Pointer to QMultiMap containing events organized for playback
     */
    QMultiMap<int, MidiEvent *> *playerData();

    // === Static Utility Methods ===

    /**
     * \brief Gets the name of a General MIDI instrument.
     * \param prog The program number (0-127)
     * \return String name of the instrument
     */
    static QString instrumentName(int prog);

    /**
     * \brief Gets the name of a MIDI control change.
     * \param control The control change number (0-127)
     * \return String name of the control change
     */
    static QString controlChangeName(int control);

    // === Cursor and Position Management ===

    /**
     * \brief Gets the current cursor position.
     * \return Cursor position in MIDI ticks
     */
    int cursorTick();

    /**
     * \brief Gets the pause position.
     * \return Pause position in MIDI ticks
     */
    int pauseTick();

    /**
     * \brief Sets the cursor position.
     * \param tick New cursor position in MIDI ticks
     */
    void setCursorTick(int tick);

    /**
     * \brief Sets the pause position.
     * \param tick New pause position in MIDI ticks
     */
    void setPauseTick(int tick);

    // === File Management ===

    /**
     * \brief Gets the file path.
     * \return String path to the MIDI file
     */
    QString path();

    /**
     * \brief Checks if the file has been saved.
     * \return True if the file is saved, false if modified
     */
    bool saved();

    /**
     * \brief Sets the saved state of the file.
     * \param b True to mark as saved, false to mark as modified
     */
    void setSaved(bool b);

    /**
     * \brief Sets the file path.
     * \param path New file path string
     */
    void setPath(QString path);

    // === Channel and Track Management ===

    /**
     * \brief Checks if a channel is muted.
     * \param ch The channel number to check
     * \return True if the channel is muted
     */
    bool channelMuted(int ch);

    /**
     * \brief Gets the number of tracks in the file.
     * \return Number of MIDI tracks
     */
    int numTracks();

    /**
     * \brief Gets the list of all tracks.
     * \return Pointer to QList containing all MidiTrack instances
     */
    QList<MidiTrack *> *tracks();

    /**
     * \brief Adds a new track to the file.
     */
    void addTrack();

    /**
     * \brief Removes a track from the file.
     * \param track The MidiTrack to remove
     * \return True if the track was successfully removed
     */
    bool removeTrack(MidiTrack *track);

    // === File Structure Modification ===

    /**
     * \brief Sets the maximum length of the file in milliseconds.
     * \param ms Maximum length in milliseconds
     */
    void setMaxLengthMs(int ms);

    /**
     * \brief Deletes a range of measures from the file.
     * \param from Starting measure number
     * \param to Ending measure number
     */
    void deleteMeasures(int from, int to);

    /**
     * \brief Inserts empty measures into the file.
     * \param after Measure number to insert after
     * \param numMeasures Number of measures to insert
     */
    void insertMeasures(int after, int numMeasures);

    // === Protocol System Integration ===

    /**
     * \brief Creates a copy of this file for the protocol system.
     * \return A new ProtocolEntry representing this file's state
     */
    ProtocolEntry *copy();

    /**
     * \brief Reloads the file's state from a protocol entry.
     * \param entry The protocol entry to restore state from
     */
    void reloadState(ProtocolEntry *entry);

    /**
     * \brief Gets this file instance (for ProtocolEntry interface).
     * \return Pointer to this MidiFile
     */
    MidiFile *file();

    /**
     * \brief Gets a track by its number.
     * \param number The track number to retrieve
     * \return Pointer to the MidiTrack, or nullptr if not found
     */
    MidiTrack *track(int number);

    /**
     * \brief Gets the tonality (key signature) at a specific tick.
     * \param tick The tick position to query
     * \return The tonality value (positive for sharps, negative for flats)
     */
    int tonalityAt(int tick);

    /**
     * \brief Gets the time signature at a specific tick.
     * \param tick The tick position to query
     * \param num Pointer to receive the numerator
     * \param denum Pointer to receive the denominator
     * \param lastTimeSigEvent Optional pointer to receive the time signature event
     */
    void meterAt(int tick, int *num, int *denum, TimeSignatureEvent **lastTimeSigEvent = 0);

    // === Static Utility Methods ===

    /**
     * \brief Reads a variable-length value from a MIDI data stream.
     * \param content The data stream to read from
     * \return The decoded variable-length value
     */
    static int variableLengthvalue(QDataStream *content);

    /**
     * \brief Encodes a value as a variable-length MIDI value.
     * \param value The value to encode
     * \return QByteArray containing the encoded variable-length value
     */
    static QByteArray writeVariableLengthValue(int value);

    /** \brief Default ticks per quarter note for new files */
    static int defaultTimePerQuarter;

    // === Copy/Paste Support ===

    /**
     * \brief Registers a track copy operation for paste functionality.
     * \param source The source track that was copied
     * \param destination The destination track for pasting
     * \param fileFrom The source file
     */
    void registerCopiedTrack(MidiTrack *source, MidiTrack *destination, MidiFile *fileFrom);

    /**
     * \brief Gets the appropriate track for pasting copied content.
     * \param source The source track that was copied
     * \param fileFrom The source file
     * \return Pointer to the track to paste into
     */
    MidiTrack *getPasteTrack(MidiTrack *source, MidiFile *fileFrom);

    // === Quantization and Timing ===

    /**
     * \brief Gets quantization tick values for a given fraction size.
     * \param fractionSize The fraction size for quantization
     * \return List of tick values for quantization grid
     */
    QList<int> quantization(int fractionSize);

    /**
     * \brief Gets the starting tick of a specific measure.
     * \param measure The measure number
     * \return The tick position where the measure starts
     */
    int startTickOfMeasure(int measure);

signals:
    /**
     * \brief Emitted when the cursor position changes.
     */
    void cursorPositionChanged();

    /**
     * \brief Emitted when widgets need to recalculate their size.
     */
    void recalcWidgetSize();

    /**
     * \brief Emitted when track information changes.
     */
    void trackChanged();

private:
    // === File Reading Methods ===

    /**
     * \brief Reads a complete MIDI file from a data stream.
     * \param content The data stream containing MIDI data
     * \param log Optional string list to receive loading messages
     * \return True if reading was successful
     */
    bool readMidiFile(QDataStream *content, QStringList *log);

    /**
     * \brief Reads a single MIDI track from a data stream.
     * \param content The data stream containing track data
     * \param num The track number being read
     * \param log Optional string list to receive loading messages
     * \return True if reading was successful
     */
    bool readTrack(QDataStream *content, int num, QStringList *log);

    /**
     * \brief Reads a delta time value from a data stream.
     * \param content The data stream to read from
     * \return The delta time value
     */
    int deltaTime(QDataStream *content);

    /**
     * \brief Prints log messages to debug output.
     * \param log The string list containing log messages
     */
    void printLog(QStringList *log);

    // === Private Member Variables ===

    /** \brief Ticks per quarter note resolution */
    int timePerQuarter;

    /** \brief Array of MIDI channels (0-15 standard, 16-18 special) */
    MidiChannel *channels[19];

    /** \brief File path and basic properties */
    QString _path;
    int midiTicks, maxTimeMS, _cursorTick, _pauseTick, _midiFormat;

    /** \brief Protocol system for undo/redo */
    Protocol *prot;

    /** \brief Player data and state */
    QMultiMap<int, MidiEvent *> *playerMap;
    bool _saved;

    /** \brief Track management */
    QList<MidiTrack *> *_tracks;
    QMap<MidiFile *, QMap<MidiTrack *, MidiTrack *> > pasteTracks;
};

#endif // MIDIFILE_H_
