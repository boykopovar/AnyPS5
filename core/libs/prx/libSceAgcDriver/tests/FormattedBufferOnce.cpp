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
    static constexpr std::array<std::uint32_t, 3> capabilities{
        spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess,
    };
    static constexpr std::array<std::string_view, 4> extensions{
        "SPV_EXT_descriptor_indexing", "SPV_KHR_8bit_storage", "SPV_KHR_float_controls", "SPV_KHR_physical_storage_buffer",
    };
    return {0x00401000u, 0x00010300u, 32, BdaAbi::Version, capabilities, extensions, false,
            {1024, 1024, 64}, 1024, 32768, {}, {}};
}

struct Counts {
    std::size_t words = 0;
    std::size_t switches = 0;
    std::size_t calls = 0;
    std::size_t functions = 0;
    std::size_t notInlined = 0;
};

Counts Count(std::span<const std::uint32_t> words) {
    Check(words.size() >= 5 && words[0] == spv::MagicNumber, "formatted buffer once: missing SPIR-V module");
    Counts counts{words.size()};
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        Check(count != 0 && count <= words.size() - offset, "formatted buffer once: malformed SPIR-V instruction");
        const auto opcode = words[offset] & spv::OpCodeMask;
        if (opcode == spv::OpSwitch) ++counts.switches;
        if (opcode == spv::OpFunctionCall) ++counts.calls;
        if (opcode == spv::OpFunction) {
            ++counts.functions;
            if (count > 3 && (words[offset + 3] & spv::FunctionControlDontInlineMask) != 0) ++counts.notInlined;
        }
        offset += count;
    }
    return counts;
}

Counts Compile(std::size_t loads, bool formattedStores = false) {
    std::vector<std::uint32_t> code;
    for (std::uint32_t load = 0; load < loads; ++load) code.insert(code.end(), {0xe0000000u | (load * 4u), 0x80020000u | ((10u + load) << 8u)});
    code.push_back(0xbf8c3f70u);
    for (std::uint32_t load = 0; load < loads; ++load) code.insert(code.end(), {(formattedStores ? 0xe0100000u : 0xe0700000u) | (load * 4u), 0x80010000u | ((10u + load) << 8u)});
    code.push_back(0xbf810000u);
    const std::span<const std::uint32_t> words(code);
    const std::array<MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    const std::array<std::uint32_t, 12> userData{0u, 0u, 0u, 0u, 0x10000000u, 0u, 0x1000u, 0x31016facu, 0x20000000u, 0u, 0x1000u, 0x31038facu};
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
        Check(one.calls == none.calls + 1 && one.functions == none.functions + 1, "formatted buffer once: one formatted load emitted " + std::to_string(one.calls - none.calls) + " calls and " + std::to_string(one.functions - none.functions) + " functions more than none, expected one call to one shared function");
        Check(three.calls == one.calls + 2 && three.functions == one.functions, "formatted buffer once: two more formatted loads from the same descriptor emitted " + std::to_string(three.calls - one.calls) + " calls and " + std::to_string(three.functions - one.functions) + " functions more, expected two calls and no function");
        Check(three.switches == one.switches, "formatted buffer once: two more formatted loads from the same descriptor emitted " + std::to_string(three.switches - one.switches) + " more format switches");
        Check(one.notInlined == none.notInlined + 1, "formatted buffer once: the shared formatted access function may be inlined back into its callers");
        const auto storeOne = Compile(1, true);
        const auto storeThree = Compile(3, true);
        Check(storeOne.functions == none.functions + 2 && storeOne.calls == none.calls + 2, "formatted buffer once: a formatted load and a formatted store to another resource emitted " + std::to_string(storeOne.functions - none.functions) + " functions and " + std::to_string(storeOne.calls - none.calls) + " calls more than none, expected one function and one call each");
        Check(storeThree.functions == storeOne.functions && storeThree.calls == storeOne.calls + 4 && storeThree.switches == storeOne.switches, "formatted buffer once: two more formatted loads and stores emitted " + std::to_string(storeThree.functions - storeOne.functions) + " functions, " + std::to_string(storeThree.calls - storeOne.calls) + " calls and " + std::to_string(storeThree.switches - storeOne.switches) + " switches more, expected four calls only");
        Check(three.words - one.words < one.words / 10u, "formatted buffer once: 2 more formatted loads added " + std::to_string(three.words - one.words) + " SPIR-V words to a " + std::to_string(one.words) + "-word module");
        std::printf("formatted buffer once tests passed (%zu words for 1 load, %zu for 3)\n", one.words, three.words);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
