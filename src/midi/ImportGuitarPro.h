#ifndef IMPORTGUITARPRO_H
#define IMPORTGUITARPRO_H

#include <QString>

class MidiFile;

class ImportGuitarPro {
public:
    static MidiFile* loadFile(QString path, bool* ok);
};

#endif
