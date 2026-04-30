#include "MmlImporter.h"
#include "MmlConverter.h"
#include "MmlMidiWriter.h"
#include "../../midi/MidiFile.h"

#include <QFile>
#include <QTemporaryFile>
#include <QTextStream>
#include <QDebug>
#include <QFileInfo>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#else
#include <QTextCodec>
#endif

namespace MML {

MidiFile* MmlImporter::loadFile(QString path, bool* ok) {
    if (ok) *ok = false;

    QFile originalFile(path);
    if (!originalFile.open(QIODevice::ReadOnly)) {
        return nullptr;
    }

    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        originalFile.close();
        return nullptr;
    }

    QByteArray data = originalFile.readAll();
    tempFile.write(data);
    tempFile.flush();
    tempFile.seek(0);
    originalFile.close();

    QTextStream in(&tempFile);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif

    QString content = in.readAll();

    if (content.trimmed().isEmpty()) {
        return nullptr;
    }

    MmlDialect dialect = MmlDialect::Auto;
    if (path.endsWith(".ms2mml", Qt::CaseInsensitive)) {
        dialect = MmlDialect::MapleStory2;
    } else if (path.endsWith(".3mle", Qt::CaseInsensitive)) {
        dialect = MmlDialect::Mabinogi;
    }

    std::string stdContent = content.toStdString();
    std::vector<MmlEvent> events = MmlConverter::ConvertAuto(stdContent, dialect);

    if (events.empty()) {
        return nullptr;
    }

    MidiFile* midiFile = MmlMidiWriter::BuildMidiFile(events, 480);
    
    if (midiFile) {
        if (ok) *ok = true;
    }

    return midiFile;
}

} // namespace MML
