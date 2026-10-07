#ifndef PKG_BYTEORDER_HPP
#define PKG_BYTEORDER_HPP

#include <cstddef>
#include <cstdint>

namespace Pkg {

inline std::uint64_t LoadLittle(const std::uint8_t* bytes, const std::size_t size) {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < size; ++index) value |= static_cast<std::uint64_t>(bytes[index]) << (8 * index);
    return value;
}

inline std::uint16_t LoadLittle16(const std::uint8_t* bytes) { return static_cast<std::uint16_t>(LoadLittle(bytes, 2)); }
inline std::uint32_t LoadLittle32(const std::uint8_t* bytes) { return static_cast<std::uint32_t>(LoadLittle(bytes, 4)); }
inline std::uint64_t LoadLittle64(const std::uint8_t* bytes) { return LoadLittle(bytes, 8); }

inline std::uint32_t LoadBig32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) << 24 | static_cast<std::uint32_t>(bytes[1]) << 16 | static_cast<std::uint32_t>(bytes[2]) << 8 | bytes[3];
}

}

#endif
