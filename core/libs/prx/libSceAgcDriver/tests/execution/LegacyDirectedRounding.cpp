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
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e180280u, 0x7e1a0280u, 0xd540000au, 0x041a0b04u,
    0x7e160306u, 0x0c160b04u, 0xe0781000u, 0x80010a01u, 0xbf810000u,
};
alignas(256) std::array<std::uint32_t, 14> RuntimeCode{};

constexpr std::array<std::uint32_t, Threads * Words> Values{
    0x3807aff0u, 0x87f17f1bu, 0x00800000u, 0x00000000u,
    0x3aed1f75u, 0x850a30a1u, 0x80000000u, 0x00000000u,
    0xb8fcceddu, 0x87019dbau, 0x00000000u, 0x00000000u,
    0xbedd15c4u, 0x811436e8u, 0x00000000u, 0x00000000u,
    0xb8aa77dcu, 0x87403939u, 0x80800000u, 0x00000000u,
    0xbaac6efcu, 0x053e085bu, 0x80000000u, 0x00000000u,
    0xfe89ca0du, 0x06dd0d6eu, 0x45edf56du, 0x00000001u,
    0xff70b125u, 0x1b4e6edfu, 0xfee82314u, 0x00000001u,
    0x3f800000u, 0xbf800000u, 0x7f7fffffu, 0x00000000u,
    0x7f038558u, 0x01edcfbbu, 0xc1745a62u, 0x00000000u,
    0x7f7fffffu, 0x0084f74eu, 0x803fac57u, 0x00000000u,
    0xff139fcfu, 0x81722939u, 0xc10ba4d8u, 0x00000001u,
    0x01086386u, 0xe5b576bfu, 0x27415b44u, 0x00000001u,
    0x39599074u, 0x86969d13u, 0x80000000u, 0x00000000u,
    0x18cd72feu, 0x7efbc61fu, 0x23cccaa8u, 0x00000000u,
    0xd6d340a0u, 0x010feb10u, 0x80dfecbau, 0x00000000u,
    0x7ef07588u, 0x7ea1f207u, 0x80285754u, 0x00000001u,
    0xca243f35u, 0x7e9aacdau, 0xff16b5e8u, 0x00000000u,
    0xe47c714bu, 0x0096150au, 0x2593ff21u, 0x00000000u,
    0xba48159cu, 0x05a3c558u, 0x80000000u, 0x00000000u,
    0x42942871u, 0x00f71240u, 0x840efd95u, 0x00000001u,
    0xbf0d7529u, 0x80e7a531u, 0x00000000u, 0x00000000u,
    0xba025347u, 0x05fb6eb0u, 0x00c00000u, 0x00000000u,
    0xfe85bdd8u, 0xa5f630d5u, 0xe5009df0u, 0x00000001u,
    0x7f03a833u, 0x80ebc96fu, 0x407285feu, 0x00000001u,
    0x6e5418bau, 0xb7a26687u, 0x66868c96u, 0x00000000u,
    0x7ece0d49u, 0x650029d2u, 0x00cd7097u, 0x00000001u,
    0xfe95800du, 0x3864aee9u, 0x86b0e5c8u, 0x00000001u,
    0xff274779u, 0x00c5b71au, 0x408131b3u, 0x00000000u,
    0xfd677e38u, 0xff383de7u, 0xe6cfa5b2u, 0x00000001u,
    0x3f800000u, 0x3f800000u, 0xbf800000u, 0x00000000u,
    0x00000000u, 0x7f800000u, 0x80000000u, 0x00000000u
};

constexpr std::uint32_t Expected[3][Threads] = {
    {
        0x00800000u, 0x80800000u, 0x00800000u, 0x00800001u, 0x00000000u, 0x80000000u, 0x3c700000u, 0xfee82314u,
        0x7f7fffffu, 0xb7300000u, 0x4084f74eu, 0x00000000u, 0x1c000000u, 0x80800021u, 0x584a0ec0u, 0x986d8625u,
        0x7f800000u, 0xff7fffffu, 0x00000000u, 0x80000000u, 0x00000000u, 0x00800000u, 0x00c00000u, 0x59800000u,
        0x34800000u, 0xdb000000u, 0x7f800000u, 0xf7858c30u, 0x36e00000u, 0x7f800000u, 0x00000000u, 0x00000000u
    },
    {
        0x80000000u, 0x80800001u, 0x00000000u, 0x00800000u, 0x80800000u, 0x80800000u, 0x3c680000u, 0xfee82315u,
        0x7f7ffffeu, 0xb7400000u, 0x4084f74du, 0xb5800000u, 0x1b800000u, 0x80800022u, 0x584a0ebeu, 0x986d8627u,
        0x7f7fffffu, 0xff800000u, 0x9a000000u, 0x80800000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80000000u,
        0x80000000u, 0xdb800000u, 0x7f7fffffu, 0xf7858c32u, 0x36d00000u, 0x7f7ffffeu, 0x80000000u, 0x80000000u
    },
    {
        0x00800000u, 0x80800000u, 0x00000000u, 0x00800000u, 0x80800000u, 0x80000000u, 0x3c700000u, 0xfee82314u,
        0x7f7ffffeu, 0xb7400000u, 0x4084f74du, 0xb5800000u, 0x1c000000u, 0x80800021u, 0x584a0ebeu, 0x986d8625u,
        0x7f7fffffu, 0xff7fffffu, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00c00000u, 0x00000000u,
        0x34800000u, 0xdb000000u, 0x7f7fffffu, 0xf7858c30u, 0x36e00000u, 0x7f7ffffeu, 0x00000000u, 0x00000000u
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
        Require(Output[tid * Words + 0u] == expected, "v_mad_legacy_f32 " + mode + " of " + Hex(in[0]) + ", " + Hex(in[1]) + ", " + Hex(in[2]) + " is " + Hex(Output[tid * Words + 0u]) + ", expected " + Hex(expected));
        Require(Output[tid * Words + 1u] == expected, "v_mac_legacy_f32 " + mode + " of " + Hex(in[0]) + ", " + Hex(in[1]) + ", " + Hex(in[2]) + " is " + Hex(Output[tid * Words + 1u]) + ", expected " + Hex(expected));
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
        std::printf("v_mad_legacy_f32 and v_mac_legacy_f32 directed rounding tests passed (%u of 24 runs, declared and set by s_round_mode)\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
