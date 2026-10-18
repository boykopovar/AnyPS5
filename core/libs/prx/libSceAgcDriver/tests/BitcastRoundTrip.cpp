#define SPV_ENABLE_UTILITY_CODE
#include "BdaAbi.hpp"
#include "Recompiler.hpp"
#include <array>
#include <cstdio>
#include <exception>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

using namespace ShaderRecompiler;

void Check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

SpirvTarget Target() {
    static constexpr std::array<std::uint32_t, 8> capabilities{
        spv::CapabilityInt64, spv::CapabilityFloat64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityClipDistance,
        spv::CapabilityFloat16, spv::CapabilityRoundingModeRTE, spv::CapabilityDenormPreserve,
    };
    static constexpr std::array<std::string_view, 4> extensions{
        "SPV_EXT_descriptor_indexing", "SPV_KHR_8bit_storage", "SPV_KHR_float_controls", "SPV_KHR_physical_storage_buffer",
    };
    return {0x00401000u, 0x00010300u, 32, BdaAbi::Version, capabilities, extensions, false,
            {1024, 1024, 64}, 1024, 32768, {}, {}};
}

struct Counts {
    std::size_t bitcasts = 0;
    std::size_t roundTrips = 0;
};

Counts Count(std::span<const std::uint32_t> words) {
    Check(words.size() >= 5 && words[0] == spv::MagicNumber, "bitcast round trip: missing SPIR-V module");
    std::unordered_map<std::uint32_t, std::uint32_t> types;
    std::unordered_map<std::uint32_t, std::uint32_t> sources;
    Counts counts;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        Check(count != 0 && count <= words.size() - offset, "bitcast round trip: malformed SPIR-V instruction");
        const auto opcode = static_cast<spv::Op>(words[offset] & spv::OpCodeMask);
        bool hasResult = false;
        bool hasType = false;
        spv::HasResultAndType(opcode, &hasResult, &hasType);
        if (hasResult && hasType && count >= 3u) types[words[offset + 2u]] = words[offset + 1u];
        if (opcode == spv::OpBitcast && count == 4u) {
            ++counts.bitcasts;
            const auto type = words[offset + 1u];
            const auto operand = words[offset + 3u];
            if (const auto source = sources.find(operand); source != sources.end() && types[source->second] == type) ++counts.roundTrips;
            sources[words[offset + 2u]] = operand;
        }
        offset += count;
    }
    return counts;
}

Counts Compile(std::uint32_t adds) {
    std::vector<std::uint32_t> code;
    for (std::uint32_t load = 0; load < 3u; ++load) code.insert(code.end(), {0xe0300000u | (load * 4u), 0x80010000u | ((1u + load) << 8u)});
    code.push_back(0xbf8c3f70u);
    for (std::uint32_t add = 0; add < adds; ++add) {
        code.push_back(0x7e080d00u | (256u + 1u + add % 3u));
        code.push_back(0x060a0004u | (2u << 9u) | (256u + 4u));
        code.push_back(0x060c0004u | (5u << 9u) | (256u + 4u));
    }
    code.insert(code.end(), {0xe0700000u, 0x80010000u | (6u << 8u)});
    code.push_back(0xbf810000u);
    const std::span<const std::uint32_t> words(code);
    const std::array<MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    const std::array<std::uint32_t, 8> userData{0u, 0u, 0u, 0u, 0x10000000u, 0u, 0x1000u, 0x31016facu};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0, {}};
    request.target = Target();
    request.layout = {0, 0, 0, 128};
    request.context.waveSize = 32;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.context.memory = memory;
    request.useCache = false;
    return Count(Recompile(request).spirv.Words());
}

}

int main() {
    try {
        const auto counts = Compile(6);
        Check(counts.bitcasts != 0u, "bitcast round trip: v_cvt_f32_u32 and v_add_f32 emitted no bitcast, so the test no longer covers register round trips");
        Check(counts.roundTrips == 0u, "bitcast round trip: v_cvt_f32_u32 results added with v_add_f32 emitted " + std::to_string(counts.roundTrips) + " bitcasts back to the type of a bitcast's source (" + std::to_string(counts.bitcasts) + " bitcasts in all)");
        std::printf("bitcast round trip tests passed (%zu bitcasts)\n", counts.bitcasts);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
