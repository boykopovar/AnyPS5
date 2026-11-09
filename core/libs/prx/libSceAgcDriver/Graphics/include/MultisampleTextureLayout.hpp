#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_MULTISAMPLETEXTURELAYOUT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_MULTISAMPLETEXTURELAYOUT_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace AgcDriver::Graphics {

class MultisampleTextureLayout {
public:
    static constexpr std::uint32_t Alignment = 65536u;

    MultisampleTextureLayout(TextureTileMode tileMode, std::uint32_t elementBytes, std::uint32_t fragments, std::uint32_t width, std::uint32_t height, std::uint32_t layers, std::uint32_t pipeBankXor = 0);
    std::uint64_t Bytes() const { return sliceBytes * layers; }
    std::uint64_t SliceBytes() const { return sliceBytes; }
    std::uint64_t LinearBytes() const { return static_cast<std::uint64_t>(width) * height * layers * fragments * elementBytes; }
    std::uint32_t Pitch() const { return pitch; }
    std::uint32_t PaddedHeight() const { return paddedHeight; }
    std::uint32_t BlockWidth() const { return blockWidth; }
    std::uint32_t BlockHeight() const { return blockHeight; }
    const std::array<std::uint32_t, 16>& InverseBlockEquation() const { return inverseBlockEquation; }
    std::uint64_t Offset(std::uint32_t x, std::uint32_t y, std::uint32_t layer, std::uint32_t fragment) const;
    void Detile(std::span<const std::byte> source, std::span<std::byte> destination) const;
    void Tile(std::span<const std::byte> source, std::span<std::byte> destination) const;

private:
    std::uint64_t offset(std::uint32_t x, std::uint32_t y, std::uint32_t layer, std::uint32_t fragment) const;
    void transfer(std::span<const std::byte> source, std::span<std::byte> destination, bool retile) const;

    std::uint32_t elementBytes;
    std::uint32_t fragments;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t layers;
    std::uint32_t pipeBankXor;
    std::uint32_t blockWidth;
    std::uint32_t blockHeight;
    std::uint32_t pitch;
    std::uint32_t paddedHeight;
    std::uint64_t sliceBytes;
    std::array<std::uint32_t, 16> inverseBlockEquation;
    std::vector<std::uint32_t> xOffsets;
    std::vector<std::uint32_t> yOffsets;
    std::vector<std::uint32_t> layerOffsets;
    std::vector<std::uint32_t> fragmentOffsets;
};

}

#endif
