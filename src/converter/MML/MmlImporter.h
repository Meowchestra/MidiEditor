#pragma once
#include <QString>

class MidiFile;

namespace MML {

class MmlImporter {
public:
    static MidiFile* loadFile(QString path, bool* ok);
};

} // namespace MML
