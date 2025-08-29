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

#include "SharedClipboard.h"
#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/TempoChangeEvent.h"
#include "../MidiEvent/TimeSignatureEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../MidiEvent/OnEvent.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiTrack.h"

#include <QDataStream>
#include <QDateTime>
#include <QIODevice>

// Static member definitions
SharedClipboard *SharedClipboard::_instance = nullptr;
const QString SharedClipboard::SHARED_MEMORY_KEY = "MidiEditor_Clipboard_v1";
const QString SharedClipboard::SEMAPHORE_KEY = "MidiEditor_Clipboard_Semaphore_v1";
const int SharedClipboard::CLIPBOARD_VERSION = 1;
const int SharedClipboard::MAX_CLIPBOARD_SIZE = 1024 * 1024; // 1MB max

SharedClipboard::SharedClipboard(QObject *parent)
    : QObject(parent)
      , _sharedMemory(nullptr)
      , _semaphore(nullptr)
      , _initialized(false) {
}

SharedClipboard::~SharedClipboard() {
    cleanup();
}

SharedClipboard *SharedClipboard::instance() {
    if (!_instance) {
        _instance = new SharedClipboard();
    }
    return _instance;
}

bool SharedClipboard::initialize() {
    if (_initialized) {
        return true;
    }

    // Create semaphore for synchronization
    _semaphore = new QSystemSemaphore(SEMAPHORE_KEY, 1, QSystemSemaphore::Create);
    if (_semaphore->error() != QSystemSemaphore::NoError) {
        delete _semaphore;
        _semaphore = nullptr;
        return false;
    }

    // Create shared memory
    _sharedMemory = new QSharedMemory(SHARED_MEMORY_KEY);

    // Try to attach to existing shared memory first
    bool attached = _sharedMemory->attach();

    if (!attached) {
        // If attach fails, try to create new shared memory
        if (!_sharedMemory->create(MAX_CLIPBOARD_SIZE)) {
            delete _sharedMemory;
            _sharedMemory = nullptr;
            delete _semaphore;
            _semaphore = nullptr;
            return false;
        }

        // Initialize the shared memory with empty data
        if (lockMemory()) {
            ClipboardHeader *header = static_cast<ClipboardHeader *>(_sharedMemory->data());
            header->version = CLIPBOARD_VERSION;
            header->eventCount = 0;
            header->dataSize = 0;
            header->timestamp = 0;
            header->sourceProcessId = 0; // No data yet
            header->hasTempoEvents = 0;
            unlockMemory();
        }
    } else {
        // Check if existing data is valid
        if (lockMemory()) {
            ClipboardHeader *header = static_cast<ClipboardHeader *>(_sharedMemory->data());
            unlockMemory();
        }
    }

    _initialized = true;
    return true;
}

bool SharedClipboard::copyEvents(const QList<MidiEvent *> &events, MidiFile *sourceFile) {
    if (!_initialized || !_sharedMemory || events.isEmpty() || !sourceFile) {
        return false;
    }

    QByteArray serializedData = serializeEvents(events, sourceFile);
    if (serializedData.isEmpty()) {
        return false;
    }

    if (!lockMemory()) {
        return false;
    }

    // Check if data fits in shared memory
    int headerSize = sizeof(ClipboardHeader);
    int totalSize = headerSize + serializedData.size();
    int availableSize = _sharedMemory->size();

    if (totalSize > availableSize) {
        unlockMemory();
        return false;
    }

    // Check if events contain tempo/time signature events
    bool hasTempoEvents = false;
    for (MidiEvent *event : events) {
        if (dynamic_cast<TempoChangeEvent *>(event) || dynamic_cast<TimeSignatureEvent *>(event)) {
            hasTempoEvents = true;
            break;
        }
    }

    // Write header
    ClipboardHeader *header = static_cast<ClipboardHeader *>(_sharedMemory->data());
    header->version = CLIPBOARD_VERSION;
    header->ticksPerQuarter = sourceFile->ticksPerQuarter();
    header->tempoBeatsPerQuarter = getCurrentTempo(sourceFile);
    header->eventCount = events.size();
    header->dataSize = serializedData.size();
    header->timestamp = QDateTime::currentMSecsSinceEpoch();
    header->sourceProcessId = QCoreApplication::applicationPid();
    header->hasTempoEvents = hasTempoEvents ? 1 : 0;

    // Write serialized event data
    char *dataPtr = static_cast<char *>(_sharedMemory->data()) + sizeof(ClipboardHeader);
    memcpy(dataPtr, serializedData.constData(), serializedData.size());

    unlockMemory();
    return true;
}

bool SharedClipboard::pasteEvents(MidiFile *targetFile, QList<MidiEvent *> &pastedEvents, bool applyTempoConversion, int targetCursorTick) {
    if (!_initialized || !_sharedMemory || !targetFile) {
        return false;
    }

    if (!lockMemory()) {
        return false;
    }

    ClipboardHeader *header = static_cast<ClipboardHeader *>(_sharedMemory->data());

    // Check version compatibility
    if (header->version != CLIPBOARD_VERSION || header->eventCount == 0) {
        unlockMemory();
        return false;
    }

    // Store header info for tempo conversion
    int sourceTicksPerQuarter = header->ticksPerQuarter;
    int sourceTempo = header->tempoBeatsPerQuarter;
    bool hasTempoEvents = (header->hasTempoEvents == 1);

    // Read serialized data - copy it to ensure data integrity
    char *dataPtr = static_cast<char *>(_sharedMemory->data()) + sizeof(ClipboardHeader);
    QByteArray serializedData;
    serializedData.resize(header->dataSize);
    memcpy(serializedData.data(), dataPtr, header->dataSize);

    unlockMemory();

    // Deserialize events
    bool result = deserializeEvents(serializedData, targetFile, pastedEvents);
    
    // Apply tempo conversion if requested and needed
    if (result && applyTempoConversion && !hasTempoEvents) {
        int targetTicksPerQuarter = targetFile->ticksPerQuarter();
        
        // Check if any conversion is needed by comparing basic parameters
        int cursorTempo = getCurrentTempo(targetFile, targetCursorTick);
        if (sourceTempo != cursorTempo || sourceTicksPerQuarter != targetTicksPerQuarter) {
            for (int i = 0; i < pastedEvents.size(); i++) {
                QPair<int, int> originalTiming = getOriginalTiming(i);
                if (originalTiming.first != -1) {
                    // Calculate where this note will be placed in the target file
                    int firstTick = -1;
                    for (int j = 0; j < pastedEvents.size(); j++) {
                        QPair<int, int> timing = getOriginalTiming(j);
                        if (timing.first != -1 && (timing.first < firstTick || firstTick == -1)) {
                            firstTick = timing.first;
                        }
                    }
                    if (firstTick == -1) firstTick = 0;
                    
                    int diff = targetCursorTick - firstTick;
                    int targetNotePosition = originalTiming.first + diff;
                    
                    // Get the tempo that will be in effect at this note's position
                    int targetTempo = getCurrentTempo(targetFile, targetNotePosition);
                    
                    int convertedTime = convertTiming(originalTiming.first, 
                                                    sourceTicksPerQuarter, sourceTempo,
                                                    targetTicksPerQuarter, targetTempo);
                    // Update the stored timing for this event
                    g_originalTimings[i] = QPair<int, int>(convertedTime, originalTiming.second);
                }
            }
        }
    }
    
    return result;
}

bool SharedClipboard::hasData() {
    if (!_initialized || !_sharedMemory) {
        return false;
    }

    if (!lockMemory()) {
        return false;
    }

    ClipboardHeader *header = static_cast<ClipboardHeader *>(_sharedMemory->data());
    bool hasValidData = (header->version == CLIPBOARD_VERSION && header->eventCount > 0);

    unlockMemory();
    return hasValidData;
}

bool SharedClipboard::hasDataFromDifferentProcess() {
    if (!_initialized || !_sharedMemory) {
        return false;
    }

    if (!lockMemory()) {
        return false;
    }

    ClipboardHeader *header = static_cast<ClipboardHeader *>(_sharedMemory->data());
    qint64 currentPid = QCoreApplication::applicationPid();

    // Check all conditions for valid cross-process data
    bool validVersion = (header->version == CLIPBOARD_VERSION);
    bool hasEvents = (header->eventCount > 0);
    bool differentProcess = (header->sourceProcessId != 0 && header->sourceProcessId != currentPid);
    bool hasValidData = validVersion && hasEvents && differentProcess;

    unlockMemory();
    return hasValidData;
}

void SharedClipboard::clear() {
    if (!_initialized || !_sharedMemory) {
        return;
    }

    if (lockMemory()) {
        ClipboardHeader *header = static_cast<ClipboardHeader *>(_sharedMemory->data());
        header->eventCount = 0;
        header->dataSize = 0;
        header->timestamp = 0;
        header->hasTempoEvents = 0;
        unlockMemory();
    }
}

void SharedClipboard::cleanup() {
    if (_sharedMemory) {
        _sharedMemory->detach();
        delete _sharedMemory;
        _sharedMemory = nullptr;
    }

    if (_semaphore) {
        delete _semaphore;
        _semaphore = nullptr;
    }

    _initialized = false;
}

QByteArray SharedClipboard::serializeEvents(const QList<MidiEvent *> &events, MidiFile *sourceFile) {
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    // Write each event with its timing information
    for (MidiEvent *event: events) {
        if (!event) continue;

        // Skip OffEvents as they will be handled by their OnEvents
        OffEvent *offEvent = dynamic_cast<OffEvent *>(event);
        if (offEvent) continue;

        // Write event timing
        int midiTime = event->midiTime();
        int channel = event->channel();
        stream << midiTime;
        stream << channel;

        // Write event data
        QByteArray eventData = event->save();
        int dataSize = eventData.size();
        stream << dataSize;
        stream.writeRawData(eventData.constData(), eventData.size());

        // If this is an OnEvent, also serialize its corresponding OffEvent
        OnEvent *onEvent = dynamic_cast<OnEvent *>(event);
        if (onEvent && onEvent->offEvent()) {
            OffEvent *off = onEvent->offEvent();
            int offMidiTime = off->midiTime();
            int offChannel = off->channel();
            stream << offMidiTime;
            stream << offChannel;
            QByteArray offData = off->save();
            int offDataSize = offData.size();
            stream << offDataSize;
            stream.writeRawData(offData.constData(), offData.size());
        }
    }

    return data;
}

// Global storage for timing information during deserialization
static QList<QPair<int, int> > g_originalTimings; // midiTime, channel pairs

bool SharedClipboard::deserializeEvents(const QByteArray &data, MidiFile *targetFile, QList<MidiEvent *> &events) {
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    events.clear();
    g_originalTimings.clear(); // Clear previous timing data

    int eventIndex = 0;
    while (!stream.atEnd()) {
        int midiTime, channel, dataSize;

        // Check if we have enough data for the header
        int available = stream.device()->bytesAvailable();
        if (available < 12) { // 3 ints = 12 bytes
            break;
        }

        // Read the three integers
        stream >> midiTime;
        stream >> channel;
        stream >> dataSize;

        if (dataSize <= 0 || dataSize > 1024) { // Sanity check
            return false;
        }

        QByteArray eventData(dataSize, 0);
        int bytesRead = stream.readRawData(eventData.data(), dataSize);
        if (bytesRead != dataSize) {
            return false;
        }

        // Create a temporary data stream to parse the event
        QDataStream eventStream(eventData);
        eventStream.setByteOrder(QDataStream::BigEndian);

        bool ok = false;
        bool endEvent = false;
        // Use track 0 as default, will be reassigned during paste
        MidiTrack *defaultTrack = targetFile->track(0);
        if (!defaultTrack) {
            return false;
        }

        MidiEvent *event = MidiEvent::loadMidiEvent(&eventStream, &ok, &endEvent, defaultTrack);

        if (ok && event && !endEvent) {
            // Store the original timing information in global storage
            g_originalTimings.append(QPair<int, int>(midiTime, channel));

            // Don't try to set properties on deserialized events - they can crash
            // Just add the event as-is
            events.append(event);
        } else if (event) {
            try {
                delete event; // Clean up failed event
            } catch (...) {
            }
        }

        eventIndex++;
    }

    return !events.isEmpty();
}

QPair<int, int> SharedClipboard::getOriginalTiming(int index) {
    if (index >= 0 && index < g_originalTimings.size()) {
        return g_originalTimings[index];
    }
    return QPair<int, int>(-1, -1); // Invalid
}

int SharedClipboard::getCurrentTempo(MidiFile *file, int atTick) {
    if (!file) return 120; // Default tempo

    QMap<int, MidiEvent *> *tempoEvents = file->tempoEvents();
    if (!tempoEvents || tempoEvents->isEmpty()) {
        return 120; // Default tempo
    }

    // Find the most recent tempo event at or before the given tick
    TempoChangeEvent *currentTempo = nullptr;
    for (auto it = tempoEvents->begin(); it != tempoEvents->end(); ++it) {
        if (it.key() <= atTick) {
            TempoChangeEvent *tempo = dynamic_cast<TempoChangeEvent *>(it.value());
            if (tempo) {
                currentTempo = tempo;
            }
        } else {
            break;
        }
    }

    return currentTempo ? currentTempo->beatsPerQuarter() : 120;
}

int SharedClipboard::convertTiming(int originalTime, int sourceTicksPerQuarter, int sourceTempo, 
                                 int targetTicksPerQuarter, int targetTempo) {
    if (sourceTempo == targetTempo && sourceTicksPerQuarter == targetTicksPerQuarter) {
        return originalTime; // No conversion needed
    }
    
    // Convert to real time (milliseconds) using source timing
    double sourceTickDurationMs = (60000.0 / sourceTempo) / sourceTicksPerQuarter;
    double realTimeMs = originalTime * sourceTickDurationMs;
    
    // Convert back to ticks using target timing
    double targetTickDurationMs = (60000.0 / targetTempo) / targetTicksPerQuarter;
    int targetTime = (int)(realTimeMs / targetTickDurationMs + 0.5); // Round to nearest tick
    
    return targetTime;
}

bool SharedClipboard::lockMemory() {
    if (!_semaphore) {
        return false;
    }

    bool acquired = _semaphore->acquire();
    return acquired;
}

void SharedClipboard::unlockMemory() {
    if (_semaphore) {
        bool released = _semaphore->release();
    }
}
