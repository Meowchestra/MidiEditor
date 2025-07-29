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

#include "ScissorsTool.h"

#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../gui/HybridMatrixWidget.h"
#include "../gui/Appearance.h"
#include "../midi/MidiChannel.h"
#include "../midi/MidiFile.h"
#include "../midi/MidiTrack.h"
#include "../protocol/Protocol.h"
#include "Selection.h"
#include "StandardTool.h"

#include <QPainter>
#include <QList>

ScissorsTool::ScissorsTool()
    : EventTool(), _splitTick(0)
{
    setImage(":/run_environment/graphics/tool/scissors.png");
    setToolTipText("Split notes");
}

ScissorsTool::ScissorsTool(ScissorsTool& other)
    : EventTool(other), _splitTick(other._splitTick)
{
}

void ScissorsTool::draw(QPainter* painter)
{
    if (!matrixWidget || !file()) {
        return;
    }

    // Draw red vertical line at cursor position (like Cubase scissors)
    painter->setPen(QPen(Appearance::playbackCursorColor(), 2));

    // Calculate the tick position from mouse X coordinate
    int ms = matrixWidget->msOfXPos(mouseX);
    _splitTick = file()->tick(ms);

    // Draw the vertical line from the timeline area down to the bottom
    // The timeline area starts at y=0 and has a height of about 50 pixels
    // We want to draw from the bottom of the timeline to the bottom of the widget
    int timelineHeight = 50; // This is the typical timeline height
    painter->drawLine(mouseX, timelineHeight, mouseX, matrixWidget->height());

    painter->setPen(Appearance::foregroundColor());
}

bool ScissorsTool::press(bool leftClick)
{
    Q_UNUSED(leftClick);
    return true;
}

bool ScissorsTool::release()
{
    if (!file()) {
        return false;
    }

    performSplitOperation();

    // Return to standard tool if set
    if (_standardTool) {
        Tool::setCurrentTool(_standardTool);
        _standardTool->move(mouseX, mouseY);
        _standardTool->release();
    }

    return true;
}

ProtocolEntry* ScissorsTool::copy()
{
    return new ScissorsTool(*this);
}

void ScissorsTool::reloadState(ProtocolEntry* entry)
{
    EventTool::reloadState(entry);
    ScissorsTool* other = dynamic_cast<ScissorsTool*>(entry);
    if (!other) {
        return;
    }
    _splitTick = other->_splitTick;
}

bool ScissorsTool::showsSelection()
{
    return false; // Scissors tool doesn't need to show selection
}

void ScissorsTool::performSplitOperation()
{
    // Calculate the split position from mouse coordinates
    int ms = matrixWidget->msOfXPos(mouseX);
    int splitTick = file()->tick(ms);

    // Find all notes that need to be split
    QList<NoteOnEvent*> notesToSplit = findNotesToSplit(splitTick);

    if (notesToSplit.isEmpty()) {
        return; // Nothing to split
    }

    // Start protocol action
    currentProtocol()->startNewAction(QObject::tr("Split notes"), image());

    // Split each note
    for (NoteOnEvent* note : notesToSplit) {
        splitNote(note, splitTick);
    }

    currentProtocol()->endAction();
}

QList<NoteOnEvent*> ScissorsTool::findNotesToSplit(int splitTick)
{
    QList<NoteOnEvent*> notesToSplit;

    // Search through all channels and tracks (like Cubase scissors)
    for (int ch = 0; ch < 16; ch++) {
        MidiChannel* channel = file()->channel(ch);
        if (!channel->visible()) continue;

        QMultiMap<int, MidiEvent*>* eventMap = channel->eventMap();
        for (auto it = eventMap->begin(); it != eventMap->end(); ++it) {
            MidiEvent* event = it.value();
            if (event->track()->hidden()) continue;

            NoteOnEvent* noteOn = dynamic_cast<NoteOnEvent*>(event);
            if (!noteOn) continue;

            // Check if this note spans across the split position
            if (noteSpansAcrossTick(noteOn, splitTick)) {
                notesToSplit.append(noteOn);
            }
        }
    }

    return notesToSplit;
}

void ScissorsTool::splitNote(NoteOnEvent* originalNote, int splitTick)
{
    if (!originalNote || !originalNote->offEvent()) {
        return;
    }

    int originalStartTick = originalNote->midiTime();
    int originalEndTick = originalNote->offEvent()->midiTime();

    // Don't split if the split position is at the very beginning or end
    if (splitTick <= originalStartTick || splitTick >= originalEndTick) {
        return;
    }

    // Get note properties
    int note = originalNote->note();
    int velocity = originalNote->velocity();
    int channel = originalNote->channel();
    MidiTrack* track = originalNote->track();

    // Create the second note (from split position to original end)
    MidiChannel* midiChannel = file()->channel(channel);
    NoteOnEvent* secondNote = midiChannel->insertNote(note, splitTick, originalEndTick, velocity, track);

    // Modify the original note to end at the split position
    originalNote->offEvent()->setMidiTime(splitTick);

    // The new note is automatically added to the channel by insertNote()
    // No need to manually add it to any selection since scissors tool doesn't use selection
}

bool ScissorsTool::noteSpansAcrossTick(NoteOnEvent* note, int tick)
{
    if (!note || !note->offEvent()) {
        return false;
    }

    int noteStart = note->midiTime();
    int noteEnd = note->offEvent()->midiTime();

    // Note spans across the tick if the tick is between start and end (exclusive)
    return (tick > noteStart && tick < noteEnd);
}
