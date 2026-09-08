#ifndef IO_BYTEREADER_HPP
#define IO_BYTEREADER_HPP

#include <io/IByteReader.hpp>

namespace Io {

class ByteReader : public IByteReader {
public:
    std::uint16_t ReadU16(const std::vector<std::uint8_t>& buf, std::size_t offset) const override;
    std::uint32_t ReadU32(const std::vector<std::uint8_t>& buf, std::size_t offset) const override;
    std::uint64_t ReadU64(const std::vector<std::uint8_t>& buf, std::size_t offset) const override;
};

}

#endif
