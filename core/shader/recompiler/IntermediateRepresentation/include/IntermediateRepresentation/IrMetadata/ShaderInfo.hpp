#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_SHADERINFO_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_SHADERINFO_HPP

#include "IntermediateRepresentation/IrMetadata/Resources.hpp"
#include "IntermediateRepresentation/IrMetadata/StageIO.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ShaderRecompiler {

struct ShaderInfo {
    std::uint32_t scratchDwords = 0;
    std::uint32_t sharedMemoryBytes = 0;
    static constexpr std::uint32_t MaxImages = 128;
    static constexpr std::uint32_t MaxSamplers = 128;
    static constexpr std::uint32_t MaxSampledPairs = 256;

    std::vector<BufferResource> buffers;
    std::vector<ImageResource> images;
    std::vector<std::vector<ImageResource>> runtimeImageModes;
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
    bool bdaWrites = false;
    bool usesFaultBuffer = false;
    bool dispatchThreadLimit = false;
    bool rasterPrimitive = false;
    bool layerExported = false;
    bool smoothOffsetBarycentrics = false;

    bool operator==(const ShaderInfo& other) const = default;
};

struct SamplerMask {
    std::array<std::uint64_t, (ShaderInfo::MaxSamplers + 63u) / 64u> words{};

    void Set(std::uint32_t sampler) {
        words.at(sampler / 64u) |= std::uint64_t{1} << (sampler % 64u);
    }

    [[nodiscard]] bool Test(std::uint32_t sampler) const {
        return sampler / 64u < words.size() && ((words[sampler / 64u] >> (sampler % 64u)) & 1u) != 0u;
    }

    template<typename TVisit>
    void ForEach(TVisit&& visit) const {
        for (std::size_t word = 0; word < words.size(); ++word) {
            for (auto bits = words[word]; bits != 0u; bits &= bits - 1u) visit(static_cast<std::uint32_t>(word * 64u) + static_cast<std::uint32_t>(std::countr_zero(bits)));
        }
    }

    SamplerMask& operator|=(const SamplerMask& other) {
        for (std::size_t word = 0; word < words.size(); ++word) words[word] |= other.words[word];
        return *this;
    }

    bool operator==(const SamplerMask& other) const = default;
};

[[nodiscard]] inline std::uint32_t WorkgroupMemoryStrideDwords(const ShaderInfo& info) {
    return info.sharedMemoryBytes == 0u ? 0u : (info.sharedMemoryBytes / 4u + 2u) & ~1u;
}

}

#endif
