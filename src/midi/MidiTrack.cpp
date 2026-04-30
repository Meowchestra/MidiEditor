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

#include "MidiTrack.h"

#include "../gui/Appearance.h"
#include "../MidiEvent/TextEvent.h"
#include "../MidiEvent/ProgChangeEvent.h"
#include "MidiChannel.h"
#include "MidiFile.h"

MidiTrack::MidiTrack(MidiFile *file)
    : ProtocolEntry() {
    _number = 0;
    _nameEvent = 0;
    _file = file;
    _hidden = false;
    _muted = false;
    _assignedChannel = -1;
}

MidiTrack::MidiTrack(MidiTrack &other)
    : QObject()
      , ProtocolEntry(other) {
    _number = other._number;
    _nameEvent = other._nameEvent;
    _file = other._file;
    _hidden = other._hidden;
    _muted = other._muted;
    _assignedChannel = other._assignedChannel;
}

MidiTrack::~MidiTrack() {
}

MidiFile *MidiTrack::file() {
    return _file;
}

QString MidiTrack::name() {
    if (_nameEvent) {
        return _nameEvent->text();
    }
    return tr("Untitled track");
}

void MidiTrack::setName(QString name) {
    if (!_nameEvent) {
        _nameEvent = new TextEvent(16, this);
        _nameEvent->setType(TextEvent::TRACKNAME);
        _file->channel(16)->insertEvent(_nameEvent, 0);
    }

    _nameEvent->setText(name);
    emit trackChanged();
}

int MidiTrack::number() {
    return _number;
}

void MidiTrack::setNumber(int number) {
    ProtocolEntry *toCopy = copy();
    _number = number;
    protocol(toCopy, this);
}

void MidiTrack::setNameEvent(TextEvent *nameEvent) {
    if ((_nameEvent) && (_nameEvent->type() == TextEvent::TRACKNAME)) {
        _nameEvent->setType(TextEvent::TEXT);
    }
    ProtocolEntry *toCopy = copy();
    _nameEvent = nameEvent;
    if (_nameEvent) {
        _nameEvent->setType(TextEvent::TRACKNAME);
    }
    protocol(toCopy, this);
    emit trackChanged();
}

TextEvent *MidiTrack::nameEvent() {
    return _nameEvent;
}

ProtocolEntry *MidiTrack::copy() {
    return new MidiTrack(*this);
}

void MidiTrack::reloadState(ProtocolEntry *entry) {
    MidiTrack *other = dynamic_cast<MidiTrack *>(entry);
    if (!other) {
        return;
    }
    if (_number != other->number()) {
        setNumber(other->_number);
    }
    _nameEvent = other->_nameEvent;
    _file = other->_file;
    _hidden = other->_hidden;
    _muted = other->_muted;
    _assignedChannel = other->_assignedChannel;
}

void MidiTrack::setHidden(bool hidden) {
    ProtocolEntry *toCopy = copy();
    _hidden = hidden;
    protocol(toCopy, this);
    emit trackChanged();
}

bool MidiTrack::hidden() {
    return _hidden;
}

void MidiTrack::setMuted(bool muted) {
    ProtocolEntry *toCopy = copy();
    _muted = muted;
    protocol(toCopy, this);
    emit trackChanged();
}

bool MidiTrack::muted() {
    return _muted;
}

QColor *MidiTrack::color() {
    return Appearance::trackColor(number());
}

MidiTrack *MidiTrack::copyToFile(MidiFile *file) {
    file->addTrack();
    MidiTrack *newTrack = file->tracks()->last();
    newTrack->setName(this->name());

    file->registerCopiedTrack(this, newTrack, this->file());

    return newTrack;
}

void MidiTrack::assignChannel(int ch) {
    ProtocolEntry *toCopy = copy();
    _assignedChannel = ch;
    protocol(toCopy, this);
}

int MidiTrack::assignedChannel() {
    return _assignedChannel;
}

int MidiTrack::progAtTick(int tick) {
    if (!_file) return 0;

    // Scan all 16 MIDI channels for ProgChangeEvents belonging to this track
    int bestProg = -1;
    int bestTick = -1;

    for (int ch = 0; ch < 16; ch++) {
        MidiChannel *channel = _file->channel(ch);
        if (!channel) continue;

        QMultiMap<int, MidiEvent *> *events = channel->eventMap();
        if (!events || events->isEmpty()) continue;

        // Walk backwards from the target tick to find the latest ProgChangeEvent on this track
        QMultiMap<int, MidiEvent *>::iterator it = events->upperBound(tick);
        while (it != events->begin()) {
            --it;
            if (it.key() > tick) continue;
            ProgChangeEvent *ev = dynamic_cast<ProgChangeEvent *>(it.value());
            if (ev && ev->track() == this && it.key() > bestTick) {
                bestProg = ev->program();
                bestTick = it.key();
                break; // found the latest one in this channel at or before tick
            }
        }
    }

    if (bestProg >= 0) return bestProg;

    // Fallback: find the very first ProgChangeEvent on this track anywhere
    for (int ch = 0; ch < 16; ch++) {
        MidiChannel *channel = _file->channel(ch);
        if (!channel) continue;

        QMultiMap<int, MidiEvent *> *events = channel->eventMap();
        if (!events || events->isEmpty()) continue;

        for (auto it = events->begin(); it != events->end(); ++it) {
            ProgChangeEvent *ev = dynamic_cast<ProgChangeEvent *>(it.value());
            if (ev && ev->track() == this) {
                return ev->program();
            }
        }
    }

    return 0;
}
