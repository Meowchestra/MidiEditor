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

#include "DeleteOverlapsTool.h"

#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../gui/HybridMatrixWidget.h"
#include "../midi/MidiChannel.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiTrack.h"
#include "../protocol/Protocol.h"
#include "Selection.h"
#include "StandardTool.h"

#include <QMap>
#include <algorithm>

DeleteOverlapsTool::DeleteOverlapsTool()
    : EventTool() {
    setImage(":/run_environment/graphics/tool/deleteoverlap.png");
    setToolTipText("Delete overlapping notes");
}

DeleteOverlapsTool::DeleteOverlapsTool(DeleteOverlapsTool &other)
    : EventTool(other) {
}

void DeleteOverlapsTool::draw(QPainter *painter) {
    paintSelectedEvents(painter);
}

bool DeleteOverlapsTool::press(bool leftClick) {
    Q_UNUSED(leftClick);
    return true;
}

bool DeleteOverlapsTool::release() {
    if (!file()) {
        return false;
    }

    // Default to mono mode when used as a tool
    performDeleteOverlapsOperation(MONO_MODE);

    // Return to standard tool if set
    if (_standardTool) {
        Tool::setCurrentTool(_standardTool);
        _standardTool->move(mouseX, mouseY);
        _standardTool->release();
    }

    return true;
}

ProtocolEntry *DeleteOverlapsTool::copy() {
    return new DeleteOverlapsTool(*this);
}

void DeleteOverlapsTool::reloadState(ProtocolEntry *entry) {
    EventTool::reloadState(entry);
    DeleteOverlapsTool *other = dynamic_cast<DeleteOverlapsTool *>(entry);
    if (!other) {
        return;
    }
}

bool DeleteOverlapsTool::showsSelection() {
    return true;
}

void DeleteOverlapsTool::performDeleteOverlapsOperation(OverlapMode mode, bool respectChannels, bool respectTracks) {
    // Only work on selected events
    QList<MidiEvent *> eventsToProcess = Selection::instance()->selectedEvents();

    if (eventsToProcess.isEmpty()) {
        return; // Nothing to process
    }

    // Filter to only NoteOnEvents
    QList<NoteOnEvent *> notes;
    for (MidiEvent *event: eventsToProcess) {
        NoteOnEvent *noteOn = dynamic_cast<NoteOnEvent *>(event);
        if (noteOn) {
            notes.append(noteOn);
        }
    }

    if (notes.isEmpty()) {
        return;
    }

    // Start protocol action
    QString actionName;
    switch (mode) {
        case MONO_MODE:
            actionName = QObject::tr("Delete overlaps (mono)");
            break;
        case POLY_MODE:
            actionName = QObject::tr("Delete overlaps (poly)");
            break;
        case DOUBLES_MODE:
            actionName = QObject::tr("Delete doubles");
            break;
    }

    currentProtocol()->startNewAction(actionName, image());

    // Perform the appropriate operation
    switch (mode) {
        case MONO_MODE:
            deleteOverlapsMono(notes, respectChannels, respectTracks);
            break;
        case POLY_MODE:
            deleteOverlapsPoly(notes, respectTracks);
            break;
        case DOUBLES_MODE:
            deleteDoubles(notes, respectChannels, respectTracks);
            break;
    }

    currentProtocol()->endAction();

    // Clear selection since some notes may have been deleted or modified
    Selection::instance()->clearSelection();
}

void DeleteOverlapsTool::deleteOverlapsMono(const QList<NoteOnEvent *> &notes, bool respectChannels, bool respectTracks) {
    // Group notes by pitch, optionally by track and channel
    QMap<QString, QList<NoteOnEvent *> > noteGroups;

    for (NoteOnEvent *note: notes) {
        QString key = QString("pitch_%1").arg(note->note());

        // Optionally include track in grouping
        if (respectTracks) {
            key += QString("_track_%1").arg((quintptr) note->track());
        }

        // Optionally include channel in grouping
        if (respectChannels) {
            key += QString("_channel_%1").arg(note->channel());
        }

        noteGroups[key].append(note);
    }

    // Process each group
    for (auto it = noteGroups.begin(); it != noteGroups.end(); ++it) {
        QList<NoteOnEvent *> groupNotes = it.value();

        if (groupNotes.size() < 2) continue;

        // Sort notes by start time
        std::sort(groupNotes.begin(), groupNotes.end(), [](NoteOnEvent *a, NoteOnEvent *b) {
            return a->midiTime() < b->midiTime();
        });

        // Handle overlaps: prioritize longer notes, remove shorter overlapping ones
        QList<NoteOnEvent *> notesToRemove;

        for (int i = 0; i < groupNotes.size(); i++) {
            if (notesToRemove.contains(groupNotes[i])) continue;

            NoteOnEvent *currentNote = groupNotes[i];
            int currentStart = currentNote->midiTime();
            int currentEnd = currentNote->offEvent()->midiTime();
            int currentLength = currentEnd - currentStart;

            for (int j = i + 1; j < groupNotes.size(); j++) {
                if (notesToRemove.contains(groupNotes[j])) continue;

                NoteOnEvent *laterNote = groupNotes[j];
                int laterStart = laterNote->midiTime();
                int laterEnd = laterNote->offEvent()->midiTime();
                int laterLength = laterEnd - laterStart;

                // Check if notes overlap
                if (notesOverlap(currentNote, laterNote)) {
                    // Decide which note to keep based on length and position
                    if (laterStart >= currentStart && laterEnd <= currentEnd) {
                        // Later note is completely inside current note - remove later note
                        notesToRemove.append(laterNote);
                    } else if (currentStart >= laterStart && currentEnd <= laterEnd) {
                        // Current note is completely inside later note - remove current note
                        notesToRemove.append(currentNote);
                        break; // Current note is removed, no need to check further
                    } else if (currentLength >= laterLength) {
                        // Keep longer note (current), shorten or remove later note
                        if (laterStart < currentEnd) {
                            // Shorten the later note to start after current note ends
                            int newStart = currentEnd;
                            if (newStart < laterEnd - 1) {
                                laterNote->setMidiTime(newStart);
                            } else {
                                // Later note would be too short, remove it
                                notesToRemove.append(laterNote);
                            }
                        }
                    } else {
                        // Keep longer note (later), shorten current note
                        if (currentEnd > laterStart) {
                            // Shorten current note to end before later note starts
                            int newEnd = qMax(laterStart - 1, currentStart + 1);
                            currentNote->offEvent()->setMidiTime(newEnd);
                            currentEnd = newEnd;
                        }
                    }
                }
            }
        }

        // Remove the notes marked for deletion
        for (NoteOnEvent *note: notesToRemove) {
            removeNote(note);
        }
    }
}

void DeleteOverlapsTool::deleteOverlapsPoly(const QList<NoteOnEvent *> &notes, bool respectTracks) {
    if (respectTracks) {
        // Group notes by track and process each track separately
        QMap<MidiTrack *, QList<NoteOnEvent *> > trackGroups;

        for (NoteOnEvent *note: notes) {
            trackGroups[note->track()].append(note);
        }

        // Process each track separately
        for (auto it = trackGroups.begin(); it != trackGroups.end(); ++it) {
            processPolyOverlaps(it.value());
        }
    } else {
        // Process all notes together regardless of track
        processPolyOverlaps(notes);
    }
}

void DeleteOverlapsTool::deleteDoubles(const QList<NoteOnEvent *> &notes, bool respectChannels, bool respectTracks) {
    QList<NoteOnEvent *> notesToRemove;

    // Compare each note with every other note to find exact duplicates
    for (int i = 0; i < notes.size(); i++) {
        for (int j = i + 1; j < notes.size(); j++) {
            if (notesAreDuplicates(notes[i], notes[j], respectChannels, respectTracks)) {
                // Remove the later note (j)
                if (!notesToRemove.contains(notes[j])) {
                    notesToRemove.append(notes[j]);
                }
            }
        }
    }

    // Remove the duplicate notes
    for (NoteOnEvent *note: notesToRemove) {
        removeNote(note);
    }
}

void DeleteOverlapsTool::processPolyOverlaps(const QList<NoteOnEvent *> &notes) {
    // Sort all notes by start time regardless of pitch
    QList<NoteOnEvent *> sortedNotes = notes;
    std::sort(sortedNotes.begin(), sortedNotes.end(), [](NoteOnEvent *a, NoteOnEvent *b) {
        return a->midiTime() < b->midiTime();
    });

    // Process notes to make them monophonic
    for (int i = 0; i < sortedNotes.size() - 1; i++) {
        NoteOnEvent *currentNote = sortedNotes[i];
        int currentEndTime = currentNote->offEvent()->midiTime();

        for (int j = i + 1; j < sortedNotes.size(); j++) {
            NoteOnEvent *laterNote = sortedNotes[j];
            int laterStartTime = laterNote->midiTime();

            // If current note overlaps with any later note, shorten current note
            if (currentEndTime > laterStartTime) {
                // Shorten the current note to end just before the later note starts
                // But ensure minimum note length of 1 tick
                int newEndTime = qMax(laterStartTime - 1, currentNote->midiTime() + 1);
                currentNote->offEvent()->setMidiTime(newEndTime);
                currentEndTime = newEndTime;
                break; // Once shortened, no need to check further
            }
        }
    }
}

bool DeleteOverlapsTool::notesOverlap(NoteOnEvent *note1, NoteOnEvent *note2) {
    if (!note1 || !note2 || !note1->offEvent() || !note2->offEvent()) {
        return false;
    }

    int note1Start = note1->midiTime();
    int note1End = note1->offEvent()->midiTime();
    int note2Start = note2->midiTime();
    int note2End = note2->offEvent()->midiTime();

    // Notes overlap if one starts before the other ends
    return (note1Start < note2End && note2Start < note1End);
}

bool DeleteOverlapsTool::notesAreDuplicates(NoteOnEvent *note1, NoteOnEvent *note2, bool respectChannels, bool respectTracks) {
    if (!note1 || !note2 || !note1->offEvent() || !note2->offEvent()) {
        return false;
    }

    // Notes are duplicates if they have the same:
    // - pitch (note number)
    // - start time
    // - end time
    bool basicMatch = (note1->note() == note2->note() &&
                       note1->midiTime() == note2->midiTime() &&
                       note1->offEvent()->midiTime() == note2->offEvent()->midiTime());

    if (!basicMatch) {
        return false;
    }

    // If respectTracks is true, also check track
    if (respectTracks && note1->track() != note2->track()) {
        return false;
    }

    // If respectChannels is true, also check channel
    if (respectChannels && note1->channel() != note2->channel()) {
        return false;
    }

    return true;
}

void DeleteOverlapsTool::removeNote(NoteOnEvent *note) {
    if (!note || !note->offEvent()) {
        return;
    }

    // Remove from selection if selected
    deselectEvent(note);

    // Remove the note and its off event from the channel
    MidiChannel *channel = file()->channel(note->channel());
    if (channel) {
        channel->removeEvent(note);
        channel->removeEvent(note->offEvent());
    }
}
