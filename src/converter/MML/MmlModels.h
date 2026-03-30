#pragma once
#include <string>
#include <vector>

namespace MML {

enum class TokenType {
    Note,           // C D E F G A B
    Rest,           // R
    Octave,         // O4
    OctaveUp,       // >
    OctaveDown,     // <
    Length,         // L8
    Tempo,          // T120
    Volume,         // V12
    Channel,        // CH1
    ChordStart,     // [
    ChordEnd,       // ]
    Dot,            // .  (Länge × 1,5)
    Tie,            // &
    Pan,            // P
    Instrument,     // @3  (GM-Program)
    Eof
};

struct Token {
    TokenType Type = TokenType::Eof;
    std::string Raw;
    int IntValue = 0;
    char NoteChar = '\0';
    int Accidental = 0;
    int Length = 0;
    bool Dotted = false;
};

enum class MidiEventType { NoteOn, NoteOff, ProgramChange, Controller, Tempo };

struct MmlEvent {
    long long Tick = 0;
    MidiEventType Type = MidiEventType::NoteOn;
    int Channel = 0;
    int Param1 = 0;
    int Param2 = 0;
    int Tempo = 0;
};

struct ThreeMleTrack {
    std::string Name;
    int Channel = 1;
    int Instrument = 0;
    std::string MML;
};

struct ThreeMleFile {
    std::string Title;
    int Tempo = 120;
    std::vector<ThreeMleTrack> Tracks;
};

enum class MmlDialect {
    Auto,
    Generic,
    Mabinogi,
    MapleStory2
};

} // namespace MML
