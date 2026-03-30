#ifndef GPIMPORTER_H
#define GPIMPORTER_H

#include <QString>

class MidiFile;

// Main entry point for Guitar Pro file import.
class GpImporter {
public:
    // Load a Guitar Pro file (.gp3, .gp4, .gp5, .gp6, .gp7, .gp8, .gpx, .gp)
    // Returns a MidiFile* (caller takes ownership), or nullptr on failure.
    // Sets *ok to true/false.
    static MidiFile* loadFile(QString path, bool* ok);
};

#endif // GPIMPORTER_H
