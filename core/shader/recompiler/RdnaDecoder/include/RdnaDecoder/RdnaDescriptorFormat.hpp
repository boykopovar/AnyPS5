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

[[nodiscard]] bool IsFmaskTextureFormat(IrBufferFormat format);
[[nodiscard]] IrTextureNumericClass SampledTextureNumericClass(IrBufferFormat format);
[[nodiscard]] IrBufferFormat RemapTextureFormat(IrBufferFormat format);
// A sampled T# (dwords 1 and 3) of a 32-bit integer format over a depth layout (a Z swizzle mode):
// the raw bits of a D32 depth plane. A host cannot view a depth image with an integer format, so
// the image binds its float depth view and the shader takes the texel bits (ImageResource::depthBits).
[[nodiscard]] bool IsDepthBitsTexture(std::uint32_t word1, std::uint32_t word3);

}

#endif
