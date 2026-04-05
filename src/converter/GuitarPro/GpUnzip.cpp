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
    // Find End of Central Directory record (search backwards from end)
    // EOCD signature: PK\x05\x06
    if (data_.size() < 22) return;

    size_t eocdPos = 0;
    bool found = false;
    // EOCD can have a variable-length comment, search backwards (up to 65535+22 bytes)
    size_t searchStart = data_.size() >= 65557 ? data_.size() - 65557 : 0;
    for (size_t i = data_.size() - 22; i >= searchStart; i--) {
        if (data_[i] == 0x50 && data_[i + 1] == 0x4B &&
            data_[i + 2] == 0x05 && data_[i + 3] == 0x06) {
            eocdPos = i;
            found = true;
            break;
        }
        if (i == 0) break;
    }
    if (!found) return;

    // Read central directory offset and size from EOCD
    uint32_t cdSize, cdOffset;
    std::memcpy(&cdSize, &data_[eocdPos + 12], 4);
    std::memcpy(&cdOffset, &data_[eocdPos + 16], 4);

    // Parse central directory entries (signature: PK\x01\x02)
    size_t pos = cdOffset;
    while (pos + 46 <= data_.size()) {
        if (data_[pos] != 0x50 || data_[pos + 1] != 0x4B ||
            data_[pos + 2] != 0x01 || data_[pos + 3] != 0x02) {
            break;
        }

        ZipEntry entry;

        std::memcpy(&entry.compressionMethod, &data_[pos + 10], 2);
        std::memcpy(&entry.compressedSize, &data_[pos + 20], 4);
        std::memcpy(&entry.uncompressedSize, &data_[pos + 24], 4);

        uint16_t filenameLen, extraLen, commentLen;
        std::memcpy(&filenameLen, &data_[pos + 28], 2);
        std::memcpy(&extraLen, &data_[pos + 30], 2);
        std::memcpy(&commentLen, &data_[pos + 32], 2);

        uint32_t localHeaderOffset;
        std::memcpy(&localHeaderOffset, &data_[pos + 42], 4);

        if (pos + 46 + filenameLen > data_.size()) break;
        entry.filename = std::string(reinterpret_cast<const char*>(&data_[pos + 46]), filenameLen);

        // Calculate actual data offset from local file header
        if (localHeaderOffset + 30 <= data_.size()) {
            uint16_t localFilenameLen, localExtraLen;
            std::memcpy(&localFilenameLen, &data_[localHeaderOffset + 26], 2);
            std::memcpy(&localExtraLen, &data_[localHeaderOffset + 28], 2);
            entry.dataOffset = localHeaderOffset + 30 + localFilenameLen + localExtraLen;
        }

        entries_.push_back(entry);

        // Move to next central directory entry
        pos += 46 + filenameLen + extraLen + commentLen;
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
                return inflateData(&data_[entry.dataOffset],
                               entry.compressedSize, entry.uncompressedSize);
            } else {
                throw std::runtime_error("GpUnzip: unsupported compression method: " +
                                         std::to_string(entry.compressionMethod));
            }
        }
    }
    throw std::runtime_error("GpUnzip: entry not found: " + entryPath);
}

std::vector<uint8_t> GpUnzip::inflateData(const uint8_t* compressedData,
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

    ret = inflate(&stream, Z_FINISH);
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
            ret = inflate(&stream, Z_FINISH);
            inflateEnd(&stream);
        }
    }

    output.resize(stream.total_out);
    return output;
}
