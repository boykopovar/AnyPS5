#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_SHADERINFO_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_SHADERINFO_HPP

#include "IntermediateRepresentation/IrMetadata/Resources.hpp"
#include "IntermediateRepresentation/IrMetadata/StageIO.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace ShaderRecompiler {

struct ShaderInfo {
    std::uint32_t scratchDwords = 0;
    std::uint32_t sharedMemoryBytes = 0;
    static constexpr std::uint32_t MaxBuffers = 32;
    static constexpr std::uint32_t MaxImages = 64;
    static constexpr std::uint32_t MaxSamplers = 32;
    static constexpr std::uint32_t MaxSampledPairs = 64;

    std::vector<BufferResource> buffers;
    std::vector<ImageResource> images;
    std::vector<SamplerResource> samplers;
    std::vector<SampledResourcePair> sampledPairs;
    std::vector<StageInput> inputs;
    std::vector<StageOutput> outputs;
    std::array<std::uint8_t, 32> vertexFetchComponents {};
    std::int32_t vertexOffsetSgpr = -1;
    std::int32_t instanceOffsetSgpr = -1;
    bool vertexOffsetShared = false;
    bool instanceOffsetShared = false;
    bool vertexOffsetConflict = false;
    bool instanceOffsetConflict = false;
    bool hasBitwiseXor = false;
    bool usesDma = false;

    bool operator==(const ShaderInfo& other) const = default;
};

}

#endif
