#include "MmlMidiWriter.h"
#include "../../midi/MidiTrack.h"
#include "../../MidiEvent/MidiEvent.h"
#include "../../MidiEvent/NoteOnEvent.h"
#include "../../MidiEvent/OffEvent.h"
#include "../../MidiEvent/ControlChangeEvent.h"
#include "../../MidiEvent/ProgChangeEvent.h"
#include "../../MidiEvent/TempoChangeEvent.h"

#include <algorithm>
#include <map>

namespace MML {

MidiFile* MmlMidiWriter::BuildMidiFile(const std::vector<MmlEvent>& events, int ppq) {
    MidiFile* midiFile = new MidiFile();
    
    // Create tempo track 0
    MidiTrack* tempoTrack = midiFile->track(0);

    std::map<int, std::vector<MmlEvent>> trackGroup;
    long long maxTick = 0;

    for (const auto& e : events) {
        if (e.Tick > maxTick) maxTick = e.Tick;

        if (e.Type == MidiEventType::Tempo) {
            TempoChangeEvent* tempoEv = new TempoChangeEvent(17, e.Tempo, tempoTrack);
            tempoEv->setFile(midiFile);
            tempoEv->setMidiTime(e.Tick, false);
        } else {
            trackGroup[e.Channel].push_back(e);
        }
    }

    OffEvent::clearOnEvents(); // prevents dangling GUI ties

    for (const auto& pair : trackGroup) {
        int ch = pair.first;
        // Channel 0 maps to Track 1, Channel 1 to Track 2...
        int trackIdx = ch + 1;
        while (midiFile->numTracks() <= trackIdx) {
            midiFile->addTrack();
        }
        MidiTrack* track = midiFile->track(trackIdx);

        for (const auto& e : pair.second) {
            switch (e.Type) {
                case MidiEventType::NoteOn: {
                    NoteOnEvent* noteOn = new NoteOnEvent(e.Param1, e.Param2, ch, track);
                    noteOn->setFile(midiFile);
                    noteOn->setMidiTime(e.Tick, false);
                    break;
                }
                case MidiEventType::NoteOff: {
                    int line = 127 - e.Param1; // MidiEditor's GUI coordinate system 
                    OffEvent* noteOff = new OffEvent(ch, line, track);
                    noteOff->setFile(midiFile);
                    noteOff->setMidiTime(e.Tick, false);
                    break;
                }
                case MidiEventType::ProgramChange: {
                    ProgChangeEvent* pc = new ProgChangeEvent(ch, e.Param1, track);
                    pc->setFile(midiFile);
                    pc->setMidiTime(e.Tick, false);
                    break;
                }
                case MidiEventType::Controller: {
                    ControlChangeEvent* cc = new ControlChangeEvent(ch, e.Param1, e.Param2, track);
                    cc->setFile(midiFile);
                    cc->setMidiTime(e.Tick, false);
                    break;
                }
                default:
                    break;
            }
        }
    }

    OffEvent::clearOnEvents();

    if (maxTick > 0) {
        midiFile->setMidiTicks(maxTick);
    }

    return midiFile;
}

} // namespace MML
