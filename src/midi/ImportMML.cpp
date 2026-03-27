/*
 * MidiEditor - MML File Importer
 * Converts MML (Music Macro Language) files to standard MIDI events.
 */

#include "ImportMML.h"
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

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QList>
#include <QMap>
#include <QString>
#include <algorithm>
#include <cctype>

// ═══════════════════════════════════════════════════════════════════════
//  Token types matching MMLLexer.cs
// ═══════════════════════════════════════════════════════════════════════
namespace MML {

enum class TokenType {
    Note, Rest, Octave, OctaveUp, OctaveDown,
    Length, Tempo, Volume, Channel,
    ChordStart, ChordEnd, Dot, Tie,
    Pan, Instrument, Eof
};

struct Token {
    TokenType type = TokenType::Eof;
    int intValue = 0;
    QChar noteChar;
    int accidental = 0;
    int length = 0;
    bool dotted = false;
};

// ═══════════════════════════════════════════════════════════════════════
//  Lexer – tokenizes MML text into a stream of Tokens
// ═══════════════════════════════════════════════════════════════════════
class Lexer {
    QString _src;
    int _pos;

    QString stripComments(const QString& src) {
        QString result;
        int i = 0;
        int len = src.length();
        while (i < len) {
            if (i + 1 < len && src[i] == '/' && src[i + 1] == '/') {
                while (i < len && src[i] != '\n') i++;
            } else if (i + 1 < len && src[i] == '/' && src[i + 1] == '*') {
                i += 2;
                while (i + 1 < len && !(src[i] == '*' && src[i + 1] == '/')) i++;
                if (i + 1 < len) i += 2;
            } else {
                result += src[i++];
            }
        }
        return result;
    }

    void skipWhitespace() {
        while (_pos < _src.length() && _src[_pos].isSpace())
            _pos++;
    }

    QChar peek(int offset = 0) {
        int idx = _pos + offset;
        return (idx >= 0 && idx < _src.length()) ? _src[idx] : QChar('\0');
    }

    int readInt() {
        int start = _pos;
        while (_pos < _src.length() && _src[_pos].isDigit())
            _pos++;
        return (_pos == start) ? 0 : _src.mid(start, _pos - start).toInt();
    }

    int readRequiredInt(int min, int max) {
        int v = readInt();
        if (v == 0 && min > 0) v = min;
        return std::clamp(v, min, max);
    }

    int readOptionalInt() {
        if (_pos >= _src.length() || !_src[_pos].isDigit()) return 0;
        return readInt();
    }

    bool consumeDot() {
        if (_pos < _src.length() && _src[_pos] == '.') {
            _pos++;
            return true;
        }
        return false;
    }

    int readAccidental() {
        if (_pos >= _src.length()) return 0;
        QChar c = _src[_pos];
        if (c == '#' || c == '+') { _pos++; return 1; }
        if (c == '-') { _pos++; return -1; }
        return 0;
    }

public:
    Lexer(const QString& source) {
        _src = stripComments(source.toUpper());
        _pos = 0;
    }

    QList<Token> tokenize() {
        QList<Token> tokens;
        while (_pos < _src.length()) {
            skipWhitespace();
            if (_pos >= _src.length()) break;

            QChar c = _src[_pos];
            Token t;
            bool found = true;

            // "CH" prefix for channel selection (e.g. CH1-CH16)
            if (c == 'C' && peek(1) == 'H') {
                _pos += 2;
                t.type = TokenType::Channel;
                t.intValue = readRequiredInt(1, 16);
            }
            // Note letters
            else if (QString("CDEFGAB").contains(c)) {
                t.type = TokenType::Note;
                t.noteChar = _src[_pos++];
                t.accidental = readAccidental();
                t.length = readOptionalInt();
                t.dotted = consumeDot();
            }
            // Rest
            else if (c == 'R') {
                _pos++;
                t.type = TokenType::Rest;
                t.length = readOptionalInt();
                t.dotted = consumeDot();
            }
            // Octave set
            else if (c == 'O') {
                _pos++;
                t.type = TokenType::Octave;
                t.intValue = readRequiredInt(0, 8);
            }
            // Octave shift
            else if (c == '>') { _pos++; t.type = TokenType::OctaveUp; }
            else if (c == '<') { _pos++; t.type = TokenType::OctaveDown; }
            // Default length
            else if (c == 'L') {
                _pos++;
                t.type = TokenType::Length;
                t.intValue = readRequiredInt(1, 64);
                t.dotted = consumeDot();
            }
            // Tempo
            else if (c == 'T') {
                _pos++;
                t.type = TokenType::Tempo;
                t.intValue = readRequiredInt(20, 600);
            }
            // Volume (0-15 MML style)
            else if (c == 'V') {
                _pos++;
                t.type = TokenType::Volume;
                t.intValue = readRequiredInt(0, 15);
            }
            // Pan
            else if (c == 'P') {
                _pos++;
                t.type = TokenType::Pan;
                t.intValue = readRequiredInt(0, 127);
            }
            // Instrument / Program Change
            else if (c == '@') {
                _pos++;
                t.type = TokenType::Instrument;
                t.intValue = readRequiredInt(0, 127);
            }
            // Chord brackets
            else if (c == '[') { _pos++; t.type = TokenType::ChordStart; }
            else if (c == ']') { _pos++; t.type = TokenType::ChordEnd; }
            // Dot (standalone)
            else if (c == '.') { _pos++; t.type = TokenType::Dot; }
            // Tie
            else if (c == '&') { _pos++; t.type = TokenType::Tie; }
            // Unknown character — skip
            else { _pos++; found = false; }

            if (found) tokens.append(t);
        }

        Token eofT;
        eofT.type = TokenType::Eof;
        tokens.append(eofT);
        return tokens;
    }
};

// ═══════════════════════════════════════════════════════════════════════
//  Channel state tracked per MIDI channel during parsing
// ═══════════════════════════════════════════════════════════════════════
struct ChannelState {
    int octave = 4;
    int defaultLen = 4;   // quarter note
    bool defaultDot = false;
    int volume = 100;     // 0-127 MIDI velocity
    int pan = 64;         // 0-127
    int program = 0;      // GM instrument
    int tick = 0;         // current tick position
};

// ═══════════════════════════════════════════════════════════════════════
//  Parser – converts Token stream to MIDI events on a MidiFile
// ═══════════════════════════════════════════════════════════════════════
class Parser {
    MidiFile* _file;
    QList<Token> _tokens;
    int _pos = 0;
    int _tempo = 120;
    int _channel = 0;
    QMap<int, ChannelState> _channels;
    int _maxTick = 0;

    // Use the file's own ticks-per-quarter for all tick math
    int ppq() const { return _file->ticksPerQuarter(); }

    ChannelState& getOrCreate(int ch) {
        if (!_channels.contains(ch))
            _channels.insert(ch, ChannelState());
        return _channels[ch];
    }

    Token peek() { return _tokens[_pos]; }
    Token advance() { return _tokens[_pos++]; }

    int calcTicks(int len, bool dotted, ChannelState& cur) {
        if (len == 0) {
            len = cur.defaultLen;
            dotted = dotted || cur.defaultDot;
        }
        if (len <= 0) len = 4;
        int ticks = (ppq() * 4) / len;
        if (dotted) ticks = (ticks * 3) / 2;
        return ticks;
    }

    static int noteToMidi(QChar note, int accidental, int octave) {
        // C=0 D=2 E=4 F=5 G=7 A=9 B=11
        static const int semitone[] = { 0, 2, 4, 5, 7, 9, 11 };
        int idx = QString("CDEFGAB").indexOf(note);
        if (idx < 0) idx = 0;
        int midi = 12 * (octave + 1) + semitone[idx] + accidental;
        return std::clamp(midi, 0, 127);
    }

    // Get or create a track for the given MIDI channel.
    // Track 0 is always the tempo track; tracks 1+ are instrument tracks.
    MidiTrack* trackForChannel(int ch) {
        // Map channels to track indices: ch0→track1, ch1→track2, etc.
        int trackIdx = ch + 1;
        while (_file->numTracks() <= trackIdx) {
            _file->addTrack();
        }
        return _file->track(trackIdx);
    }

    void emitNote(Token t, ChannelState& cur, int overrideStartTick = -1) {
        int midiNote = noteToMidi(t.noteChar, t.accidental, cur.octave);
        int ticks = calcTicks(t.length, t.dotted, cur);
        int start = (overrideStartTick >= 0) ? overrideStartTick : cur.tick;

        int totalTicks = ticks;

        // Handle ties: &note extends the duration
        while (peek().type == TokenType::Tie) {
            advance(); // consume '&'
            if (peek().type == TokenType::Note) {
                Token tied = advance();
                int tieTicks = calcTicks(tied.length, tied.dotted, cur);
                totalTicks += tieTicks;
            }
        }

        int endTick = start + totalTicks;
        MidiTrack* track = trackForChannel(_channel);
        int line = 127 - midiNote;

        // Create NoteOn (this auto-registers with OffEvent::enterOnEvent)
        NoteOnEvent* noteOn = new NoteOnEvent(midiNote, cur.volume, _channel, track);
        noteOn->setFile(_file);
        noteOn->setMidiTime(start, false);

        // Create matching OffEvent using (channel, line, track) constructor
        // which auto-pairs with the registered NoteOnEvent via the static map
        OffEvent* noteOff = new OffEvent(_channel, line, track);
        noteOff->setFile(_file);
        noteOff->setMidiTime(endTick - 1, false);

        if (overrideStartTick < 0) {
            cur.tick += totalTicks;
        }
        if (cur.tick > _maxTick) _maxTick = cur.tick;
    }

    void emitChord(ChannelState& cur) {
        QList<Token> chordNotes;
        while (peek().type != TokenType::ChordEnd && peek().type != TokenType::Eof) {
            Token t = advance();
            if (t.type == TokenType::Note) {
                chordNotes.append(t);
            }
        }
        if (peek().type == TokenType::ChordEnd) advance();

        int startTick = cur.tick;
        int maxTicks = 0;

        for (const Token& n : chordNotes) {
            int ticks = calcTicks(n.length, n.dotted, cur);
            if (ticks > maxTicks) maxTicks = ticks;
            emitNote(n, cur, startTick);
        }

        cur.tick = startTick + maxTicks;
        if (cur.tick > _maxTick) _maxTick = cur.tick;
    }

public:
    Parser(MidiFile* file) : _file(file) {}

    int maxTick() const { return _maxTick; }

    void parse(const QList<Token>& tokens) {
        _tokens = tokens;
        _pos = 0;
        _channel = 0;
        _maxTick = 0;

        // Clear the static OnEvent registry to avoid cross-contamination
        OffEvent::clearOnEvents();

        MidiTrack* tempoTrack = _file->track(0);
        ChannelState& cur = getOrCreate(_channel);

        while (peek().type != TokenType::Eof) {
            Token t = advance();
            ChannelState& c = getOrCreate(_channel);

            switch (t.type) {
                case TokenType::Channel:
                    _channel = std::clamp(t.intValue - 1, 0, 15);
                    break;

                case TokenType::Tempo: {
                    _tempo = t.intValue;
                    int micros = 60000000 / std::max(_tempo, 1);
                    TempoChangeEvent* tempoEv = new TempoChangeEvent(17, micros, tempoTrack);
                    tempoEv->setFile(_file);
                    tempoEv->setMidiTime(c.tick, false);
                    break;
                }

                case TokenType::Octave:     c.octave = t.intValue; break;
                case TokenType::OctaveUp:   c.octave = std::min(8, c.octave + 1); break;
                case TokenType::OctaveDown: c.octave = std::max(0, c.octave - 1); break;

                case TokenType::Length:
                    c.defaultLen = t.intValue;
                    c.defaultDot = t.dotted;
                    break;

                case TokenType::Volume:
                    c.volume = static_cast<int>((t.intValue / 15.0) * 127);
                    break;

                case TokenType::Pan: {
                    c.pan = t.intValue;
                    MidiTrack* track = trackForChannel(_channel);
                    // ControlChangeEvent(channel, controller, value, track)
                    ControlChangeEvent* cc = new ControlChangeEvent(_channel, 10, c.pan, track);
                    cc->setFile(_file);
                    cc->setMidiTime(c.tick, false);
                    break;
                }

                case TokenType::Instrument: {
                    c.program = t.intValue;
                    MidiTrack* track = trackForChannel(_channel);
                    // ProgChangeEvent(channel, program, track)
                    ProgChangeEvent* pc = new ProgChangeEvent(_channel, c.program, track);
                    pc->setFile(_file);
                    pc->setMidiTime(c.tick, false);
                    break;
                }

                case TokenType::Rest:
                    c.tick += calcTicks(t.length, t.dotted, c);
                    if (c.tick > _maxTick) _maxTick = c.tick;
                    break;

                case TokenType::Note:
                    emitNote(t, c);
                    break;

                case TokenType::ChordStart:
                    emitChord(c);
                    break;

                default:
                    break;
            }
        }

        // Clean up the static OnEvent registry
        OffEvent::clearOnEvents();
    }
};

} // namespace MML

// ═══════════════════════════════════════════════════════════════════════
//  Mabinogi MML normalization helpers
// ═══════════════════════════════════════════════════════════════════════
static QString normalizeMabiMML(const QString& mml) {
    QString result = mml;

    // y<cc>,<val> → Controller commands
    // CC7/CC11 → Volume (V0-V15), CC10 → Pan (P0-127), others → ignore
    QRegularExpression rxY("y(\\d+)\\s*,\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = rxY.globalMatch(result);
    int offset = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        int cc = m.captured(1).toInt();
        int val = m.captured(2).toInt();
        QString replacement;
        if (cc == 7 || cc == 11)
            replacement = QString("V%1").arg(qRound(val / 127.0 * 15));
        else if (cc == 10)
            replacement = QString("P%1").arg(val);
        // else: ignore (empty replacement)
        result.replace(m.capturedStart() + offset, m.capturedLength(), replacement);
        offset += replacement.length() - m.capturedLength();
    }

    // n<0-127> → direct MIDI note → convert to O<octave><note>
    static const QString noteNames[] = {"C","C+","D","D+","E","F","F+","G","G+","A","A+","B"};
    QRegularExpression rxN("\\bn(\\d+)", QRegularExpression::CaseInsensitiveOption);
    it = rxN.globalMatch(result);
    QString out;
    int lastEnd = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        out += result.mid(lastEnd, m.capturedStart() - lastEnd);
        int midi = qBound(0, m.captured(1).toInt(), 127);
        int octave = (midi / 12) - 1;
        int semi = midi % 12;
        out += QString("O%1%2").arg(octave).arg(noteNames[semi]);
        lastEnd = m.capturedEnd();
    }
    out += result.mid(lastEnd);
    result = out;

    // Normalize whitespace
    return result.simplified();
}

// ═══════════════════════════════════════════════════════════════════════
//  3MLE track data
// ═══════════════════════════════════════════════════════════════════════
struct MMLTrackData {
    QString name;
    int channel;        // 0-based MIDI channel
    int instrument;     // GM program number
    QString mml;
};

// ═══════════════════════════════════════════════════════════════════════
//  Format detection
// ═══════════════════════════════════════════════════════════════════════
static bool isIniFormat(const QString& s) {
    return s.contains("[Settings]") ||
           s.contains(QRegularExpression("^\\[Channel\\d+\\]", QRegularExpression::MultilineOption));
}

static bool isXmlFormat(const QString& s) {
    QString trimmed = s.trimmed();
    return trimmed.startsWith("<");
}

static bool isMabiClipboard(const QString& s) {
    return s.contains("MML@", Qt::CaseInsensitive);
}

static bool looksMabinogi(const QString& s) {
    return s.contains(QRegularExpression("(?i)y\\d+\\s*,\\s*\\d+")) ||
           s.contains(QRegularExpression("(?i)\\bn\\d{1,3}"));
}

// ═══════════════════════════════════════════════════════════════════════
//  3MLE INI format parser
// ═══════════════════════════════════════════════════════════════════════
static QList<MMLTrackData> parse3MLE(const QString& content, int* globalTempo) {
    QList<MMLTrackData> tracks;
    *globalTempo = 120;

    // Split into sections
    QMap<QString, QStringList> sections;
    QString current = "";
    sections[current] = QStringList();

    for (const QString& rawLine : content.split('\n')) {
        QString line = rawLine.trimmed();
        QRegularExpressionMatch secMatch = QRegularExpression("^\\[(.+?)\\]").match(line);
        if (secMatch.hasMatch()) {
            current = secMatch.captured(1).trimmed();
            if (!sections.contains(current))
                sections[current] = QStringList();
        } else {
            sections[current].append(line);
        }
    }

    // Parse [Settings]
    if (sections.contains("Settings")) {
        for (const QString& line : sections["Settings"]) {
            QRegularExpressionMatch m = QRegularExpression("^Tempo\\s*=\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption).match(line);
            if (m.hasMatch()) *globalTempo = m.captured(1).toInt();
        }
    }

    // Parse [Channel1], [Channel2], ...
    QStringList channelKeys;
    for (auto it = sections.constBegin(); it != sections.constEnd(); ++it) {
        if (it.key().contains(QRegularExpression("^Channel\\d+$", QRegularExpression::CaseInsensitiveOption)))
            channelKeys.append(it.key());
    }
    std::sort(channelKeys.begin(), channelKeys.end(), [](const QString& a, const QString& b) {
        return QRegularExpression("\\d+").match(a).captured().toInt()
             < QRegularExpression("\\d+").match(b).captured().toInt();
    });

    for (const QString& key : channelKeys) {
        QRegularExpressionMatch numMatch = QRegularExpression("\\d+").match(key);
        int sectionNum = numMatch.hasMatch() ? numMatch.captured().toInt() : 1;
        int midiChannel = sectionNum - 1; // 0-based

        QStringList mmlParts;
        for (const QString& rawLine : sections[key]) {
            // Strip measure markers /*M n */
            QString line = rawLine;
            line.replace(QRegularExpression("/\\*M\\s*\\d+\\s*\\*/"), "");
            line = line.trimmed();
            if (line.isEmpty()) continue;

            // Parse #using_channel directive
            QRegularExpressionMatch chMatch = QRegularExpression("//\\s*#using_channel\\s*=\\s*(\\d+)").match(line);
            if (chMatch.hasMatch()) {
                midiChannel = chMatch.captured(1).toInt();
                continue;
            }

            // Skip comment lines
            if (line.startsWith("//")) continue;

            // Strip inline comments
            line.replace(QRegularExpression("/\\*.*?\\*/"), "");
            line = line.trimmed();
            if (line.isEmpty()) continue;

            mmlParts.append(normalizeMabiMML(line));
        }

        QString mml = mmlParts.join(" ").trimmed();

        // Skip tempo-only tracks
        QString mmlWithoutTempo = mml;
        mmlWithoutTempo.replace(QRegularExpression("(?i)\\bt\\d+\\s*"), "");
        mmlWithoutTempo = mmlWithoutTempo.trimmed();
        if (mmlWithoutTempo.isEmpty()) {
            QRegularExpressionMatch tm = QRegularExpression("(?i)\\bt(\\d+)").match(mml);
            if (tm.hasMatch() && *globalTempo == 120)
                *globalTempo = tm.captured(1).toInt();
            continue;
        }

        MMLTrackData td;
        td.name = key;
        td.channel = midiChannel;
        td.instrument = -1;
        td.mml = mml;
        tracks.append(td);
    }

    return tracks;
}

// ═══════════════════════════════════════════════════════════════════════
//  XML format parser (MapleStory2, generic <Track>/<melody>/<chord>)
// ═══════════════════════════════════════════════════════════════════════
static QList<MMLTrackData> parseXmlMML(const QString& content, int* globalTempo) {
    QList<MMLTrackData> tracks;
    *globalTempo = 120;

    QXmlStreamReader xml(content);
    int autoCh = 0;

    // Read root element attributes
    while (!xml.atEnd() && !xml.isStartElement()) xml.readNext();
    if (xml.isStartElement()) {
        if (xml.attributes().hasAttribute("tempo"))
            *globalTempo = xml.attributes().value("tempo").toInt();
    }

    // Track/melody/chord elements
    static const QStringList trackTags = {"track","part","channel","voice","melody","harmony","chord","rhythm"};
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        QString tag = xml.name().toString().toLower();
        if (!trackTags.contains(tag)) continue;

        MMLTrackData td;
        int ch = autoCh;
        if (tag == "melody") ch = 0;
        else if (tag == "chord") {
            ch = xml.attributes().hasAttribute("index")
                 ? xml.attributes().value("index").toInt() + 1 : autoCh;
        } else {
            if (xml.attributes().hasAttribute("channel"))
                ch = xml.attributes().value("channel").toInt();
            else if (xml.attributes().hasAttribute("ch"))
                ch = xml.attributes().value("ch").toInt();
        }

        td.channel = ch;
        td.instrument = -1;
        if (xml.attributes().hasAttribute("instrument"))
            td.instrument = xml.attributes().value("instrument").toInt();
        else if (xml.attributes().hasAttribute("program"))
            td.instrument = xml.attributes().value("program").toInt();

        td.name = (tag == "melody") ? "Melody"
                : (tag == "chord") ? QString("Chord %1").arg(ch)
                : QString("Track %1").arg(autoCh + 1);

        td.mml = xml.readElementText().simplified();
        if (!td.mml.isEmpty()) {
            if (td.instrument >= 0 && !td.mml.trimmed().startsWith("@"))
                td.mml = QString("@%1 ").arg(td.instrument) + td.mml;
            tracks.append(td);
        }
        autoCh++;
    }

    return tracks;
}

// ═══════════════════════════════════════════════════════════════════════
//  MML@ clipboard multi-track parser (Mabinogi format)
// ═══════════════════════════════════════════════════════════════════════
static QList<MMLTrackData> parseMabiClipboard(const QString& content, int* globalTempo) {
    QList<MMLTrackData> tracks;
    *globalTempo = 120;
    bool isMabi = looksMabinogi(content);

    int start = content.indexOf("MML@", 0, Qt::CaseInsensitive);
    if (start < 0) return tracks;
    start += 4;
    int end = content.indexOf(';', start);
    QString body = (end >= 0) ? content.mid(start, end - start) : content.mid(start);

    QStringList parts = body.split(',');
    int channel = 0;
    for (const QString& part : parts) {
        QString mml = part.trimmed();
        if (mml.isEmpty()) continue;

        // Normalize if Mabinogi dialect
        if (isMabi) mml = normalizeMabiMML(mml);

        // Check for tempo-only parts
        QString test = mml;
        test.replace(QRegularExpression("(?i)\\bt\\d+\\s*"), "");
        if (test.trimmed().isEmpty()) {
            QRegularExpressionMatch tm = QRegularExpression("(?i)\\bt(\\d+)").match(mml);
            if (tm.hasMatch() && *globalTempo == 120)
                *globalTempo = tm.captured(1).toInt();
            continue;
        }

        MMLTrackData td;
        td.name = QString("Track %1").arg(channel + 1);
        td.channel = channel;
        td.instrument = -1;
        td.mml = mml;
        tracks.append(td);
        channel++;
    }

    // Extract tempo from tracks if still default
    if (*globalTempo == 120) {
        for (const MMLTrackData& t : tracks) {
            QRegularExpressionMatch m = QRegularExpression("(?i)\\bt(\\d+)").match(t.mml);
            if (m.hasMatch()) { *globalTempo = m.captured(1).toInt(); break; }
        }
    }

    return tracks;
}

// ═══════════════════════════════════════════════════════════════════════
//  Build combined MML string for multi-track formats
// ═══════════════════════════════════════════════════════════════════════
static QString buildCombinedMML(const QList<MMLTrackData>& tracks, int tempo) {
    QString combined = QString("T%1 ").arg(tempo);
    for (const MMLTrackData& t : tracks) {
        int ch = t.channel + 1; // CH command is 1-based
        QString mml = t.mml;
        // Strip tempo from individual tracks (set globally)
        mml.replace(QRegularExpression("(?i)\\bt\\d+\\s*"), "");
        mml = mml.trimmed();
        if (mml.isEmpty()) continue;
        combined += QString("CH%1 ").arg(ch);
        if (t.instrument >= 0 && !mml.trimmed().startsWith("@"))
            combined += QString("@%1 ").arg(t.instrument);
        combined += mml + " ";
    }
    return combined.trimmed();
}

// ═══════════════════════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════════════════════
MidiFile* ImportMML::loadFile(QString path, bool* ok) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *ok = false;
        return nullptr;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    QString mmlToProcess;
    int globalTempo = 120;

    // ── Format detection cascade ──
    if (isIniFormat(content)) {
        // 3MLE project file
        QList<MMLTrackData> tracks = parse3MLE(content, &globalTempo);
        mmlToProcess = buildCombinedMML(tracks, globalTempo);
    } else if (isXmlFormat(content)) {
        // XML-based MML (MapleStory2, etc.)
        QList<MMLTrackData> tracks = parseXmlMML(content, &globalTempo);
        mmlToProcess = buildCombinedMML(tracks, globalTempo);
    } else if (isMabiClipboard(content)) {
        // MML@ clipboard format — proper multi-track
        QList<MMLTrackData> tracks = parseMabiClipboard(content, &globalTempo);
        mmlToProcess = buildCombinedMML(tracks, globalTempo);
    } else {
        // Raw MML (single track)
        mmlToProcess = content;
    }

    if (mmlToProcess.trimmed().isEmpty()) {
        *ok = false;
        return nullptr;
    }

    // Create a standard empty MidiFile
    MidiFile* mf = new MidiFile();

    MML::Lexer lexer(mmlToProcess);
    QList<MML::Token> tokens = lexer.tokenize();

    MML::Parser parser(mf);
    parser.parse(tokens);

    // Set the file's total length directly from ticks
    if (parser.maxTick() > 0) {
        mf->setMidiTicks(parser.maxTick());
    }

    mf->setPath(path);
    mf->setSaved(true);

    *ok = true;
    return mf;
}
