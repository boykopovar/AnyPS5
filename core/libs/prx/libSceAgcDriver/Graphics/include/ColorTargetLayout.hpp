#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COLORTARGETLAYOUT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_COLORTARGETLAYOUT_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace AgcDriver::Graphics {

struct TextureSwizzleEquation;

enum class ColorTileMode : std::uint32_t {
    Linear = 0,
    Standard4KB = 5,
    Standard64KB = 9,
    S4KBX = 0x15,
    D4KBX = 0x16,
    Z64KBX = 0x18,
    D64KBX = 0x1a,
    RenderTarget = 0x1b,
    GcnThin1D = 0x100,
    GcnDisplay1D = 0x101,
    GcnDisplay2D = 0x102,
    GcnThin2D = 0x103,
    GcnLinear = 0x104
};

constexpr bool ColorTileModeIsGcn(ColorTileMode mode) {
    return mode == ColorTileMode::GcnThin1D || mode == ColorTileMode::GcnDisplay1D || mode == ColorTileMode::GcnDisplay2D || mode == ColorTileMode::GcnThin2D || mode == ColorTileMode::GcnLinear;
}

constexpr bool ColorTileModeIsLinear(ColorTileMode mode) {
    return mode == ColorTileMode::Linear || mode == ColorTileMode::GcnLinear;
}

constexpr bool ColorTileModeIsXor(ColorTileMode mode) {
    return mode == ColorTileMode::S4KBX || mode == ColorTileMode::D4KBX || mode == ColorTileMode::Z64KBX || mode == ColorTileMode::D64KBX || mode == ColorTileMode::RenderTarget;
}

constexpr std::uint32_t ColorTileModeBlockBytes(ColorTileMode mode) {
    return ColorTileModeIsLinear(mode) ? 256u : mode == ColorTileMode::Standard4KB || mode == ColorTileMode::S4KBX || mode == ColorTileMode::D4KBX ? 4096u : 65536u;
}

constexpr TextureTileMode ColorTextureTileMode(ColorTileMode mode) {
    if (mode == ColorTileMode::GcnThin1D) return TextureTileMode::kGcnThin1D;
    if (mode == ColorTileMode::GcnDisplay1D) return TextureTileMode::kGcnDisplay1D;
    if (mode == ColorTileMode::GcnDisplay2D) return TextureTileMode::kGcnDisplay2D;
    if (mode == ColorTileMode::GcnThin2D) return TextureTileMode::kGcnThin2D;
    return ColorTileModeIsLinear(mode) ? TextureTileMode::kLinear : mode == ColorTileMode::Standard4KB ? TextureTileMode::kStandard4KB : mode == ColorTileMode::Standard64KB ? TextureTileMode::kStandard64KB : mode == ColorTileMode::S4KBX ? TextureTileMode::kS4KBX : mode == ColorTileMode::D4KBX ? TextureTileMode::kD4KBX : mode == ColorTileMode::D64KBX ? TextureTileMode::kD64KBX : mode == ColorTileMode::Z64KBX ? TextureTileMode::kZ64KBX : TextureTileMode::kR64KBX;
}

ColorTileMode DecodeColorTileMode(std::uint32_t attrib3);
TileMipLayout ComputeColorTargetMipLayout(ColorTileMode mode, std::uint32_t bytesPerElement, std::uint32_t surfaceWidth, std::uint32_t surfaceHeight, std::uint32_t mipCount, std::uint32_t mipLevel);

class ColorTargetLayout {
public:
    ColorTargetLayout(std::uint32_t width, std::uint32_t height, ColorTileMode mode, std::uint32_t bytesPerElement = 4, std::uint32_t pipeBankXor = 0);
    std::size_t Bytes() const { return bytes; }
    std::size_t LinearBytes() const { return static_cast<std::size_t>(width) * height * elementBytes; }
    std::size_t Alignment() const { return blockBytes; }
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
    std::size_t blockBytes = 0;
    std::uint32_t pipeBankXor = 0;
    bool gcn = false;
    std::array<std::uint32_t, 16> gcnEquation{};
    // The XOR swizzle is linear over GF(2), so a block offset is xOffsets[x] ^ yOffsets[y].
    const std::uint32_t* xOffsets = nullptr;
    const std::uint32_t* yOffsets = nullptr;
    const TextureSwizzleEquation* equation = nullptr;
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
