#ifndef CHORDDETECTOR_H
#define CHORDDETECTOR_H

#include <QList>
#include <QString>

class ChordDetector {
public:
    // Detects chord from a list of MIDI note numbers
    // Returns the chord name (e.g., "C Major", "Am", "Gmaj7")
    // Returns empty string if notes don't form a recognized chord
    static QString detectChord(QList<int> notes);

    // Get note name from MIDI note number
    static QString getNoteName(int note, bool includeOctave = false);

private:
    // Helper to normalize notes to a single octave (0-11)
    static QList<int> normalizeToSingleOctave(QList<int> notes);

    // Get the interval pattern from the root note
    static QList<int> getIntervals(const QList<int>& normalizedNotes);

    // Identify chord type from interval pattern
    static QString identifyChordType(int base, const QList<int>& intervals);
};

#endif // CHORDDETECTOR_H
