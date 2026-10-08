#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COLORTARGETLAYOUT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COLORTARGETLAYOUT_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace AgcDriver::Graphics {

enum class ColorTileMode : std::uint32_t {
    Linear = 0,
    Standard4KB = 5,
    Standard64KB = 9,
    RenderTarget = 0x1b
};

constexpr TextureTileMode ColorTextureTileMode(ColorTileMode mode) {
    return mode == ColorTileMode::Linear ? TextureTileMode::kLinear : mode == ColorTileMode::Standard4KB ? TextureTileMode::kStandard4KB : mode == ColorTileMode::Standard64KB ? TextureTileMode::kStandard64KB : TextureTileMode::kR64KBX;
}

ColorTileMode DecodeColorTileMode(std::uint32_t attrib3);

class ColorTargetLayout {
public:
    ColorTargetLayout(std::uint32_t width, std::uint32_t height, ColorTileMode mode, std::uint32_t bytesPerElement = 4, std::uint32_t pipeBankXor = 0);
    std::size_t Bytes() const { return bytes; }
    std::size_t LinearBytes() const { return static_cast<std::size_t>(width) * height * elementBytes; }
    std::size_t Alignment() const { return mode == ColorTileMode::Linear ? 256u : mode == ColorTileMode::Standard4KB ? 4096u : 65536u; }
    std::uint32_t BlocksPerRow() const { return pitch / blockWidth; }
    std::size_t Offset(std::uint32_t x, std::uint32_t y) const;
    void Detile(std::span<const std::byte> source, std::span<std::byte> destination) const;
    void Tile(std::span<const std::byte> source, std::span<std::byte> destination) const;

private:
    std::size_t offset(std::uint32_t x, std::uint32_t y) const;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t pitch;
    ColorTileMode mode;
    std::size_t bytes;
    std::uint32_t elementBytes;
    std::uint32_t blockWidth = 1;
    std::uint32_t blockHeight = 1;
    std::uint32_t pipeBankXor = 0;
    // The XOR swizzle is linear over GF(2), so a block offset is xOffsets[x] ^ yOffsets[y].
    const std::uint32_t* xOffsets = nullptr;
    const std::uint32_t* yOffsets = nullptr;
};

class CmaskLayout {
public:
    static constexpr std::size_t Alignment = 4096;
    CmaskLayout(std::uint32_t width, std::uint32_t height);
    std::size_t Bytes() const { return bytes; }
    std::uint32_t BlocksPerRow() const { return blocksPerRow; }
    std::uint32_t TilesX() const { return (width + 7u) / 8u; }
    std::uint32_t TilesY() const { return (height + 7u) / 8u; }
    std::size_t Nibble(std::uint32_t tileX, std::uint32_t tileY) const;

private:
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t blocksPerRow;
    std::size_t bytes;
};

}

#endif
