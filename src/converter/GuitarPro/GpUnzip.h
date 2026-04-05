#ifndef GPUNZIP_H
#define GPUNZIP_H

#include <string>
#include <vector>
#include <cstdint>

// Ported from Unzip.cs — extracts files from ZIP archives (GP7/GP8)

class GpUnzip {
public:
    explicit GpUnzip(const std::string& filePath);
    explicit GpUnzip(const std::vector<uint8_t>& data);

    // Extract a file by path, returns its contents
    std::vector<uint8_t> extract(const std::string& entryPath);

    // Check if an entry exists
    bool hasEntry(const std::string& entryPath) const;

private:
    struct ZipEntry {
        std::string filename;
        uint16_t compressionMethod = 0;
        uint32_t compressedSize = 0;
        uint32_t uncompressedSize = 0;
        uint32_t dataOffset = 0;
    };

    std::vector<uint8_t> data_;
    std::vector<ZipEntry> entries_;

    void parseEntries();
    std::vector<uint8_t> inflateData(const uint8_t* compressedData, uint32_t compressedSize, uint32_t uncompressedSize);
};

#endif // GPUNZIP_H
