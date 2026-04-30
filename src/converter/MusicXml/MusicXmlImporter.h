#ifndef MUSICXMLIMPORTER_H
#define MUSICXMLIMPORTER_H

#include <QString>

class MidiFile;

// Imports MusicXML files (.musicxml, .xml) and compressed MusicXML (.mxl).
// Parses the XML, builds an intermediate score model, writes Standard MIDI
// File bytes to a temporary file, then loads it via MidiFile(tempPath).
//
// Pattern matches MmlImporter / GpImporter — same loadFile signature so
// MainWindow::openFile() can dispatch by extension.
class MusicXmlImporter {
public:
    static MidiFile* loadFile(QString path, bool* ok);
};

#endif // MUSICXMLIMPORTER_H
