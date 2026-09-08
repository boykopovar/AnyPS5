#ifndef IO_IBYTEREADER_HPP
#define IO_IBYTEREADER_HPP

#include <cstdint>
#include <vector>

namespace Io {

class IByteReader {
public:
    virtual ~IByteReader() = default;

    virtual std::uint16_t ReadU16(const std::vector<std::uint8_t>& buf, std::size_t offset) const = 0;
    virtual std::uint32_t ReadU32(const std::vector<std::uint8_t>& buf, std::size_t offset) const = 0;
    virtual std::uint64_t ReadU64(const std::vector<std::uint8_t>& buf, std::size_t offset) const = 0;
};

}

#endif

