#ifndef GP678PARSER_H
#define GP678PARSER_H

#include "GpModels.h"
#include "Gp345Parser.h"
#include <string>
#include <vector>
#include <memory>

// ============================================================
// XML Node — ported from GP6File.cs Node class
// ============================================================

struct XmlNode {
    std::string name;
    std::vector<std::unique_ptr<XmlNode>> subnodes;
    std::vector<std::string> propertyNames;
    std::vector<std::string> propertyValues;
    std::string content;

    XmlNode() = default;
    XmlNode(const std::string& name_, const std::string& content_ = "")
        : name(name_), content(content_) {}

    XmlNode* getSubnodeByName(const std::string& name, bool directOnly = false) const;
    XmlNode* getSubnodeByProperty(const std::string& propName, const std::string& propValue) const;
};

// ============================================================
// BitStream — ported from GP6File.cs BitStream class
// ============================================================

class GpBitStream {
public:
    explicit GpBitStream(const std::vector<uint8_t>& data);

    bool getBit();
    std::vector<bool> getBits(int amount);
    uint8_t getByte();
    int getBitsLE(int amount);
    int getBitsBE(int amount);
    void skipBits(int bits);
    void skipBytes(int bytes);
    bool isFinished() const { return finished_; }

private:
    std::vector<uint8_t> data_;
    int pointer_ = 0;
    int subpointer_ = 0;
    bool finished_ = false;

    void increaseSubpointer();
};

// ============================================================
// GP6 Parser — ported from GP6File.cs
// ============================================================

class Gp6Parser : public GpFile {
public:
    explicit Gp6Parser(const std::vector<uint8_t>& data);
    explicit Gp6Parser(const std::string& xml); // For GP7 which passes XML directly
    void readSong() override;

protected:
    std::vector<uint8_t> rawData_;
    std::string xmlContent_;
    bool isXmlDirect_ = false;

    // GP6 decompression
    std::string decompressGPX(const std::vector<uint8_t>& data);
    static std::unique_ptr<XmlNode> parseGP6(const std::string& xml, int start = 0);

    // Transfer methods — convert XML nodes to GP5-compatible data
    void gp6NodeToGP5File(XmlNode* root);
    static std::vector<std::unique_ptr<MeasureHeader>> transferMeasureHeaders(
        XmlNode* nMasterBars, Gp5Parser* song);
    static std::vector<std::unique_ptr<GpTrack>> transferTracks(
        XmlNode* nTracks, Gp5Parser* song);
    static std::vector<std::string> transferDirections(XmlNode* nDirections);
    static std::vector<std::string> transferFromDirections(XmlNode* nDirections);
    static std::vector<Lyrics> transferLyrics(XmlNode* nTracks);
    static void transferBars(XmlNode* nBars, Gp5Parser* song,
                              const std::vector<std::unique_ptr<MeasureHeader>>& headers);
    static void transferVoice(XmlNode* nVoice, GpVoice* voice,
                               XmlNode* nBeats, XmlNode* nNotes, XmlNode* nRhythms,
                               GpMeasure* measure, const std::vector<GP6Rhythm>& rhythms,
                               const std::vector<GP6Chord>& chords,
                               const std::vector<GP6Tempo>& tempos,
                               int masterBarIdx);
    static void transferBeat(XmlNode* nBeat, GpBeat* beat,
                              XmlNode* nNotes, XmlNode* nRhythms,
                              GpMeasure* measure, const std::vector<GP6Rhythm>& rhythms,
                              const std::vector<GP6Chord>& chords,
                              const std::vector<GP6Tempo>& tempos,
                              int masterBarIdx, int beatPos);
    static void transferNote(XmlNode* nNote, GpNote* note, GpBeat* beat,
                              const std::string& tremolo, GraceEffect* graceEffect,
                              int velocity);

    static std::vector<GP6Rhythm> readRhythms(XmlNode* nRhythms);
    static std::vector<GP6Chord> readChords(XmlNode* nTracks);
    static int getGP6DrumValue(int element, int variation);
};

// ============================================================
// GP7 Parser — thin wrapper, ported from GP7File.cs
// ============================================================

class Gp7Parser : public Gp6Parser {
public:
    explicit Gp7Parser(const std::string& xml);
    void readSong() override;
};

#endif // GP678PARSER_H
