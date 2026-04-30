#ifndef MSCZIMPORTER_H
#define MSCZIMPORTER_H

#include <QString>

class MidiFile;

// Importer for native MuseScore project files:
//   .mscz — ZIP container holding a .mscx XML document
//   .mscx — uncompressed MuseScore XML document
//
// Implementation strategy mirrors MusicXmlImporter:
//   parse .mscx XML → XmlScore IR → XmlScoreToMidi::encode → MidiFile
class MsczImporter {
public:
    static MidiFile* loadFile(QString path, bool* ok);
};

#endif // MSCZIMPORTER_H
