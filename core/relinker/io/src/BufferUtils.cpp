#include <io/BufferUtils.hpp>
#include <cstring>
#include <stdexcept>

namespace Io {

void WriteU8(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint8_t v) {
    if (offset >= buf.size())
        throw std::out_of_range("WriteU8 out of bounds");
    buf[offset] = v;
}

void WriteU16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t v) {
    if (offset + 2 > buf.size())
        throw std::out_of_range("WriteU16 out of bounds");
    std::memcpy(buf.data() + offset, &v, 2);
}

void WriteU32(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint32_t v) {
    if (offset + 4 > buf.size())
        throw std::out_of_range("WriteU32 out of bounds");
    std::memcpy(buf.data() + offset, &v, 4);
}

void WriteU64(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint64_t v) {
    if (offset + 8 > buf.size())
        throw std::out_of_range("WriteU64 out of bounds");
    std::memcpy(buf.data() + offset, &v, 8);
}

void AppendU8(std::vector<std::uint8_t>& buf, std::uint8_t v) {
    buf.push_back(v);
}

void AppendU16(std::vector<std::uint8_t>& buf, std::uint16_t v) {
    const std::size_t pos = buf.size();
    buf.resize(pos + 2);
    std::memcpy(buf.data() + pos, &v, 2);
}

void AppendU32(std::vector<std::uint8_t>& buf, std::uint32_t v) {
    const std::size_t pos = buf.size();
    buf.resize(pos + 4);
    std::memcpy(buf.data() + pos, &v, 4);
}

void AppendU64(std::vector<std::uint8_t>& buf, std::uint64_t v) {
    const std::size_t pos = buf.size();
    buf.resize(pos + 8);
    std::memcpy(buf.data() + pos, &v, 8);
}

void AppendI64(std::vector<std::uint8_t>& buf, std::int64_t v) {
    AppendU64(buf, static_cast<std::uint64_t>(v));
}

void AppendString(std::vector<std::uint8_t>& buf, const std::string& str) {
    buf.insert(buf.end(), str.begin(), str.end());
    buf.push_back(0);
}

void AlignBuffer(std::vector<std::uint8_t>& buf, std::size_t alignment) {
    const std::size_t remainder = buf.size() % alignment;
    if (remainder != 0) {
        const std::size_t padding = alignment - remainder;
        buf.resize(buf.size() + padding, 0);
    }
}

std::uint64_t AlignUp64(std::uint64_t value, std::uint64_t alignment) {
    return AlignUp(value, alignment);
}

std::uint16_t ReadU16(const std::vector<std::uint8_t>& buf, std::size_t offset) {
    if (offset + 2 > buf.size())
        throw std::out_of_range("ReadU16 out of bounds");
    std::uint16_t v;
    std::memcpy(&v, buf.data() + offset, 2);
    return v;
}

std::uint32_t ReadU32(const std::vector<std::uint8_t>& buf, std::size_t offset) {
    if (offset + 4 > buf.size())
        throw std::out_of_range("ReadU32 out of bounds");
    std::uint32_t v;
    std::memcpy(&v, buf.data() + offset, 4);
    return v;
}

std::uint64_t ReadU64(const std::vector<std::uint8_t>& buf, std::size_t offset) {
    if (offset + 8 > buf.size())
        throw std::out_of_range("ReadU64 out of bounds");
    std::uint64_t v;
    std::memcpy(&v, buf.data() + offset, 8);
    return v;
}

}
