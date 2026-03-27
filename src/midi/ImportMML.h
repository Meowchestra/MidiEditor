#ifndef IMPORTMML_H
#define IMPORTMML_H

#include <QString>

class MidiFile;

class ImportMML {
public:
    static MidiFile* loadFile(QString path, bool* ok);
};

#endif
