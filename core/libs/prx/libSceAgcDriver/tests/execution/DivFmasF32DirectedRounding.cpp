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

alignas(256) constexpr std::array<std::uint32_t, 13> Code{
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7d8a0e80u, 0x7e160280u, 0x7e180280u, 0x7e1a0280u,
    0xd56f000au, 0x041a0b04u, 0xe0781000u, 0x80010a01u, 0xbf810000u,
};
alignas(256) std::array<std::uint32_t, 14> RuntimeCode{};

constexpr std::array<std::uint32_t, Threads * Words> Values{
    0x813c26e2u, 0x00cd060bu, 0x8152eaf5u, 0x00000000u,
    0x20432fa8u, 0xca0b888bu, 0xc067186eu, 0x00000001u,
    0xff668645u, 0x80000001u, 0x8021e17eu, 0x00000000u,
    0xfec7ededu, 0x80d24164u, 0x00000001u, 0x00000001u,
    0x0121adc3u, 0x7f591fb1u, 0x174067adu, 0x00000000u,
    0xff7fffffu, 0x893a04c9u, 0xc93a0483u, 0x00000001u,
    0xb07f73b3u, 0x777ecdfeu, 0x00000001u, 0x00000000u,
    0x00000001u, 0x7e9214a2u, 0xb5df284bu, 0x00000001u,
    0xf2d7eb39u, 0xe2745d2au, 0xf3e20c25u, 0x00000000u,
    0x606e1ae9u, 0x3ecab320u, 0x7d2112acu, 0x00000001u,
    0xc1d77694u, 0x7f7cb668u, 0xff13bc61u, 0x00000000u,
    0x80cf985cu, 0x7f5800c0u, 0xff4982bfu, 0x00000001u,
    0x00000001u, 0xddb5688bu, 0x1335688bu, 0x00000000u,
    0x00000001u, 0xff341b91u, 0x34b41b91u, 0x00000001u,
    0x2503d9c3u, 0x00b757d0u, 0x02e3622bu, 0x00000000u,
    0xff2ae54du, 0x4d0f6744u, 0x810da9f4u, 0x00000000u,
    0x18fa51a0u, 0xc12c924du, 0xa5b1eae8u, 0x00000000u,
    0xf1b39f6bu, 0x7f7fffffu, 0xf77dfaf4u, 0x00000000u,
    0xd260b013u, 0xfa62bf17u, 0x81315977u, 0x00000001u,
    0xbbd5af43u, 0x80a9f8e4u, 0xfeea31d1u, 0x00000000u,
    0x8a4931a3u, 0xff7fffffu, 0xca49319cu, 0x00000001u,
    0xfd6800d8u, 0x4eaa6384u, 0xff3de230u, 0x00000001u,
    0x7f7fffffu, 0x80fd3207u, 0x00977215u, 0x00000001u,
    0xff356dfbu, 0x80f6a103u, 0x810878d5u, 0x00000001u,
    0x7f0d3cbcu, 0xdb3aff42u, 0x5a614d5bu, 0x00000001u,
    0x6aac6088u, 0xff117dddu, 0x5907e5f6u, 0x00000000u,
    0x7f345b61u, 0x81567456u, 0x41171638u, 0x00000000u,
    0x72b60166u, 0x77fb2de5u, 0x6a1ba2f8u, 0x00000000u,
    0x0061442au, 0x7f4b6396u, 0x00000001u, 0x00000000u,
    0x00b5b4f5u, 0x901bb0beu, 0x7edfeef7u, 0x00000000u,
    0x3f800000u, 0x3f800000u, 0xbf800000u, 0x00000000u,
    0x3f800000u, 0x3f800000u, 0xbf800000u, 0x00000001u
};

constexpr std::uint32_t Expected[3][Threads] = {
    {
        0x8152eaf5u, 0xe067186eu, 0x34e68645u, 0x2024343eu, 0x4109204fu, 0x608a8bf7u, 0xe87e4258u, 0x95cce5b6u,
        0x7f800000u, 0x7f800000u, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0x02e3622cu, 0xff7fffffu,
        0xa5b1eaeau, 0xff7fffffu, 0x6d470329u, 0xfeea31d0u, 0x5fc6d9ccu, 0xff7fffffu, 0xa0fd3206u, 0x20aec9ccu,
        0xff7fffffu, 0xff7fffffu, 0xb804529au, 0x7f800000u, 0x401a8dc8u, 0x7edfeef7u, 0x00000000u, 0x00000000u
    },
    {
        0x8152eaf6u, 0xe067186fu, 0x34e68644u, 0x2024343du, 0x4109204eu, 0x608a8bf6u, 0xe87e4259u, 0x95cce5b7u,
        0x7f7fffffu, 0x7f7fffffu, 0xff800000u, 0xff800000u, 0x80000000u, 0x80000000u, 0x02e3622bu, 0xff800000u,
        0xa5b1eaebu, 0xff800000u, 0x6d470328u, 0xfeea31d1u, 0x5fc6d9cbu, 0xff800000u, 0xa0fd3207u, 0x20aec9cbu,
        0xff800000u, 0xff800000u, 0xb804529bu, 0x7f7fffffu, 0x401a8dc7u, 0x7edfeef6u, 0x80000000u, 0x80000000u
    },
    {
        0x8152eaf5u, 0xe067186eu, 0x34e68644u, 0x2024343du, 0x4109204eu, 0x608a8bf6u, 0xe87e4258u, 0x95cce5b6u,
        0x7f7fffffu, 0x7f7fffffu, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0x02e3622bu, 0xff7fffffu,
        0xa5b1eaeau, 0xff7fffffu, 0x6d470328u, 0xfeea31d0u, 0x5fc6d9cbu, 0xff7fffffu, 0xa0fd3206u, 0x20aec9cbu,
        0xff7fffffu, 0xff7fffffu, 0xb804529au, 0x7f7fffffu, 0x401a8dc7u, 0x7edfeef6u, 0x00000000u, 0x00000000u
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
        const std::uint32_t expected = Expected[rounding - 1u][tid];
        Require(Output[tid * Words + 0u] == expected, "v_div_fmas_f32 " + mode + " of " + Hex(in[0]) + ", " + Hex(in[1]) + ", " + Hex(in[2]) + ", " + Hex(in[3]) + " is " + Hex(Output[tid * Words + 0u]) + ", expected " + Hex(expected));
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
        std::printf("v_div_fmas_f32 directed rounding tests passed (%u of 24 runs, declared and set by s_round_mode)\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
