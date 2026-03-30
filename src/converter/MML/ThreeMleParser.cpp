#include "ThreeMleParser.h"
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QMap>
#include <QXmlStreamReader>
#include <algorithm>
#include <cmath>

namespace MML {

ThreeMleFile ThreeMleParser::Parse(const std::string& content, MmlDialect dialect) {
    QString qContent = QString::fromStdString(content).trimmed();
    std::string trimmedContent = qContent.toStdString();

    if (IsIniFormat(trimmedContent)) return ParseIni(trimmedContent);
    if (IsXmlFormat(trimmedContent)) return ParseXml(trimmedContent);

    if (IsMabiClipboard(trimmedContent)) {
        bool isMabi = (dialect == MmlDialect::Mabinogi) ||
                      (dialect == MmlDialect::Auto && LooksMabinogi(trimmedContent));
        return ParseMabiClipboard(trimmedContent, isMabi);
    }

    return ParseRawMultiTrack(trimmedContent);
}

bool ThreeMleParser::IsIniFormat(const std::string& s) {
    QString qs = QString::fromStdString(s);
    return qs.contains("[Settings]") ||
           qs.contains(QRegularExpression("^\\[Channel\\d+\\]", QRegularExpression::MultilineOption));
}

bool ThreeMleParser::IsXmlFormat(const std::string& s) {
    QString qs = QString::fromStdString(s).trimmed();
    return qs.startsWith("<") || qs.startsWith("<?");
}

bool ThreeMleParser::IsMabiClipboard(const std::string& s) {
    return QString::fromStdString(s).contains("MML@", Qt::CaseInsensitive);
}

bool ThreeMleParser::LooksMabinogi(const std::string& s) {
    QString qs = QString::fromStdString(s);
    return qs.contains(QRegularExpression("(?i)y\\d+\\s*,\\s*\\d+")) ||
           qs.contains(QRegularExpression("(?i)\\bn\\d{1,3}"));
}

ThreeMleFile ThreeMleParser::ParseIni(const std::string& content) {
    ThreeMleFile file;
    QMap<QString, QStringList> sections;
    QString current = "";
    sections[current] = QStringList();

    QStringList lines = QString::fromStdString(content).split('\n');
    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();
        QRegularExpressionMatch secMatch = QRegularExpression("^\\[(.+?)\\]").match(line);
        if (secMatch.hasMatch()) {
            current = secMatch.captured(1).trimmed();
            if (!sections.contains(current))
                sections[current] = QStringList();
        } else {
            sections[current].append(line);
        }
    }

    if (sections.contains("Settings")) {
        for (const QString& line : sections["Settings"]) {
            QRegularExpressionMatch mTitle = QRegularExpression("(?i)^Title\\s*=\\s*(.+)").match(line);
            if (mTitle.hasMatch()) { file.Title = mTitle.captured(1).trimmed().toStdString(); continue; }

            QRegularExpressionMatch mTempo = QRegularExpression("(?i)^Tempo\\s*=\\s*(\\d+)").match(line);
            if (mTempo.hasMatch()) file.Tempo = mTempo.captured(1).toInt();
        }
    }

    QStringList channelKeys;
    for (auto it = sections.constBegin(); it != sections.constEnd(); ++it) {
        if (it.key().contains(QRegularExpression("(?i)^Channel\\d+$")))
            channelKeys.append(it.key());
    }
    std::sort(channelKeys.begin(), channelKeys.end(), [](const QString& a, const QString& b) {
         return QRegularExpression("\\d+").match(a).captured().toInt() < QRegularExpression("\\d+").match(b).captured().toInt();
    });

    int fallbackChannel = 1;
    for (const QString& key : channelKeys) {
        int sectionNum = QRegularExpression("\\d+").match(key).captured().toInt();
        int midiChannel = sectionNum - 1;
        QString trackName = key;
        QStringList MMLParts;

        for (const QString& rawLine : sections[key]) {
            QString line = rawLine;
            line.replace(QRegularExpression("/\\*M\\s*\\d+\\s*\\*/"), "");
            line = line.trimmed();
            if (line.isEmpty()) continue;

            QRegularExpressionMatch chMatch = QRegularExpression("//\\s*#using_channel\\s*=\\s*(\\d+)").match(line);
            if (chMatch.hasMatch()) {
                midiChannel = chMatch.captured(1).toInt();
                continue;
            }

            QRegularExpressionMatch titleMatch = QRegularExpression("(?i)//\\s*(?:title|name)\\s*[:=]\\s*(.+)").match(line);
            if (titleMatch.hasMatch()) {
                trackName = titleMatch.captured(1).trimmed();
                if (file.Title.empty()) file.Title = trackName.toStdString();
                continue;
            }

            if (line.startsWith("//")) continue;

            line.replace(QRegularExpression("(?s)/\\*.*?\\*/"), "");
            line = line.trimmed();
            if (line.isEmpty()) continue;

            line = QString::fromStdString(NormalizeMabiMML(line.toStdString()));
            if (!line.isEmpty()) MMLParts.append(line);
        }

        QString MML = MMLParts.join(" ").trimmed();
        QString MMLWithoutTempo = MML;
        MMLWithoutTempo.replace(QRegularExpression("(?i)\\bt\\d+\\s*"), "");
        MMLWithoutTempo = MMLWithoutTempo.trimmed();

        if (MMLWithoutTempo.isEmpty()) {
            QRegularExpressionMatch tempoMatch = QRegularExpression("(?i)\\bt(\\d+)").match(MML);
            if (tempoMatch.hasMatch() && file.Tempo == 120)
                file.Tempo = tempoMatch.captured(1).toInt();
            continue;
        }

        ThreeMleTrack track;
        track.Name = trackName.toStdString();
        track.Channel = midiChannel + 1;
        track.MML = MML.toStdString();
        file.Tracks.push_back(track);
        fallbackChannel++;
    }

    if (file.Tempo == 120) ExtractTempoFromTracks(file);

    return file;
}

std::string ThreeMleParser::NormalizeMabiMML(const std::string& mmlStd) {
    QString MML = QString::fromStdString(mmlStd);

    QRegularExpression rxY("(?i)y(\\d+)\\s*,\\s*(\\d+)");
    QRegularExpressionMatchIterator itY = rxY.globalMatch(MML);
    int offset = 0;
    while (itY.hasNext()) {
        QRegularExpressionMatch m = itY.next();
        int cc = m.captured(1).toInt();
        int val = m.captured(2).toInt();
        QString replacement;
        if (cc == 7 || cc == 11) replacement = QString("V%1").arg(qRound(val / 127.0 * 15));
        else if (cc == 10) replacement = QString("P%1").arg(val);
        
        MML.replace(m.capturedStart() + offset, m.capturedLength(), replacement);
        offset += replacement.length() - m.capturedLength();
    }

    QRegularExpression rxN("(?i)\\bn(\\d+)");
    QRegularExpressionMatchIterator itN = rxN.globalMatch(MML);
    QString outN;
    int lastEnd = 0;
    static const QString noteNames[] = { "C", "C+", "D", "D+", "E", "F", "F+", "G", "G+", "A", "A+", "B" };
    while (itN.hasNext()) {
        QRegularExpressionMatch m = itN.next();
        outN += MML.mid(lastEnd, m.capturedStart() - lastEnd);
        int midi = qBound(0, m.captured(1).toInt(), 127);
        int octave = (midi / 12) - 1;
        int semi = midi % 12;
        outN += QString("O%1%2").arg(octave).arg(noteNames[semi]);
        lastEnd = m.capturedEnd();
    }
    outN += MML.mid(lastEnd);
    MML = outN;

    MML.replace(QRegularExpression("\\s+"), " ");
    return MML.trimmed().toStdString();
}

std::string ThreeMleParser::NormalizeGenericMML(const std::string& MML) {
    return QString::fromStdString(MML).replace(QRegularExpression("\\s+"), " ").trimmed().toStdString();
}

std::string ThreeMleParser::NormalizeXmlMML(const std::string& raw) {
    QStringList lines = QString::fromStdString(raw).split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);
    QString out;
    for (const QString& line : lines) {
        if (!out.isEmpty()) out += " ";
        out += line.trimmed();
    }
    return out.trimmed().toStdString();
}

void ThreeMleParser::ExtractTempoFromTracks(ThreeMleFile& file) {
    for (const auto& track : file.Tracks) {
        QRegularExpressionMatch m = QRegularExpression("(?i)\\bt(\\d+)").match(QString::fromStdString(track.MML));
        if (m.hasMatch()) {
            file.Tempo = m.captured(1).toInt();
            return;
        }
    }
}

ThreeMleFile ThreeMleParser::ParseXml(const std::string& xml) {
    ThreeMleFile file;
    file.Tempo = 120;
    QXmlStreamReader reader(QString::fromStdString(xml));

    auto attrStr = [&](const QXmlStreamAttributes& attrs, const QString& name, const QString& fallback) -> QString {
        if (attrs.hasAttribute(name)) return attrs.value(name).toString();
        if (attrs.hasAttribute(name.toLower())) return attrs.value(name.toLower()).toString();
        if (attrs.hasAttribute(name.toUpper())) return attrs.value(name.toUpper()).toString();
        return fallback;
    };
    auto attrInt = [&](const QXmlStreamAttributes& attrs, const QString& name, int fallback) -> int {
        QString s = attrStr(attrs, name, "");
        bool ok;
        int v = s.toInt(&ok);
        return ok ? v : fallback;
    };

    bool rootFound = false;
    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNext();
        if (reader.isStartElement() && !rootFound) {
            rootFound = true;
            file.Title = attrStr(reader.attributes(), "title", attrStr(reader.attributes(), "name", "")).toStdString();
            file.Tempo = attrInt(reader.attributes(), "tempo", 120);
        }

        static const QStringList trackTags = {"track", "part", "channel", "voice", "melody", "harmony", "chord", "rhythm"};
        if (reader.isStartElement() && trackTags.contains(reader.name().toString().toLower())) {
            QString localName = reader.name().toString().toLower();
            int ch = 1;
            int autoCh = file.Tracks.size() + 1;

            if (localName == "melody") ch = 1;
            else if (localName == "chord") ch = attrInt(reader.attributes(), "index", autoCh) + 1;
            else ch = attrInt(reader.attributes(), "channel", attrInt(reader.attributes(), "ch", attrInt(reader.attributes(), "part", autoCh)));

            QString trackName = localName == "melody" ? "Melody"
                : localName == "chord" ? QString("Chord %1").arg(attrInt(reader.attributes(), "index", autoCh))
                : attrStr(reader.attributes(), "name", attrStr(reader.attributes(), "title", QString("Track %1").arg(autoCh)));

            ThreeMleTrack track;
            track.Name = trackName.toStdString();
            track.Channel = ch;
            track.Instrument = attrInt(reader.attributes(), "instrument", attrInt(reader.attributes(), "program", attrInt(reader.attributes(), "inst", 0)));
            
            QString text = reader.readElementText();
            track.MML = NormalizeXmlMML(text.toStdString());

            if (!track.MML.empty()) {
                if (!QString::fromStdString(track.MML).trimmed().startsWith("@")) {
                    track.MML = "@" + std::to_string(track.Instrument) + " " + track.MML;
                }
                file.Tracks.push_back(track);
            }
        }
    }

    if (file.Tempo == 120) ExtractTempoFromTracks(file);
    return file;
}

ThreeMleFile ThreeMleParser::ParseMabiClipboard(const std::string& textStr, bool isMabi) {
    ThreeMleFile file;
    file.Tempo = 120;
    QString text = QString::fromStdString(textStr);

    for (const QString& line : text.split('\n')) {
        QString t = line.trimmed();
        if (t.startsWith("//title=", Qt::CaseInsensitive)) file.Title = t.mid(8).trimmed().toStdString();
        else if (t.startsWith("//tempo=", Qt::CaseInsensitive)) file.Tempo = t.mid(8).trimmed().toInt();
    }

    int start = text.indexOf("MML@", 0, Qt::CaseInsensitive);
    if (start < 0) return file;
    start += 4;

    int end = text.indexOf(';', start);
    QString body = end >= 0 ? text.mid(start, end - start) : text.mid(start);
    QStringList parts = body.split(',', Qt::SkipEmptyParts);

    int channel = 1;
    for (const QString& part : parts) {
        QString MML = part.trimmed();
        if (MML.isEmpty()) continue;

        QString MMLWithoutTempo = MML;
        MMLWithoutTempo.replace(QRegularExpression("(?i)\\bt\\d+\\s*"), "");
        if (MMLWithoutTempo.trimmed().isEmpty()) {
            QRegularExpressionMatch tm = QRegularExpression("(?i)\\bt(\\d+)").match(MML);
            if (tm.hasMatch() && file.Tempo == 120) file.Tempo = tm.captured(1).toInt();
            continue;
        }

        ThreeMleTrack track;
        track.Name = "Track " + std::to_string(channel);
        track.Channel = channel;
        track.Instrument = 0;
        track.MML = isMabi ? NormalizeMabiMML(MML.toStdString()) : NormalizeGenericMML(MML.toStdString());
        file.Tracks.push_back(track);
        channel++;
    }

    if (file.Tempo == 120) ExtractTempoFromTracks(file);

    return file;
}

ThreeMleFile ThreeMleParser::ParseRawMultiTrack(const std::string& text) {
    ThreeMleFile file;
    QStringList parts = QString::fromStdString(text).split(',', Qt::SkipEmptyParts);
    for (int i = 0; i < parts.size(); i++) {
        QString MML = parts[i].trimmed();
        if (!MML.isEmpty()) {
            ThreeMleTrack track;
            track.Name = "Track " + std::to_string(i + 1);
            track.Channel = i + 1;
            track.MML = MML.toStdString();
            file.Tracks.push_back(track);
        }
    }
    return file;
}

} // namespace MML
