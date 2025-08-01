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

#ifndef SHAREDCLIPBOARD_H_
#define SHAREDCLIPBOARD_H_

// Qt includes
#include <QSharedMemory>
#include <QSystemSemaphore>
#include <QList>
#include <QByteArray>

// Forward declarations
class MidiEvent;
class MidiFile;

/**
 * \class SharedClipboard
 *
 * \brief Manages inter-process clipboard functionality for MidiEditor.
 *
 * This class handles sharing copied MIDI events between multiple instances
 * of MidiEditor using Qt's QSharedMemory. It preserves tempo information
 * and handles proper serialization/deserialization of MIDI events.
 *
 * Key features:
 * - **Inter-process communication**: Share clipboard data between editor instances
 * - **Event serialization**: Proper preservation of MIDI event data
 * - **Tempo preservation**: Maintains timing information across instances
 * - **Thread safety**: Uses semaphores for safe concurrent access
 * - **Process detection**: Identifies data from different processes
 * - **Automatic cleanup**: Manages shared memory lifecycle
 */
class SharedClipboard : public QObject {
    Q_OBJECT

public:
    /**
     * \brief Gets the singleton instance of SharedClipboard.
     * \return Pointer to the global SharedClipboard instance
     */
    static SharedClipboard *instance();

    /**
     * \brief Initializes the shared clipboard system.
     * \return True if initialization was successful
     */
    bool initialize();

    /**
     * @brief Copy events to shared clipboard
     * @param events List of MIDI events to copy
     * @param sourceFile Source MIDI file (for tempo and timing info)
     * @return true if copy was successful
     */
    bool copyEvents(const QList<MidiEvent *> &events, MidiFile *sourceFile);

    /**
     * @brief Paste events from shared clipboard
     * @param targetFile Target MIDI file
     * @param pastedEvents Output list of pasted events
     * @return true if paste was successful
     */
    bool pasteEvents(MidiFile *targetFile, QList<MidiEvent *> &pastedEvents);

    /**
     * @brief Check if shared clipboard has data
     * @return true if clipboard contains data
     */
    bool hasData();

    /**
     * @brief Check if shared clipboard has data from a different process
     * @return true if clipboard contains data from another process
     */
    bool hasDataFromDifferentProcess();

    /**
     * @brief Clear the shared clipboard
     */
    void clear();

    /**
     * @brief Cleanup shared memory resources
     */
    void cleanup();

    /**
     * @brief Get original timing for deserialized events
     * @param index Event index
     * @return QPair of (midiTime, channel) or (-1, -1) if invalid
     */
    static QPair<int, int> getOriginalTiming(int index);

private:
    explicit SharedClipboard(QObject *parent = nullptr);

    ~SharedClipboard();

    // Prevent copying
    SharedClipboard(const SharedClipboard &) = delete;

    SharedClipboard &operator=(const SharedClipboard &) = delete;

    /**
     * @brief Serialize events to byte array
     */
    QByteArray serializeEvents(const QList<MidiEvent *> &events, MidiFile *sourceFile);

    /**
     * @brief Deserialize events from byte array
     */
    bool deserializeEvents(const QByteArray &data, MidiFile *targetFile, QList<MidiEvent *> &events);

    /**
     * @brief Get current tempo information from file
     */
    int getCurrentTempo(MidiFile *file, int atTick = 0);

    /**
     * @brief Lock shared memory for exclusive access
     */
    bool lockMemory();

    /**
     * @brief Unlock shared memory
     */
    void unlockMemory();

    // === Static Members ===

    /** \brief Singleton instance */
    static SharedClipboard *_instance;

    /** \brief Shared memory key identifier */
    static const QString SHARED_MEMORY_KEY;

    /** \brief Semaphore key identifier */
    static const QString SEMAPHORE_KEY;

    /** \brief Clipboard data format version */
    static const int CLIPBOARD_VERSION;

    /** \brief Maximum clipboard data size in bytes */
    static const int MAX_CLIPBOARD_SIZE;

    // === Instance Members ===

    /** \brief Shared memory segment */
    QSharedMemory *_sharedMemory;

    /** \brief Semaphore for synchronization */
    QSystemSemaphore *_semaphore;

    /** \brief Initialization state flag */
    bool _initialized;

    // === Clipboard Data Structure ===

    /**
     * \brief Header structure for clipboard data.
     */
    struct ClipboardHeader {
        int version;                ///< Data format version
        int ticksPerQuarter;        ///< Source file timing resolution
        int tempoBeatsPerQuarter;   ///< Source file tempo information
        int eventCount;             ///< Number of events in clipboard
        int dataSize;               ///< Size of serialized event data
        qint64 timestamp;           ///< Timestamp for detecting stale data
        qint64 sourceProcessId;     ///< Process ID that wrote the data
    };
};

#endif // SHAREDCLIPBOARD_H_
