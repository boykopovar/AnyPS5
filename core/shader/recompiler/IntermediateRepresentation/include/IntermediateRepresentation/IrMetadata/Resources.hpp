#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_RESOURCES_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_RESOURCES_HPP

#include "IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include "IntermediateRepresentation/IrOpcode.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"
#include <cstdint>
#include <limits>
#include <vector>

namespace ShaderRecompiler {

struct BufferResource {
    static constexpr std::uint32_t NoImageAlias = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t source = 0;
    std::uint32_t firstUsePc = 0;
    std::uint32_t maxByteExtent = 0;
    std::uint32_t packedStride = 0;
    IrBufferFormat descriptorFormat = IrBufferFormat::Invalid;
    std::uint32_t descriptorSwizzle = 0x00000facu;
    std::uint32_t imageAlias = NoImageAlias;
    bool read = false;
    bool written = false;
    bool atomic = false;
    bool formatted = false;
    bool scalar = false;
    // Specialized: the V# has no records or is at address 0 (a null V#), so every access is out of
    // bounds: loads read zero (or the format's default), stores and atomics are dropped. For no
    // records that is the hardware's behavior; at address 0 an access would fault on hardware, so a
    // working title never reaches such a slot.
    bool empty = false;

    bool operator==(const BufferResource& other) const = default;
};

enum class ImageMipMode { None, DynamicStorage };

struct ImageResource {
    static constexpr std::uint32_t NoIndirectImage = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t source = 0;
    std::uint32_t firstUsePc = 0;
    ImageResourceClass resourceClass = ImageResourceClass::None;
    IrTextureNumericClass numericClass = IrTextureNumericClass::Unsupported;
    RdnaImageDimension dimension = RdnaImageDimension::Unknown;
    ImageMipMode mipMode = ImageMipMode::None;
    std::uint32_t mipCount = 1;
    IrBufferFormat conversionFormat = IrBufferFormat::Invalid;
    std::uint32_t shaderSwizzle = ShaderImageIdentitySwizzle;
    bool read = false;
    bool written = false;
    bool atomic = false;
    bool depthCompare = false;
    bool cube = false;
    bool r128 = false;
    // Specialized: a 32-bit integer read of a depth plane (IsDepthBitsTexture), bound as the float
    // depth view; the texel bits are the result, the swizzle's constants integers.
    bool depthBits = false;
    std::uint32_t indirectRoot = NoIndirectImage;
    std::uint32_t indirectMappingOffset = 0;
    std::uint32_t indirectSearchIterations = 0;
    std::vector<std::uint32_t> indirectResources;

    bool operator==(const ImageResource& other) const = default;
};

struct SamplerResource {
    std::uint32_t source = 0;
    std::uint32_t firstUsePc = 0;
    bool forcePointFiltering = false;
    bool depthCompare = false;

    bool operator==(const SamplerResource& other) const = default;
};

struct SampledResourcePair {
    std::uint32_t image = 0;
    std::uint32_t sampler = 0;
    std::uint32_t firstUsePc = 0;

    bool operator==(const SampledResourcePair& other) const = default;
};

}

#endif
