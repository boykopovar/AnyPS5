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
#include <vector>

namespace {

using namespace ShaderRecompiler;

void Check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

SpirvTarget Target() {
    static constexpr std::array<std::uint32_t, 5> capabilities{
        spv::CapabilityInt64, spv::CapabilityFloat64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityClipDistance,
    };
    static constexpr std::array<std::string_view, 4> extensions{
        "SPV_EXT_descriptor_indexing", "SPV_KHR_8bit_storage", "SPV_KHR_float_controls", "SPV_KHR_physical_storage_buffer",
    };
    return {0x00401000u, 0x00010300u, 32, BdaAbi::Version, capabilities, extensions, false,
            {1024, 1024, 64}, 1024, 32768, {}, {}};
}

struct Counts {
    std::size_t words = 0;
    std::size_t functions = 0;
};

Counts Count(std::span<const std::uint32_t> words) {
    Check(words.size() >= 5 && words[0] == spv::MagicNumber, "fma mix once: missing SPIR-V module");
    Counts counts{words.size()};
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        Check(count != 0 && count <= words.size() - offset, "fma mix once: malformed SPIR-V instruction");
        if ((words[offset] & spv::OpCodeMask) == spv::OpFunction) ++counts.functions;
        offset += count;
    }
    return counts;
}

Counts Compile(std::uint32_t uses) {
    std::vector<std::uint32_t> code;
    for (std::uint32_t load = 0; load < 3u; ++load) code.insert(code.end(), {0xe0300000u | (load * 4u), 0x80010000u | ((1u + load) << 8u)});
    code.push_back(0xbf8c3f70u);
    std::uint32_t result = 1u;
    for (std::uint32_t use = 0; use < uses; ++use) {
        const std::uint32_t opcode = use % 2u == 0u ? 0x21u : 0x22u;
        code.insert(code.end(), {0xcc000000u | (opcode << 16u) | 10u, (256u + result) | ((256u + 2u) << 9u) | ((256u + 3u) << 18u)});
        result = 10u;
    }
    code.insert(code.end(), {0xe0700000u, 0x80010000u | (result << 8u)});
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
        const auto none = Compile(0);
        const auto one = Compile(1);
        const auto three = Compile(3);
        Check(three.words - one.words < one.words - none.words, "fma mix once: 2 more v_fma_mixlo/mixhi_f16 added " + std::to_string(three.words - one.words) + " SPIR-V words, as many as the first one with its shared emulation (" + std::to_string(one.words - none.words) + ")");
        Check(one.functions == none.functions + 2u, "fma mix once: one v_fma_mixlo_f16 emitted " + std::to_string(one.functions - none.functions) + " functions more than none, expected one shared f32 path and its shared exact emulation");
        Check(three.functions == one.functions, "fma mix once: two more v_fma_mixlo/mixhi_f16 in the same float mode emitted " + std::to_string(three.functions - one.functions) + " more functions, expected calls to the shared one");
        std::printf("fma mix once tests passed (%zu words without v_fma_mix, %zu for 1, %zu for 3)\n", none.words, one.words, three.words);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
