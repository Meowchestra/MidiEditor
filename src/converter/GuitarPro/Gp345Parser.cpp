#include "Gp345Parser.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <stdexcept>

// ============================================================
// GP3 Parser — ported from GP3File.cs
// ============================================================

Gp3Parser::Gp3Parser(const std::vector<uint8_t>& data) {
    reader.setData(data);
}

void Gp3Parser::readSong() {
    version = readVersion();
    readVersionTuple();
    readInfo();
    tripletFeel_ = reader.readBool()[0] ? TripletFeel::eigth : TripletFeel::none;
    tempo = reader.readInt()[0];
    key_ = static_cast<KeySignature>(reader.readInt()[0] * 10);
    readMidiChannels();
    measureCount = reader.readInt()[0];
    trackCount = reader.readInt()[0];
    readMeasureHeaders(measureCount);
    readTracks(trackCount);
    readMeasures();
}

std::string Gp3Parser::readVersion() {
    return reader.readByteSizeString(30);
}

void Gp3Parser::readVersionTuple() {
    if (version.size() >= 4) {
        std::string tail = version.substr(version.size() - 4);
        auto dotPos = tail.find('.');
        if (dotPos != std::string::npos) {
            try {
                versionTuple[0] = std::stoi(tail.substr(0, dotPos));
                versionTuple[1] = std::stoi(tail.substr(dotPos + 1));
            } catch (...) {
                versionTuple[0] = 3; versionTuple[1] = 0;
            }
        }
    }
}

void Gp3Parser::addMeasureHeader(std::unique_ptr<MeasureHeader> header) {
    header->song = this;
    auto* ptr = header.get();
    measureHeaders.push_back(std::move(header));
    if (ptr->isRepeatOpen ||
        (!ptr->repeatAlternatives.empty() && currentRepeatGroup_.isClosed &&
         ptr->repeatAlternatives[0] <= 0)) {
        currentRepeatGroup_ = RepeatGroup();
    }
    currentRepeatGroup_.addMeasureHeader(ptr);
}

void Gp3Parser::readInfo() {
    title = reader.readIntByteSizeString();
    subtitle = reader.readIntByteSizeString();
    interpret = reader.readIntByteSizeString();
    album = reader.readIntByteSizeString();
    author = reader.readIntByteSizeString();
    copyright = reader.readIntByteSizeString();
    tab_author = reader.readIntByteSizeString();
    instructional = reader.readIntByteSizeString();
    noticeCount_ = reader.readInt()[0];
    for (int i = 0; i < noticeCount_ && i < 256; i++) {
        notice_[i] = reader.readIntByteSizeString();
    }
}

void Gp3Parser::readLyrics() {
    lyrics.clear();
    Lyrics lyr;
    lyr.trackChoice = reader.readInt()[0];
    for (int i = 0; i < 5; i++) {
        lyr.lines[i].startingMeasure = reader.readInt()[0];
        lyr.lines[i].lyrics = reader.readIntSizeString();
    }
    lyrics.push_back(lyr);
}

void Gp3Parser::readMidiChannels() {
    for (int i = 0; i < 64; i++) {
        channels_[i] = GpMidiChannel();
        channels_[i].channel = i;
        channels_[i].effectChannel = i;
        int instrument = reader.readInt()[0];
        if (channels_[i].isPercussionChannel() && instrument == -1) instrument = 0;
        channels_[i].instrument = instrument;
        channels_[i].volume = toChannelShort(reader.readByte()[0]);
        channels_[i].balance = toChannelShort(reader.readByte()[0]);
        channels_[i].chorus = toChannelShort(reader.readByte()[0]);
        channels_[i].reverb = toChannelShort(reader.readByte()[0]);
        channels_[i].phaser = toChannelShort(reader.readByte()[0]);
        channels_[i].tremolo = toChannelShort(reader.readByte()[0]);
        reader.skip(2);
    }
}

int Gp3Parser::toChannelShort(uint8_t data) {
    int8_t sd = static_cast<int8_t>(data);
    int value = std::max(-32768, std::min(32767, (static_cast<int>(sd) << 3) - 1));
    return std::max(value, -1) + 1;
}

GpMidiChannel Gp3Parser::readChannel() {
    int index = reader.readInt()[0] - 1;
    GpMidiChannel trackChannel;
    int effectChannel = reader.readInt()[0] - 1;
    if (index >= 0 && index < 64) {
        trackChannel = channels_[index];
        if (trackChannel.instrument < 0) trackChannel.instrument = 0;
        if (!trackChannel.isPercussionChannel()) {
            trackChannel.effectChannel = effectChannel;
        }
    }
    return trackChannel;
}

// --- Measure Headers ---

void Gp3Parser::readMeasureHeaders(int count) {
    MeasureHeader* previous = nullptr;
    for (int n = 1; n <= count; n++) {
        auto* header = readMeasureHeader(n, previous);
        previous = header;
    }
}

MeasureHeader* Gp3Parser::readMeasureHeader(int number, MeasureHeader* previous) {
    uint8_t flags = reader.readByte()[0];
    auto header = std::make_unique<MeasureHeader>();
    header->number = number;
    header->start = 0;
    header->tempo.value = tempo;
    header->tripletFeel = tripletFeel_;

    if (flags & 0x01) {
        header->timeSignature.numerator = reader.readSignedByte()[0];
    } else if (previous) {
        header->timeSignature.numerator = previous->timeSignature.numerator;
    }
    if (flags & 0x02) {
        header->timeSignature.denominator.value = reader.readSignedByte()[0];
    } else if (previous) {
        header->timeSignature.denominator.value = previous->timeSignature.denominator.value;
    }

    header->isRepeatOpen = (flags & 0x04) != 0;
    if (flags & 0x08) {
        header->repeatClose = reader.readSignedByte()[0];
    }
    if (flags & 0x10) {
        header->repeatAlternatives.push_back(readRepeatAlternative());
    }
    if (flags & 0x20) {
        header->marker = std::make_unique<Marker>(readMarker(header.get()));
    }
    if (flags & 0x40) {
        int8_t root = reader.readSignedByte()[0];
        int8_t type = reader.readSignedByte()[0];
        int dir = (root < 0) ? -1 : 1;
        header->keySignature = static_cast<KeySignature>(static_cast<int>(root) * 10 + dir * type);
    } else if (number > 1 && previous) {
        header->keySignature = previous->keySignature;
    }

    header->hasDoubleBar = (flags & 0x80) != 0;

    auto* ptr = header.get();
    addMeasureHeader(std::move(header));
    return ptr;
}

int Gp3Parser::readRepeatAlternative() {
    uint8_t value = reader.readByte()[0];
    int existing = 0;
    for (int x = static_cast<int>(measureHeaders.size()) - 1; x >= 0; x--) {
        if (measureHeaders[x]->isRepeatOpen) break;
        if (!measureHeaders[x]->repeatAlternatives.empty())
            existing |= measureHeaders[x]->repeatAlternatives[0];
    }
    return ((1 << value) - 1) ^ existing;
}

Marker Gp3Parser::readMarker(MeasureHeader* header) {
    Marker marker;
    marker.title = reader.readIntByteSizeString();
    marker.color = readColor();
    marker.measureHeader = header;
    return marker;
}

GpColor Gp3Parser::readColor() {
    uint8_t r = reader.readByte()[0];
    uint8_t g = reader.readByte()[0];
    uint8_t b = reader.readByte()[0];
    reader.skip(1);
    return GpColor(r, g, b);
}

// --- Tracks ---

void Gp3Parser::readTracks(int count) {
    for (int i = 0; i < count; i++) {
        auto track = std::make_unique<GpTrack>(this, i + 1);
        readTrack(track.get());
        tracks.push_back(std::move(track));
    }
}

void Gp3Parser::readTrack(GpTrack* track) {
    uint8_t flags = reader.readByte()[0];
    track->isPercussionTrack = (flags & 0x01) != 0;
    track->is12StringedGuitarTrack = (flags & 0x02) != 0;
    track->isBanjoTrack = (flags & 0x04) != 0;
    track->name = reader.readByteSizeString(40);

    int stringCount = reader.readInt()[0];
    for (int i = 0; i < 7; i++) {
        int tuning = reader.readInt()[0];
        if (i < stringCount) {
            track->strings.push_back(GuitarString(i + 1, tuning));
        }
    }

    track->port = reader.readInt()[0];
    track->channel = readChannel();
    if (track->channel.channel == 9) track->isPercussionTrack = true;
    track->fretCount = reader.readInt()[0];
    track->offset = reader.readInt()[0];
    track->color = readColor();
}

// --- Measures ---

void Gp3Parser::readMeasures() {
    int start = Duration::quarterTime;
    for (int i = 0; i < measureCount; i++) {
        auto* header = measureHeaders[i].get();
        header->start = start;
        for (int j = 0; j < trackCount; j++) {
            auto measure = std::make_unique<GpMeasure>(tracks[j].get(), header);
            readMeasure(measure.get());
            tracks[j]->addMeasure(std::move(measure));
        }
        start += header->length();
    }
}

void Gp3Parser::readMeasure(GpMeasure* measure) {
    int start = measure->start();
    auto& voice = measure->voices[0];
    int beatCount = reader.readInt()[0];
    for (int i = 0; i < beatCount; i++) {
        readBeat(start, measure, voice.get(), 0);
        // Advance start by beat duration
        if (!voice->beats.empty()) {
            auto& lastBeat = voice->beats.back();
            start += lastBeat->duration.time();
        }
    }
}

GpBeat* Gp3Parser::getBeat(GpVoice* voice, int start) {
    for (int x = static_cast<int>(voice->beats.size()) - 1; x >= 0; x--) {
        if (voice->beats[x]->start == start) return voice->beats[x].get();
    }
    auto beat = std::make_unique<GpBeat>();
    beat->voice = voice;
    beat->start = start;
    auto* ptr = beat.get();
    voice->beats.push_back(std::move(beat));
    return ptr;
}

void Gp3Parser::readBeat(int start, GpMeasure* measure, GpVoice* voice, int /*voiceIndex*/) {
    uint8_t flags = reader.readByte()[0];
    auto* beat = getBeat(voice, start);

    if (flags & 0x40) {
        uint8_t beatType = reader.readByte()[0];
        beat->status = (beatType == 0) ? BeatStatus::empty : BeatStatus::rest;
    } else {
        beat->status = BeatStatus::normal;
    }

    beat->duration = readDuration(flags);

    if (flags & 0x02) {
        beat->effect.chord = readChord(static_cast<int>(
            measure->track ? measure->track->strings.size() : 6));
    }
    if (flags & 0x04) {
        beat->text = std::make_unique<BeatText>(readText());
    }
    if (flags & 0x08) {
        beat->effect = readBeatEffects(nullptr);
    }
    if (flags & 0x10) {
        readMixTableChange(measure, beat->effect);
    }

    int stringFlags = reader.readByte()[0];
    for (int i = 6; i >= 0; i--) {
        if (stringFlags & (1 << i)) {
            auto note = std::make_unique<GpNote>(beat);
            note->str = 6 - i + 1;
            readNote(note.get(), beat);
            beat->notes.push_back(std::move(note));
        }
    }
}

void Gp3Parser::readNote(GpNote* note, GpBeat* /*beat*/) {
    uint8_t flags = reader.readByte()[0];

    if (flags & 0x20) {
        uint8_t noteType = reader.readByte()[0];
        note->type = static_cast<NoteType>(noteType);
    } else {
        note->type = NoteType::normal;
    }

    if (flags & 0x01) {
        // Duration and tuplet overrides
        note->duration = reader.readSignedByte()[0];
        note->tuplet = reader.readSignedByte()[0];
    }

    if (flags & 0x10) {
        int velocity = reader.readSignedByte()[0];
        note->velocity = Velocities::minVelocity +
            (Velocities::velocityIncrement * velocity) - Velocities::velocityIncrement;
    }

    if (flags & 0x20) {
        int fret = reader.readSignedByte()[0];
        if (note->type == NoteType::normal) {
            note->value = fret;
        }
    }

    if (flags & 0x80) {
        note->effect.leftHandFinger = static_cast<Fingering>(reader.readSignedByte()[0]);
        note->effect.rightHandFinger = static_cast<Fingering>(reader.readSignedByte()[0]);
    }

    if (flags & 0x08) {
        readNoteEffects(note);
    }
}

void Gp3Parser::readNoteEffects(GpNote* note) {
    uint8_t flags = reader.readByte()[0];
    note->effect.hammer = (flags & 0x02) != 0;
    note->effect.letRing = (flags & 0x08) != 0;

    if (flags & 0x01) {
        note->effect.bend = std::make_unique<BendEffect>(readBend());
    }
    if (flags & 0x04) {
        note->effect.slides.push_back(SlideType::shiftSlideTo);
    }
    if (flags & 0x10) {
        note->effect.grace = std::make_unique<GraceEffect>(readGrace());
    }
}

BendEffect Gp3Parser::readBend() {
    BendEffect bend;
    bend.type = static_cast<BendType>(reader.readSignedByte()[0]);
    bend.value = reader.readInt()[0];
    int pointCount = reader.readInt()[0];
    for (int i = 0; i < pointCount; i++) {
        int position = reader.readInt()[0]; // 0..12
        int value = reader.readInt()[0];    // 100 = 1 semitone
        bool vibrato = reader.readBool()[0];
        bend.points.push_back(BendPoint(
            static_cast<int>(std::round(position * BendEffect::maxPosition / static_cast<float>(GPBaseConstants::bendPosition))),
            static_cast<int>(std::round(value * BendEffect::semitoneLength * 2.0f / static_cast<float>(GPBaseConstants::bendSemitone))),
            vibrato
        ));
    }
    return bend;
}

GraceEffect Gp3Parser::readGrace() {
    GraceEffect grace;
    grace.fret = reader.readByte()[0];
    grace.velocity = Velocities::minVelocity +
        (Velocities::velocityIncrement * reader.readByte()[0]) - Velocities::velocityIncrement;
    grace.duration = reader.readByte()[0];
    int8_t transition = reader.readSignedByte()[0];
    grace.transition = static_cast<GraceEffectTransition>(transition);
    return grace;
}

BeatEffect Gp3Parser::readBeatEffects(GpNoteEffect* /*effect*/) {
    BeatEffect beatEffect;
    uint8_t flags = reader.readByte()[0];
    beatEffect.vibrato = (flags & 0x01) != 0;

    if (flags & 0x20) {
        int8_t slapValue = reader.readSignedByte()[0];
        if (slapValue == 0) {
            // GP3 tremoloBar: simplified dip with just a value, not full bend curve
            auto tremoloBar = std::make_unique<BendEffect>();
            tremoloBar->type = BendType::dip;
            tremoloBar->value = reader.readInt()[0];
            tremoloBar->points.push_back(BendPoint(0, 0, false));
            tremoloBar->points.push_back(BendPoint(
                static_cast<int>(std::round(BendEffect::maxPosition / 2.0f)),
                static_cast<int>(std::round(tremoloBar->value * BendEffect::semitoneLength * 2.0f / static_cast<float>(GPBaseConstants::bendSemitone))),
                false));
            tremoloBar->points.push_back(BendPoint(BendEffect::maxPosition, 0, false));
            beatEffect.tremoloBar = std::move(tremoloBar);
        } else {
            beatEffect.slapEffect = static_cast<SlapEffect>(slapValue);
            reader.readInt(); // skip value
        }
    }
    if (flags & 0x40) {
        beatEffect.stroke = std::make_unique<BeatStroke>(readBeatStroke());
    }
    return beatEffect;
}

BeatStroke Gp3Parser::readBeatStroke() {
    int8_t strokeDown = reader.readSignedByte()[0];
    int8_t strokeUp = reader.readSignedByte()[0];
    BeatStroke result;
    if (strokeUp > 0) {
        result = BeatStroke(BeatStrokeDirection::up, toStrokeValue(strokeUp), 0.0f);
    } else {
        result = BeatStroke(BeatStrokeDirection::down, toStrokeValue(strokeDown), 0.0f);
    }
    return result.swapDirection();
}

int Gp3Parser::toStrokeValue(int8_t value) {
    switch (value) {
        case 1: return Duration::hundredTwentyEigth;
        case 2: return Duration::sixtyFourth;
        case 3: return Duration::thirtySecond;
        case 4: return Duration::sixteenth;
        case 5: return Duration::eigth;
        case 6: return Duration::quarter;
        default: return Duration::sixtyFourth;
    }
}

void Gp3Parser::readMixTableChange(GpMeasure* measure, BeatEffect& beatEffect) {
    auto tc = std::make_unique<MixTableChange>();
    int instrument = reader.readSignedByte()[0];
    int volume = reader.readSignedByte()[0];
    int balance = reader.readSignedByte()[0];
    int chorus = reader.readSignedByte()[0];
    int reverb = reader.readSignedByte()[0];
    int phaser = reader.readSignedByte()[0];
    int tremolo = reader.readSignedByte()[0];
    int tempoVal = reader.readInt()[0];

    if (instrument >= 0) tc->instrument = std::make_unique<MixTableItem>(instrument);
    if (volume >= 0) tc->volume = std::make_unique<MixTableItem>(volume);
    if (balance >= 0) tc->balance = std::make_unique<MixTableItem>(balance);
    if (chorus >= 0) tc->chorus = std::make_unique<MixTableItem>(chorus);
    if (reverb >= 0) tc->reverb = std::make_unique<MixTableItem>(reverb);
    if (phaser >= 0) tc->phaser = std::make_unique<MixTableItem>(phaser);
    if (tremolo >= 0) tc->tremolo = std::make_unique<MixTableItem>(tremolo);
    if (tempoVal >= 0) {
        tc->tempo = std::make_unique<MixTableItem>(tempoVal);
        if (measure) measure->tempo().value = tempoVal;
    }

    // Read durations
    if (tc->volume) tc->volume->duration = reader.readSignedByte()[0];
    if (tc->balance) tc->balance->duration = reader.readSignedByte()[0];
    if (tc->chorus) tc->chorus->duration = reader.readSignedByte()[0];
    if (tc->reverb) tc->reverb->duration = reader.readSignedByte()[0];
    if (tc->phaser) tc->phaser->duration = reader.readSignedByte()[0];
    if (tc->tremolo) tc->tremolo->duration = reader.readSignedByte()[0];
    if (tc->tempo) tc->tempo->duration = reader.readSignedByte()[0];

    beatEffect.mixTableChange = std::move(tc);
}

Duration Gp3Parser::readDuration(uint8_t flags) {
    Duration dur;
    dur.value = 1 << (reader.readSignedByte()[0] + 2);
    dur.isDotted = (flags & 0x01) != 0;
    if (flags & 0x20) {
        int tuplet = reader.readInt()[0];
        switch (tuplet) {
            case 3:  dur.tuplet.enters = 3;  dur.tuplet.times = 2; break;
            case 5:  dur.tuplet.enters = 5;  dur.tuplet.times = 4; break;
            case 6:  dur.tuplet.enters = 6;  dur.tuplet.times = 4; break;
            case 7:  dur.tuplet.enters = 7;  dur.tuplet.times = 4; break;
            case 9:  dur.tuplet.enters = 9;  dur.tuplet.times = 8; break;
            case 10: dur.tuplet.enters = 10; dur.tuplet.times = 8; break;
            case 11: dur.tuplet.enters = 11; dur.tuplet.times = 8; break;
            case 12: dur.tuplet.enters = 12; dur.tuplet.times = 8; break;
        }
    }
    return dur;
}

std::unique_ptr<Chord> Gp3Parser::readChord(int stringCount) {
    auto chord = std::make_unique<Chord>(stringCount);
    chord->newFormat = reader.readBool()[0];
    if (!chord->newFormat) {
        readOldChord(*chord);
    } else {
        readNewChord(*chord);
    }
    if (!chord->notes().empty()) return chord;
    return nullptr;
}

void Gp3Parser::readOldChord(Chord& chord) {
    chord.name = reader.readIntByteSizeString();
    chord.firstFret = reader.readInt()[0];
    if (chord.firstFret > 0) {
        for (int i = 0; i < 6; i++) {
            int fret = reader.readInt()[0];
            if (i < static_cast<int>(chord.strings.size())) chord.strings[i] = fret;
        }
    }
}

void Gp3Parser::readNewChord(Chord& chord) {
    // GP3 uses readInt for most fields (4 bytes each)
    chord.sharp = reader.readBool()[0];
    reader.skip(3);
    chord.root = PitchClass(reader.readInt()[0], -1);
    chord.chordType = static_cast<ChordType>(reader.readInt()[0]);
    chord.extension = static_cast<ChordExtension>(reader.readInt()[0]);
    chord.bass = PitchClass(reader.readInt()[0], -1);
    chord.tonality = static_cast<ChordAlteration>(reader.readInt()[0]);
    chord.add = reader.readBool()[0];
    chord.name = reader.readByteSizeString(22);
    chord.fifth = static_cast<ChordAlteration>(reader.readInt()[0]);
    chord.ninth = static_cast<ChordAlteration>(reader.readInt()[0]);
    chord.eleventh = static_cast<ChordAlteration>(reader.readInt()[0]);
    chord.firstFret = reader.readInt()[0];
    for (int i = 0; i < 6; i++) {
        int fret = reader.readInt()[0];
        if (i < static_cast<int>(chord.strings.size())) chord.strings[i] = fret;
    }
    chord.barres.clear();
    int barresCount = reader.readInt()[0];
    auto barreFrets = reader.readInt(2);
    auto barreStarts = reader.readInt(2);
    auto barreEnds = reader.readInt(2);
    for (int x = 0; x < std::min(2, barresCount); x++) {
        chord.barres.push_back(Chord::Barre(barreFrets[x], barreStarts[x], barreEnds[x]));
    }
    chord.omissions = reader.readBool(7);
    reader.skip(1);
}

BeatText Gp3Parser::readText() {
    BeatText text;
    text.value = reader.readIntByteSizeString();
    return text;
}

// ============================================================
// GP4 Parser — ported from GP4File.cs
// ============================================================

Gp4Parser::Gp4Parser(const std::vector<uint8_t>& data) : Gp3Parser(data) {}

void Gp4Parser::readSong() {
    version = readVersion();
    readVersionTuple();
    readInfo();
    tripletFeel_ = reader.readBool()[0] ? TripletFeel::eigth : TripletFeel::none;
    readLyrics();
    tempo = reader.readInt()[0];
    key_ = static_cast<KeySignature>(reader.readInt()[0] * 10);
    reader.readSignedByte(); // octave
    readMidiChannels();
    measureCount = reader.readInt()[0];
    trackCount = reader.readInt()[0];
    readMeasureHeaders(measureCount);
    readTracks(trackCount);
    readMeasures();
}

void Gp4Parser::readInfo() {
    Gp3Parser::readInfo();
}

void Gp4Parser::readNoteEffects(GpNote* note) {
    uint8_t flags1 = reader.readByte()[0];
    uint8_t flags2 = reader.readByte()[0];

    note->effect.hammer = (flags1 & 0x02) != 0;
    note->effect.letRing = (flags1 & 0x08) != 0;

    if (flags1 & 0x01) {
        note->effect.bend = std::make_unique<BendEffect>(readBend());
    }
    if (flags1 & 0x10) {
        note->effect.grace = std::make_unique<GraceEffect>(readGrace());
    }

    // GP4 flags2
    if (flags2 & 0x01) {
        note->effect.staccato = true;
    }
    if (flags2 & 0x02) {
        note->effect.palmMute = true;
    }
    if (flags2 & 0x04) {
        note->effect.tremoloPicking = std::make_unique<TremoloPickingEffect>();
        int8_t val = reader.readSignedByte()[0];
        switch (val) {
            case 1: note->effect.tremoloPicking->duration.value = 8; break;
            case 2: note->effect.tremoloPicking->duration.value = 16; break;
            case 3: note->effect.tremoloPicking->duration.value = 32; break;
        }
    }
    if (flags2 & 0x08) {
        int8_t slideVal = reader.readSignedByte()[0];
        note->effect.slides.push_back(static_cast<SlideType>(slideVal));
    }
    if (flags2 & 0x10) {
        int8_t harmonicType = reader.readSignedByte()[0];
        switch (harmonicType) {
            case 1: note->effect.harmonic = std::make_unique<NaturalHarmonic>(); break;
            case 3: note->effect.harmonic = std::make_unique<TappedHarmonic>(); break;
            case 4: note->effect.harmonic = std::make_unique<PinchHarmonic>(); break;
            case 5: note->effect.harmonic = std::make_unique<SemiHarmonic>(); break;
            case 15:
            case 17:
            case 22:
                note->effect.harmonic = std::make_unique<ArtificialHarmonic>();
                break;
            default: note->effect.harmonic = std::make_unique<NaturalHarmonic>(); break;
        }
    }
    if (flags2 & 0x20) {
        note->effect.trill = std::make_unique<TrillEffect>();
        note->effect.trill->fret = reader.readSignedByte()[0];
        int8_t period = reader.readSignedByte()[0];
        switch (period) {
            case 1: note->effect.trill->duration.value = 4; break;
            case 2: note->effect.trill->duration.value = 8; break;
            case 3: note->effect.trill->duration.value = 16; break;
        }
    }
    if (flags2 & 0x40) {
        note->effect.vibrato = true;
    }
}

BeatEffect Gp4Parser::readBeatEffects(GpNoteEffect* /*effect*/) {
    BeatEffect beatEffect;
    int8_t flags1 = reader.readSignedByte()[0];
    int8_t flags2 = reader.readSignedByte()[0];

    beatEffect.vibrato = (flags1 & 0x02) != 0;
    beatEffect.fadeIn = (flags1 & 0x10) != 0;

    if (flags1 & 0x20) {
        int8_t slapValue = reader.readSignedByte()[0];
        beatEffect.slapEffect = static_cast<SlapEffect>(slapValue);
    }
    if (flags2 & 0x04) {
        beatEffect.tremoloBar = std::make_unique<BendEffect>(readBend());
    }
    if (flags1 & 0x40) {
        beatEffect.stroke = std::make_unique<BeatStroke>(readBeatStroke());
    }
    if (flags2 & 0x02) {
        int8_t direction = reader.readSignedByte()[0];
        beatEffect.pickStroke = static_cast<BeatStrokeDirection>(direction);
    }
    return beatEffect;
}

void Gp4Parser::readMixTableChange(GpMeasure* measure, BeatEffect& beatEffect) {
    auto tc = std::make_unique<MixTableChange>();
    int instrument = reader.readSignedByte()[0];
    int volume = reader.readSignedByte()[0];
    int balance = reader.readSignedByte()[0];
    int chorus = reader.readSignedByte()[0];
    int reverb = reader.readSignedByte()[0];
    int phaser = reader.readSignedByte()[0];
    int tremolo = reader.readSignedByte()[0];
    int tempoVal = reader.readInt()[0];

    if (instrument >= 0) tc->instrument = std::make_unique<MixTableItem>(instrument);
    if (volume >= 0) tc->volume = std::make_unique<MixTableItem>(volume);
    if (balance >= 0) tc->balance = std::make_unique<MixTableItem>(balance);
    if (chorus >= 0) tc->chorus = std::make_unique<MixTableItem>(chorus);
    if (reverb >= 0) tc->reverb = std::make_unique<MixTableItem>(reverb);
    if (phaser >= 0) tc->phaser = std::make_unique<MixTableItem>(phaser);
    if (tremolo >= 0) tc->tremolo = std::make_unique<MixTableItem>(tremolo);
    if (tempoVal >= 0) {
        tc->tempo = std::make_unique<MixTableItem>(tempoVal);
        if (measure) measure->tempo().value = tempoVal;
    }

    readMixTableChangeDurations(tc.get());

    // GP4+ reads allTracks flags byte after durations
    int8_t allTracksFlags = reader.readSignedByte()[0];
    if (tc->volume) tc->volume->allTracks = (allTracksFlags & 0x01) != 0;
    if (tc->balance) tc->balance->allTracks = (allTracksFlags & 0x02) != 0;
    if (tc->chorus) tc->chorus->allTracks = (allTracksFlags & 0x04) != 0;
    if (tc->reverb) tc->reverb->allTracks = (allTracksFlags & 0x08) != 0;
    if (tc->phaser) tc->phaser->allTracks = (allTracksFlags & 0x10) != 0;
    if (tc->tremolo) tc->tremolo->allTracks = (allTracksFlags & 0x20) != 0;

    beatEffect.mixTableChange = std::move(tc);
}

void Gp4Parser::readMixTableChangeDurations(MixTableChange* tc) {
    if (tc->volume) tc->volume->duration = reader.readSignedByte()[0];
    if (tc->balance) tc->balance->duration = reader.readSignedByte()[0];
    if (tc->chorus) tc->chorus->duration = reader.readSignedByte()[0];
    if (tc->reverb) tc->reverb->duration = reader.readSignedByte()[0];
    if (tc->phaser) tc->phaser->duration = reader.readSignedByte()[0];
    if (tc->tremolo) tc->tremolo->duration = reader.readSignedByte()[0];
    if (tc->tempo) {
        tc->tempo->duration = reader.readSignedByte()[0];
    }
}

void Gp4Parser::readNewChord(Chord& chord) {
    // GP4 uses readByte (1 byte) for root/type/extension/fifth/ninth/eleventh
    // GP3 used readInt (4 bytes) for those same fields
    chord.sharp = reader.readBool()[0];
    reader.skip(3);
    chord.root = PitchClass(reader.readByte()[0], -1);
    chord.chordType = static_cast<ChordType>(reader.readByte()[0]);
    chord.extension = static_cast<ChordExtension>(reader.readByte()[0]);
    chord.bass = PitchClass(reader.readInt()[0], -1);
    chord.tonality = static_cast<ChordAlteration>(reader.readInt()[0]);
    chord.add = reader.readBool()[0];
    chord.name = reader.readByteSizeString(22);
    chord.fifth = static_cast<ChordAlteration>(reader.readByte()[0]);
    chord.ninth = static_cast<ChordAlteration>(reader.readByte()[0]);
    chord.eleventh = static_cast<ChordAlteration>(reader.readByte()[0]);
    chord.firstFret = reader.readInt()[0];
    for (int i = 0; i < 7; i++) {
        int fret = reader.readInt()[0];
        if (i < static_cast<int>(chord.strings.size())) chord.strings[i] = fret;
    }
    chord.barres.clear();
    int barresCount = reader.readByte()[0];
    auto barreFrets = reader.readByte(5);
    auto barreStarts = reader.readByte(5);
    auto barreEnds = reader.readByte(5);
    for (int x = 0; x < std::min(5, barresCount); x++) {
        chord.barres.push_back(Chord::Barre(barreFrets[x], barreStarts[x], barreEnds[x]));
    }
    chord.omissions = reader.readBool(7);
    reader.skip(1);
    for (int x = 0; x < 7; x++) {
        chord.fingerings.push_back(static_cast<Fingering>(reader.readSignedByte()[0]));
    }
    chord.show = reader.readBool()[0];
}

// ============================================================
// GP5 Parser — ported from GP5File.cs
// ============================================================

Gp5Parser::Gp5Parser(const std::vector<uint8_t>& data) : Gp4Parser(data) {}

void Gp5Parser::readSong() {
    version = readVersion();
    readVersionTuple();
    readInfo();
    readLyrics();

    // GP5: RSE master effect (volume + equalizer, only v5.1+)
    readRSEMasterEffect();

    // GP5 page setup
    readPageSetup();

    tempoName = reader.readIntByteSizeString();
    tempo = reader.readInt()[0];
    if (versionTuple[1] > 0) {
        hideTempo = reader.readBool()[0];
    }

    key_ = static_cast<KeySignature>(reader.readSignedByte()[0] * 10);
    reader.readInt(); // octave (GP5 uses readInt, not readSignedByte)

    readMidiChannels();
    readDirections();

    // GP5: master reverb
    reader.readInt(); // masterEffect.reverb

    measureCount = reader.readInt()[0];
    trackCount = reader.readInt()[0];

    readMeasureHeaders(measureCount);
    readTracks(trackCount);
    readMeasures();
}

void Gp5Parser::readDirections() {
    directionsGp5_.clear();
    // GP5 direction signs: Coda, Double Coda, Segno, Segno Segno, Fine,
    // Da Capo, Da Capo al Coda, Da Capo al Double Coda, Da Capo al Fine,
    // Da Segno, Da Segno al Coda, Da Segno al Double Coda, Da Segno al Fine,
    // Da Segno Segno, Da Segno Segno al Coda, Da Segno Segno al Double Coda,
    // Da Segno Segno al Fine, Da Coda, Da Double Coda
    std::vector<std::string> names = {
        "Coda", "Double Coda", "Segno", "Segno Segno", "Fine",
        "Da Capo", "Da Capo al Coda", "Da Capo al Double Coda", "Da Capo al Fine",
        "Da Segno", "Da Segno al Coda", "Da Segno al Double Coda", "Da Segno al Fine",
        "Da Segno Segno", "Da Segno Segno al Coda", "Da Segno Segno al Double Coda",
        "Da Segno Segno al Fine", "Da Coda", "Da Double Coda"
    };
    for (const auto& name : names) {
        int m = reader.readShort()[0];
        directionsGp5_.push_back(DirectionSign(name, m));
    }
}

void Gp5Parser::readPageSetup() {
    pageSetup = std::make_unique<PageSetup>();
    pageSetup->pageSize.x = reader.readInt()[0];
    pageSetup->pageSize.y = reader.readInt()[0];
    pageSetup->pageMargin.left = reader.readInt()[0];
    pageSetup->pageMargin.right = reader.readInt()[0];
    pageSetup->pageMargin.top = reader.readInt()[0];
    pageSetup->pageMargin.bottom = reader.readInt()[0];
    pageSetup->scoreSizeProportion = reader.readInt()[0] / 100.0f;
    pageSetup->headerAndFooter = reader.readShort()[0];

    pageSetup->title = reader.readIntByteSizeString();
    pageSetup->subtitle = reader.readIntByteSizeString();
    pageSetup->artist = reader.readIntByteSizeString();
    pageSetup->album = reader.readIntByteSizeString();
    pageSetup->words = reader.readIntByteSizeString();
    pageSetup->music = reader.readIntByteSizeString();
    pageSetup->wordsAndMusic = reader.readIntByteSizeString();
    pageSetup->copyright = reader.readIntByteSizeString();
    pageSetup->copyright += "\n" + reader.readIntByteSizeString();
    pageSetup->pageNumber = reader.readIntByteSizeString();
}

void Gp5Parser::readInfo() {
    // GP5 reads 10 fields (adds 'words' and 'music' vs GP3/GP4's 8)
    title = reader.readIntByteSizeString();
    subtitle = reader.readIntByteSizeString();
    interpret = reader.readIntByteSizeString();
    album = reader.readIntByteSizeString();
    words = reader.readIntByteSizeString();
    music = reader.readIntByteSizeString();
    copyright = reader.readIntByteSizeString();
    tab_author = reader.readIntByteSizeString();
    instructional = reader.readIntByteSizeString();
    noticeCount_ = reader.readInt()[0];
    for (int i = 0; i < noticeCount_ && i < 256; i++) {
        notice_[i] = reader.readIntByteSizeString();
    }
}

void Gp5Parser::readRSEMasterEffect() {
    if (versionTuple[1] <= 0) return;
    reader.readInt(); // volume
    reader.readInt(); // unknown
    readEqualizer(11);
}

void Gp5Parser::readEqualizer(int knobsCount) {
    // Read knobsCount - 1 band values + 1 gain value (all signed bytes)
    for (int x = 0; x < knobsCount; x++) {
        reader.readSignedByte(); // band value or gain
    }
}

// --- GP5 Measure Headers ---

void Gp5Parser::readMeasureHeaders(int count) {
    MeasureHeader* previous = nullptr;
    for (int n = 1; n <= count; n++) {
        auto* header = readMeasureHeader(n, previous);
        previous = header;
    }

    // Apply GP5 directions
    int fromPosition = 5;
    for (int x = 0; x < fromPosition && x < static_cast<int>(directionsGp5_.size()); x++) {
        if (directionsGp5_[x].measure > -1 &&
            directionsGp5_[x].measure - 1 < static_cast<int>(measureHeaders.size())) {
            measureHeaders[directionsGp5_[x].measure - 1]->direction.push_back(directionsGp5_[x].name);
        }
    }
    for (int x = fromPosition; x < static_cast<int>(directionsGp5_.size()); x++) {
        if (directionsGp5_[x].measure > -1 &&
            directionsGp5_[x].measure - 1 < static_cast<int>(measureHeaders.size())) {
            measureHeaders[directionsGp5_[x].measure - 1]->fromDirection.push_back(directionsGp5_[x].name);
        }
    }
}

MeasureHeader* Gp5Parser::readMeasureHeader(int number, MeasureHeader* previous) {
    if (previous) reader.skip(1);

    uint8_t flags = reader.readByte()[0];
    auto header = std::make_unique<MeasureHeader>();
    header->number = number;
    header->start = 0;
    header->tempo.value = tempo;
    header->tripletFeel = tripletFeel_;

    if (flags & 0x01) {
        header->timeSignature.numerator = reader.readSignedByte()[0];
    } else if (previous) {
        header->timeSignature.numerator = previous->timeSignature.numerator;
    }
    if (flags & 0x02) {
        header->timeSignature.denominator.value = reader.readSignedByte()[0];
    } else if (previous) {
        header->timeSignature.denominator.value = previous->timeSignature.denominator.value;
    }

    header->isRepeatOpen = (flags & 0x04) != 0;
    if (flags & 0x08) {
        header->repeatClose = reader.readSignedByte()[0];
    }
    if (flags & 0x10) {
        header->repeatAlternatives.push_back(readRepeatAlternativeGp5());
    }
    if (flags & 0x20) {
        header->marker = std::make_unique<Marker>(readMarker(header.get()));
    }
    if (flags & 0x40) {
        int8_t root = reader.readSignedByte()[0];
        int8_t type = reader.readSignedByte()[0];
        int dir = (root < 0) ? -1 : 1;
        header->keySignature = static_cast<KeySignature>(static_cast<int>(root) * 10 + dir * type);
    } else if (number > 1 && previous) {
        header->keySignature = previous->keySignature;
    }

    header->hasDoubleBar = (flags & 0x80) != 0;

    // GP5 extras
    if (header->repeatClose > -1) header->repeatClose -= 1;
    if (flags & 0x03) {
        auto beams = reader.readByte(4);
        for (int i = 0; i < 4; i++) header->timeSignature.beams[i] = beams[i];
    } else if (previous) {
        for (int i = 0; i < 4; i++)
            header->timeSignature.beams[i] = previous->timeSignature.beams[i];
    }
    if (!(flags & 0x10)) reader.skip(1);
    header->tripletFeel = static_cast<TripletFeel>(reader.readByte()[0]);

    auto* ptr = header.get();
    addMeasureHeader(std::move(header));
    return ptr;
}

int Gp5Parser::readRepeatAlternativeGp5() {
    return reader.readByte()[0];
}

// --- GP5 Tracks ---

void Gp5Parser::readTracks(int count) {
    for (int i = 0; i < count; i++) {
        auto track = std::make_unique<GpTrack>(this, i + 1);
        readTrack(track.get());
        tracks.push_back(std::move(track));
    }
    reader.skip((versionTuple[1] == 0) ? 2 : 1);
}

void Gp5Parser::readTrack(GpTrack* track) {
    if (track->number == 1 || versionTuple[1] == 0) reader.skip(1);

    uint8_t flags1 = reader.readByte()[0];
    track->isPercussionTrack = (flags1 & 0x01) != 0;
    track->is12StringedGuitarTrack = (flags1 & 0x02) != 0;
    track->isBanjoTrack = (flags1 & 0x04) != 0;
    track->isVisible = (flags1 & 0x08) != 0;
    track->isSolo = (flags1 & 0x10) != 0;
    track->isMute = (flags1 & 0x20) != 0;
    track->useRSE = (flags1 & 0x40) != 0;
    track->indicateTuning = (flags1 & 0x80) != 0;

    track->name = reader.readByteSizeString(40);
    int stringCount = reader.readInt()[0];
    for (int i = 0; i < 7; i++) {
        int tuning = reader.readInt()[0];
        if (i < stringCount) {
            track->strings.push_back(GuitarString(i + 1, tuning));
        }
    }

    track->port = reader.readInt()[0];
    track->channel = readChannel();
    if (track->channel.channel == 9) track->isPercussionTrack = true;
    track->fretCount = reader.readInt()[0];
    track->offset = reader.readInt()[0];
    track->color = readColor();

    // GP5 track settings
    int16_t flags2 = reader.readShort()[0];
    track->settings.tablature = (flags2 & 0x0001) != 0;
    track->settings.notation = (flags2 & 0x0002) != 0;
    track->settings.diagramsAreBelow = (flags2 & 0x0004) != 0;
    track->settings.showRhythm = (flags2 & 0x0008) != 0;
    track->settings.forceHorizontal = (flags2 & 0x0010) != 0;
    track->settings.forceChannels = (flags2 & 0x0020) != 0;
    track->settings.diagramList = (flags2 & 0x0040) != 0;
    track->settings.diagramsInScore = (flags2 & 0x0080) != 0;
    track->settings.autoLetRing = (flags2 & 0x0200) != 0;
    track->settings.autoBrush = (flags2 & 0x0400) != 0;
    track->settings.extendRhythmic = (flags2 & 0x0800) != 0;

    track->rse = std::make_unique<TrackRSE>();
    track->rse->autoAccentuation = static_cast<Accentuation>(reader.readByte()[0]);
    track->channel.bank = reader.readByte()[0];
    readTrackRSE(track->rse.get());
}

void Gp5Parser::readTrackRSE(TrackRSE* rse) {
    rse->humanize = reader.readByte()[0];
    reader.readInt(3);
    reader.skip(12);
    rse->instrument = std::make_unique<RSEInstrument>(readRSEInstrument());
    if (versionTuple[1] >= 10) reader.skip(4);
    if (versionTuple[1] > 0) {
        readRSEInstrumentEffect(rse->instrument.get());
    }
}

RSEInstrument Gp5Parser::readRSEInstrument() {
    RSEInstrument inst;
    inst.instrument = reader.readInt()[0];
    inst.unknown = reader.readInt()[0];
    inst.soundBank = reader.readInt()[0];
    if (versionTuple[1] == 0) {
        inst.effectNumber = reader.readShort()[0];
        reader.skip(1);
    } else {
        inst.effectNumber = reader.readInt()[0];
    }
    return inst;
}

void Gp5Parser::readRSEInstrumentEffect(RSEInstrument* rse) {
    if (versionTuple[1] > 0) {
        rse->effect = reader.readIntByteSizeString();
        rse->effectCategory = reader.readIntByteSizeString();
    }
}

// --- GP5 Measures ---

void Gp5Parser::readMeasures() {
    int start = Duration::quarterTime;
    for (int i = 0; i < measureCount; i++) {
        auto* header = measureHeaders[i].get();
        header->start = start;
        for (int j = 0; j < trackCount; j++) {
            auto measure = std::make_unique<GpMeasure>(tracks[j].get(), header);
            readMeasure(measure.get());
            tracks[j]->addMeasure(std::move(measure));
        }
        start += header->length();
    }
}

void Gp5Parser::readMeasure(GpMeasure* measure) {
    // GP5 has two voices
    for (int voiceIdx = 0; voiceIdx < 2; voiceIdx++) {
        int start = measure->start();
        int beatCount = reader.readInt()[0];
        auto* voice = measure->voices[voiceIdx].get();
        for (int i = 0; i < beatCount; i++) {
            readBeat(start, measure, voice, voiceIdx);
            if (!voice->beats.empty()) {
                start += voice->beats.back()->duration.time();
            }
        }
    }

    // GP5: linebreak byte (always present, unless at EOF for the very last entry)
    if (reader.getPointer() < reader.dataSize()) {
        measure->lineBreak = static_cast<LineBreak>(reader.readByte()[0]);
    }
}

void Gp5Parser::readBeat(int start, GpMeasure* measure, GpVoice* voice, int voiceIndex) {
    uint8_t flags = reader.readByte()[0];
    auto* beat = getBeat(voice, start);

    if (flags & 0x40) {
        uint8_t beatType = reader.readByte()[0];
        beat->status = (beatType == 0) ? BeatStatus::empty : BeatStatus::rest;
    } else {
        beat->status = BeatStatus::normal;
    }

    beat->duration = readDuration(flags);

    if (flags & 0x02) {
        beat->effect.chord = readChord(static_cast<int>(
            measure->track ? measure->track->strings.size() : 6));
    }
    if (flags & 0x04) {
        beat->text = std::make_unique<BeatText>(readText());
    }
    if (flags & 0x08) {
        beat->effect = readBeatEffects(nullptr);
    }
    if (flags & 0x10) {
        readMixTableChange(measure, beat->effect);
    }

    int stringFlags = reader.readByte()[0];
    for (int i = 6; i >= 0; i--) {
        if (stringFlags & (1 << i)) {
            auto note = std::make_unique<GpNote>(beat);
            note->str = 6 - i + 1;
            readNote(note.get(), beat);
            beat->notes.push_back(std::move(note));
        }
    }

    // GP5 extras: read beat display flags
    int16_t flags2 = reader.readShort()[0];
    if (flags2 & 0x0800) {
        reader.readByte(); // breakSecondary
    }
}

void Gp5Parser::readNote(GpNote* note, GpBeat* beat) {
    uint8_t flags = reader.readByte()[0];

    note->effect.heavyAccentuatedNote = (flags & 0x02) != 0;
    note->effect.ghostNote = (flags & 0x04) != 0;
    note->effect.accentuatedNote = (flags & 0x40) != 0;

    if (flags & 0x20) {
        uint8_t noteType = reader.readByte()[0];
        note->type = static_cast<NoteType>(noteType);
    } else {
        note->type = NoteType::normal;
    }

    if (flags & 0x10) {
        int velocity = reader.readSignedByte()[0];
        note->velocity = Velocities::minVelocity +
            (Velocities::velocityIncrement * velocity) - Velocities::velocityIncrement;
    }

    if (flags & 0x20) {
        int fret = reader.readSignedByte()[0];
        if (note->type == NoteType::normal) {
            note->value = fret;
        }
    }

    if (flags & 0x80) {
        note->effect.leftHandFinger = static_cast<Fingering>(reader.readSignedByte()[0]);
        note->effect.rightHandFinger = static_cast<Fingering>(reader.readSignedByte()[0]);
    }

    if (flags & 0x01) {
        // GP5: durationPercent comes AFTER fingering (not before velocity)
        note->durationPercent = reader.readDouble()[0];
    }

    // GP5: extra flags2 byte before note effects
    uint8_t flags2 = reader.readByte()[0];
    note->swapAccidentals = (flags2 & 0x02) != 0;

    if (flags & 0x08) {
        readNoteEffects(note);
    }
}

void Gp5Parser::readNoteEffects(GpNote* note) {
    uint8_t flags1 = reader.readByte()[0];
    uint8_t flags2 = reader.readByte()[0];

    note->effect.hammer = (flags1 & 0x02) != 0;
    note->effect.letRing = (flags1 & 0x08) != 0;

    if (flags1 & 0x01) {
        note->effect.bend = std::make_unique<BendEffect>(readBend());
    }
    if (flags1 & 0x10) {
        note->effect.grace = std::make_unique<GraceEffect>(readGrace());
    }

    if (flags2 & 0x01) {
        note->effect.staccato = true;
    }
    if (flags2 & 0x02) {
        note->effect.palmMute = true;
    }
    if (flags2 & 0x04) {
        note->effect.tremoloPicking = std::make_unique<TremoloPickingEffect>();
        int8_t val = reader.readSignedByte()[0];
        switch (val) {
            case 1: note->effect.tremoloPicking->duration.value = 8; break;
            case 2: note->effect.tremoloPicking->duration.value = 16; break;
            case 3: note->effect.tremoloPicking->duration.value = 32; break;
        }
    }
    if (flags2 & 0x08) {
        // GP5: unsigned byte interpreted as bitmask
        uint8_t slideVal = reader.readByte()[0];
        if (slideVal & 0x01) note->effect.slides.push_back(SlideType::shiftSlideTo);
        if (slideVal & 0x02) note->effect.slides.push_back(SlideType::legatoSlideTo);
        if (slideVal & 0x04) note->effect.slides.push_back(SlideType::outDownwards);
        if (slideVal & 0x08) note->effect.slides.push_back(SlideType::outUpwards);
        if (slideVal & 0x10) note->effect.slides.push_back(SlideType::intoFromBelow);
        if (slideVal & 0x20) note->effect.slides.push_back(SlideType::intoFromAbove);
    }
    if (flags2 & 0x10) {
        // GP5 harmonic: different from GP4 (case 2 = artificial, case 3 = tapped with fret)
        int8_t harmonicType = reader.readSignedByte()[0];
        switch (harmonicType) {
            case 1: note->effect.harmonic = std::make_unique<NaturalHarmonic>(); break;
            case 2: {
                // GP5: ArtificialHarmonic with pitch class + octave (3 extra bytes)
                uint8_t semitone = reader.readByte()[0];
                int8_t accidental = reader.readSignedByte()[0];
                uint8_t octave = reader.readByte()[0];
                PitchClass pc(semitone, accidental);
                note->effect.harmonic = std::make_unique<ArtificialHarmonic>(pc, static_cast<Octave>(octave));
                break;
            }
            case 3: {
                // GP5: TappedHarmonic with fret (1 extra byte)
                uint8_t fret = reader.readByte()[0];
                auto th = std::make_unique<TappedHarmonic>();
                th->fret = fret;
                note->effect.harmonic = std::move(th);
                break;
            }
            case 4: note->effect.harmonic = std::make_unique<PinchHarmonic>(); break;
            case 5: note->effect.harmonic = std::make_unique<SemiHarmonic>(); break;
            case 15:
            case 17:
            case 22:
                note->effect.harmonic = std::make_unique<ArtificialHarmonic>();
                break;
            default: note->effect.harmonic = std::make_unique<NaturalHarmonic>(); break;
        }
    }
    if (flags2 & 0x20) {
        note->effect.trill = std::make_unique<TrillEffect>();
        note->effect.trill->fret = reader.readSignedByte()[0];
        int8_t period = reader.readSignedByte()[0];
        switch (period) {
            case 1: note->effect.trill->duration.value = 4; break;
            case 2: note->effect.trill->duration.value = 8; break;
            case 3: note->effect.trill->duration.value = 16; break;
        }
    }
    if (flags2 & 0x40) {
        note->effect.vibrato = true;
    }
}

BeatEffect Gp5Parser::readBeatEffects(GpNoteEffect* effect) {
    return Gp4Parser::readBeatEffects(effect);
}

void Gp5Parser::readMixTableChange(GpMeasure* measure, BeatEffect& beatEffect) {
    auto tc = std::make_unique<MixTableChange>();
    int instrument = reader.readSignedByte()[0];
    reader.skip(16); // RSE related (GP5)
    int volume = reader.readSignedByte()[0];
    int balance = reader.readSignedByte()[0];
    int chorus = reader.readSignedByte()[0];
    int reverb = reader.readSignedByte()[0];
    int phaser = reader.readSignedByte()[0];
    int tremolo = reader.readSignedByte()[0];
    std::string tn = reader.readIntByteSizeString(); // tempo name
    int tempoVal = reader.readInt()[0];

    if (instrument >= 0) {
        tc->instrument = std::make_unique<MixTableItem>(instrument);
        tc->rse = std::make_unique<RSEInstrument>();
    }
    if (volume >= 0) tc->volume = std::make_unique<MixTableItem>(volume);
    if (balance >= 0) tc->balance = std::make_unique<MixTableItem>(balance);
    if (chorus >= 0) tc->chorus = std::make_unique<MixTableItem>(chorus);
    if (reverb >= 0) tc->reverb = std::make_unique<MixTableItem>(reverb);
    if (phaser >= 0) tc->phaser = std::make_unique<MixTableItem>(phaser);
    if (tremolo >= 0) tc->tremolo = std::make_unique<MixTableItem>(tremolo);
    if (tempoVal >= 0) {
        tc->tempo = std::make_unique<MixTableItem>(tempoVal);
        tc->tempoName = tn;
        if (measure) measure->tempo().value = tempoVal;
    }

    readMixTableChangeDurations(tc.get());

    // GP5 mix table "apply to all tracks" flags
    int8_t allTracksFlags = reader.readSignedByte()[0];
    if (tc->volume) tc->volume->allTracks = (allTracksFlags & 0x01) != 0;
    if (tc->balance) tc->balance->allTracks = (allTracksFlags & 0x02) != 0;
    // There may be more flags but they are rarely significant

    // GP5 wah/RSE extras
    if (versionTuple[1] > 0) {
        reader.readIntByteSizeString(); // wah effect name or RSE
    }
    if (tc->rse && versionTuple[1] > 0) {
        readRSEInstrumentEffect(tc->rse.get());
    }

    beatEffect.mixTableChange = std::move(tc);
}

void Gp5Parser::readMixTableChangeDurations(MixTableChange* tc) {
    if (tc->volume) tc->volume->duration = reader.readSignedByte()[0];
    if (tc->balance) tc->balance->duration = reader.readSignedByte()[0];
    if (tc->chorus) tc->chorus->duration = reader.readSignedByte()[0];
    if (tc->reverb) tc->reverb->duration = reader.readSignedByte()[0];
    if (tc->phaser) tc->phaser->duration = reader.readSignedByte()[0];
    if (tc->tremolo) tc->tremolo->duration = reader.readSignedByte()[0];
    if (tc->tempo) {
        tc->tempo->duration = reader.readSignedByte()[0];
        if (versionTuple[1] > 0) {
            tc->hideTempo = reader.readBool()[0];
        }
    }
}
