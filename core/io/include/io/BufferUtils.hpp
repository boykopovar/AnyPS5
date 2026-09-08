#ifndef IO_BUFFERUTILS_HPP
#define IO_BUFFERUTILS_HPP

#include <cstdint>
#include <vector>
#include <string>

namespace Io {

void WriteU8(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint8_t v);
void WriteU16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t v);
void WriteU32(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint32_t v);
void WriteU64(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint64_t v);

void AppendU8(std::vector<std::uint8_t>& buf, std::uint8_t v);
void AppendU16(std::vector<std::uint8_t>& buf, std::uint16_t v);
void AppendU32(std::vector<std::uint8_t>& buf, std::uint32_t v);
void AppendU64(std::vector<std::uint8_t>& buf, std::uint64_t v);
void AppendI64(std::vector<std::uint8_t>& buf, std::int64_t v);

void AppendString(std::vector<std::uint8_t>& buf, const std::string& str);
void AlignBuffer(std::vector<std::uint8_t>& buf, std::size_t alignment);

std::uint16_t ReadU16(const std::vector<std::uint8_t>& buf, std::size_t offset);
std::uint32_t ReadU32(const std::vector<std::uint8_t>& buf, std::size_t offset);
std::uint64_t ReadU64(const std::vector<std::uint8_t>& buf, std::size_t offset);

template<typename T>
constexpr T AlignUp(T value, T alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

std::uint64_t AlignUp64(std::uint64_t value, std::uint64_t alignment);

}

#endif
