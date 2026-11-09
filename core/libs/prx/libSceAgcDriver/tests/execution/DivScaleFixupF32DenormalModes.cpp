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
constexpr std::uint32_t Results = 5;
constexpr std::uint32_t Stride = 8;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Stride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 20> Code{
    0x34020084u, 0x34040085u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd56d6a0au, 0x04160904u, 0xd501000bu,
    0x01a90280u, 0xd56d6a0cu, 0x04160905u, 0xd501000du, 0x01a90280u, 0xd55f000eu, 0x04160906u, 0xe0781000u,
    0x80010a02u, 0xe0701010u, 0x80010e02u, 0xbf810000u,
};

constexpr const char* Names[Results] = {
    "v_div_scale_f32 v10, vcc_lo, v4, v4, v5",
    "its vcc_lo",
    "v_div_scale_f32 v12, vcc_lo, v5, v4, v5",
    "its vcc_lo",
    "v_div_fixup_f32 v14, v6, v4, v5",
};

constexpr std::uint32_t Rows[Threads][3] = {
    {0x7e800000u, 0x00000001u, 0x00000005u},
    {0x7f7fffffu, 0x807fffffu, 0x3f800000u},
    {0x7e800000u, 0x00400000u, 0x80400000u},
    {0xff7fffffu, 0x00000003u, 0x7fc00001u},
    {0x00000001u, 0x3f800000u, 0x00000005u},
    {0x007fffffu, 0x40490fdbu, 0x3f800000u},
    {0x80000001u, 0xbfc00000u, 0x80400000u},
    {0x00400000u, 0x7f7fffffu, 0x7fc00001u},
    {0x3f800000u, 0x00000003u, 0x00000005u},
    {0xbfc00000u, 0x80000001u, 0x3f800000u},
    {0x40490fdbu, 0x007fffffu, 0x80400000u},
    {0x5f800000u, 0x00400000u, 0x7fc00001u},
    {0x00000001u, 0x00000001u, 0x00000005u},
    {0x007fffffu, 0x807fffffu, 0x3f800000u},
    {0x00400000u, 0x00000003u, 0x80400000u},
    {0x00800000u, 0x00000001u, 0x7fc00001u},
    {0x00000001u, 0x00800000u, 0x00000005u},
    {0x0c000000u, 0x00000001u, 0x3f800000u},
    {0x00000003u, 0x0c800000u, 0x80400000u},
    {0x34000000u, 0x80000001u, 0x7fc00001u},
    {0x00000001u, 0x00000000u, 0x00000005u},
    {0x00000000u, 0x00000001u, 0x3f800000u},
    {0x7f800000u, 0x00000001u, 0x80400000u},
    {0x00000001u, 0xff800000u, 0x7fc00001u},
    {0x7fc00000u, 0x00000001u, 0x00000005u},
    {0x00000003u, 0x7fc00000u, 0x3f800000u},
    {0x00000001u, 0x7e800000u, 0x80400000u},
    {0x807fffffu, 0x5f800000u, 0x7fc00001u},
    {0x3f800000u, 0x3f800000u, 0x00000005u},
    {0x40490fdbu, 0xbfc00000u, 0x3f800000u},
    {0x0c800000u, 0x7f7fffffu, 0x80400000u},
    {0x00800000u, 0x0c000000u, 0x7fc00001u}
};
constexpr std::uint32_t Mode1Expected[Threads][Results] = {
    {0x5e800000u, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000000u},
    {0x5f7fffffu, 0x00000001u, 0x80000000u, 0x00000001u, 0x80000000u},
    {0x5e800000u, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000000u},
    {0xdf7fffffu, 0x00000001u, 0x00000000u, 0x00000001u, 0x80000000u},
    {0x15000000u, 0x00000001u, 0x3f800000u, 0x00000001u, 0x00000005u},
    {0x207ffffeu, 0x00000001u, 0x40490fdbu, 0x00000001u, 0x3f800000u},
    {0x95000000u, 0x00000001u, 0xbfc00000u, 0x00000001u, 0x00400000u},
    {0x20000000u, 0x00000001u, 0x7f7fffffu, 0x00000001u, 0x7f800000u},
    {0x3f800000u, 0x00000001u, 0x15c00000u, 0x00000001u, 0x00000005u},
    {0xbfc00000u, 0x00000001u, 0x95000000u, 0x00000001u, 0x3f800000u},
    {0x40490fdbu, 0x00000001u, 0x207ffffeu, 0x00000001u, 0x00400000u},
    {0x5f800000u, 0x00000001u, 0x20000000u, 0x00000001u, 0x00000000u},
    {0x15000000u, 0x00000000u, 0x15000000u, 0x00000000u, 0x00000005u},
    {0x207ffffeu, 0x00000000u, 0xa07ffffeu, 0x00000000u, 0xbf800000u},
    {0x20000000u, 0x00000000u, 0x15c00000u, 0x00000000u, 0x00400000u},
    {0x20800000u, 0x00000000u, 0x15000000u, 0x00000000u, 0x7f800000u},
    {0x15000000u, 0x00000000u, 0x20800000u, 0x00000000u, 0x00000005u},
    {0x2c000000u, 0x00000000u, 0x15000000u, 0x00000000u, 0x3f800000u},
    {0x15c00000u, 0x00000000u, 0x2c800000u, 0x00000000u, 0x00400000u},
    {0x34000000u, 0x00000001u, 0x95000000u, 0x00000001u, 0xff800000u},
    {0xffc00000u, 0x00000000u, 0xffc00000u, 0x00000000u, 0x00000000u},
    {0xffc00000u, 0x00000000u, 0xffc00000u, 0x00000000u, 0x7f800000u},
    {0x7f800000u, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000000u},
    {0x15000000u, 0x00000001u, 0xff800000u, 0x00000001u, 0xff800000u},
    {0x7fc00000u, 0x00000001u, 0x00000000u, 0x00000001u, 0x7fc00000u},
    {0x15c00000u, 0x00000001u, 0x7fc00000u, 0x00000001u, 0x7fc00000u},
    {0x15000000u, 0x00000001u, 0x7e800000u, 0x00000001u, 0x00400000u},
    {0xa07ffffeu, 0x00000001u, 0x5f800000u, 0x00000001u, 0xff800000u},
    {0x3f800000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000005u},
    {0x40490fdbu, 0x00000000u, 0xbfc00000u, 0x00000000u, 0xbf800000u},
    {0x2c800000u, 0x00000001u, 0x7f7fffffu, 0x00000001u, 0x00400000u},
    {0x20800000u, 0x00000000u, 0x2c000000u, 0x00000000u, 0x7f800000u}
};
constexpr std::uint32_t Mode2Expected[Threads][Results] = {
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x00000000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x80000000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x00000000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x80000000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x7f800000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x7f800000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x7f800000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x7f800000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x00000000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x00000000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x00000000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x00000000u},
    {0xffc00000u, 0x00000000u, 0xffc00000u, 0x00000000u, 0xffc00000u},
    {0xffc00000u, 0x00000000u, 0xffc00000u, 0x00000000u, 0xffc00000u},
    {0xffc00000u, 0x00000000u, 0xffc00000u, 0x00000000u, 0xffc00000u},
    {0xffc00000u, 0x00000000u, 0xffc00000u, 0x00000000u, 0x00000000u},
    {0xffc00000u, 0x00000000u, 0xffc00000u, 0x00000000u, 0x7f800000u},
    {0xffc00000u, 0x00000000u, 0xffc00000u, 0x00000000u, 0x00000000u},
    {0xffc00000u, 0x00000000u, 0xffc00000u, 0x00000000u, 0x7f800000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x80000000u},
    {0xffc00000u, 0x00000000u, 0xffc00000u, 0x00000000u, 0xffc00000u},
    {0xffc00000u, 0x00000000u, 0xffc00000u, 0x00000000u, 0xffc00000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x00000000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0xff800000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x7fc00000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x7fc00000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0x7f800000u},
    {0xffc00000u, 0x00000001u, 0xffc00000u, 0x00000001u, 0xff800000u},
    {0x3f800000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000005u},
    {0x40490fdbu, 0x00000000u, 0xbfc00000u, 0x00000000u, 0xbf800000u},
    {0x2c800000u, 0x00000001u, 0x7f7fffffu, 0x00000001u, 0x00400000u},
    {0x20800000u, 0x00000000u, 0x2c000000u, 0x00000000u, 0x7f800000u}
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
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
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
            const std::uint32_t actual = Output[tid * Stride + i];
            Require(actual == expected[tid][i], "f32 denormal mode " + std::to_string(denormals) + (ieee ? ", IEEE=1, " : ", IEEE=0, ") + Names[i] + " with v4 " + Hex(Rows[tid][0]) + ", v5 " + Hex(Rows[tid][1]) + ", v6 " + Hex(Rows[tid][2]) + " is " + Hex(actual) + ", expected " + Hex(expected[tid][i]));
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
        std::printf("v_div_scale_f32 and v_div_fixup_f32 denormal mode 1 and 2 tests passed (%u of 4 modes run; the others need DenormPreserve 32)\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
