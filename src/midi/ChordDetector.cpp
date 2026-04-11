#include "ChordDetector.h"
#include <QSet>
#include <QDebug>
#include <algorithm>
#include <set>

QString ChordDetector::detectChord(const QList<int>& notes) {
    if (notes.isEmpty()) return QString();

    std::set<int> uniqueNotes;
    std::set<int> uniqueNotesCopy;
    int lowestNote = 128; // Max MIDI note is 127

    // Find the true lowest note and build our pitch classes
    for (int note : notes) {
        if (note < 0 || note > 127) continue; // Guard against bad MIDI data
        if (note < lowestNote) lowestNote = note;
        
        uniqueNotes.insert(note % 12);
        uniqueNotesCopy.insert(note % 12);
    }

    if (uniqueNotes.size() <= 1) return QString(); // Unison/Empty is not a chord

    int bassPitchClass = lowestNote % 12;
    QString detectedChord = "";
    int detectedRoot = -1;

    for (int base: uniqueNotesCopy) {
        uniqueNotes.erase(base);
        QList<int> intervals;
        for (int note : uniqueNotes) {
            intervals.append(note);
        }
        if(intervals.isEmpty()) return QString();

        QString chordType = identifyChordType(base, intervals);
        if (chordType != "INVALID") {
            detectedChord = getNoteName(base, false) + chordType;
            detectedRoot = base;
            
            // If the root is the actual bass note, this is the perfect match. 
            if (base == bassPitchClass) {
                break;
            }
        }
        uniqueNotes.insert(base + 12);
    }

    if (detectedChord.isEmpty()) return QString();

    // If the lowest note played is not the root of the chord, append a slash bass
    if (detectedRoot != bassPitchClass) {
        detectedChord += "/" + getNoteName(bassPitchClass, false);
    }

    return detectedChord;
}

QString ChordDetector::getNoteName(int note, bool includeOctave) {
    if (note < 0) return QString("-");
    static const QString noteNames[] = {
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B"
    };
    QString name = noteNames[note % 12];
    if (includeOctave) {
        name += QString::number(note / 12 - 1);
    }
    return name;
}

QString ChordDetector::identifyChordType(int base, const QList<int>& rest) {
    QList<int> intervals;
    for (int i : rest) {
        intervals.append(i - base);
    }
    std::sort(intervals.begin(), intervals.end());

    // 2-note combinations (Shells / Power chords)
    if (intervals.size() == 1) {
        switch (intervals[0]) {
            case 2: return "sus2";
            case 3: return "m";
            case 4: return "";
            case 5: return "sus4";
            case 6: return "dim"; // or b5
            case 7: return "5";
            case 8: return "aug";
            case 9: return "6";
            case 10: return "7";
            case 11: return "maj7";
        }
    }

    // 3-note chords (Triads and Shell extensions)
    if (intervals.size() == 2) {
        if (intervals[0] == 3 && intervals[1] == 7) return "m";
        if (intervals[0] == 4 && intervals[1] == 7) return "";
        if (intervals[0] == 3 && intervals[1] == 6) return "dim";
        if (intervals[0] == 4 && intervals[1] == 8) return "aug";
        if (intervals[0] == 2 && intervals[1] == 7) return "sus2";
        if (intervals[0] == 5 && intervals[1] == 7) return "sus4";
        
        if (intervals[0] == 4 && intervals[1] == 10) return "7";
        if (intervals[0] == 4 && intervals[1] == 11) return "maj7";
        if (intervals[0] == 3 && intervals[1] == 10) return "m7";
        if (intervals[0] == 3 && intervals[1] == 11) return "m(maj7)";
        if (intervals[0] == 4 && intervals[1] == 9) return "6";
        if (intervals[0] == 3 && intervals[1] == 9) return "m6";
        if (intervals[0] == 2 && intervals[1] == 4) return "add9";
        if (intervals[0] == 2 && intervals[1] == 3) return "m(add9)";
        
        if (intervals[0] == 4 && intervals[1] == 6) return "(b5)";
        if (intervals[0] == 3 && intervals[1] == 8) return "m(#5)";
    }

    // 4-note chords (7ths, 6ths, and extended shells)
    if (intervals.size() == 3) {
        if (intervals[0] == 4 && intervals[1] == 7 && intervals[2] == 10) return "7";
        if (intervals[0] == 4 && intervals[1] == 7 && intervals[2] == 11) return "maj7";
        if (intervals[0] == 3 && intervals[1] == 7 && intervals[2] == 10) return "m7";
        if (intervals[0] == 3 && intervals[1] == 7 && intervals[2] == 11) return "m(maj7)";
        if (intervals[0] == 3 && intervals[1] == 6 && intervals[2] == 10) return "m7(b5)";
        if (intervals[0] == 3 && intervals[1] == 6 && intervals[2] == 9) return "dim7";
        if (intervals[0] == 4 && intervals[1] == 8 && intervals[2] == 10) return "7(#5)";
        if (intervals[0] == 4 && intervals[1] == 8 && intervals[2] == 11) return "maj7(#5)";
        
        if (intervals[0] == 4 && intervals[1] == 7 && intervals[2] == 9) return "6";
        if (intervals[0] == 3 && intervals[1] == 7 && intervals[2] == 9) return "m6";
        if (intervals[0] == 2 && intervals[1] == 4 && intervals[2] == 9) return "6/9";
        
        if (intervals[0] == 2 && intervals[1] == 4 && intervals[2] == 10) return "9";
        if (intervals[0] == 2 && intervals[1] == 4 && intervals[2] == 11) return "maj9";
        if (intervals[0] == 2 && intervals[1] == 3 && intervals[2] == 10) return "m9";
        if (intervals[0] == 1 && intervals[1] == 4 && intervals[2] == 10) return "7(b9)";
        if (intervals[0] == 3 && intervals[1] == 4 && intervals[2] == 10) return "7(#9)";
        
        if (intervals[0] == 4 && intervals[1] == 6 && intervals[2] == 10) return "7(b5)"; // Also covers 7(#11)no5
        if (intervals[0] == 4 && intervals[1] == 9 && intervals[2] == 10) return "13";
        
        if (intervals[0] == 5 && intervals[1] == 7 && intervals[2] == 10) return "7sus4";
        if (intervals[0] == 2 && intervals[1] == 7 && intervals[2] == 10) return "7sus2";
        
        if (intervals[0] == 2 && intervals[1] == 4 && intervals[2] == 7) return "add9";
        if (intervals[0] == 2 && intervals[1] == 3 && intervals[2] == 7) return "m(add9)";
    }

    // 5-note chords
    if (intervals.size() == 4) {
        if (intervals[0] == 2 && intervals[1] == 4 && intervals[2] == 7 && intervals[3] == 10) return "9";
        if (intervals[0] == 2 && intervals[1] == 4 && intervals[2] == 7 && intervals[3] == 11) return "maj9";
        if (intervals[0] == 2 && intervals[1] == 3 && intervals[2] == 7 && intervals[3] == 10) return "m9";
        if (intervals[0] == 1 && intervals[1] == 4 && intervals[2] == 7 && intervals[3] == 10) return "7(b9)";
        if (intervals[0] == 3 && intervals[1] == 4 && intervals[2] == 7 && intervals[3] == 10) return "7(#9)";
        if (intervals[0] == 2 && intervals[1] == 4 && intervals[2] == 7 && intervals[3] == 9) return "6/9";
        
        if (intervals[0] == 4 && intervals[1] == 7 && intervals[2] == 9 && intervals[3] == 10) return "13";
        if (intervals[0] == 4 && intervals[1] == 7 && intervals[2] == 9 && intervals[3] == 11) return "maj13";
        if (intervals[0] == 3 && intervals[1] == 7 && intervals[2] == 9 && intervals[3] == 10) return "m13";
        if (intervals[0] == 4 && intervals[1] == 6 && intervals[2] == 7 && intervals[3] == 10) return "7(#11)";
        if (intervals[0] == 2 && intervals[1] == 5 && intervals[2] == 7 && intervals[3] == 10) return "9sus4";
    }

    // 6-note chords
    if (intervals.size() == 5) {
        if (intervals[0] == 2 && intervals[1] == 4 && intervals[2] == 5 && intervals[3] == 7 && intervals[4] == 10) return "11";
        if (intervals[0] == 2 && intervals[1] == 3 && intervals[2] == 5 && intervals[3] == 7 && intervals[4] == 10) return "m11";
        if (intervals[0] == 2 && intervals[1] == 4 && intervals[2] == 7 && intervals[3] == 9 && intervals[4] == 10) return "13";
        if (intervals[0] == 2 && intervals[1] == 4 && intervals[2] == 7 && intervals[3] == 9 && intervals[4] == 11) return "maj13";
        if (intervals[0] == 2 && intervals[1] == 3 && intervals[2] == 7 && intervals[3] == 9 && intervals[4] == 10) return "m13";
    }

    // 7-note chords
    if (intervals.size() == 6) {
        if (intervals[0] == 2 && intervals[1] == 4 && intervals[2] == 5 && intervals[3] == 7 && intervals[4] == 9 && intervals[5] == 10) return "13";
    }

    return "INVALID";
}