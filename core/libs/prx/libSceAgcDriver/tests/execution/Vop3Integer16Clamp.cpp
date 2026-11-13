#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Checked = 8;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 47> Code{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0x7e160280u, 0x7e180280u,
    0x7e1a0280u, 0x7e1c0280u, 0x7e1e0280u, 0x7e200280u, 0x7e220280u, 0x7e240280u, 0x7e260280u, 0x7e280280u,
    0x7e2a0280u, 0x7e2c0280u, 0x7e2e0280u, 0x7e300280u, 0x7e320280u,
    0xd705800au, 0x00020b04u,
    0xd707800bu, 0x00020b04u,
    0xd708800cu, 0x00020b04u,
    0xd709800du, 0x00020b04u,
    0xd70a800eu, 0x00020b04u,
    0xd70b800fu, 0x00020b04u,
    0xd70c8010u, 0x00020b04u,
    0xd7148011u, 0x00020b04u,
    0xe0781000u, 0x80010a02u,
    0xe0781010u, 0x80010e02u,
    0xe0781020u, 0x80011202u,
    0xe0781030u, 0x80011602u,
    0xbf8c3f70u, 0xbf810000u,
};

constexpr std::uint32_t Rows[32][4] = {
    {0xffffffffu, 0x00000001u, 0x00000000u, 0x00000000u},
    {0xfffffffeu, 0x00000001u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u},
    {0x7fffffffu, 0x80000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000005u, 0x00000005u, 0x00000000u, 0x00000000u},
    {0x12345678u, 0xfedcba98u, 0x00000000u, 0x00000000u},
    {0xfedcba98u, 0x12345678u, 0x00000000u, 0x00000000u},
    {0x00007fffu, 0x00000001u, 0x00000000u, 0x00000000u},
    {0x00008000u, 0x0000ffffu, 0x00000000u, 0x00000000u},
    {0x00008000u, 0x00000001u, 0x00000000u, 0x00000000u},
    {0x0000ffffu, 0x00000001u, 0x00000000u, 0x00000000u},
    {0x0000ffffu, 0x0000ffffu, 0x00000000u, 0x00000000u},
    {0x00007fffu, 0x00007fffu, 0x00000000u, 0x00000000u},
    {0x00008000u, 0x00008000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00008000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00007fffu, 0x00000000u, 0x00000000u},
    {0x1234ffffu, 0xabcd0001u, 0x00000000u, 0x00000000u},
    {0xabcd7fffu, 0x12340001u, 0x00000000u, 0x00000000u},
    {0xffff8000u, 0x00017fffu, 0x00000000u, 0x00000000u},
    {0x00010000u, 0x00000001u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x00010000u, 0x00000000u, 0x00000000u},
    {0x0000fffeu, 0x0000ffffu, 0x00000000u, 0x00000000u},
    {0x00007ffeu, 0x0000fffeu, 0x00000000u, 0x00000000u},
    {0x00008001u, 0x00000002u, 0x00000000u, 0x00000000u},
    {0x00000002u, 0x00008001u, 0x00000000u, 0x00000000u},
    {0xffffffffu, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x7fff8000u, 0x8000ffffu, 0x00000000u, 0x00000000u},
    {0x40000000u, 0xc0000000u, 0x00000000u, 0x00000000u}
};

constexpr std::uint32_t Expected[32][Checked] = {
    {0x0000ffffu, 0x00000000u, 0x00000000u, 0x0000ffffu, 0x00000001u, 0x00000001u, 0x0000ffffu, 0x00008000u},
    {0x0000fffeu, 0x00000000u, 0x00000000u, 0x0000fffeu, 0x00000001u, 0x00000001u, 0x0000fffeu, 0x00004000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x0000ffffu, 0x00000000u, 0x00000000u, 0x0000ffffu, 0x00000000u},
    {0x00000000u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000019u, 0x00000000u, 0x00000000u, 0x00000005u, 0x00000005u, 0x00000005u, 0x00000005u, 0x000000a0u},
    {0x00008740u, 0x000000bau, 0x0000ffbau, 0x0000ba98u, 0x00005678u, 0x00005678u, 0x0000ba98u, 0x00009800u},
    {0x00008740u, 0x00000056u, 0x00000056u, 0x0000ba98u, 0x00005678u, 0x00005678u, 0x0000ba98u, 0x00007800u},
    {0x00007fffu, 0x00000000u, 0x00000000u, 0x00007fffu, 0x00007fffu, 0x00000001u, 0x00000001u, 0x00008000u},
    {0x00008000u, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x00008000u, 0x00008000u, 0x0000ffffu},
    {0x00008000u, 0x00000001u, 0x00000001u, 0x00008000u, 0x00000001u, 0x00000001u, 0x00008000u, 0x00000001u},
    {0x0000ffffu, 0x00000000u, 0x00000000u, 0x0000ffffu, 0x00000001u, 0x00000001u, 0x0000ffffu, 0x00008000u},
    {0x00000001u, 0x00000001u, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x00008000u},
    {0x00000001u, 0x00000000u, 0x00000000u, 0x00007fffu, 0x00007fffu, 0x00007fffu, 0x00007fffu, 0x00008000u},
    {0x00000000u, 0x00008000u, 0x00008000u, 0x00008000u, 0x00008000u, 0x00008000u, 0x00008000u, 0x00008000u},
    {0x00000000u, 0x00008000u, 0x00008000u, 0x00008000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x00008000u},
    {0x00000000u, 0x00007fffu, 0x00007fffu, 0x00007fffu, 0x00007fffu, 0x00000000u, 0x00000000u, 0x00007fffu},
    {0x0000ffffu, 0x00000000u, 0x00000000u, 0x0000ffffu, 0x00000001u, 0x00000001u, 0x0000ffffu, 0x00008000u},
    {0x00007fffu, 0x00000000u, 0x00000000u, 0x00007fffu, 0x00007fffu, 0x00000001u, 0x00000001u, 0x00008000u},
    {0x00008000u, 0x00007fffu, 0x00007fffu, 0x00008000u, 0x00007fffu, 0x00007fffu, 0x00008000u, 0x00007fffu},
    {0x00000000u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000002u, 0x00000003u, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x0000fffeu, 0x0000fffeu, 0x0000c000u},
    {0x00000004u, 0x00000003u, 0x0000ffffu, 0x0000fffeu, 0x00007ffeu, 0x00007ffeu, 0x0000fffeu, 0x00008000u},
    {0x00000002u, 0x00000001u, 0x00000001u, 0x00008001u, 0x00000002u, 0x00000002u, 0x00008001u, 0x00000004u},
    {0x00000002u, 0x00002000u, 0x0000e000u, 0x00008001u, 0x00000002u, 0x00000002u, 0x00008001u, 0x00000004u},
    {0x00000001u, 0x00000001u, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x00008000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x00000000u, 0x00000000u, 0x0000ffffu, 0x0000ffffu},
    {0x00008000u, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x0000ffffu, 0x00008000u, 0x00008000u, 0x0000ffffu},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}
};

constexpr const char* Names[Checked] = {
    "v_mul_lo_u16 v10, v4, v5 clamp",
    "v_lshrrev_b16 v11, v4, v5 clamp",
    "v_ashrrev_i16 v12, v4, v5 clamp",
    "v_max_u16 v13, v4, v5 clamp",
    "v_max_i16 v14, v4, v5 clamp",
    "v_min_u16 v15, v4, v5 clamp",
    "v_min_i16 v16, v4, v5 clamp",
    "v_lshlrev_b16 v17, v4, v5 clamp",
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
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

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            for (std::uint32_t index = 0; index < Checked; ++index) {
                const std::uint32_t actual = Output[tid * Results + index];
                Require(actual == Expected[tid][index], std::string("VOP3 16-bit integer clamp: lane ") + std::to_string(tid) + " " + Names[index] + " of " + Hex(Rows[tid][0]) + " and " +
                                                            Hex(Rows[tid][1]) + " is " + Hex(actual) + ", expected " + Hex(Expected[tid][index]));
            }
        }
        std::puts("VOP3 16-bit integer clamp tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
