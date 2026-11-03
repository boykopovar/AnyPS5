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

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 11> Code{
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e144104u, 0x7e164904u, 0x7e184504u, 0x7e1a8104u,
    0xe0781000u, 0x80010a01u, 0xbf810000u,
};

constexpr const char* Names[Results] = {"v_fract_f32", "v_floor_f32", "v_ceil_f32", "v_frexp_mant_f32"};

constexpr std::uint32_t Values[Threads] = {
    0x00000000u, 0x00000001u, 0x00000002u, 0x00000003u, 0x00400000u, 0x007ffffeu, 0x007fffffu, 0x00800000u,
    0x00800001u, 0x00ffffffu, 0x01000000u, 0x0c000000u, 0x33ffffffu, 0x34000000u, 0x3effffffu, 0x3f000000u,
    0x3f7fffffu, 0x3f800000u, 0x3fc00000u, 0x40490fdbu, 0x4b000000u, 0x4b7fffffu, 0x7f7fffffu, 0x7f800000u,
    0x7fc00000u, 0x7fc12345u, 0x80000000u, 0x80000001u, 0x80000002u, 0x80000003u, 0x80400000u, 0x807ffffeu,
    0x807fffffu, 0x80800000u, 0x80800001u, 0x80ffffffu, 0x81000000u, 0x8c000000u, 0xb3ffffffu, 0xb4000000u,
    0xbeffffffu, 0xbf000000u, 0xbf7fffffu, 0xbf800000u, 0xbfc00000u, 0xc0490fdbu, 0xcb000000u, 0xcb7fffffu,
    0xff7fffffu, 0xff800000u, 0xffc00000u, 0xffc12345u, 0x804fbb3fu, 0x805ef2e1u, 0x0065c8e8u, 0x806b9087u,
    0x0063529du, 0x0053119au, 0x007350b6u, 0x800e7d89u, 0x003c0a75u, 0x0030bcadu, 0x004976e8u, 0x0001adbau
};
constexpr std::uint32_t Mode1Expected[Threads][Results] = {
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f000000u},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f000000u},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f400000u},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f000000u},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f7ffffcu},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f7ffffeu},
    {0x00800000u, 0x00000000u, 0x3f800000u, 0x3f000000u},
    {0x00800001u, 0x00000000u, 0x3f800000u, 0x3f000001u},
    {0x00ffffffu, 0x00000000u, 0x3f800000u, 0x3f7fffffu},
    {0x01000000u, 0x00000000u, 0x3f800000u, 0x3f000000u},
    {0x0c000000u, 0x00000000u, 0x3f800000u, 0x3f000000u},
    {0x33ffffffu, 0x00000000u, 0x3f800000u, 0x3f7fffffu},
    {0x34000000u, 0x00000000u, 0x3f800000u, 0x3f000000u},
    {0x3effffffu, 0x00000000u, 0x3f800000u, 0x3f7fffffu},
    {0x3f000000u, 0x00000000u, 0x3f800000u, 0x3f000000u},
    {0x3f7fffffu, 0x00000000u, 0x3f800000u, 0x3f7fffffu},
    {0x00000000u, 0x3f800000u, 0x3f800000u, 0x3f000000u},
    {0x3f000000u, 0x3f800000u, 0x40000000u, 0x3f400000u},
    {0x3e10fdb0u, 0x40400000u, 0x40800000u, 0x3f490fdbu},
    {0x00000000u, 0x4b000000u, 0x4b000000u, 0x3f000000u},
    {0x00000000u, 0x4b7fffffu, 0x4b7fffffu, 0x3f7fffffu},
    {0x00000000u, 0x7f7fffffu, 0x7f7fffffu, 0x3f7fffffu},
    {0xffc00000u, 0x7f800000u, 0x7f800000u, 0x7f800000u},
    {0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u},
    {0x7fc12345u, 0x7fc12345u, 0x7fc12345u, 0x7fc12345u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf000000u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf000000u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf400000u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf000000u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf7ffffcu},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf7ffffeu},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf000000u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf000001u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf7fffffu},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf000000u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf000000u},
    {0x3f7ffffeu, 0xbf800000u, 0x80000000u, 0xbf7fffffu},
    {0x3f7ffffeu, 0xbf800000u, 0x80000000u, 0xbf000000u},
    {0x3f000000u, 0xbf800000u, 0x80000000u, 0xbf7fffffu},
    {0x3f000000u, 0xbf800000u, 0x80000000u, 0xbf000000u},
    {0x33800000u, 0xbf800000u, 0x80000000u, 0xbf7fffffu},
    {0x00000000u, 0xbf800000u, 0xbf800000u, 0xbf000000u},
    {0x3f000000u, 0xc0000000u, 0xbf800000u, 0xbf400000u},
    {0x3f5bc094u, 0xc0800000u, 0xc0400000u, 0xbf490fdbu},
    {0x00000000u, 0xcb000000u, 0xcb000000u, 0xbf000000u},
    {0x00000000u, 0xcb7fffffu, 0xcb7fffffu, 0xbf7fffffu},
    {0x00000000u, 0xff7fffffu, 0xff7fffffu, 0xbf7fffffu},
    {0xffc00000u, 0xff800000u, 0xff800000u, 0xff800000u},
    {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
    {0xffc12345u, 0xffc12345u, 0xffc12345u, 0xffc12345u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf1f767eu},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf3de5c2u},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f4b91d0u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf57210eu},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f46a53au},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f262334u},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f66a16cu},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf67d890u},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f7029d4u},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f42f2b4u},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f12edd0u},
    {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f56dd00u}
};
constexpr std::uint32_t Mode2Expected[Threads][Results] = {
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x00000000u, 0x3f800000u, 0x3f000000u},
    {0x00800001u, 0x00000000u, 0x3f800000u, 0x3f000001u},
    {0x00ffffffu, 0x00000000u, 0x3f800000u, 0x3f7fffffu},
    {0x01000000u, 0x00000000u, 0x3f800000u, 0x3f000000u},
    {0x0c000000u, 0x00000000u, 0x3f800000u, 0x3f000000u},
    {0x33ffffffu, 0x00000000u, 0x3f800000u, 0x3f7fffffu},
    {0x34000000u, 0x00000000u, 0x3f800000u, 0x3f000000u},
    {0x3effffffu, 0x00000000u, 0x3f800000u, 0x3f7fffffu},
    {0x3f000000u, 0x00000000u, 0x3f800000u, 0x3f000000u},
    {0x3f7fffffu, 0x00000000u, 0x3f800000u, 0x3f7fffffu},
    {0x00000000u, 0x3f800000u, 0x3f800000u, 0x3f000000u},
    {0x3f000000u, 0x3f800000u, 0x40000000u, 0x3f400000u},
    {0x3e10fdb0u, 0x40400000u, 0x40800000u, 0x3f490fdbu},
    {0x00000000u, 0x4b000000u, 0x4b000000u, 0x3f000000u},
    {0x00000000u, 0x4b7fffffu, 0x4b7fffffu, 0x3f7fffffu},
    {0x00000000u, 0x7f7fffffu, 0x7f7fffffu, 0x3f7fffffu},
    {0xffc00000u, 0x7f800000u, 0x7f800000u, 0x7f800000u},
    {0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u},
    {0x7fc12345u, 0x7fc12345u, 0x7fc12345u, 0x7fc12345u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf000000u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf000001u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf7fffffu},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf000000u},
    {0x3f7fffffu, 0xbf800000u, 0x80000000u, 0xbf000000u},
    {0x3f7ffffeu, 0xbf800000u, 0x80000000u, 0xbf7fffffu},
    {0x3f7ffffeu, 0xbf800000u, 0x80000000u, 0xbf000000u},
    {0x3f000000u, 0xbf800000u, 0x80000000u, 0xbf7fffffu},
    {0x3f000000u, 0xbf800000u, 0x80000000u, 0xbf000000u},
    {0x33800000u, 0xbf800000u, 0x80000000u, 0xbf7fffffu},
    {0x00000000u, 0xbf800000u, 0xbf800000u, 0xbf000000u},
    {0x3f000000u, 0xc0000000u, 0xbf800000u, 0xbf400000u},
    {0x3f5bc094u, 0xc0800000u, 0xc0400000u, 0xbf490fdbu},
    {0x00000000u, 0xcb000000u, 0xcb000000u, 0xbf000000u},
    {0x00000000u, 0xcb7fffffu, 0xcb7fffffu, 0xbf7fffffu},
    {0x00000000u, 0xff7fffffu, 0xff7fffffu, 0xbf7fffffu},
    {0xffc00000u, 0xff800000u, 0xff800000u, 0xff800000u},
    {0xffc00000u, 0xffc00000u, 0xffc00000u, 0xffc00000u},
    {0xffc12345u, 0xffc12345u, 0xffc12345u, 0xffc12345u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}
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

std::optional<std::string> Run(AgcDriver::VulkanDevice& device, ShaderRecompiler::ShaderFloatMode floatMode) {
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Input[tid * Inputs] = Values[tid];
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
    request.context.floatMode = floatMode;
    std::optional<decltype(ShaderRecompiler::Recompile(request))> result;
    try {
        result.emplace(ShaderRecompiler::Recompile(request));
    } catch (const std::exception& error) {
        return std::string(error.what());
    }
    device.Dispatch(*result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return std::nullopt;
}

void Check(std::uint32_t denormals, bool ieee) {
    const auto& expected = denormals == 1u ? Mode1Expected : Mode2Expected;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t i = 0; i < Results; ++i) {
            const std::uint32_t actual = Output[tid * Results + i];
            Require(actual == expected[tid][i], std::string(Names[i]) + " f32 denormal mode " + std::to_string(denormals) + (ieee ? ", IEEE=1" : ", IEEE=0") + " of " + Hex(Values[tid]) + " is " + Hex(actual) + ", expected " + Hex(expected[tid][i]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool keepsDenormals = device->Target().float32DenormPreserve;
        std::uint32_t checked = 0u;
        for (const std::uint32_t denormals : {1u, 2u}) {
            for (const bool ieee : {false, true}) {
                const ShaderRecompiler::ShaderFloatMode floatMode{0xc0u | denormals << 4u, true, ieee, false};
                const auto refusal = Run(*device, floatMode);
                if (refusal) {
                    Require(denormals == 1u && !keepsDenormals && refusal->find("needs DenormPreserve 32") != std::string::npos, "FLOAT_MODE " + Hex(floatMode.floatMode) + ": " + *refusal);
                    continue;
                }
                ++checked;
                Check(denormals, ieee);
            }
        }
        std::printf("f32 fract, floor, ceil and frexp_mant denormal mode 1 and 2 tests passed (%u of 4 modes run; the others need DenormPreserve 32)\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
