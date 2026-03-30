#pragma once
#include "MmlModels.h"
#include "../../midi/MidiFile.h"
#include <vector>

namespace MML {

class MmlMidiWriter {
public:
    static MidiFile* BuildMidiFile(const std::vector<MmlEvent>& events, int ppq);
};

} // namespace MML
