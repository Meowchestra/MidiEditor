#ifndef GP345PARSER_H
#define GP345PARSER_H

#include "GpModels.h"
#include "GpBinaryReader.h"
#include <memory>
#include <vector>
#include <string>

// ============================================================
// GP3/4/5 Binary Parsers — ported from GP3File.cs, GP4File.cs, GP5File.cs
// Uses inheritance chain: GP3 ← GP4 ← GP5
// ============================================================

class Gp3Parser : public GpFile {
public:
    explicit Gp3Parser(const std::vector<uint8_t>& data);
    void readSong() override;

protected:
    GpBinaryReader reader;
    KeySignature key_ = KeySignature::CMajor;
    GpMidiChannel channels_[64];
    std::string notice_[256];
    int noticeCount_ = 0;
    RepeatGroup currentRepeatGroup_;

    void addMeasureHeader(std::unique_ptr<MeasureHeader> header);

    // Reading methods
    std::string readVersion();
    void readVersionTuple();
    virtual void readInfo();
    virtual void readLyrics();
    virtual void readMeasureHeaders(int count);
    virtual MeasureHeader* readMeasureHeader(int number, MeasureHeader* previous);
    virtual void readTracks(int count);
    virtual void readTrack(GpTrack* track);
    virtual void readMeasures();
    virtual void readMeasure(GpMeasure* measure);
    virtual void readBeat(int start, GpMeasure* measure, GpVoice* voice, int voiceIndex);
    virtual void readNote(GpNote* note, GpBeat* beat);
    virtual void readNoteEffects(GpNote* note);
    virtual BendEffect readBend();
    virtual BeatEffect readBeatEffects(GpNoteEffect* effect);
    virtual void readMixTableChange(GpMeasure* measure, BeatEffect& beatEffect);
    virtual GraceEffect readGrace();
    virtual void readOldChord(Chord& chord);
    virtual void readNewChord(Chord& chord);
    virtual std::unique_ptr<Chord> readChord(int stringCount);
    virtual Duration readDuration(uint8_t flags);
    virtual BeatText readText();

    void readMidiChannels();
    GpMidiChannel readChannel();
    int readRepeatAlternative();
    Marker readMarker(MeasureHeader* header);
    GpColor readColor();
    int toChannelShort(uint8_t data);
    int toStrokeValue(int8_t value);
    BeatStroke readBeatStroke();
    GpBeat* getBeat(GpVoice* voice, int start);
};

// ============================================================
// GP4 Parser
// ============================================================

class Gp4Parser : public Gp3Parser {
public:
    explicit Gp4Parser(const std::vector<uint8_t>& data);
    void readSong() override;

protected:
    void readInfo() override;
    void readNoteEffects(GpNote* note) override;
    BeatEffect readBeatEffects(GpNoteEffect* effect) override;
    void readMixTableChange(GpMeasure* measure, BeatEffect& beatEffect) override;
    void readNewChord(Chord& chord) override;
    virtual void readMixTableChangeDurations(MixTableChange* tc);
};

// ============================================================
// GP5 Parser
// ============================================================

class Gp5Parser : public Gp4Parser {
public:
    explicit Gp5Parser(const std::vector<uint8_t>& data);
    void readSong() override;

protected:
    std::vector<DirectionSign> directionsGp5_;

    void readMeasureHeaders(int count) override;
    MeasureHeader* readMeasureHeader(int number, MeasureHeader* previous) override;
    void readInfo() override;
    void readTracks(int count) override;
    void readTrack(GpTrack* track) override;
    void readMeasures() override;
    void readMeasure(GpMeasure* measure) override;
    void readBeat(int start, GpMeasure* measure, GpVoice* voice, int voiceIndex) override;
    void readNote(GpNote* note, GpBeat* beat) override;
    void readNoteEffects(GpNote* note) override;
    BeatEffect readBeatEffects(GpNoteEffect* effect) override;
    void readMixTableChange(GpMeasure* measure, BeatEffect& beatEffect) override;
    void readMixTableChangeDurations(MixTableChange* tc) override;
    GraceEffect readGrace() override;

    void readDirections();
    void readPageSetup();
    void readRSEMasterEffect();
    void readEqualizer(int knobsCount);
    void readTrackRSE(TrackRSE* rse);
    RSEInstrument readRSEInstrument();
    void readRSEInstrumentEffect(RSEInstrument* rse);
    int readRepeatAlternativeGp5();
};

#endif // GP345PARSER_H
