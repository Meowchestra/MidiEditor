#ifndef GPBINARYREADER_H
#define GPBINARYREADER_H

#include <vector>
#include <string>
#include <cstdint>

// Binary reader utility – ported from GPBase.cs
// Instance-based (no global state) for thread safety

class GpBinaryReader {
public:
    GpBinaryReader() = default;
    explicit GpBinaryReader(const std::vector<uint8_t>& data);

    void setData(const std::vector<uint8_t>& data);
    int getPointer() const { return pointer_; }
    void setPointer(int pos) { pointer_ = pos; }
    size_t dataSize() const { return data_.size(); }
    bool atEnd() const { return pointer_ >= static_cast<int>(data_.size()); }

    // Read primitives (return vector for C# compatibility)
    std::vector<uint8_t>  readByte(int count = 1);
    std::vector<int8_t>   readSignedByte(int count = 1);
    std::vector<bool>     readBool(int count = 1);
    std::vector<int16_t>  readShort(int count = 1);
    std::vector<int32_t>  readInt(int count = 1);
    std::vector<float>    readFloat(int count = 1);
    std::vector<double>   readDouble(int count = 1);

    // Read strings
    std::string readString(int size, int length = -1);
    std::string readByteSizeString(int size);
    std::string readIntByteSizeString();
    std::string readIntSizeString();

    void skip(int count);

private:
    std::vector<uint8_t> data_;
    int pointer_ = 0;

    void checkBounds(int count) const;
};

#endif // GPBINARYREADER_H
