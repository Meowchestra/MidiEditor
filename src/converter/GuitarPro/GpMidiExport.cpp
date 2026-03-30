#include "GpMidiExport.h"
#include <cmath>
#include <stdexcept>
#include <cstring>
#include <algorithm>

// ============================================================
// GpMidiMessage — ported from MidiExport.cs MidiMessage
// ============================================================

GpMidiMessage::GpMidiMessage(const std::string& type_, const std::vector<std::string>& args,
                             int time_, const std::vector<uint8_t>& sysexData)
    : type(type_), time(time_) {
    is_meta = false;
    data = sysexData;

    auto parseInt = [](const std::string& s) -> int { return std::stoi(s); };

    // Meta messages
    if (type == "sequence_number") { is_meta = true; code_ = 0x00; number = parseInt(args[0]); }
    if (type == "text" || type == "copyright" || type == "lyrics" ||
        type == "marker" || type == "cue_marker") {
        is_meta = true;
        text = args[0];
    }
    if (type == "text") code_ = 0x01;
    if (type == "copyright") code_ = 0x02;
    if (type == "lyrics") code_ = 0x05;
    if (type == "marker") code_ = 0x06;
    if (type == "cue_marker") code_ = 0x07;

    if (type == "track_name" || type == "instrument_name" || type == "device_name") {
        is_meta = true; code_ = 0x03;
        name = args[0];
    }
    if (type == "instrument_name") code_ = 0x04;
    if (type == "device_name") code_ = 0x08;

    if (type == "channel_prefix") { code_ = 0x20; channel = parseInt(args[0]); is_meta = true; }
    if (type == "midi_port") { code_ = 0x21; port = parseInt(args[0]); is_meta = true; }
    if (type == "end_of_track") { code_ = 0x2f; is_meta = true; }
    if (type == "set_tempo") { code_ = 0x51; tempo = parseInt(args[0]); is_meta = true; }

    if (type == "time_signature") {
        is_meta = true; code_ = 0x58;
        numerator = parseInt(args[0]);
        denominator = parseInt(args[1]);
        clocks_per_click = parseInt(args[2]);
        notated_32nd_notes_per_beat = parseInt(args[3]);
    }

    if (type == "key_signature") {
        is_meta = true; code_ = 0x59;
        key = parseInt(args[0]);
        is_major = (args[1] == "0");
    }

    // Channel messages
    if (type == "note_off") {
        code_ = 0x80;
        channel = parseInt(args[0]); note = parseInt(args[1]); velocity = parseInt(args[2]);
    }
    if (type == "note_on") {
        code_ = 0x90;
        channel = parseInt(args[0]); note = parseInt(args[1]); velocity = parseInt(args[2]);
    }
    if (type == "polytouch") {
        code_ = 0xa0;
        channel = parseInt(args[0]); note = parseInt(args[1]); value = parseInt(args[2]);
    }
    if (type == "control_change") {
        code_ = 0xb0;
        channel = parseInt(args[0]); control = parseInt(args[1]); value = parseInt(args[2]);
    }
    if (type == "program_change") {
        code_ = 0xc0;
        channel = parseInt(args[0]); program = parseInt(args[1]);
    }
    if (type == "aftertouch") {
        code_ = 0xd0;
        channel = parseInt(args[0]); value = parseInt(args[1]);
    }
    if (type == "pitchwheel") {
        code_ = 0xe0;
        channel = parseInt(args[0]); pitch = parseInt(args[1]);
    }
    if (type == "sysex") {
        code_ = 0xf0;
    }
}

std::vector<uint8_t> GpMidiMessage::createBytes() const {
    if (is_meta) return createMetaBytes();
    return createMessageBytes();
}

std::vector<uint8_t> GpMidiMessage::createMetaBytes() const {
    std::vector<uint8_t> payload;

    if (type == "sequence_number") {
        payload.push_back(static_cast<uint8_t>(number >> 8));
        payload.push_back(static_cast<uint8_t>(number & 0xff));
    }
    if (type == "text" || type == "copyright" || type == "lyrics" ||
        type == "marker" || type == "cue_marker") {
        std::string t = text.empty() ? "" : text;
        payload.insert(payload.end(), t.begin(), t.end());
    }
    if (type == "track_name" || type == "instrument_name" || type == "device_name") {
        payload.insert(payload.end(), name.begin(), name.end());
    }
    if (type == "channel_prefix") {
        payload.push_back(static_cast<uint8_t>(channel));
    }
    if (type == "midi_port") {
        payload.push_back(static_cast<uint8_t>(port));
    }
    if (type == "set_tempo") {
        payload.push_back(static_cast<uint8_t>(tempo >> 16));
        payload.push_back(static_cast<uint8_t>((tempo >> 8) & 0xff));
        payload.push_back(static_cast<uint8_t>(tempo & 0xff));
    }
    if (type == "time_signature") {
        payload.push_back(static_cast<uint8_t>(numerator));
        payload.push_back(static_cast<uint8_t>(std::log2(denominator)));
        payload.push_back(static_cast<uint8_t>(clocks_per_click));
        payload.push_back(static_cast<uint8_t>(notated_32nd_notes_per_beat));
    }
    if (type == "key_signature") {
        payload.push_back(static_cast<uint8_t>(key & 0xff));
        payload.push_back(is_major ? static_cast<uint8_t>(0x00) : static_cast<uint8_t>(0x01));
    }

    // Build final: 0xFF, code, variable-length-size, payload
    std::vector<uint8_t> result;
    result.push_back(0xff);
    result.push_back(code_);
    auto sizeBytes = GpMidiExport::encodeVariableInt(static_cast<int>(payload.size()));
    result.insert(result.end(), sizeBytes.begin(), sizeBytes.end());
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

std::vector<uint8_t> GpMidiMessage::createMessageBytes() const {
    std::vector<uint8_t> result;

    if (type == "note_off" || type == "note_on") {
        result.push_back(static_cast<uint8_t>(code_ | static_cast<uint8_t>(channel)));
        result.push_back(static_cast<uint8_t>(note));
        result.push_back(static_cast<uint8_t>(velocity));
    }
    else if (type == "polytouch") {
        result.push_back(static_cast<uint8_t>(code_ | static_cast<uint8_t>(channel)));
        result.push_back(static_cast<uint8_t>(note));
        result.push_back(static_cast<uint8_t>(value));
    }
    else if (type == "control_change") {
        result.push_back(static_cast<uint8_t>(code_ | static_cast<uint8_t>(channel)));
        result.push_back(static_cast<uint8_t>(control));
        result.push_back(static_cast<uint8_t>(value));
    }
    else if (type == "program_change") {
        result.push_back(static_cast<uint8_t>(code_ | static_cast<uint8_t>(channel)));
        result.push_back(static_cast<uint8_t>(program));
    }
    else if (type == "aftertouch") {
        result.push_back(static_cast<uint8_t>(code_ | static_cast<uint8_t>(channel)));
        result.push_back(static_cast<uint8_t>(value));
    }
    else if (type == "pitchwheel") {
        result.push_back(static_cast<uint8_t>(code_ | static_cast<uint8_t>(channel)));
        int p = pitch - (-8192); // offset to unsigned
        result.push_back(static_cast<uint8_t>(p & 0x7f));
        result.push_back(static_cast<uint8_t>(p >> 7));
    }
    else if (type == "sysex") {
        result.insert(result.end(), data.begin(), data.end());
    }

    return result;
}

// ============================================================
// GpMidiTrack — ported from MidiExport.cs MidiTrack
// ============================================================

std::vector<uint8_t> GpMidiTrack::createBytes() const {
    std::vector<uint8_t> trackData;
    uint8_t runningStatusByte = 0x00;
    bool statusByteSet = false;

    for (const auto& message : messages) {
        int t = std::max(0, message->time);
        auto timeBytes = GpMidiExport::encodeVariableInt(t);
        trackData.insert(trackData.end(), timeBytes.begin(), timeBytes.end());

        if (message->type == "sysex") {
            statusByteSet = false;
            trackData.push_back(0xf0);
            auto sizeBytes = GpMidiExport::encodeVariableInt(static_cast<int>(message->data.size()) + 1);
            trackData.insert(trackData.end(), sizeBytes.begin(), sizeBytes.end());
            trackData.insert(trackData.end(), message->data.begin(), message->data.end());
            trackData.push_back(0xf7);
        } else {
            auto raw = message->createBytes();
            uint8_t temp = raw[0];
            if (statusByteSet && !message->is_meta && raw[0] < 0xf0 && raw[0] == runningStatusByte) {
                // Running status: skip the status byte
                trackData.insert(trackData.end(), raw.begin() + 1, raw.end());
            } else {
                trackData.insert(trackData.end(), raw.begin(), raw.end());
            }
            runningStatusByte = temp;
            statusByteSet = true;
        }
    }

    return GpMidiExport::writeChunk("MTrk", trackData);
}

// ============================================================
// GpMidiExport — ported from MidiExport.cs MidiExport
// ============================================================

GpMidiExport::GpMidiExport(int ft, int tpb) : fileType(ft), ticksPerBeat(tpb) {}

std::vector<uint8_t> GpMidiExport::createBytes() const {
    std::vector<uint8_t> result;
    auto hdr = createHeader();
    result.insert(result.end(), hdr.begin(), hdr.end());
    for (const auto& track : midiTracks) {
        auto trackBytes = track->createBytes();
        result.insert(result.end(), trackBytes.begin(), trackBytes.end());
    }
    return result;
}

std::vector<uint8_t> GpMidiExport::createHeader() const {
    std::vector<uint8_t> header;
    auto ft = toBEShort(fileType);
    header.insert(header.end(), ft.begin(), ft.end());
    auto tc = toBEShort(static_cast<int>(midiTracks.size()));
    header.insert(header.end(), tc.begin(), tc.end());
    auto tpb = toBEShort(ticksPerBeat);
    header.insert(header.end(), tpb.begin(), tpb.end());
    return writeChunk("MThd", header);
}

std::vector<uint8_t> GpMidiExport::writeChunk(const std::string& name, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> result;
    result.insert(result.end(), name.begin(), name.end());
    auto size = toBEULong(static_cast<int>(data.size()));
    result.insert(result.end(), size.begin(), size.end());
    result.insert(result.end(), data.begin(), data.end());
    return result;
}

std::vector<uint8_t> GpMidiExport::toBEULong(int val) {
    uint32_t v = static_cast<uint32_t>(val);
    return {
        static_cast<uint8_t>((v >> 24) & 0xff),
        static_cast<uint8_t>((v >> 16) & 0xff),
        static_cast<uint8_t>((v >> 8) & 0xff),
        static_cast<uint8_t>(v & 0xff)
    };
}

std::vector<uint8_t> GpMidiExport::toBEShort(int val) {
    int16_t v = static_cast<int16_t>(val);
    return {
        static_cast<uint8_t>((v >> 8) & 0xff),
        static_cast<uint8_t>(v & 0xff)
    };
}

std::vector<uint8_t> GpMidiExport::encodeVariableInt(int val) {
    if (val < 0) throw std::runtime_error("Variable int must be positive.");

    std::vector<uint8_t> data;
    if (val == 0) {
        data.push_back(0x00);
        return data;
    }

    while (val > 0) {
        data.push_back(static_cast<uint8_t>(val & 0x7f));
        val >>= 7;
    }

    std::reverse(data.begin(), data.end());
    for (size_t i = 0; i < data.size() - 1; i++) {
        data[i] |= 0x80;
    }

    return data;
}
