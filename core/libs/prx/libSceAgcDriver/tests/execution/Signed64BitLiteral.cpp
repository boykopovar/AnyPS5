#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 54> Code{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd4a10008u, 0x000100ffu, 0x80000000u,
    0xd501000au, 0x00210280u, 0xd4a40008u, 0x0001fe80u, 0xff00ff00u, 0xd501000bu, 0x00210280u, 0xd4a20008u,
    0x000208ffu, 0xfffffffbu, 0xd501000cu, 0x00210280u, 0xbe8918ffu, 0x80000000u, 0x7e1a0209u, 0xd701000eu,
    0x0001fe84u, 0x80000000u, 0xd7000010u, 0x0001fe84u, 0x80000000u, 0xd5770a12u, 0x03fd0481u, 0xfffffffbu,
    0xd5760a14u, 0x03fd0481u, 0xfffffffbu, 0x918c84ffu, 0x80000000u, 0x7e2c020cu, 0x7e2e020du, 0xbe9003ffu,
    0x00280010u, 0x950e10ffu, 0x80000000u, 0x7e30020eu, 0x7e32020fu, 0xe0781000u, 0x80010a02u, 0xe0781010u,
    0x80010e02u, 0xe0781020u, 0x80011202u, 0xe0781030u, 0x80011602u, 0xbf810000u,
};

constexpr std::array<const char*, Results> Names{
    "v_cmp_lt_i64 0x80000000, 0",
    "v_cmp_gt_i64 0, 0xff00ff00",
    "v_cmp_eq_i64 0xfffffffb, -5",
    "s_flbit_i32_i64 0x80000000",
    "v_ashrrev_i64 4, 0x80000000 (low)",
    "v_ashrrev_i64 4, 0x80000000 (high)",
    "v_lshrrev_b64 4, 0x80000000 (low)",
    "v_lshrrev_b64 4, 0x80000000 (high)",
    "v_mad_i64_i32 1, 2, 0xfffffffb (low)",
    "v_mad_i64_i32 1, 2, 0xfffffffb (high)",
    "v_mad_u64_u32 1, 2, 0xfffffffb (low)",
    "v_mad_u64_u32 1, 2, 0xfffffffb (high)",
    "s_ashr_i64 0x80000000, 4 (low)",
    "s_ashr_i64 0x80000000, 4 (high)",
    "s_bfe_i64 0x80000000, offset 16 width 40 (low)",
    "s_bfe_i64 0x80000000, offset 16 width 40 (high)",
};

constexpr std::array<std::uint32_t, Results> Expected{
    0x00000001u, 0x00000001u, 0x00000001u, 0x00000021u, 0xf8000000u, 0xffffffffu, 0x08000000u, 0x00000000u,
    0xfffffffdu, 0xffffffffu, 0xfffffffdu, 0x00000000u, 0xf8000000u, 0xffffffffu, 0xffff8000u, 0xffffffffu,
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
    Input.fill(0u);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Input[lane * Inputs] = 0xfffffffbu;
        Input[lane * Inputs + 1u] = 0xffffffffu;
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
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

void Check() {
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        for (std::uint32_t column = 0; column < Results; ++column) {
            const auto value = Output[lane * Results + column];
            Require(value == Expected[column], std::string(Names[column]) + " in thread " + std::to_string(lane) + " is " + Hex(value) + ", expected " + Hex(Expected[column]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        std::puts("signed 64-bit literal tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
