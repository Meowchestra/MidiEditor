#include "MmlLexer.h"
#include <cctype>
#include <algorithm>

namespace MML {

MmlLexer::MmlLexer(const std::string& source) {
    std::string upperSource = source;
    std::transform(upperSource.begin(), upperSource.end(), upperSource.begin(), ::toupper);
    _src = StripComments(upperSource);
    _pos = 0;
}

std::vector<Token> MmlLexer::Tokenize() {
    std::vector<Token> tokens;
    while (_pos < _src.length()) {
        SkipWhitespace();
        if (_pos >= _src.length()) break;

        char c = _src[_pos];
        bool found = false;
        Token t;

        if (c == 'C' && Peek(1) == 'H') { t = ReadChannel(); found = true; }
        else if (std::string("CDEFGAB").find(c) != std::string::npos) { t = ReadNote(); found = true; }
        else if (c == 'R') { t = ReadRest(); found = true; }
        else if (c == 'O') { t = ReadOctave(); found = true; }
        else if (c == '>') { t = Simple(TokenType::OctaveUp); found = true; }
        else if (c == '<') { t = Simple(TokenType::OctaveDown); found = true; }
        else if (c == 'L') { t = ReadLength(); found = true; }
        else if (c == 'T') { t = ReadIntCommand(TokenType::Tempo, 20, 600); found = true; }
        else if (c == 'V') { t = ReadIntCommand(TokenType::Volume, 0, 15); found = true; }
        else if (c == 'P') { t = ReadIntCommand(TokenType::Pan, 0, 127); found = true; }
        else if (c == '@') { t = ReadIntCommand(TokenType::Instrument, 0, 127, '@'); found = true; }
        else if (c == '[') { t = Simple(TokenType::ChordStart); found = true; }
        else if (c == ']') { t = Simple(TokenType::ChordEnd); found = true; }
        else if (c == '.') { t = Simple(TokenType::Dot); found = true; }
        else if (c == '&') { t = Simple(TokenType::Tie); found = true; }
        else { _pos++; } // skip unkown chars

        if (found) tokens.push_back(t);
    }
    Token eofT;
    eofT.Type = TokenType::Eof;
    tokens.push_back(eofT);
    return tokens;
}

Token MmlLexer::ReadNote() {
    Token t;
    t.Type = TokenType::Note;
    t.NoteChar = _src[_pos++];
    t.Accidental = ReadAccidental();
    t.Length = ReadOptionalInt();
    t.Dotted = ConsumeDot();
    return t;
}

Token MmlLexer::ReadRest() {
    _pos++; // 'R'
    Token t;
    t.Type = TokenType::Rest;
    t.Length = ReadOptionalInt();
    t.Dotted = ConsumeDot();
    return t;
}

Token MmlLexer::ReadOctave() {
    _pos++; // 'O'
    int val = ReadRequiredInt(0, 8);
    Token t;
    t.Type = TokenType::Octave;
    t.IntValue = val;
    return t;
}

Token MmlLexer::ReadLength() {
    _pos++; // 'L'
    int val = ReadRequiredInt(1, 64);
    bool dotted = ConsumeDot();
    Token t;
    t.Type = TokenType::Length;
    t.IntValue = val;
    t.Dotted = dotted;
    return t;
}

Token MmlLexer::ReadChannel() {
    _pos += 2; // 'CH'
    int val = ReadRequiredInt(1, 16);
    Token t;
    t.Type = TokenType::Channel;
    t.IntValue = val;
    return t;
}

Token MmlLexer::ReadIntCommand(TokenType type, int min, int max, char skip) {
    _pos++; // Command-Character
    if (skip != '\0' && _pos < _src.length() && _src[_pos] == skip) _pos++;
    int val = ReadRequiredInt(min, max);
    Token t;
    t.Type = type;
    t.IntValue = val;
    return t;
}

Token MmlLexer::Simple(TokenType type) {
    _pos++;
    Token t;
    t.Type = type;
    return t;
}

int MmlLexer::ReadAccidental() {
    if (_pos >= _src.length()) return 0;
    if (_src[_pos] == '#' || _src[_pos] == '+') { _pos++; return 1; }
    if (_src[_pos] == '-' || _src[_pos] == 'B') { _pos++; return -1; }
    return 0;
}

int MmlLexer::ReadOptionalInt() {
    if (_pos >= _src.length() || !std::isdigit(_src[_pos])) return 0;
    return ReadInt();
}

int MmlLexer::ReadRequiredInt(int min, int max) {
    int v = ReadInt();
    if (v < min) v = min;
    if (v > max) v = max;
    return v;
}

int MmlLexer::ReadInt() {
    size_t start = _pos;
    while (_pos < _src.length() && std::isdigit(_src[_pos])) _pos++;
    if (_pos == start) return 0;
    return std::stoi(_src.substr(start, _pos - start));
}

bool MmlLexer::ConsumeDot() {
    if (_pos < _src.length() && _src[_pos] == '.') {
        _pos++;
        return true;
    }
    return false;
}

void MmlLexer::SkipWhitespace() {
    while (_pos < _src.length() && std::isspace(_src[_pos])) _pos++;
}

char MmlLexer::Peek(int offset) {
    if (_pos + offset < _src.length()) return _src[_pos + offset];
    return '\0';
}

std::string MmlLexer::StripComments(const std::string& src) {
    std::string sb;
    size_t i = 0;
    while (i < src.length()) {
        if (i + 1 < src.length() && src[i] == '/' && src[i + 1] == '/') {
            while (i < src.length() && src[i] != '\n') i++;
        } else if (i + 1 < src.length() && src[i] == '/' && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < src.length() && !(src[i] == '*' && src[i + 1] == '/')) i++;
            i += 2;
        } else {
            sb += src[i++];
        }
    }
    return sb;
}

} // namespace MML
