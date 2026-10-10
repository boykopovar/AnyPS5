#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 8;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 51> BitCountCode{
    0x34020082, 0x34060083, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xbf8c3f70, 0xbe940380,
    0xd7600008, 0x00002904, 0xd7600009, 0x00002905, 0xbe8a0e08, 0x850f8081, 0xbe8b1208, 0xbe8c1708,
    0xbe8d1808, 0xbe8e2c08, 0x85108081, 0xd761000a, 0x0000280a, 0xd761000b, 0x0000280b, 0xd761000c,
    0x0000280c, 0xd761000d, 0x0000280d, 0xd761000e, 0x0000280e, 0xd761000f, 0x0000280f, 0xd7610010,
    0x00002810, 0x80148114, 0xbf0aa014, 0xbf85ffe4, 0xe0702000, 0x80010a03, 0xe0702004, 0x80010b03,
    0xe0702008, 0x80010c03, 0xe070200c, 0x80010d03, 0xe0702010, 0x80010e03, 0xe0702014, 0x80010f03,
    0xe0702018, 0x80011003, 0xbf810000,
};

constexpr std::array<std::uint64_t, 10> Edges{
    0u, ~0ull, 0xffffffff00000000ull, 0x00000000ffffffffull, 1u, 0x8000000000000000ull,
    0x7fffffffffffffffull, 0x00000000f0000000ull, 0xfffffffe7fffffffull, 0x0000000100000000ull,
};

void FillInput() {
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        const std::uint64_t value = tid < Edges.size() ? Edges[tid] : state >> (tid % 13u);
        Input[tid * Inputs] = static_cast<std::uint32_t>(value);
        Input[tid * Inputs + 1] = static_cast<std::uint32_t>(value >> 32u);
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    FillInput();
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(BitCountCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::uint32_t SignedLeadingBits(std::uint64_t value, std::uint64_t sign, int width) {
    const std::uint64_t folded = (value & sign) != 0u ? ~value & (sign | (sign - 1u)) : value;
    return folded == 0u ? 0xffffffffu : static_cast<std::uint32_t>(std::countl_zero(folded) - (64 - width));
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint64_t value = Input[tid * Inputs] | (static_cast<std::uint64_t>(Input[tid * Inputs + 1]) << 32u);
        const auto low = static_cast<std::uint32_t>(value);
        std::uint32_t quads = 0;
        for (std::uint32_t quad = 0; quad < 8u; ++quad) quads |= ((low >> (quad * 4u)) & 0xfu) != 0u ? 1u << quad : 0u;
        const auto zeros = static_cast<std::uint32_t>(64 - std::popcount(value));
        const std::array<std::uint32_t, 7> expected{
            zeros,
            ~value == 0u ? 0xffffffffu : static_cast<std::uint32_t>(std::countr_one(value)),
            SignedLeadingBits(low, 0x80000000u, 32),
            SignedLeadingBits(value, 0x8000000000000000ull, 64),
            quads,
            zeros != 0u ? 1u : 0u,
            quads != 0u ? 1u : 0u,
        };
        const auto* out = &Output[tid * Results];
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            Testing::Require(out[j] == expected[j], "scalar bit count: lane " + std::to_string(tid) + " result " + std::to_string(j) + " is " + std::to_string(out[j]) + ", expected " + std::to_string(expected[j]));
        }
    }
}

const Testing::Case scalarBitCount{"ScalarBitCount_EdgeAndRandomInputs_MatchReference", [] {
    auto& device = SharedVulkanTestDevice();
    if (device.Target().subgroupSize < Threads) Testing::Skip("subgroup size " + std::to_string(device.Target().subgroupSize) + " cannot hold a wave32");
    Run(device);
    Check();
}};

} // namespace
