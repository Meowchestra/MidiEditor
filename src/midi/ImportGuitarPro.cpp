/*
 * MidiEditor - GuitarPro File Importer
 * Converts GuitarPro files (.gp3-.gp7, .gpx) to standard MIDI events.
 *
 * GP3/GP4/GP5: Binary format parsing via BinaryReader + GPParser.
 * GP6/GPX:     Custom bitstream decompression + XML parsing.
 * GP7:         ZIP archive extraction + GPIF XML parsing.
 *
 * Guitar-Pro–specific metadata (bends, slides, harmonics, tab fingerings)
 * is skipped since those concepts don't map cleanly to standard MIDI.
 *
 */

#include "ImportGuitarPro.h"
#include "MidiFile.h"
#include "MidiTrack.h"
#include "MidiChannel.h"
#include "../MidiEvent/MidiEvent.h"
#include "../MidiEvent/NoteOnEvent.h"
#include "../MidiEvent/OffEvent.h"
#include "../MidiEvent/OnEvent.h"
#include "../MidiEvent/ControlChangeEvent.h"
#include "../MidiEvent/ProgChangeEvent.h"
#include "../MidiEvent/TempoChangeEvent.h"
#include "../MidiEvent/TimeSignatureEvent.h"
#include "../MidiEvent/TextEvent.h"
#include "../MidiEvent/PitchBendEvent.h"

#include <QFile>
#include <QDebug>
#include <QString>
#include <QList>
#include <QByteArray>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QMap>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <zlib.h>

// ═══════════════════════════════════════════════════════════════════════
//  Binary reader — mirrors GPBase.cs
// ═══════════════════════════════════════════════════════════════════════
namespace GP {

class BinaryReader {
    const char* _data;
    int _len;
    int _pos;
public:
    BinaryReader(const QByteArray& ba)
        : _data(ba.constData()), _len(ba.size()), _pos(0) {}

    bool atEnd() const { return _pos >= _len; }
    int position() const { return _pos; }
    int length() const { return _len; }
    bool canRead(int n) const { return _pos + n <= _len; }

    void skip(int n) {
        _pos = std::min(_pos + n, _len);
    }

    quint8 readByte() {
        if (_pos >= _len) return 0;
        return static_cast<quint8>(_data[_pos++]);
    }
    qint8 readSignedByte() {
        return static_cast<qint8>(readByte());
    }
    bool readBool() {
        return readByte() != 0;
    }
    qint16 readShort() {
        quint8 lo = readByte();
        quint8 hi = readByte();
        return static_cast<qint16>(lo | (hi << 8));
    }
    qint32 readInt() {
        quint8 b0 = readByte();
        quint8 b1 = readByte();
        quint8 b2 = readByte();
        quint8 b3 = readByte();
        return static_cast<qint32>(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
    }
    float readFloat() {
        qint32 v = readInt();
        float f;
        memcpy(&f, &v, 4);
        return f;
    }
    double readDouble() {
        quint64 lo = static_cast<quint32>(readInt());
        quint64 hi = static_cast<quint32>(readInt());
        quint64 val = lo | (hi << 32);
        double d;
        memcpy(&d, &val, 8);
        return d;
    }

    // Read a fixed-size string, padded to `size` bytes
    QString readByteSizeString(int size) {
        int len = readByte();
        len = std::min(len, size);
        QByteArray ba(_data + _pos, std::min(len, _len - _pos));
        _pos += size; // Always advance by the full padded size
        if (_pos > _len) _pos = _len;
        return QString::fromLatin1(ba);
    }

    // Read a string prefixed with a 4-byte length
    QString readIntSizeString() {
        int len = readInt();
        if (len <= 0 || len > _len - _pos) { skip(std::max(0, len)); return QString(); }
        QByteArray ba(_data + _pos, std::min(len, _len - _pos));
        _pos += len;
        return QString::fromLatin1(ba);
    }

    // Read a string prefixed with int+byte size (GP standard pattern)
    QString readIntByteSizeString() {
        int totalLen = readInt() - 1;
        int strLen = readByte();
        int toRead = std::max(totalLen, strLen);
        if (toRead <= 0 || toRead > _len - _pos) { skip(std::max(0, toRead)); return QString(); }
        QByteArray ba(_data + _pos, std::min(strLen, _len - _pos));
        _pos += toRead;
        return QString::fromLatin1(ba);
    }
};

// ═══════════════════════════════════════════════════════════════════════
//  Internal data structures mirroring the GP object model
// ═══════════════════════════════════════════════════════════════════════
struct GuitarString {
    int number;    // string number (1-based from thinnest)
    int value;     // MIDI note value of open string
};

struct Duration {
    static const int QUARTER_TIME = 960;
    int value = 4;       // 1=whole, 2=half, 4=quarter, 8=eighth, etc.
    bool isDotted = false;
    int tupletEnters = 1;
    int tupletTimes = 1;

    int time() const {
        if (value <= 0) return QUARTER_TIME;
        int result = static_cast<int>(QUARTER_TIME * (4.0 / value));
        if (isDotted) result = static_cast<int>(result * 1.5);
        if (tupletEnters > 0 && tupletEnters != tupletTimes)
            result = static_cast<int>(result * tupletTimes / static_cast<double>(tupletEnters));
        return std::max(result, 1);
    }
};

enum class HarmonicType { None, Natural, Artificial, Pinch, Tapped, Semi };
enum class Fading { None, FadeIn, FadeOut, VolumeSwell };
enum class TripletFeel { None, Eighth, Sixteenth, Dotted8th, Dotted16th, Scottish8th, Scottish16th };

struct BendPoint {
    int index;
    int value;
};

struct TremoloPoint {
    int index;
    float value;
};

struct GPNote {
    int tick = 0;
    int duration = 0;
    
    int str = 0;         // string number (1-based)
    int fret = 0;        // fret number
    int velocity = 100;
    bool isTied = false;
    bool isDead = false;
    
    // Advanced Effects
    bool isGhost = false;
    bool isPalmMute = false;
    bool isStaccato = false;
    bool isAccentuated = false;
    bool isHeavyAccentuated = false;

    bool isVibrato = false;
    bool isHammer = false;
    bool isTrill = false;
    int trillFret = 0;
    int trillDuration = 0;
    int tremoloPickingDuration = 0;
    
    bool isGrace = false;
    int graceFret = 0;
    int graceVelocity = 100;
    int graceDurationTicks = 120; // Default 32nd note

    HarmonicType harmonic = HarmonicType::None;
    float harmonicFret = 0.0f;

    Fading fading = Fading::None;

    QList<BendPoint> bendPoints;

    bool slideInFromAbove = false;
    bool slideInFromBelow = false;
    bool slideOutDownwards = false;
    bool slideOutUpwards = false;
    bool slidesToNext = false;

    int fallbackMidiNote = -1; // Used when fret/str logic isn't available
};

struct GPMeasureHeader {
    int numerator = 4;
    int denominator = 4;
    int tempo = -1;       // -1 = no change; positive = new tempo
    int start = 0;        // absolute tick position

    int length() const {
        int baseTicks = Duration::QUARTER_TIME * 4 / std::max(denominator, 1);
        return numerator * baseTicks;
    }
    
    TripletFeel tripletFeel = TripletFeel::None;
};

struct MidiChannelInfo {
    int instrument = 25;
    int volume = 127;
    int balance = 64;
};

struct GPTrack {
    QString name;
    int channel = 0;         // MIDI channel 0-15
    int instrument = 25;     // GM program number
    int capo = 0;
    QList<GuitarString> strings;

    struct TempoEvent {
        int tick;
        int tempo;
    };
    QList<TempoEvent> tempoEvents;

    QList<TremoloPoint> tremoloPoints;
    QList<GPNote> noteEvents;
};

struct GPBeatEffects {
    int strokeDown = 0;
    int strokeUp = 0;
    bool isVibrato = false;
    Fading fading = Fading::None;
    QList<BendPoint> tremoloBar;
};

// ═══════════════════════════════════════════════════════════════════════
//  GP3/GP4/GP5 Binary Format Parser
// ═══════════════════════════════════════════════════════════════════════
class GPParser {
    BinaryReader& _r;
    int _version;       // 3, 4, or 5
    int _versionMinor;
    int _trackCount;

    QList<GPMeasureHeader> _measureHeaders;
    QList<GPTrack> _tracks;
    MidiChannelInfo _midiChannels[64];
    int _globalTempo;
    QString _title;

public:
    GPParser(BinaryReader& reader, int version)
        : _r(reader), _version(version), _versionMinor(0),
          _trackCount(0), _globalTempo(120) {}

    bool parse() {
        if (!readVersion()) return false;
        readInfo();
        readPreMidiSettings();
        readMidiChannels();
        readPostMidiSettings();

        int measureCount = _r.readInt();
        _trackCount = _r.readInt();

        if (measureCount <= 0 || measureCount > 10000 || _trackCount <= 0 || _trackCount > 100) {
            qWarning() << "ImportGuitarPro: Invalid measure/track count:"
                       << measureCount << "/" << _trackCount;
            return false;
        }

        readMeasureHeaders(measureCount);
        readTracks();
        readMeasures();
        return true;
    }

    const QList<GPMeasureHeader>& measureHeaders() const { return _measureHeaders; }
    const QList<GPTrack>& tracks() const { return _tracks; }
    int globalTempo() const { return _globalTempo; }
    const QString& title() const { return _title; }

private:
    // ─────────────────────────────────────────────────────
    //  Version detection
    // ─────────────────────────────────────────────────────
    bool readVersion() {
        QString versionStr = _r.readByteSizeString(30);
        // Examples: "FICHIER GUITAR PRO v3.00", "FICHIER GUITAR PRO v5.10"
        if (versionStr.contains("v3", Qt::CaseInsensitive)) _version = 3;
        else if (versionStr.contains("v4", Qt::CaseInsensitive)) _version = 4;
        else if (versionStr.contains("v5", Qt::CaseInsensitive)) _version = 5;

        // Parse minor version from last 4 chars like "5.10"
        if (versionStr.length() >= 4) {
            QStringList parts = versionStr.right(4).split('.');
            if (parts.size() == 2) {
                _versionMinor = parts[1].toInt();
            }
        }
        return true;
    }

    // ─────────────────────────────────────────────────────
    //  File info (title, author, etc.)
    // ─────────────────────────────────────────────────────
    void readInfo() {
        _title = _r.readIntByteSizeString();        // title
        _r.readIntByteSizeString();                 // subtitle
        _r.readIntByteSizeString();                 // interpret/artist
        _r.readIntByteSizeString();                 // album
        _r.readIntByteSizeString();                 // words/author
        if (_version >= 5) {
            _r.readIntByteSizeString();             // music (GP5 only)
        }
        _r.readIntByteSizeString();                 // copyright
        _r.readIntByteSizeString();                 // tab author
        _r.readIntByteSizeString();                 // instructional

        // Notice lines
        int lineCount = _r.readInt();
        for (int i = 0; i < lineCount; i++) {
            _r.readIntByteSizeString();
        }
    }

    // ─────────────────────────────────────────────────────
    //  Settings BEFORE readMidiChannels (differs by version)
    // ─────────────────────────────────────────────────────
    void readPreMidiSettings() {
        // GP3: tripletFeel(bool), tempo(int), key(int)
        // GP4: tripletFeel(bool), lyrics, tempo(int), key(int), octave(signedByte)
        // GP5: lyrics, RSE, pageSetup, tempoName, tempo(int), [hideTempo], key(signedByte), octave(int)

        if (_version < 5) {
            _r.readBool(); // tripletFeel
        }

        if (_version >= 4) {
            readLyrics();
        }

        if (_version >= 5) {
            readRSEMasterEffect();
            readPageSetup();
            _r.readIntByteSizeString(); // tempo name
        }

        _globalTempo = _r.readInt();
        if (_globalTempo <= 0) _globalTempo = 120;

        if (_version >= 5 && _versionMinor > 0) {
            _r.readBool(); // hideTempo
        }

        if (_version >= 5) {
            _r.readSignedByte(); // key (1 byte for GP5)
            _r.readInt();        // octave
        } else if (_version == 4) {
            _r.readInt();        // key (4 bytes for GP4)
            _r.readSignedByte(); // octave (1 byte for GP4)
        } else {
            _r.readInt();        // key (4 bytes for GP3, no octave)
        }
    }

    void readLyrics() {
        _r.readInt(); // lyric track number
        for (int i = 0; i < 5; i++) {
            _r.readInt();           // starting measure
            _r.readIntSizeString(); // lyric text
        }
    }

    void readRSEMasterEffect() {
        if (_versionMinor <= 0) return;
        _r.readInt(); // master volume
        _r.readInt(); // unknown
        // 11-band equalizer: 10 bands + 1 gain
        for (int i = 0; i < 11; i++) _r.readSignedByte();
    }

    void readPageSetup() {
        // pageSize: 2 ints (width, height)
        _r.readInt(); _r.readInt();
        // margins: 4 ints (left, right, top, bottom)
        _r.readInt(); _r.readInt(); _r.readInt(); _r.readInt();
        // scoreSizeProportion
        _r.readInt();
        // headerAndFooter flags
        _r.readShort();
        // 10 template strings
        for (int i = 0; i < 10; i++) _r.readIntByteSizeString();
    }

    // ─────────────────────────────────────────────────────
    //  Settings AFTER readMidiChannels (GP5 only)
    // ─────────────────────────────────────────────────────
    void readPostMidiSettings() {
        if (_version >= 5) {
            // Directions: 19 shorts
            for (int i = 0; i < 19; i++) _r.readShort();
            // Master effect reverb
            _r.readInt();
        }
    }

    // ─────────────────────────────────────────────────────
    //  MIDI channel definitions (64 channels total, same for all versions)
    // ─────────────────────────────────────────────────────
    void readMidiChannels() {
        for (int i = 0; i < 64; i++) {
            _midiChannels[i].instrument = _r.readInt();
            _midiChannels[i].volume = _r.readByte();
            _midiChannels[i].balance = _r.readByte();
            _r.readByte();   // chorus
            _r.readByte();   // reverb
            _r.readByte();   // phaser
            _r.readByte();   // tremolo
            _r.readShort();  // padding (backward compat)
        }
    }

    // ─────────────────────────────────────────────────────
    //  Measure headers
    // ─────────────────────────────────────────────────────
    void readMeasureHeaders(int count) {
        int prevNum = 4, prevDen = 4;

        for (int i = 0; i < count && !_r.atEnd(); i++) {
            GPMeasureHeader header;
            header.numerator = prevNum;
            header.denominator = prevDen;

            quint8 flags = _r.readByte();

            if (flags & 0x01) header.numerator = _r.readSignedByte();
            if (flags & 0x02) header.denominator = _r.readSignedByte();

            // Repeat open: flag 0x04 — no data to read
            if (flags & 0x08) _r.readSignedByte(); // repeat close count
            if (flags & 0x10) {
                _r.readByte(); // alternate ending number
            }
            if (flags & 0x20) {
                // Marker
                _r.readIntByteSizeString(); // marker name
                _r.readByte(); _r.readByte(); _r.readByte(); _r.readByte(); // color RGBA
            }
            if (flags & 0x40) {
                _r.readSignedByte(); // key
                _r.readSignedByte(); // minor/major
            }
            // 0x80 = double bar — no data to read

            // GP5 extra measure header data
            if (_version >= 5) {
                if (flags & 0x03) { // time sig changed → beam groups
                    _r.skip(4);
                }
                if (!(flags & 0x10)) {
                    _r.readByte(); // alternate ending
                }
                int tf = _r.readByte(); // tripletFeel
                if (tf == 1) header.tripletFeel = TripletFeel::Eighth;
                else if (tf == 2) header.tripletFeel = TripletFeel::Sixteenth;
                else if (tf == 3) header.tripletFeel = TripletFeel::Dotted8th;
                else if (tf == 4) header.tripletFeel = TripletFeel::Dotted16th;
                else if (tf == 5) header.tripletFeel = TripletFeel::Scottish8th;
                else if (tf == 6) header.tripletFeel = TripletFeel::Scottish16th;
                
                _r.readByte(); // padding
            }

            header.tempo = -1; // Will be updated from measure data
            prevNum = header.numerator;
            prevDen = header.denominator;
            _measureHeaders.append(header);
        }
    }

    // ─────────────────────────────────────────────────────
    //  Track definitions
    // ─────────────────────────────────────────────────────
    void readTracks() {
        for (int i = 0; i < _trackCount && !_r.atEnd(); i++) {
            GPTrack track;

            // GP5: padding byte before each track (or just the first)
            if (_version >= 5) {
                if (i == 0 || _versionMinor > 0) {
                    _r.readByte();
                }
            }

            quint8 flags = _r.readByte(); // track flags (isPercussion, is12String, etc.)
            track.name = _r.readByteSizeString(40);

            // Strings
            int stringCount = _r.readInt();
            for (int s = 0; s < 7; s++) {
                int tuning = _r.readInt();
                if (s < stringCount && tuning > 0) {
                    GuitarString gs;
                    gs.number = s + 1;
                    gs.value = tuning;
                    track.strings.append(gs);
                }
            }

            // MIDI port (1-based, skip)
            _r.readInt();
            // MIDI channel index (1-based → 0-based into 64-channel array)
            int channelIdx = _r.readInt() - 1;
            // MIDI channel is the index mod 16 (4 ports × 16 channels)
            track.channel = (channelIdx >= 0 && channelIdx < 64) ? (channelIdx % 16) : 0;
            // Channel for effects (skip)
            _r.readInt();
            // Number of frets (skip)
            _r.readInt();
            // Capo
            track.capo = _r.readInt();
            // Track color (skip)
            _r.readInt();

            // Look up instrument from MIDI channel data using FULL index
            if (channelIdx >= 0 && channelIdx < 64) {
                int inst = _midiChannels[channelIdx].instrument;
                if (inst < 0) inst = 0;
                track.instrument = std::clamp(inst, 0, 127);
            }
            // Percussion: flag 0x01 or channel 9
            if ((flags & 0x01) || track.channel == 9) {
                track.channel = 9;
                track.instrument = 0;
            }

            // GP5 extra track data
            if (_version >= 5) {
                _r.readByte();  // tablature editing mode
                _r.readByte();  // diagrams below
                _r.readByte();  // show rhythm
                _r.readByte();  // force horizontal beams
                _r.readByte();  // force channels
                _r.readByte();  // diagram list
                _r.readByte();  // diagrams in score
                _r.readByte();  // padding
                _r.readInt();   // auto-accentuation
                _r.readByte();  // midi bank
                _r.readByte();  // human playback
                _r.readByte();  // auto playback
                _r.readInt();   // humanize
                _r.readInt();   // unknown
                _r.readInt();   // unknown
                _r.readInt();   // unknown
                _r.readInt();   // unknown
                _r.readInt();   // unknown
                _r.readIntByteSizeString(); // RSE effect category
                _r.readIntByteSizeString(); // RSE effect name
            }

            _tracks.append(track);
        }
    }

    // ─────────────────────────────────────────────────────
    //  Measure data (notes, beats, etc.)
    // ─────────────────────────────────────────────────────
    void readMeasures() {
        // Assign tick positions
        int start = Duration::QUARTER_TIME;
        for (int i = 0; i < _measureHeaders.size(); i++) {
            _measureHeaders[i].start = start;
            start += _measureHeaders[i].length();
        }

        // Read measure × track data
        for (int m = 0; m < _measureHeaders.size(); m++) {
            for (int t = 0; t < _tracks.size(); t++) {
                readMeasure(m, t);
            }
        }
    }

    void readMeasure(int measureIdx, int trackIdx) {
        if (_r.atEnd()) return;

        GPMeasureHeader& header = _measureHeaders[measureIdx];
        GPTrack& track = _tracks[trackIdx];
        int measureStart = header.start;

        // GP5 has 2 voices per measure; GP3/GP4 have 1
        int voiceCount = (_version >= 5) ? 2 : 1;

        for (int v = 0; v < voiceCount; v++) {
            if (_r.atEnd()) return;
            int beatCount = _r.readInt();
            if (beatCount < 0 || beatCount > 10000) return; // sanity check

            int tickPos = 0;
            for (int b = 0; b < beatCount; b++) {
                if (_r.atEnd()) return;
                int beatDur = readBeat(track, measureStart + tickPos, header);
                tickPos += beatDur;
            }
        }

        // GP5: line break byte at end of measure
        if (_version >= 5) {
            _r.readByte();
        }
    }

    int readBeat(GPTrack& track, int startTick, GPMeasureHeader& header) {
        quint8 flags = _r.readByte();

        bool isEmpty = false;
        bool isRest = false;
        if (flags & 0x40) {
            quint8 status = _r.readByte();
            isEmpty = (status == 0x00);
            isRest = (status == 0x02);
        }

        // Duration
        Duration dur;
        qint8 durByte = _r.readSignedByte();
        dur.value = 1 << (durByte + 2);
        dur.isDotted = (flags & 0x01) != 0;

        if (flags & 0x20) {
            int tupletVal = _r.readInt();
            switch (tupletVal) {
                case 3:  dur.tupletEnters = 3;  dur.tupletTimes = 2; break;
                case 5:  dur.tupletEnters = 5;  dur.tupletTimes = 4; break;
                case 6:  dur.tupletEnters = 6;  dur.tupletTimes = 4; break;
                case 7:  dur.tupletEnters = 7;  dur.tupletTimes = 4; break;
                case 9:  dur.tupletEnters = 9;  dur.tupletTimes = 8; break;
                case 10: dur.tupletEnters = 10; dur.tupletTimes = 8; break;
                case 11: dur.tupletEnters = 11; dur.tupletTimes = 8; break;
                case 12: dur.tupletEnters = 12; dur.tupletTimes = 8; break;
                default: break;
            }
        }

        // Chord diagram
        if (flags & 0x02) skipChord();

        // Text
        if (flags & 0x04) _r.readIntByteSizeString();

        GPBeatEffects beatEffects = readBeatEffects();

        // Mix table change
        int tempoFromMix = -1;
        if (flags & 0x10) tempoFromMix = readMixTableChange();

        // Notes
        quint8 stringFlags = _r.readByte();
        QList<GPNote> notes;

        for (int s = 0; s < track.strings.size(); s++) {
            if (stringFlags & (1 << (7 - track.strings[s].number))) {
                GPNote note = readNote();
                note.str = track.strings[s].number;
                notes.append(note);
            }
        }

        // GP5: extra beat data (flags2 short + optional breakSecondary byte)
        if (_version >= 5) {
            qint16 flags2 = _r.readShort();
            if (flags2 & 0x0800) {
                _r.readByte(); // breakSecondary
            }
        }

        // Convert to GPNote events
        int beatDuration = dur.time();

        if (!isEmpty && !isRest) {
            // Apply beat stroke
            int strokeDuration = 0;
            if (beatEffects.strokeDown > 0) strokeDuration = beatEffects.strokeDown;
            else if (beatEffects.strokeUp > 0) strokeDuration = beatEffects.strokeUp;
            
            int spread = 0;
            if (strokeDuration > 0 && notes.size() > 1) {
                // Brush duration in ticks. Rough approximation of GP brushes:
                spread = (Duration::QUARTER_TIME / 16) * strokeDuration / 64; 
            }

            int noteOffset = 0;
            if (beatEffects.strokeUp > 0) {
                noteOffset = spread * (notes.size() - 1);
                spread = -spread; // Reverse spread for up stroke
            }

            for (GPNote& n : notes) {
                n.tick = startTick + noteOffset;
                n.duration = beatDuration;
                if (beatEffects.isVibrato) n.isVibrato = true;
                if (beatEffects.fading != Fading::None) n.fading = beatEffects.fading;

                track.noteEvents.append(n);
                noteOffset += spread;
            }
        }

        // Apply tremolo bar bends to track
        for (const BendPoint& bp : beatEffects.tremoloBar) {
            TremoloPoint tp;
            // The GP bend point index is 0-60 representing 0-100% of duration
            tp.index = startTick + static_cast<int>(bp.index * beatDuration / 60.0);
            tp.value = bp.value / 25.6f; // Map GP bend value to float semitones
            track.tremoloPoints.append(tp);
        }

        // Pass tempo changes up
        if (tempoFromMix > 0) {
            header.tempo = tempoFromMix;
        }

        return isEmpty ? 0 : beatDuration;
    }

    GPNote readNote() {
        GPNote note;
        quint8 flags = _r.readByte();

        // Note type
        if (flags & 0x20) {
            quint8 noteType = _r.readByte();
            note.isTied = (noteType == 2);
            note.isDead = (noteType == 3);
        }

        // Time-independent duration (same for all versions)
        if (flags & 0x01) {
            _r.readSignedByte(); // duration
            _r.readSignedByte(); // tuplet
        }

        // Dynamics
        if (flags & 0x10) {
            qint8 dyn = _r.readSignedByte();
            note.velocity = std::clamp(15 + 16 * static_cast<int>(dyn) - 16, 1, 127);
        }

        // Fret
        if (flags & 0x20) {
            note.fret = std::clamp(static_cast<int>(_r.readSignedByte()), 0, 99);
        }

        // Fingering
        if (flags & 0x80) {
            _r.readSignedByte(); // left hand
            _r.readSignedByte(); // right hand
        }

        // GP5: durationPercent (double) if flag 0x01
        if (_version >= 5) {
            if (flags & 0x01) _r.readDouble();
            _r.readByte(); // flags2 (swap accidentals etc.)
        }

        // Note effects
        if (flags & 0x08) readNoteEffects(note);

        return note;
    }

    // ─────────────────────────────────────────────────────
    //  Skip routines for complex effect data we don't need
    // ─────────────────────────────────────────────────────

    void skipChord() {
        bool newFormat = _r.readBool();
        if (!newFormat) {
            _r.readIntByteSizeString(); // name
            int firstFret = _r.readInt();
            if (firstFret > 0) {
                for (int i = 0; i < 6; i++) _r.readInt();
            }
        } else {
            _r.readBool(); // sharp
            _r.skip(3);
            for (int i = 0; i < 5; i++) _r.readInt(); // root, type, ext, bass, tonality
            _r.readBool(); // add
            _r.readByteSizeString(22); // name
            for (int i = 0; i < 3; i++) _r.readInt(); // fifth, ninth, eleventh
            _r.readInt(); // first fret
            for (int i = 0; i < 6; i++) _r.readInt(); // frets
            _r.readInt(); // barresCount
            for (int i = 0; i < 2; i++) _r.readInt(); // barre frets
            for (int i = 0; i < 2; i++) _r.readInt(); // barre starts
            for (int i = 0; i < 2; i++) _r.readInt(); // barre ends
            for (int i = 0; i < 7; i++) _r.readBool(); // omissions
            _r.skip(1);
        }
    }

    GPBeatEffects readBeatEffects() {
        GPBeatEffects effects;
        quint8 flags1 = _r.readByte();
        quint8 flags2 = 0;
        if (_version >= 4) {
            flags2 = _r.readByte(); // GP4/GP5: two flags bytes
        }

        if (flags1 & 0x01) effects.isVibrato = true;
        if (flags1 & 0x02) {
            // Note: wide vibrato
        }
        if (flags1 & 0x10) {
            int f = _r.readByte(); 
            if (f == 1) effects.fading = Fading::FadeIn;
            else if (f == 2) effects.fading = Fading::FadeOut;
            else if (f == 3) effects.fading = Fading::VolumeSwell;
        }

        if (flags1 & 0x20) {
            if (_version >= 4) {
                // GP4/GP5: slapEffect byte is a type indicator:
                //   0=none, 1=tapping, 2=slapping, 3=popping
                // No additional data follows. Tremolo bar is flags2 & 0x04.
                _r.readByte(); // slapEffect — just consume, no follow-up data
            } else {
                // GP3: flags2 byte is the slapEffect/tremolo selector.
                // If 0 → tremolo bar (readBend format), else tapping → readInt
                quint8 slapEffect = _r.readByte();
                if (slapEffect == 0) {
                    effects.tremoloBar = readBend(); // tremolo bar in GP3 uses bend format
                } else {
                    _r.readInt(); // tapping/slapping/popping value
                }
            }
        }

        if (_version >= 4 && (flags2 & 0x04)) {
            // Tremolo bar affects all notes in beat; usually we can attach it to the first note or handle separately.
            // For now, attaching to the entire beat will require passing note array.
            effects.tremoloBar = readBend(); // Read tremolo
        }

        if (flags1 & 0x40) {
            effects.strokeDown = _r.readSignedByte(); // stroke down
            effects.strokeUp = _r.readSignedByte(); // stroke up
        }

        if (_version >= 4) {
            if (flags2 & 0x02) _r.readSignedByte(); // pick stroke
        }
        
        return effects;
    }

    void readNoteEffects(GPNote& n) {
        quint8 flags1 = _r.readByte();
        quint8 flags2 = 0;

        if (_version >= 4) {
            flags2 = _r.readByte(); // GP4/GP5: two flags bytes
        }

        // flags1 bits:
        // 0x01 = bend, 0x02 = hammer, 0x04 = slide (GP3: no data)
        // 0x08 = let ring, 0x10 = grace note

        if (flags1 & 0x01) n.bendPoints = readBend();
        if (flags1 & 0x02) n.isHammer = true;
        if (flags1 & 0x04) n.slidesToNext = true;

        if (flags1 & 0x10) {
            n.isGrace = true;
            n.graceFret = _r.readByte();
            n.graceVelocity = std::clamp(static_cast<int>(_r.readByte()) * 10, 1, 127);
            
            int gDur = _r.readByte();
            if (gDur == 1) n.graceDurationTicks = 120; // 32nd (960/8)
            else if (gDur == 2) n.graceDurationTicks = 160; // 24th
            else if (gDur == 3) n.graceDurationTicks = 240; // 16th
            else n.graceDurationTicks = 120;
            
            if (_version >= 4) _r.readByte(); // extra byte for GP4
            if (_version >= 5) _r.readByte(); // extra flags for GP5
        }

        if (_version >= 4) {
            // flags2 bits for GP4/GP5:
            // 0x01 = staccato, 0x02 = palm mute, 0x04 = tremolo picking
            // 0x08 = slide, 0x10 = harmonic, 0x20 = trill, 0x40 = vibrato
            if (flags2 & 0x01) n.isStaccato = true;
            if (flags2 & 0x02) n.isPalmMute = true;
            if (flags2 & 0x04) n.tremoloPickingDuration = _r.readSignedByte(); 
            if (flags2 & 0x08) {
                int slType = _r.readSignedByte(); // slide type
                if (slType == 1) n.slidesToNext = true; // shift
                else if (slType == 2) n.slidesToNext = true; // legato
                else if (slType == 3) n.slideOutDownwards = true;
                else if (slType == 4) n.slideOutUpwards = true;
                else if (slType == 5) n.slideInFromBelow = true;
                else if (slType == 6) n.slideInFromAbove = true;
            }
            if (flags2 & 0x10) {
                // Harmonic
                if (_version >= 5) {
                    qint8 hType = _r.readSignedByte();
                    if (hType == 1) n.harmonic = HarmonicType::Natural;
                    else if (hType == 2) {
                        n.harmonic = HarmonicType::Artificial;
                        _r.readByte(); _r.readSignedByte(); _r.readByte();
                    }
                    else if (hType == 3) {
                        n.harmonic = HarmonicType::Tapped;
                        _r.readByte();
                    }
                    else if (hType == 4) n.harmonic = HarmonicType::Pinch;
                    else if (hType == 5) n.harmonic = HarmonicType::Semi;
                } else {
                    int hType = _r.readSignedByte(); // GP4 harmonic type (just one byte)
                    if (hType == 1) n.harmonic = HarmonicType::Natural;
                    else if (hType == 2) n.harmonic = HarmonicType::Artificial;
                    else if (hType == 3) n.harmonic = HarmonicType::Tapped;
                    else if (hType == 4) n.harmonic = HarmonicType::Pinch;
                    else if (hType == 5) n.harmonic = HarmonicType::Semi;
                }
            }
            if (flags2 & 0x20) {
                // Trill
                n.isTrill = true;
                n.trillFret = _r.readSignedByte(); // fret
                n.trillDuration = _r.readSignedByte(); // period
            }
            if (flags2 & 0x40) n.isVibrato = true;
        } else {
            // GP3: slide flag 0x04 — no extra data to read
            // already captured above
        }
    }

    QList<BendPoint> readBend() {
        QList<BendPoint> points;
        _r.readSignedByte(); // type
        _r.readInt();        // value
        int pointCount = _r.readInt();
        for (int i = 0; i < pointCount && i < 1000; i++) {
            BendPoint bp;
            bp.index = _r.readInt();  // position
            bp.value = _r.readInt();  // value
            _r.readBool(); // vibrato
            points.append(bp);
        }
        return points;
    }

    int readMixTableChange() {
        qint8 instrument = _r.readSignedByte();
        if (_version >= 5 && instrument >= 0) {
            _r.readInt(); // RSE instrument number
        }
        qint8 volume = _r.readSignedByte();
        qint8 balance = _r.readSignedByte();
        qint8 chorus = _r.readSignedByte();
        qint8 reverb = _r.readSignedByte();
        qint8 phaser = _r.readSignedByte();
        qint8 tremolo = _r.readSignedByte();

        if (_version >= 5) _r.readIntByteSizeString(); // tempo name

        int tempo = _r.readInt();

        // Duration bytes for each changed value
        if (volume >= 0)  _r.readSignedByte();
        if (balance >= 0) _r.readSignedByte();
        if (chorus >= 0)  _r.readSignedByte();
        if (reverb >= 0)  _r.readSignedByte();
        if (phaser >= 0)  _r.readSignedByte();
        if (tremolo >= 0) _r.readSignedByte();
        if (tempo >= 0) {
            _r.readSignedByte(); // duration
            if (_version >= 5) _r.readBool(); // hideTempo
        }

        // GP4+: allTracks flags
        if (_version >= 4) _r.readSignedByte();

        // GP5: wah + RSE
        if (_version >= 5) {
            _r.readSignedByte();        // wah
            _r.readIntByteSizeString(); // RSE effect category
            _r.readIntByteSizeString(); // RSE effect
        }

        return (tempo >= 0) ? tempo : -1;
    }
};

// ═══════════════════════════════════════════════════════════════════════
//  GPX Decompression (GP6 / GPX bitstream format)
// ═══════════════════════════════════════════════════════════════════════
static QByteArray decompressGPX(const QByteArray& data) {
    if (data.size() < 8) return QByteArray();
    const quint8* d = reinterpret_cast<const quint8*>(data.constData());
    int len = data.size();

    // Check GPX header: "BCFZ" magic
    if (d[0] != 'B' || d[1] != 'C' || d[2] != 'F' || d[3] != 'Z') {
        // Not BCFZ — might be raw XML or another format
        return data;
    }

    // Read expected uncompressed size (little-endian int at offset 4)
    quint32 expectedSize = d[4] | (d[5] << 8) | (d[6] << 16) | (d[7] << 24);
    QByteArray result;
    result.reserve(expectedSize);

    int srcPos = 8;
    while (srcPos < len) {
        // Read flag bits (4 bytes, little-endian)
        if (srcPos + 4 > len) break;
        quint32 flags = d[srcPos] | (d[srcPos+1] << 8) | (d[srcPos+2] << 16) | (d[srcPos+3] << 24);
        srcPos += 4;

        for (int bit = 0; bit < 32 && srcPos < len; bit++) {
            if (flags & (1u << bit)) {
                // Compressed block: 2-byte back-reference
                if (srcPos + 2 > len) break;
                quint16 word = d[srcPos] | (d[srcPos+1] << 8);
                srcPos += 2;
                int offset = (word >> 4) + 1;
                int length = (word & 0x0F) + 3;
                int start = result.size() - offset;
                for (int i = 0; i < length; i++) {
                    if (start + i >= 0 && start + i < result.size())
                        result.append(result.at(start + i));
                    else
                        result.append('\0');
                }
            } else {
                // Literal byte
                result.append(static_cast<char>(d[srcPos++]));
            }
        }
    }
    return result;
}

// Extract inner XML from decompressed GPX data
// The decompressed data contains a sector-based file system:
//   Header: sector_size (4 bytes)
//   File table sectors: entries of [offset(4), size(4), name(null-terminated)]
//   Data sectors: raw file content
static QString extractXmlFromGPX(const QByteArray& decompressed) {
    if (decompressed.isEmpty()) return QString();
    const quint8* d = reinterpret_cast<const quint8*>(decompressed.constData());
    int len = decompressed.size();

    // Read sector size from header
    if (len < 4) return QString();
    int sectorSize = d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
    if (sectorSize <= 0 || sectorSize > 65536) sectorSize = 4096;

    // The file table starts at offset = sectorSize (after header sector)
    // and continues for multiple sectors until we reach the data area.
    // Each file entry in the table is:
    //   offset (4 bytes, in sectors from data start)
    //   size   (4 bytes, in bytes)
    //   name   (null-terminated, padded to fill rest of entry space)

    struct FileEntry {
        int sectorOffset;  // sector index into data area
        int size;          // byte size of file
        QString name;
    };
    QList<FileEntry> files;

    // Scan file table sectors
    int fileTableStart = sectorSize;
    int sectorCount = (len - sectorSize) / sectorSize; // total sectors after header

    // Iterate over file table sector(s)
    int pos = fileTableStart;
    bool foundDataArea = false;

    while (pos + 8 <= len) {
        // Read potential file entry header
        int entryOffset = d[pos] | (d[pos+1] << 8) | (d[pos+2] << 16) | (d[pos+3] << 24);
        int entrySize   = d[pos+4] | (d[pos+5] << 8) | (d[pos+6] << 16) | (d[pos+7] << 24);

        // Heuristic: if both values are 0 or size is negative, we're past the file table
        if (entrySize <= 0 && entryOffset == 0) {
            pos += 8;
            // Skip remaining null padding in this sector
            int nextSector = ((pos - fileTableStart + sectorSize - 1) / sectorSize) * sectorSize + fileTableStart;
            pos = nextSector;
            break;
        }
        if (entrySize <= 0 || entrySize > len) {
            break;
        }

        // Read null-terminated filename after the 8-byte header
        QString name;
        int nameStart = pos + 8;
        for (int i = nameStart; i < len && d[i] != 0; i++) {
            name += QChar(d[i]);
        }

        FileEntry fe;
        fe.sectorOffset = entryOffset;
        fe.size = entrySize;
        fe.name = name;
        files.append(fe);

        // Advance past this entry — entries seem to be variable-sized
        // The name is null-terminated, followed by padding. Skip to next entry.
        // File table entries are typically 128 bytes in GP6 format
        pos += 128; // standard GP6 file table entry size

        // If we've crossed a sector boundary, align to next sector
        if (pos >= fileTableStart + sectorSize) {
            // Check if next sector is still file table or data
            int nextSectorStart = ((pos - fileTableStart + sectorSize - 1) / sectorSize) * sectorSize + fileTableStart;
            if (nextSectorStart + 8 <= len) {
                int peek1 = d[nextSectorStart] | (d[nextSectorStart+1] << 8) | (d[nextSectorStart+2] << 16) | (d[nextSectorStart+3] << 24);
                int peek2 = d[nextSectorStart+4] | (d[nextSectorStart+5] << 8) | (d[nextSectorStart+6] << 16) | (d[nextSectorStart+7] << 24);
                if (peek2 <= 0 && peek1 == 0) {
                    pos = nextSectorStart + sectorSize; // skip empty sector, move to data
                    break;
                }
            }
        }
    }

    // Data area starts after file table sectors
    int dataAreaStart = pos;

    // Try to find and extract "score.gpif" first, then any .gpif file, then largest file
    int bestIdx = -1;
    int largestIdx = -1;
    int largestSize = 0;

    for (int i = 0; i < files.size(); i++) {
        if (files[i].name.contains("score.gpif", Qt::CaseInsensitive)) {
            bestIdx = i;
            break;
        }
        if (files[i].name.endsWith(".gpif", Qt::CaseInsensitive) && bestIdx < 0) {
            bestIdx = i;
        }
        if (files[i].size > largestSize) {
            largestSize = files[i].size;
            largestIdx = i;
        }
    }

    if (bestIdx < 0) bestIdx = largestIdx;

    if (bestIdx >= 0) {
        const FileEntry& fe = files[bestIdx];
        // File data is at dataAreaStart + (sectorOffset * sectorSize)
        int fileDataPos = dataAreaStart + fe.sectorOffset * sectorSize;
        if (fileDataPos >= 0 && fileDataPos + fe.size <= len) {
            return QString::fromUtf8(decompressed.mid(fileDataPos, fe.size));
        }
        // Fallback: try treating sectorOffset as a direct byte offset
        if (fe.sectorOffset >= 0 && fe.sectorOffset + fe.size <= len) {
            return QString::fromUtf8(decompressed.mid(fe.sectorOffset, fe.size));
        }
    }

    // Fallback: scan for XML markers in the raw decompressed data
    // (handles edge cases where the file table parsing didn't work)
    for (int i = 0; i + 5 < len; i++) {
        if (d[i] == '<' && (
            (d[i+1] == '?' && d[i+2] == 'x' && d[i+3] == 'm' && d[i+4] == 'l') ||
            (d[i+1] == 'G' && d[i+2] == 'P' && d[i+3] == 'I' && d[i+4] == 'F') ||
            (d[i+1] == 'S' && d[i+2] == 'c' && d[i+3] == 'o' && d[i+4] == 'r'))) {
            // Found XML start — extract until end; scan for closing tag or EOF
            // Build the string skipping null bytes (which appear between sectors)
            QByteArray xmlData;
            xmlData.reserve(len - i);
            for (int j = i; j < len; j++) {
                if (d[j] != 0) xmlData.append(static_cast<char>(d[j]));
            }
            return QString::fromUtf8(xmlData);
        }
    }

    return QString();
}

// ═══════════════════════════════════════════════════════════════════════
//  GP7 ZIP extraction (minimal ZIP reader for score.gpif)
// ═══════════════════════════════════════════════════════════════════════
static QString extractGP7Xml(const QByteArray& data) {
    // GP7 files are ZIP archives containing Content/score.gpif
    const quint8* d = reinterpret_cast<const quint8*>(data.constData());
    int len = data.size();

    // Search for local file headers (PK\x03\x04)
    for (int pos = 0; pos + 30 < len; ) {
        if (d[pos] != 0x50 || d[pos+1] != 0x4B || d[pos+2] != 0x03 || d[pos+3] != 0x04) {
            pos++;
            continue;
        }

        // Parse local file header
        quint16 compression = d[pos+8] | (d[pos+9] << 8);
        quint32 compSize = d[pos+18] | (d[pos+19] << 8) | (d[pos+20] << 16) | (d[pos+21] << 24);
        quint32 uncompSize = d[pos+22] | (d[pos+23] << 8) | (d[pos+24] << 16) | (d[pos+25] << 24);
        quint16 nameLen = d[pos+26] | (d[pos+27] << 8);
        quint16 extraLen = d[pos+28] | (d[pos+29] << 8);

        if (pos + 30 + nameLen > len) break;
        QString fileName = QString::fromLatin1(reinterpret_cast<const char*>(d + pos + 30), nameLen);

        int dataStart = pos + 30 + nameLen + extraLen;

        // Check if this is the score file
        bool isScoreFile = fileName.endsWith("score.gpif", Qt::CaseInsensitive) ||
                          fileName.endsWith(".gpif", Qt::CaseInsensitive);

        if (isScoreFile && dataStart + (int)compSize <= len) {
            QByteArray fileData;
            if (compression == 0) {
                // Stored (no compression)
                fileData = QByteArray(reinterpret_cast<const char*>(d + dataStart), compSize);
            } else if (compression == 8) {
                // Deflate — use zlib raw inflate (ZIP stores raw deflate, not zlib format)
                QByteArray compressed(reinterpret_cast<const char*>(d + dataStart), compSize);
                fileData.resize(uncompSize + 256); // small extra buffer

                z_stream stream;
                memset(&stream, 0, sizeof(stream));
                stream.next_in = reinterpret_cast<Bytef*>(compressed.data());
                stream.avail_in = compressed.size();
                stream.next_out = reinterpret_cast<Bytef*>(fileData.data());
                stream.avail_out = fileData.size();

                // -MAX_WBITS = raw deflate (no zlib/gzip header)
                if (inflateInit2(&stream, -MAX_WBITS) == Z_OK) {
                    int ret = inflate(&stream, Z_FINISH);
                    inflateEnd(&stream);
                    if (ret == Z_STREAM_END || ret == Z_OK) {
                        fileData.resize(stream.total_out);
                    } else {
                        fileData.clear();
                    }
                } else {
                    fileData.clear();
                }
            }

            if (!fileData.isEmpty()) {
                return QString::fromUtf8(fileData);
            }
        }

        // Move to next entry
        pos = dataStart + compSize;
        if (pos <= dataStart) break; // overflow guard: compSize was 0 or wrapped
    }

    return QString();
}

// ═══════════════════════════════════════════════════════════════════════
//  GPIF XML → GPTrack / GPMeasureHeader conversion
// ═══════════════════════════════════════════════════════════════════════
struct GPIFRhythm {
    int noteValue = 4;  // 1=whole, 2=half, 4=quarter, etc.
    int dots = 0;
    int tupletNum = 1;
    int tupletDen = 1;

    int ticks() const {
        if (noteValue <= 0) return Duration::QUARTER_TIME;
        int base = static_cast<int>(Duration::QUARTER_TIME * (4.0 / noteValue));
        int result = base;
        if (dots >= 1) result += base / 2;
        if (dots >= 2) result += base / 4;
        if (tupletNum > 0 && tupletNum != tupletDen)
            result = static_cast<int>(result * tupletDen / static_cast<double>(tupletNum));
        return std::max(result, 1);
    }
};

static int parseNoteValue(const QString& s) {
    if (s == "Whole") return 1;
    if (s == "Half") return 2;
    if (s == "Quarter") return 4;
    if (s == "Eighth" || s == "8th") return 8;
    if (s == "16th") return 16;
    if (s == "32nd") return 32;
    if (s == "64th") return 64;
    return 4;
}

static bool parseGPIF(const QString& xml,
                      QList<GPTrack>& outTracks,
                      QList<GPMeasureHeader>& outHeaders,
                      int& outTempo) {
    QXmlStreamReader reader(xml);
    outTempo = 120;

    // Lookup tables from XML
    QMap<QString, GPIFRhythm> rhythms;  // id → rhythm
    struct XMLNote { int midiNote; int velocity; bool isTied; int fret; int str; };
    QMap<QString, XMLNote> notes;       // id → note
    struct XMLBeat { QString rhythmRef; QStringList noteRefs; };
    QMap<QString, XMLBeat> beats;       // id → beat
    struct XMLVoice { QStringList beatRefs; };
    QMap<QString, XMLVoice> voices;     // id → voice
    struct XMLBar {
        QStringList voiceRefs;
        int simileMark = 0; // 1=simple, 2=firstOfDouble, 3=secondOfDouble
    };
    QMap<QString, XMLBar> bars;         // id → bar

    // Track info from XML
    struct XMLTrackInfo {
        QString name;
        int channel = 0;
        int program = 25;
        QList<int> tuning; // low to high string MIDI values
        int capo = 0;
    };
    QList<XMLTrackInfo> xmlTracks;

    // MasterBar info
    struct XMLMasterBar {
        int num = 4, den = 4;
        QStringList barRefs;
    };
    QList<XMLMasterBar> masterBars;

    // Automation (tempo) events
    struct TempoAuto { int bar; double pos; double tempo; };
    QList<TempoAuto> tempoAutos;

    // Parse the XML
    QString currentSection;
    QString currentElement;
    QString currentId;
    int nestLevel = 0;

    while (!reader.atEnd()) {
        reader.readNext();

        if (reader.isStartElement()) {
            QString name = reader.name().toString();

            // Top-level sections
            if (name == "Score" || name == "MasterTrack" || name == "Tracks" ||
                name == "MasterBars" || name == "Bars" || name == "Voices" ||
                name == "Beats" || name == "Notes" || name == "Rhythms") {
                currentSection = name;
                continue;
            }

            // Tempo via Score element
            if (currentSection == "Score" && name == "Tempo") {
                QString tempoText = reader.readElementText();
                int t = tempoText.toInt();
                if (t > 0) outTempo = t;
                continue;
            }

            // MasterTrack automations (tempo)
            if (currentSection == "MasterTrack" && name == "Automation") {
                // Read sub-elements: Type, Value, Bar, Position
                QString aType, aValue;
                int aBar = 0;
                double aPos = 0.0;
                while (!reader.atEnd()) {
                    reader.readNext();
                    if (reader.isEndElement() && reader.name().toString() == "Automation") break;
                    if (reader.isStartElement()) {
                        QString sub = reader.name().toString();
                        if (sub == "Type") aType = reader.readElementText();
                        else if (sub == "Value") aValue = reader.readElementText();
                        else if (sub == "Bar") aBar = reader.readElementText().toInt();
                        else if (sub == "Position") aPos = reader.readElementText().toDouble();
                    }
                }
                if (aType == "Tempo") {
                    QStringList parts = aValue.split(' ', Qt::SkipEmptyParts);
                    if (!parts.isEmpty()) {
                        double tempo = parts.first().toDouble();
                        if (parts.size() > 1) {
                            int tType = parts[1].toInt();
                            if (tType == 1) tempo /= 2.0;
                            else if (tType == 3) tempo *= 1.5;
                            else if (tType == 4) tempo *= 2.0;
                            else if (tType == 5) tempo *= 3.0;
                        }
                        if (tempo > 0) {
                            tempoAutos.append({aBar, aPos, tempo});
                            if (aBar == 0 && aPos == 0.0) outTempo = static_cast<int>(tempo);
                        }
                    }
                }
                continue;
            }

            // Track definitions
            if (currentSection == "Tracks" && name == "Track") {
                XMLTrackInfo ti;
                if (reader.attributes().hasAttribute("id"))
                    currentId = reader.attributes().value("id").toString();

                while (!reader.atEnd()) {
                    reader.readNext();
                    if (reader.isEndElement() && reader.name().toString() == "Track") break;
                    if (!reader.isStartElement()) continue;
                    QString sub = reader.name().toString();
                    if (sub == "Name") ti.name = reader.readElementText();
                    else if (sub == "Channel" || sub == "MidiChannel") ti.channel = reader.readElementText().toInt();
                    else if (sub == "Program" || sub == "ProgramChange") ti.program = reader.readElementText().toInt();
                    else if (sub == "Capo") ti.capo = reader.readElementText().toInt();
                    else if (sub == "Property") {
                        QString propName;
                        if (reader.attributes().hasAttribute("name"))
                            propName = reader.attributes().value("name").toString();
                        if (propName == "Tuning") {
                            while (!reader.atEnd()) {
                                reader.readNext();
                                if (reader.isEndElement() && reader.name().toString() == "Property") break;
                                if (reader.isStartElement() && reader.name().toString() == "Pitches") {
                                    QString pitches = reader.readElementText();
                                    for (const QString& p : pitches.split(' ', Qt::SkipEmptyParts))
                                        ti.tuning.append(p.toInt());
                                }
                            }
                        }
                    }
                }
                xmlTracks.append(ti);
                continue;
            }

            // MasterBar definitions
            if (currentSection == "MasterBars" && name == "MasterBar") {
                XMLMasterBar mb;
                while (!reader.atEnd()) {
                    reader.readNext();
                    if (reader.isEndElement() && reader.name().toString() == "MasterBar") break;
                    if (!reader.isStartElement()) continue;
                    QString sub = reader.name().toString();
                    if (sub == "Time") {
                        QString ts = reader.readElementText(); // e.g. "4/4"
                        QStringList parts = ts.split('/');
                        if (parts.size() == 2) {
                            mb.num = parts[0].toInt();
                            mb.den = parts[1].toInt();
                        }
                    } else if (sub == "Bars") {
                        mb.barRefs = reader.readElementText().split(' ', Qt::SkipEmptyParts);
                    }
                }
                masterBars.append(mb);
                continue;
            }

            // Bar definitions
            if (currentSection == "Bars" && name == "Bar") {
                QString id = reader.attributes().value("id").toString();
                XMLBar bar;
                while (!reader.atEnd()) {
                    reader.readNext();
                    if (reader.isEndElement() && reader.name().toString() == "Bar") break;
                    if (reader.isStartElement() && reader.name().toString() == "Voices")
                        bar.voiceRefs = reader.readElementText().split(' ', Qt::SkipEmptyParts);
                    if (reader.isStartElement() && reader.name().toString() == "SimileMark") {
                        QString sm = reader.readElementText();
                        if (sm == "Simple") bar.simileMark = 1;
                        else if (sm == "FirstOfDouble") bar.simileMark = 2;
                        else if (sm == "SecondOfDouble") bar.simileMark = 3;
                    }
                }
                bars[id] = bar;
                continue;
            }

            // Voice definitions
            if (currentSection == "Voices" && name == "Voice") {
                QString id = reader.attributes().value("id").toString();
                XMLVoice voice;
                while (!reader.atEnd()) {
                    reader.readNext();
                    if (reader.isEndElement() && reader.name().toString() == "Voice") break;
                    if (reader.isStartElement() && reader.name().toString() == "Beats")
                        voice.beatRefs = reader.readElementText().split(' ', Qt::SkipEmptyParts);
                }
                voices[id] = voice;
                continue;
            }

            // Beat definitions
            if (currentSection == "Beats" && name == "Beat") {
                QString id = reader.attributes().value("id").toString();
                XMLBeat beat;
                while (!reader.atEnd()) {
                    reader.readNext();
                    if (reader.isEndElement() && reader.name().toString() == "Beat") break;
                    if (!reader.isStartElement()) continue;
                    QString sub = reader.name().toString();
                    if (sub == "Rhythm") {
                        if (reader.attributes().hasAttribute("ref"))
                            beat.rhythmRef = reader.attributes().value("ref").toString();
                    } else if (sub == "Notes") {
                        beat.noteRefs = reader.readElementText().split(' ', Qt::SkipEmptyParts);
                    }
                }
                beats[id] = beat;
                continue;
            }

            // Note definitions
            if (currentSection == "Notes" && name == "Note") {
                QString id = reader.attributes().value("id").toString();
                XMLNote note;
                note.midiNote = 60;
                note.velocity = 100;
                note.isTied = false;
                note.fret = 0;
                note.str = -1;
                while (!reader.atEnd()) {
                    reader.readNext();
                    if (reader.isEndElement() && reader.name().toString() == "Note") break;
                    if (!reader.isStartElement()) continue;
                    QString sub = reader.name().toString();
                    if (sub == "Velocity") note.velocity = reader.readElementText().toInt() * 127 / 16;
                    else if (sub == "Tie") {
                        if (reader.attributes().value("destination").toString() == "true")
                            note.isTied = true;
                    }
                    else if (sub == "Property") {
                        QString pn;
                        if (reader.attributes().hasAttribute("name"))
                            pn = reader.attributes().value("name").toString();
                        if (pn == "Fret") {
                            while (!reader.atEnd()) {
                                reader.readNext();
                                if (reader.isEndElement() && reader.name().toString() == "Property") break;
                                if (reader.isStartElement() && reader.name().toString() == "Fret")
                                    note.fret = reader.readElementText().toInt();
                            }
                        } else if (pn == "String") {
                            while (!reader.atEnd()) {
                                reader.readNext();
                                if (reader.isEndElement() && reader.name().toString() == "Property") break;
                                if (reader.isStartElement() && reader.name().toString() == "String")
                                    note.str = reader.readElementText().toInt();
                            }
                        } else if (pn == "Midi") {
                            while (!reader.atEnd()) {
                                reader.readNext();
                                if (reader.isEndElement() && reader.name().toString() == "Property") break;
                                if (reader.isStartElement() && reader.name().toString() == "Number")
                                    note.midiNote = reader.readElementText().toInt();
                            }
                        }
                    }
                }
                notes[id] = note;
                continue;
            }

            // Rhythm definitions
            if (currentSection == "Rhythms" && name == "Rhythm") {
                QString id = reader.attributes().value("id").toString();
                GPIFRhythm rhythm;
                while (!reader.atEnd()) {
                    reader.readNext();
                    if (reader.isEndElement() && reader.name().toString() == "Rhythm") break;
                    if (!reader.isStartElement()) continue;
                    QString sub = reader.name().toString();
                    if (sub == "NoteValue") rhythm.noteValue = parseNoteValue(reader.readElementText());
                    else if (sub == "AugmentationDot") { rhythm.dots++; reader.readElementText(); }
                    else if (sub == "PrimaryTuplet") {
                        if (reader.attributes().hasAttribute("num"))
                            rhythm.tupletNum = reader.attributes().value("num").toInt();
                        if (reader.attributes().hasAttribute("den"))
                            rhythm.tupletDen = reader.attributes().value("den").toInt();
                    }
                }
                rhythms[id] = rhythm;
                continue;
            }
        }
    }

    if (reader.hasError()) {
        qWarning() << "GPIF XML parse error:" << reader.errorString();
    }

    // Default tuning if tracks have none
    QList<int> defaultTuning = {40, 45, 50, 55, 59, 64}; // E2 A2 D3 G3 B3 E4

    // ── Build GPTracks ──
    for (int t = 0; t < xmlTracks.size(); t++) {
        const XMLTrackInfo& xti = xmlTracks[t];
        GPTrack gpt;
        gpt.name = xti.name;
        gpt.channel = xti.channel;
        gpt.instrument = xti.program;
        gpt.capo = xti.capo;

        QList<int> tuning = xti.tuning.isEmpty() ? defaultTuning : xti.tuning;
        for (int s = 0; s < tuning.size(); s++) {
            GuitarString gs;
            gs.number = s + 1;
            gs.value = tuning[s];
            gpt.strings.append(gs);
        }
        outTracks.append(gpt);
    }

    // ── Build GPMeasureHeaders + note events ──
    int currentTick = 0;
    QVector<QList<GPNote>> previousMeasureNotes(outTracks.size());
    QVector<QList<GPNote>> doublePreviousMeasureNotes(outTracks.size());

    for (int mbIdx = 0; mbIdx < masterBars.size(); mbIdx++) {
        const XMLMasterBar& mb = masterBars[mbIdx];

        GPMeasureHeader mh;
        mh.numerator = mb.num;
        mh.denominator = mb.den;
        mh.start = currentTick;

        // Compute measure ticks
        int measureLen = mh.length();

        // Exact tick position for tempo automation at this bar
        for (const TempoAuto& ta : tempoAutos) {
            if (ta.bar == mbIdx) {
                int tempoTick = currentTick + static_cast<int>(measureLen * ta.pos);
                // Attach to header if it's strictly at start (legacy compatibility format)
                if (ta.pos <= 0.0 || mh.tempo <= 0) mh.tempo = static_cast<int>(ta.tempo);
                
                // Add explicit global midi event data representation
                GPTrack::TempoEvent te;
                te.tick = tempoTick;
                te.tempo = static_cast<int>(ta.tempo);
                if (!outTracks.isEmpty()) outTracks[0].tempoEvents.append(te);
            }
        }
        outHeaders.append(mh);

        QVector<QList<GPNote>> perTrackCurrentNotes(outTracks.size());

        // Process bars for each track
        for (int trackIdx = 0; trackIdx < mb.barRefs.size() && trackIdx < outTracks.size(); trackIdx++) {
            const QString& barId = mb.barRefs[trackIdx];
            if (!bars.contains(barId)) continue;
            const XMLBar& bar = bars[barId];

            GPTrack& gpt = outTracks[trackIdx];
            QList<int> tuning;
            for (const GuitarString& gs : gpt.strings) tuning.append(gs.value);

            if (bar.simileMark == 1) { // Simple repeat
                for (const GPNote& pn : previousMeasureNotes[trackIdx]) {
                    GPNote copy = pn;
                    copy.tick += measureLen;
                    perTrackCurrentNotes[trackIdx].append(copy);
                }
            } else if (bar.simileMark == 2 || bar.simileMark == 3) { // Double repeat
                for (const GPNote& pn : doublePreviousMeasureNotes[trackIdx]) {
                    GPNote copy = pn;
                    copy.tick += (measureLen * (bar.simileMark == 2 ? 2 : 1)); // offset
                    perTrackCurrentNotes[trackIdx].append(copy);
                }
            } else {
                for (const QString& voiceId : bar.voiceRefs) {
                    if (voiceId == "-1" || !voices.contains(voiceId)) continue;
                    const XMLVoice& voice = voices[voiceId];

                int beatTick = currentTick;
                for (const QString& beatId : voice.beatRefs) {
                    if (!beats.contains(beatId)) continue;
                    const XMLBeat& beat = beats[beatId];

                    // Get rhythm duration
                    int duration = Duration::QUARTER_TIME;
                    if (rhythms.contains(beat.rhythmRef))
                        duration = rhythms[beat.rhythmRef].ticks();

                    // Process notes in this beat
                    for (const QString& noteId : beat.noteRefs) {
                        if (!notes.contains(noteId)) continue;
                        const XMLNote& note = notes[noteId];
                        if (note.isTied) continue;

                        int midiNote = note.midiNote;
                        // If fret+string info available and tuning exists
                        if (note.str >= 0 && note.str < tuning.size()) {
                            midiNote = tuning[note.str] + note.fret + gpt.capo;
                        }

                        GPNote n;
                        n.tick = beatTick;
                        n.str = note.str;
                        n.fret = note.fret;
                        n.fallbackMidiNote = midiNote;
                        n.velocity = qBound(1, note.velocity, 127);
                        n.duration = duration;
                        n.isTied = note.isTied;
                        perTrackCurrentNotes[trackIdx].append(n);
                    }

                    beatTick += duration;
                }
            }
        }
    }

    for (int trackIdx = 0; trackIdx < mb.barRefs.size() && trackIdx < outTracks.size(); trackIdx++) {
        outTracks[trackIdx].noteEvents.append(perTrackCurrentNotes[trackIdx]);
        doublePreviousMeasureNotes[trackIdx] = previousMeasureNotes[trackIdx];
        previousMeasureNotes[trackIdx] = perTrackCurrentNotes[trackIdx];
    }

    currentTick += measureLen;
    }

    return !outTracks.isEmpty();
}

} // namespace GP

static void applyTripletFeel(QList<GP::GPTrack>& tracks, const QList<GP::GPMeasureHeader>& headers) {
    if (headers.isEmpty()) return;
    
    for (GP::GPTrack& track : tracks) {
        for (GP::GPNote& n : track.noteEvents) {
            // Find which measure we are in
            const GP::GPMeasureHeader* currentHeader = &headers.first();
            for (const GP::GPMeasureHeader& h : headers) {
                if (n.tick >= h.start) {
                    currentHeader = &h;
                } else break; 
            }
            
            if (currentHeader->tripletFeel != GP::TripletFeel::None) {
                int measureTick = n.tick - currentHeader->start;
                bool is8ThPos = (measureTick % 480) == 0;
                bool is16ThPos = (measureTick % 240) == 0;
                bool isFirst = true;

                if (is8ThPos) isFirst = (measureTick % 960) == 0;
                if (is16ThPos) isFirst = is8ThPos;
                
                // Assuming simple tuplet filtering
                bool is8Th = (n.duration == 480);
                bool is16Th = (n.duration == 240);
                
                switch (currentHeader->tripletFeel) {
                    case GP::TripletFeel::Eighth:
                        if (is8ThPos && is8Th) {
                            if (isFirst) n.duration = (n.duration * 4) / 3;
                            else {
                                n.duration = (n.duration * 2) / 3;
                                n.tick += (n.duration / 2); // delay 1/3 of orig
                            }
                        }
                        break;
                    case GP::TripletFeel::Sixteenth:
                        if (is16ThPos && is16Th) {
                            if (isFirst) n.duration = (n.duration * 4) / 3;
                            else {
                                n.duration = (n.duration * 2) / 3;
                                n.tick += (n.duration / 2);
                            }
                        }
                        break;
                    case GP::TripletFeel::Scottish8th:
                        if (is8ThPos && is8Th) {
                            if (isFirst) n.duration /= 2;
                            else {
                                n.duration = (n.duration * 3) / 2;
                                n.tick -= (n.duration / 3);
                            }
                        }
                        break;
                    default: break;
                }
            }
        }
    }
}

static void buildMidiTracks(MidiFile* mf, const QList<GP::GPTrack>& gpTracks, double tickScale, int& maxTick) {
    int channelFreeTime[16] = {0};

    for (int t = 0; t < gpTracks.size(); t++) {
        const GP::GPTrack& gpTrack = gpTracks[t];
        int trackIdx = t + 1;
        while (mf->numTracks() <= trackIdx) mf->addTrack();
        MidiTrack* midiTrack = mf->track(trackIdx);
        midiTrack->setName(gpTrack.name.isEmpty() ? QString("Track %1").arg(t+1) : gpTrack.name);
        int midiChannel = gpTrack.channel;

        if (midiChannel != 9) {
            ProgChangeEvent* pc = new ProgChangeEvent(midiChannel, gpTrack.instrument, midiTrack);
            pc->setFile(mf);
            pc->setMidiTime(0, false);
        }

        OffEvent* activeStringOffs[10] = {nullptr};

        // Process tempo macros
        for (const GP::GPTrack::TempoEvent& te : gpTrack.tempoEvents) {
            TempoChangeEvent* tce = new TempoChangeEvent(17, 60000000 / std::max(te.tempo, 1), mf->track(0));
            tce->setFile(mf);
            tce->setMidiTime(static_cast<int>(te.tick * tickScale), false);
        }

        // Process notes
        for (const GP::GPNote& n : gpTrack.noteEvents) {
            int startTick = static_cast<int>(n.tick * tickScale);
            int durTicks = std::max(1, static_cast<int>(n.duration * tickScale));

            int computedMidiNote = n.fallbackMidiNote;
            if (n.str > 0) {
                int openStringMidi = 0;
                for (const GP::GuitarString& gs : gpTrack.strings) {
                    if (gs.number == n.str) { openStringMidi = gs.value; break; }
                }
                
                int effectiveFret = n.fret;
                
                // Harmonics Simulation
                if (n.harmonic == GP::HarmonicType::Natural) {
                    if (n.fret == 12) effectiveFret += 12;
                    else if (n.fret == 7) effectiveFret += 19;
                    else if (n.fret == 5) effectiveFret += 24;
                    else if (n.fret == 4) effectiveFret += 28;
                    else if (n.fret == 3) effectiveFret += 31;
                    else effectiveFret += 12;
                } else if (n.harmonic == GP::HarmonicType::Artificial || 
                           n.harmonic == GP::HarmonicType::Pinch || 
                           n.harmonic == GP::HarmonicType::Tapped) {
                    effectiveFret += 12; // Standard octave artificial bump
                }

                computedMidiNote = std::clamp(openStringMidi + effectiveFret + gpTrack.capo, 0, 127);
            }

            if (computedMidiNote < 0) continue;

            if (n.isTied) {
                if (n.str >= 0 && n.str < 10 && activeStringOffs[n.str]) {
                    int endT = startTick + durTicks;
                    activeStringOffs[n.str]->setMidiTime(endT - 1, false);
                    if (endT > maxTick) maxTick = endT;

                    if (!n.bendPoints.isEmpty()) {
                        int bentCh = activeStringOffs[n.str]->channel();
                        for (const GP::BendPoint& bp : n.bendPoints) {
                            int bTick = startTick + static_cast<int>(bp.index * durTicks / 60.0);
                            int pwVal = static_cast<int>(bp.value * 8191 / 50.0);
                            pwVal = std::clamp(pwVal, -8192, 8191);
                            PitchBendEvent* pw = new PitchBendEvent(bentCh, 8192 + pwVal, midiTrack);
                            pw->setFile(mf);
                            pw->setMidiTime(bTick, false);
                        }
                        PitchBendEvent* pwReset = new PitchBendEvent(bentCh, 8192, midiTrack);
                        pwReset->setFile(mf);
                        pwReset->setMidiTime(endT, false);
                    }
                }
                continue;
            }

            int velocity = n.velocity;
            if (n.isDead || n.isGhost) {
                velocity = static_cast<int>(velocity * 0.7f);
                durTicks = std::max(1, durTicks / 4);
            } else if (n.isPalmMute) {
                velocity = static_cast<int>(velocity * 0.8f);
                durTicks = std::max(1, durTicks / 2);
            } else if (n.isStaccato) {
                durTicks = std::max(1, durTicks / 2);
            }

            int endTick = startTick + durTicks;
            int line = 127 - computedMidiNote;

            int targetChannel = midiChannel;
            if (midiChannel != 9 && (!n.bendPoints.isEmpty() || !gpTrack.tremoloPoints.isEmpty() || n.fading != GP::Fading::None || n.isTrill)) {
                for (int c = 0; c < 16; c++) {
                    if (c != 9 && c != midiChannel && startTick >= channelFreeTime[c]) {
                        targetChannel = c;
                        channelFreeTime[c] = endTick + 1;
                        
                        ProgChangeEvent* pc = new ProgChangeEvent(targetChannel, gpTrack.instrument, midiTrack);
                        pc->setFile(mf);
                        pc->setMidiTime(startTick, false);

                        ControlChangeEvent* volReset = new ControlChangeEvent(targetChannel, 7, 100, midiTrack);
                        volReset->setFile(mf);
                        volReset->setMidiTime(startTick, false);
                        break;
                    }
                }
            }

            int tremLen = 0;
            if (n.tremoloPickingDuration > 0) tremLen = n.tremoloPickingDuration;
            else if (n.isTrill && n.trillDuration > 0) tremLen = n.trillDuration;

            if (tremLen > 0) {
                int scaledTrem = std::max(1, static_cast<int>(tremLen * tickScale));
                int curTick = startTick;
                bool originalFret = true;

                while (curTick + scaledTrem <= startTick + durTicks) {
                    int subMidi = computedMidiNote;
                    // GP represents trillFret directly as the absolute target fret if positive,
                    // but usually it's a relative offset. Wait, `GuitarProToMidi.Console` does `n.effect.trill.fret - tuning`.
                    // We'll safely add standard +1 / +2 for now if it's missing or if trillFret is interpreted relative.
                    if (n.isTrill && !originalFret) subMidi += ((n.trillFret > 0 && n.trillFret < 10) ? n.trillFret : 2);
                    
                    int subLine = 127 - subMidi;
                    NoteOnEvent* subOn = new NoteOnEvent(subMidi, velocity, targetChannel, midiTrack);
                    subOn->setFile(mf);
                    subOn->setMidiTime(curTick, false);
                    
                    OffEvent* subOff = new OffEvent(targetChannel, subLine, midiTrack);
                    subOff->setFile(mf);
                    subOff->setMidiTime(curTick + scaledTrem - 1, false);
                    
                    if (n.str >= 0 && n.str < 10) activeStringOffs[n.str] = subOff;
                    if (curTick + scaledTrem > maxTick) maxTick = curTick + scaledTrem;
                    
                    curTick += scaledTrem;
                    originalFret = !originalFret;
                }
            } else {
                if (n.isGrace && startTick >= n.graceDurationTicks) {
                    int graceStart = startTick - n.graceDurationTicks;
                    int graceScaled = std::max(1, static_cast<int>(n.graceDurationTicks * tickScale));
                    
                    int graceMidi = std::clamp(computedMidiNote - n.fret + n.graceFret, 0, 127);
                    NoteOnEvent* gOn = new NoteOnEvent(graceMidi, n.graceVelocity, targetChannel, midiTrack);
                    gOn->setFile(mf);
                    gOn->setMidiTime(graceStart, false);
                    
                    OffEvent* gOff = new OffEvent(targetChannel, 127 - graceMidi, midiTrack);
                    gOff->setFile(mf);
                    gOff->setMidiTime(startTick - 1, false);
                    
                    // Steal duration from principal note by squishing it slightly or 
                    // shifting the entire principal note index back? Format.cs shifts index forward!
                    // So we shrink principal note duration by graceScaled and shift startTick by graceScaled
                    startTick += graceScaled;
                    durTicks = std::max(1, durTicks - graceScaled);
                }

                NoteOnEvent* noteOn = new NoteOnEvent(computedMidiNote, velocity, targetChannel, midiTrack);
                noteOn->setFile(mf);
                noteOn->setMidiTime(startTick, false);

                OffEvent* noteOff = new OffEvent(targetChannel, line, midiTrack);
                noteOff->setFile(mf);
                noteOff->setMidiTime(endTick - 1, false);
                
                if (n.str >= 0 && n.str < 10) activeStringOffs[n.str] = noteOff;
                if (endTick > maxTick) maxTick = endTick;
            }

            // Simplified Bend via PitchWheel
            if (!n.bendPoints.isEmpty()) {
                for (const GP::BendPoint& bp : n.bendPoints) {
                    int bTick = startTick + static_cast<int>(bp.index * durTicks / 60.0);
                    int pwVal = static_cast<int>(bp.value * 8191 / 50.0);
                    pwVal = std::clamp(pwVal, -8192, 8191);
                    PitchBendEvent* pw = new PitchBendEvent(targetChannel, 8192 + pwVal, midiTrack);
                    pw->setFile(mf);
                    pw->setMidiTime(bTick, false);
                }
                PitchBendEvent* pwReset = new PitchBendEvent(targetChannel, 8192, midiTrack);
                pwReset->setFile(mf);
                pwReset->setMidiTime(endTick, false);
            }
            
            // Fading (Volume Swells)
            if (n.fading == GP::Fading::FadeIn) {
                for (int i = 0; i < 4; i++) {
                    int fTick = startTick + (i * durTicks / 4);
                    int fVol = (127 * i) / 3;
                    ControlChangeEvent* cc = new ControlChangeEvent(targetChannel, 7, fVol, midiTrack);
                    cc->setFile(mf);
                    cc->setMidiTime(fTick, false);
                }
            } else if (n.fading == GP::Fading::VolumeSwell) {
                for (int i = 0; i < 5; i++) {
                    int fTick = startTick + (i * durTicks / 4);
                    int fVol = (i == 2) ? 127 : ((i == 0 || i == 4) ? 0 : 64);
                    ControlChangeEvent* cc = new ControlChangeEvent(targetChannel, 7, fVol, midiTrack);
                    cc->setFile(mf);
                    cc->setMidiTime(fTick, false);
                }
            } else if (n.fading == GP::Fading::FadeOut) {
                for (int i = 0; i < 4; i++) {
                    int fTick = startTick + (i * durTicks / 4);
                    int fVol = 127 - ((127 * i) / 3);
                    ControlChangeEvent* cc = new ControlChangeEvent(midiChannel, 7, fVol, midiTrack);
                    cc->setFile(mf);
                    cc->setMidiTime(fTick, false);
                }
            }
        }
        
        // Tremolo bar
        for (const GP::TremoloPoint& tp : gpTrack.tremoloPoints) {
            int tTick = static_cast<int>(tp.index * tickScale);
            int pwVal = static_cast<int>(tp.value * 8191 / 2.0f); // assuming +-2 semitones
            pwVal = std::clamp(pwVal, -8192, 8191);
            PitchBendEvent* pw = new PitchBendEvent(midiChannel, 8192 + pwVal, midiTrack);
            pw->setFile(mf);
            pw->setMidiTime(tTick, false);
            if (tTick > maxTick) maxTick = tTick;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Public API: Convert parsed GP data → MidiFile
// ═══════════════════════════════════════════════════════════════════════
MidiFile* ImportGuitarPro::loadFile(QString path, bool* ok) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *ok = false;
        return nullptr;
    }
    QByteArray fileData = file.readAll();
    file.close();

    if (fileData.isEmpty()) {
        *ok = false;
        return nullptr;
    }

    // Detect format version from extension
    QString ext = path.toLower();
    int version = 5;
    if (ext.endsWith(".gp3")) version = 3;
    else if (ext.endsWith(".gp4")) version = 4;
    else if (ext.endsWith(".gp5")) version = 5;
    else if (ext.endsWith(".gpx") || ext.endsWith(".gp6") || ext.endsWith(".gp7") || ext.endsWith(".gp")) {
        // GP6/GPX: custom bitstream compression + XML
        // GP7:     ZIP archive containing Content/score.gpif
        QString gpifXml;

        if (ext.endsWith(".gp7") || ext.endsWith(".gp")) {
            gpifXml = GP::extractGP7Xml(fileData);
        } else {
            // GP6/GPX: decompress bitstream, then extract XML
            QByteArray decompressed = GP::decompressGPX(fileData);
            gpifXml = GP::extractXmlFromGPX(decompressed);
        }

        if (gpifXml.isEmpty()) {
            qWarning() << "ImportGuitarPro: Could not extract XML from" << path;
            *ok = false;
            return nullptr;
        }

        // Parse GPIF XML into intermediate structures
        QList<GP::GPTrack> gpTracks;
        QList<GP::GPMeasureHeader> gpHeaders;
        int tempo = 120;

        if (!GP::parseGPIF(gpifXml, gpTracks, gpHeaders, tempo)) {
            qWarning() << "ImportGuitarPro: Failed to parse GPIF XML from" << path;
            *ok = false;
            return nullptr;
        }

        // Build MidiFile from parsed data (reuse the same logic as GP3-5 below)
        MidiFile* mf = new MidiFile();
        OffEvent::clearOnEvents();
        MidiTrack* tempoTrack = mf->track(0);
        int ppq = mf->ticksPerQuarter();
        double tickScale = static_cast<double>(ppq) / GP::Duration::QUARTER_TIME;

        // Set tempo
        if (tempo <= 0) tempo = 120;
        TempoChangeEvent* tempoEv = new TempoChangeEvent(17, 60000000 / tempo, tempoTrack);
        tempoEv->setFile(mf);
        tempoEv->setMidiTime(0, false);

        // Time signatures and tempo changes from headers
        int prevNum = -1, prevDen = -1, prevTempo = tempo;
        for (const GP::GPMeasureHeader& mh : gpHeaders) {
            int tick = static_cast<int>(mh.start * tickScale);
            if (mh.numerator != prevNum || mh.denominator != prevDen) {
                int denPow = static_cast<int>(std::log2(std::max(mh.denominator, 1)));
                TimeSignatureEvent* ts = new TimeSignatureEvent(18, mh.numerator, denPow, 24, 8, tempoTrack);
                ts->setFile(mf);
                ts->setMidiTime(tick, false);
                prevNum = mh.numerator;
                prevDen = mh.denominator;
            }
        }

        // Create tracks with note events
        int maxTick = 0;
        GP::applyTripletFeel(gpTracks, gpHeaders);
        GP::buildMidiTracks(mf, gpTracks, tickScale, maxTick);

        OffEvent::clearOnEvents();
        if (maxTick > 0) mf->setMidiTicks(maxTick);
        mf->setPath(path);
        mf->setSaved(true);
        *ok = true;
        return mf;
    }

    // Parse GP3/GP4/GP5 binary
    GP::BinaryReader reader(fileData);
    GP::GPParser parser(reader, version);

    if (!parser.parse()) {
        qWarning() << "ImportGuitarPro: Failed to parse" << path;
        *ok = false;
        return nullptr;
    }

    // Create a new MidiFile and populate it
    MidiFile* mf = new MidiFile();
    OffEvent::clearOnEvents();

    MidiTrack* tempoTrack = mf->track(0);
    int ppq = mf->ticksPerQuarter();
    double tickScale = static_cast<double>(ppq) / GP::Duration::QUARTER_TIME;

    // Insert initial tempo
    int globalTempo = parser.globalTempo();
    if (globalTempo <= 0) globalTempo = 120;
    int microsPerQuarter = 60000000 / globalTempo;

    TempoChangeEvent* tempoEv = new TempoChangeEvent(17, microsPerQuarter, tempoTrack);
    tempoEv->setFile(mf);
    tempoEv->setMidiTime(0, false);

    // Insert initial time signature from the first measure header
    if (!parser.measureHeaders().isEmpty()) {
        const GP::GPMeasureHeader& first = parser.measureHeaders().first();
        int num = first.numerator;
        int denPow = static_cast<int>(std::log2(std::max(first.denominator, 1)));
        TimeSignatureEvent* ts = new TimeSignatureEvent(18, num, denPow, 24, 8, tempoTrack);
        ts->setFile(mf);
        ts->setMidiTime(0, false);
    }

    // Insert tempo changes found in measure headers
    int prevTempo = globalTempo;
    for (const GP::GPMeasureHeader& mh : parser.measureHeaders()) {
        if (mh.tempo > 0 && mh.tempo != prevTempo) {
            int tick = static_cast<int>(mh.start * tickScale);
            int micros = 60000000 / mh.tempo;
            TempoChangeEvent* te = new TempoChangeEvent(17, micros, tempoTrack);
            te->setFile(mf);
            te->setMidiTime(tick, false);
            prevTempo = mh.tempo;
        }
    }

    // Insert time signature changes
    int prevNum = -1, prevDen = -1;
    for (const GP::GPMeasureHeader& mh : parser.measureHeaders()) {
        if (mh.numerator != prevNum || mh.denominator != prevDen) {
            int tick = static_cast<int>(mh.start * tickScale);
            int denPow = static_cast<int>(std::log2(std::max(mh.denominator, 1)));
            TimeSignatureEvent* ts = new TimeSignatureEvent(18, mh.numerator, denPow, 24, 8, tempoTrack);
            ts->setFile(mf);
            ts->setMidiTime(tick, false);
            prevNum = mh.numerator;
            prevDen = mh.denominator;
        }
    }

    // Create tracks with note events
    int maxTick = 0;
    QList<GP::GPTrack> gpTracks = parser.tracks();
    GP::applyTripletFeel(gpTracks, parser.measureHeaders());
    GP::buildMidiTracks(mf, gpTracks, tickScale, maxTick);

    OffEvent::clearOnEvents();

    // Set file total length directly from ticks
    if (maxTick > 0) {
        mf->setMidiTicks(maxTick);
    }

    mf->setPath(path);
    mf->setSaved(true);

    *ok = true;
    return mf;
}
