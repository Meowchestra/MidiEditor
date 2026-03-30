#pragma once
#include "MmlModels.h"
#include <vector>
#include <map>

namespace MML {

class ChannelState {
public:
    int Octave = 4;
    int DefaultLen = 4;
    bool DefaultDot = false;
    int Volume = 100;
    int Pan = 64;
    int Program = 0;
    long long Tick = 0;
};

class MmlParser {
public:
    static const int PPQ = 480;

    std::vector<MmlEvent> Parse(const std::vector<Token>& tokens);

private:
    std::vector<Token> _tokens;
    size_t _pos;
    int _tempo = 120;
    ChannelState* _cur;
    int _channel = 0;
    std::map<int, ChannelState> _channels;

    std::vector<MmlEvent> EmitNote(const Token& t, long long* startTick = nullptr);
    std::vector<MmlEvent> EmitChord();

    long long CalcTicks(int len, bool dotted);
    static int NoteToMidi(char note, int accidental, int octave);
    static int BpmToMicros(int bpm);
    MmlEvent MakeCC(int cc, int value);
    ChannelState* GetOrCreate(int ch);
    Token Peek();
    Token Advance();
};

} // namespace MML
