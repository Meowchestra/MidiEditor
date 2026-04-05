#include "GpBinaryReader.h"
#include <stdexcept>
#include <cstring>

GpBinaryReader::GpBinaryReader(const std::vector<uint8_t>& data)
    : data_(data), pointer_(0) {}

void GpBinaryReader::setData(const std::vector<uint8_t>& data) {
    data_ = data;
    pointer_ = 0;
}

void GpBinaryReader::checkBounds(int count) const {
    if (pointer_ + count > static_cast<int>(data_.size())) {
        throw std::runtime_error("GpBinaryReader: read past end of data at offset " +
                                 std::to_string(pointer_) + ", requested " +
                                 std::to_string(count) + " bytes");
    }
}

std::vector<uint8_t> GpBinaryReader::readByte(int count) {
    checkBounds(count);
    std::vector<uint8_t> result(data_.begin() + pointer_, data_.begin() + pointer_ + count);
    pointer_ += count;
    return result;
}

std::vector<int8_t> GpBinaryReader::readSignedByte(int count) {
    checkBounds(count);
    std::vector<int8_t> result(count);
    for (int i = 0; i < count; i++) {
        result[i] = static_cast<int8_t>(data_[pointer_ + i]);
    }
    pointer_ += count;
    return result;
}

std::vector<bool> GpBinaryReader::readBool(int count) {
    checkBounds(count);
    std::vector<bool> result(count);
    for (int i = 0; i < count; i++) {
        result[i] = (data_[pointer_ + i] != 0);
    }
    pointer_ += count;
    return result;
}

std::vector<int16_t> GpBinaryReader::readShort(int count) {
    checkBounds(count * 2);
    std::vector<int16_t> result(count);
    for (int i = 0; i < count; i++) {
        int16_t val;
        std::memcpy(&val, &data_[pointer_], 2);
        result[i] = val; // little-endian on x86
        pointer_ += 2;
    }
    return result;
}

std::vector<int32_t> GpBinaryReader::readInt(int count) {
    checkBounds(count * 4);
    std::vector<int32_t> result(count);
    for (int i = 0; i < count; i++) {
        int32_t val;
        std::memcpy(&val, &data_[pointer_], 4);
        result[i] = val; // little-endian on x86
        pointer_ += 4;
    }
    return result;
}

std::vector<float> GpBinaryReader::readFloat(int count) {
    checkBounds(count * 4);
    std::vector<float> result(count);
    for (int i = 0; i < count; i++) {
        float val;
        std::memcpy(&val, &data_[pointer_], 4);
        result[i] = val;
        pointer_ += 4;
    }
    return result;
}

std::vector<double> GpBinaryReader::readDouble(int count) {
    checkBounds(count * 8);
    std::vector<double> result(count);
    for (int i = 0; i < count; i++) {
        double val;
        std::memcpy(&val, &data_[pointer_], 8);
        result[i] = val;
        pointer_ += 8;
    }
    return result;
}

std::string GpBinaryReader::readString(int size, int length) {
    // Read 'size' bytes, but only use first 'length' characters
    checkBounds(size);
    int actualLength = (length >= 0) ? std::min(length, size) : size;
    std::string result(reinterpret_cast<const char*>(&data_[pointer_]), actualLength);
    pointer_ += size;
    return result;
}

std::string GpBinaryReader::readByteSizeString(int size) {
    // Read a string preceded by a byte indicating its length
    int length = readByte()[0];
    return readString(size, length);
}

std::string GpBinaryReader::readIntByteSizeString() {
    // Read length of string + 1 stored as int32, then byte-prefixed string
    int d = readInt()[0] - 1;
    return readByteSizeString(d);
}

std::string GpBinaryReader::readIntSizeString() {
    // Read a string preceded by an int indicating its length
    int length = readInt()[0];
    return readString(length);
}

void GpBinaryReader::skip(int count) {
    pointer_ += count;
}
