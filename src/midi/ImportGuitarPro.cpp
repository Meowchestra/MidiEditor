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

struct GPNote {
    int str = 0;         // string number (1-based)
    int fret = 0;        // fret number
    int velocity = 100;
    bool isTied = false;
    bool isDead = false;
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

    struct NoteEvent {
        int tick;
        int midiNote;
        int velocity;
        int duration;
    };
    QList<NoteEvent> noteEvents;
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
                _r.readByte(); // tripletFeel
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
            // MIDI channel index (1-based → 0-based)
            int channelIdx = _r.readInt() - 1;
            track.channel = std::clamp(channelIdx, 0, 15);
            // Channel for effects (skip)
            _r.readInt();
            // Number of frets (skip)
            _r.readInt();
            // Capo
            track.capo = _r.readInt();
            // Track color (skip)
            _r.readInt();

            // Look up instrument from MIDI channel data
            int midiChPort = track.channel; // simplified: use channel index directly
            if (midiChPort >= 0 && midiChPort < 64) {
                int inst = _midiChannels[midiChPort].instrument;
                track.instrument = std::clamp(inst, 0, 127);
            }
            // Percussion
            if (flags & 0x01) {
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

        // Beat effects
        if (flags & 0x08) skipBeatEffects();

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

        // Convert to MIDI events
        int beatDuration = dur.time();

        if (!isEmpty && !isRest) {
            for (const GPNote& n : notes) {
                if (n.isDead || n.isTied) continue;

                int openStringMidi = 0;
                for (const GuitarString& gs : track.strings) {
                    if (gs.number == n.str) {
                        openStringMidi = gs.value;
                        break;
                    }
                }
                int midiNote = std::clamp(openStringMidi + n.fret + track.capo, 0, 127);
                int vel = std::clamp(n.velocity, 1, 127);

                GPTrack::NoteEvent ne;
                ne.tick = startTick;
                ne.midiNote = midiNote;
                ne.velocity = vel;
                ne.duration = beatDuration;
                track.noteEvents.append(ne);
            }
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
        if (flags & 0x08) skipNoteEffects();

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

    void skipBeatEffects() {
        quint8 flags1 = _r.readByte();
        quint8 flags2 = 0;
        if (_version >= 4) {
            flags2 = _r.readByte(); // GP4/GP5: two flags bytes
        }

        if (flags1 & 0x20) {
            if (_version >= 4) {
                quint8 slapEffect = _r.readByte();
                if (slapEffect == 0) {
                    skipBend(); // tremolo bar (uses bend format in GP4)
                }
            } else {
                // GP3: just reads the tremolo bar value as int
                _r.readInt();
            }
        }

        if (_version >= 4 && (flags2 & 0x04)) {
            skipBend(); // tremolo bar (GP4 flags2)
        }

        if (flags1 & 0x40) {
            _r.readSignedByte(); // stroke down
            _r.readSignedByte(); // stroke up
        }

        if (_version >= 4) {
            if (flags2 & 0x02) _r.readSignedByte(); // pick stroke
        }
    }

    void skipNoteEffects() {
        quint8 flags1 = _r.readByte();
        quint8 flags2 = 0;

        if (_version >= 4) {
            flags2 = _r.readByte(); // GP4/GP5: two flags bytes
        }

        // flags1 bits:
        // 0x01 = bend, 0x02 = hammer, 0x04 = slide (GP3: no data)
        // 0x08 = let ring, 0x10 = grace note

        if (flags1 & 0x01) skipBend();

        if (flags1 & 0x10) {
            // Grace note
            _r.readByte();  // fret
            _r.readByte();  // velocity
            _r.readByte();  // transition (GP3) or duration (GP4+)
            if (_version >= 4) _r.readByte(); // extra byte for GP4
            if (_version >= 5) _r.readByte(); // extra flags for GP5
        }

        if (_version >= 4) {
            // flags2 bits for GP4/GP5:
            // 0x01 = staccato, 0x02 = palm mute, 0x04 = tremolo picking
            // 0x08 = slide, 0x10 = harmonic, 0x20 = trill, 0x40 = vibrato
            if (flags2 & 0x04) _r.readSignedByte(); // tremolo picking duration
            if (flags2 & 0x08) _r.readSignedByte(); // slide type
            if (flags2 & 0x10) {
                // Harmonic
                if (_version >= 5) {
                    qint8 hType = _r.readSignedByte();
                    switch (hType) {
                        case 2: // artificial
                            _r.readByte(); _r.readSignedByte(); _r.readByte();
                            break;
                        case 3: // tapped
                            _r.readByte();
                            break;
                        default: break;
                    }
                } else {
                    _r.readSignedByte(); // GP4 harmonic type (just one byte)
                }
            }
            if (flags2 & 0x20) {
                // Trill
                _r.readSignedByte(); // fret
                _r.readSignedByte(); // period
            }
        } else {
            // GP3: slide flag 0x04 — no extra data to read
        }
    }

    void skipBend() {
        _r.readSignedByte(); // type
        _r.readInt();        // value
        int pointCount = _r.readInt();
        for (int i = 0; i < pointCount && i < 1000; i++) {
            _r.readInt();  // position
            _r.readInt();  // value
            _r.readBool(); // vibrato
        }
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
// The decompressed data contains a file table followed by file data
static QString extractXmlFromGPX(const QByteArray& decompressed) {
    if (decompressed.isEmpty()) return QString();
    const quint8* d = reinterpret_cast<const quint8*>(decompressed.constData());
    int len = decompressed.size();
    int pos = 0;

    // Simple approach: find the XML content (look for "<?xml" or "<GPIF" or "<Score")
    QString asText = QString::fromUtf8(decompressed);
    int xmlStart = asText.indexOf("<?xml");
    if (xmlStart < 0) xmlStart = asText.indexOf("<GPIF");
    if (xmlStart < 0) xmlStart = asText.indexOf("<Score");
    if (xmlStart >= 0) {
        return asText.mid(xmlStart);
    }

    // Try the file table approach
    // Header: 4 bytes sector size
    if (pos + 4 > len) return QString();
    int sectorSize = d[pos] | (d[pos+1] << 8) | (d[pos+2] << 16) | (d[pos+3] << 24);
    pos += 4;
    if (sectorSize <= 0 || sectorSize > 65536) sectorSize = 4096;

    // Skip sectors, look for file entries
    struct FileEntry { int offset; int size; QString name; };
    QList<FileEntry> files;

    while (pos + 8 < len) {
        // Try reading file entry: offset(4), size(4), name(varies)
        int fOffset = d[pos] | (d[pos+1] << 8) | (d[pos+2] << 16) | (d[pos+3] << 24);
        pos += 4;
        int fSize = d[pos] | (d[pos+1] << 8) | (d[pos+2] << 16) | (d[pos+3] << 24);
        pos += 4;

        if (fSize > 0 && fSize < len && fOffset >= 0 && fOffset + fSize <= len) {
            FileEntry fe;
            fe.offset = fOffset;
            fe.size = fSize;
            files.append(fe);
        } else {
            break;
        }
    }

    // Return the largest file (likely the score XML)
    if (!files.isEmpty()) {
        int maxIdx = 0;
        for (int i = 1; i < files.size(); i++) {
            if (files[i].size > files[maxIdx].size) maxIdx = i;
        }
        return QString::fromUtf8(decompressed.mid(files[maxIdx].offset, files[maxIdx].size));
    }

    // Last resort: return as-is
    return asText;
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
                // Deflate — use Qt's qUncompress with zlib raw inflate
                // qUncompress expects zlib format (header+data), so we prepend a minimal header
                QByteArray compressed(reinterpret_cast<const char*>(d + dataStart), compSize);
                // Build zlib-compatible stream: CMF=0x78, FLG=0x01, then deflate data, then Adler32
                QByteArray zlibData;
                zlibData.append('\x78');
                zlibData.append('\x01');
                zlibData.append(compressed);
                // Adler32 placeholder (qUncompress may still work or we try raw)
                quint32 adler = 1; // placeholder
                zlibData.append(static_cast<char>((adler >> 24) & 0xFF));
                zlibData.append(static_cast<char>((adler >> 16) & 0xFF));
                zlibData.append(static_cast<char>((adler >> 8) & 0xFF));
                zlibData.append(static_cast<char>(adler & 0xFF));

                // Prepend expected size for qUncompress (4 bytes, big-endian)
                QByteArray sizeHeader(4, '\0');
                sizeHeader[0] = static_cast<char>((uncompSize >> 24) & 0xFF);
                sizeHeader[1] = static_cast<char>((uncompSize >> 16) & 0xFF);
                sizeHeader[2] = static_cast<char>((uncompSize >> 8) & 0xFF);
                sizeHeader[3] = static_cast<char>(uncompSize & 0xFF);
                fileData = qUncompress(sizeHeader + zlibData);

                if (fileData.isEmpty()) {
                    // Fallback: try without zlib header manipulation
                    fileData = qUncompress(sizeHeader + compressed);
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
    struct XMLBar { QStringList voiceRefs; };
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
                GPTrack::NoteEvent pseudoTempo;
                pseudoTempo.tick = tempoTick;
                pseudoTempo.velocity = static_cast<int>(ta.tempo); // pack tempo
                pseudoTempo.midiNote = -1; // special flag for tempo change
                if (!outTracks.isEmpty()) outTracks[0].noteEvents.append(pseudoTempo);
            }
        }
        outHeaders.append(mh);

        // Process bars for each track
        for (int trackIdx = 0; trackIdx < mb.barRefs.size() && trackIdx < outTracks.size(); trackIdx++) {
            const QString& barId = mb.barRefs[trackIdx];
            if (!bars.contains(barId)) continue;
            const XMLBar& bar = bars[barId];

            GPTrack& gpt = outTracks[trackIdx];
            QList<int> tuning;
            for (const GuitarString& gs : gpt.strings) tuning.append(gs.value);

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

                        GPTrack::NoteEvent ne;
                        ne.tick = beatTick;
                        ne.midiNote = qBound(0, midiNote, 127);
                        ne.velocity = qBound(1, note.velocity, 127);
                        ne.duration = duration;
                        gpt.noteEvents.append(ne);
                    }

                    beatTick += duration;
                }
            }
        }

        currentTick += measureLen;
    }

    return !outTracks.isEmpty();
}

} // namespace GP

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
        int currentTempo = tempo;
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

            for (const GP::GPTrack::NoteEvent& ne : gpTrack.noteEvents) {
                int startTick = static_cast<int>(ne.tick * tickScale);
                
                // Decode special pseudo-tempo events embedded in track 0
                if (ne.midiNote == -1) {
                    if (ne.velocity != currentTempo) {
                        TempoChangeEvent* te = new TempoChangeEvent(17, 60000000 / std::max(ne.velocity, 1), tempoTrack);
                        te->setFile(mf);
                        te->setMidiTime(startTick, false);
                        currentTempo = ne.velocity;
                    }
                    continue;
                }

                int durTicks = std::max(1, static_cast<int>(ne.duration * tickScale));
                int endTick = startTick + durTicks;
                int line = 127 - ne.midiNote;

                NoteOnEvent* noteOn = new NoteOnEvent(ne.midiNote, ne.velocity, midiChannel, midiTrack);
                noteOn->setFile(mf);
                noteOn->setMidiTime(startTick, false);

                OffEvent* noteOff = new OffEvent(midiChannel, line, midiTrack);
                noteOff->setFile(mf);
                noteOff->setMidiTime(endTick - 1, false);

                if (endTick > maxTick) maxTick = endTick;
            }
        }

        OffEvent::clearOnEvents();
        if (maxTick > 0) mf->setMaxLengthMs(mf->msOfTick(maxTick) + 2000);
        mf->calcMaxTime();
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
    const QList<GP::GPTrack>& gpTracks = parser.tracks();

    for (int t = 0; t < gpTracks.size(); t++) {
        const GP::GPTrack& gpTrack = gpTracks[t];

        // Ensure we have enough tracks. Track 0 = tempo, instruments start at 1
        int trackIdx = t + 1;
        while (mf->numTracks() <= trackIdx) {
            mf->addTrack();
        }
        MidiTrack* midiTrack = mf->track(trackIdx);
        midiTrack->setName(gpTrack.name.isEmpty()
            ? QString("Track %1").arg(t + 1) : gpTrack.name);

        int midiChannel = gpTrack.channel;

        // Program change (skip percussion channel)
        if (midiChannel != 9) {
            ProgChangeEvent* pc = new ProgChangeEvent(midiChannel, gpTrack.instrument, midiTrack);
            pc->setFile(mf);
            pc->setMidiTime(0, false);
        }

        // Insert all note events
        for (const GP::GPTrack::NoteEvent& ne : gpTrack.noteEvents) {
            int startTick = static_cast<int>(ne.tick * tickScale);
            int durTicks = std::max(1, static_cast<int>(ne.duration * tickScale));
            int endTick = startTick + durTicks;
            int line = 127 - ne.midiNote;

            NoteOnEvent* noteOn = new NoteOnEvent(ne.midiNote, ne.velocity, midiChannel, midiTrack);
            noteOn->setFile(mf);
            noteOn->setMidiTime(startTick, false);

            OffEvent* noteOff = new OffEvent(midiChannel, line, midiTrack);
            noteOff->setFile(mf);
            noteOff->setMidiTime(endTick - 1, false);

            if (endTick > maxTick) maxTick = endTick;
        }
    }

    OffEvent::clearOnEvents();

    // Set file total length
    if (maxTick > 0) {
        mf->setMaxLengthMs(mf->msOfTick(maxTick) + 2000);
    }

    mf->calcMaxTime();
    mf->setPath(path);
    mf->setSaved(true);

    *ok = true;
    return mf;
}
