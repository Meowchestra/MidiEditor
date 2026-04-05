#include "Gp12Parser.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

// ============================================================
// String reading helpers for GP1/GP2 format
// ============================================================

// TuxGuitar's readStringByteSizeOfByte: byte N, byte len, (N-1) bytes buffer
static std::string gpReadStringBSoB(GpBinaryReader& reader) {
    int size = reader.readByte(1)[0] - 1;
    return reader.readByteSizeString(size);
}

// TuxGuitar's readStringByte(0): byte len, then len bytes of string
static std::string gpReadStringByte0(GpBinaryReader& reader) {
    int len = reader.readByte(1)[0];
    return reader.readString(len);
}

// Deep copy a GpNoteEffect (handles unique_ptr members)
static GpNoteEffect cloneNoteEffect(const GpNoteEffect& src) {
    GpNoteEffect dst;
    dst.vibrato = src.vibrato;
    dst.slides = src.slides;
    dst.hammer = src.hammer;
    dst.ghostNote = src.ghostNote;
    dst.accentuatedNote = src.accentuatedNote;
    dst.heavyAccentuatedNote = src.heavyAccentuatedNote;
    dst.palmMute = src.palmMute;
    dst.staccato = src.staccato;
    dst.letRing = src.letRing;
    dst.leftHandFinger = src.leftHandFinger;
    dst.rightHandFinger = src.rightHandFinger;
    if (src.bend) dst.bend = std::make_unique<BendEffect>(*src.bend);
    if (src.harmonic) dst.harmonic = std::make_unique<HarmonicEffect>(*src.harmonic);
    if (src.grace) dst.grace = std::make_unique<GraceEffect>(*src.grace);
    if (src.trill) dst.trill = std::make_unique<TrillEffect>(*src.trill);
    if (src.tremoloPicking) dst.tremoloPicking = std::make_unique<TremoloPickingEffect>(*src.tremoloPicking);
    return dst;
}

// ============================================================
// Gp1Parser
// ============================================================

Gp1Parser::Gp1Parser(const std::vector<uint8_t>& data) : reader(data) {}

void Gp1Parser::readVersion() {
    // Version header is always 31 bytes: 1 byte length + 30 byte buffer
    uint8_t len = reader.readByte(1)[0];
    std::string ver = reader.readString(30, len);
    version = ver;

    if      (ver == "FICHIER GUITARE PRO v1")    versionCode_ = 0;
    else if (ver == "FICHIER GUITARE PRO v1.01") versionCode_ = 1;
    else if (ver == "FICHIER GUITARE PRO v1.02") versionCode_ = 2;
    else if (ver == "FICHIER GUITARE PRO v1.03") versionCode_ = 3;
    else if (ver == "FICHIER GUITARE PRO v1.04") versionCode_ = 4;
    else throw std::runtime_error("Unsupported GP1 version: " + ver);
}

void Gp1Parser::readSong() {
    readVersion();

    trackCount_ = (versionCode_ > 2) ? 8 : 1;

    readInfo();

    tempo = reader.readInt(1)[0];
    int tripletFeelVal = reader.readInt(1)[0];
    tripletFeel_ = (tripletFeelVal == 1) ? TripletFeel::eigth : TripletFeel::none;

    if (versionCode_ > 2) {
        keySignature_ = static_cast<KeySignature>(reader.readInt(1)[0]);
    }

    // Create tracks with channels and strings
    for (int i = 0; i < trackCount_; i++) {
        auto track = std::make_unique<GpTrack>(this, i + 1);
        track->channel.channel = TRACK_CHANNELS[i][1];
        track->channel.effectChannel = TRACK_CHANNELS[i][2];
        track->color = GpColor(255, 0, 0);

        int stringCount = (versionCode_ > 1) ? reader.readInt(1)[0] : 6;
        for (int j = 0; j < stringCount; j++) {
            track->strings.push_back(GuitarString(j + 1, reader.readInt(1)[0]));
        }

        tracks.push_back(std::move(track));
    }

    measureCount = reader.readInt(1)[0];
    trackCount = trackCount_;

    // Read per-track info (program, name, channel params)
    for (int i = 0; i < trackCount_; i++) {
        readTrack(tracks[i].get(), i);
    }

    if (versionCode_ > 2) {
        reader.skip(10);
    }

    // Read measures (interleaved: all tracks per measure)
    std::vector<long> lastReadStarts(trackCount_, 0);
    for (int i = 0; i < measureCount; i++) {
        readTrackMeasures(lastReadStarts.data());
    }
}

void Gp1Parser::readInfo() {
    title = gpReadStringBSoB(reader);
    interpret = gpReadStringBSoB(reader);
    gpReadStringBSoB(reader); // notice (discarded)
}

void Gp1Parser::readTrack(GpTrack* track, int /*trackIndex*/) {
    track->name = "Track 1";
    track->channel.instrument = reader.readInt(1)[0]; // program
    if (versionCode_ > 2) {
        reader.readInt(1); // fret count (discarded)
        track->name = gpReadStringBSoB(reader);
        track->isSolo = reader.readBool(1)[0];
        track->channel.volume = reader.readInt(1)[0];
        track->channel.balance = reader.readInt(1)[0];
        track->channel.chorus = reader.readInt(1)[0];
        track->channel.reverb = reader.readInt(1)[0];
        track->offset = reader.readInt(1)[0];
    }
}

void Gp1Parser::readTrackMeasures(long* lastReadStarts) {
    // Create measure header
    auto header = std::make_unique<MeasureHeader>();
    MeasureHeader* prev = measureHeaders.empty() ? nullptr : measureHeaders.back().get();

    header->start = prev ? (prev->start + prev->length()) : Duration::quarterTime;
    header->number = prev ? (prev->number + 1) : 1;
    header->tempo = prev ? Tempo(prev->tempo.value) : Tempo(tempo);
    header->tripletFeel = tripletFeel_;
    header->keySignature = keySignature_;
    header->song = this;

    // Read time signature
    readTimeSignature(header->timeSignature);

    reader.skip(6);

    // Read beat counts per track
    std::vector<int> beats(trackCount_);
    for (int i = 0; i < trackCount_; i++) {
        reader.readByte(1); // skip
        reader.readByte(1); // skip
        beats[i] = reader.readByte(1)[0];
        if (beats[i] > 127) beats[i] = 0;
        reader.skip(9);
    }

    reader.skip(2);

    uint8_t flags = reader.readByte(1)[0];

    header->isRepeatOpen = ((flags & 0x01) != 0);
    if ((flags & 0x02) != 0) {
        header->repeatClose = reader.readByte(1)[0];
    }
    if ((flags & 0x04) != 0) {
        int altValue = reader.readByte(1)[0];
        int alt = parseRepeatAlternative(header->number, altValue);
        if (alt != 0) {
            header->repeatAlternatives.push_back(alt);
        }
    }

    MeasureHeader* hdr = header.get();
    measureHeaders.push_back(std::move(header));

    // Create measures for each track
    for (int i = 0; i < trackCount_; i++) {
        auto measure = std::make_unique<GpMeasure>(tracks[i].get(), hdr);

        long start = hdr->start;
        for (int j = 0; j < beats[i]; j++) {
            long length = readBeat(tracks[i].get(), measure.get(), start, lastReadStarts[i]);
            lastReadStarts[i] = start;
            start += length;
        }

        measure->clef = getClef(tracks[i].get());
        tracks[i]->addMeasure(std::move(measure));
    }
}

long Gp1Parser::readBeat(GpTrack* track, GpMeasure* measure, long start, long lastReadStart) {
    reader.readInt(1); // skip 4 bytes

    auto beat = std::make_unique<GpBeat>();
    beat->start = static_cast<int>(start);
    Duration duration = readDuration();
    GpNoteEffect noteEffect;

    uint8_t flags = reader.readByte(1)[0];

    duration.isDotted = ((flags & 0x10) != 0);
    if ((flags & 0x20) != 0) {
        duration.tuplet.enters = 3;
        duration.tuplet.times = 2;
        reader.skip(1);
    }

    // Beat effects
    if ((flags & 0x04) != 0) {
        readBeatEffects(beat->effect, noteEffect);
    }

    // Chord diagram
    if ((flags & 0x02) != 0) {
        readChord(static_cast<int>(track->strings.size()), beat.get());
    }

    // Text
    if ((flags & 0x01) != 0) {
        beat->text = std::make_unique<BeatText>(readText());
    }

    GpVoice* voice = measure->voices[0].get();

    if ((flags & 0x40) != 0) {
        // Tied notes — copy from previous beat
        if (lastReadStart < start) {
            GpBeat* prevBeat = findBeat(track, measure, lastReadStart);
            if (prevBeat) {
                for (auto& prevNote : prevBeat->notes) {
                    auto note = std::make_unique<GpNote>(beat.get());
                    note->value = prevNote->value;
                    note->str = prevNote->str;
                    note->velocity = prevNote->velocity;
                    note->type = NoteType::tie;
                    beat->notes.push_back(std::move(note));
                }
            }
        }
    } else if ((flags & 0x08) == 0) {
        // Normal notes
        uint8_t stringsFlags = reader.readByte(1)[0];
        uint8_t effectsFlags = reader.readByte(1)[0];

        for (int i = 5; i >= 0; i--) {
            if ((stringsFlags & (1 << i)) != 0) {
                auto note = std::make_unique<GpNote>(beat.get());

                uint8_t fret = reader.readByte(1)[0];

                GpNoteEffect thisNoteEffect = cloneNoteEffect(noteEffect);
                if ((effectsFlags & (1 << i)) != 0) {
                    readNoteEffects(thisNoteEffect);
                }

                note->value = (fret < 100) ? fret : 0;
                note->velocity = Velocities::def;
                note->str = static_cast<int>(track->strings.size()) - i;
                note->effect = std::move(thisNoteEffect);
                note->type = (fret >= 100) ? NoteType::dead : NoteType::normal;

                beat->notes.push_back(std::move(note));
            }
        }
    } else {
        // Rest beat
        beat->status = BeatStatus::rest;
    }

    beat->duration = duration;
    beat->voice = voice;
    voice->beats.push_back(std::move(beat));

    return duration.time();
}

Duration Gp1Parser::readDuration() {
    Duration d;
    int8_t val = reader.readSignedByte(1)[0];
    d.value = static_cast<int>(std::pow(2.0, val + 4) / 4.0);
    return d;
}

void Gp1Parser::readTimeSignature(TimeSignature& ts) {
    ts.numerator = reader.readByte(1)[0];
    ts.denominator.value = reader.readByte(1)[0];
}

void Gp1Parser::readBeatEffects(BeatEffect& beatEffect, GpNoteEffect& noteEffect) {
    uint8_t flags = reader.readByte(1)[0];

    noteEffect.vibrato = (flags == 1 || flags == 2);
    beatEffect.vibrato = noteEffect.vibrato;
    beatEffect.fadeIn = (flags == 4);

    if (flags == 5) beatEffect.slapEffect = SlapEffect::tapping;
    else if (flags == 6) beatEffect.slapEffect = SlapEffect::slapping;
    else if (flags == 7) beatEffect.slapEffect = SlapEffect::popping;

    if (flags == 3) {
        beatEffect.tremoloBar = std::make_unique<BendEffect>(readBend());
    } else if (flags == 8 || flags == 9) {
        auto harmonic = std::make_unique<HarmonicEffect>();
        harmonic->type = (flags == 8) ? 1 : 2; // Natural : Artificial
        noteEffect.harmonic = std::move(harmonic);
    }
}

void Gp1Parser::readNoteEffects(GpNoteEffect& effect) {
    uint8_t flags = reader.readByte(1)[0];

    effect.hammer = (flags == 1 || flags == 2);
    if (flags == 3 || flags == 4) {
        effect.slides.push_back(SlideType::shiftSlideTo);
    }
    if (flags == 5 || flags == 6) {
        effect.bend = std::make_unique<BendEffect>(readBend());
    }
}

BendEffect Gp1Parser::readBend() {
    reader.skip(6);
    float value = std::max(reader.readByte(1)[0] / 8.0f - 26.0f, 1.0f);
    BendEffect bend;
    bend.type = BendType::bend;
    bend.points.push_back(BendPoint(0, 0));
    bend.points.push_back(BendPoint(
        static_cast<int>(std::round(BendEffect::maxPosition / 2.0f)),
        static_cast<int>(std::round(value * BendEffect::semitoneLength))
    ));
    bend.points.push_back(BendPoint(
        BendEffect::maxPosition,
        static_cast<int>(std::round(value * BendEffect::semitoneLength))
    ));
    reader.skip(1);
    return bend;
}

void Gp1Parser::readChord(int stringCount, GpBeat* beat) {
    if (versionCode_ > 3) {
        auto chord = std::make_unique<Chord>(stringCount);
        chord->name = gpReadStringByte0(reader);
        reader.skip(1);
        if (reader.readInt(1)[0] < 12) {
            reader.skip(32);
        }
        chord->firstFret = reader.readInt(1)[0];
        if (chord->firstFret != 0) {
            for (int i = 0; i < 6; i++) {
                int fret = reader.readInt(1)[0];
                if (i < static_cast<int>(chord->strings.size())) {
                    chord->strings[i] = fret;
                }
            }
        }
        if (!chord->notes().empty()) {
            beat->effect.chord = std::move(chord);
        }
    } else {
        gpReadStringBSoB(reader); // simple chord name, discarded
    }
}

std::string Gp1Parser::readText() {
    return gpReadStringByte0(reader);
}

int Gp1Parser::parseRepeatAlternative(int measureNumber, int value) {
    int repeatAlternative = 0;
    int existentAlternatives = 0;

    for (const auto& header : measureHeaders) {
        if (header->number == measureNumber) break;
        if (header->isRepeatOpen) existentAlternatives = 0;
        for (int alt : header->repeatAlternatives) {
            existentAlternatives |= alt;
        }
    }

    for (int i = 0; i < 8; i++) {
        if (value > i && (existentAlternatives & (1 << i)) == 0) {
            repeatAlternative |= (1 << i);
        }
    }

    return repeatAlternative;
}

MeasureClef Gp1Parser::getClef(GpTrack* track) {
    if (!track->channel.isPercussionChannel()) {
        for (const auto& s : track->strings) {
            if (s.value <= 34) return MeasureClef::bass;
        }
    }
    return MeasureClef::treble;
}

GpBeat* Gp1Parser::findBeat(GpTrack* track, GpMeasure* measure, long start) {
    GpBeat* beat = findBeatInMeasure(measure, start);
    if (!beat) {
        for (int i = static_cast<int>(track->measures.size()) - 1; i >= 0; i--) {
            beat = findBeatInMeasure(track->measures[i].get(), start);
            if (beat) break;
        }
    }
    return beat;
}

GpBeat* Gp1Parser::findBeatInMeasure(GpMeasure* measure, long start) {
    int mStart = measure->start();
    int mLength = measure->header->length();
    if (start >= mStart && start < (mStart + mLength)) {
        for (auto& beat : measure->voices[0]->beats) {
            if (beat->start == static_cast<int>(start)) return beat.get();
        }
    }
    return nullptr;
}

// ============================================================
// Gp2Parser
// ============================================================

Gp2Parser::Gp2Parser(const std::vector<uint8_t>& data) : Gp1Parser(data) {}

void Gp2Parser::readVersion() {
    // Version header is always 31 bytes: 1 byte length + 30 byte buffer
    uint8_t len = reader.readByte(1)[0];
    std::string ver = reader.readString(30, len);
    version = ver;

    if      (ver == "FICHIER GUITAR PRO v2.20") versionCode_ = 0;
    else if (ver == "FICHIER GUITAR PRO v2.21") versionCode_ = 1;
    else throw std::runtime_error("Unsupported GP2 version: " + ver);
}

void Gp2Parser::readSong() {
    readVersion();

    trackCount_ = 8; // Always 8 tracks in GP2

    readInfo();

    tempo = reader.readInt(1)[0];
    int tripletFeelVal = reader.readInt(1)[0];
    tripletFeel_ = (tripletFeelVal == 1) ? TripletFeel::eigth : TripletFeel::none;

    keySignature_ = static_cast<KeySignature>(reader.readInt(1)[0]);

    // Create tracks with channels and strings
    for (int i = 0; i < trackCount_; i++) {
        auto track = std::make_unique<GpTrack>(this, i + 1);
        track->channel.channel = TRACK_CHANNELS[i][1];
        track->channel.effectChannel = TRACK_CHANNELS[i][2];
        track->color = GpColor(255, 0, 0);

        int stringCount = reader.readInt(1)[0];
        for (int j = 0; j < stringCount; j++) {
            track->strings.push_back(GuitarString(j + 1, reader.readInt(1)[0]));
        }

        tracks.push_back(std::move(track));
    }

    measureCount = reader.readInt(1)[0];
    trackCount = trackCount_;

    for (int i = 0; i < trackCount_; i++) {
        readTrack(tracks[i].get(), i);
    }

    reader.skip(10);

    // Read measures (interleaved)
    std::vector<long> lastReadStarts(trackCount_, 0);
    for (int i = 0; i < measureCount; i++) {
        readTrackMeasures(lastReadStarts.data());
    }
}

void Gp2Parser::readTrack(GpTrack* track, int /*trackIndex*/) {
    track->channel.instrument = reader.readInt(1)[0]; // program
    reader.readInt(1); // fret count (discarded)
    track->name = gpReadStringBSoB(reader);
    track->isSolo = reader.readBool(1)[0];
    track->channel.volume = reader.readInt(1)[0];
    track->channel.balance = reader.readInt(1)[0];
    track->channel.chorus = reader.readInt(1)[0];
    track->channel.reverb = reader.readInt(1)[0];
    track->offset = reader.readInt(1)[0];
}

long Gp2Parser::readBeat(GpTrack* track, GpMeasure* measure, long start, long lastReadStart) {
    reader.readInt(1); // skip 4 bytes

    auto beat = std::make_unique<GpBeat>();
    beat->start = static_cast<int>(start);
    Duration duration = readDuration();
    GpNoteEffect noteEffect;

    uint8_t flags1 = reader.readByte(1)[0];
    uint8_t flags2 = reader.readByte(1)[0];

    // Mix table change
    if ((flags2 & 0x02) != 0) {
        readMixChange(measure->tempo());
    }

    // Stroke
    if ((flags2 & 0x01) != 0) {
        reader.readByte(1); // strokeType
        reader.readByte(1); // strokeDuration
    }

    duration.isDotted = ((flags1 & 0x10) != 0);
    if ((flags1 & 0x20) != 0) {
        duration.tuplet.enters = 3;
        duration.tuplet.times = 2;
        reader.skip(1);
    }

    // Beat effects
    if ((flags1 & 0x04) != 0) {
        readBeatEffects(beat->effect, noteEffect);
    }

    // Chord diagram
    if ((flags1 & 0x02) != 0) {
        readChord(static_cast<int>(track->strings.size()), beat.get());
    }

    // Text
    if ((flags1 & 0x01) != 0) {
        beat->text = std::make_unique<BeatText>(readText());
    }

    GpVoice* voice = measure->voices[0].get();

    if ((flags1 & 0x40) != 0) {
        // Tied notes
        if (lastReadStart < start) {
            GpBeat* prevBeat = findBeat(track, measure, lastReadStart);
            if (prevBeat) {
                for (auto& prevNote : prevBeat->notes) {
                    auto note = std::make_unique<GpNote>(beat.get());
                    note->value = prevNote->value;
                    note->str = prevNote->str;
                    note->velocity = prevNote->velocity;
                    note->type = NoteType::tie;
                    beat->notes.push_back(std::move(note));
                }
            }
        }
    } else if ((flags1 & 0x08) == 0) {
        // Normal notes
        uint8_t stringsFlags = reader.readByte(1)[0];
        uint8_t effectsFlags = reader.readByte(1)[0];
        uint8_t graceFlags = reader.readByte(1)[0];

        for (int i = 5; i >= 0; i--) {
            if ((stringsFlags & (1 << i)) != 0) {
                auto note = std::make_unique<GpNote>(beat.get());

                uint8_t fret = reader.readByte(1)[0];
                uint8_t dynamic = reader.readByte(1)[0];

                GpNoteEffect thisNoteEffect = cloneNoteEffect(noteEffect);
                if ((effectsFlags & (1 << i)) != 0) {
                    readNoteEffects(thisNoteEffect);
                }

                note->value = (fret < 100) ? fret : 0;
                note->velocity = (Velocities::minVelocity + (Velocities::velocityIncrement * dynamic)) - Velocities::velocityIncrement;
                note->str = static_cast<int>(track->strings.size()) - i;
                note->effect = std::move(thisNoteEffect);
                note->type = (fret >= 100) ? NoteType::dead : NoteType::normal;

                beat->notes.push_back(std::move(note));
            }

            // Grace note (just skip 3 bytes)
            if ((graceFlags & (1 << i)) != 0) {
                readGraceNote();
            }
        }
    } else {
        // Rest beat
        beat->status = BeatStatus::rest;
    }

    beat->duration = duration;
    beat->voice = voice;
    voice->beats.push_back(std::move(beat));

    return duration.time();
}

void Gp2Parser::readChord(int stringCount, GpBeat* beat) {
    // GP2 always uses full chord format
    auto chord = std::make_unique<Chord>(stringCount);
    chord->name = gpReadStringByte0(reader);
    reader.skip(1);
    if (reader.readInt(1)[0] < 12) {
        reader.skip(32);
    }
    chord->firstFret = reader.readInt(1)[0];
    if (chord->firstFret != 0) {
        for (int i = 0; i < 6; i++) {
            int fret = reader.readInt(1)[0];
            if (i < static_cast<int>(chord->strings.size())) {
                chord->strings[i] = fret;
            }
        }
    }
    if (!chord->notes().empty()) {
        beat->effect.chord = std::move(chord);
    }
}

void Gp2Parser::readMixChange(Tempo& tmpo) {
    uint8_t flags = reader.readByte(1)[0];

    // Tempo
    if ((flags & 0x20) != 0) {
        tmpo.value = reader.readInt(1)[0];
        reader.readByte(1); // transition duration
    }
    // Reverb
    if ((flags & 0x10) != 0) {
        reader.readByte(1); // value
        reader.readByte(1); // transition
    }
    // Chorus
    if ((flags & 0x08) != 0) {
        reader.readByte(1); // value
        reader.readByte(1); // transition
    }
    // Balance
    if ((flags & 0x04) != 0) {
        reader.readByte(1); // value
        reader.readByte(1); // transition
    }
    // Volume
    if ((flags & 0x02) != 0) {
        reader.readByte(1); // value
        reader.readByte(1); // transition
    }
    // Instrument
    if ((flags & 0x01) != 0) {
        reader.readByte(1); // value
    }
}

void Gp2Parser::readGraceNote() {
    reader.skip(3);
}
