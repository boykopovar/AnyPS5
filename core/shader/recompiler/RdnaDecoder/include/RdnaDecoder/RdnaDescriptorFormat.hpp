#ifndef CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNADESCRIPTORFORMAT_HPP
#define CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNADESCRIPTORFORMAT_HPP

#include "IntermediateRepresentation/IrMetadata.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace ShaderRecompiler {

enum class ImageType : std::uint32_t {
    Color1D = 8,
    Color2D = 9,
    Color3D = 10,
    Cube = 11,
    Color1DArray = 12,
    Color2DArray = 13,
    Color2DMsaa = 14,
    Color2DMsaaArray = 15
};

struct ImageFormatRange {
    std::uint32_t first;
    std::uint32_t last;
};

inline constexpr std::array<ImageFormatRange, 4> DefinedImageFormats{{{1u, 77u}, {128u, 154u}, {156u, 182u}, {265u, 292u}}};

[[nodiscard]] IrTextureNumericClass VertexInputNumericClass(IrBufferFormat format);
[[nodiscard]] bool IsDefinedImageFormat(IrBufferFormat format);
[[nodiscard]] bool IsFmaskTextureFormat(IrBufferFormat format);

struct FmaskLayout {
    std::uint32_t samples;
    std::uint32_t fragments;
    std::uint32_t sampleBits;
    std::uint32_t storageBits;
};

inline constexpr IrBufferFormat FmaskExpansion = IrBufferFormat::Fmask8_S2_F1;

[[nodiscard]] std::optional<FmaskLayout> FmaskLayoutOf(IrBufferFormat format);
[[nodiscard]] IrTextureNumericClass SampledTextureNumericClass(IrBufferFormat format);
[[nodiscard]] IrBufferFormat RemapTextureFormat(IrBufferFormat format);
[[nodiscard]] IrBufferFormat RemapStorageTextureFormat(IrBufferFormat format);
[[nodiscard]] std::uint32_t DepthBitsTextureWidth(std::uint32_t word1, std::uint32_t word3);
[[nodiscard]] bool IsDepthBitsTexture(std::uint32_t word1, std::uint32_t word3);

[[nodiscard]] const std::array<std::uint8_t, 128>& GcnBufferFormats();
[[nodiscard]] std::uint32_t GcnBufferWord3(std::uint32_t word1, std::uint32_t word3);
inline constexpr std::uint32_t GcnBufferWord1Mask = 0xffff0fffu;
[[nodiscard]] std::uint32_t GcnImageFormat(std::uint32_t dataFormat, std::uint32_t numberFormat);
[[nodiscard]] std::array<std::uint32_t, 8> GcnImageWords(std::span<const std::uint32_t, 8> words);
[[nodiscard]] constexpr std::uint32_t GcnImageTilingIndex(std::uint32_t word7) { return word7 & 0x1fu; }
[[nodiscard]] constexpr bool GcnImagePow2Pad(std::uint32_t word7) { return ((word7 >> 5u) & 1u) != 0u; }
[[nodiscard]] constexpr std::uint32_t GcnImagePitch(std::uint32_t word7) { return ((word7 >> 6u) & 0x3fffu) + 1u; }

[[nodiscard]] constexpr std::uint32_t SrgbDecodeBit(IrBufferFormat format) {
    switch (format) {
        case IrBufferFormat::Format8Srgb: return 1u;
        case IrBufferFormat::Format8_8Srgb: return 2u;
        default: return 0u;
    }
}

[[nodiscard]] constexpr IrBufferFormat SrgbUnormFormat(IrBufferFormat format) {
    switch (format) {
        case IrBufferFormat::Format8Srgb: return IrBufferFormat::Format8UNorm;
        case IrBufferFormat::Format8_8Srgb: return IrBufferFormat::Format8_8UNorm;
        default: return IrBufferFormat::Invalid;
    }
}

}

#endif
