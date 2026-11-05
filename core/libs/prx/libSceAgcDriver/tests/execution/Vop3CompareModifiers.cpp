#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 73> ModifierCode{
    0x34020082, 0x34060082, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000e01,
    0xe030200c, 0x80000f01, 0x7e140280, 0xbf8c3f70, 0xd401006a, 0x20020b04, 0xd5010014, 0x01a90280,
    0xd76f000a, 0x04290114, 0xd404016a, 0x60020b04, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04290314,
    0xd409026a, 0x40020b04, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04290514, 0xd4020014, 0x20020b04,
    0xd5010014, 0x00510280, 0xd76f000a, 0x04290714, 0xd4ca006a, 0x20021f0e, 0xd5010014, 0x01a90280,
    0xd76f000a, 0x04290914, 0xd4c9016a, 0x60021f0e, 0xd5010014, 0x01a90280, 0xd76f000a, 0x04290b14,
    0xd4ed0014, 0x40021f0e, 0xd5010014, 0x00510280, 0xd76f000a, 0x04290d14, 0xbe9e037e, 0x7e280280,
    0xd413007e, 0x20020b04, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04290f14, 0xbe9e037e, 0x7e280280,
    0xd4dc027e, 0x20021f0e, 0x7e280281, 0xbefe031e, 0xd76f000a, 0x04291114, 0xe0702000, 0x80010a03,
    0xbf810000,
};

constexpr std::array<std::uint32_t, 12> FloatEdges{
    0x7fc00000u, 0x3f800000u, 0x80000000u, 0x00000000u, 0x7f800000u, 0xff800000u,
    0xbf800000u, 0x3f800000u, 0x00800000u, 0x40490fdbu, 0xffc00001u, 0xc0000000u,
};

constexpr std::array<std::uint16_t, 12> HalfEdges{
    0x7e00u, 0x3c00u, 0x8000u, 0x0000u, 0x7c00u, 0xfc00u, 0xbc00u, 0x3c00u, 0x0001u, 0x4248u, 0xfe01u, 0xc000u,
};

float HalfValue(std::uint32_t bits) {
    const auto exponent = (bits >> 10u) & 0x1fu;
    const auto fraction = bits & 0x3ffu;
    float magnitude = 0.0f;
    if (exponent == 0x1fu) {
        magnitude = fraction != 0u ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
    } else {
        magnitude = exponent == 0u ? std::ldexp(static_cast<float>(fraction), -24) : std::ldexp(static_cast<float>(fraction | 0x400u), static_cast<int>(exponent) - 25);
    }
    return (bits & 0x8000u) != 0u ? -magnitude : magnitude;
}

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        words[0] = FloatEdges[tid % FloatEdges.size()];
        words[1] = FloatEdges[(tid / FloatEdges.size() + tid * 7u) % FloatEdges.size()] ^ ((tid & 4u) != 0u ? 0x80000000u : 0u);
        words[2] = 0xabcd0000u | HalfEdges[tid % HalfEdges.size()];
        words[3] = 0x12340000u | (HalfEdges[(tid / HalfEdges.size() + tid * 5u) % HalfEdges.size()] ^ ((tid & 8u) != 0u ? 0x8000u : 0u));
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(ModifierCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto* in = &Input[tid * Inputs];
        const float fa = std::bit_cast<float>(in[0]);
        const float fb = std::bit_cast<float>(in[1]);
        const float ha = HalfValue(in[2] & 0xffffu);
        const float hb = HalfValue(in[3] & 0xffffu);
        const std::array<bool, 9> expected{
            -fa < fb,
            -std::fabs(fa) > -fb,
            !(fa >= -std::fabs(fb)),
            -fa == fb,
            -ha == hb,
            -std::fabs(ha) < -hb,
            !(ha == -hb),
            -fa <= fb,
            -ha > std::fabs(hb),
        };
        for (std::uint32_t k = 0; k < expected.size(); ++k) {
            const bool actual = ((Output[tid * Results] >> k) & 1u) != 0u;
            Require(actual == expected[k], "vop3 compare modifiers: thread " + std::to_string(tid) + " compare " + std::to_string(k) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[k]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        Run(*device);
        Check();
        std::puts("vop3 compare modifier tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
