#ifndef IO_BYTEWRITER_HPP
#define IO_BYTEWRITER_HPP

#include <io/IByteWriter.hpp>

namespace Io {

class ByteWriter : public IByteWriter {
public:
    void WriteU16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t v) const override;
    void WriteU32(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint32_t v) const override;
    void WriteU64(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint64_t v) const override;
    void AppendU64(std::vector<std::uint8_t>& buf, std::uint64_t v) const override;
    void AppendI64(std::vector<std::uint8_t>& buf, std::int64_t v) const override;
};

}

#endif
