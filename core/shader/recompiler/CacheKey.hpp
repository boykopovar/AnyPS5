#ifndef CORE_SHADER_RECOMPILER_CACHEKEY_HPP
#define CORE_SHADER_RECOMPILER_CACHEKEY_HPP

#include "Recompiler.hpp"
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

namespace ShaderRecompiler {

class RecompileCacheKey {
public:
    static void Build(const RecompileRequest& request, std::vector<std::uint64_t>& key) {
        key.clear();
        append(key, request.shader.stage);
        // The code enters as a hash rather than word by word: the key is built, hashed and compared
        // on every dispatch and draw. The cache verifies a match against the code it stored.
        // Debug aid: APS5_NO_CODE_HASH_KEY=1 appends every code word, as before.
        static const bool hashedCode = std::getenv("APS5_NO_CODE_HASH_KEY") == nullptr;
        if (hashedCode) {
            append(key, request.shader.code.size());
            append(key, HashCode(request.shader.code));
        } else {
            append(key, request.shader.code);
        }
        append(key, request.context.waveSize);
        append(key, request.context.userDataBaseRegister);
        append(key, request.context.userData.size());
        append(key, request.context.compute);
        append(key, request.context.pixel);
        append(key, request.context.vertex);
        append(key, request.target);
        append(key, DebugProbeActive());
    }

    // A 64-bit hash of the code, two dwords per step; collisions are resolved by comparing the code.
    static std::uint64_t HashCode(std::span<const std::uint32_t> code) {
        std::uint64_t hash = 0x9e3779b97f4a7c15ull ^ (static_cast<std::uint64_t>(code.size()) * 0x100000001b3ull);
        const auto mix = [&](std::uint64_t chunk) {
            hash = (hash ^ chunk) * 0x9e3779b97f4a7c15ull;
            hash ^= hash >> 29u;
        };
        std::size_t index = 0;
        for (; index + 2 <= code.size(); index += 2) mix(code[index] | (static_cast<std::uint64_t>(code[index + 1]) << 32u));
        if (index < code.size()) mix(code[index]);
        return hash;
    }

private:
    template<typename TValue>
    static void append(std::vector<std::uint64_t>& key, TValue value) requires (std::is_integral_v<TValue> || std::is_enum_v<TValue>) {
        key.push_back(static_cast<std::uint64_t>(value));
    }

    template<typename TValue, std::size_t TSize>
    static void append(std::vector<std::uint64_t>& key, const std::array<TValue, TSize>& values) {
        for (const auto value : values) append(key, value);
    }

    template<typename TValue>
    static void append(std::vector<std::uint64_t>& key, std::span<TValue> values) {
        append(key, values.size());
        for (const auto value : values) append(key, value);
    }

    template<typename TValue>
    static void append(std::vector<std::uint64_t>& key, const std::optional<TValue>& value) {
        append(key, value.has_value());
        if (value) append(key, *value);
    }

    static void append(std::vector<std::uint64_t>& key, std::string_view value) {
        append(key, value.size());
        for (const unsigned char byte : value) append(key, byte);
    }

    static void append(std::vector<std::uint64_t>& key, const ShaderComputeStageInfo& value) {
        append(key, value.numThreads);
        append(key, value.ldsSizeDwords);
        append(key, value.groupIdEnable);
        append(key, value.tgSizeEnable);
        append(key, value.threadIdComponentCount);
    }

    static void append(std::vector<std::uint64_t>& key, const ShaderPixelStageInfo& value) {
        append(key, value.interpolatorCount);
        if (value.interpolatorCount > value.interpolatorSettings.size()) throw std::runtime_error("Shader cache: invalid interpolator count");
        for (std::uint32_t i = 0; i < value.interpolatorCount; ++i) append(key, value.interpolatorSettings[i]);
        append(key, value.wave32);
        append(key, value.perspectiveCenterVgpr);
        append(key, value.hasPerspectiveCenterVgpr);
        append(key, value.posX);
        append(key, value.posY);
        append(key, value.posZ);
        append(key, value.posW);
        append(key, value.frontFace);
        append(key, value.ancillary);
        append(key, value.sampleShading);
        append(key, value.noPerspective);
        append(key, value.pixelKillEnable);
        append(key, value.depthExportEnable);
        append(key, value.sampleMaskExportEnable);
        append(key, value.earlyZ);
        append(key, value.executeOnNoop);
        append(key, value.targetOutputMode);
        append(key, value.targetExportMapping);
    }

    static void append(std::vector<std::uint64_t>& key, const ShaderVertexResourceDestination& value) {
        append(key, value.registerStart);
        append(key, value.registersNum);
        append(key, value.attrId);
        append(key, value.fetchIndex);
    }

    static void append(std::vector<std::uint64_t>& key, const ShaderVertexStageInfo& value) {
        append(key, value.resourcesNum);
        append(key, value.fetchAttribReg);
        append(key, value.fetchBufferReg);
        append(key, value.fetchEmbedded);
        if (value.resourcesNum > value.resources.size()) throw std::runtime_error("Shader cache: invalid vertex resource count");
        for (std::uint32_t i = 0; i < value.resourcesNum; ++i) {
            append(key, value.resources[i].fields[1] & 0xffff0000u);
            append(key, value.resources[i].fields[3]);
            append(key, value.resourcesDst[i]);
        }
    }

    static void append(std::vector<std::uint64_t>& key, const MeshTargetLimits& value) {
        append(key, value.maxWorkgroupSize);
        append(key, value.maxWorkgroupInvocations);
        append(key, value.maxSharedMemoryBytes);
        append(key, value.maxOutputVertices);
        append(key, value.maxOutputPrimitives);
        append(key, value.maxOutputComponents);
        append(key, value.maxOutputMemoryBytes);
        append(key, value.outputPerVertexGranularity);
        append(key, value.outputPerPrimitiveGranularity);
    }

    static void append(std::vector<std::uint64_t>& key, const TessellationTargetLimits& value) {
        append(key, value.maxPatchSize);
        append(key, value.maxControlPerVertexInputComponents);
        append(key, value.maxControlPerVertexOutputComponents);
        append(key, value.maxControlPerPatchOutputComponents);
        append(key, value.maxControlTotalOutputComponents);
        append(key, value.maxEvaluationInputComponents);
        append(key, value.maxEvaluationOutputComponents);
    }

    static void append(std::vector<std::uint64_t>& key, const SpirvTarget& value) {
        append(key, value.vulkanVersion);
        append(key, value.spirvVersion);
        append(key, value.subgroupSize);
        append(key, value.bdaAbiVersion);
        append(key, value.supportedCapabilities);
        append(key, value.supportedExtensions);
        append(key, value.fragmentShaderBarycentricEnabled);
        append(key, value.maxWorkgroupSize);
        append(key, value.maxWorkgroupInvocations);
        append(key, value.maxWorkgroupSharedMemoryBytes);
        append(key, value.mesh);
        append(key, value.tessellation);
    }
};

}

#endif
