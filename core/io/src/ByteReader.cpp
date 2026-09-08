#include <io/ByteReader.hpp>
#include <cstring>
#include <stdexcept>

namespace Io {

std::uint16_t ByteReader::ReadU16(const std::vector<std::uint8_t>& buf, std::size_t offset) const {
    if (offset + 2 > buf.size())
        throw std::out_of_range("ByteReader::ReadU16 out of bounds");
    std::uint16_t v;
    std::memcpy(&v, buf.data() + offset, 2);
    return v;
}

std::uint32_t ByteReader::ReadU32(const std::vector<std::uint8_t>& buf, std::size_t offset) const {
    if (offset + 4 > buf.size())
        throw std::out_of_range("ByteReader::ReadU32 out of bounds");
    std::uint32_t v;
    std::memcpy(&v, buf.data() + offset, 4);
    return v;
}

std::uint64_t ByteReader::ReadU64(const std::vector<std::uint8_t>& buf, std::size_t offset) const {
    if (offset + 8 > buf.size())
        throw std::out_of_range("ByteReader::ReadU64 out of bounds");
    std::uint64_t v;
    std::memcpy(&v, buf.data() + offset, 8);
    return v;
}

}
