#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Literal = 0x80000000u;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 73> CarryOutCode{
    0x34020082, 0x34060084, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xbf8c3f70, 0xd70f080a,
    0x00020b04, 0xd501000b, 0x00210280, 0xd710080c, 0x00020b04, 0xd501000d, 0x00210280, 0xd719080e,
    0x00020b04, 0xd501000f, 0x00210280, 0xd70f6a10, 0x00020b04, 0xd5010011, 0x01a90280, 0xd7106a12,
    0x00020b04, 0xd5010013, 0x01a90280, 0xd7196a14, 0x00020b04, 0xd5010015, 0x01a90280, 0xd7100a16,
    0x000208ff, Literal,    0xd5010017, 0x00290280, 0xd7196a18, 0x00020a81, 0xd5010019, 0x01a90280,
    0xe0702000, 0x80010a03, 0xe0702004, 0x80010b03, 0xe0702008, 0x80010c03, 0xe070200c, 0x80010d03,
    0xe0702010, 0x80010e03, 0xe0702014, 0x80010f03, 0xe0702018, 0x80011003, 0xe070201c, 0x80011103,
    0xe0702020, 0x80011203, 0xe0702024, 0x80011303, 0xe0702028, 0x80011403, 0xe070202c, 0x80011503,
    0xe0702030, 0x80011603, 0xe0702034, 0x80011703, 0xe0702038, 0x80011803, 0xe070203c, 0x80011903,
    0xbf810000,
};

constexpr std::array<std::array<std::uint32_t, 8>, 14> Edges{{
    {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u},
    {0u, 1u, 1u, 0u, 0xffffffffu, 1u, 1u, 0u},
    {1u, 0u, 1u, 0u, 1u, 0u, 0xffffffffu, 1u},
    {0xffffffffu, 1u, 0u, 1u, 0xfffffffeu, 0u, 2u, 1u},
    {1u, 0xffffffffu, 0u, 1u, 2u, 1u, 0xfffffffeu, 0u},
    {0xffffffffu, 0xffffffffu, 0xfffffffeu, 1u, 0u, 0u, 0u, 0u},
    {0x80000000u, 0x80000000u, 0u, 1u, 0u, 0u, 0u, 0u},
    {0x7fffffffu, 1u, 0x80000000u, 0u, 0x7ffffffeu, 0u, 0x80000002u, 1u},
    {0x80000000u, 0x7fffffffu, 0xffffffffu, 0u, 1u, 0u, 0xffffffffu, 1u},
    {0x7fffffffu, 0x80000000u, 0xffffffffu, 0u, 0xffffffffu, 1u, 1u, 0u},
    {0u, 0xffffffffu, 0xffffffffu, 0u, 1u, 1u, 0xffffffffu, 0u},
    {0xffffffffu, 0u, 0xffffffffu, 0u, 0xffffffffu, 0u, 1u, 1u},
    {0x80000001u, 0x80000000u, 1u, 1u, 1u, 0u, 0xffffffffu, 1u},
    {0x12345678u, 0x9abcdef0u, 0xacf13568u, 0u, 0x77777788u, 1u, 0x88888878u, 0u},
}};

void FillInput() {
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        for (std::uint32_t j = 0; j < 2u; ++j) {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            words[j] = tid < Edges.size() ? Edges[tid][j] : static_cast<std::uint32_t>(state >> 32u);
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(CarryOutCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::array<std::uint32_t, 2> Add(std::uint32_t lhs, std::uint32_t rhs) {
    const std::uint64_t sum = static_cast<std::uint64_t>(lhs) + rhs;
    return {static_cast<std::uint32_t>(sum), static_cast<std::uint32_t>(sum >> 32u)};
}

std::array<std::uint32_t, 2> Sub(std::uint32_t lhs, std::uint32_t rhs) {
    return {lhs - rhs, rhs > lhs ? 1u : 0u};
}

void Check(std::uint32_t waveSize) {
    constexpr std::array<const char*, 8> names{
        "v_add_co_u32 into an sgpr", "v_sub_co_u32 into an sgpr", "v_subrev_co_u32 into an sgpr", "v_add_co_u32 into vcc",
        "v_sub_co_u32 into vcc", "v_subrev_co_u32 into vcc", "v_sub_co_u32 from a literal", "v_subrev_co_u32 of an inline constant",
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t a = Input[tid * Inputs];
        const std::uint32_t b = Input[tid * Inputs + 1u];
        const bool edge = tid < Edges.size();
        const std::array<std::array<std::uint32_t, 2>, 8> expected{{
            edge ? std::array<std::uint32_t, 2>{Edges[tid][2], Edges[tid][3]} : Add(a, b),
            edge ? std::array<std::uint32_t, 2>{Edges[tid][4], Edges[tid][5]} : Sub(a, b),
            edge ? std::array<std::uint32_t, 2>{Edges[tid][6], Edges[tid][7]} : Sub(b, a),
            Add(a, b),
            Sub(a, b),
            Sub(b, a),
            Sub(Literal, a),
            Sub(b, 1u),
        }};
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            const std::string what = "wave" + std::to_string(waveSize) + " " + names[j] + ": thread " + std::to_string(tid) + " (" + Hex(a) + ", " + Hex(b) + ")";
            const std::uint32_t value = Output[tid * Results + j * 2u];
            const std::uint32_t carry = Output[tid * Results + j * 2u + 1u];
            Require(value == expected[j][0], what + " is " + Hex(value) + ", expected " + Hex(expected[j][0]));
            Require(carry == expected[j][1], what + " carries " + std::to_string(carry) + ", expected " + std::to_string(expected[j][1]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        for (const std::uint32_t waveSize : {32u, 64u}) {
            Run(*device, waveSize);
            Check(waveSize);
        }
        std::puts("vop3 carry out tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
