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
constexpr std::uint32_t Words = 4;
constexpr std::uint32_t Results = 2;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Words> Output{};

alignas(256) constexpr std::array<std::uint32_t, 11> Code{
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e146b04u, 0x7e166d04u, 0x7e180280u, 0x7e1a0280u,
    0xe0781000u, 0x80010a01u, 0xbf810000u,
};

constexpr const char* Names[Results] = {"v_sin_f32", "v_cos_f32"};

constexpr std::uint32_t Values[Threads] = {
    0x00000000u, 0x00000001u, 0x00000002u, 0x00145f2fu, 0x00145f30u, 0x00145f31u, 0x00400000u, 0x007fffffu,
    0x00800000u, 0x00800001u, 0x01000000u, 0x2f800000u, 0x397fffffu, 0x39800000u, 0x39800001u, 0x3e800000u,
    0x3f000000u, 0x3f800000u, 0x4b000000u, 0x7f7fffffu, 0x7f800000u, 0x7fc00000u, 0x80000000u, 0x80000001u,
    0x80000002u, 0x80145f2fu, 0x80145f30u, 0x80145f31u, 0x80400000u, 0x807fffffu, 0x80800000u, 0x80800001u,
    0x81000000u, 0xaf800000u, 0xb97fffffu, 0xb9800000u, 0xb9800001u, 0xbe800000u, 0xbf000000u, 0xbf800000u,
    0xcb000000u, 0xff7fffffu, 0xff800000u, 0xffc00000u, 0x00013b4cu, 0x0001cb93u, 0x000255e7u, 0x0003fa33u,
    0x0006001au, 0x000632beu, 0x0006aba4u, 0x0008689du, 0x000c2a12u, 0x000e0b06u, 0x8010b84au, 0x8010f5f6u,
    0x8016de0fu, 0x80217d1du, 0x8026573eu, 0x8026db6eu, 0x8027045bu, 0x802e9a1cu, 0x80321ba8u, 0x8037909cu
};
constexpr std::uint32_t Mode1Expected[Threads][Results] = {
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00800000u, 0x3f800000u},
    {0x01490fd5u, 0x3f800000u},
    {0x01c90fd3u, 0x3f800000u},
    {0x01c90fd5u, 0x3f800000u},
    {0x01c90fd7u, 0x3f800000u},
    {0x02490fd5u, 0x3f800000u},
    {0x30c90fd5u, 0x3f800000u},
    {0x3ac90fd4u, 0x3f7fffecu},
    {0x3ac90fd5u, 0x3f7fffecu},
    {0x3ac90fd7u, 0x3f7fffecu},
    {0x3f800000u, 0x00000000u},
    {0x00000000u, 0xbf800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0xffc00000u, 0xffc00000u},
    {0x7fc00000u, 0x7fc00000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80800000u, 0x3f800000u},
    {0x81490fd5u, 0x3f800000u},
    {0x81c90fd3u, 0x3f800000u},
    {0x81c90fd5u, 0x3f800000u},
    {0x81c90fd7u, 0x3f800000u},
    {0x82490fd5u, 0x3f800000u},
    {0xb0c90fd5u, 0x3f800000u},
    {0xbac90fd4u, 0x3f7fffecu},
    {0xbac90fd5u, 0x3f7fffecu},
    {0xbac90fd7u, 0x3f7fffecu},
    {0xbf800000u, 0x00000000u},
    {0x00000000u, 0xbf800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0xffc00000u, 0xffc00000u},
    {0xffc00000u, 0xffc00000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x808fae1fu, 0x3f800000u},
    {0x80d26a70u, 0x3f800000u},
    {0x80f0e6f6u, 0x3f800000u},
    {0x80f42584u, 0x3f800000u},
    {0x80f526a9u, 0x3f800000u},
    {0x81126787u, 0x3f800000u},
    {0x811d6b41u, 0x3f800000u},
    {0x812e8fe9u, 0x3f800000u}
};
constexpr std::uint32_t Mode2Expected[Threads][Results] = {
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x01c90fd5u, 0x3f800000u},
    {0x01c90fd7u, 0x3f800000u},
    {0x02490fd5u, 0x3f800000u},
    {0x30c90fd5u, 0x3f800000u},
    {0x3ac90fd4u, 0x3f7fffecu},
    {0x3ac90fd5u, 0x3f7fffecu},
    {0x3ac90fd7u, 0x3f7fffecu},
    {0x3f800000u, 0x00000000u},
    {0x00000000u, 0xbf800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0xffc00000u, 0xffc00000u},
    {0x7fc00000u, 0x7fc00000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x81c90fd5u, 0x3f800000u},
    {0x81c90fd7u, 0x3f800000u},
    {0x82490fd5u, 0x3f800000u},
    {0xb0c90fd5u, 0x3f800000u},
    {0xbac90fd4u, 0x3f7fffecu},
    {0xbac90fd5u, 0x3f7fffecu},
    {0xbac90fd7u, 0x3f7fffecu},
    {0xbf800000u, 0x00000000u},
    {0x00000000u, 0xbf800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0xffc00000u, 0xffc00000u},
    {0xffc00000u, 0xffc00000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x00000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u},
    {0x80000000u, 0x3f800000u}
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
            const std::uint32_t actual = Output[tid * Words + i];
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
        std::printf("f32 sin and cos denormal mode 1 and 2 tests passed (%u of 4 modes run; the others need DenormPreserve 32)\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
