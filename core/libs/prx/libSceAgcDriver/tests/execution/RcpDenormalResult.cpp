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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Results = 4;

constexpr std::array<std::array<std::uint32_t, 4>, Threads> Measured{{
    {0x7e800001u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x7ea00000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x7ec00000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x7effffffu, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x7f000000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x7f000001u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x7f200000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x7f400000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x7f7fffffu, 0x00000000u, 0x00000000u, 0x80000000u},
    {0xfe800001u, 0x80000000u, 0x80000000u, 0x00000000u},
    {0xfec00000u, 0x80000000u, 0x80000000u, 0x00000000u},
    {0xff000000u, 0x80000000u, 0x80000000u, 0x00000000u},
    {0xff7fffffu, 0x80000000u, 0x80000000u, 0x00000000u},
    {0x7f123456u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0xfe9abcdeu, 0x80000000u, 0x80000000u, 0x00000000u},
    {0x7efedcbau, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x7e800000u, 0x00800000u, 0x00800000u, 0x80800000u},
    {0xfe800000u, 0x80800000u, 0x80800000u, 0x00800000u},
    {0x7e000000u, 0x01000000u, 0x01000000u, 0x81000000u},
    {0x7e000001u, 0x00fffffeu, 0x00fffffeu, 0x80fffffeu},
    {0x3f800000u, 0x3f800000u, 0x3f800000u, 0xbf800000u},
    {0x40000000u, 0x3f000000u, 0x3f000000u, 0xbf000000u},
    {0xbf800000u, 0xbf800000u, 0xbf800000u, 0x3f800000u},
    {0x00800000u, 0x7e800000u, 0x7e800000u, 0xfe800000u},
    {0x7f800000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0xff800000u, 0x80000000u, 0x80000000u, 0x00000000u},
    {0x00000000u, 0x7f800000u, 0x7f800000u, 0xff800000u},
    {0x80000000u, 0xff800000u, 0xff800000u, 0x7f800000u},
    {0x00000001u, 0x7f800000u, 0x7f800000u, 0xff800000u},
    {0x80400000u, 0xff800000u, 0xff800000u, 0x7f800000u},
    {0x01000000u, 0x7e000000u, 0x7e000000u, 0xfe000000u},
    {0x7e7fffffu, 0x00800001u, 0x00800001u, 0x80800001u},
}};

constexpr const char* Names[3] = {"v_rcp_f32", "v_rcp_iflag_f32", "v_rcp_f32 of a negated source"};

alignas(256) std::array<std::uint32_t, Threads> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 13> Code{
    0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x7e065502u, 0x7e085702u, 0xd5aa0005u, 0x20000102u,
    0x340c0084u, 0xe07c1000u, 0x80010306u, 0xbf8c3f70u, 0xbf810000u,
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

void Run(AgcDriver::VulkanDevice& device, std::uint32_t floatMode) {
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
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{floatMode, true, true, false};
    const auto result = ShaderRecompiler::Recompile(request);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Input[tid] = Measured[tid][0];
    Output.fill(0xdeadbeefu);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t op = 0; op < 3u; ++op) {
            const auto got = Output[tid * Results + op];
            const auto wanted = Measured[tid][op + 1u];
            Require(got == wanted, std::string(Names[op]) + " of " + Hex(Measured[tid][0]) + " with FLOAT_MODE " + Hex(floatMode) +
                                       " is " + Hex(got) + ", expected the RDNA 2 value " + Hex(wanted));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, 0xf0u);
        Run(*device, 0xc0u);
        std::puts("rcp denormal result tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
