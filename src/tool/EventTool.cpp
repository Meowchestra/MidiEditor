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

#include "EventTool.h"

#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../MidiEvent/OnEvent.h"
#include "../MidiEvent/TempoChangeEvent.h"
#include "../MidiEvent/TimeSignatureEvent.h"
#include "../gui/EventWidget.h"
#include "../gui/MainWindow.h"
#include "../gui/MatrixWidget.h"
#include "../gui/Appearance.h"
#include "../gui/ChannelVisibilityManager.h"
#include "../midi/MidiChannel.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiPlayer.h"
#include "../midi/MidiTrack.h"
#include "../protocol/Protocol.h"
#include "NewNoteTool.h"
#include "Selection.h"
#include "SharedClipboard.h"

#include <set>
#include <vector>
#include <cmath>

QList<MidiEvent *> *EventTool::copiedEvents = new QList<MidiEvent *>;

int EventTool::_pasteChannel = -1;
int EventTool::_pasteTrack = -2;

bool EventTool::_magnet = false;

EventTool::EventTool()
    : EditorTool() {
}

EventTool::EventTool(EventTool &other)
    : EditorTool(other) {
}

void EventTool::selectEvent(MidiEvent *event, bool single, bool ignoreStr, bool setSelection) {
    if (!ChannelVisibilityManager::instance().isChannelVisible(event->channel())) {
        return;
    }

    if (event->track()->hidden()) {
        return;
    }

    QList<MidiEvent *> &selected = Selection::instance()->selectedEvents();

    OffEvent *offevent = dynamic_cast<OffEvent *>(event);
    if (offevent) {
        return;
    }

    if (single && !QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier) && (!QApplication::keyboardModifiers().testFlag(Qt::ControlModifier) || ignoreStr)) {
        selected.clear();
        NoteOnEvent *on = dynamic_cast<NoteOnEvent *>(event);
        if (on) {
            MidiPlayer::play(on);
        }
    }
    if (!selected.contains(event) && (!QApplication::keyboardModifiers().testFlag(Qt::ControlModifier) || ignoreStr)) {
        selected.append(event);
    } else if (QApplication::keyboardModifiers().testFlag(Qt::ControlModifier) && !ignoreStr) {
        selected.removeAll(event);
    }

    if (setSelection) {
        Selection::instance()->setSelection(selected);
    }
    _mainWindow->eventWidget()->reportSelectionChangedByTool();
}

void EventTool::deselectEvent(MidiEvent *event) {
    QList<MidiEvent *> &selected = Selection::instance()->selectedEvents();
    selected.removeAll(event);

    if (_mainWindow->eventWidget()->events().contains(event)) {
        _mainWindow->eventWidget()->removeEvent(event);
    }
}

void EventTool::clearSelection() {
    Selection::instance()->clearSelection();
    _mainWindow->eventWidget()->reportSelectionChangedByTool();
}

void EventTool::batchSelectEvents(const QList<MidiEvent *> &events) {
    if (events.isEmpty()) {
        return;
    }

    // Clear existing selection
    QList<MidiEvent *> &selected = Selection::instance()->selectedEvents();
    selected.clear();

    // Reserve space for better performance with large selections
    selected.reserve(events.size());

    // Add all valid events to selection in batch
    // Note: Events should already be pre-filtered for channel/track visibility
    foreach(MidiEvent* event, events) {
        // Double-check visibility as a safety measure using the global visibility manager
        if (!ChannelVisibilityManager::instance().isChannelVisible(event->channel())) {
            continue;
        }

        if (event->track()->hidden()) {
            continue;
        }

        // Skip OffEvents
        OffEvent *offevent = dynamic_cast<OffEvent *>(event);
        if (offevent) {
            continue;
        }

        selected.append(event);
    }

    // Update selection state once at the end
    Selection::instance()->setSelection(selected);
    _mainWindow->eventWidget()->reportSelectionChangedByTool();
}

void EventTool::paintSelectedEvents(QPainter *painter) {
    foreach(MidiEvent* event, Selection::instance()->selectedEvents()) {
        bool show = event->shown();

        if (!show) {
            OnEvent *ev = dynamic_cast<OnEvent *>(event);
            if (ev) {
                show = ev->offEvent() && ev->offEvent()->shown();
            }
        }

        if (event->track()->hidden()) {
            show = false;
        }
        if (!ChannelVisibilityManager::instance().isChannelVisible(event->channel())) {
            show = false;
        }

        if (show) {
            painter->setBrush(Appearance::noteSelectionColor());
            painter->setPen(Appearance::selectionBorderColor());
            painter->drawRoundedRect(event->x(), event->y(), event->width(),
                                     event->height(), 1, 1);
        }
    }
}

void EventTool::changeTick(MidiEvent *event, int shiftX) {
    // TODO: falls event gezeigt ist, über matrixWidget tick erfragen (effizienter)
    //int newMs = matrixWidget->msOfXPos(event->x()-shiftX);

    int newMs = file()->msOfTick(event->midiTime()) - matrixWidget->timeMsOfWidth(shiftX);
    int tick = file()->tick(newMs);

    if (tick < 0) {
        tick = 0;
    }

    // with magnet: set to div value if pixel refers to this tick
    if (magnetEnabled()) {
        int newX = matrixWidget->xPosOfMs(newMs);
        typedef QPair<int, int> TMPPair;
        foreach(TMPPair p, matrixWidget->divs()) {
            int xt = p.first;
            if (newX == xt) {
                tick = p.second;
                break;
            }
        }
    }
    event->setMidiTime(tick);
}

void EventTool::copyAction() {
    if (Selection::instance()->selectedEvents().size() > 0) {
        // clear old copied Events
        copiedEvents->clear();

        foreach(MidiEvent* event, Selection::instance()->selectedEvents()) {
            // add the current Event
            MidiEvent *ev = dynamic_cast<MidiEvent *>(event->copy());
            if (ev) {
                // do not append off event here
                OffEvent *off = dynamic_cast<OffEvent *>(ev);
                if (!off) {
                    copiedEvents->append(ev);
                }
            }

            // if its onEvent, add a copy of the OffEvent
            OnEvent *onEv = dynamic_cast<OnEvent *>(ev);
            if (onEv) {
                OffEvent *offEv = dynamic_cast<OffEvent *>(onEv->offEvent()->copy());
                if (offEv) {
                    offEv->setOnEvent(onEv);
                    copiedEvents->append(offEv);
                }
            }
        }

        // Also copy to shared clipboard for cross-instance pasting
        copyToSharedClipboard();

        _mainWindow->copiedEventsChanged();
    }
}

void EventTool::pasteAction() {
    // Check if shared clipboard has newer data from a different process
    bool hasSharedData = hasSharedClipboardData();
    bool hasLocalData = (copiedEvents->size() > 0);

    if (hasSharedData) {
        // Always prefer shared clipboard data (cross-instance) when available
        if (pasteFromSharedClipboard()) {
            return;
        }
    }

    if (hasLocalData) {
        // Continue with local clipboard paste logic below
    } else {
        return;
    }

    // TODO what happens to TempoEvents??

    // copy copied events to insert unique events
    QList<MidiEvent *> copiedCopiedEvents;
    foreach(MidiEvent* event, *copiedEvents) {
        // add the current Event
        MidiEvent *ev = dynamic_cast<MidiEvent *>(event->copy());
        if (ev) {
            // do not append off event here
            OffEvent *off = dynamic_cast<OffEvent *>(ev);
            if (!off) {
                copiedCopiedEvents.append(ev);
            }
        }

        // if its onEvent, add a copy of the OffEvent
        OnEvent *onEv = dynamic_cast<OnEvent *>(ev);
        if (onEv) {
            OffEvent *offEv = dynamic_cast<OffEvent *>(onEv->offEvent()->copy());
            if (offEv) {
                offEv->setOnEvent(onEv);
                copiedCopiedEvents.append(offEv);
            }
        }
    }

    if (copiedCopiedEvents.count() > 0) {
        // Begin a new ProtocolAction
        currentFile()->protocol()->startNewAction(QObject::tr("Paste ") + QString::number(copiedCopiedEvents.count()) + QObject::tr(" events"));

        double tickscale = 1;
        if (currentFile() != copiedEvents->first()->file()) {
            tickscale = ((double) (currentFile()->ticksPerQuarter())) / ((double) copiedEvents->first()->file()->ticksPerQuarter());
        }

        // get first Tick of the copied events
        int firstTick = -1;
        foreach(MidiEvent* event, copiedCopiedEvents) {
            if ((int) (tickscale * event->midiTime()) < firstTick || firstTick < 0) {
                firstTick = (int) (tickscale * event->midiTime());
            }
        }

        if (firstTick < 0)
            firstTick = 0;

        // calculate the difference of old/new events in MidiTicks
        int diff = currentFile()->cursorTick() - firstTick;

        // set the Positions and add the Events to the channels
        clearSelection();

        std::sort(copiedCopiedEvents.begin(), copiedCopiedEvents.end(), [](MidiEvent *a, MidiEvent *b) {return a->midiTime() < b->midiTime();});

        std::vector<std::pair<ProtocolEntry *, ProtocolEntry *> > channelCopies;
        std::set<int> copiedChannels;

        // Determine which channels are associated with the pasted events and copy them
        for (auto event: copiedCopiedEvents) {
            // get channel
            int channelNum = event->channel();
            if (_pasteChannel == -2) {
                channelNum = NewNoteTool::editChannel();
            }
            if ((_pasteChannel >= 0) && (channelNum < 16)) {
                channelNum = _pasteChannel;
            }

            if (copiedChannels.find(channelNum) == copiedChannels.end()) {
                MidiChannel *channel = currentFile()->channel(channelNum);
                ProtocolEntry *channelCopy = channel->copy();
                channelCopies.push_back(std::make_pair(channelCopy, channel));
                copiedChannels.insert(channelNum);
            }
        }

        for (auto it = copiedCopiedEvents.rbegin(); it != copiedCopiedEvents.rend(); it++) {
            MidiEvent *event = *it;

            // get channel
            int channelNum = event->channel();
            if (_pasteChannel == -2) {
                channelNum = NewNoteTool::editChannel();
            }
            if ((_pasteChannel >= 0) && (channelNum < 16)) {
                channelNum = _pasteChannel;
            }

            // get track
            MidiTrack *track = event->track();
            if (pasteTrack() == -2) {
                track = currentFile()->track(NewNoteTool::editTrack());
            } else if ((pasteTrack() >= 0) && (pasteTrack() < currentFile()->tracks()->size())) {
                track = currentFile()->track(pasteTrack());
            } else if (event->file() != currentFile() || !currentFile()->tracks()->contains(track)) {
                track = currentFile()->getPasteTrack(event->track(), event->file());
                if (!track) {
                    track = event->track()->copyToFile(currentFile());
                }
            }

            if ((!track) || (track->file() != currentFile())) {
                track = currentFile()->track(0);
            }

            event->setFile(currentFile());
            event->setChannel(channelNum, false);
            event->setTrack(track, false);
            currentFile()->channel(channelNum)->insertEvent(event,
                                                            (int) (tickscale * event->midiTime()) + diff, false);
            selectEvent(event, false, true, false);
        }
        Selection::instance()->setSelection(Selection::instance()->selectedEvents());

        // Put the copied channels from before the event insertion onto the protocol stack
        for (auto channelPair: channelCopies) {
            ProtocolEntry *channel = channelPair.first;
            channel->protocol(channel, channelPair.second);
        }

        currentFile()->protocol()->endAction();
    }
}

bool EventTool::showsSelection() {
    return false;
}

void EventTool::setPasteTrack(int track) {
    _pasteTrack = track;
}

int EventTool::pasteTrack() {
    return _pasteTrack;
}

void EventTool::setPasteChannel(int channel) {
    _pasteChannel = channel;
}

int EventTool::pasteChannel() {
    return _pasteChannel;
}

int EventTool::rasteredX(int x, int *tick) {
    if (!_magnet) {
        if (tick) {
            *tick = _currentFile->tick(matrixWidget->msOfXPos(x));
        }
        return x;
    }
    typedef QPair<int, int> TMPPair;
    foreach(TMPPair p, matrixWidget->divs()) {
        int xt = p.first;
        if (std::abs(xt - x) <= 5) {
            if (tick) {
                *tick = p.second;
            }
            return xt;
        }
    }
    if (tick) {
        *tick = _currentFile->tick(matrixWidget->msOfXPos(x));
    }
    return x;
}

void EventTool::enableMagnet(bool enable) {
    _magnet = enable;
}

bool EventTool::magnetEnabled() {
    return _magnet;
}

bool EventTool::copyToSharedClipboard() {
    SharedClipboard *clipboard = SharedClipboard::instance();
    if (!clipboard->initialize()) {
        return false;
    }

    // Use the same events that were copied to local clipboard
    if (copiedEvents->isEmpty()) {
        return false;
    }

    // Get the source file from the first event
    MidiFile *sourceFile = copiedEvents->first()->file();
    if (!sourceFile) {
        return false;
    }

    bool result = clipboard->copyEvents(*copiedEvents, sourceFile);
    return result;
}

bool EventTool::pasteFromSharedClipboard() {
    SharedClipboard *clipboard = SharedClipboard::instance();
    if (!clipboard->initialize()) {
        return false;
    }

    // Only paste from shared clipboard if data is from a different process
    if (!clipboard->hasDataFromDifferentProcess()) {
        return false;
    }

    QList<MidiEvent *> sharedEvents;
    if (!clipboard->pasteEvents(currentFile(), sharedEvents, true, currentFile()->cursorTick())) {
        return false;
    }

    if (sharedEvents.isEmpty()) {
        return false;
    }

    // Now actually paste these events using the same logic as regular paste
    if (sharedEvents.count() > 0) {
        // Begin a new ProtocolAction
        currentFile()->protocol()->startNewAction(QObject::tr("Paste ") + QString::number(sharedEvents.count()) + QObject::tr(" events from shared clipboard"));

        // Get first tick using the original timing information (not deserialized timing)
        int firstTick = -1;
        for (int i = 0; i < sharedEvents.size(); i++) {
            QPair<int, int> originalTiming = SharedClipboard::getOriginalTiming(i);
            int originalTime = originalTiming.first;

            if (originalTime != -1) {
                if (originalTime < firstTick || firstTick < 0) {
                    firstTick = originalTime;
                }
            }
        }

        if (firstTick < 0)
            firstTick = 0;

        // Calculate the difference to paste at cursor position
        int diff = currentFile()->cursorTick() - firstTick;

        // Get current editing context (where to paste)
        int targetChannel = NewNoteTool::editChannel();
        MidiTrack *targetTrack = currentFile()->track(NewNoteTool::editTrack());
        if (!targetTrack) {
            targetTrack = currentFile()->track(0);
        }

        if (!targetTrack) {
            // Clean up events
            for (MidiEvent *event: sharedEvents) {
                if (event) delete event;
            }
            return false;
        }

        if (!currentFile()) {
            // Clean up events
            for (MidiEvent *event: sharedEvents) {
                if (event) delete event;
            }
            return false;
        }

        // Clear selection and paste events
        clearSelection();

        // Separate tempo/time signature events from regular events
        QList<MidiEvent *> tempoEvents;
        QList<MidiEvent *> regularEvents;
        
        for (MidiEvent *event : sharedEvents) {
            if (dynamic_cast<TempoChangeEvent *>(event) || dynamic_cast<TimeSignatureEvent *>(event)) {
                tempoEvents.append(event);
            } else {
                regularEvents.append(event);
            }
        }

        // First, paste tempo/time signature events and integrate them into the file
        int tempoEventIndex = 0;
        for (MidiEvent *event : tempoEvents) {
            if (!event) {
                tempoEventIndex++;
                continue;
            }

            try {
                // Get the original timing information
                QPair<int, int> originalTiming = SharedClipboard::getOriginalTiming(tempoEventIndex);
                int originalTime = originalTiming.first;

                if (originalTime == -1) {
                    originalTime = event->midiTime();
                }

                // Calculate new timing
                int newTime = originalTime + diff;
                if (newTime < 0) newTime = 0;

                // Set event properties
                event->setFile(currentFile());
                event->setChannel(0, false); // Meta events typically use channel 0
                event->setTrack(targetTrack, false);

                // Insert tempo events into channel 17, time signature events into channel 18
                TempoChangeEvent *tempoEvent = dynamic_cast<TempoChangeEvent *>(event);
                TimeSignatureEvent *timeSigEvent = dynamic_cast<TimeSignatureEvent *>(event);
                
                if (tempoEvent) {
                    currentFile()->channel(17)->insertEvent(event, newTime, false);
                } else if (timeSigEvent) {
                    currentFile()->channel(18)->insertEvent(event, newTime, false);
                } else {
                    // Fallback for other meta events
                    currentFile()->channel(0)->insertEvent(event, newTime, false);
                }

                selectEvent(event, false, true, false);
            } catch (...) {
                delete event;
            }

            tempoEventIndex++;
        }

        // Then paste regular events
        int regularEventIndex = tempoEvents.size(); // Offset by tempo events
        for (MidiEvent *event : regularEvents) {
            if (!event) {
                regularEventIndex++;
                continue;
            }

            try {
                // Get the original timing information from SharedClipboard
                QPair<int, int> originalTiming = SharedClipboard::getOriginalTiming(regularEventIndex);
                int originalTime = originalTiming.first;

                if (originalTime == -1) {
                    // Fallback to event's current timing if no stored timing
                    originalTime = event->midiTime();
                }

                // Calculate new timing (preserve relative timing, paste at cursor)
                int newTime = originalTime + diff;

                // Set the event properties safely (timing is already restored in deserializeEvents)
                event->setFile(currentFile());

                event->setChannel(targetChannel, false);

                event->setTrack(targetTrack, false);

                // Insert into the target channel at the calculated time
                currentFile()->channel(targetChannel)->insertEvent(event, newTime, false);

                selectEvent(event, false, true, false);
            } catch (...) {
                delete event;
            }

            regularEventIndex++;
        }

        // Update the selection to show the pasted events
        Selection::instance()->setSelection(Selection::instance()->selectedEvents());

        currentFile()->protocol()->endAction();
    }

    // Note: sharedEvents are now owned by the file/channels
    return true;
}

bool EventTool::hasSharedClipboardData() {
    SharedClipboard *clipboard = SharedClipboard::instance();
    if (!clipboard->initialize()) {
        return false;
    }
    // Only return true if data is from a different process
    return clipboard->hasDataFromDifferentProcess();
}
