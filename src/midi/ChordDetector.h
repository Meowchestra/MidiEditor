#ifndef CHORDDETECTOR_H
#define CHORDDETECTOR_H

#include <QList>
#include <QString>

class ChordDetector {
public:
    // Detects chord from a list of MIDI note numbers
    // Returns the chord name (e.g., "C Major", "Am", "Gmaj7")
    // Returns empty string if notes don't form a recognized chord
    static QString detectChord(const QList<int> &notes);
    // Get note name from MIDI note number
    static QString getNoteName(int note, bool includeOctave = false);

private:
    // Identify chord type from interval pattern
    static QString identifyChordType(int base, const QList<int>& intervals);
};

#endif // CHORDDETECTOR_H
