#include "GpUnzip.h"
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <zlib.h>

GpUnzip::GpUnzip(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("GpUnzip: cannot open file: " + filePath);
    }
    data_ = std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                                  std::istreambuf_iterator<char>());
    parseEntries();
}

GpUnzip::GpUnzip(const std::vector<uint8_t>& data) : data_(data) {
    parseEntries();
}

void GpUnzip::parseEntries() {
    size_t pos = 0;
    while (pos + 30 <= data_.size()) {
        // Check for local file header signature: PK\x03\x04
        if (data_[pos] != 0x50 || data_[pos + 1] != 0x4B ||
            data_[pos + 2] != 0x03 || data_[pos + 3] != 0x04) {
            break; // No more local file headers
        }

        ZipEntry entry;

        // Read local file header fields
        uint16_t compressionMethod;
        std::memcpy(&compressionMethod, &data_[pos + 8], 2);
        entry.compressionMethod = compressionMethod;

        uint32_t compressedSize, uncompressedSize;
        std::memcpy(&compressedSize, &data_[pos + 18], 4);
        std::memcpy(&uncompressedSize, &data_[pos + 22], 4);
        entry.compressedSize = compressedSize;
        entry.uncompressedSize = uncompressedSize;

        uint16_t filenameLen, extraLen;
        std::memcpy(&filenameLen, &data_[pos + 26], 2);
        std::memcpy(&extraLen, &data_[pos + 28], 2);

        if (pos + 30 + filenameLen > data_.size()) break;

        entry.filename = std::string(reinterpret_cast<const char*>(&data_[pos + 30]), filenameLen);
        entry.dataOffset = static_cast<uint32_t>(pos + 30 + filenameLen + extraLen);

        // Handle data descriptor (bit 3 of general purpose flags)
        uint16_t gpFlags;
        std::memcpy(&gpFlags, &data_[pos + 6], 2);

        entries_.push_back(entry);

        // Move to next entry
        pos = entry.dataOffset + entry.compressedSize;

        // If data descriptor is present, skip it
        if (gpFlags & 0x08) {
            // Data descriptor may or may not have signature
            if (pos + 4 <= data_.size() &&
                data_[pos] == 0x50 && data_[pos + 1] == 0x4B &&
                data_[pos + 2] == 0x07 && data_[pos + 3] == 0x08) {
                // Has signature — read crc32, compressed size, uncompressed size
                if (pos + 16 <= data_.size()) {
                    std::memcpy(&entry.compressedSize, &data_[pos + 8], 4);
                    std::memcpy(&entry.uncompressedSize, &data_[pos + 12], 4);
                    entries_.back().compressedSize = entry.compressedSize;
                    entries_.back().uncompressedSize = entry.uncompressedSize;
                }
                pos += 16;
            } else {
                // No signature — just 3 fields of 4 bytes each
                if (pos + 12 <= data_.size()) {
                    std::memcpy(&entry.compressedSize, &data_[pos + 4], 4);
                    std::memcpy(&entry.uncompressedSize, &data_[pos + 8], 4);
                    entries_.back().compressedSize = entry.compressedSize;
                    entries_.back().uncompressedSize = entry.uncompressedSize;
                }
                pos += 12;
            }
        }
    }
}

bool GpUnzip::hasEntry(const std::string& entryPath) const {
    for (const auto& entry : entries_) {
        if (entry.filename == entryPath) return true;
    }
    return false;
}

std::vector<uint8_t> GpUnzip::extract(const std::string& entryPath) {
    for (const auto& entry : entries_) {
        if (entry.filename == entryPath) {
            if (entry.compressionMethod == 0) {
                // Stored (no compression)
                return std::vector<uint8_t>(
                    data_.begin() + entry.dataOffset,
                    data_.begin() + entry.dataOffset + entry.compressedSize);
            } else if (entry.compressionMethod == 8) {
                // Deflated
                return inflate(&data_[entry.dataOffset],
                               entry.compressedSize, entry.uncompressedSize);
            } else {
                throw std::runtime_error("GpUnzip: unsupported compression method: " +
                                         std::to_string(entry.compressionMethod));
            }
        }
    }
    throw std::runtime_error("GpUnzip: entry not found: " + entryPath);
}

std::vector<uint8_t> GpUnzip::inflate(const uint8_t* compressedData,
                                       uint32_t compressedSize,
                                       uint32_t uncompressedSize) {
    std::vector<uint8_t> output(uncompressedSize);

    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));
    stream.next_in = const_cast<uint8_t*>(compressedData);
    stream.avail_in = compressedSize;
    stream.next_out = output.data();
    stream.avail_out = uncompressedSize;

    // -MAX_WBITS for raw deflate (no zlib/gzip header)
    int ret = inflateInit2(&stream, -MAX_WBITS);
    if (ret != Z_OK) {
        throw std::runtime_error("GpUnzip: inflateInit2 failed: " + std::to_string(ret));
    }

    ret = ::inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if (ret != Z_STREAM_END) {
        // Try again with auto-detect
        std::memset(&stream, 0, sizeof(stream));
        stream.next_in = const_cast<uint8_t*>(compressedData);
        stream.avail_in = compressedSize;
        stream.next_out = output.data();
        stream.avail_out = uncompressedSize;
        ret = inflateInit2(&stream, 15 + 32); // auto-detect
        if (ret == Z_OK) {
            ret = ::inflate(&stream, Z_FINISH);
            inflateEnd(&stream);
        }
    }

    output.resize(stream.total_out);
    return output;
}
