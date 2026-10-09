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
constexpr std::uint32_t Literal = 0x8000ffffu;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 51> SadCode{
    0x34020082, 0x34060084, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xbf8c3f70, 0xd55a000a, 0x041a0b04, 0xd55b000b, 0x041a0b04, 0xd55c000c, 0x041a0b04, 0xd571000d,
    0x041a0b04, 0xd55d000e, 0x041a0b04, 0xd55a000f, 0x04190104, 0xd55a0010, 0x041a0ac1, 0xd55b0011,
    0x042a0905, 0xd55c0012, 0x0419ff04, Literal,    0xd5710013, 0x041a0905, 0xe0702000, 0x80010a03,
    0xe0702004, 0x80010b03, 0xe0702008, 0x80010c03, 0xe070200c, 0x80010d03, 0xe0702010, 0x80010e03,
    0xe0702014, 0x80010f03, 0xe0702018, 0x80011003, 0xe070201c, 0x80011103, 0xe0702020, 0x80011203,
    0xe0702024, 0x80011303, 0xbf810000,
};

constexpr std::array<std::array<std::uint32_t, 3>, 12> Edges{{
    {0xffffffffu, 0u, 0u}, {0u, 0xffffffffu, 0xffffffffu}, {0xff00ff00u, 0x00ff00ffu, 0xfffffc04u}, {0x80000000u, 0x7fffffffu, 1u},
    {0x7f80ff00u, 0x807f00ffu, 0xffff0000u}, {0x01020304u, 0x04030201u, 0x10u}, {0x12345678u, 0x12345678u, 0x9abcdef0u},
    {0x80808080u, 0x7f7f7f7fu, 0u}, {0x8000ffffu, 0x7fff0000u, 0xfffe0002u}, {0x00ff0001u, 0x000000ffu, 0x7fffffffu},
    {0xffffffffu, 0x00ff0000u, 0x03fcu}, {0u, 0u, 0u},
}};

void FillInput() {
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        for (std::uint32_t j = 0; j < 3u; ++j) {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            words[j] = tid < Edges.size() ? Edges[tid][j] : static_cast<std::uint32_t>(state >> 32u);
        }
        if (tid >= Edges.size() && (tid & 1u) != 0u) {
            words[1] &= (tid & 2u) != 0u ? 0x00ffff00u : 0xff0000ffu;
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

auto Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    const std::span<const std::uint32_t> code(SadCode);
    const auto result = Compile(device, code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::uint32_t Sad(std::uint32_t lhs, std::uint32_t rhs, std::uint32_t fieldBits, bool masked) {
    const std::uint32_t fieldMask = (1u << fieldBits) - 1u;
    std::uint32_t sum = 0u;
    for (std::uint32_t offset = 0; offset < 32u; offset += fieldBits) {
        const std::uint32_t left = (lhs >> offset) & fieldMask;
        const std::uint32_t right = (rhs >> offset) & fieldMask;
        if (masked && right == 0u) continue;
        sum += left > right ? left - right : right - left;
    }
    return sum;
}

void Check() {
    constexpr std::array<const char*, 10> names{
        "v_sad_u8", "v_sad_hi_u8", "v_sad_u16", "v_msad_u8", "v_sad_u32", "v_sad_u8 with a zero reference", "v_sad_u8 with an inline constant",
        "v_sad_hi_u8 accumulating into a v_sad_u8 result", "v_sad_u16 with a literal", "v_msad_u8 with swapped sources",
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto* in = &Input[tid * Inputs];
        const std::uint32_t a = in[0];
        const std::uint32_t b = in[1];
        const std::uint32_t c = in[2];
        const std::uint32_t bytes = Sad(a, b, 8u, false) + c;
        const std::array<std::uint32_t, 10> expected{
            bytes,
            (Sad(a, b, 8u, false) << 16u) + c,
            Sad(a, b, 16u, false) + c,
            Sad(a, b, 8u, true) + c,
            (a > b ? a - b : b - a) + c,
            Sad(a, 0u, 8u, false) + c,
            Sad(0xffffffffu, b, 8u, false) + c,
            (Sad(b, a, 8u, false) << 16u) + bytes,
            Sad(a, Literal, 16u, false) + c,
            Sad(b, a, 8u, true) + c,
        };
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            const std::uint32_t actual = Output[tid * Results + j];
            Require(actual == expected[j], std::string(names[j]) + ": thread " + std::to_string(tid) + " (" + Hex(a) + ", " + Hex(b) + ", " + Hex(c) + ") is " + Hex(actual) + ", expected " + Hex(expected[j]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        Run(*device);
        Check();
        std::puts("vop3 sad tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
