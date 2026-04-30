#ifndef MUSICXMLMODELS_H
#define MUSICXMLMODELS_H

#include <QList>
#include <QString>

// Intermediate representation of a parsed MusicXML score.
// Times are normalised to a single PPQ (XmlScore::ticksPerQuarter).

struct XmlNote {
    int  startTick = 0;
    int  duration  = 0;   // in ticks
    int  pitch     = 60;  // MIDI 0..127
    int  velocity  = 80;
};

struct XmlTempoEvent {
    int    tick = 0;
    double bpm  = 120.0;
};

struct XmlTimeSigEvent {
    int tick    = 0;
    int numerator   = 4;
    int denominator = 4;  // 1, 2, 4, 8, 16, 32
};

struct XmlKeySigEvent {
    int  tick    = 0;
    int  fifths  = 0;     // -7..+7
    bool isMinor = false;
};

struct XmlPart {
    QString id;
    QString name;
    int     channel = 0;          // 0..15
    int     program = 0;          // GM 0..127
    QList<XmlNote> notes;
};

struct XmlScore {
    QString title;
    int     ticksPerQuarter = 960;  // fixed; divides cleanly into 1,2,3,4,5,6,8,10,12,15,16,20,24,30,32,40,48,60,…
    QList<XmlTempoEvent>    tempos;
    QList<XmlTimeSigEvent>  timeSigs;
    QList<XmlKeySigEvent>   keySigs;
    QList<XmlPart>          parts;
};

#endif // MUSICXMLMODELS_H
