#include "MsczImporter.h"
#include "MusicXmlModels.h"
#include "XmlScoreToMidi.h"

#include "../../midi/MidiFile.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStack>
#include <QTemporaryFile>
#include <QXmlStreamReader>
#include <algorithm>
#include <vector>

#include "../GuitarPro/GpUnzip.h"

namespace {

constexpr int kPpq = 960;

// Duration in ticks for a base duration type (no dots, no tuplet).
int baseTicks(const QString& type) {
    static const QHash<QString, int> kMap = {
        {"long",    kPpq * 16},   // longa = 16 quarters
        {"breve",   kPpq * 8},
        {"whole",   kPpq * 4},
        {"half",    kPpq * 2},
        {"quarter", kPpq},
        {"eighth",  kPpq / 2},
        {"16th",    kPpq / 4},
        {"32nd",    kPpq / 8},
        {"64th",    kPpq / 16},
        {"128th",   kPpq / 32},
        {"256th",   kPpq / 64},
        {"zero",    0},
    };
    return kMap.value(type, kPpq);
}

int applyDots(int base, int dots) {
    int total = base;
    int add = base;
    for (int i = 0; i < dots; ++i) {
        add /= 2;
        total += add;
    }
    return total;
}

// =====================================================================
// .mscz extraction
// =====================================================================

QByteArray extractMscz(const QString& path) {
    GpUnzip zip(path.toStdString());

    // Spec says META-INF/container.xml lists the rootfile (usually a .mscx).
    if (zip.hasEntry("META-INF/container.xml")) {
        std::vector<uint8_t> cb = zip.extract("META-INF/container.xml");
        QByteArray container(reinterpret_cast<const char*>(cb.data()),
                             static_cast<int>(cb.size()));
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

    // Fallback — find any .mscx entry by walking known names. GpUnzip
    // doesn't enumerate, so try the most common ones first.
    static const std::vector<std::string> candidates = {
        "score.mscx", "Score.mscx",
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
// .mscx parser
// =====================================================================

struct PartInfo {
    QString id;            // <Part id="..."> or order index as string
    QString name;
    int channel = 0;
    int program = 0;
    QStringList staffIds;  // <Staff id="N"/> entries inside <Part>
};

class MscxParser {
public:
    struct Tup { int actual; int normal; };

    bool parse(const QByteArray& xml, XmlScore& out) {
        out = {};
        out.ticksPerQuarter = kPpq;

        QXmlStreamReader xr(xml);
        while (!xr.atEnd() && !xr.hasError()) {
            xr.readNext();
            if (xr.isStartElement() && xr.name() == QStringLiteral("Score")) {
                parseScore(xr);
            }
        }
        if (xr.hasError()) return false;

        // Materialize XmlParts in declaration order, merging staves that
        // belong to the same <Part>.
        for (const PartInfo& p : _parts) {
            XmlPart xp;
            xp.id = p.id;
            xp.name = p.name;
            xp.channel = p.channel;
            xp.program = p.program;
            for (const QString& sid : p.staffIds) {
                auto it = _staffNotes.find(sid);
                if (it != _staffNotes.end()) {
                    xp.notes.append(*it);
                }
            }
            // Sort notes by start tick.
            std::sort(xp.notes.begin(), xp.notes.end(),
                [](const XmlNote& a, const XmlNote& b) {
                    return a.startTick < b.startTick;
                });
            out.parts.append(xp);
        }
        out.tempos   = _tempos;
        out.timeSigs = _timeSigs;
        out.keySigs  = _keySigs;
        return true;
    }

private:
    QList<PartInfo> _parts;
    QHash<QString, QList<XmlNote>> _staffNotes;
    QList<XmlTempoEvent> _tempos;
    QList<XmlTimeSigEvent> _timeSigs;
    QList<XmlKeySigEvent> _keySigs;
    int _currentTimeSigNum = 4;
    int _currentTimeSigDen = 4;
    int _midiChannelCounter = 0;

    int currentMeasureTicks() const {
        return (kPpq * 4 / std::max(1, _currentTimeSigDen)) * _currentTimeSigNum;
    }

    void parseScore(QXmlStreamReader& xr) {
        // Track the start tick of the next measure across all staves; we
        // assume MuseScore's <Staff id="N"> blocks each contain the same
        // sequence of <Measure> elements with identical durations.
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("Score")) return;
            if (xr.isStartElement()) {
                const auto n = xr.name();
                if (n == QStringLiteral("Part")) {
                    parsePart(xr);
                } else if (n == QStringLiteral("Staff")) {
                    QString sid = xr.attributes().value("id").toString();
                    parseStaff(xr, sid);
                }
            }
        }
    }

    void parsePart(QXmlStreamReader& xr) {
        PartInfo p;
        p.id = xr.attributes().value("id").toString();
        if (p.id.isEmpty()) p.id = QString::number(_parts.size() + 1);
        p.channel = (_midiChannelCounter++) & 0x0F;
        // MuseScore convention: skip channel 9 (drums) for melodic parts.
        if (p.channel == 9) p.channel = (_midiChannelCounter++) & 0x0F;

        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("Part")) break;
            if (!xr.isStartElement()) continue;
            const auto n = xr.name();
            if (n == QStringLiteral("Staff")) {
                p.staffIds.append(xr.attributes().value("id").toString());
            } else if (n == QStringLiteral("trackName") ||
                       n == QStringLiteral("name")) {
                if (p.name.isEmpty()) p.name = xr.readElementText().trimmed();
            } else if (n == QStringLiteral("Instrument")) {
                parseInstrument(xr, p);
            }
        }
        _parts.append(p);
    }

    void parseInstrument(QXmlStreamReader& xr, PartInfo& p) {
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("Instrument")) return;
            if (!xr.isStartElement()) continue;
            const auto n = xr.name();
            if (n == QStringLiteral("longName") || n == QStringLiteral("trackName")) {
                if (p.name.isEmpty()) p.name = xr.readElementText().trimmed();
            } else if (n == QStringLiteral("Channel")) {
                parseInstrumentChannel(xr, p);
            }
        }
    }

    void parseInstrumentChannel(QXmlStreamReader& xr, PartInfo& p) {
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("Channel")) return;
            if (!xr.isStartElement()) continue;
            const auto n = xr.name();
            if (n == QStringLiteral("program")) {
                QString v = xr.attributes().value("value").toString();
                if (!v.isEmpty()) p.program = std::clamp(v.toInt(), 0, 127);
            }
        }
    }

    void parseStaff(QXmlStreamReader& xr, const QString& staffId) {
        int cursor = 0;
        // Tuplet stack: each entry is (actual, normal). The cumulative ratio
        // is product(normal)/product(actual).
        QStack<Tup> tupletStack;

        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("Staff")) return;
            if (!xr.isStartElement()) continue;
            if (xr.name() == QStringLiteral("Measure")) {
                int measureStart = cursor;
                int measureLen = currentMeasureTicks();
                parseMeasure(xr, staffId, measureStart, measureLen, tupletStack);
                cursor = measureStart + measureLen;
            }
        }
    }

    int applyTuplets(int rawTicks, const QStack<Tup>& stack) const {
        long long t = rawTicks;
        for (const auto& tup : stack) {
            if (tup.actual > 0)
                t = t * tup.normal / tup.actual;
        }
        return static_cast<int>(t);
    }

    void parseMeasure(QXmlStreamReader& xr, const QString& staffId,
                      int measureStart, int measureLen,
                      QStack<Tup>& tupletStack) {
        // Each <voice> resets cursor to measureStart.
        int cursor = measureStart;
        int voiceIdx = 0;

        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("Measure")) return;
            if (!xr.isStartElement()) continue;

            const auto n = xr.name();
            if (n == QStringLiteral("voice")) {
                if (voiceIdx > 0) {
                    cursor = measureStart;
                }
                voiceIdx++;
                parseVoice(xr, staffId, measureStart, measureLen,
                           cursor, tupletStack);
            } else if (n == QStringLiteral("KeySig")) {
                parseKeySig(xr, measureStart);
            } else if (n == QStringLiteral("TimeSig")) {
                parseTimeSig(xr, measureStart);
                measureLen = currentMeasureTicks();
            } else if (n == QStringLiteral("Tempo")) {
                parseTempo(xr, measureStart);
            } else if (n == QStringLiteral("Chord") || n == QStringLiteral("Rest") ||
                       n == QStringLiteral("Tuplet") || n == QStringLiteral("endTuplet")) {
                // No <voice> wrapper — treat as implicit voice 1.
                handleVoiceElement(xr, n.toString(), staffId,
                                   measureStart, cursor, tupletStack);
            }
        }
    }

    void parseVoice(QXmlStreamReader& xr, const QString& staffId,
                    int measureStart, int measureLen,
                    int& cursor, QStack<Tup>& tupletStack) {
        Q_UNUSED(measureLen);
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("voice")) return;
            if (!xr.isStartElement()) continue;
            const auto n = xr.name();
            if (n == QStringLiteral("KeySig")) {
                parseKeySig(xr, measureStart);
            } else if (n == QStringLiteral("TimeSig")) {
                parseTimeSig(xr, measureStart);
            } else if (n == QStringLiteral("Tempo")) {
                parseTempo(xr, measureStart);
            } else {
                handleVoiceElement(xr, n.toString(), staffId,
                                   measureStart, cursor, tupletStack);
            }
        }
    }

    void handleVoiceElement(QXmlStreamReader& xr, const QString& tag,
                            const QString& staffId, int measureStart,
                            int& cursor, QStack<Tup>& tupletStack) {
        Q_UNUSED(measureStart);
        if (tag == QStringLiteral("Tuplet")) {
            Tup t = parseTuplet(xr);
            tupletStack.push(t);
        } else if (tag == QStringLiteral("endTuplet")) {
            if (!tupletStack.isEmpty()) tupletStack.pop();
            // <endTuplet/> is empty — consume self-close if any.
            if (xr.isStartElement()) {
                // skip; already at start, no body expected for self-closing
            }
        } else if (tag == QStringLiteral("Chord")) {
            parseChord(xr, staffId, cursor, tupletStack);
        } else if (tag == QStringLiteral("Rest")) {
            int dur = parseDurationOnly(xr);
            cursor += applyTuplets(dur, tupletStack);
        }
    }

    Tup parseTuplet(QXmlStreamReader& xr) {
        Tup t{1, 1};
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("Tuplet")) break;
            if (!xr.isStartElement()) continue;
            if (xr.name() == QStringLiteral("actualNotes"))
                t.actual = std::max(1, xr.readElementText().toInt());
            else if (xr.name() == QStringLiteral("normalNotes"))
                t.normal = std::max(1, xr.readElementText().toInt());
        }
        return t;
    }

    int parseDurationOnly(QXmlStreamReader& xr) {
        QString endTag = xr.name().toString();
        QString type = "quarter";
        int dots = 0;
        int durOverride = -1;
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == endTag) break;
            if (!xr.isStartElement()) continue;
            if (xr.name() == QStringLiteral("durationType"))
                type = xr.readElementText().trimmed();
            else if (xr.name() == QStringLiteral("dots"))
                dots = xr.readElementText().toInt();
            else if (xr.name() == QStringLiteral("duration")) {
                // MuseScore sometimes writes <duration>X/Y</duration> as a
                // fraction; only used for whole-measure rests.
                QString s = xr.readElementText().trimmed();
                int slash = s.indexOf('/');
                if (slash > 0) {
                    int num = s.left(slash).toInt();
                    int den = s.mid(slash + 1).toInt();
                    if (den > 0) durOverride = kPpq * 4 * num / den;
                }
            }
        }
        if (type == "measure" && durOverride < 0) durOverride = currentMeasureTicks();
        if (durOverride >= 0) return durOverride;
        return applyDots(baseTicks(type), dots);
    }

    void parseChord(QXmlStreamReader& xr, const QString& staffId,
                    int& cursor, QStack<Tup>& tupletStack) {
        QString type = "quarter";
        int dots = 0;
        QList<int> pitches;
        bool grace = false;

        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("Chord")) break;
            if (!xr.isStartElement()) continue;
            const auto n = xr.name();
            if (n == QStringLiteral("durationType")) {
                type = xr.readElementText().trimmed();
            } else if (n == QStringLiteral("dots")) {
                dots = xr.readElementText().toInt();
            } else if (n.startsWith(QStringLiteral("acciaccatura")) ||
                       n.startsWith(QStringLiteral("appoggiatura")) ||
                       n.startsWith(QStringLiteral("grace"))) {
                grace = true;
                xr.skipCurrentElement();
            } else if (n == QStringLiteral("Note")) {
                int p = parseNote(xr);
                if (p >= 0) pitches.append(p);
            }
        }

        int dur = applyDots(baseTicks(type), dots);
        dur = applyTuplets(dur, tupletStack);

        if (grace) {
            // Grace notes don't consume time; emit them as a very short note
            // before the principal beat.
            int gdur = std::max(1, kPpq / 16);
            int start = std::max(0, cursor - gdur);
            for (int p : pitches) {
                XmlNote n;
                n.startTick = start;
                n.duration = gdur;
                n.pitch = p;
                n.velocity = 90;
                _staffNotes[staffId].append(n);
            }
            return;
        }

        for (int p : pitches) {
            XmlNote n;
            n.startTick = cursor;
            n.duration = std::max(1, dur);
            n.pitch = p;
            n.velocity = 90;
            _staffNotes[staffId].append(n);
        }
        cursor += dur;
    }

    int parseNote(QXmlStreamReader& xr) {
        int pitch = -1;
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("Note")) break;
            if (!xr.isStartElement()) continue;
            if (xr.name() == QStringLiteral("pitch")) {
                pitch = std::clamp(xr.readElementText().toInt(), 0, 127);
            }
        }
        return pitch;
    }

    void parseTimeSig(QXmlStreamReader& xr, int tick) {
        int n = 4, d = 4;
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("TimeSig")) break;
            if (!xr.isStartElement()) continue;
            if (xr.name() == QStringLiteral("sigN")) n = std::max(1, xr.readElementText().toInt());
            else if (xr.name() == QStringLiteral("sigD")) d = std::max(1, xr.readElementText().toInt());
        }
        XmlTimeSigEvent ts;
        ts.tick = tick;
        ts.numerator = n;
        ts.denominator = d;
        _timeSigs.append(ts);
        _currentTimeSigNum = n;
        _currentTimeSigDen = d;
    }

    void parseKeySig(QXmlStreamReader& xr, int tick) {
        int fifths = 0;
        bool minor = false;
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("KeySig")) break;
            if (!xr.isStartElement()) continue;
            const auto n = xr.name();
            if (n == QStringLiteral("accidental") || n == QStringLiteral("concertKey") ||
                n == QStringLiteral("key")) {
                fifths = std::clamp(xr.readElementText().toInt(), -7, 7);
            } else if (n == QStringLiteral("mode")) {
                if (xr.readElementText().compare("minor", Qt::CaseInsensitive) == 0)
                    minor = true;
            }
        }
        XmlKeySigEvent k;
        k.tick = tick;
        k.fifths = fifths;
        k.isMinor = minor;
        _keySigs.append(k);
    }

    void parseTempo(QXmlStreamReader& xr, int tick) {
        // MuseScore stores <tempo> as beats-per-second (e.g. 2.0 = 120 BPM).
        double bps = 2.0;
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isEndElement() && xr.name() == QStringLiteral("Tempo")) break;
            if (!xr.isStartElement()) continue;
            if (xr.name() == QStringLiteral("tempo")) {
                bps = xr.readElementText().toDouble();
            }
        }
        XmlTempoEvent t;
        t.tick = tick;
        t.bpm = bps * 60.0;
        if (t.bpm < 10.0 || t.bpm > 999.0) t.bpm = 120.0;
        _tempos.append(t);
    }
};

} // namespace

MidiFile* MsczImporter::loadFile(QString path, bool* ok) {
    if (ok) *ok = false;

    QByteArray xml;
    QString lower = path.toLower();

    if (lower.endsWith(".mscz")) {
        xml = extractMscz(path);
        if (xml.isEmpty()) return nullptr;
    } else {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return nullptr;
        xml = f.readAll();
        f.close();
    }

    if (xml.isEmpty()) return nullptr;

    XmlScore score;
    MscxParser parser;
    if (!parser.parse(xml, score)) return nullptr;

    // Title from filename.
    QFileInfo fi(path);
    score.title = fi.completeBaseName();

    if (score.parts.isEmpty()) return nullptr;

    QByteArray midiBytes = XmlScoreToMidi::encode(score);
    if (midiBytes.isEmpty()) return nullptr;

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
