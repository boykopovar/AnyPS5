#include "prx/libSceAgcDriver/Graphics/include/MultisampleTextureLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include <utility>

namespace AgcDriver::Graphics {
namespace {

std::array<std::uint32_t, 16> inverseBlock(const MultisampleSwizzleEquation& equation, std::uint32_t byteBits, std::uint32_t fragmentBits, std::uint32_t widthBits) {
    std::array<std::uint32_t, 16> rows{}, inverse{};
    for (std::uint32_t input = 0; input < 16u; ++input) {
        std::uint32_t address = 0u;
        if (input < byteBits) address = 1u << input;
        else {
            const auto coordinateBit = input < byteBits + fragmentBits ? 48u + input - byteBits : input < byteBits + fragmentBits + widthBits ? input - byteBits - fragmentBits : 16u + input - byteBits - fragmentBits - widthBits;
            const auto coordinate = std::uint64_t{1} << coordinateBit;
            for (std::uint32_t output = 0; output < 16u; ++output) address |= static_cast<std::uint32_t>(std::popcount(coordinate & equation.bits[output]) & 1u) << output;
        }
        for (std::uint32_t output = 0; output < 16u; ++output) rows[output] |= ((address >> output) & 1u) << input;
        inverse[input] = 1u << input;
    }
    for (std::uint32_t input = 0; input < 16u; ++input) {
        auto pivot = input;
        while (pivot < 16u && (rows[pivot] & (1u << input)) == 0u) ++pivot;
        Require(pivot < 16u, "multisample block address equation is not invertible");
        std::swap(rows[input], rows[pivot]);
        std::swap(inverse[input], inverse[pivot]);
        for (std::uint32_t row = 0; row < 16u; ++row) {
            if (row == input || (rows[row] & (1u << input)) == 0u) continue;
            rows[row] ^= rows[input];
            inverse[row] ^= inverse[input];
        }
    }
    return inverse;
}

std::vector<std::uint32_t> axisOffsets(const MultisampleSwizzleEquation& equation, std::uint32_t count, std::uint32_t shift) {
    std::vector<std::uint32_t> offsets(count);
    for (std::uint32_t value = 0; value < count; ++value) {
        const auto coordinate = static_cast<std::uint64_t>(value) << shift;
        for (std::uint32_t bit = 0; bit < equation.bits.size(); ++bit) {
            offsets[value] |= static_cast<std::uint32_t>(std::popcount(coordinate & equation.bits[bit]) & 1u) << bit;
        }
    }
    return offsets;
}

}

MultisampleTextureLayout::MultisampleTextureLayout(TextureTileMode tileMode, std::uint32_t elementBytes, std::uint32_t fragments, std::uint32_t width, std::uint32_t height, std::uint32_t layers, std::uint32_t pipeBankXor) : elementBytes(elementBytes), fragments(fragments), width(width), height(height), layers(layers), pipeBankXor(pipeBankXor) {
    Require(width != 0 && height != 0 && width <= 16384u && height <= 16384u && layers != 0 && layers <= 8192u, "invalid multisampled surface extent or layer count");
    Require(std::has_single_bit(elementBytes) && elementBytes <= 16u, "unsupported multisampled surface element size");
    Require(fragments == 2u || fragments == 4u || fragments == 8u, "unsupported multisampled surface fragment count");
    Require((pipeBankXor & ~0xcf00u) == 0, "multisampled surface pipe/bank XOR uses bits outside its 16-pipe swizzle");
    Require(tileMode != TextureTileMode::kZ64KBX || elementBytes <= 4u, "multisampled Z surface element size exceeds 32 bits");
    const auto* equation = FindMultisampleSwizzleEquation(tileMode, elementBytes, fragments);
    Require(equation != nullptr, "unsupported multisampled surface tile mode");

    const auto fragmentBits = static_cast<std::uint32_t>(std::countr_zero(fragments));
    const auto elementBits = 16u - static_cast<std::uint32_t>(std::countr_zero(elementBytes)) - fragmentBits;
    const auto widthBits = (elementBits + ((fragmentBits & 1u) == 0 ? 1u : 0u)) / 2u;
    blockWidth = 1u << widthBits;
    blockHeight = 1u << (elementBits - widthBits);
    inverseBlockEquation = inverseBlock(*equation, static_cast<std::uint32_t>(std::countr_zero(elementBytes)), fragmentBits, widthBits);
    pitch = (width + blockWidth - 1u) / blockWidth * blockWidth;
    paddedHeight = (height + blockHeight - 1u) / blockHeight * blockHeight;
    sliceBytes = static_cast<std::uint64_t>(pitch) * paddedHeight * fragments * elementBytes;
    Require(sliceBytes <= std::numeric_limits<std::uint64_t>::max() / layers, "multisampled surface size overflows");
    xOffsets = axisOffsets(*equation, width, 0u);
    yOffsets = axisOffsets(*equation, height, 16u);
    layerOffsets = axisOffsets(*equation, layers, 32u);
    fragmentOffsets = axisOffsets(*equation, fragments, 48u);
}

std::uint64_t MultisampleTextureLayout::offset(std::uint32_t x, std::uint32_t y, std::uint32_t layer, std::uint32_t fragment) const {
    const auto block = static_cast<std::uint64_t>(y / blockHeight) * (pitch / blockWidth) + x / blockWidth;
    const auto withinBlock = xOffsets[x] ^ yOffsets[y] ^ layerOffsets[layer] ^ fragmentOffsets[fragment] ^ pipeBankXor;
    return static_cast<std::uint64_t>(layer) * sliceBytes + block * Alignment + withinBlock;
}

std::uint64_t MultisampleTextureLayout::Offset(std::uint32_t x, std::uint32_t y, std::uint32_t layer, std::uint32_t fragment) const {
    Require(x < width && y < height && layer < layers && fragment < fragments, "multisampled surface coordinate is outside its extent");
    return offset(x, y, layer, fragment);
}

void MultisampleTextureLayout::transfer(std::span<const std::byte> source, std::span<std::byte> destination, bool retile) const {
    const auto sourceBytes = retile ? LinearBytes() : Bytes();
    const auto destinationBytes = retile ? Bytes() : LinearBytes();
    Require(source.size() >= sourceBytes && destination.size() >= destinationBytes, "multisampled surface transfer buffer is too small");
    const auto sourceAddress = reinterpret_cast<std::uintptr_t>(source.data());
    const auto destinationAddress = reinterpret_cast<std::uintptr_t>(destination.data());
    Require(sourceAddress <= std::numeric_limits<std::uintptr_t>::max() - sourceBytes && destinationAddress <= std::numeric_limits<std::uintptr_t>::max() - destinationBytes, "multisampled surface transfer range overflows");
    Require(sourceAddress + sourceBytes <= destinationAddress || destinationAddress + destinationBytes <= sourceAddress, "multisampled surface transfer buffers overlap");

    std::uint64_t linear = 0;
    for (std::uint32_t layer = 0; layer < layers; ++layer) {
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                for (std::uint32_t fragment = 0; fragment < fragments; ++fragment) {
                    const auto tiled = offset(x, y, layer, fragment);
                    std::memcpy(destination.data() + (retile ? tiled : linear), source.data() + (retile ? linear : tiled), elementBytes);
                    linear += elementBytes;
                }
            }
        }
    }
}

void MultisampleTextureLayout::Detile(std::span<const std::byte> source, std::span<std::byte> destination) const {
    transfer(source, destination, false);
}

void MultisampleTextureLayout::Tile(std::span<const std::byte> source, std::span<std::byte> destination) const {
    transfer(source, destination, true);
}

}
