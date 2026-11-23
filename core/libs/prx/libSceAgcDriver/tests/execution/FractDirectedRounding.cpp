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

alignas(256) constexpr std::array<std::uint32_t, 11> Code{
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e160280u, 0x7e144104u, 0x7e16bf05u, 0x7e187d06u,
    0xe0781000u, 0x80010a01u, 0xbf810000u,
};
alignas(256) std::array<std::uint32_t, 12> RuntimeCode{};

constexpr std::array<std::uint32_t, Threads * Words> Values{
    0xb5de8693u, 0x00008001u, 0x00000001u, 0xbec00000u,
    0xb3000000u, 0x000083ffu, 0x00000000u, 0xbca00000u,
    0xb3800000u, 0x00008400u, 0x00000001u, 0xbca00000u,
    0xb3800001u, 0x00009001u, 0x00000001u, 0xbcb00000u,
    0xb4000001u, 0x0000a801u, 0x00000001u, 0xbfd00000u,
    0xbe800001u, 0x0000b401u, 0xffffffffu, 0xbfdfffffu,
    0xbeffffffu, 0x0000b7ffu, 0x00000001u, 0xbfe00000u,
    0xbf000001u, 0x0000b801u, 0xffffffffu, 0xbfefffffu,
    0xbf7fffffu, 0x0000bbffu, 0x00000001u, 0xbff80000u,
    0xbfc00001u, 0x0000be01u, 0x54442d18u, 0xc00921fbu,
    0xc0490fdbu, 0x0000c248u, 0x9999999au, 0xc0591999u,
    0xc2c8999au, 0x0000d641u, 0xfffffff1u, 0xc33fffffu,
    0xcb7ffff1u, 0x0000e3ffu, 0x00000000u, 0x3ff40000u,
    0x3fa00000u, 0x00003d00u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u,
    0x80000000u, 0x00008000u, 0xffffffffu, 0x3fdfffffu,
    0x3effffffu, 0x000037ffu, 0x00000000u, 0xbff00000u,
    0xbf800000u, 0x0000bc00u, 0x00000000u, 0xb0000000u,
    0xb0000000u, 0x00008200u, 0x00000001u, 0xbf700000u,
    0xbd800001u, 0x0000b3ffu, 0x00000003u, 0xbe000000u,
    0xb5a8e4deu, 0x0000cdc3u, 0xa341738cu, 0xc071389bu,
    0xc279c597u, 0x0000a002u, 0x16f44881u, 0xbdb8eb51u,
    0xc252d763u, 0x0000d052u, 0x4ac87fc0u, 0xc0d49a04u,
    0xb5712ad6u, 0x00009126u, 0x24114258u, 0xbf9e060au,
    0xb972890au, 0x000091ccu, 0xb437bdb5u, 0xc29b766fu,
    0xc54f8ccau, 0x00008742u, 0x4f1a8a1du, 0xc1706555u,
    0xc3b5d6b5u, 0x0000c2b7u, 0x5d111a9du, 0xbe27608du,
    0xcb4949efu, 0x0000acd7u, 0x17371472u, 0xbf066160u,
    0xc3ebd3d1u, 0x0000b501u, 0x8df05f25u, 0xc197f9acu,
    0xc96366bau, 0x0000b3ebu, 0x6ce9da66u, 0xba181a0bu,
    0xcabed5f4u, 0x0000b338u, 0xe01c683eu, 0xc1d7db43u,
    0xca62f40cu, 0x0000b7ffu, 0xeab06e9bu, 0xbe9864fau
};

constexpr std::uint32_t Expected[3][Threads][Words] = {
    {
        {0x3f7fffe5u, 0x00003bffu, 0x00000000u, 0x3feffffcu},
        {0x3f7fffffu, 0x00003bffu, 0xffffffffu, 0x3fefffffu},
        {0x3f7fffffu, 0x00003bffu, 0xffffffffu, 0x3fefffffu},
        {0x3f7fffffu, 0x00003bffu, 0xfffffffeu, 0x3fefffffu},
        {0x3f7ffffeu, 0x00003bc0u, 0x00000000u, 0x3fe80000u},
        {0x3f400000u, 0x00003a00u, 0x00000001u, 0x3fe00000u},
        {0x3f000001u, 0x00003801u, 0xfffffffeu, 0x3fdfffffu},
        {0x3efffffeu, 0x000037feu, 0x00000000u, 0x3ca00000u},
        {0x33800000u, 0x00001000u, 0xfffffffcu, 0x3fdfffffu},
        {0x3efffffcu, 0x000037fcu, 0xaeef4ba0u, 0x3feb7812u},
        {0x3f5bc094u, 0x00003ae0u, 0x33333300u, 0x3fe33333u},
        {0x3f333300u, 0x00003b80u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00003800u, 0x00000000u, 0x3fd00000u},
        {0x3e800000u, 0x00003400u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0xffffffffu, 0x3fdfffffu},
        {0x3effffffu, 0x000037ffu, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0xffffffffu, 0x3fefffffu},
        {0x3f7fffffu, 0x00003bffu, 0x00000000u, 0x3fefe000u},
        {0x3f700000u, 0x00003a01u, 0xffc00000u, 0x3fefffffu},
        {0x3f7fffebu, 0x00003ba0u, 0xfa31d000u, 0x3fdd9172u},
        {0x3f0e9a40u, 0x00003bf0u, 0xfffce296u, 0x3fefffffu},
        {0x3e944e80u, 0x00003700u, 0xc0200000u, 0x3fedda9bu},
        {0x3f7ffff1u, 0x00003bffu, 0xaedf75eeu, 0x3fef0fcfu},
        {0x3f7ff0d8u, 0x00003bffu, 0x00000000u, 0x3fe25800u},
        {0x3e4d8000u, 0x00003bffu, 0x60000000u, 0x3facaebcu},
        {0x3ea52c00u, 0x00003924u, 0xfe89f72bu, 0x3fefffffu},
        {0x00000000u, 0x00003b66u, 0x7a7fa324u, 0x3fefffa6u},
        {0x3eb0bc00u, 0x00003980u, 0xd8000000u, 0x3fe07d06u},
        {0x3ec00000u, 0x00003a06u, 0xffffffffu, 0x3fefffffu},
        {0x00000000u, 0x00003a32u, 0x00000000u, 0x3fe1cbe1u},
        {0x00000000u, 0x00003801u, 0x3cd828abu, 0x3fefffffu}
    },
    {
        {0x3f7fffe4u, 0x00003bffu, 0xffffffffu, 0x3feffffbu},
        {0x3f7fffffu, 0x00003bffu, 0xffffffffu, 0x3fefffffu},
        {0x3f7fffffu, 0x00003bffu, 0xfffffffeu, 0x3fefffffu},
        {0x3f7ffffeu, 0x00003bfeu, 0xfffffffdu, 0x3fefffffu},
        {0x3f7ffffdu, 0x00003bbfu, 0xffffffffu, 0x3fe7ffffu},
        {0x3f3fffffu, 0x000039ffu, 0x00000000u, 0x3fe00000u},
        {0x3f000000u, 0x00003800u, 0xfffffffeu, 0x3fdfffffu},
        {0x3efffffeu, 0x000037feu, 0x00000000u, 0x3ca00000u},
        {0x33800000u, 0x00001000u, 0xfffffffcu, 0x3fdfffffu},
        {0x3efffffcu, 0x000037fcu, 0xaeef4ba0u, 0x3feb7812u},
        {0x3f5bc094u, 0x00003ae0u, 0x33333300u, 0x3fe33333u},
        {0x3f333300u, 0x00003b80u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00003800u, 0x00000000u, 0x3fd00000u},
        {0x3e800000u, 0x00003400u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0xffffffffu, 0x3fdfffffu},
        {0x3effffffu, 0x000037ffu, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0xffffffffu, 0x3fefffffu},
        {0x3f7fffffu, 0x00003bffu, 0xffffffffu, 0x3fefdfffu},
        {0x3f6fffffu, 0x00003a00u, 0xffbfffffu, 0x3fefffffu},
        {0x3f7fffeau, 0x00003ba0u, 0xfa31d000u, 0x3fdd9172u},
        {0x3f0e9a40u, 0x00003befu, 0xfffce295u, 0x3fefffffu},
        {0x3e944e80u, 0x00003700u, 0xc0200000u, 0x3fedda9bu},
        {0x3f7ffff0u, 0x00003bfeu, 0xaedf75edu, 0x3fef0fcfu},
        {0x3f7ff0d7u, 0x00003bfeu, 0x00000000u, 0x3fe25800u},
        {0x3e4d8000u, 0x00003bffu, 0x60000000u, 0x3facaebcu},
        {0x3ea52c00u, 0x00003924u, 0xfe89f72au, 0x3fefffffu},
        {0x00000000u, 0x00003b65u, 0x7a7fa323u, 0x3fefffa6u},
        {0x3eb0bc00u, 0x0000397fu, 0xd8000000u, 0x3fe07d06u},
        {0x3ec00000u, 0x00003a05u, 0xffffffffu, 0x3fefffffu},
        {0x00000000u, 0x00003a32u, 0x00000000u, 0x3fe1cbe1u},
        {0x00000000u, 0x00003800u, 0x3cd828aau, 0x3fefffffu}
    },
    {
        {0x3f7fffe4u, 0x00003bffu, 0xffffffffu, 0x3feffffbu},
        {0x3f7fffffu, 0x00003bffu, 0xffffffffu, 0x3fefffffu},
        {0x3f7fffffu, 0x00003bffu, 0xfffffffeu, 0x3fefffffu},
        {0x3f7ffffeu, 0x00003bfeu, 0xfffffffdu, 0x3fefffffu},
        {0x3f7ffffdu, 0x00003bbfu, 0xffffffffu, 0x3fe7ffffu},
        {0x3f3fffffu, 0x000039ffu, 0x00000000u, 0x3fe00000u},
        {0x3f000000u, 0x00003800u, 0xfffffffeu, 0x3fdfffffu},
        {0x3efffffeu, 0x000037feu, 0x00000000u, 0x3ca00000u},
        {0x33800000u, 0x00001000u, 0xfffffffcu, 0x3fdfffffu},
        {0x3efffffcu, 0x000037fcu, 0xaeef4ba0u, 0x3feb7812u},
        {0x3f5bc094u, 0x00003ae0u, 0x33333300u, 0x3fe33333u},
        {0x3f333300u, 0x00003b80u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00003800u, 0x00000000u, 0x3fd00000u},
        {0x3e800000u, 0x00003400u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0xffffffffu, 0x3fdfffffu},
        {0x3effffffu, 0x000037ffu, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0xffffffffu, 0x3fefffffu},
        {0x3f7fffffu, 0x00003bffu, 0xffffffffu, 0x3fefdfffu},
        {0x3f6fffffu, 0x00003a00u, 0xffbfffffu, 0x3fefffffu},
        {0x3f7fffeau, 0x00003ba0u, 0xfa31d000u, 0x3fdd9172u},
        {0x3f0e9a40u, 0x00003befu, 0xfffce295u, 0x3fefffffu},
        {0x3e944e80u, 0x00003700u, 0xc0200000u, 0x3fedda9bu},
        {0x3f7ffff0u, 0x00003bfeu, 0xaedf75edu, 0x3fef0fcfu},
        {0x3f7ff0d7u, 0x00003bfeu, 0x00000000u, 0x3fe25800u},
        {0x3e4d8000u, 0x00003bffu, 0x60000000u, 0x3facaebcu},
        {0x3ea52c00u, 0x00003924u, 0xfe89f72au, 0x3fefffffu},
        {0x00000000u, 0x00003b65u, 0x7a7fa323u, 0x3fefffa6u},
        {0x3eb0bc00u, 0x0000397fu, 0xd8000000u, 0x3fe07d06u},
        {0x3ec00000u, 0x00003a05u, 0xffffffffu, 0x3fefffffu},
        {0x00000000u, 0x00003a32u, 0x00000000u, 0x3fe1cbe1u},
        {0x00000000u, 0x00003800u, 0x3cd828aau, 0x3fefffffu}
    }
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
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
    const auto& expected = Expected[rounding - 1u];
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Values[tid * Words];
        const std::uint32_t* out = &Output[tid * Words];
        Require(out[0] == expected[tid][0], "v_fract_f32 " + mode + " of " + Hex(in[0]) + " is " + Hex(out[0]) + ", expected " + Hex(expected[tid][0]));
        Require(out[1] == expected[tid][1], "v_fract_f16 " + mode + " of " + Hex(in[1]) + " is " + Hex(out[1]) + ", expected " + Hex(expected[tid][1]));
        const std::uint64_t source = in[2] | static_cast<std::uint64_t>(in[3]) << 32u;
        const std::uint64_t actual = out[2] | static_cast<std::uint64_t>(out[3]) << 32u;
        const std::uint64_t wanted = expected[tid][2] | static_cast<std::uint64_t>(expected[tid][3]) << 32u;
        Require(actual == wanted, "v_fract_f64 " + mode + " of " + Hex(source) + " is " + Hex(actual) + ", expected " + Hex(wanted));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityFloat64)) {
            std::puts("skipped, the device has no shaderFloat64");
            return VulkanTestSkipped;
        }
        std::uint32_t checked = 0u;
        for (std::uint32_t rounding = 1u; rounding < 4u; ++rounding) {
            for (const std::uint32_t denormals : {0xc0u, 0xf0u}) {
                for (const bool ieee : {false, true}) {
                    const std::uint32_t field = rounding | rounding << 2u;
                    const std::string declared = "FLOAT_MODE " + Hex(denormals | field) + (ieee ? ", IEEE=1" : ", IEEE=0");
                    const auto refusal = Run(*device, ShaderRecompiler::ShaderFloatMode{denormals | field, true, ieee, false}, Code);
                    if (refusal) {
                        Require(denormals == 0xf0u && refusal->find("DenormPreserve") != std::string::npos, declared + ": " + *refusal);
                        continue;
                    }
                    Check(rounding, declared);
                    RuntimeCode[0] = 0xbfa40000u | field;
                    std::copy(Code.begin(), Code.end(), RuntimeCode.begin() + 1);
                    const auto runtimeRefusal = Run(*device, ShaderRecompiler::ShaderFloatMode{denormals, true, ieee, false}, RuntimeCode);
                    Require(!runtimeRefusal, "s_round_mode " + Hex(field) + " after " + declared + ": " + runtimeRefusal.value_or(""));
                    Check(rounding, "s_round_mode " + Hex(field) + " after FLOAT_MODE " + Hex(denormals) + (ieee ? ", IEEE=1" : ", IEEE=0"));
                    checked += 2u;
                }
            }
        }
        std::printf("v_fract_f32, v_fract_f16 and v_fract_f64 directed rounding tests passed (%u of 24 runs, declared and set by s_round_mode)\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
