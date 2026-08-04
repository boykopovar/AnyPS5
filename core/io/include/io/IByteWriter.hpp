#ifndef IO_IBYTEWRITER_HPP
#define IO_IBYTEWRITER_HPP

#include <cstdint>
#include <vector>

namespace Io {

class IByteWriter {
public:
    virtual ~IByteWriter() = default;

    virtual void WriteU16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t v) const = 0;
    virtual void WriteU32(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint32_t v) const = 0;
    virtual void WriteU64(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint64_t v) const = 0;
    virtual void AppendU64(std::vector<std::uint8_t>& buf, std::uint64_t v) const = 0;
    virtual void AppendI64(std::vector<std::uint8_t>& buf, std::int64_t v) const = 0;
};

}

#endif
