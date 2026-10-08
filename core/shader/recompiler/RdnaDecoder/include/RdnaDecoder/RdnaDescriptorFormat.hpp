#ifndef CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNADESCRIPTORFORMAT_HPP
#define CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNADESCRIPTORFORMAT_HPP

#include "IntermediateRepresentation/IrMetadata.hpp"
#include <cstdint>

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

[[nodiscard]] IrTextureNumericClass VertexInputNumericClass(IrBufferFormat format);
[[nodiscard]] bool IsFmaskTextureFormat(IrBufferFormat format);
[[nodiscard]] IrTextureNumericClass SampledTextureNumericClass(IrBufferFormat format);
[[nodiscard]] IrBufferFormat RemapTextureFormat(IrBufferFormat format);
[[nodiscard]] std::uint32_t DepthBitsTextureWidth(std::uint32_t word1, std::uint32_t word3);
[[nodiscard]] bool IsDepthBitsTexture(std::uint32_t word1, std::uint32_t word3);

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
