#ifndef GPMODELS_H
#define GPMODELS_H

#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <optional>
#include <algorithm>
#include <memory>

// ============================================================
// Enums — ported from GPBase.cs
// ============================================================

enum class NoteType { rest = 0, normal = 1, tie = 2, dead = 3 };
enum class BeatStatus { empty = 0, normal = 1, rest = 2 };
enum class BeatStrokeDirection { none = 0, up = 1, down = 2 };
enum class SlapEffect { none = 0, tapping = 1, slapping = 2, popping = 3 };
enum class Fingering { unknown = -2, open_ = -1, thumb = 0, index = 1, middle = 2, annular = 3, little = 4 };
enum class GraceEffectTransition { none = 0, slide = 1, bend = 2, hammer = 3 };
enum class LineBreak { none = 0, break_ = 1, protect = 2 };
enum class MeasureClef { treble = 0, bass = 1, tenor = 2, alto = 3, neutral = 4 };
enum class VoiceDirection { none = 0, up = 1, down = 2 };
enum class TupletBracket { none = 0, start = 1, end = 2 };
enum class Octave { none = 0, ottava = 1, quindicesima = 2, ottavaBassa = 3, quindicesimaBassa = 4 };
enum class Accentuation { none = 0, verySoft = 1, soft = 2, medium = 3, strong = 4, veryStrong = 5 };
enum class WahState { off = -2, closed = -1, opened = 0 };

enum class SlideType {
    intoFromAbove = -2, intoFromBelow = -1, none = 0,
    shiftSlideTo = 1, legatoSlideTo = 2,
    outDownwards = 3, outUpwards = 4,
    pickScrapeOutDownwards = 5, pickScrapeOutUpwards = 6
};

enum class TripletFeel {
    none = 0, eigth = 1, sixteenth = 2,
    dotted8th = 3, dotted16th = 4,
    scottish8th = 5, scottish16th = 6
};

enum class BendType {
    none = 0, bend = 1, bendRelease = 2, bendReleaseBend = 3,
    prebend = 4, prebendRelease = 5,
    dip = 6, dive = 7, releaseUp = 8, invertedDip = 9,
    return_ = 10, releaseDown = 11
};

enum class KeySignature {
    FMajorFlat = -80, CMajorFlat = -70, GMajorFlat = -60,
    DMajorFlat = -50, AMajorFlat = -40, EMajorFlat = -30,
    BMajorFlat = -20, FMajor = -10, CMajor = 0,
    GMajor = 10, DMajor = 20, AMajor = 30,
    EMajor = 40, BMajor = 50, FMajorSharp = 60,
    CMajorSharp = 70, GMajorSharp = 80,
    DMinorFlat = -81, AMinorFlat = -71, EMinorFlat = -61,
    BMinorFlat = -51, FMinor = -41, CMinor = -31,
    GMinor = -21, DMinor = -11, AMinor = 1,
    EMinor = 11, BMinor = 21, FMinorSharp = 31,
    CMinorSharp = 41, GMinorSharp = 51, DMinorSharp = 61,
    AMinorSharp = 71, EMinorSharp = 81
};

enum class ChordType {
    major = 0, seventh = 1, majorSeventh = 2, sixth = 3,
    minor_ = 4, minorSeventh = 5, minorMajor = 6, minorSixth = 7,
    suspendedSecond = 8, suspendedFourth = 9,
    seventhSuspendedSecond = 10, seventhSuspendedFourth = 11,
    diminished = 12, augmented = 13, power = 14
};

enum class ChordAlteration { perfect = 0, diminished = 1, augmented = 2 };
enum class ChordExtension { none = 0, ninth = 1, eleventh = 2, thirteenth = 3 };

enum class SimileMark { none = 0, simple = 1, firstOfDouble = 2, secondOfDouble = 3 };
enum class PlaybackState { Def = 0, Mute = 1, Solo = 2 };
enum class HarmonicType { None = 0, Natural = 1, Artificial = 2, Pinch = 3, Tapped = 4, Semi = 5, Feedback = 6 };
enum class Fading { None = 0, FadeIn = 1, FadeOut = 2, VolumeSwell = 3 };

// ============================================================
// Constants — ported from GPBase.cs
// ============================================================

struct Velocities {
    static constexpr int minVelocity = 15;
    static constexpr int velocityIncrement = 16;
    static constexpr int pianoPianissimo = minVelocity;
    static constexpr int pianissimo = minVelocity + velocityIncrement;
    static constexpr int piano = minVelocity + velocityIncrement * 2;
    static constexpr int mezzoPiano = minVelocity + velocityIncrement * 3;
    static constexpr int mezzoForte = minVelocity + velocityIncrement * 4;
    static constexpr int forte = minVelocity + velocityIncrement * 5;
    static constexpr int fortissimo = minVelocity + velocityIncrement * 6;
    static constexpr int forteFortissimo = minVelocity + velocityIncrement * 7;
    static constexpr int def = forte;
};

// ============================================================
// Data Model Structs — ported from GPBase.cs
// ============================================================

struct Tuplet {
    int enters = 1;
    int times = 1;
};

struct Duration {
    int value = 4; // 1=whole, 2=half, 4=quarter, 8=eighth, etc.
    bool isDotted = false;
    bool isDoubleDotted = false;
    Tuplet tuplet;

    static constexpr int quarter = 4;
    static constexpr int eigth = 8;
    static constexpr int sixteenth = 16;
    static constexpr int thirtySecond = 32;
    static constexpr int sixtyFourth = 64;
    static constexpr int hundredTwentyEigth = 128;
    static constexpr int quarterTime = 960;

    int time() const {
        int result = 0;
        switch (value) {
            case 1: result = quarterTime * 4; break;
            case 2: result = quarterTime * 2; break;
            case 4: result = quarterTime; break;
            case 8: result = quarterTime / 2; break;
            case 16: result = quarterTime / 4; break;
            case 32: result = quarterTime / 8; break;
            case 64: result = quarterTime / 16; break;
            case 128: result = quarterTime / 32; break;
            default: result = quarterTime; break;
        }
        if (isDotted) result = static_cast<int>(result * 1.5f);
        if (isDoubleDotted) result = static_cast<int>(result * 1.75f);
        result = (tuplet.enters > 0)
            ? static_cast<int>(result * tuplet.times / static_cast<float>(tuplet.enters))
            : result;
        return result;
    }
};

struct Tempo {
    int value = 120;
    Tempo(int v = 120) : value(v) {}
};

struct TimeSignature {
    int numerator = 4;
    Duration denominator;
    int beams[4] = {0, 0, 0, 0};
};

struct GpColor {
    float r = 1.0f, g = 0.0f, b = 0.0f, a = 1.0f;
    GpColor(int r_ = 255, int g_ = 0, int b_ = 0, int a_ = 255)
        : r(r_ / 255.0f), g(g_ / 255.0f), b(b_ / 255.0f), a(a_ / 255.0f) {}
};

struct GuitarString {
    int number = 0;
    int value = 0;
    GuitarString(int n = 0, int v = 0) : number(n), value(v) {}
};

struct GpMidiChannel {
    int channel = 0;
    int effectChannel = 0;
    int instrument = 0;
    int volume = 0;
    int balance = 0;
    int chorus = 0;
    int reverb = 0;
    int phaser = 0;
    int tremolo = 0;
    int bank = 0;

    bool isPercussionChannel() const { return channel == 9; }
};

struct PitchClass {
    int just = 0;
    int accidental = 0;
    int value = 0;
    float actualOvertone = 0.0f;

    PitchClass(int arg0 = 0, int arg1 = -1, const std::string& = "",
               const std::string& intonation = "", float overtone = 0.0f)
        : actualOvertone(overtone) {
        if (arg1 == -1) {
            value = arg0 % 12;
        } else {
            just = arg0 % 12;
            accidental = arg1;
            value = just + accidental;
        }
    }
};

struct BendPoint {
    float GP6position = 0.0f;
    float GP6value = 0.0f;
    bool vibrato = false;
    int position = 0;
    int value = 0;

    BendPoint() = default;
    // For GP3/4/5 format
    BendPoint(int pos, int val, bool vib = false)
        : position(pos), value(val), vibrato(vib),
          GP6position(static_cast<float>(pos)), GP6value(static_cast<float>(val)) {}
    // For GP6/7 format (float positions/values)
    BendPoint(float pos, float val)
        : GP6position(pos), GP6value(val), position(static_cast<int>(pos)), value(static_cast<int>(val)) {}
};

struct BendEffect {
    BendType type = BendType::none;
    int value = 0;
    std::vector<BendPoint> points;

    static constexpr int maxPosition = 12;
    static constexpr int semitoneLength = 1;
};

struct HarmonicEffect {
    int type = 0; // 1=natural, 2=artificial, 3=tapped, 4=pinch, 5=semi, 6=feedback
    float fret = 0.0f;
    virtual ~HarmonicEffect() = default;
};

struct NaturalHarmonic : HarmonicEffect { NaturalHarmonic() { type = 1; } };
struct ArtificialHarmonic : HarmonicEffect {
    PitchClass pitch;
    Octave octave_ = Octave::none;
    ArtificialHarmonic() { type = 2; }
    ArtificialHarmonic(PitchClass p, Octave o) : pitch(p), octave_(o) { type = 2; }
};
struct TappedHarmonic : HarmonicEffect {
    TappedHarmonic() { type = 3; }
    TappedHarmonic(int f) { type = 3; fret = static_cast<float>(f); }
};
struct PinchHarmonic : HarmonicEffect { PinchHarmonic() { type = 4; } };
struct SemiHarmonic : HarmonicEffect { SemiHarmonic() { type = 5; } };
struct FeedbackHarmonic : HarmonicEffect { FeedbackHarmonic() { type = 6; } };

struct GraceEffect {
    int fret = 0;
    int velocity = Velocities::def;
    int duration = -1; // -1 means GP6/7 format (not set)
    bool isDead = false;
    bool isOnBeat = false;
    GraceEffectTransition transition = GraceEffectTransition::none;
};

struct TremoloPickingEffect {
    Duration duration;
};

struct TrillEffect {
    int fret = 0;
    Duration duration;
};

struct GpNoteEffect {
    bool vibrato = false;
    std::vector<SlideType> slides;
    bool hammer = false;
    bool ghostNote = false;
    bool accentuatedNote = false;
    bool heavyAccentuatedNote = false;
    bool palmMute = false;
    bool staccato = false;
    bool letRing = false;
    Fingering leftHandFinger = Fingering::open_;
    Fingering rightHandFinger = Fingering::open_;
    std::unique_ptr<BendEffect> bend;
    std::unique_ptr<HarmonicEffect> harmonic;
    std::unique_ptr<GraceEffect> grace;
    std::unique_ptr<TrillEffect> trill;
    std::unique_ptr<TremoloPickingEffect> tremoloPicking;

    bool isHarmonic() const { return harmonic != nullptr; }
    bool isBend() const { return bend != nullptr && !bend->points.empty(); }
};

struct BeatStroke {
    BeatStrokeDirection direction = BeatStrokeDirection::none;
    int value = 0;
    float startTime = 0.0f;

    BeatStroke() = default;
    BeatStroke(BeatStrokeDirection d, int v, float st) : direction(d), value(v), startTime(st) {}

    void setByGP6Standard(int dur) {
        // GP6 stores brush duration in ticks - convert to note value
        if (dur <= 30) value = Duration::hundredTwentyEigth;
        else if (dur <= 60) value = Duration::sixtyFourth;
        else if (dur <= 120) value = Duration::thirtySecond;
        else if (dur <= 240) value = Duration::sixteenth;
        else if (dur <= 480) value = Duration::eigth;
        else value = Duration::quarter;
    }

    BeatStroke swapDirection() const {
        BeatStrokeDirection d = (direction == BeatStrokeDirection::up) ?
            BeatStrokeDirection::down : BeatStrokeDirection::up;
        return BeatStroke(d, value, startTime);
    }
};

struct Chord {
    bool newFormat = false;
    std::string name;
    int firstFret = 0;
    std::vector<int> strings;
    bool sharp = false;
    PitchClass root;
    ChordType chordType = ChordType::major;
    ChordExtension extension = ChordExtension::none;
    PitchClass bass;
    ChordAlteration tonality = ChordAlteration::perfect;
    bool add = false;
    ChordAlteration fifth = ChordAlteration::perfect;
    ChordAlteration ninth = ChordAlteration::perfect;
    ChordAlteration eleventh = ChordAlteration::perfect;

    struct Barre {
        int fret = 0; int start = 0; int end = 0;
        Barre(int f = 0, int s = 0, int e = 0) : fret(f), start(s), end(e) {}
    };
    std::vector<Barre> barres;
    std::vector<bool> omissions;
    std::vector<Fingering> fingerings;
    bool show = false;

    Chord(int stringCount = 6) : strings(stringCount, -1) {}

    std::vector<int> notes() const {
        std::vector<int> result;
        for (int s : strings) {
            if (s >= 0) result.push_back(s);
        }
        return result;
    }
};

struct BeatText {
    std::string value;
    BeatText() = default;
    BeatText(const std::string& v) : value(v) {}
};

struct MixTableItem {
    int value = 0;
    int duration = 0;
    bool allTracks = false;

    MixTableItem() = default;
    MixTableItem(int v, int d = 0, bool at = false) : value(v), duration(d), allTracks(at) {}
};

struct WahEffect {
    bool display = false;
    WahState state = WahState::off;
};

struct RSEInstrument {
    int instrument = -1;
    int unknown = 1;
    int soundBank = -1;
    int effectNumber = -1;
    std::string effectCategory;
    std::string effect;
};

struct MixTableChange {
    std::unique_ptr<MixTableItem> instrument;
    std::unique_ptr<MixTableItem> volume;
    std::unique_ptr<MixTableItem> balance;
    std::unique_ptr<MixTableItem> chorus;
    std::unique_ptr<MixTableItem> reverb;
    std::unique_ptr<MixTableItem> phaser;
    std::unique_ptr<MixTableItem> tremolo;
    std::unique_ptr<MixTableItem> tempo;
    std::string tempoName;
    bool hideTempo = false;
    bool useRSE = false;
    std::unique_ptr<WahEffect> wah;
    std::unique_ptr<RSEInstrument> rse;
};

struct BeatEffect {
    bool vibrato = false;
    bool fadeIn = false;
    bool fadeOut = false;
    bool volumeSwell = false;
    SlapEffect slapEffect = SlapEffect::none;
    std::unique_ptr<BendEffect> tremoloBar;
    std::unique_ptr<BeatStroke> stroke;
    BeatStrokeDirection pickStroke = BeatStrokeDirection::none;
    std::unique_ptr<Chord> chord;
    std::unique_ptr<MixTableChange> mixTableChange;
};

// Forward declarations for circular refs
struct GpVoice;
struct GpMeasure;
struct GpTrack;
struct GpFile;

struct GpBeat {
    GpVoice* voice = nullptr;
    BeatStatus status = BeatStatus::normal;
    Duration duration;
    std::vector<struct GpNote*> notes_ptrs; // non-owning
    std::vector<std::unique_ptr<struct GpNote>> notes;
    BeatEffect effect;
    std::unique_ptr<BeatText> text;
    Octave octave = Octave::none;
    int start = 0;

    struct BeatDisplay {
        bool breakBeam = false;
        bool forceBeam = false;
        bool forceBracket = false;
        bool breakSecondaryTuplet = false;
        VoiceDirection beamDirection = VoiceDirection::none;
        TupletBracket tupletBracket = TupletBracket::none;
        int breakSecondary = 0;
    };
    std::unique_ptr<BeatDisplay> display;
};

struct GpNote {
    GpBeat* beat = nullptr;
    int value = 0;
    int velocity = Velocities::def;
    int str = 0;
    NoteType type = NoteType::rest;
    GpNoteEffect effect;
    double durationPercent = 1.0;
    bool swapAccidentals = false;
    int duration = 0;
    int tuplet = 0;
    std::optional<int> midiNote;

    GpNote() = default;
    explicit GpNote(GpBeat* b) : beat(b) {}

    int realValue() const;
};

struct GpVoice {
    GpMeasure* measure = nullptr;
    std::vector<std::unique_ptr<GpBeat>> beats;
    VoiceDirection direction = VoiceDirection::none;

    GpVoice() = default;
    explicit GpVoice(GpMeasure* m) : measure(m) {}
};

struct Marker {
    std::string title;
    GpColor color;
    struct MeasureHeader* measureHeader = nullptr;
};

struct RepeatGroup {
    std::vector<struct MeasureHeader*> measureHeaders;
    std::vector<struct MeasureHeader*> openings;
    std::vector<struct MeasureHeader*> closings;
    bool isClosed = false;

    void addMeasureHeader(struct MeasureHeader* h);
};

struct MeasureHeader {
    int number = 0;
    int start = 0;
    TimeSignature timeSignature;
    Tempo tempo;
    bool isRepeatOpen = false;
    int repeatClose = -1;
    std::vector<int> repeatAlternatives;
    bool hasDoubleBar = false;
    KeySignature keySignature = KeySignature::CMajor;
    std::unique_ptr<Marker> marker;
    TripletFeel tripletFeel = TripletFeel::none;
    RepeatGroup* repeatGroup = nullptr;
    GpFile* song = nullptr;
    std::vector<std::string> direction;
    std::vector<std::string> fromDirection;

    int length() const {
        int denom_time = 0;
        switch (timeSignature.denominator.value) {
            case 1: denom_time = Duration::quarterTime * 4; break;
            case 2: denom_time = Duration::quarterTime * 2; break;
            case 4: denom_time = Duration::quarterTime; break;
            case 8: denom_time = Duration::quarterTime / 2; break;
            case 16: denom_time = Duration::quarterTime / 4; break;
            case 32: denom_time = Duration::quarterTime / 8; break;
            default: denom_time = Duration::quarterTime; break;
        }
        return denom_time * timeSignature.numerator;
    }
};

struct GpMeasure {
    GpTrack* track = nullptr;
    MeasureHeader* header = nullptr;
    std::vector<std::unique_ptr<GpVoice>> voices;
    MeasureClef clef = MeasureClef::treble;
    LineBreak lineBreak = LineBreak::none;
    SimileMark simileMark = SimileMark::none;

    static constexpr int maxVoices = 2;

    GpMeasure() {
        for (int i = 0; i < maxVoices; i++) {
            voices.push_back(std::make_unique<GpVoice>());
            voices.back()->measure = this;
        }
    }

    GpMeasure(GpTrack* t, MeasureHeader* h) : track(t), header(h) {
        for (int i = 0; i < maxVoices; i++) {
            voices.push_back(std::make_unique<GpVoice>());
            voices.back()->measure = this;
        }
    }

    int start() const { return header ? header->start : 0; }
    Tempo& tempo() { return header->tempo; }
};

struct LyricLine {
    int startingMeasure = 1;
    std::string lyrics;
};

struct Lyrics {
    int trackChoice = 0;
    LyricLine lines[5];
};

struct DirectionSign {
    std::string name;
    int measure = -1;
    DirectionSign(const std::string& n = "", int m = -1) : name(n), measure(m) {}
};

struct TrackSettings {
    bool tablature = true;
    bool notation = true;
    bool diagramsAreBelow = false;
    bool showRhythm = false;
    bool forceHorizontal = false;
    bool forceChannels = false;
    bool diagramList = true;
    bool diagramsInScore = false;
    bool autoLetRing = false;
    bool autoBrush = false;
    bool extendRhythmic = false;
};

struct TrackRSE {
    std::unique_ptr<RSEInstrument> instrument;
    int humanize = 0;
    Accentuation autoAccentuation = Accentuation::none;
};

struct GpTrack {
    GpFile* song = nullptr;
    int number = 0;
    int offset = 0; // Capo
    bool isSolo = false;
    bool isMute = false;
    bool isVisible = true;
    bool indicateTuning = true;
    std::string name;
    std::vector<std::unique_ptr<GpMeasure>> measures;
    std::vector<GuitarString> strings;
    std::string tuningName;
    GpMidiChannel channel;
    GpColor color;
    TrackSettings settings;
    int port = 0;
    bool isPercussionTrack = false;
    bool isBanjoTrack = false;
    bool is12StringedGuitarTrack = false;
    bool useRSE = false;
    int fretCount = 24;
    std::unique_ptr<TrackRSE> rse;

    GpTrack() = default;
    GpTrack(GpFile* s, int n) : song(s), number(n) {}

    void addMeasure(std::unique_ptr<GpMeasure> m) {
        m->track = this;
        measures.push_back(std::move(m));
    }
};

struct RSEEqualizer {
    float gain = 0.0f;
    std::vector<float> knobs;
    RSEEqualizer(const std::vector<float>& k = {}, float g = 0.0f) : gain(g), knobs(k) {}
};

struct RSEMasterEffect {
    int volume = 0;
    int reverb = 0;
    std::unique_ptr<RSEEqualizer> equalizer;
};

struct PageSetup {
    struct Point { int x = 210, y = 297; };
    struct Padding { int left = 10, top = 15, right = 10, bottom = 10; };
    Point pageSize;
    Padding pageMargin;
    float scoreSizeProportion = 1.0f;
    int headerAndFooter = 0;
    std::string title, subtitle, artist, album, words, music, wordsAndMusic, copyright, pageNumber;
};

struct Clipboard {
    int startMeasure = 1, stopMeasure = 1;
    int startTrack = 1, stopTrack = 1;
    int startBeat = 1, stopBeat = 1;
    bool subBarCopy = false;
};

// Main file structure
struct GpFile {
    std::string version;
    int versionTuple[2] = {0, 0};
    std::string title, subtitle, interpret, album, author;
    std::string words, music;
    std::string copyright, tab_author, instructional;
    std::vector<Lyrics> lyrics;
    int tempo = 120;
    std::string tempoName;
    bool hideTempo = false;
    std::vector<std::unique_ptr<GpTrack>> tracks;
    std::vector<std::unique_ptr<MeasureHeader>> measureHeaders;
    TripletFeel tripletFeel_ = TripletFeel::none;
    std::unique_ptr<RSEMasterEffect> masterEffect;
    std::unique_ptr<PageSetup> pageSetup;
    std::vector<DirectionSign> directions;
    std::unique_ptr<Clipboard> clipboard;
    int measureCount = 0;
    int trackCount = 0;

    // Self-pointer for GP6/7 which create a GP5File internally
    GpFile* self = nullptr;

    virtual ~GpFile() = default;
    virtual void readSong() {}

    // Gets the effective file (self or this)
    GpFile* effective() { return self ? self : this; }
};

// ============================================================
// Inline implementations
// ============================================================

inline int GpNote::realValue() const {
    if (!beat || !beat->voice || !beat->voice->measure || !beat->voice->measure->track) return value;
    auto* track = beat->voice->measure->track;
    if (str - 1 < 0 || str - 1 >= static_cast<int>(track->strings.size())) return value;
    return value + track->strings[str - 1].value;
}

inline void RepeatGroup::addMeasureHeader(MeasureHeader* h) {
    if (openings.empty()) openings.push_back(h);
    measureHeaders.push_back(h);
    h->repeatGroup = this;
    if (h->repeatClose > 0) {
        closings.push_back(h);
        isClosed = true;
    } else if (isClosed) {
        isClosed = false;
        openings.push_back(h);
    }
}

// GP6-specific helper structs
struct GP6Tempo {
    bool linear = false;
    int bar = 0;
    float position = 0.0f;
    bool visible = true;
    int tempo = 120;
    int tempoType = 2;
    bool transferred = false;
};

struct GP6Rhythm {
    int id = 0;
    int noteValue = 4;
    int augmentationDots = 0;
    Tuplet primaryTuplet;

    GP6Rhythm(int id_ = 0, int nv = 4, int ad = 0, int n = 1, int m = 1)
        : id(id_), noteValue(nv), augmentationDots(ad) {
        primaryTuplet.enters = n;
        primaryTuplet.times = m;
    }
};

struct GP6Chord {
    int id = 0;
    int forTrack = 0;
    std::string name;
};

// Global constant used in bend parsing
namespace GPBaseConstants {
    constexpr int bendPosition = 60;
    constexpr int bendSemitone = 25;
}

#endif // GPMODELS_H
