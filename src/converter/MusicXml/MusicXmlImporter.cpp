#include "MusicXmlImporter.h"
#include "MusicXmlModels.h"
#include "XmlScoreToMidi.h"

#include "../../midi/MidiFile.h"

#include <QByteArray>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTemporaryFile>
#include <QXmlStreamReader>
#include <algorithm>
#include <vector>

#include "../GuitarPro/GpUnzip.h"

// =====================================================================
// Helpers
// =====================================================================

namespace {

// Convert MusicXML <step><alter><octave> to MIDI note number.
// MIDI: C4 = 60. step ∈ {C,D,E,F,G,A,B}, alter = semitone offset.
int pitchToMidi(QChar step, int alter, int octave) {
    static const QHash<QChar, int> kStepSemis = {
        {QChar('C'), 0}, {QChar('D'), 2}, {QChar('E'), 4},
        {QChar('F'), 5}, {QChar('G'), 7}, {QChar('A'), 9}, {QChar('B'), 11}
    };
    int base = kStepSemis.value(step.toUpper(), 0);
    int midi = (octave + 1) * 12 + base + alter;
    return std::clamp(midi, 0, 127);
}

// =====================================================================
// .mxl (compressed MusicXML) extraction
// =====================================================================

// Try to find the rootfile inside an .mxl ZIP. If META-INF/container.xml
// names one, use it; otherwise return the first non-META-INF .xml/.musicxml
// entry encountered. Returns the uncompressed XML bytes, or empty on failure.
QByteArray extractMxl(const QString& path) {
    GpUnzip zip(path.toStdString());

    // Try the spec-mandated rootfile first.
    if (zip.hasEntry("META-INF/container.xml")) {
        std::vector<uint8_t> containerBytes = zip.extract("META-INF/container.xml");
        QByteArray container(reinterpret_cast<const char*>(containerBytes.data()),
                             static_cast<int>(containerBytes.size()));
        QXmlStreamReader xr(container);
        while (!xr.atEnd() && !xr.hasError()) {
            xr.readNext();
            if (xr.isStartElement() && xr.name() == QStringLiteral("rootfile")) {
                QString full = xr.attributes().value("full-path").toString();
                if (!full.isEmpty() && zip.hasEntry(full.toStdString())) {
                    std::vector<uint8_t> body = zip.extract(full.toStdString());
                    return QByteArray(reinterpret_cast<const char*>(body.data()),
                                      static_cast<int>(body.size()));
                }
            }
        }
    }

    // Fallback — common candidate names.
    static const std::vector<std::string> candidates = {
        "score.xml", "score.musicxml", "musicxml.xml"
    };
    for (const auto& name : candidates) {
        if (zip.hasEntry(name)) {
            std::vector<uint8_t> body = zip.extract(name);
            return QByteArray(reinterpret_cast<const char*>(body.data()),
                              static_cast<int>(body.size()));
        }
    }
    return {};
}

// =====================================================================
// Parser — walks the MusicXML stream and fills XmlScore.
// =====================================================================

class Parser {
public:
    bool parse(const QByteArray& xml, XmlScore& out) {
        _score = &out;
        QXmlStreamReader xr(xml);
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isStartElement()) {
                if (xr.name() == QStringLiteral("part-list")) {
                    parsePartList(xr);
                } else if (xr.name() == QStringLiteral("part")) {
                    parsePart(xr);
                } else if (xr.name() == QStringLiteral("work-title")
                        || xr.name() == QStringLiteral("movement-title")) {
                    if (_score->title.isEmpty())
                        _score->title = xr.readElementText();
                }
            }
        }
        return !xr.hasError();
    }

private:
    XmlScore* _score = nullptr;

    // ---- part-list — collect part metadata (channel, program, name) ------
    void parsePartList(QXmlStreamReader& xr) {
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("part-list"))
                return;
            if (xr.isStartElement() && xr.name() == QStringLiteral("score-part")) {
                XmlPart p;
                p.id = xr.attributes().value("id").toString();
                // Default channel: sequential by part order.
                p.channel = std::min(15, static_cast<int>(_score->parts.size()));
                parseScorePart(xr, p);
                _score->parts.append(p);
            }
        }
    }

    void parseScorePart(QXmlStreamReader& xr, XmlPart& p) {
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("score-part"))
                return;
            if (xr.isStartElement()) {
                if (xr.name() == QStringLiteral("part-name")) {
                    p.name = xr.readElementText();
                } else if (xr.name() == QStringLiteral("midi-instrument")) {
                    parseMidiInstrument(xr, p);
                }
            }
        }
    }

    void parseMidiInstrument(QXmlStreamReader& xr, XmlPart& p) {
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("midi-instrument"))
                return;
            if (xr.isStartElement()) {
                if (xr.name() == QStringLiteral("midi-channel")) {
                    int ch = xr.readElementText().toInt();
                    if (ch >= 1 && ch <= 16) p.channel = ch - 1;
                } else if (xr.name() == QStringLiteral("midi-program")) {
                    int pr = xr.readElementText().toInt();
                    if (pr >= 1 && pr <= 128) p.program = pr - 1;
                }
            }
        }
    }

    // ---- part — the actual notes ---------------------------------------
    void parsePart(QXmlStreamReader& xr) {
        QString partId = xr.attributes().value("id").toString();
        XmlPart* part = nullptr;
        for (XmlPart& p : _score->parts) {
            if (p.id == partId) { part = &p; break; }
        }
        if (!part) {
            // Part body without matching <score-part> — synthesise.
            XmlPart synth;
            synth.id = partId;
            synth.channel = std::min(15, static_cast<int>(_score->parts.size()));
            _score->parts.append(synth);
            part = &_score->parts.last();
        }

        // Per-part state.
        int divisions   = 480;            // ticks per quarter (XML local)
        int currentTick = 0;              // global PPQ
        int prevNoteStartTick = 0;        // for <chord/>

        const int globalPPQ = _score->ticksPerQuarter;
        auto toGlobal = [&](int xmlDur) {
            // Scale local divisions to global PPQ.
            return divisions > 0 ? (xmlDur * globalPPQ) / divisions : xmlDur;
        };

        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("part"))
                return;
            if (!xr.isStartElement()) continue;

            const auto name = xr.name();
            if (name == QStringLiteral("attributes")) {
                parseAttributes(xr, divisions, currentTick);
            } else if (name == QStringLiteral("direction")) {
                parseDirection(xr, currentTick);
            } else if (name == QStringLiteral("sound")) {
                // <sound> can appear directly in measure too.
                handleSoundAttrs(xr.attributes(), currentTick);
            } else if (name == QStringLiteral("note")) {
                int dur = 0;
                bool isRest = false;
                bool isChord = false;
                int  pitch = -1;
                parseNote(xr, dur, isRest, isChord, pitch);

                int globalDur = toGlobal(dur);
                int noteStart = currentTick;
                if (isChord) {
                    // Chord — share start tick of previous note, do NOT advance.
                    noteStart = prevNoteStartTick;
                } else {
                    prevNoteStartTick = currentTick;
                    currentTick += globalDur;
                }
                if (!isRest && pitch >= 0 && globalDur > 0) {
                    XmlNote n;
                    n.startTick = noteStart;
                    n.duration  = globalDur;
                    n.pitch     = pitch;
                    n.velocity  = 80;
                    part->notes.append(n);
                }
            } else if (name == QStringLiteral("backup")) {
                int dur = parseDurationOnly(xr, QStringLiteral("backup"));
                currentTick = std::max(0, currentTick - toGlobal(dur));
            } else if (name == QStringLiteral("forward")) {
                int dur = parseDurationOnly(xr, QStringLiteral("forward"));
                currentTick += toGlobal(dur);
            }
        }
    }

    // ---- <attributes> — divisions, time, key (and nested <sound>) -------
    void parseAttributes(QXmlStreamReader& xr, int& divisions, int currentTick) {
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("attributes"))
                return;
            if (!xr.isStartElement()) continue;

            if (xr.name() == QStringLiteral("divisions")) {
                int d = xr.readElementText().toInt();
                if (d > 0) divisions = d;
            } else if (xr.name() == QStringLiteral("time")) {
                parseTimeSig(xr, currentTick);
            } else if (xr.name() == QStringLiteral("key")) {
                parseKeySig(xr, currentTick);
            }
        }
    }

    void parseTimeSig(QXmlStreamReader& xr, int currentTick) {
        XmlTimeSigEvent ev;
        ev.tick = currentTick;
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("time")) {
                _score->timeSigs.append(ev);
                return;
            }
            if (xr.isStartElement()) {
                if (xr.name() == QStringLiteral("beats"))
                    ev.numerator = xr.readElementText().toInt();
                else if (xr.name() == QStringLiteral("beat-type"))
                    ev.denominator = xr.readElementText().toInt();
            }
        }
    }

    void parseKeySig(QXmlStreamReader& xr, int currentTick) {
        XmlKeySigEvent ev;
        ev.tick = currentTick;
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("key")) {
                _score->keySigs.append(ev);
                return;
            }
            if (xr.isStartElement()) {
                if (xr.name() == QStringLiteral("fifths"))
                    ev.fifths = xr.readElementText().toInt();
                else if (xr.name() == QStringLiteral("mode"))
                    ev.isMinor = (xr.readElementText().toLower() == QStringLiteral("minor"));
            }
        }
    }

    // ---- <direction> — wraps a <sound> with optional tempo --------------
    void parseDirection(QXmlStreamReader& xr, int currentTick) {
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("direction"))
                return;
            if (xr.isStartElement() && xr.name() == QStringLiteral("sound")) {
                handleSoundAttrs(xr.attributes(), currentTick);
            }
        }
    }

    void handleSoundAttrs(const QXmlStreamAttributes& attrs, int currentTick) {
        if (attrs.hasAttribute("tempo")) {
            XmlTempoEvent t;
            t.tick = currentTick;
            t.bpm  = attrs.value("tempo").toDouble();
            if (t.bpm > 0.0) _score->tempos.append(t);
        }
    }

    // ---- <note> — read duration / chord flag / pitch (or rest) ----------
    void parseNote(QXmlStreamReader& xr,
                   int& outDuration, bool& outRest, bool& outChord, int& outPitch) {
        QChar step('C');
        int   octave = 4;
        int   alter  = 0;
        bool  havePitch = false;

        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("note")) {
                if (havePitch) outPitch = pitchToMidi(step, alter, octave);
                return;
            }
            if (!xr.isStartElement()) continue;

            const auto name = xr.name();
            if (name == QStringLiteral("rest")) {
                outRest = true;
                xr.skipCurrentElement();
            } else if (name == QStringLiteral("chord")) {
                outChord = true;
                xr.skipCurrentElement();
            } else if (name == QStringLiteral("duration")) {
                outDuration = xr.readElementText().toInt();
            } else if (name == QStringLiteral("grace")) {
                // Grace notes have no duration — treat as rest of length 0.
                outRest = true;
                xr.skipCurrentElement();
            } else if (name == QStringLiteral("pitch")) {
                havePitch = true;
                while (!xr.atEnd()) {
                    xr.readNext();
                    if (xr.isEndElement() && xr.name() == QStringLiteral("pitch"))
                        break;
                    if (!xr.isStartElement()) continue;
                    if (xr.name() == QStringLiteral("step")) {
                        QString s = xr.readElementText();
                        if (!s.isEmpty()) step = s.at(0);
                    } else if (xr.name() == QStringLiteral("octave")) {
                        octave = xr.readElementText().toInt();
                    } else if (xr.name() == QStringLiteral("alter")) {
                        alter = xr.readElementText().toInt();
                    }
                }
            } else if (name == QStringLiteral("unpitched")) {
                // Drum / unpitched percussion — substitute display pitch.
                havePitch = true;
                while (!xr.atEnd()) {
                    xr.readNext();
                    if (xr.isEndElement() && xr.name() == QStringLiteral("unpitched"))
                        break;
                    if (!xr.isStartElement()) continue;
                    if (xr.name() == QStringLiteral("display-step")) {
                        QString s = xr.readElementText();
                        if (!s.isEmpty()) step = s.at(0);
                    } else if (xr.name() == QStringLiteral("display-octave")) {
                        octave = xr.readElementText().toInt();
                    }
                }
            }
        }
    }

    // ---- <backup> / <forward> — only the <duration> matters -------------
    int parseDurationOnly(QXmlStreamReader& xr, const QString& closeName) {
        int dur = 0;
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == closeName)
                return dur;
            if (xr.isStartElement() && xr.name() == QStringLiteral("duration"))
                dur = xr.readElementText().toInt();
        }
        return dur;
    }
};

} // namespace

// =====================================================================
// Public entry point
// =====================================================================

MidiFile* MusicXmlImporter::loadFile(QString path, bool* ok) {
    if (ok) *ok = false;

    // Load raw bytes — either plain XML or compressed .mxl.
    QByteArray xml;
    QString lower = path.toLower();

    if (lower.endsWith(".mxl")) {
        xml = extractMxl(path);
        if (xml.isEmpty()) return nullptr;
    } else {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return nullptr;
        xml = f.readAll();
        f.close();
    }

    if (xml.isEmpty()) return nullptr;

    XmlScore score;
    Parser parser;
    if (!parser.parse(xml, score)) return nullptr;
    if (score.parts.isEmpty())     return nullptr;

    // Build SMF bytes.
    QByteArray midiBytes = XmlScoreToMidi::encode(score);
    if (midiBytes.isEmpty()) return nullptr;

    // Round-trip via temp file — same pattern as MmlImporter / GpImporter.
    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    if (!tmp.open()) return nullptr;
    tmp.write(midiBytes);
    QString tmpPath = tmp.fileName();
    tmp.close();

    bool midiOk = false;
    MidiFile* mf = new MidiFile(tmpPath, &midiOk);
    QFile::remove(tmpPath);

    if (!midiOk || !mf) {
        delete mf;
        return nullptr;
    }

    mf->setPath(path);
    if (ok) *ok = true;
    return mf;
}
