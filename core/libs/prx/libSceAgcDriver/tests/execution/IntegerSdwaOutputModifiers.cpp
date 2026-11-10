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
constexpr std::uint32_t Rows = 16;
constexpr std::uint32_t Cases = 16;
constexpr std::uint32_t Preset = 0xa5a5a5a5u;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 53> Code{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e1402f9u, 0x00066604u, 0x7e160307u, 0x7e1602f9u,
    0x0006d504u, 0x7e186ef9u, 0x00062604u, 0x7e1a70f9u, 0x00068604u, 0x7e1c72f9u, 0x00066604u, 0x7e1e74f9u, 0x0006c604u,
    0x7e2076f9u, 0x0006a604u, 0xbeea03ffu, 0x96a596a5u, 0x02220af9u, 0x06062604u, 0xbeea03ffu, 0x96a596a5u, 0x02240af9u,
    0x05054604u, 0x4a260af9u, 0x06066604u, 0x4c280af9u, 0x0d0de604u, 0x162a0af9u, 0x0606a604u, 0x122c0af9u, 0x06066604u,
    0x3a2e0af9u, 0x06064604u, 0x24300af9u, 0x0606e404u, 0xbeea03ffu, 0x96a596a5u, 0x50320af9u, 0x0606a604u, 0xe0781000u,
    0x80010a03u, 0xe0781010u, 0x80010e03u, 0xe0781020u, 0x80011203u, 0xe0781030u, 0x80011603u, 0xbf810000u,
};

constexpr std::uint32_t Expected[Rows][Cases] = {
    {0x00000000u, 0x0000a5a5u, 0xffffffffu, 0x00000000u, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0x00000005u,
     0x00000000u, 0x00000005u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000005u, 0x00000005u, 0x00000006u},
    {0x00000001u, 0x0001a5a5u, 0xfffffffeu, 0x80000000u, 0x0000001fu, 0x00000000u, 0x0000001fu, 0x00000001u,
     0x00000000u, 0xffffffffu, 0x00000000u, 0x00ffffffu, 0xffffffffu, 0xfffffffeu, 0x00000001u, 0xffffffffu},
    {0xffffffffu, 0xffffa5a5u, 0x00000000u, 0xffffffffu, 0x00000000u, 0x00000000u, 0xffffffffu, 0x00000001u,
     0x00000000u, 0xffffffffu, 0xffffffffu, 0x00ffffffu, 0xffffffffu, 0xfffffffeu, 0x00000001u, 0xffffffffu},
    {0x80000000u, 0x0000a5a5u, 0x7fffffffu, 0x00000001u, 0x00000000u, 0x0000001fu, 0x00000001u, 0x80000000u,
     0x00008000u, 0x80000003u, 0xffff8000u, 0x00000000u, 0x00000000u, 0x80000003u, 0x00000003u, 0x80000003u},
    {0x7fffffffu, 0xffffa5a5u, 0x80000000u, 0xfffffffeu, 0x00000001u, 0x00000000u, 0x00000001u, 0x7fffffffu,
     0x00007fffu, 0xffffffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0x0000ffffu, 0xffffffffu},
    {0x00008001u, 0x8001a5a5u, 0xffff7ffeu, 0x80010000u, 0x00000010u, 0x00000000u, 0x00000010u, 0x7fffffffu,
     0x00007fffu, 0x80008000u, 0x00000000u, 0xffffffffu, 0xffff7fffu, 0x7fff7ffeu, 0x0000ffffu, 0x80008001u},
    {0xffff7fffu, 0x7fffa5a5u, 0x00008000u, 0xfffeffffu, 0x00000000u, 0x00000000u, 0x00000010u, 0xffff7fffu,
     0x0000ffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xbffeffffu, 0xfffffffeu, 0x00008001u, 0xffffffffu},
    {0x12345678u, 0x5678a5a5u, 0xedcba987u, 0x1e6a2c48u, 0x00000003u, 0x00000003u, 0x00000003u, 0x12345678u,
     0x00001234u, 0x2468acf0u, 0x00000000u, 0xffffffffu, 0x7fffffffu, 0x00000000u, 0x00005678u, 0x2468acf1u},
    {0x00000080u, 0x0080a5a5u, 0xffffff7fu, 0x01000000u, 0x00000018u, 0x00000007u, 0x00000018u, 0x00000080u,
     0x00000000u, 0xffff807fu, 0x00000000u, 0x7fbfff80u, 0xffbfff80u, 0xffff7f7fu, 0x00000080u, 0xffff807fu},
    {0x0000ff80u, 0xff80a5a5u, 0xffff007fu, 0x01ff0000u, 0x00000010u, 0x00000007u, 0x00000010u, 0x00000000u,
     0x00000000u, 0x0000ff80u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000ff80u, 0x0000ff80u, 0x0000ff81u},
    {0x80008000u, 0x8000a5a5u, 0x7fff7fffu, 0x00010001u, 0x00000000u, 0x0000000fu, 0x00000001u, 0x80008000u,
     0x00008000u, 0xffffffffu, 0x00000000u, 0x40000000u, 0x40000000u, 0x00000000u, 0x00008000u, 0xffffffffu},
    {0xfffffffeu, 0xfffea5a5u, 0x00000001u, 0x7fffffffu, 0x00000000u, 0x00000001u, 0x0000001fu, 0xfffffffeu,
     0x0000ffffu, 0xffffffffu, 0xffffffffu, 0x0fffffe0u, 0xffffffe0u, 0xffffffeeu, 0x00000010u, 0xffffffffu},
    {0x3f800000u, 0x0000a5a5u, 0xc07fffffu, 0x000001fcu, 0x00000002u, 0x00000017u, 0x00000002u, 0xbf800000u,
     0x0000bf80u, 0xff000000u, 0x00000000u, 0xffffffffu, 0x7fffffffu, 0x80000000u, 0x00000000u, 0xff000001u},
    {0x00800000u, 0x0000a5a5u, 0xff7fffffu, 0x00000100u, 0x00000008u, 0x00000017u, 0x00000008u, 0x00800000u,
     0x00000080u, 0x7fffffffu, 0x00000000u, 0xffffffffu, 0x80000000u, 0x7fffffffu, 0x0000ffffu, 0x7fffffffu},
    {0xc0000000u, 0x0000a5a5u, 0x3fffffffu, 0x00000003u, 0x00000000u, 0x0000001eu, 0x00000002u, 0xc0000000u,
     0x0000c000u, 0xc0000002u, 0xffffc000u, 0x00000000u, 0x00000000u, 0xc0000002u, 0x00000002u, 0xc0000002u},
    {0x0000001fu, 0x001fa5a5u, 0xffffffe0u, 0xf8000000u, 0x0000001bu, 0x00000000u, 0x0000001bu, 0xffffff80u,
     0x0000ffffu, 0xffffff9fu, 0x00000000u, 0x1efff080u, 0xfffff080u, 0xffffff9fu, 0x0000001fu, 0xffffffa0u},
};

constexpr const char* Names[Cases] = {
    "v_mov_b32 clamp mul:2", "v_mov_b32 div:2 WORD_1 preserve", "v_not_b32 clamp", "v_bfrev_b32 mul:4",
    "v_ffbh_u32 clamp mul:2", "v_ffbl_b32 div:2", "v_ffbh_i32 clamp mul:4", "v_cndmask_b32 clamp",
    "v_cndmask_b32 mul:2 WORD_1", "v_add_nc_u32 clamp mul:2", "v_sub_nc_u32 clamp div:2 sext(WORD_1)", "v_mul_u32_u24 clamp mul:4",
    "v_mul_i32_i24 clamp mul:2", "v_xor_b32 mul:2", "v_max_i32 clamp div:2 WORD_0", "v_add_co_ci_u32 clamp mul:4",
};

constexpr std::uint32_t Sources[Rows][2] = {
    {0x00000000u, 0x00000005u},
    {0x00000001u, 0xffffffffu},
    {0xffffffffu, 0x00000001u},
    {0x80000000u, 0x00000003u},
    {0x7fffffffu, 0x80000000u},
    {0x00008001u, 0x7fffffffu},
    {0xffff7fffu, 0x00008001u},
    {0x12345678u, 0x12345678u},
    {0x00000080u, 0xffff7fffu},
    {0x0000ff80u, 0x00000000u},
    {0x80008000u, 0x80008000u},
    {0xfffffffeu, 0x00000010u},
    {0x3f800000u, 0xbf800000u},
    {0x00800000u, 0x7f7fffffu},
    {0xc0000000u, 0x00000002u},
    {0x0000001fu, 0xffffff80u},
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
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0x00u, true, false, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * Inputs] = Sources[tid % Rows][0];
        Input[tid * Inputs + 1u] = Sources[tid % Rows][1];
        Input[tid * Inputs + 2u] = 0u;
        Input[tid * Inputs + 3u] = Preset;
    }
    Run(device);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t row = tid % Rows;
        for (std::uint32_t column = 0; column < Cases; ++column) {
            const std::uint32_t actual = Output[tid * Results + column];
            Require(actual == Expected[row][column], std::string("integer sdwa output modifiers: lane ") + std::to_string(tid) + " " + Names[column] + " of " +
                Hex(Sources[row][0]) + ", " + Hex(Sources[row][1]) + " is " + Hex(actual) + ", expected " + Hex(Expected[row][column]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Check(*device);
        std::puts("integer sdwa output modifier tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
