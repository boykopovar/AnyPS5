#include <io/ByteWriter.hpp>
#include <cstring>

namespace Io {

void ByteWriter::WriteU16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t v) const {
    std::memcpy(buf.data() + offset, &v, 2);
}

void ByteWriter::WriteU32(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint32_t v) const {
    std::memcpy(buf.data() + offset, &v, 4);
}

void ByteWriter::WriteU64(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint64_t v) const {
    std::memcpy(buf.data() + offset, &v, 8);
}

void ByteWriter::AppendU64(std::vector<std::uint8_t>& buf, std::uint64_t v) const {
    const std::size_t pos = buf.size();
    buf.resize(pos + 8);
    std::memcpy(buf.data() + pos, &v, 8);
}

void ByteWriter::AppendI64(std::vector<std::uint8_t>& buf, std::int64_t v) const {
    AppendU64(buf, static_cast<std::uint64_t>(v));
}

}
