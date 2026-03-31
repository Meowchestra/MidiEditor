#include "ChordDetector.h"
#include <QSet>
#include <QDebug>
#include <algorithm>

QString ChordDetector::detectChord(QList<int> notes) {
    if (notes.isEmpty()) return QString();

    // Remove duplicates and sort
    std::set<int> uniqueNotes;
    std::set<int> uniqueNotesCopy;
    for (int note : notes) {
      uniqueNotes.insert(note % 12);
      uniqueNotesCopy.insert(note % 12);
    }

    if (uniqueNotes.size() == 1) return QString(); // Unison is not a chord

    for (int base: uniqueNotesCopy) {
        uniqueNotes.erase(base);
        QList<int> intervals(uniqueNotes.begin(), uniqueNotes.end());
        if(intervals.size() == 0) return QString();

        QString chordType = identifyChordType(base, intervals);
        if (chordType != "INVALID") {
            QString rootName = getNoteName(base);
            return rootName + chordType;
        }
        uniqueNotes.insert(base + 12);
    }

    return QString();
}

QString ChordDetector::getNoteName(int note, bool includeOctave) {
    if (note < 0) return QString("-");
    static const QString noteNames[] = {
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B"
    };
    QString name = noteNames[note % 12];
    if (includeOctave) {
        // MIDI note 60 is C4 (Middle C)
        name += QString::number(note / 12 - 1);
    }
    return name;
}

QString ChordDetector::identifyChordType(int base, const QList<int>& rest) {
    QList<int> intervals;
    for (int i: rest) {
        intervals.append(i - base);
    }

    // 3-note chords (triads)
    if (rest.size() == 1) {
        switch (intervals[0]) {
            case 3: return "m";
            case 4: return "";
            case 7: return " (5th)";
        }
    }

    if (intervals.size() == 2) {
        if (intervals[0] == 3 && intervals[1] == 7) return "m"; // Minor
        if (intervals[0] == 3 && intervals[1] == 6) return "dim"; // Diminished
        if (intervals[0] == 4 && intervals[1] == 7) return ""; // Major
        if (intervals[0] == 4 && intervals[1] == 8) return "aug"; // Augmented
        if (intervals[0] == 2 && intervals[1] == 7) return "sus2"; // Sus2
        if (intervals[0] == 5 && intervals[1] == 7) return "sus4"; // Sus4
    }

    // 4-note chords (seventh chords)
    if (intervals.size() == 3) {
        if (intervals[0] == 4 && intervals[1] == 7 && intervals[2] == 11) return "maj7"; // Major 7th
        if (intervals[0] == 3 && intervals[1] == 7 && intervals[2] == 10) return "m7"; // Minor 7th
        if (intervals[0] == 4 && intervals[1] == 7 && intervals[2] == 10) return "7"; // Dominant 7th
        if (intervals[0] == 3 && intervals[1] == 6 && intervals[2] == 10) return "m7b5"; // Half-diminished
        if (intervals[0] == 3 && intervals[1] == 6 && intervals[2] == 9) return "dim7"; // Diminished 7th
        if (intervals[0] == 3 && intervals[1] == 7 && intervals[2] == 11) return "mmaj7"; // Minor major 7th
        if (intervals[0] == 4 && intervals[1] == 8 && intervals[2] == 10) return "aug7"; // Augmented 7th
        if (intervals[0] == 4 && intervals[1] == 8 && intervals[2] == 11) return "augmaj7"; // Augmented major 7th
    }

    // Extended chords (9th, 11th, 13th)
    if (intervals.size() == 4) {
        if (intervals[0] == 2 && intervals[1] == 4 && intervals[2] == 7 && intervals[3] == 10) return "9"; // Dominant 9th
        if (intervals[0] == 2 && intervals[1] == 4 && intervals[2] == 7 && intervals[3] == 11) return "maj9"; // Major 9th
        if (intervals[0] == 2 && intervals[1] == 3 && intervals[2] == 7 && intervals[3] == 10) return "m9"; // Minor 9th
        if (intervals[0] == 1 && intervals[1] == 4 && intervals[2] == 7 && intervals[3] == 10) return "7b9"; // Dominant 7th flat 9
        if (intervals[0] == 3 && intervals[1] == 4 && intervals[2] == 7 && intervals[3] == 10) return "7#9"; // Dominant 7th sharp 9
    }

    return "INVALID";
}
