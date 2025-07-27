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

#ifndef SHAREDCLIPBOARD_H
#define SHAREDCLIPBOARD_H

#include <QObject>
#include <QSharedMemory>
#include <QSystemSemaphore>
#include <QList>
#include <QByteArray>

class MidiEvent;
class MidiFile;

/**
 * @brief Manages inter-process clipboard functionality for MidiEditor
 * 
 * This class handles sharing copied MIDI events between multiple instances
 * of MidiEditor using Qt's QSharedMemory. It preserves tempo information
 * and handles proper serialization/deserialization of MIDI events.
 */
class SharedClipboard : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance of SharedClipboard
     */
    static SharedClipboard* instance();

    /**
     * @brief Initialize the shared clipboard system
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Copy events to shared clipboard
     * @param events List of MIDI events to copy
     * @param sourceFile Source MIDI file (for tempo and timing info)
     * @return true if copy was successful
     */
    bool copyEvents(const QList<MidiEvent*>& events, MidiFile* sourceFile);

    /**
     * @brief Paste events from shared clipboard
     * @param targetFile Target MIDI file
     * @param pastedEvents Output list of pasted events
     * @return true if paste was successful
     */
    bool pasteEvents(MidiFile* targetFile, QList<MidiEvent*>& pastedEvents);

    /**
     * @brief Check if shared clipboard has data
     * @return true if clipboard contains data
     */
    bool hasData();

    /**
     * @brief Clear the shared clipboard
     */
    void clear();

    /**
     * @brief Cleanup shared memory resources
     */
    void cleanup();

private:
    explicit SharedClipboard(QObject* parent = nullptr);
    ~SharedClipboard();

    // Prevent copying
    SharedClipboard(const SharedClipboard&) = delete;
    SharedClipboard& operator=(const SharedClipboard&) = delete;

    /**
     * @brief Serialize events to byte array
     */
    QByteArray serializeEvents(const QList<MidiEvent*>& events, MidiFile* sourceFile);

    /**
     * @brief Deserialize events from byte array
     */
    bool deserializeEvents(const QByteArray& data, MidiFile* targetFile, QList<MidiEvent*>& events);

    /**
     * @brief Get current tempo information from file
     */
    int getCurrentTempo(MidiFile* file, int atTick = 0);

    /**
     * @brief Lock shared memory for exclusive access
     */
    bool lockMemory();

    /**
     * @brief Unlock shared memory
     */
    void unlockMemory();

    static SharedClipboard* _instance;
    static const QString SHARED_MEMORY_KEY;
    static const QString SEMAPHORE_KEY;
    static const int CLIPBOARD_VERSION;
    static const int MAX_CLIPBOARD_SIZE;

    QSharedMemory* _sharedMemory;
    QSystemSemaphore* _semaphore;
    bool _initialized;

    // Clipboard data structure
    struct ClipboardHeader {
        int version;
        int ticksPerQuarter;
        int tempoBeatsPerQuarter;
        int eventCount;
        int dataSize;
        qint64 timestamp;  // For detecting stale data
    };
};

#endif // SHAREDCLIPBOARD_H
