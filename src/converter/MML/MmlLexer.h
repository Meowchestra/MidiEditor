#pragma once
#include "MmlModels.h"
#include <string>
#include <vector>

namespace MML {

class MmlLexer {
public:
    explicit MmlLexer(const std::string& source);
    std::vector<Token> Tokenize();

private:
    std::string _src;
    size_t _pos;

    bool ConsumeDot();
    void SkipWhitespace();
    char Peek(int offset);

    Token ReadNote();
    Token ReadRest();
    Token ReadOctave();
    Token ReadLength();
    Token ReadChannel();
    Token ReadIntCommand(TokenType type, int min, int max, char skip = '\0');
    Token Simple(TokenType type);

    int ReadAccidental();
    int ReadOptionalInt();
    int ReadRequiredInt(int min, int max);
    int ReadInt();

    static std::string StripComments(const std::string& src);
};

} // namespace MML
