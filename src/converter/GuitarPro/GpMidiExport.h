#ifndef GPMIDIEXPORT_H
#define GPMIDIEXPORT_H

#include <vector>
#include <string>
#include <cstdint>
#include <memory>

// Ported from MidiExport.cs — generates standard MIDI file bytes

struct GpMidiMessage {
    std::string type;
    int time = 0;
    bool is_meta = false;

    // Meta fields
    int number = 0;
    std::string text;
    std::string name;
    int channel = 0;
    int port = 0;
    int tempo = 500000;
    int numerator = 4;
    int denominator = 2;
    int clocks_per_click = 24;
    int notated_32nd_notes_per_beat = 8;
    int key = 0;
    bool is_major = true;

    // Channel message fields
    int note = 0;
    int velocity = 0;
    int value = 0;
    int control = 0;
    int program = 0;
    int pitch = 0;
    std::vector<uint8_t> data;

    GpMidiMessage(const std::string& type, const std::vector<std::string>& args,
                  int time, const std::vector<uint8_t>& sysexData = {});

    std::vector<uint8_t> createBytes() const;

private:
    uint8_t code_ = 0x00;
    std::vector<uint8_t> createMetaBytes() const;
    std::vector<uint8_t> createMessageBytes() const;
};

struct GpMidiTrack {
    std::vector<std::unique_ptr<GpMidiMessage>> messages;
    std::vector<uint8_t> createBytes() const;
};

class GpMidiExport {
public:
    int fileType = 1;
    int ticksPerBeat = 960;
    std::vector<std::unique_ptr<GpMidiTrack>> midiTracks;

    GpMidiExport(int fileType = 1, int ticksPerBeat = 960);

    std::vector<uint8_t> createBytes() const;

    // Static helpers
    static std::vector<uint8_t> writeChunk(const std::string& name, const std::vector<uint8_t>& data);
    static std::vector<uint8_t> toBEULong(int val);
    static std::vector<uint8_t> toBEShort(int val);
    static std::vector<uint8_t> encodeVariableInt(int val);

private:
    std::vector<uint8_t> createHeader() const;
};

#endif // GPMIDIEXPORT_H
