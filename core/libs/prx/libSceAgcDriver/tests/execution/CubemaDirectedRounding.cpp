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
constexpr std::uint32_t Words = 4;
alignas(256) std::array<std::uint32_t, Threads * Words> Input{};
alignas(256) std::array<std::uint32_t, Threads * Words> Output{};

alignas(256) constexpr std::array<std::uint32_t, 12> Code{
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e160280u, 0x7e180280u, 0x7e1a0280u, 0xd547000au,
    0x041a0b04u, 0xe0781000u, 0x80010a01u, 0xbf810000u,
};
alignas(256) std::array<std::uint32_t, 13> RuntimeCode{};

constexpr std::array<std::uint32_t, Threads * Words> Values{
    0x3f800000u, 0x40000000u, 0x40400000u, 0x00000000u,
    0x40400000u, 0xc0000000u, 0x3f800000u, 0x00000000u,
    0x3f800000u, 0xc0800000u, 0x40000000u, 0x00000000u,
    0xc1200000u, 0x3f800000u, 0x40000000u, 0x00000000u,
    0xd425f3c7u, 0x008689c1u, 0x14431c88u, 0x00000000u,
    0xb17e6cc6u, 0x7d12cce4u, 0x0a1c4278u, 0x00000000u,
    0x3e800000u, 0x3e800000u, 0xbe800000u, 0x00000000u,
    0x00800000u, 0x00800000u, 0x80800000u, 0x00000000u,
    0x7f7fffffu, 0x172523e3u, 0x0059db9au, 0x00000000u,
    0xff7fffffu, 0x3f800000u, 0x00000000u, 0x00000000u,
    0x3f800000u, 0x7f000000u, 0x2459f126u, 0x00000000u,
    0x80000000u, 0xff000000u, 0x00400000u, 0x00000000u,
    0x736e865fu, 0x7f3966d0u, 0xff37721eu, 0x00000000u,
    0xd3567b9cu, 0xa1125b08u, 0xff000000u, 0x00000000u,
    0x7effffffu, 0x80000000u, 0xfeffffffu, 0x00000000u,
    0x00000000u, 0x80000000u, 0x7f000000u, 0x00000000u,
    0x7f000000u, 0x7f000000u, 0x7f000000u, 0x00000000u,
    0xff000000u, 0xff000000u, 0xff000000u, 0x00000000u,
    0x7e800000u, 0x7e000000u, 0x00000000u, 0x00000000u,
    0xfe800000u, 0x00000000u, 0x7e000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x807fffffu, 0x00000000u,
    0x80000000u, 0x00000000u, 0x00000001u, 0x00000000u,
    0x00000001u, 0x00000001u, 0x80400000u, 0x00000000u,
    0x807fffffu, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x80000001u, 0x00000000u, 0x00000000u,
    0x80000000u, 0x007fffffu, 0x80000001u, 0x00000000u,
    0x7f800000u, 0x3f800000u, 0x00000000u, 0x00000000u,
    0x3f800000u, 0xff800000u, 0x40000000u, 0x00000000u,
    0x00000000u, 0x7f7fffffu, 0x7f800000u, 0x00000000u,
    0xbf800000u, 0x3f800000u, 0xff7fffffu, 0x00000000u
};

constexpr std::uint32_t Expected[3][Threads] = {
    {
        0x40c00000u, 0x40c00000u, 0xc1000000u, 0xc1a00000u, 0xd4a5f3c7u, 0x7d92cce4u, 0xbf000000u, 0x81000000u,
        0x7f800000u, 0xff7fffffu, 0x7f800000u, 0xff7fffffu, 0x7f800000u, 0xff7fffffu, 0xff7fffffu, 0x7f800000u,
        0x7f800000u, 0xff7fffffu, 0x7f000000u, 0xff000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
        0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x7f800000u, 0xff800000u, 0x7f800000u, 0xff7fffffu
    },
    {
        0x40c00000u, 0x40c00000u, 0xc1000000u, 0xc1a00000u, 0xd4a5f3c7u, 0x7d92cce4u, 0xbf000000u, 0x81000000u,
        0x7f7fffffu, 0xff800000u, 0x7f7fffffu, 0xff800000u, 0x7f7fffffu, 0xff800000u, 0xff7fffffu, 0x7f7fffffu,
        0x7f7fffffu, 0xff800000u, 0x7f000000u, 0xff000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u,
        0x80000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x7f800000u, 0xff800000u, 0x7f800000u, 0xff800000u
    },
    {
        0x40c00000u, 0x40c00000u, 0xc1000000u, 0xc1a00000u, 0xd4a5f3c7u, 0x7d92cce4u, 0xbf000000u, 0x81000000u,
        0x7f7fffffu, 0xff7fffffu, 0x7f7fffffu, 0xff7fffffu, 0x7f7fffffu, 0xff7fffffu, 0xff7fffffu, 0x7f7fffffu,
        0x7f7fffffu, 0xff7fffffu, 0x7f000000u, 0xff000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
        0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x7f800000u, 0xff800000u, 0x7f800000u, 0xff7fffffu
    }
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%x", value);
    return text;
}

std::optional<std::string> Run(AgcDriver::VulkanDevice& device, ShaderRecompiler::ShaderFloatMode floatMode, std::span<const std::uint32_t> code) {
    Input = Values;
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
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

void Check(std::uint32_t rounding, const std::string& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Values[tid * Words];
        const std::uint32_t actual = Output[tid * Words];
        const std::uint32_t expected = Expected[rounding - 1u][tid];
        Require(actual == expected, "v_cubema_f32 " + mode + " of " + Hex(in[0]) + ", " + Hex(in[1]) + ", " + Hex(in[2]) + " is " + Hex(actual) + ", expected " + Hex(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::uint32_t checked = 0u;
        for (std::uint32_t rounding = 1u; rounding < 4u; ++rounding) {
            for (const std::uint32_t denormals : {0xc0u, 0xf0u}) {
                for (const bool ieee : {false, true}) {
                    const std::string declared = "FLOAT_MODE " + Hex(denormals | rounding) + (ieee ? ", IEEE=1" : ", IEEE=0");
                    const auto refusal = Run(*device, ShaderRecompiler::ShaderFloatMode{denormals | rounding, true, ieee, false}, Code);
                    if (refusal) {
                        Require(denormals == 0xf0u && refusal->find("DenormPreserve") != std::string::npos, declared + ": " + *refusal);
                        continue;
                    }
                    Check(rounding, declared);
                    RuntimeCode[0] = 0xbfa40000u | rounding;
                    std::copy(Code.begin(), Code.end(), RuntimeCode.begin() + 1);
                    const auto runtimeRefusal = Run(*device, ShaderRecompiler::ShaderFloatMode{denormals, true, ieee, false}, RuntimeCode);
                    Require(!runtimeRefusal, "s_round_mode " + Hex(rounding) + " after " + declared + ": " + runtimeRefusal.value_or(""));
                    Check(rounding, "s_round_mode " + Hex(rounding) + " after FLOAT_MODE " + Hex(denormals) + (ieee ? ", IEEE=1" : ", IEEE=0"));
                    checked += 2u;
                }
            }
        }
        std::printf("v_cubema_f32 directed rounding tests passed (%u of 24 runs, declared and set by s_round_mode)\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
