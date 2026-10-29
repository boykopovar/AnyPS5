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

alignas(256) constexpr std::array<std::uint32_t, 15> Code{
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd54b000au, 0x041a0b04u, 0x7e160306u, 0x56160b04u,
    0x58180d04u, 0x3f7fffffu, 0x5a1a0b04u, 0x80000001u, 0xe0781000u, 0x80010a01u, 0xbf810000u,
};

constexpr const char* Names[Results] = {"v_fma_f32 v10, v4, v5, v6", "v_fmac_f32 v11, v4, v5", "v_fmamk_f32 v12, v4, 0x3f7fffff, v6", "v_fmaak_f32 v13, v4, v5, 0x80000001"};

constexpr std::uint32_t Rows[Threads][3] = {
    {0x00000000u, 0x7f800000u, 0x3f000000u},
    {0x00800000u, 0x00000000u, 0x00000000u},
    {0x00800000u, 0x00000000u, 0x80000000u},
    {0x00800000u, 0x00000000u, 0x00000001u},
    {0x00800000u, 0x00000000u, 0x807fffffu},
    {0x00800000u, 0x00000000u, 0x00400000u},
    {0x00800000u, 0x00000000u, 0x80800001u},
    {0x00800000u, 0x80000000u, 0x00000000u},
    {0x00800000u, 0x80000000u, 0x80000000u},
    {0x00800000u, 0x00400000u, 0x00000001u},
    {0x00800000u, 0x00800000u, 0x80000000u},
    {0x00800000u, 0x3f800000u, 0x80800001u},
    {0x00800000u, 0x3f7fffffu, 0x00000000u},
    {0x00800000u, 0x3f7fffffu, 0x80000000u},
    {0x00800000u, 0x3f7fffffu, 0x00000001u},
    {0x00800000u, 0x3f7fffffu, 0x807fffffu},
    {0x00800000u, 0x3f7fffffu, 0x00400000u},
    {0x00800000u, 0x3f7fffffu, 0x00800000u},
    {0x00800000u, 0x3f7fffffu, 0x80800001u},
    {0x00800000u, 0x3f7fffffu, 0x3f800000u},
    {0x00800000u, 0x3f7fffffu, 0xbf800000u},
    {0x00800000u, 0x3f7fffffu, 0x3f7fffffu},
    {0x00800000u, 0x3f7fffffu, 0x3f000000u},
    {0x00800000u, 0x3f7fffffu, 0x1f800000u},
    {0x00800000u, 0x3f7fffffu, 0x9fffffffu},
    {0x00800000u, 0x3f7fffffu, 0x20000000u},
    {0x00800000u, 0x3f000000u, 0x00000000u},
    {0x00800000u, 0x3f000000u, 0x80000000u},
    {0x00800000u, 0x3f000000u, 0x00000001u},
    {0x00800000u, 0x3f000000u, 0x807fffffu},
    {0x00800000u, 0x3f000000u, 0x00400000u},
    {0x00800000u, 0x3f000000u, 0x80800001u},
    {0x00800000u, 0x1f800000u, 0x80000000u},
    {0x80800001u, 0x3f800000u, 0x00800000u},
    {0x80800001u, 0x3f000000u, 0x7f800000u},
    {0x3f800000u, 0x80800001u, 0x00800000u},
    {0x3f800000u, 0x7fc00000u, 0x80800001u},
    {0x3f7fffffu, 0xbf800000u, 0x3f000000u},
    {0x1f800000u, 0x3f800000u, 0x20000000u},
    {0x9fffffffu, 0x00000001u, 0x80000000u},
    {0x9fffffffu, 0x20000000u, 0x00400000u},
    {0x20000000u, 0x3f800000u, 0x00000000u},
    {0x20000000u, 0x9fffffffu, 0x20000000u},
    {0x20000000u, 0x9fffffffu, 0x7fc00000u},
    {0x7f800000u, 0x00400000u, 0x00000001u},
    {0xa041753bu, 0x1f0d790au, 0x80000000u},
    {0x9efedcd2u, 0x9ff05395u, 0x80000000u},
    {0x1f47e4fdu, 0x1eb6f5c4u, 0x80000000u},
    {0x9e9e1749u, 0x1f7a0022u, 0x8086d61eu},
    {0x9fc47dc5u, 0x1f7a84eeu, 0x80ad97ffu},
    {0x9e398753u, 0x20a52a9au, 0x809fead7u},
    {0x1ee684f0u, 0x204b1dd4u, 0x00000000u},
    {0x1e8933deu, 0x205cc4a7u, 0x00000000u},
    {0x1f19a9f4u, 0x9fbc243cu, 0x80000000u},
    {0x9eb3c398u, 0x9f630477u, 0x00000000u},
    {0x1e32d290u, 0x1fa0e0b3u, 0x00000000u},
    {0x1f074330u, 0x9e704ff0u, 0x00000000u},
    {0x1ea63ad2u, 0x1e7c9dc9u, 0x806cc811u},
    {0x9f42587bu, 0xa0dd6a0cu, 0x80000000u},
    {0x202187c6u, 0x1eb02829u, 0x80000000u},
    {0x1fe6f5d7u, 0x208cc5a7u, 0x80df394au},
    {0x1f2af88cu, 0x9e6ce54fu, 0x80000000u},
    {0x1eeac417u, 0x9e8ead57u, 0x00000000u},
    {0x9e406f2cu, 0x20e09d8du, 0x004226bcu}
};
constexpr std::uint32_t Expected[Threads][Results] = {
    {0xffc00000u, 0xffc00000u, 0x3f000000u, 0xffc00000u},
    {0x00000000u, 0x00000000u, 0x00800000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00800000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00800000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00800000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00800000u, 0x00000000u},
    {0x80800001u, 0x80800001u, 0x80000002u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00800000u, 0x80000000u},
    {0x80000000u, 0x80000000u, 0x00800000u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0x00800000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00800000u, 0x00000000u},
    {0x80000001u, 0x80000001u, 0x80000002u, 0x00800000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u},
    {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u},
    {0x01000000u, 0x01000000u, 0x01000000u, 0x00800000u},
    {0x80000002u, 0x80000002u, 0x80000002u, 0x00800000u},
    {0x3f800000u, 0x3f800000u, 0x3f800000u, 0x00800000u},
    {0xbf800000u, 0xbf800000u, 0xbf800000u, 0x00800000u},
    {0x3f7fffffu, 0x3f7fffffu, 0x3f7fffffu, 0x00800000u},
    {0x3f000000u, 0x3f000000u, 0x3f000000u, 0x00800000u},
    {0x1f800000u, 0x1f800000u, 0x1f800000u, 0x00800000u},
    {0x9fffffffu, 0x9fffffffu, 0x9fffffffu, 0x00800000u},
    {0x20000000u, 0x20000000u, 0x20000000u, 0x00800000u},
    {0x00400000u, 0x00400000u, 0x00800000u, 0x00400000u},
    {0x00400000u, 0x00400000u, 0x00800000u, 0x00400000u},
    {0x00400000u, 0x00400000u, 0x00800000u, 0x00400000u},
    {0x00400000u, 0x00400000u, 0x00800000u, 0x00400000u},
    {0x00400000u, 0x00400000u, 0x00800000u, 0x00400000u},
    {0x80400001u, 0x80400001u, 0x80000002u, 0x00400000u},
    {0x00000000u, 0x00000000u, 0x00800000u, 0x00000000u},
    {0x80000001u, 0x80000001u, 0x80000000u, 0x80800001u},
    {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x80400000u},
    {0x80000001u, 0x80000001u, 0x3f7fffffu, 0x80800001u},
    {0x7fc00000u, 0x7fc00000u, 0x3f7fffffu, 0x7fc00000u},
    {0xbefffffeu, 0xbefffffeu, 0x3fbfffffu, 0xbf7fffffu},
    {0x20400000u, 0x20400000u, 0x20400000u, 0x1f800000u},
    {0x80000000u, 0x80000000u, 0x9ffffffeu, 0x80000000u},
    {0x80800000u, 0x80800000u, 0x9ffffffeu, 0x80800000u},
    {0x20000000u, 0x20000000u, 0x1fffffffu, 0x20000000u},
    {0x20000000u, 0x20000000u, 0x20800000u, 0x80800000u},
    {0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x80800000u},
    {0xffc00000u, 0xffc00000u, 0x7f800000u, 0xffc00000u},
    {0x80357485u, 0x80357485u, 0xa041753au, 0x80357485u},
    {0x001de847u, 0x001de847u, 0x9efedcd1u, 0x001de847u},
    {0x0008edcbu, 0x0008edcbu, 0x1f47e4fcu, 0x0008edcbu},
    {0x80907c4bu, 0x80907c4bu, 0x9e9e1748u, 0x8009a62du},
    {0x80ddaa35u, 0x80ddaa35u, 0x9fc47dc4u, 0x80301236u},
    {0x80bdd79du, 0x80bdd79du, 0x9e398752u, 0x801decc6u},
    {0x002db992u, 0x002db992u, 0x1ee684efu, 0x002db992u},
    {0x001d947eu, 0x001d947eu, 0x1e8933ddu, 0x001d947eu},
    {0x801c3ba4u, 0x801c3ba4u, 0x1f19a9f3u, 0x801c3ba4u},
    {0x0009f699u, 0x0009f699u, 0x9eb3c397u, 0x0009f699u},
    {0x00070609u, 0x00070609u, 0x1e32d28fu, 0x00070609u},
    {0x8003f7cau, 0x8003f7cau, 0x1f07432fu, 0x8003f7cau},
    {0x00029021u, 0x00029021u, 0x1ea63ad1u, 0x00029021u},
    {0x00a816e4u, 0x00a816e4u, 0x9f42587au, 0x00a816e4u},
    {0x001bc9acu, 0x001bc9acu, 0x202187c5u, 0x001bc9acu},
    {0x001ec83du, 0x001ec83du, 0x1fe6f5d6u, 0x00fe0187u},
    {0x8004f1b2u, 0x8004f1b2u, 0x1f2af88bu, 0x8004f1b2u},
    {0x800416beu, 0x800416beu, 0x1eeac416u, 0x800416beu},
    {0x802a35edu, 0x802a35edu, 0x9e406f2bu, 0x802a35edu}
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

void Check(bool ieee) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t i = 0; i < Results; ++i) {
            const std::uint32_t actual = Output[tid * Results + i];
            Require(actual == Expected[tid][i], std::string(Names[i]) + " f32 denormal mode 2" + (ieee ? ", IEEE=1" : ", IEEE=0") + " with v4 " + Hex(Rows[tid][0]) + ", v5 " + Hex(Rows[tid][1]) + ", v6 " + Hex(Rows[tid][2]) + " is " + Hex(actual) + ", expected " + Hex(Expected[tid][i]));
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
        for (const bool ieee : {false, true}) {
            const ShaderRecompiler::ShaderFloatMode floatMode{0xe0u, true, ieee, false};
            const auto refusal = Run(*device, floatMode);
            if (refusal) {
                Require(!keepsDenormals && refusal->find("needs DenormPreserve 32") != std::string::npos, "FLOAT_MODE " + Hex(floatMode.floatMode) + ": " + *refusal);
                continue;
            }
            ++checked;
            Check(ieee);
        }
        std::printf("f32 fma denormal mode 2 tests passed (%u of 2 IEEE modes run; the others need DenormPreserve 32)\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
