#include "GpToNative.h"
#include <cmath>
#include <algorithm>
#include <map>
#include <numeric>

// ============================================================
// BendingPlan — ported from Native/BendingPlan.cs
// ============================================================

NativeBendingPlan NativeBendingPlan::create(std::vector<NativeBendPoint> bendPoints,
    int originalChannel, int usedChannel,
    int duration, int index, float resize, bool isVibrato)
{
    int maxDistance = duration / 10;
    if (isVibrato) {
        maxDistance = std::min(maxDistance, 60);
    }

    if (bendPoints.empty()) {
        // Create Vibrato Plan
        bendPoints.push_back(NativeBendPoint(index, 0.0f, usedChannel));
        bendPoints.push_back(NativeBendPoint(index + duration, 0.0f, usedChannel));
    }

    std::vector<NativeBendPoint> result;

    // Resize the points according to (changed) note duration
    for (auto& bp : bendPoints) {
        bp.index = static_cast<int>(index + (bp.index - index) * resize);
        bp.usedChannel = usedChannel;
    }

    int oldPos = index;
    float oldValue = 0.0f;
    bool start = true;
    int vibratoSize = 0;
    int vibratoChange = 0;
    if (isVibrato) {
        vibratoSize = 12;
        vibratoChange = 6;
    }

    int vibrato = 0;
    for (const auto& bp : bendPoints) {
        if (bp.index - oldPos > maxDistance) {
            // Add in-between points
            for (int x = oldPos + maxDistance; x < bp.index; x += maxDistance) {
                float value = oldValue + (bp.value - oldValue) *
                    (static_cast<float>(x) - oldPos) / (static_cast<float>(bp.index) - oldPos);
                result.push_back(NativeBendPoint(x, value + vibrato, usedChannel));
                if (isVibrato && std::abs(vibrato) == vibratoSize) {
                    vibratoChange = -vibratoChange;
                }
                vibrato += vibratoChange;
            }
        }

        if (start || bp.index != oldPos) {
            if (isVibrato) {
                result.push_back(NativeBendPoint(bp.index, bp.value + vibrato, bp.usedChannel));
            } else {
                result.push_back(bp);
            }
        }

        oldPos = bp.index;
        oldValue = bp.value;
        if ((start || bp.index != oldPos) && isVibrato) {
            oldValue -= vibrato;
        }

        start = false;
        if (isVibrato && std::abs(vibrato) == vibratoSize) {
            vibratoChange = -vibratoChange;
        }
        vibrato += vibratoChange;
    }

    if (std::abs(index + duration - oldPos) > maxDistance) {
        result.push_back(NativeBendPoint(index + duration, oldValue, usedChannel));
    }

    return NativeBendingPlan(originalChannel, usedChannel, result);
}

// ============================================================
// NativeTrack helper methods — ported from Native/Track.cs
// ============================================================

int NativeTrack::tryToFindChannel(const bool availableChannels[16]) {
    for (int cnt = 0; cnt < 16; cnt++) {
        if (availableChannels[cnt]) {
            return cnt;
        }
    }
    return -1;
}

int NativeTrack::getHarmonic(int baseTone, int fret, int capo,
                              float harmonicFret, HarmonicType type)
{
    // Capo, base tone and fret (if not natural harmonic) shift the harmonics simply
    int val = baseTone + capo;
    if (type != HarmonicType::Natural) {
        val += static_cast<int>(std::round(harmonicFret));
    }
    val += fret;

    // Harmonic fret lookup table
    auto approxEq = [](float a, float b) { return std::abs(a - b) < 0.001f; };

    if (approxEq(harmonicFret, 2.4f)) val += 34;
    else if (approxEq(harmonicFret, 2.7f)) val += 31;
    else if (approxEq(harmonicFret, 3.2f)) val += 28;
    else if (approxEq(harmonicFret, 4.0f)) val += 24;
    else if (approxEq(harmonicFret, 5.0f)) val += 19;
    else if (approxEq(harmonicFret, 5.8f)) val += 28;
    else if (approxEq(harmonicFret, 7.0f)) val += 12;
    else if (approxEq(harmonicFret, 8.2f)) val += 28;
    else if (approxEq(harmonicFret, 9.0f)) val += 19;
    else if (approxEq(harmonicFret, 9.6f)) val += 24;
    else if (approxEq(harmonicFret, 12.0f)) val += 0;
    else if (approxEq(harmonicFret, 14.7f)) val += 19;
    else if (approxEq(harmonicFret, 16.0f)) val += 12;
    else if (approxEq(harmonicFret, 17.0f)) val += 19;
    else if (approxEq(harmonicFret, 19.0f)) val += 0;
    else if (approxEq(harmonicFret, 21.7f)) val += 12;
    else if (approxEq(harmonicFret, 24.0f)) val += 0;
    else {
        // Default: estimate based on common overtone positions
        if (harmonicFret <= 3.0f) val += 24;
        else if (harmonicFret <= 6.0f) val += 19;
        else if (harmonicFret <= 10.0f) val += 12;
        else val += 0;
    }

    return std::min(val, 127);
}

std::vector<NativeBendPoint> NativeTrack::findAndSortCurrentBendPoints(
    const std::vector<NativeBendingPlan>& activeBendingPlans, int index)
{
    std::vector<NativeBendPoint> bendPoints;
    for (const auto& plan : activeBendingPlans) {
        for (const auto& bp : plan.bendingPoints) {
            if (bp.index <= index) {
                NativeBendPoint copy = bp;
                copy.usedChannel = plan.usedChannel;
                bendPoints.push_back(copy);
            }
        }
    }
    std::sort(bendPoints.begin(), bendPoints.end(),
        [](const NativeBendPoint& a, const NativeBendPoint& b) {
            return a.index < b.index;
        });
    return bendPoints;
}

std::vector<NativeTremoloPoint> NativeTrack::addDetailsToTremoloPoints(
    const std::vector<NativeTremoloPoint>& tremoloPoints, int maxDistance)
{
    std::vector<NativeTremoloPoint> tremPoints;
    float oldValue = 0.0f;
    int oldIndex = 0;
    for (const auto& tp : tremoloPoints) {
        if (tp.index - oldIndex > maxDistance &&
            !(std::abs(oldValue) < 0.0001f && std::abs(tp.value) < 0.0001f))
        {
            for (int x = oldIndex + maxDistance; x < tp.index; x += maxDistance) {
                float value = oldValue + (tp.value - oldValue) *
                    (static_cast<float>(x) - oldIndex) / (static_cast<float>(tp.index) - oldIndex);
                tremPoints.push_back(NativeTremoloPoint(x, value));
            }
        }
        tremPoints.push_back(tp);
        oldValue = tp.value;
        oldIndex = tp.index;
    }
    return tremPoints;
}

std::vector<std::array<int,2>> NativeTrack::createVolumeChanges(
    int index, int duration, int velocity, Fading fading)
{
    constexpr int segments = 20;
    std::vector<std::array<int,2>> changes;

    switch (fading) {
        case Fading::FadeIn:
        case Fading::FadeOut: {
            int step = velocity / segments;
            int val = (fading == Fading::FadeIn) ? 0 : velocity;
            if (fading == Fading::FadeOut) {
                step = static_cast<int>(-step * 1.25f);
            }
            for (int x = index; x < index + duration; x += duration / segments) {
                changes.push_back({x, std::min(127, std::max(0, val))});
                val += step;
            }
            break;
        }
        case Fading::VolumeSwell: {
            int step = static_cast<int>(velocity / (segments * 0.8f));
            int val = 0;
            int times = 0;
            for (int x = index; x < index + duration; x += duration / segments) {
                changes.push_back({x, std::min(127, std::max(0, val))});
                val += step;
                if (times == segments / 2) {
                    step = -step;
                }
                times++;
            }
            break;
        }
        case Fading::None:
            break;
    }

    changes.push_back({index + duration, velocity}); // Reset to normal
    return changes;
}

std::vector<int> NativeTrack::getActiveChannels(
    int trackChannel, const std::vector<std::array<int,3>>& channelConnections)
{
    std::vector<int> activeChannels = {trackChannel};
    for (const auto& cc : channelConnections) {
        activeChannels.push_back(cc[1]);
    }
    return activeChannels;
}

// ============================================================
// NativeTrack::getMidi — ported from Native/Track.cs GetMidi()
// ============================================================

std::unique_ptr<GpMidiTrack> NativeTrack::getMidi(bool availableChannels[16]) {
    auto midiTrack = std::make_unique<GpMidiTrack>();

    midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
        "midi_port", std::vector<std::string>{std::to_string(port)}, 0));
    midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
        "track_name", std::vector<std::string>{name}, 0));
    midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
        "program_change",
        std::vector<std::string>{std::to_string(channel), std::to_string(patch)}, 0));

    if (notes.empty()) {
        return midiTrack;
    }

    // Work with local copies to avoid permanently mutating the track
    auto localNotes = notes;
    auto localTremoloPoints = addDetailsToTremoloPoints(tremoloPoints, 60);

    // Add sentinel note
    NativeNote sentinel;
    sentinel.index = localNotes.back().index + localNotes.back().duration;
    sentinel.str = -2;
    localNotes.push_back(sentinel);

    std::vector<std::array<int,3>> noteOffs;        // [time, note, channel]
    std::vector<std::array<int,3>> channelConnections; // [origChannel, usedChannel, endIndex]
    std::vector<NativeBendingPlan> activeBendingPlans;
    std::vector<std::array<int,2>> volumeChanges;      // [time, value]
    int currentIndex = 0;

    for (const auto& n : localNotes) {
        std::sort(noteOffs.begin(), noteOffs.end(),
            [](const std::array<int,3>& a, const std::array<int,3>& b) {
                return a[0] < b[0];
            });

        // Check for active bendings in progress
        auto currentBPs = findAndSortCurrentBendPoints(activeBendingPlans, n.index);
        float tremBarChange = 0.0f;

        for (const auto& bp : currentBPs) {
            // Check note-offs before this bend point
            std::vector<std::array<int,3>> newNoteOffs;
            for (const auto& noteOff : noteOffs) {
                if (noteOff[0] <= bp.index) {
                    midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                        "note_off",
                        std::vector<std::string>{"" + std::to_string(noteOff[2]),
                            "" + std::to_string(noteOff[1]), "0"},
                        noteOff[0] - currentIndex));
                    currentIndex = noteOff[0];
                } else {
                    newNoteOffs.push_back(noteOff);
                }
            }
            noteOffs = newNoteOffs;

            // Check tremolo points before this bend point
            std::vector<NativeTremoloPoint> newTremPoints;
            for (const auto& tp : localTremoloPoints) {
                if (tp.index <= bp.index) {
                    tremBarChange = tp.value;
                } else {
                    newTremPoints.push_back(tp);
                }
            }
            localTremoloPoints = newTremPoints;

            // Check volume changes before this bend point
            std::vector<std::array<int,2>> newVC;
            for (const auto& vc : volumeChanges) {
                if (vc[0] <= bp.index) {
                    midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                        "control_change",
                        std::vector<std::string>{"" + std::to_string(bp.usedChannel),
                            "7", "" + std::to_string(vc[1])},
                        vc[0] - currentIndex));
                    currentIndex = vc[0];
                } else {
                    newVC.push_back(vc);
                }
            }
            volumeChanges = newVC;

            // Emit pitchwheel for this bend point
            midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                "pitchwheel",
                std::vector<std::string>{"" + std::to_string(bp.usedChannel),
                    "" + std::to_string(static_cast<int>((bp.value + tremBarChange) * 25.6f))},
                bp.index - currentIndex));
            currentIndex = bp.index;
        }

        // Delete no longer active Bending Plans
        std::vector<NativeBendingPlan> finalPlans;
        for (const auto& bpl : activeBendingPlans) {
            std::vector<NativeBendPoint> remaining;
            for (const auto& bp2 : bpl.bendingPoints) {
                if (bp2.index > n.index) {
                    remaining.push_back(bp2);
                }
            }
            if (!remaining.empty()) {
                finalPlans.push_back(NativeBendingPlan(bpl.originalChannel,
                    bpl.usedChannel, remaining));
            } else {
                // Bending plan finished — reset
                midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                    "pitchwheel",
                    std::vector<std::string>{"" + std::to_string(bpl.usedChannel), "-128"}, 0));
                midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                    "control_change",
                    std::vector<std::string>{"" + std::to_string(bpl.usedChannel), "101", "127"}, 0));
                midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                    "control_change",
                    std::vector<std::string>{"" + std::to_string(bpl.usedChannel), "10", "127"}, 0));

                // Remove channel from connections
                std::vector<std::array<int,3>> newCC;
                for (const auto& cc : channelConnections) {
                    if (cc[1] != bpl.usedChannel) {
                        newCC.push_back(cc);
                    }
                }
                channelConnections = newCC;
                availableChannels[bpl.usedChannel] = true;
            }
        }
        activeBendingPlans = finalPlans;

        // Handle tremolo points
        auto activeChans = getActiveChannels(channel, channelConnections);
        std::vector<NativeTremoloPoint> newTremPts;
        for (const auto& tp : localTremoloPoints) {
            if (tp.index <= n.index) {
                float value = tp.value * 25.6f;
                value = std::min(std::max(value, -8192.0f), 8191.0f);
                for (int ch : activeChans) {
                    midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                        "pitchwheel",
                        std::vector<std::string>{"" + std::to_string(ch),
                            "" + std::to_string(static_cast<int>(value))},
                        tp.index - currentIndex));
                    currentIndex = tp.index;
                }
            } else {
                newTremPts.push_back(tp);
            }
        }
        localTremoloPoints = newTremPts;

        // Handle volume changes
        std::vector<std::array<int,2>> newVolChanges;
        for (const auto& vc : volumeChanges) {
            if (vc[0] <= n.index) {
                for (int ch : activeChans) {
                    midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                        "control_change",
                        std::vector<std::string>{"" + std::to_string(ch),
                            "7", "" + std::to_string(vc[1])},
                        vc[0] - currentIndex));
                    currentIndex = vc[0];
                }
            } else {
                newVolChanges.push_back(vc);
            }
        }
        volumeChanges = newVolChanges;

        // Handle pending note-offs
        std::vector<std::array<int,3>> temp;
        for (const auto& noteOff : noteOffs) {
            if (noteOff[0] <= n.index) {
                midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                    "note_off",
                    std::vector<std::string>{"" + std::to_string(noteOff[2]),
                        "" + std::to_string(noteOff[1]), "0"},
                    noteOff[0] - currentIndex));
                currentIndex = noteOff[0];
            } else {
                temp.push_back(noteOff);
            }
        }
        noteOffs = temp;

        // Sentinel check
        if (n.str == -2) break;

        // Calculate MIDI note
        int midiNoteVal;
        if (n.midiNote.has_value()) {
            midiNoteVal = n.midiNote.value();
        } else {
            if (n.str - 1 < 0) continue;
            if (n.str - 1 >= static_cast<int>(tuning.size()) && !tuning.empty()) continue;

            if (!tuning.empty()) {
                midiNoteVal = tuning[n.str - 1] + capo + n.fret;
            } else {
                midiNoteVal = capo + n.fret;
            }

            if (n.harmonic != HarmonicType::None &&
                n.str - 1 >= 0 && n.str - 1 < static_cast<int>(tuning.size()))
            {
                midiNoteVal = getHarmonic(tuning[n.str - 1], n.fret, capo,
                    n.harmonicFret, n.harmonic);
            }
        }

        int noteChannel = channel;

        // Bending setup
        if (!n.bendPoints.empty()) {
            int usedChannel = tryToFindChannel(availableChannels);
            if (usedChannel == -1) usedChannel = channel;

            availableChannels[usedChannel] = false;
            channelConnections.push_back({channel, usedChannel, n.index + n.duration});
            midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                "program_change",
                std::vector<std::string>{"" + std::to_string(usedChannel),
                    "" + std::to_string(patch)},
                n.index - currentIndex));
            noteChannel = usedChannel;
            currentIndex = n.index;
            activeBendingPlans.push_back(NativeBendingPlan::create(
                n.bendPoints, channel, usedChannel, n.duration, n.index,
                n.resizeValue, n.isVibrato));
        }

        // Vibrato without bending
        if (n.isVibrato && n.bendPoints.empty()) {
            activeBendingPlans.push_back(NativeBendingPlan::create(
                {}, channel, channel, n.duration, n.index,
                n.resizeValue, true));
        }

        // Fading
        if (n.fading != Fading::None) {
            volumeChanges = createVolumeChanges(n.index, n.duration, n.velocity, n.fading);
        }

        // Note on
        midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
            "note_on",
            std::vector<std::string>{"" + std::to_string(noteChannel),
                "" + std::to_string(midiNoteVal), "" + std::to_string(n.velocity)},
            n.index - currentIndex));
        currentIndex = n.index;

        // Pitch bend range setup (after note_on)
        if (!n.bendPoints.empty()) {
            midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                "control_change",
                std::vector<std::string>{"" + std::to_string(noteChannel), "101", "0"}, 0));
            midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                "control_change",
                std::vector<std::string>{"" + std::to_string(noteChannel), "100", "0"}, 0));
            midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                "control_change",
                std::vector<std::string>{"" + std::to_string(noteChannel), "6", "6"}, 0));
            midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
                "control_change",
                std::vector<std::string>{"" + std::to_string(noteChannel), "38", "0"}, 0));
        }

        // Add to noteOffs
        noteOffs.push_back({n.index + n.duration, midiNoteVal, noteChannel});
    }

    midiTrack->messages.push_back(std::make_unique<GpMidiMessage>(
        "end_of_track", std::vector<std::string>{}, 0));
    return midiTrack;
}

// ============================================================
// NativeFormat — ported from Native/Format.cs
// ============================================================

NativeFormat::NativeFormat(GpFile* gpFile) : gpFile_(gpFile) {
    title_ = gpFile_->title;
    subtitle_ = gpFile_->subtitle;
    artist_ = gpFile_->interpret;
    album_ = gpFile_->album;
    words_ = gpFile_->words;
    music_ = gpFile_->music;
    tempos_ = retrieveTempos();
    masterBars_ = retrieveMasterBars();
    nativeTracks_ = retrieveTracks();
    updateAvailableChannels();
}

GpMidiExport NativeFormat::toMidi() {
    GpMidiExport mid(1, 960);
    mid.midiTracks.push_back(getMidiHeader());
    for (auto& track : nativeTracks_) {
        mid.midiTracks.push_back(track.getMidi(availableChannels));
    }
    return mid;
}

std::unique_ptr<GpMidiTrack> NativeFormat::getMidiHeader() {
    auto midiHeader = std::make_unique<GpMidiTrack>();

    midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
        "track_name", std::vector<std::string>{"untitled"}, 0));
    midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
        "text", std::vector<std::string>{title_}, 0));
    midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
        "text", std::vector<std::string>{subtitle_}, 0));
    midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
        "text", std::vector<std::string>{artist_}, 0));
    midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
        "text", std::vector<std::string>{album_}, 0));
    midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
        "text", std::vector<std::string>{words_}, 0));
    midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
        "text", std::vector<std::string>{music_}, 0));
    midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
        "copyright", std::vector<std::string>{"Copyright 2017 by Gitaro"}, 0));
    midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
        "marker", std::vector<std::string>{title_ + " / " + artist_ + " - Copyright 2017 by Gitaro"}, 0));
    midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
        "midi_port", std::vector<std::string>{"0"}, 0));

    // Merge tempo and masterbar events chronologically
    int tempoIndex = 0;
    int masterBarIndex = 0;
    int currentIdx = 0;
    std::string oldTimeSignature;
    std::string oldKeySignature;

    if (tempos_.empty()) {
        NativeTempo defaultTempo;
        defaultTempo.position = 0;
        defaultTempo.value = 120.0f;
        tempos_.push_back(defaultTempo);
    }

    while (tempoIndex < static_cast<int>(tempos_.size()) ||
           masterBarIndex < static_cast<int>(masterBars_.size()))
    {
        if (tempoIndex == static_cast<int>(tempos_.size()) ||
            (masterBarIndex < static_cast<int>(masterBars_.size()) &&
             tempos_[tempoIndex].position >= masterBars_[masterBarIndex].index))
        {
            // Next measure comes first
            if (masterBars_[masterBarIndex].keyBoth != oldKeySignature) {
                midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
                    "key_signature",
                    std::vector<std::string>{std::to_string(masterBars_[masterBarIndex].key),
                        std::to_string(masterBars_[masterBarIndex].keyType)},
                    masterBars_[masterBarIndex].index - currentIdx));
                currentIdx = masterBars_[masterBarIndex].index;
                oldKeySignature = masterBars_[masterBarIndex].keyBoth;
            }

            if (masterBars_[masterBarIndex].time != oldTimeSignature) {
                midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
                    "time_signature",
                    std::vector<std::string>{std::to_string(masterBars_[masterBarIndex].num),
                        std::to_string(masterBars_[masterBarIndex].den), "24", "8"},
                    masterBars_[masterBarIndex].index - currentIdx));
                currentIdx = masterBars_[masterBarIndex].index;
                oldTimeSignature = masterBars_[masterBarIndex].time;
            }

            masterBarIndex++;
        } else {
            // Next tempo comes first
            int tempo = static_cast<int>(std::round(60.0 * 1000000.0 / tempos_[tempoIndex].value));
            midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
                "set_tempo", std::vector<std::string>{std::to_string(tempo)},
                tempos_[tempoIndex].position - currentIdx));
            currentIdx = tempos_[tempoIndex].position;
            tempoIndex++;
        }
    }

    midiHeader->messages.push_back(std::make_unique<GpMidiMessage>(
        "end_of_track", std::vector<std::string>{}, 0));
    return midiHeader;
}

void NativeFormat::updateAvailableChannels() {
    for (int x = 0; x < 16; x++) {
        availableChannels[x] = (x != 9);
    }
    for (const auto& track : nativeTracks_) {
        if (track.channel >= 0 && track.channel < 16) {
            availableChannels[track.channel] = false;
        }
    }
}

// ============================================================
// retrieveTempos — ported from Native/Format.cs
// ============================================================

std::vector<NativeFormat::NativeTempo> NativeFormat::retrieveTempos() {
    std::vector<NativeTempo> tempos;
    int version = gpFile_->versionTuple[0];

    if (version < 4) {
        NativeTempo init;
        init.position = 0;
        init.value = static_cast<float>(gpFile_->tempo);
        if (init.value != 0) tempos.push_back(init);

        int pos = 0;
        float oldTempo = static_cast<float>(gpFile_->tempo);
        for (const auto& mh : gpFile_->measureHeaders) {
            NativeTempo t;
            t.value = static_cast<float>(mh->tempo.value);
            t.position = pos;
            pos += flipDuration(mh->timeSignature.denominator) * mh->timeSignature.numerator;
            if (std::abs(oldTempo - t.value) > 0.0001f) {
                tempos.push_back(t);
            }
            oldTempo = t.value;
        }
    } else {
        int pos = 0;
        NativeTempo init;
        init.position = 0;
        init.value = static_cast<float>(gpFile_->tempo);
        if (init.value != 0) tempos.push_back(init);

        if (!gpFile_->tracks.empty()) {
            for (const auto& m : gpFile_->tracks[0]->measures) {
                int smallPos = 0;
                if (m->voices.empty()) continue;
                for (const auto& b : m->voices[0]->beats) {
                    if (b->effect.mixTableChange && b->effect.mixTableChange->tempo) {
                        NativeTempo t;
                        t.value = static_cast<float>(b->effect.mixTableChange->tempo->value);
                        t.position = pos + smallPos;
                        tempos.push_back(t);
                    }
                    smallPos += flipDuration(b->duration);
                }
                pos += flipDuration(m->header->timeSignature.denominator) *
                       m->header->timeSignature.numerator;
            }
        }
    }
    return tempos;
}

// ============================================================
// retrieveMasterBars — ported from Native/Format.cs
// Note: Index and Duration are filled later during retrieveNotes
// ============================================================

std::vector<NativeMasterBar> NativeFormat::retrieveMasterBars() {
    std::vector<NativeMasterBar> masterBars;
    for (const auto& mh : gpFile_->measureHeaders) {
        NativeMasterBar mb;
        mb.time = std::to_string(mh->timeSignature.numerator) + "/" +
                  std::to_string(mh->timeSignature.denominator.value);
        mb.num = mh->timeSignature.numerator;
        mb.den = mh->timeSignature.denominator.value;

        // Decompose key signature
        std::string keyFull = std::to_string(static_cast<int>(mh->keySignature));
        if (keyFull.length() != 1) {
            mb.keyType = std::stoi(keyFull.substr(keyFull.length() - 1));
            mb.key = std::stoi(keyFull.substr(0, keyFull.length() - 1));
        } else {
            mb.key = 0;
            mb.keyType = std::stoi(keyFull);
        }
        mb.keyBoth = keyFull;
        mb.tripletFeel = mh->tripletFeel;
        masterBars.push_back(mb);
    }
    return masterBars;
}

// ============================================================
// retrieveTracks — ported from Native/Format.cs
// ============================================================

std::vector<int> NativeFormat::getTuning(const std::vector<GuitarString>& strings) {
    std::vector<int> tuning(strings.size());
    for (size_t x = 0; x < strings.size(); x++) {
        tuning[x] = strings[x].value;
    }
    return tuning;
}

std::vector<NativeTrack> NativeFormat::retrieveTracks() {
    std::vector<NativeTrack> tracks;
    for (const auto& tr : gpFile_->tracks) {
        NativeTrack track;
        track.name = tr->name;
        track.patch = tr->channel.instrument;
        track.port = tr->port;
        track.channel = tr->channel.channel;
        track.state = PlaybackState::Def;
        track.capo = tr->offset;

        if (tr->isMute) track.state = PlaybackState::Mute;
        if (tr->isSolo) track.state = PlaybackState::Solo;

        track.tuning = getTuning(tr->strings);
        track.notes = retrieveNotes(*tr, track.tuning, track);
        tracks.push_back(std::move(track));
    }
    return tracks;
}

// ============================================================
// Helper methods for retrieveNotes
// ============================================================

void NativeFormat::addToTremoloBarList(int index, int duration,
    const BendEffect& bend, NativeTrack& myTrack)
{
    myTrack.tremoloPoints.push_back(NativeTremoloPoint(index, 0.0f));
    for (const auto& bp : bend.points) {
        int at = index + static_cast<int>(bp.GP6position * duration / 100.0f);
        myTrack.tremoloPoints.push_back(NativeTremoloPoint(at, bp.GP6value));
    }
    myTrack.tremoloPoints.push_back(NativeTremoloPoint(index + duration, 0));
}

std::vector<NativeBendPoint> NativeFormat::getBendPoints(int index, int duration,
    const BendEffect& bend)
{
    std::vector<NativeBendPoint> ret;
    for (const auto& bp : bend.points) {
        int at = index + static_cast<int>(bp.GP6position * duration / 100.0f);
        ret.push_back(NativeBendPoint(at, bp.GP6value));
    }
    return ret;
}

int NativeFormat::flipDuration(const Duration& dur) {
    constexpr int ticksPerBeat = 960;
    int result = 0;
    switch (dur.value) {
        case 1:   result = ticksPerBeat * 4; break;
        case 2:   result = ticksPerBeat * 2; break;
        case 4:   result = ticksPerBeat; break;
        case 8:   result = ticksPerBeat / 2; break;
        case 16:  result = ticksPerBeat / 4; break;
        case 32:  result = ticksPerBeat / 8; break;
        case 64:  result = ticksPerBeat / 16; break;
        case 128: result = ticksPerBeat / 32; break;
        default:  result = ticksPerBeat; break;
    }
    if (dur.isDotted) result = static_cast<int>(result * 1.5f);
    if (dur.isDoubleDotted) result = static_cast<int>(result * 1.75f);

    int enters = dur.tuplet.enters;
    int times = dur.tuplet.times;
    result = static_cast<int>(result * times / static_cast<float>(enters));
    return result;
}

// ============================================================
// retrieveNotes — ported from Native/Format.cs RetrieveNotes()
// ============================================================

std::vector<NativeNote> NativeFormat::retrieveNotes(const GpTrack& track,
    const std::vector<int>& tuning, NativeTrack& myTrack)
{
    std::vector<NativeNote> notes;
    int index = 0;
    int lastNoteIdx[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    bool lastWasTie[10] = {};

    // Grace note state
    bool rememberGrace = false;
    bool rememberedGrace = false;
    int graceLength = 0;
    int subtractSubindex = 0;

    int measureIndex = -1;
    for (const auto& m : track.measures) {
        int notesInMeasure = 0;
        measureIndex++;
        bool skipVoice = false;

        // Handle SimileMark (measure repeat)
        switch (m->simileMark) {
            case SimileMark::simple: {
                if (!notesInMeasures_.empty()) {
                    int amountNotes = notesInMeasures_.back();
                    int endPoint = static_cast<int>(notes.size());
                    for (int x = endPoint - amountNotes; x < endPoint; x++) {
                        NativeNote newNote = notes[x];
                        const auto& oldM = track.measures[measureIndex - 1];
                        newNote.index += flipDuration(oldM->header->timeSignature.denominator) *
                                         oldM->header->timeSignature.numerator;
                        notes.push_back(newNote);
                        notesInMeasure++;
                    }
                }
                skipVoice = true;
                break;
            }
            case SimileMark::firstOfDouble:
            case SimileMark::secondOfDouble: {
                if (notesInMeasures_.size() >= 2) {
                    int secondAmount = notesInMeasures_[notesInMeasures_.size() - 1];
                    int firstAmount = notesInMeasures_[notesInMeasures_.size() - 2];
                    int endPoint = static_cast<int>(notes.size()) - secondAmount;
                    for (int x = endPoint - firstAmount; x < endPoint; x++) {
                        NativeNote newNote = notes[x];
                        const auto& oldM1 = track.measures[measureIndex - 2];
                        const auto& oldM2 = track.measures[measureIndex - 1];
                        newNote.index += flipDuration(oldM1->header->timeSignature.denominator) *
                                         oldM1->header->timeSignature.numerator;
                        newNote.index += flipDuration(oldM2->header->timeSignature.denominator) *
                                         oldM2->header->timeSignature.numerator;
                        notes.push_back(newNote);
                        notesInMeasure++;
                    }
                }
                skipVoice = true;
                break;
            }
            case SimileMark::none:
            default:
                break;
        }

        for (const auto& v : m->voices) {
            if (skipVoice) break;

            int subIndex = 0;
            for (const auto& b : v->beats) {
                // Tremolo bar
                if (b->effect.tremoloBar) {
                    addToTremoloBarList(index + subIndex, flipDuration(b->duration),
                        *b->effect.tremoloBar, myTrack);
                }

                // Prepare Brush or Arpeggio
                bool hasBrush = false;
                int brushInit = 0;
                int brushIncrease = 0;
                BeatStrokeDirection brushDirection = BeatStrokeDirection::none;

                if (b->effect.stroke) {
                    int notesCnt = static_cast<int>(b->notes.size());
                    brushDirection = b->effect.stroke->direction;
                    if (brushDirection != BeatStrokeDirection::none && notesCnt > 1) {
                        hasBrush = true;
                        Duration temp;
                        temp.value = b->effect.stroke->value;
                        int brushTotalDuration = flipDuration(temp);
                        brushIncrease = brushTotalDuration / notesCnt;
                        int startPos = index + subIndex +
                            static_cast<int>((brushTotalDuration - brushIncrease) *
                                (b->effect.stroke->startTime - 1));
                        int endPos = startPos + brushTotalDuration - brushIncrease;

                        if (brushDirection == BeatStrokeDirection::down) {
                            brushInit = startPos;
                        } else {
                            brushInit = endPos;
                            brushIncrease = -brushIncrease;
                        }
                    }
                }

                for (const auto& n : b->notes) {
                    NativeNote note;
                    note.isTremBarVibrato = b->effect.vibrato;
                    note.fading = Fading::None;

                    // Beat fading effects
                    if (b->effect.fadeIn) note.fading = Fading::FadeIn;
                    if (b->effect.fadeOut) note.fading = Fading::FadeOut;
                    if (b->effect.volumeSwell) note.fading = Fading::VolumeSwell;

                    note.isSlapped = (b->effect.slapEffect == SlapEffect::slapping);
                    note.isPopped = (b->effect.slapEffect == SlapEffect::popping);
                    note.isHammer = n->effect.hammer;
                    note.isRhTapped = (b->effect.slapEffect == SlapEffect::tapping);
                    note.index = index + subIndex;
                    note.duration = flipDuration(b->duration);

                    // Note values
                    note.fret = n->value;
                    note.str = n->str;
                    note.midiNote = n->midiNote;
                    note.velocity = n->velocity;
                    note.isVibrato = n->effect.vibrato;
                    note.isPalmMuted = n->effect.palmMute;
                    note.isMuted = (n->type == NoteType::dead);

                    // Harmonics
                    if (n->effect.harmonic) {
                        note.harmonicFret = n->effect.harmonic->fret;
                        if (n->effect.harmonic->fret == 0.0f) {
                            if (n->effect.harmonic->type == 2) {
                                auto* ah = dynamic_cast<ArtificialHarmonic*>(
                                    n->effect.harmonic.get());
                                if (ah) {
                                    note.harmonicFret = ah->pitch.actualOvertone;
                                }
                            }
                        }
                        switch (n->effect.harmonic->type) {
                            case 1: note.harmonic = HarmonicType::Natural; break;
                            case 2: note.harmonic = HarmonicType::Artificial; break;
                            case 3: note.harmonic = HarmonicType::Pinch; break;
                            case 4: note.harmonic = HarmonicType::Tapped; break;
                            case 5: note.harmonic = HarmonicType::Semi; break;
                            default: note.harmonic = HarmonicType::Natural; break;
                        }
                    }

                    // Slides
                    for (const auto& sl : n->effect.slides) {
                        note.slidesToNext = note.slidesToNext ||
                            sl == SlideType::shiftSlideTo || sl == SlideType::legatoSlideTo;
                        note.slideInFromAbove = note.slideInFromAbove ||
                            sl == SlideType::intoFromAbove;
                        note.slideInFromBelow = note.slideInFromBelow ||
                            sl == SlideType::intoFromBelow;
                        note.slideOutDownwards = note.slideOutDownwards ||
                            sl == SlideType::outDownwards;
                        note.slideOutUpwards = note.slideOutUpwards ||
                            sl == SlideType::outUpwards;
                    }

                    // Bends
                    if (n->effect.isBend()) {
                        note.bendPoints = getBendPoints(index + subIndex,
                            flipDuration(b->duration), *n->effect.bend);
                    }

                    // Ties
                    bool dontAddNote = false;

                    if (n->type == NoteType::tie) {
                        dontAddNote = true;
                        int strIdx = std::max(0, note.str - 1);
                        int lastIdx = lastNoteIdx[strIdx];

                        if (lastIdx >= 0) {
                            note.fret = notes[lastIdx].fret; // For GP3 & GP4
                            if (notes[lastIdx].harmonic != note.harmonic ||
                                std::abs(notes[lastIdx].harmonicFret - note.harmonicFret) > 0.0001f)
                            {
                                dontAddNote = false;
                            }

                            if (dontAddNote) {
                                note.connect = true;
                                notes[lastIdx].duration += note.duration;
                                notes[lastIdx].addBendPoints(note.bendPoints);
                            }
                        }
                    } else {
                        lastWasTie[std::max(0, note.str - 1)] = false;
                    }

                    // Triplet Feel
                    if (measureIndex < static_cast<int>(masterBars_.size()) &&
                        masterBars_[measureIndex].tripletFeel != TripletFeel::none)
                    {
                        auto trip = masterBars_[measureIndex].tripletFeel;
                        bool is8ThPos = subIndex % 480 == 0;
                        bool is16ThPos = subIndex % 240 == 0;
                        bool isFirst = true;
                        if (is8ThPos) isFirst = (subIndex % 960 == 0);
                        if (is16ThPos) isFirst = is8ThPos;

                        bool is8Th = b->duration.value == 8 && !b->duration.isDotted &&
                            !b->duration.isDoubleDotted && b->duration.tuplet.enters == 1 &&
                            b->duration.tuplet.times == 1;
                        bool is16Th = b->duration.value == 16 && !b->duration.isDotted &&
                            !b->duration.isDoubleDotted && b->duration.tuplet.enters == 1 &&
                            b->duration.tuplet.times == 1;

                        if ((trip == TripletFeel::eigth && is8ThPos && is8Th) ||
                            (trip == TripletFeel::sixteenth && is16ThPos && is16Th))
                        {
                            if (isFirst) {
                                note.duration = static_cast<int>(note.duration * (4.0f / 3.0f));
                            } else {
                                note.duration = static_cast<int>(note.duration * (2.0f / 3.0f));
                                note.resizeValue *= 2.0f / 3.0f;
                                note.index += static_cast<int>(note.duration * (1.0f / 3.0f));
                            }
                        }
                        else if ((trip == TripletFeel::dotted8th && is8ThPos && is8Th) ||
                                 (trip == TripletFeel::dotted16th && is16ThPos && is16Th))
                        {
                            if (isFirst) {
                                note.duration = static_cast<int>(note.duration * 1.5f);
                            } else {
                                note.duration = static_cast<int>(note.duration * 0.5f);
                                note.resizeValue *= 0.5f;
                                note.index += static_cast<int>(note.duration * 0.5f);
                            }
                        }
                        else if ((trip == TripletFeel::scottish8th && is8ThPos && is8Th) ||
                                 (trip == TripletFeel::scottish16th && is16ThPos && is16Th))
                        {
                            if (isFirst) {
                                note.duration = static_cast<int>(note.duration * 0.5f);
                            } else {
                                note.duration = static_cast<int>(note.duration * 1.5f);
                                note.resizeValue *= 1.5f;
                                note.index -= static_cast<int>(note.duration * 0.5f);
                            }
                        }
                    }

                    // Tremolo Picking & Trill
                    if (n->effect.tremoloPicking || n->effect.trill) {
                        int len = note.duration;
                        if (n->effect.tremoloPicking) {
                            len = flipDuration(n->effect.tremoloPicking->duration);
                        }
                        if (n->effect.trill) {
                            len = flipDuration(n->effect.trill->duration);
                        }

                        int origDuration = note.duration;
                        note.duration = len;
                        note.resizeValue *= static_cast<float>(len) / origDuration;
                        int currentNoteIndex = note.index + len;

                        int strIdx2 = std::max(0, note.str - 1);
                        notes.push_back(note);
                        lastNoteIdx[strIdx2] = static_cast<int>(notes.size()) - 1;
                        notesInMeasure++;

                        dontAddNote = true;
                        bool originalFret = false;
                        int secondFret = note.fret;
                        if (n->effect.trill && note.str - 1 >= 0 &&
                            note.str - 1 < static_cast<int>(tuning.size()))
                        {
                            secondFret = n->effect.trill->fret - tuning[note.str - 1];
                        }

                        while (currentNoteIndex + len <= note.index + origDuration) {
                            NativeNote newOne = note;
                            newOne.index = currentNoteIndex;
                            if (!originalFret) {
                                newOne.fret = secondFret;
                            }
                            if (n->effect.trill) {
                                newOne.isHammer = true;
                            }
                            notes.push_back(newOne);
                            lastNoteIdx[strIdx2] = static_cast<int>(notes.size()) - 1;
                            notesInMeasure++;
                            currentNoteIndex += len;
                            originalFret = !originalFret;
                        }
                    }

                    // Grace Note
                    if (rememberGrace && note.duration > graceLength) {
                        int orig = note.duration;
                        note.duration -= graceLength;
                        note.resizeValue *= static_cast<float>(note.duration) / orig;
                        rememberedGrace = true;
                    }

                    if (n->effect.grace) {
                        bool isOnBeat = n->effect.grace->isOnBeat;
                        if (n->effect.grace->duration != -1) {
                            // GP3,4,5 format
                            NativeNote graceNote;
                            graceNote.index = note.index;
                            graceNote.fret = n->effect.grace->fret;
                            graceNote.str = note.str;
                            Duration dur;
                            dur.value = n->effect.grace->duration;
                            graceNote.duration = flipDuration(dur);
                            if (isOnBeat) {
                                int orig = note.duration;
                                note.duration -= graceNote.duration;
                                note.index += graceNote.duration;
                                note.resizeValue *= static_cast<float>(note.duration) / orig;
                            } else {
                                graceNote.index -= graceNote.duration;
                            }
                            notes.push_back(graceNote);
                            notesInMeasure++;
                        } else {
                            if (isOnBeat) {
                                rememberGrace = true;
                                graceLength = note.duration;
                            } else {
                                if (!notes.empty()) {
                                    note.index -= note.duration;
                                    subtractSubindex = note.duration;
                                }
                            }
                        }
                    }

                    // Dead Notes
                    if (n->type == NoteType::dead) {
                        int orig = note.duration;
                        note.velocity = static_cast<int>(note.velocity * 0.9f);
                        note.duration /= 6;
                        note.resizeValue *= static_cast<float>(note.duration) / orig;
                    }

                    // Palm Mute (Ghost Notes in C# comment)
                    if (n->effect.palmMute) {
                        int orig = note.duration;
                        note.velocity = static_cast<int>(note.velocity * 0.7f);
                        note.duration /= 2;
                        note.resizeValue *= static_cast<float>(note.duration) / orig;
                    }

                    // Ghost Notes
                    if (n->effect.ghostNote) {
                        note.velocity = static_cast<int>(note.velocity * 0.8f);
                    }

                    // Staccato
                    if (n->effect.staccato) {
                        int orig = note.duration;
                        note.duration /= 2;
                        note.resizeValue *= static_cast<float>(note.duration) / orig;
                    }

                    // Accented
                    if (n->effect.accentuatedNote) {
                        note.velocity = static_cast<int>(note.velocity * 1.2f);
                    }

                    // Heavy Accented
                    if (n->effect.heavyAccentuatedNote) {
                        note.velocity = static_cast<int>(note.velocity * 1.4f);
                    }

                    // Arpeggio / Brush
                    if (hasBrush) {
                        note.index = brushInit;
                        brushInit += brushIncrease;
                    }

                    if (!dontAddNote) {
                        int strIdx3 = std::max(0, note.str - 1);
                        notes.push_back(note);
                        lastNoteIdx[strIdx3] = static_cast<int>(notes.size()) - 1;
                        notesInMeasure++;
                    }
                }

                if (rememberedGrace) {
                    subIndex -= graceLength;
                    rememberGrace = false;
                    rememberedGrace = false;
                }

                subIndex -= subtractSubindex;
                subtractSubindex = 0;
                subIndex += flipDuration(b->duration);

                // Sort brushed tones for up-stroke
                if (hasBrush && brushDirection == BeatStrokeDirection::up) {
                    int notesCnt = static_cast<int>(b->notes.size());
                    int notesSize = static_cast<int>(notes.size());
                    if (notesCnt > 0 && notesSize >= notesCnt) {
                        std::vector<NativeNote> tempNotes(notesCnt);
                        for (int x = notesSize - notesCnt; x < notesSize; x++) {
                            tempNotes[x - (notesSize - notesCnt)] = notes[x];
                        }
                        for (int x = notesSize - notesCnt; x < notesSize; x++) {
                            notes[x] = tempNotes[static_cast<int>(tempNotes.size()) -
                                (x - (notesSize - notesCnt)) - 1];
                        }
                    }
                }
            }

            break; // Consider only the first voice
        }

        int measureDuration = flipDuration(m->header->timeSignature.denominator) *
                              m->header->timeSignature.numerator;

        // Update masterBar index and duration (filled during note retrieval)
        if (measureIndex < static_cast<int>(masterBars_.size())) {
            masterBars_[measureIndex].duration = measureDuration;
            masterBars_[measureIndex].index = index;
        }

        index += measureDuration;
        notesInMeasures_.push_back(notesInMeasure);
    }

    return notes;
}
