#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, MaxThreads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 109> Wave32Code{
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e1402ffu, 0xdeadbeefu, 0x7e1602ffu, 0xdeadbeefu,
    0x7e1802ffu, 0xdeadbeefu, 0x7e1a02ffu, 0xdeadbeefu, 0xbe940380u, 0xd7600008u, 0x02002904u, 0xd760000au,
    0x02002905u, 0xbe8c1f00u, 0x800cff0cu, 0x0000014cu, 0x820d800du, 0xbe89030au, 0x83928308u, 0xd4c40016u,
    0x02001104u, 0x8f128212u, 0xf4000386u, 0x24000000u, 0xbf8cc07fu, 0x7d820a0au, 0x808c0e0cu, 0x828d800du,
    0xbefd210cu, 0x8009ff0au, 0x11110000u, 0xbf820008u, 0x8009ff0au, 0x22220000u, 0xbf820005u, 0x8009ff0au,
    0x33330000u, 0xbf820002u, 0x8009ff0au, 0x44440000u, 0xd761000au, 0x02002809u, 0x816ac108u, 0x83ea836au,
    0x8f6a826au, 0xbe8e1f00u, 0x800eff0eu, 0x000000d4u, 0x820f800fu, 0xf4000487u, 0xd4000008u, 0x8aea167eu,
    0xbf8cc07fu, 0x808e120eu, 0x828f800fu, 0xbefd210eu, 0x8009ff0au, 0x0a000000u, 0xbf820008u, 0x8009ff0au,
    0x0b000000u, 0xbf820005u, 0x8009ff0au, 0x0c000000u, 0xbf820002u, 0x8009ff0au, 0x0d000000u, 0xd761000bu,
    0x02002809u, 0x8009ff0au, 0x600d0000u, 0xbeea1f00u, 0x806a946au, 0x826b806bu, 0xbefd216au, 0xbe8903ffu,
    0x00000badu, 0xd761000cu, 0x02002809u, 0xbe890380u, 0xbe950383u, 0x80090a09u, 0x80958115u, 0xbf068015u,
    0xbf850004u, 0xbe901f00u, 0x80909410u, 0x82918011u, 0xbefd2110u, 0xd761000du, 0x02002809u, 0x80148114u,
    0xbf0aa014u, 0xbf85ffabu, 0xe0781000u, 0x80010a01u, 0xbf810000u, 0x00000110u, 0x00000104u, 0x000000f8u,
    0x000000ecu, 0x000000acu, 0x000000a0u, 0x00000094u, 0x00000088u,
};

alignas(256) constexpr std::array<std::uint32_t, 109> Wave64Code{
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e1402ffu, 0xdeadbeefu, 0x7e1602ffu, 0xdeadbeefu,
    0x7e1802ffu, 0xdeadbeefu, 0x7e1a02ffu, 0xdeadbeefu, 0xbe940380u, 0xd7600008u, 0x02002904u, 0xd760000au,
    0x02002905u, 0xbe8c1f00u, 0x800cff0cu, 0x0000014cu, 0x820d800du, 0xbe89030au, 0x83928308u, 0xd4c40016u,
    0x02001104u, 0x8f128212u, 0xf4000386u, 0x24000000u, 0xbf8cc07fu, 0x7d820a0au, 0x808c0e0cu, 0x828d800du,
    0xbefd210cu, 0x8009ff0au, 0x11110000u, 0xbf820008u, 0x8009ff0au, 0x22220000u, 0xbf820005u, 0x8009ff0au,
    0x33330000u, 0xbf820002u, 0x8009ff0au, 0x44440000u, 0xd761000au, 0x02002809u, 0x816ac108u, 0x83ea836au,
    0x8f6a826au, 0xbe8e1f00u, 0x800eff0eu, 0x000000d4u, 0x820f800fu, 0xf4000487u, 0xd4000008u, 0x8aea167eu,
    0xbf8cc07fu, 0x808e120eu, 0x828f800fu, 0xbefd210eu, 0x8009ff0au, 0x0a000000u, 0xbf820008u, 0x8009ff0au,
    0x0b000000u, 0xbf820005u, 0x8009ff0au, 0x0c000000u, 0xbf820002u, 0x8009ff0au, 0x0d000000u, 0xd761000bu,
    0x02002809u, 0x8009ff0au, 0x600d0000u, 0xbeea1f00u, 0x806a946au, 0x826b806bu, 0xbefd216au, 0xbe8903ffu,
    0x00000badu, 0xd761000cu, 0x02002809u, 0xbe890380u, 0xbe950383u, 0x80090a09u, 0x80958115u, 0xbf068015u,
    0xbf850004u, 0xbe901f00u, 0x80909410u, 0x82918011u, 0xbefd2110u, 0xd761000du, 0x02002809u, 0x80148114u,
    0xbf0ac014u, 0xbf85ffabu, 0xe0781000u, 0x80010a01u, 0xbf810000u, 0x00000110u, 0x00000104u, 0x000000f8u,
    0x000000ecu, 0x000000acu, 0x000000a0u, 0x00000094u, 0x00000088u,
};

constexpr std::array<std::uint32_t, 8> Selectors{0u, 1u, 2u, 3u, 4u, 7u, 0x80000000u, 0xffffffffu};
constexpr std::array<std::uint32_t, 4> GetpcFirstCases{0x11110000u, 0x22220000u, 0x33330000u, 0x44440000u};
constexpr std::array<std::uint32_t, 4> IndexFirstCases{0x0a000000u, 0x0b000000u, 0x0c000000u, 0x0d000000u};
constexpr const char* Names[Results] = {"dword_table_getpc_first", "dword_table_index_first", "long_branch_vcc", "long_branch_back"};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Expected(std::uint32_t lane, std::uint32_t result) {
    const auto selector = Input[lane * Inputs];
    const auto value = Input[lane * Inputs + 1u];
    switch (result) {
        case 0: return value + GetpcFirstCases[std::min(selector, 3u)];
        case 1: return value + IndexFirstCases[std::min(selector - 1u, 3u)];
        case 2: return value + 0x600d0000u;
        default: return value * 3u;
    }
}

ShaderRecompiler::RecompileResult Compile(std::span<const std::uint32_t> code, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target) {
    for (std::uint32_t tid = 0; tid < MaxThreads; ++tid) {
        Input[tid * Inputs] = Selectors[tid % Selectors.size()];
        Input[tid * Inputs + 1u] = 0x9e3779b9u * (tid + 1u);
    }
    Output.fill(0xdeadbeefu);
    const auto result = Compile(code, waveSize, target);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(std::uint32_t lanes) {
    for (std::uint32_t tid = 0; tid < lanes; ++tid) {
        for (std::uint32_t index = 0; index < Results; ++index) {
            const auto actual = Output[tid * Results + index];
            const auto expected = Expected(tid, index);
            Testing::Require(actual == expected, "swappc wave" + std::to_string(lanes) + ": lane " + std::to_string(tid) + " " + Names[index] + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

const Testing::Case wave32{"ScalarSwappc_Wave32_JumpTablesAndLongBranchesMatchReference", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, Wave32Code, 32, device.ComputeTarget(32));
    Check(32);
}};

const Testing::Case wave64{"ScalarSwappc_Wave64_JumpTablesAndLongBranchesMatchReference", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, Wave64Code, 64, device.Target());
    Check(64);
}};

const Testing::Case wave64OnWave32Target{"ScalarSwappc_Wave64OnWave32Target_JumpTablesAndLongBranchesMatchReference", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, Wave64Code, 64, device.ComputeTarget(32));
    Check(64);
}};

} // namespace
