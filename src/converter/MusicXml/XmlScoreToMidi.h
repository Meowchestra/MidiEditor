#ifndef XMLSCORETOMIDI_H
#define XMLSCORETOMIDI_H

#include <QByteArray>

struct XmlScore;

// Encodes an XmlScore intermediate representation as Standard MIDI File
// format-1 bytes. Returns empty QByteArray on failure (empty score, etc.).
//
// Layout:
//   Track 0 — meta only (title, tempo map, time/key signatures)
//   Track N — one per XmlPart, with track-name + program change + notes
namespace XmlScoreToMidi {
    QByteArray encode(const XmlScore& score);
}

#endif // XMLSCORETOMIDI_H
