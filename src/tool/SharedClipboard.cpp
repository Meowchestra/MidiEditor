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
#include "../MidiEvent/OffEvent.h"
#include "../MidiEvent/OnEvent.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiTrack.h"

#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QIODevice>

// Static member definitions
SharedClipboard* SharedClipboard::_instance = nullptr;
const QString SharedClipboard::SHARED_MEMORY_KEY = "MidiEditor_Clipboard_v1";
const QString SharedClipboard::SEMAPHORE_KEY = "MidiEditor_Clipboard_Semaphore_v1";
const int SharedClipboard::CLIPBOARD_VERSION = 1;
const int SharedClipboard::MAX_CLIPBOARD_SIZE = 1024 * 1024; // 1MB max

SharedClipboard::SharedClipboard(QObject* parent)
    : QObject(parent)
    , _sharedMemory(nullptr)
    , _semaphore(nullptr)
    , _initialized(false)
{
}

SharedClipboard::~SharedClipboard()
{
    cleanup();
}

SharedClipboard* SharedClipboard::instance()
{
    if (!_instance) {
        _instance = new SharedClipboard();
    }
    return _instance;
}

bool SharedClipboard::initialize()
{
    if (_initialized) {
        return true;
    }

    // Create semaphore for synchronization
    _semaphore = new QSystemSemaphore(SEMAPHORE_KEY, 1, QSystemSemaphore::Create);
    if (_semaphore->error() != QSystemSemaphore::NoError) {
        qWarning() << "SharedClipboard: Failed to create semaphore:" << _semaphore->errorString();
        delete _semaphore;
        _semaphore = nullptr;
        return false;
    }

    // Create shared memory
    _sharedMemory = new QSharedMemory(SHARED_MEMORY_KEY);
    
    // Try to attach to existing shared memory first
    if (!_sharedMemory->attach()) {
        // If attach fails, try to create new shared memory
        if (!_sharedMemory->create(MAX_CLIPBOARD_SIZE)) {
            qWarning() << "SharedClipboard: Failed to create shared memory:" << _sharedMemory->errorString();
            delete _sharedMemory;
            _sharedMemory = nullptr;
            delete _semaphore;
            _semaphore = nullptr;
            return false;
        }
        
        // Initialize the shared memory with empty data
        if (lockMemory()) {
            ClipboardHeader* header = static_cast<ClipboardHeader*>(_sharedMemory->data());
            header->version = CLIPBOARD_VERSION;
            header->eventCount = 0;
            header->dataSize = 0;
            header->timestamp = 0;
            unlockMemory();
        }
    }

    _initialized = true;
    return true;
}

bool SharedClipboard::copyEvents(const QList<MidiEvent*>& events, MidiFile* sourceFile)
{
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
    int totalSize = sizeof(ClipboardHeader) + serializedData.size();
    if (totalSize > _sharedMemory->size()) {
        qWarning() << "SharedClipboard: Data too large for shared memory";
        unlockMemory();
        return false;
    }

    // Write header
    ClipboardHeader* header = static_cast<ClipboardHeader*>(_sharedMemory->data());
    header->version = CLIPBOARD_VERSION;
    header->ticksPerQuarter = sourceFile->ticksPerQuarter();
    header->tempoBeatsPerQuarter = getCurrentTempo(sourceFile);
    header->eventCount = events.size();
    header->dataSize = serializedData.size();
    header->timestamp = QDateTime::currentMSecsSinceEpoch();

    // Write serialized event data
    char* dataPtr = static_cast<char*>(_sharedMemory->data()) + sizeof(ClipboardHeader);
    memcpy(dataPtr, serializedData.constData(), serializedData.size());

    unlockMemory();
    return true;
}

bool SharedClipboard::pasteEvents(MidiFile* targetFile, QList<MidiEvent*>& pastedEvents)
{
    if (!_initialized || !_sharedMemory || !targetFile) {
        return false;
    }

    if (!lockMemory()) {
        return false;
    }

    ClipboardHeader* header = static_cast<ClipboardHeader*>(_sharedMemory->data());
    
    // Check version compatibility
    if (header->version != CLIPBOARD_VERSION || header->eventCount == 0) {
        unlockMemory();
        return false;
    }

    // Read serialized data
    char* dataPtr = static_cast<char*>(_sharedMemory->data()) + sizeof(ClipboardHeader);
    QByteArray serializedData(dataPtr, header->dataSize);

    unlockMemory();

    // Deserialize events
    return deserializeEvents(serializedData, targetFile, pastedEvents);
}

bool SharedClipboard::hasData()
{
    if (!_initialized || !_sharedMemory) {
        return false;
    }

    if (!lockMemory()) {
        return false;
    }

    ClipboardHeader* header = static_cast<ClipboardHeader*>(_sharedMemory->data());
    bool hasValidData = (header->version == CLIPBOARD_VERSION && header->eventCount > 0);

    unlockMemory();
    return hasValidData;
}

void SharedClipboard::clear()
{
    if (!_initialized || !_sharedMemory) {
        return;
    }

    if (lockMemory()) {
        ClipboardHeader* header = static_cast<ClipboardHeader*>(_sharedMemory->data());
        header->eventCount = 0;
        header->dataSize = 0;
        header->timestamp = 0;
        unlockMemory();
    }
}

void SharedClipboard::cleanup()
{
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

QByteArray SharedClipboard::serializeEvents(const QList<MidiEvent*>& events, MidiFile* sourceFile)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    // Write each event with its timing information
    for (MidiEvent* event : events) {
        if (!event) continue;

        // Skip OffEvents as they will be handled by their OnEvents
        OffEvent* offEvent = dynamic_cast<OffEvent*>(event);
        if (offEvent) continue;

        // Write event timing
        stream << event->midiTime();
        stream << event->channel();

        // Write event data
        QByteArray eventData = event->save();
        stream << eventData.size();
        stream.writeRawData(eventData.constData(), eventData.size());

        // If this is an OnEvent, also serialize its corresponding OffEvent
        OnEvent* onEvent = dynamic_cast<OnEvent*>(event);
        if (onEvent && onEvent->offEvent()) {
            OffEvent* off = onEvent->offEvent();
            stream << off->midiTime();
            stream << off->channel();
            QByteArray offData = off->save();
            stream << offData.size();
            stream.writeRawData(offData.constData(), offData.size());
        }
    }

    return data;
}

bool SharedClipboard::deserializeEvents(const QByteArray& data, MidiFile* targetFile, QList<MidiEvent*>& events)
{
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    events.clear();

    while (!stream.atEnd()) {
        int midiTime, channel, dataSize;
        stream >> midiTime >> channel >> dataSize;

        if (dataSize <= 0 || dataSize > 1024) { // Sanity check
            qWarning() << "SharedClipboard: Invalid event data size:" << dataSize;
            return false;
        }

        QByteArray eventData(dataSize, 0);
        if (stream.readRawData(eventData.data(), dataSize) != dataSize) {
            qWarning() << "SharedClipboard: Failed to read event data";
            return false;
        }

        // Create a temporary data stream to parse the event
        QDataStream eventStream(eventData);
        eventStream.setByteOrder(QDataStream::BigEndian);

        bool ok = false;
        bool endEvent = false;
        // Use track 0 as default, will be reassigned during paste
        MidiTrack* defaultTrack = targetFile->track(0);
        if (!defaultTrack) {
            qWarning() << "SharedClipboard: No tracks available in target file";
            return false;
        }

        MidiEvent* event = MidiEvent::loadMidiEvent(&eventStream, &ok, &endEvent, defaultTrack);

        if (ok && event && !endEvent) {
            event->setMidiTime(midiTime, false);
            event->setChannel(channel, false);
            event->setFile(targetFile);
            events.append(event);
        } else if (event) {
            delete event; // Clean up failed event
        }
    }

    return !events.isEmpty();
}

int SharedClipboard::getCurrentTempo(MidiFile* file, int atTick)
{
    if (!file) return 120; // Default tempo

    QMap<int, MidiEvent*>* tempoEvents = file->tempoEvents();
    if (!tempoEvents || tempoEvents->isEmpty()) {
        return 120; // Default tempo
    }

    // Find the most recent tempo event at or before the given tick
    TempoChangeEvent* currentTempo = nullptr;
    for (auto it = tempoEvents->begin(); it != tempoEvents->end(); ++it) {
        if (it.key() <= atTick) {
            TempoChangeEvent* tempo = dynamic_cast<TempoChangeEvent*>(it.value());
            if (tempo) {
                currentTempo = tempo;
            }
        } else {
            break;
        }
    }

    return currentTempo ? currentTempo->beatsPerQuarter() : 120;
}

bool SharedClipboard::lockMemory()
{
    if (!_semaphore) return false;
    return _semaphore->acquire();
}

void SharedClipboard::unlockMemory()
{
    if (_semaphore) {
        _semaphore->release();
    }
}
