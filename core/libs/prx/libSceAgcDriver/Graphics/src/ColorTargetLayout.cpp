#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace AgcDriver::Graphics {
namespace {

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::uint32_t parity(std::uint32_t value) {
    return static_cast<std::uint32_t>(std::popcount(value)) & 1u;
}

std::uint32_t standardOffset(std::uint32_t x, std::uint32_t y, std::uint32_t elementBytes) {
    switch (elementBytes) {
        case 1u: return ((y << 4) & 0x1f0u) ^ ((y << 5) & 0x400u) ^ (x & 0x00fu) ^ ((x << 5) & 0x200u) ^ ((x << 6) & 0x800u);
        case 2u: return ((y << 4) & 0x070u) ^ ((y << 5) & 0x100u) ^ ((y << 6) & 0x400u) ^ ((x << 1) & 0x00eu) ^ ((x << 4) & 0x080u) ^ ((x << 5) & 0x200u) ^ ((x << 6) & 0x800u);
        case 4u: return ((y << 4) & 0x070u) ^ ((y << 5) & 0x100u) ^ ((y << 6) & 0x400u) ^ ((x << 2) & 0x00cu) ^ ((x << 5) & 0x080u) ^ ((x << 6) & 0x200u) ^ ((x << 7) & 0x800u);
        case 8u: return ((y << 4) & 0x030u) ^ ((y << 6) & 0x100u) ^ ((y << 7) & 0x400u) ^ ((x << 3) & 0x008u) ^ ((x << 5) & 0x0c0u) ^ ((x << 6) & 0x200u) ^ ((x << 7) & 0x800u);
        default: return ((y << 4) & 0x030u) ^ ((y << 6) & 0x100u) ^ ((y << 7) & 0x400u) ^ ((x << 6) & 0x0c0u) ^ ((x << 7) & 0x200u) ^ ((x << 8) & 0x800u);
    }
}

std::uint32_t standard64Extra(std::uint32_t x, std::uint32_t y, std::uint32_t elementBytes) {
    switch (elementBytes) {
        case 1u: return ((x << 7) & 0x2000u) ^ ((x << 8) & 0x8000u) ^ ((y << 6) & 0x1000u) ^ ((y << 7) & 0x4000u);
        case 2u: return ((x << 7) & 0x2000u) ^ ((x << 8) & 0x8000u) ^ ((y << 7) & 0x1000u) ^ ((y << 8) & 0x4000u);
        case 4u: return ((x << 8) & 0x2000u) ^ ((x << 9) & 0x8000u) ^ ((y << 7) & 0x1000u) ^ ((y << 8) & 0x4000u);
        case 8u: return ((x << 8) & 0x2000u) ^ ((x << 9) & 0x8000u) ^ ((y << 8) & 0x1000u) ^ ((y << 9) & 0x4000u);
        default: return ((x << 9) & 0x2000u) ^ ((x << 10) & 0x8000u) ^ ((y << 8) & 0x1000u) ^ ((y << 9) & 0x4000u);
    }
}

}

ColorTileMode DecodeColorTileMode(std::uint32_t attrib3) {
    const auto resourceType = (attrib3 >> 24u) & 3u;
    require((attrib3 & 0x80002000u) == 0 && ((resourceType == 1 && (attrib3 & 0x1fffu) == 0) || resourceType == 2) && ((attrib3 >> 27u) & 7u) == 1, "AGC graphics: unsupported color depth, dimension, resource level or metadata mode");
    const auto mode = (attrib3 >> 14u) & 0x1fu;
    const auto fmaskMode = (attrib3 >> 19u) & 0x1fu;
    require(fmaskMode == 0 || fmaskMode == 0x18, "AGC graphics: unsupported color FMASK swizzle mode");
    require(mode == 0 || mode == 5 || mode == 9 || mode == 0x1b, "AGC graphics: unsupported color tile mode");
    return static_cast<ColorTileMode>(mode);
}

namespace {

struct SwizzleTables {
    std::vector<std::uint32_t> x;
    std::vector<std::uint32_t> y;
};

const SwizzleTables& renderTargetTables(std::uint32_t bytesPerElement, std::uint32_t blockWidth, std::uint32_t blockHeight) {
    static std::once_flag once[5];
    static SwizzleTables tables[5];
    const auto index = static_cast<std::size_t>(std::countr_zero(bytesPerElement));
    std::call_once(once[index], [&] {
        const auto* equation = FindTextureSwizzleEquation(27u, bytesPerElement);
        require(equation != nullptr, "AGC graphics: no SW_64KB_R_X equation for the color element size");
        auto& table = tables[index];
        table.x.resize(blockWidth);
        table.y.resize(blockHeight);
        for (std::uint32_t x = 0; x < blockWidth; ++x) {
            std::uint32_t offset = 0;
            for (std::uint32_t bit = 0; bit < 16u; ++bit) offset |= parity(x & equation->bits[bit] & 0xfffu) << bit;
            table.x[x] = offset;
        }
        for (std::uint32_t y = 0; y < blockHeight; ++y) {
            std::uint32_t offset = 0;
            for (std::uint32_t bit = 0; bit < 16u; ++bit) offset |= parity((y << 12u) & equation->bits[bit] & 0xfff000u) << bit;
            table.y[y] = offset;
        }
    });
    return tables[index];
}

const SwizzleTables& standardTables(std::uint32_t bytesPerElement, std::uint32_t blockWidth, std::uint32_t blockHeight, bool block64KB) {
    static std::once_flag once[2][5];
    static SwizzleTables tables[2][5];
    const auto index = static_cast<std::size_t>(std::countr_zero(bytesPerElement));
    std::call_once(once[block64KB][index], [&] {
        auto& table = tables[block64KB][index];
        table.x.resize(blockWidth);
        table.y.resize(blockHeight);
        for (std::uint32_t x = 0; x < blockWidth; ++x) table.x[x] = standardOffset(x, 0, bytesPerElement) ^ (block64KB ? standard64Extra(x, 0, bytesPerElement) : 0u);
        for (std::uint32_t y = 0; y < blockHeight; ++y) table.y[y] = standardOffset(0, y, bytesPerElement) ^ (block64KB ? standard64Extra(0, y, bytesPerElement) : 0u);
    });
    return tables[block64KB][index];
}

}

ColorTargetLayout::ColorTargetLayout(std::uint32_t width, std::uint32_t height, ColorTileMode mode, std::uint32_t bytesPerElement) : width(width), height(height), pitch(width), mode(mode), bytes(0), elementBytes(bytesPerElement) {
    require(width != 0 && height != 0 && width <= 16384 && height <= 16384, "AGC graphics: invalid color surface extent");
    require(std::has_single_bit(bytesPerElement) && bytesPerElement <= 16u, "AGC graphics: unsupported color element size");
    std::uint32_t paddedHeight = height;
    switch (mode) {
        case ColorTileMode::Linear: {
            const auto pitchAlignment = 256u / bytesPerElement;
            pitch = (width + pitchAlignment - 1u) / pitchAlignment * pitchAlignment;
            break;
        }
        case ColorTileMode::RenderTarget: {
            // SW_64KB_R_X: 64 KiB blocks of 2^(16 - log2(bpe)) elements, wider than tall for odd powers.
            const auto log2Elements = 16u - static_cast<std::uint32_t>(std::countr_zero(bytesPerElement));
            blockWidth = 1u << ((log2Elements + 1u) / 2u);
            blockHeight = 1u << (log2Elements / 2u);
            pitch = (width + blockWidth - 1u) / blockWidth * blockWidth;
            paddedHeight = (height + blockHeight - 1u) / blockHeight * blockHeight;
            const auto& tables = renderTargetTables(bytesPerElement, blockWidth, blockHeight);
            xOffsets = tables.x.data();
            yOffsets = tables.y.data();
            break;
        }
        case ColorTileMode::Standard4KB:
        case ColorTileMode::Standard64KB: {
            const auto log2Bytes = static_cast<std::uint32_t>(std::countr_zero(bytesPerElement));
            const bool block64KB = mode == ColorTileMode::Standard64KB;
            const auto log2Side = block64KB ? 8u : 6u;
            blockWidth = 1u << (log2Side - log2Bytes / 2u);
            blockHeight = 1u << (log2Side - (log2Bytes + 1u) / 2u);
            pitch = (width + blockWidth - 1u) / blockWidth * blockWidth;
            paddedHeight = (height + blockHeight - 1u) / blockHeight * blockHeight;
            const auto& tables = standardTables(bytesPerElement, blockWidth, blockHeight, block64KB);
            xOffsets = tables.x.data();
            yOffsets = tables.y.data();
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
    return block * Alignment() + (xOffsets[x % blockWidth] ^ yOffsets[y % blockHeight]);
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
