#ifndef IO_PNGVALIDATION_HPP
#define IO_PNGVALIDATION_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

namespace Io::Png {

inline std::uint32_t ReadBigEndian(std::span<const std::uint8_t> bytes, const std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) << 24 | static_cast<std::uint32_t>(bytes[offset + 1]) << 16 |
           static_cast<std::uint32_t>(bytes[offset + 2]) << 8 | bytes[offset + 3];
}

inline void Validate(std::span<const std::uint8_t> bytes) {
    const auto fail = [](const std::string& reason) { throw std::invalid_argument(reason); };
    constexpr std::array<std::uint8_t, 8> signature{0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    if (bytes.size() < 33 || !std::equal(signature.begin(), signature.end(), bytes.begin())) fail("missing PNG signature or IHDR");
    if (ReadBigEndian(bytes, 8) != 13 || ReadBigEndian(bytes, 12) != 0x49484452) fail("invalid IHDR size or type");
    const auto width = ReadBigEndian(bytes, 16);
    const auto height = ReadBigEndian(bytes, 20);
    if (width == 0 || height == 0 || width > 0x7fffffffu || height > 0x7fffffffu) fail("invalid IHDR dimensions");
    const auto depth = bytes[24];
    const auto color = bytes[25];
    const bool validDepth = (color == 0 && (depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16)) ||
                            (color == 3 && (depth == 1 || depth == 2 || depth == 4 || depth == 8)) ||
                            ((color == 2 || color == 4 || color == 6) && (depth == 8 || depth == 16));
    if (!validDepth || bytes[26] != 0 || bytes[27] != 0 || bytes[28] > 1) fail("invalid IHDR encoding");
    bool hasData = false;
    bool dataStarted = false;
    bool dataEnded = false;
    bool hasPalette = false;
    for (std::size_t offset = 8; offset < bytes.size();) {
        if (bytes.size() - offset < 12) fail("truncated chunk");
        const auto length = ReadBigEndian(bytes, offset);
        if (length > bytes.size() - offset - 12) fail("truncated chunk data");
        const auto type = ReadBigEndian(bytes, offset + 4);
        for (std::size_t index = offset + 4; index < offset + 8; ++index)
            if (!((bytes[index] >= 'A' && bytes[index] <= 'Z') || (bytes[index] >= 'a' && bytes[index] <= 'z')))
                fail("invalid chunk type");
        std::uint32_t crc = 0xffffffffu;
        for (std::size_t index = offset + 4; index < offset + 8 + length; ++index) {
            crc ^= bytes[index];
            for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
        }
        if ((crc ^ 0xffffffffu) != ReadBigEndian(bytes, offset + 8 + length)) fail("invalid chunk CRC");
        if (type == 0x49484452 && offset != 8) fail("duplicate IHDR");
        if (type == 0x504c5445) {
            if (hasPalette || dataStarted || !length || length % 3 || length > 768 || color == 0 || color == 4 ||
                (color == 3 && length / 3 > (1u << depth))) fail("invalid PLTE");
            hasPalette = true;
        }
        if (type == 0x49444154) {
            if (dataEnded || (color == 3 && !hasPalette)) fail("invalid IDAT order or missing PLTE");
            dataStarted = true;
            if (length != 0) hasData = true;
        } else {
            if (dataStarted) dataEnded = true;
            if (!(type & 0x20000000u) && type != 0x49484452 && type != 0x504c5445 && type != 0x49454e44)
                fail("unknown critical chunk");
        }
        offset += static_cast<std::size_t>(length) + 12;
        if (type == 0x49454e44) {
            if (length != 0 || offset != bytes.size() || !hasData) fail("invalid IEND or missing IDAT");
            return;
        }
    }
    fail("missing IEND");
}

}

#endif
