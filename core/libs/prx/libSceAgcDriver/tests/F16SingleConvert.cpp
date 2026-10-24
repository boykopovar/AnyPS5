#include "BdaAbi.hpp"
#include "Recompiler.hpp"
#include <array>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

using namespace ShaderRecompiler;

void Check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

SpirvTarget Target(bool nativeFloat16) {
    static constexpr std::array<std::uint32_t, 5> plain{
        spv::CapabilityInt64, spv::CapabilityFloat64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityClipDistance,
    };
    static constexpr std::array<std::uint32_t, 8> native{
        spv::CapabilityInt64, spv::CapabilityFloat64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityClipDistance,
        spv::CapabilityFloat16, spv::CapabilityRoundingModeRTE, spv::CapabilityDenormPreserve,
    };
    static constexpr std::array<std::string_view, 4> extensions{
        "SPV_EXT_descriptor_indexing", "SPV_KHR_8bit_storage", "SPV_KHR_float_controls", "SPV_KHR_physical_storage_buffer",
    };
    if (nativeFloat16) return {0x00401000u, 0x00010300u, 32, BdaAbi::Version, native, extensions, false, {1024, 1024, 64}, 1024, 32768, {}, {}};
    return {0x00401000u, 0x00010300u, 32, BdaAbi::Version, plain, extensions, false, {1024, 1024, 64}, 1024, 32768, {}, {}};
}

std::size_t ZeroPairs(std::span<const std::uint32_t> words) {
    Check(words.size() >= 5 && words[0] == spv::MagicNumber, "f16 single convert: missing SPIR-V module");
    std::unordered_set<std::uint32_t> floats;
    std::unordered_set<std::uint32_t> pairs;
    std::unordered_set<std::uint32_t> zeros;
    std::size_t count = 0;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto length = words[offset] >> spv::WordCountShift;
        Check(length != 0 && length <= words.size() - offset, "f16 single convert: malformed SPIR-V instruction");
        const auto opcode = words[offset] & spv::OpCodeMask;
        if (opcode == spv::OpTypeFloat && length == 3u && words[offset + 2u] == 32u) floats.insert(words[offset + 1u]);
        if (opcode == spv::OpTypeVector && length == 4u && floats.contains(words[offset + 2u]) && words[offset + 3u] == 2u) pairs.insert(words[offset + 1u]);
        if (opcode == spv::OpConstant && length == 4u && floats.contains(words[offset + 1u]) && words[offset + 3u] == 0u) zeros.insert(words[offset + 2u]);
        if (opcode == spv::OpConstantNull && length == 3u && floats.contains(words[offset + 1u])) zeros.insert(words[offset + 2u]);
        if (opcode == spv::OpCompositeConstruct && length == 5u && pairs.contains(words[offset + 1u]) && zeros.contains(words[offset + 4u])) ++count;
        offset += length;
    }
    return count;
}

std::size_t Compile(bool nativeFloat16) {
    std::vector<std::uint32_t> code{0xe0300000u, 0x80010100u, 0xbf8c3f70u};
    for (std::uint32_t op = 0; op < 4u; ++op) code.push_back(0x7e04a900u | (256u + (op == 0u ? 1u : 2u)));
    code.insert(code.end(), {0xe0700000u, 0x80010200u, 0xbf810000u});
    const std::span<const std::uint32_t> words(code);
    const std::array<MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    const std::array<std::uint32_t, 8> userData{0u, 0u, 0u, 0u, 0x10000000u, 0u, 0x1000u, 0x31016facu};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0, {}};
    request.target = Target(nativeFloat16);
    request.layout = {0, 0, 0, 128};
    request.context.waveSize = 32;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.context.memory = memory;
    request.useCache = false;
    return ZeroPairs(Recompile(request).spirv.Words());
}

}

int main() {
    try {
        for (const bool nativeFloat16 : {false, true}) {
            const auto pairs = Compile(nativeFloat16);
            Check(pairs == 0u, std::string("f16 single convert: four v_rcp_f16 emitted ") + std::to_string(pairs) + " (x, 0.0) f32 pairs to convert one f16 each" + (nativeFloat16 ? " with native f16" : " without native f16"));
        }
        std::puts("f16 single convert tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
