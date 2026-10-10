#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_RESOURCES_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_RESOURCES_HPP

#include "IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderStage.hpp"
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
    std::uint32_t imageAlias = NoImageAlias;
    bool read = false;
    bool written = false;
    bool atomic = false;
    bool formatted = false;
    bool descriptorFormatted = false;
    std::uint32_t formattedReadMask = 0;
    bool scalar = false;
    std::uint8_t typedAlignment = 1;

    bool operator==(const BufferResource& other) const = default;
};

enum class ImageMipMode { None, DynamicStorage };

namespace EmulatedCompare {
inline constexpr std::uint32_t NativeOffsetUnsupported = 1u << 29u;
inline constexpr std::uint32_t Unsupported = 1u << 31u;
inline constexpr std::uint32_t RequiresSingleLevel = 1u << 30u;
inline constexpr std::uint32_t Enabled = 1u << 0u;
inline constexpr std::uint32_t FunctionShift = 1u;
inline constexpr std::uint32_t Linear = 1u << 4u;
inline constexpr std::uint32_t ClampXShift = 5u;
inline constexpr std::uint32_t ClampYShift = 7u;
inline constexpr std::uint32_t ReferenceShift = 9u;
inline constexpr std::uint32_t SingleLevel = 1u << 11u;
inline constexpr std::uint32_t BorderWhite = 1u << 12u;
inline constexpr std::uint32_t AddressWrap = 0u;
inline constexpr std::uint32_t AddressEdge = 1u;
inline constexpr std::uint32_t AddressBorder = 2u;
inline constexpr std::uint32_t ReferenceFloat = 0u;
inline constexpr std::uint32_t ReferenceUnorm = 1u;
inline constexpr std::uint32_t ReferenceSnorm = 2u;
[[nodiscard]] inline std::uint32_t Function(std::uint32_t state) { return (state >> FunctionShift) & 0x7u; }
[[nodiscard]] inline std::uint32_t AddressX(std::uint32_t state) { return (state >> ClampXShift) & 0x3u; }
[[nodiscard]] inline std::uint32_t AddressY(std::uint32_t state) { return (state >> ClampYShift) & 0x3u; }
[[nodiscard]] inline std::uint32_t Reference(std::uint32_t state) { return (state >> ReferenceShift) & 0x3u; }
}

inline constexpr std::uint32_t NoTable = std::numeric_limits<std::uint32_t>::max();

enum class TableOperation : std::uint8_t { Supported, Unsupported };

struct ImageResource {
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
    bool atomic64 = false;
    bool depthCompare = false;
    bool cube = false;
    bool r128 = false;
    bool srgbDecode = false;
    bool srgbDecodeCompatible = true;
    std::uint32_t srgbDecodeFormats = 0u;
    bool depthBits = false;
    bool depthUnorm16 = false;
    bool packed = false;
    bool fmaskCompatible = true;
    bool depthBitsCompatible = true;
    bool constantSwizzle = false;
    bool constantSwizzleCompatible = true;
    bool flatVolumeCompatible = true;
    bool flatLineCompatible = true;
    std::uint32_t byElements = 0;
    std::uint32_t byComponents = 0;
    IrBufferFormat packedFormat = IrBufferFormat::Invalid;
    std::uint32_t emulatedCompare = 0;
    std::uint32_t table = NoTable;
    TableOperation tableOperation = TableOperation::Supported;

    bool operator==(const ImageResource& other) const = default;
};

enum SamplerUse : std::uint8_t {
    SamplerUseExplicitLod = 1u << 0u,
    SamplerUseImplicitLod = 1u << 1u,
    SamplerUseGradient = 1u << 2u,
    SamplerUseOffset = 1u << 3u,
    SamplerUseCompare = 1u << 4u,
    SamplerUseGather = 1u << 5u,
    SamplerUseQueryLod = 1u << 6u,
    SamplerUseAdjust = 1u << 7u,
};

inline bool ImageSampleExplicitLod(std::uint32_t flags, IrShaderStage stage) {
    return (flags & (RdnaImageSampleFlagDerivative | RdnaImageSampleFlagLod | RdnaImageSampleFlagLevelZero)) != 0u || stage != IrShaderStage::Pixel;
}

struct SamplerResource {
    static constexpr std::uint32_t NoCopy = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t source = 0;
    std::uint32_t firstUsePc = 0;
    std::uint32_t copyOf = NoCopy;
    bool forcePointFiltering = false;
    bool depthCompare = false;
    std::uint8_t uses = 0;
    std::uint32_t table = NoTable;

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
