#include "XmlScoreToMidi.h"
#include "MusicXmlModels.h"

#include <QList>
#include <algorithm>

namespace {

QByteArray writeVarLen(int value) {
    QByteArray out;
    quint32 buf = value & 0x7F;
    while ((value >>= 7) > 0) {
        buf <<= 8;
        buf |= ((value & 0x7F) | 0x80);
    }
    for (;;) {
        out.append(static_cast<char>(buf & 0xFF));
        if (buf & 0x80) buf >>= 8;
        else break;
    }
    return out;
}

void appendU16BE(QByteArray& d, quint16 v) {
    d.append(static_cast<char>((v >> 8) & 0xFF));
    d.append(static_cast<char>(v & 0xFF));
}

void appendU32BE(QByteArray& d, quint32 v) {
    d.append(static_cast<char>((v >> 24) & 0xFF));
    d.append(static_cast<char>((v >> 16) & 0xFF));
    d.append(static_cast<char>((v >> 8) & 0xFF));
    d.append(static_cast<char>(v & 0xFF));
}

int denomToPower(int denom) {
    switch (denom) {
        case 1:  return 0;
        case 2:  return 1;
        case 4:  return 2;
        case 8:  return 3;
        case 16: return 4;
        case 32: return 5;
        case 64: return 6;
        default: return 2;
    }
}

} // namespace

QByteArray XmlScoreToMidi::encode(const XmlScore& score) {
    if (score.parts.isEmpty()) return {};

    const int ppq = score.ticksPerQuarter;

    // ---- Track 0: global meta events ----
    struct Meta { int tick; QByteArray bytes; };
    QList<Meta> metas;

    // Default tempo at tick 0 if none specified.
    QList<XmlTempoEvent> tempos = score.tempos;
    if (tempos.isEmpty()) {
        XmlTempoEvent t; t.tick = 0; t.bpm = 120.0;
        tempos.append(t);
    } else if (tempos.first().tick != 0) {
        XmlTempoEvent t; t.tick = 0; t.bpm = tempos.first().bpm;
        tempos.prepend(t);
    }
    for (const auto& t : tempos) {
        int usPerBeat = static_cast<int>(60000000.0 / std::max(1.0, t.bpm));
        QByteArray b;
        b.append(static_cast<char>(0xFF));
        b.append(static_cast<char>(0x51));
        b.append(static_cast<char>(0x03));
        b.append(static_cast<char>((usPerBeat >> 16) & 0xFF));
        b.append(static_cast<char>((usPerBeat >> 8) & 0xFF));
        b.append(static_cast<char>(usPerBeat & 0xFF));
        metas.append({t.tick, b});
    }

    // Default time signature at 0 if none.
    QList<XmlTimeSigEvent> tsigs = score.timeSigs;
    if (tsigs.isEmpty()) {
        XmlTimeSigEvent ts; ts.tick = 0; ts.numerator = 4; ts.denominator = 4;
        tsigs.append(ts);
    }
    for (const auto& ts : tsigs) {
        QByteArray b;
        b.append(static_cast<char>(0xFF));
        b.append(static_cast<char>(0x58));
        b.append(static_cast<char>(0x04));
        b.append(static_cast<char>(ts.numerator & 0xFF));
        b.append(static_cast<char>(denomToPower(ts.denominator)));
        b.append(static_cast<char>(24));   // clocks per metronome click
        b.append(static_cast<char>(8));    // 32nds per quarter
        metas.append({ts.tick, b});
    }

    // Key signatures.
    for (const auto& k : score.keySigs) {
        QByteArray b;
        b.append(static_cast<char>(0xFF));
        b.append(static_cast<char>(0x59));
        b.append(static_cast<char>(0x02));
        b.append(static_cast<char>(static_cast<int8_t>(std::clamp(k.fifths, -7, 7))));
        b.append(static_cast<char>(k.isMinor ? 1 : 0));
        metas.append({k.tick, b});
    }

    std::stable_sort(metas.begin(), metas.end(),
        [](const Meta& a, const Meta& b) { return a.tick < b.tick; });

    QByteArray track0;
    {
        if (!score.title.isEmpty()) {
            QByteArray name = score.title.toUtf8();
            track0.append(writeVarLen(0));
            track0.append(static_cast<char>(0xFF));
            track0.append(static_cast<char>(0x03));
            track0.append(writeVarLen(name.size()));
            track0.append(name);
        }
        int last = 0;
        for (const auto& m : metas) {
            track0.append(writeVarLen(m.tick - last));
            track0.append(m.bytes);
            last = m.tick;
        }
        track0.append(writeVarLen(0));
        track0.append(static_cast<char>(0xFF));
        track0.append(static_cast<char>(0x2F));
        track0.append(static_cast<char>(0x00));
    }

    // ---- Track N: per-part notes ----
    QList<QByteArray> partTracks;
    for (const XmlPart& part : score.parts) {
        QByteArray tk;
        const int ch = part.channel & 0x0F;

        if (!part.name.isEmpty()) {
            QByteArray n = part.name.toUtf8();
            tk.append(writeVarLen(0));
            tk.append(static_cast<char>(0xFF));
            tk.append(static_cast<char>(0x03));
            tk.append(writeVarLen(n.size()));
            tk.append(n);
        }

        tk.append(writeVarLen(0));
        tk.append(static_cast<char>(0xC0 | ch));
        tk.append(static_cast<char>(part.program & 0x7F));

        struct Evt { int tick; bool on; int pitch; int vel; };
        QList<Evt> evts;
        evts.reserve(part.notes.size() * 2);
        for (const XmlNote& n : part.notes) {
            evts.append({n.startTick, true,  n.pitch, n.velocity});
            evts.append({n.startTick + n.duration, false, n.pitch, 0});
        }
        std::sort(evts.begin(), evts.end(), [](const Evt& a, const Evt& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            // Note-off before note-on at the same tick.
            return !a.on && b.on;
        });

        int last = 0;
        for (const Evt& e : evts) {
            tk.append(writeVarLen(e.tick - last));
            tk.append(static_cast<char>((e.on ? 0x90 : 0x80) | ch));
            tk.append(static_cast<char>(e.pitch & 0x7F));
            tk.append(static_cast<char>(e.vel & 0x7F));
            last = e.tick;
        }

        tk.append(writeVarLen(0));
        tk.append(static_cast<char>(0xFF));
        tk.append(static_cast<char>(0x2F));
        tk.append(static_cast<char>(0x00));

        partTracks.append(tk);
    }

    // ---- Assemble SMF ----
    QByteArray midi;
    midi.append("MThd", 4);
    appendU32BE(midi, 6);
    appendU16BE(midi, 1);                                           // format 1
    appendU16BE(midi, static_cast<quint16>(1 + partTracks.size())); // ntracks
    appendU16BE(midi, static_cast<quint16>(ppq));

    auto writeChunk = [&](const QByteArray& body) {
        midi.append("MTrk", 4);
        appendU32BE(midi, static_cast<quint32>(body.size()));
        midi.append(body);
    };
    writeChunk(track0);
    for (const QByteArray& t : partTracks) writeChunk(t);

    return midi;
}
