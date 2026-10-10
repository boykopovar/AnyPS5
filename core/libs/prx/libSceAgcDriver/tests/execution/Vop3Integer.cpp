#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 74> IntegerCode{
    0x34020082, 0x34060084, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0x7e1a02ff, 0xabcd0000, 0x7e1c02ff, 0xabcd0000, 0x7e1e02ff, 0xabcd0000, 0x7e2a02ff, 0xabcd0000,
    0x7e2c02ff, 0xabcd0000, 0xbf8c3f70, 0xd77f000a, 0x00020b04, 0xd77f800b, 0x00020b04, 0xd776800c,
    0x00020b04, 0xd705000d, 0x00020b04, 0xd740000e, 0x041a0b04, 0xd75e000f, 0x041a0b04, 0xd7730010,
    0x041a0b04, 0xd7750011, 0x041a0b04, 0xd7010012, 0x00020906, 0xd7731814, 0x041a0b04, 0xd7402815,
    0x041a0b04, 0xd7051016, 0x00020b04, 0xd7760017, 0x00020b04, 0xe0702000, 0x80010a03, 0xe0702004,
    0x80010b03, 0xe0702008, 0x80010c03, 0xe070200c, 0x80010d03, 0xe0702010, 0x80010e03, 0xe0702014,
    0x80010f03, 0xe0702018, 0x80011003, 0xe070201c, 0x80011103, 0xe0702020, 0x80011203, 0xe0702024,
    0x80011303, 0xe0702028, 0x80011403, 0xe070202c, 0x80011503, 0xe0702030, 0x80011603, 0xe0702034,
    0x80011703, 0xbf810000,
};

constexpr std::array<std::array<std::uint32_t, 3>, 10> Edges{{
    {0x7fffffffu, 1u, 0u}, {0x80000000u, 1u, 63u}, {0x80000000u, 0xffffffffu, 32u}, {0x7fffffffu, 0x80000000u, 31u},
    {0xffffffffu, 0xffffffffu, 0xffffu}, {0x8000ffffu, 0x7fff8000u, 0x12345678u}, {0x40000000u, 0x40000000u, 1u},
    {0xc0000000u, 0x40000001u, 64u}, {0xffff0001u, 0x0001ffffu, 0x8000ffffu}, {0u, 0x80000000u, 0x7fff7fffu},
}};

void FillInput() {
    std::uint64_t state = 0x2545f4914f6cdd1dull;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        for (std::uint32_t j = 0; j < 3u; ++j) {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            words[j] = tid < Edges.size() ? Edges[tid][j] : static_cast<std::uint32_t>(state >> 32u);
        }
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
    const std::span<const std::uint32_t> code(IntegerCode);
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

std::uint32_t Saturate(std::int64_t value) {
    constexpr std::int64_t low = std::numeric_limits<std::int32_t>::min();
    constexpr std::int64_t high = std::numeric_limits<std::int32_t>::max();
    return static_cast<std::uint32_t>(std::clamp(value, low, high));
}

std::uint32_t Signed16(std::uint32_t value) {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value)));
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto* in = &Input[tid * Inputs];
        const std::uint32_t a = in[0];
        const std::uint32_t b = in[1];
        const std::uint32_t c = in[2];
        const std::uint32_t al = a & 0xffffu;
        const std::uint32_t ah = a >> 16u;
        const std::uint32_t bl = b & 0xffffu;
        const std::uint32_t bh = b >> 16u;
        const std::int64_t sa = static_cast<std::int32_t>(a);
        const std::int64_t sb = static_cast<std::int32_t>(b);
        const auto shifted = static_cast<std::uint64_t>(static_cast<std::int64_t>(a | (static_cast<std::uint64_t>(b) << 32u)) >> (c & 63u));
        const std::array<std::uint32_t, 14> expected{
            a + b,
            Saturate(sa + sb),
            Saturate(sa - sb),
            (al * bl) & 0xffffu,
            (al * bl + (c & 0xffffu)) & 0xffffu,
            (Signed16(a) * Signed16(b) + Signed16(c)) & 0xffffu,
            al * bl + c,
            Signed16(a) * Signed16(b) + c,
            static_cast<std::uint32_t>(shifted),
            static_cast<std::uint32_t>(shifted >> 32u),
            ah * bh + c,
            (ah * bl + (c >> 16u)) & 0xffffu,
            (al * bh) & 0xffffu,
            a - b,
        };
        constexpr std::uint32_t halfResults = (1u << 3u) | (1u << 4u) | (1u << 5u) | (1u << 11u) | (1u << 12u);
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            const std::uint32_t mask = ((halfResults >> j) & 1u) != 0u ? 0xffffu : 0xffffffffu;
            const std::uint32_t actual = Output[tid * Results + j] & mask;
            Testing::Require(actual == expected[j], "vop3 integer: thread " + std::to_string(tid) + " result " + std::to_string(j) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[j]));
        }
    }
}

const Testing::Case vop3Integer{"Vop3Integer_AddSubMadAndShiftOps_MatchModel", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device);
    Check();
}};

} // namespace
