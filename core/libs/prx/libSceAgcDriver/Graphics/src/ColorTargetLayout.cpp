#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace AgcDriver::Graphics {
namespace {

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::uint32_t parity(std::uint32_t value) {
    return static_cast<std::uint32_t>(std::popcount(value)) & 1u;
}

}

ColorTileMode DecodeColorTileMode(std::uint32_t attrib3) {
    require((attrib3 & 0x80002000u) == 0 && (attrib3 & 0x1fffu) == 0 && ((attrib3 >> 24u) & 3u) == 1 && ((attrib3 >> 27u) & 7u) == 1, "AGC graphics: unsupported color depth, dimension, resource level or metadata mode");
    const auto mode = (attrib3 >> 14u) & 0x1fu;
    const auto fmaskMode = (attrib3 >> 19u) & 0x1fu;
    require(fmaskMode == 0 || fmaskMode == 0x18, "AGC graphics: unsupported color FMASK swizzle mode");
    require(mode == 0 || mode == 0x1b, "AGC graphics: unsupported color tile mode");
    return static_cast<ColorTileMode>(mode);
}

ColorTargetLayout::ColorTargetLayout(std::uint32_t width, std::uint32_t height, ColorTileMode mode, std::uint32_t bytesPerElement) : width(width), height(height), pitch(width), mode(mode), bytes(0), elementBytes(bytesPerElement) {
    require(width != 0 && height != 0 && width <= 16384 && height <= 16384, "AGC graphics: invalid color surface extent");
    require(std::has_single_bit(bytesPerElement) && bytesPerElement <= 16u, "AGC graphics: unsupported color element size");
    std::uint32_t paddedHeight = height;
    switch (mode) {
        case ColorTileMode::Linear:
            require(width * bytesPerElement % 256u == 0, "AGC graphics: linear surface pitch requires a width aligned to 256 bytes");
            break;
        case ColorTileMode::RenderTarget: {
            // SW_64KB_R_X: 64 KiB blocks of 2^(16 - log2(bpe)) elements, wider than tall for odd powers.
            const auto log2Elements = 16u - static_cast<std::uint32_t>(std::countr_zero(bytesPerElement));
            blockWidth = 1u << ((log2Elements + 1u) / 2u);
            blockHeight = 1u << (log2Elements / 2u);
            pitch = (width + blockWidth - 1u) / blockWidth * blockWidth;
            paddedHeight = (height + blockHeight - 1u) / blockHeight * blockHeight;
            const auto* equation = FindTextureSwizzleEquation(27u, bytesPerElement);
            require(equation != nullptr, "AGC graphics: no SW_64KB_R_X equation for the color element size");
            xOffsets.resize(blockWidth);
            yOffsets.resize(blockHeight);
            for (std::uint32_t x = 0; x < blockWidth; ++x) {
                std::uint32_t offset = 0;
                for (std::uint32_t bit = 0; bit < 16u; ++bit) offset |= parity(x & equation->bits[bit] & 0xfffu) << bit;
                xOffsets[x] = offset;
            }
            for (std::uint32_t y = 0; y < blockHeight; ++y) {
                std::uint32_t offset = 0;
                for (std::uint32_t bit = 0; bit < 16u; ++bit) offset |= parity((y << 12u) & equation->bits[bit] & 0xfff000u) << bit;
                yOffsets[y] = offset;
            }
            break;
        }
        default: throw std::runtime_error("AGC graphics: unsupported color tile mode");
    }
    const auto size = static_cast<std::uint64_t>(pitch) * paddedHeight * bytesPerElement;
    require(size <= std::numeric_limits<std::size_t>::max(), "AGC graphics: color surface size overflow");
    bytes = static_cast<std::size_t>(size);
}

std::size_t ColorTargetLayout::offset(std::uint32_t x, std::uint32_t y) const {
    if (mode == ColorTileMode::Linear) return (static_cast<std::size_t>(y) * pitch + x) * elementBytes;
    const auto block = static_cast<std::size_t>(y / blockHeight) * (pitch / blockWidth) + x / blockWidth;
    return block * 65536u + (xOffsets[x % blockWidth] ^ yOffsets[y % blockHeight]);
}

std::size_t ColorTargetLayout::Offset(std::uint32_t x, std::uint32_t y) const {
    require(x < width && y < height, "AGC graphics: color surface coordinate out of range");
    return offset(x, y);
}

void ColorTargetLayout::Detile(std::span<const std::byte> source, std::span<std::byte> destination) const {
    require(source.size() == Bytes() && destination.size() == LinearBytes(), "AGC graphics: color detile buffer size mismatch");
    for (std::uint32_t y = 0; y < height; ++y) {
        auto* row = destination.data() + static_cast<std::size_t>(y) * width * elementBytes;
        for (std::uint32_t x = 0; x < width; ++x) std::memcpy(row + static_cast<std::size_t>(x) * elementBytes, source.data() + offset(x, y), elementBytes);
    }
}

void ColorTargetLayout::Tile(std::span<const std::byte> source, std::span<std::byte> destination) const {
    require(source.size() == LinearBytes() && destination.size() == Bytes(), "AGC graphics: color tile buffer size mismatch");
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto* row = source.data() + static_cast<std::size_t>(y) * width * elementBytes;
        for (std::uint32_t x = 0; x < width; ++x) std::memcpy(destination.data() + offset(x, y), row + static_cast<std::size_t>(x) * elementBytes, elementBytes);
    }
}

}
