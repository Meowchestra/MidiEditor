#ifndef GP12PARSER_H
#define GP12PARSER_H

#include "GpModels.h"
#include "GpBinaryReader.h"
#include <memory>
#include <vector>
#include <string>

// ============================================================
// GP1/GP2 Binary Parsers — ported from TuxGuitar's
// GP1InputStream.java and GP2InputStream.java
//
// These are independent legacy formats (NOT subsets of GP3).
// GP1 (.gtp): "FICHIER GUITARE PRO v1.xx" (French spelling)
// GP2 (.gtp): "FICHIER GUITAR PRO v2.xx" (English spelling)
// GP2 inherits from GP1, overriding only the differences.
// ============================================================

class Gp1Parser : public GpFile {
public:
    explicit Gp1Parser(const std::vector<uint8_t>& data);
    void readSong() override;

    // Version code: GP1: 0=v1, 1=v1.01, 2=v1.02, 3=v1.03, 4=v1.04
    //               GP2: 0=v2.20, 1=v2.21
    int versionCode() const { return versionCode_; }

protected:
    GpBinaryReader reader;
    int versionCode_ = 0;
    int trackCount_ = 1;
    KeySignature keySignature_ = KeySignature::CMajor;

    // Channel mapping: [trackIndex][0=channelId, 1=gmChannel1, 2=gmChannel2]
    static constexpr int TRACK_CHANNELS[8][3] = {
        {1,0,1}, {2,2,3}, {3,4,5}, {4,6,7},
        {5,8,10}, {6,11,12}, {7,13,14}, {8,9,9}
    };

    virtual void readVersion();
    void readInfo();
    virtual void readTrack(GpTrack* track, int trackIndex);
    void readTrackMeasures(long* lastReadStarts);
    virtual long readBeat(GpTrack* track, GpMeasure* measure, long start, long lastReadStart);
    Duration readDuration();
    void readTimeSignature(TimeSignature& ts);
    void readBeatEffects(BeatEffect& beatEffect, GpNoteEffect& noteEffect);
    void readNoteEffects(GpNoteEffect& effect);
    BendEffect readBend();
    virtual void readChord(int stringCount, GpBeat* beat);
    std::string readText();
    int parseRepeatAlternative(int measureNumber, int value);
    MeasureClef getClef(GpTrack* track);
    GpBeat* findBeat(GpTrack* track, GpMeasure* measure, long start);
    GpBeat* findBeatInMeasure(GpMeasure* measure, long start);
};

class Gp2Parser : public Gp1Parser {
public:
    explicit Gp2Parser(const std::vector<uint8_t>& data);
    void readSong() override;

protected:
    void readVersion() override;
    void readTrack(GpTrack* track, int trackIndex) override;
    long readBeat(GpTrack* track, GpMeasure* measure, long start, long lastReadStart) override;
    void readChord(int stringCount, GpBeat* beat) override;
    void readMixChange(Tempo& tempo);
    void readGraceNote();
};

#endif // GP12PARSER_H
