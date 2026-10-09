#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 52> AliasCode{
    0x34020082, 0x34060084, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0x7e1402ff, 0xabcd0000, 0x7e1602ff, 0xabcd0000, 0x7e1802ff, 0xabcd0000, 0xbf8c3f70, 0xd539020a,
    0x20020b04, 0xd53a010b, 0x40020b04, 0xd535010c, 0x20020b04, 0xd58b000d, 0x10000105, 0xd58b010e,
    0x20000104, 0xd591000f, 0x08000106, 0xd5920010, 0x18000106, 0xd5938011, 0x00000106, 0xd5940012,
    0x00000106, 0xe0702000, 0x80010a03, 0xe0702004, 0x80010b03, 0xe0702008, 0x80010c03, 0xe070200c,
    0x80010d03, 0xe0702010, 0x80010e03, 0xe0702014, 0x80010f03, 0xe0702018, 0x80011003, 0xe070201c,
    0x80011103, 0xe0702020, 0x80011203, 0xbf810000,
};

float Quarter(std::uint32_t index) {
    return static_cast<float>(static_cast<int>(index % 17u) - 8) / 4.0f;
}

std::uint16_t HalfBits(float value) {
    if (value == 0.0f) return std::signbit(value) ? 0x8000u : 0u;
    int exponent = 0;
    const float mantissa = std::frexp(std::fabs(value), &exponent);
    const auto fraction = static_cast<std::uint32_t>(std::ldexp(mantissa, 11));
    Require(exponent >= -13 && exponent <= 16 && std::ldexp(static_cast<float>(fraction), exponent - 11) == std::fabs(value), "vop3 aliases: a model value is not a normal f16");
    return static_cast<std::uint16_t>((std::signbit(value) ? 0x8000u : 0u) | (static_cast<std::uint32_t>(exponent + 14) << 10u) | (fraction & 0x3ffu));
}

float HalfValue(std::uint32_t bits) {
    const auto exponent = (bits >> 10u) & 0x1fu;
    const auto fraction = bits & 0x3ffu;
    const float magnitude = exponent == 0u ? std::ldexp(static_cast<float>(fraction), -24) : std::ldexp(static_cast<float>(fraction | 0x400u), static_cast<int>(exponent) - 25);
    return (bits & 0x8000u) != 0u ? -magnitude : magnitude;
}

std::uint32_t Pack(float low, float high) {
    return static_cast<std::uint32_t>(HalfBits(low)) | (static_cast<std::uint32_t>(HalfBits(high)) << 16u);
}

float Low(std::uint32_t word) {
    return HalfValue(word & 0xffffu);
}

float High(std::uint32_t word) {
    return HalfValue(word >> 16u);
}

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        words[0] = Pack(Quarter(tid), Quarter(tid * 3u + 5u));
        words[1] = Pack(Quarter(tid * 5u + 2u), Quarter(tid * 7u + 11u));
        words[2] = (tid * 0x01030507u) ^ 0x80c0e0f0u;
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
    const std::span<const std::uint32_t> code(AliasCode);
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
        const auto* out = &Output[tid * Results];
        const auto byte = [&](std::uint32_t index) { return static_cast<float>((in[2] >> (index * 8u)) & 0xffu); };
        const std::array<float, 3> halves{
            std::max(-Low(in[0]), std::fabs(Low(in[1]))),
            std::min(std::fabs(Low(in[0])), -Low(in[1])),
            -std::fabs(Low(in[0])) * Low(in[1]),
        };
        const std::array<float, 6> floats{
            Low(in[1]) * 4.0f,
            -std::fabs(Low(in[0])),
            byte(0) * 2.0f,
            byte(1) / 2.0f,
            std::min(byte(2), 1.0f),
            byte(3),
        };
        const auto where = [&](std::uint32_t result) { return "vop3 aliases: thread " + std::to_string(tid) + " result " + std::to_string(result); };
        for (std::uint32_t j = 0; j < halves.size(); ++j) {
            Require(Low(out[j]) == halves[j], where(j) + " is " + std::to_string(Low(out[j])) + ", expected " + std::to_string(halves[j]));
        }
        for (std::uint32_t j = 0; j < floats.size(); ++j) {
            const float actual = std::bit_cast<float>(out[halves.size() + j]);
            Require(actual == floats[j], where(halves.size() + j) + " is " + std::to_string(actual) + ", expected " + std::to_string(floats[j]));
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
        std::puts("vop3 aliases tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
