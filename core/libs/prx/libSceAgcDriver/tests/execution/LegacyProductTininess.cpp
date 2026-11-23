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

constexpr std::array<std::uint32_t, Threads * Words> Values{
    0xbb57f116u, 0x0497bea0u, 0x80800000u, 0x00000000u,
    0xbb952f34u, 0x045ba5ccu, 0x00000000u, 0x00000000u,
    0x3ecd3e0bu, 0x811fa7b8u, 0x80800000u, 0x00000000u,
    0x3bf2f73fu, 0x0406dde1u, 0x80800000u, 0x00000000u,
    0xb9c40360u, 0x06272c24u, 0x80800000u, 0x00000000u,
    0x3f4bcb6fu, 0x80a0ca0fu, 0x80800000u, 0x00000000u,
    0xbbc6f27du, 0x8424b4feu, 0x80000000u, 0x00000000u,
    0xbcd85fa2u, 0x83177119u, 0x80000000u, 0x00000000u,
    0x3a7f5c0fu, 0x0580522du, 0x01000000u, 0x00000000u,
    0xbf6b14a4u, 0x808b63f6u, 0x81000000u, 0x00000000u,
    0xbe7c8123u, 0x8181c5a0u, 0x00000000u, 0x00000000u,
    0xba207098u, 0x85cc3d13u, 0x00800000u, 0x00000000u,
    0x3c96fde4u, 0x835904bau, 0x81000000u, 0x00000000u,
    0xbc844c0au, 0x8377af61u, 0x01000000u, 0x00000000u,
    0x3d983d19u, 0x02573db1u, 0x80000000u, 0x00000000u,
    0x3d56c9ebu, 0x02988f28u, 0x3f800000u, 0x00000000u,
    0xbda9b74au, 0x02411354u, 0x80800000u, 0x00000000u,
    0xbcdf1f00u, 0x8312dca2u, 0x00000000u, 0x00000000u,
    0x3a65a591u, 0x058eb04cu, 0x00000000u, 0x00000000u,
    0x3ce2ce8cu, 0x031079b1u, 0x00000000u, 0x00000000u,
    0x39cbc149u, 0x8620d207u, 0x80000000u, 0x00000000u,
    0x3b84e8b7u, 0x04768b66u, 0x00000000u, 0x00000000u,
    0xbea98e5au, 0x814141f0u, 0x80800000u, 0x00000000u,
    0x3f55c81cu, 0x00994722u, 0x80800000u, 0x00000000u,
    0x3b0b28cau, 0x04eb789au, 0x80000000u, 0x00000000u,
    0x3aec35d9u, 0x050ab94cu, 0x3f800000u, 0x00000000u,
    0xbd55d045u, 0x82994149u, 0x3f800000u, 0x00000000u,
    0xbb3c58c0u, 0x04adfa21u, 0x3f800000u, 0x00000000u,
    0x3fc00000u, 0x40000000u, 0xbf800000u, 0x00000000u,
    0x80000000u, 0x7f800000u, 0x3f800000u, 0x00000000u,
    0x3f800000u, 0x00400000u, 0x00800000u, 0x00000000u,
    0xc0490fdbu, 0x3f000000u, 0x40490fdbu, 0x00000000u
};

constexpr std::array<std::uint32_t, Threads> Expected{
    0x81000000u, 0x80800000u, 0x81000000u, 0x00000000u, 0x81000000u, 0x81000000u, 0x00800000u, 0x00800000u,
    0x01400000u, 0x80800000u, 0x00800000u, 0x01000000u, 0x81400000u, 0x01400000u, 0x00800000u, 0x3f800000u,
    0x80800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x80800000u, 0x80800000u,
    0x00800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x40000000u, 0x3f800000u, 0x00800000u, 0x3fc90fdbu
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

std::optional<std::string> Run(AgcDriver::VulkanDevice& device, ShaderRecompiler::ShaderFloatMode floatMode) {
    Input = Values;
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

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::uint32_t checked = 0u;
        for (const std::uint32_t denormals : {0xc0u, 0xd0u, 0xe0u, 0xf0u}) {
            for (const bool ieee : {false, true}) {
                const std::string mode = "FLOAT_MODE " + Hex(denormals) + (ieee ? ", IEEE=1" : ", IEEE=0");
                const auto refusal = Run(*device, ShaderRecompiler::ShaderFloatMode{denormals, true, ieee, false});
                if (refusal) {
                    Require(denormals != 0xc0u && refusal->find("DenormPreserve") != std::string::npos, mode + ": " + *refusal);
                    continue;
                }
                for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                    const std::uint32_t* in = &Values[tid * Words];
                    const std::uint32_t* out = &Output[tid * Words];
                    const std::string operands = " of " + Hex(in[0]) + ", " + Hex(in[1]) + ", " + Hex(in[2]) + " is ";
                    Require(out[0] == Expected[tid], "v_mad_legacy_f32 " + mode + operands + Hex(out[0]) + ", expected " + Hex(Expected[tid]));
                    Require(out[1] == Expected[tid], "v_mac_legacy_f32 " + mode + operands + Hex(out[1]) + ", expected " + Hex(Expected[tid]));
                }
                ++checked;
            }
        }
        std::printf("v_mad_legacy_f32 and v_mac_legacy_f32 product tininess tests passed (%u of 8 float modes)\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
