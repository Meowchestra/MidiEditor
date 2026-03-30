#include "MmlParser.h"
#include <algorithm>

namespace MML {

std::vector<MmlEvent> MmlParser::Parse(const std::vector<Token>& tokens) {
    _tokens = tokens;
    _pos = 0;
    _channel = 0;
    _tempo = 120;
    _channels.clear();
    _cur = GetOrCreate(0);

    std::vector<MmlEvent> events;

    MmlEvent initialTempo;
    initialTempo.Tick = 0;
    initialTempo.Type = MidiEventType::Tempo;
    initialTempo.Tempo = BpmToMicros(_tempo);
    events.push_back(initialTempo);

    while (Peek().Type != TokenType::Eof) {
        Token t = Advance();
        switch (t.Type) {
            case TokenType::Channel:
                _channel = t.IntValue - 1;
                _cur = GetOrCreate(_channel);
                break;
            case TokenType::Tempo:
                {
                    _tempo = t.IntValue;
                    MmlEvent evt;
                    evt.Tick = _cur->Tick;
                    evt.Type = MidiEventType::Tempo;
                    evt.Tempo = BpmToMicros(_tempo);
                    events.push_back(evt);
                }
                break;
            case TokenType::Octave:
                _cur->Octave = t.IntValue;
                break;
            case TokenType::OctaveUp:
                _cur->Octave = std::min(8, _cur->Octave + 1);
                break;
            case TokenType::OctaveDown:
                _cur->Octave = std::max(0, _cur->Octave - 1);
                break;
            case TokenType::Length:
                _cur->DefaultLen = t.IntValue;
                _cur->DefaultDot = t.Dotted;
                break;
            case TokenType::Volume:
                _cur->Volume = static_cast<int>((t.IntValue / 15.0) * 127);
                break;
            case TokenType::Pan:
                _cur->Pan = t.IntValue;
                events.push_back(MakeCC(0x0A, _cur->Pan));
                break;
            case TokenType::Instrument:
                _cur->Program = t.IntValue;
                {
                    MmlEvent evt;
                    evt.Tick = _cur->Tick;
                    evt.Type = MidiEventType::ProgramChange;
                    evt.Channel = _channel;
                    evt.Param1 = _cur->Program;
                    events.push_back(evt);
                }
                break;
            case TokenType::Rest:
                _cur->Tick += CalcTicks(t.Length, t.Dotted);
                break;
            case TokenType::Note:
                {
                    auto notes = EmitNote(t);
                    events.insert(events.end(), notes.begin(), notes.end());
                }
                break;
            case TokenType::ChordStart:
                {
                    auto chord = EmitChord();
                    events.insert(events.end(), chord.begin(), chord.end());
                }
                break;
            default:
                break;
        }
    }

    std::sort(events.begin(), events.end(), [](const MmlEvent& a, const MmlEvent& b) {
        return a.Tick < b.Tick;
    });
    return events;
}

std::vector<MmlEvent> MmlParser::EmitNote(const Token& t, long long* startTick) {
    int midiNote = NoteToMidi(t.NoteChar, t.Accidental, _cur->Octave);
    long long ticks = CalcTicks(t.Length, t.Dotted);
    long long start = startTick ? *startTick : _cur->Tick;

    long long totalTicks = ticks;
    long long endTick = start + ticks;

    while (Peek().Type == TokenType::Tie) {
        Advance(); // &
        if (Peek().Type == TokenType::Note) {
            Token tied = Advance();
            long long tieTicks = CalcTicks(tied.Length, tied.Dotted);
            totalTicks += tieTicks;
            endTick += tieTicks;
        }
    }

    std::vector<MmlEvent> evts;
    
    MmlEvent onEvt;
    onEvt.Tick = start;
    onEvt.Type = MidiEventType::NoteOn;
    onEvt.Channel = _channel;
    onEvt.Param1 = midiNote;
    onEvt.Param2 = _cur->Volume;
    evts.push_back(onEvt);

    MmlEvent offEvt;
    offEvt.Tick = endTick - 1;
    offEvt.Type = MidiEventType::NoteOff;
    offEvt.Channel = _channel;
    offEvt.Param1 = midiNote;
    offEvt.Param2 = 0;
    evts.push_back(offEvt);

    if (startTick == nullptr) {
        _cur->Tick += totalTicks;
    }

    return evts;
}

std::vector<MmlEvent> MmlParser::EmitChord() {
    std::vector<Token> chordNotes;
    while (Peek().Type != TokenType::ChordEnd && Peek().Type != TokenType::Eof) {
        Token t = Advance();
        if (t.Type == TokenType::Note) chordNotes.push_back(t);
    }
    if (Peek().Type == TokenType::ChordEnd) Advance();

    long long startTick = _cur->Tick;
    long long maxTicks = 0;
    std::vector<MmlEvent> all;

    for (const auto& n : chordNotes) {
        long long ticks = CalcTicks(n.Length, n.Dotted);
        if (ticks > maxTicks) maxTicks = ticks;
        auto notes = EmitNote(n, &startTick);
        all.insert(all.end(), notes.begin(), notes.end());
    }

    _cur->Tick = startTick + maxTicks;
    return all;
}

long long MmlParser::CalcTicks(int len, bool dotted) {
    if (len == 0) {
        len = _cur->DefaultLen;
        dotted = dotted || _cur->DefaultDot;
    }
    if (len == 0) len = 4; // safety
    long long ticks = (PPQ * 4) / len;
    if (dotted) ticks = ticks * 3 / 2;
    return ticks;
}

int MmlParser::NoteToMidi(char note, int accidental, int octave) {
    int semitone[] = { 0, 2, 4, 5, 7, 9, 11 };
    size_t idx = std::string("CDEFGAB").find(note);
    if (idx == std::string::npos) idx = 0;
    return 12 * (octave + 1) + semitone[idx] + accidental;
}

int MmlParser::BpmToMicros(int bpm) {
    if (bpm <= 0) bpm = 120;
    return 60000000 / bpm;
}

MmlEvent MmlParser::MakeCC(int cc, int value) {
    MmlEvent evt;
    evt.Tick = _cur->Tick;
    evt.Type = MidiEventType::Controller;
    evt.Channel = _channel;
    evt.Param1 = cc;
    evt.Param2 = value;
    return evt;
}

ChannelState* MmlParser::GetOrCreate(int ch) {
    if (_channels.find(ch) == _channels.end()) {
        _channels[ch] = ChannelState();
    }
    return &_channels[ch];
}

Token MmlParser::Peek() {
    if (_pos < _tokens.size()) return _tokens[_pos];
    Token eofT;
    eofT.Type = TokenType::Eof;
    return eofT;
}

Token MmlParser::Advance() {
    if (_pos < _tokens.size()) return _tokens[_pos++];
    Token eofT;
    eofT.Type = TokenType::Eof;
    return eofT;
}

} // namespace MML
