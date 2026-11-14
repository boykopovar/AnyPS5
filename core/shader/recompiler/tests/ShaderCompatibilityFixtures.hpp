#ifndef CORE_SHADER_RECOMPILER_TESTS_SHADERCOMPATIBILITYFIXTURES_HPP
#define CORE_SHADER_RECOMPILER_TESTS_SHADERCOMPATIBILITYFIXTURES_HPP

#include "BdaAbi.hpp"
#include "CacheKey.hpp"
#include "Recompiler.hpp"
#include "ShaderDiskCache.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <string_view>
#include <vector>

namespace ShaderRecompiler::Tests {

inline const SpirvTarget& VulkanTarget() {
    static constexpr std::array<std::uint32_t, 5> capabilities{
        spv::CapabilityInt64, spv::CapabilityFloat64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityClipDistance,
    };
    static constexpr std::array<std::string_view, 4> extensions{
        "SPV_EXT_descriptor_indexing", "SPV_KHR_8bit_storage", "SPV_KHR_float_controls", "SPV_KHR_physical_storage_buffer",
    };
    static const SpirvTarget target{0x00401000u, 0x00010300u, 32, BdaAbi::Version, capabilities, extensions, false, {1024, 1024, 64}, 1024, 32768, {}, {}};
    return target;
}

struct CompatibilityProgram {
    std::vector<std::uint32_t> code;
    std::array<MemoryRegion, 1> memory{};
    std::array<std::uint32_t, 8> userData{0u, 0u, 0u, 0u, 0x10000000u, 0u, 0x1000u, 0x31016facu};

    explicit CompatibilityProgram(std::vector<std::uint32_t> words) : code(std::move(words)) {
        memory[0] = {0x40000000u, std::as_bytes(std::span<const std::uint32_t>(code))};
    }

    RecompileRequest Request(const SpirvTarget& target, bool useCache) const {
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x40000000u, code, 0, {}};
        request.target = target;
        request.layout = {0, 0, 0, 128};
        request.context.waveSize = 32;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.context.memory = memory;
        request.useCache = useCache;
        return request;
    }
};

inline std::vector<CompatibilityProgram> CompatibilityPrograms() {
    std::vector<CompatibilityProgram> programs;
    programs.reserve(3);
    programs.emplace_back(std::vector<std::uint32_t>{0xe0300000u, 0x80010100u, 0xbf8c3f70u, 0xe0700000u, 0x80010100u, 0xbf810000u});
    programs.emplace_back(std::vector<std::uint32_t>{0xe0300000u, 0x80010100u, 0xbf8c3f70u, 0x06040301u, 0xe0700004u, 0x80010200u, 0xbf810000u});
    programs.emplace_back(std::vector<std::uint32_t>{0xe0300000u, 0x80010100u, 0xe0300004u, 0x80010200u, 0xbf8c3f70u, 0x06060501u, 0xe0700008u, 0x80010300u, 0xbf810000u});
    return programs;
}

inline std::uint64_t CompatibilityHash(std::span<const std::byte> bytes) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto byte : bytes) hash = (hash ^ std::to_integer<std::uint8_t>(byte)) * 1099511628211ull;
    return hash;
}

inline std::vector<std::uint64_t> CompatibilityKey(const RecompileRequest& request) {
    std::vector<std::uint64_t> key;
    RecompileCacheKey::Build(request, key);
    return key;
}

inline std::vector<std::byte> CompatibilityArtifact(const RecompileResult& result) {
    std::vector<std::byte> bytes;
    ShaderDiskCache::EncodeResult(result, bytes);
    return bytes;
}

}  // namespace ShaderRecompiler::Tests

#endif
