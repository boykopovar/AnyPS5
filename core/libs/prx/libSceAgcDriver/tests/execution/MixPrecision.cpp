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
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Float64Capability = 10;
constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 52> MixCode{
    0x34020082, 0x34060084, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xe030200c, 0x80000701, 0x7e2002ff, 0xabcd0000, 0x7e2202ff, 0x0000abcd, 0xbf8c3f70, 0xcc20400a,
    0x1c1a0b04, 0xcc20780b, 0x1c1a0b04, 0xcc20400c, 0x941a0af2, 0xcc20700d, 0x941a0af2, 0xcc20100e,
    0x141e0907, 0xcc204a0f, 0x1c1a0b04, 0xcc214010, 0x1c1a0b04, 0xcc227811, 0x1c1a0b04, 0xcc203812,
    0x041e0f07, 0xe0702000, 0x80010a03, 0xe0702004, 0x80010b03, 0xe0702008, 0x80010c03, 0xe070200c,
    0x80010d03, 0xe0702010, 0x80010e03, 0xe0702014, 0x80010f03, 0xe0702018, 0x80011003, 0xe070201c,
    0x80011103, 0xe0702020, 0x80011203, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 52> RoundingCode{
    0x34020082, 0x34060084, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0x7e1402ff, 0xabcd0000, 0x7e1602ff, 0x0000abcd, 0x7e1802ff, 0xabcd0000, 0x7e1a02ff, 0xabcd0000,
    0x7e1c02ff, 0xabcd0000, 0x7e1e02ff, 0xabcd0000, 0x7e2002ff, 0xabcd0000, 0xbf8c3f70, 0xcc21000a,
    0x041a0b04, 0xcc22000b, 0x041a0b04, 0xcc21400c, 0x1c1a0b04, 0xcc21000d, 0x1c1a0b04, 0xcc21400e,
    0x041a0b04, 0xcc21400f, 0x0c1a0b04, 0xcc210010, 0x141a0b04, 0xe0702000, 0x80010a03, 0xe0702004,
    0x80010b03, 0xe0702008, 0x80010c03, 0xe070200c, 0x80010d03, 0xe0702010, 0x80010e03, 0xe0702014,
    0x80010f03, 0xe0702018, 0x80011003, 0xbf810000,
};

struct RoundingRow {
    std::uint32_t a, b, c, result, expected;
};

constexpr std::array<RoundingRow, Threads> RoundingRows{{
    {0x000062e9u, 0x0000d159u, 0x000023ecu, 2u, 0xf89fu},
    {0xbfc00000u, 0x33800000u, 0x00800000u, 0u, 0x8001u},
    {0x3bec0000u, 0xc73c0000u, 0xb767d928u, 0u, 0xdd6bu},
    {0xb4e70000u, 0xc5500000u, 0x8d000000u, 0u, 0x15ddu},
    {0xcc750000u, 0xb0500000u, 0x2f800000u, 0u, 0x2a39u},
    {0xc7b30000u, 0x35500000u, 0x0d000000u, 0u, 0xac8bu},
    {0x36400000u, 0x4c414000u, 0xb3800000u, 0u, 0x5887u},
    {0x4ae00000u, 0x3604c000u, 0x00800000u, 0u, 0x4b43u},
    {0xcf840000u, 0x36fc0000u, 0x31054000u, 0u, 0xf80fu},
    {0x43320000u, 0x41580000u, 0xaf800000u, 0u, 0x68b1u},
    {0x38400000u, 0x34406b26u, 0x43ab9000u, 0u, 0x5d5du},
    {0x00007780u, 0x00001478u, 0x00000002u, 2u, 0x5031u},
    {0x0000e520u, 0x0000cc10u, 0x00000002u, 2u, 0x7535u},
    {0x00004138u, 0x0000e440u, 0x00000001u, 2u, 0xe98bu},
    {0x000020ccu, 0x0000d880u, 0x00000001u, 2u, 0xbd65u},
    {0x00004880u, 0x0000d81cu, 0x00000002u, 2u, 0xe49fu},
    {0x0000cad4u, 0x00005900u, 0x00008002u, 2u, 0xe845u},
    {0x00001836u, 0x0000b5b3u, 0x00003a9au, 2u, 0x3a99u},
    {0x000063f0u, 0x00005340u, 0x00008002u, 2u, 0x7b31u},
    {0x0000e700u, 0x0000cca4u, 0x00008100u, 2u, 0x780fu},
    {0x00000003u, 0x0000f89du, 0x0d000000u, 3u, 0x9eebu},
    {0x00009d20u, 0x000036a0u, 0x8d000000u, 3u, 0x983fu},
    {0x000008a5u, 0x0000b400u, 0x80800000u, 3u, 0x8253u},
    {0x00009400u, 0x00002935u, 0x8d000000u, 3u, 0x829bu},
    {0x0000c5c0u, 0x00004630u, 0xaf800000u, 3u, 0xd073u},
    {0x0000140du, 0x00000c40u, 0x42e11000u, 3u, 0x5709u},
    {0x00008800u, 0x0000b439u, 0x00800000u, 3u, 0x021du},
    {0x00006c80u, 0x00003c04u, 0x33800000u, 3u, 0x6c85u},
    {0x00002840u, 0x0000f018u, 0x33800000u, 3u, 0xdc59u},
    {0x4f200000u, 0x34d98000u, 0x00008100u, 4u, 0x643fu},
    {0xced08000u, 0x37900000u, 0x00008100u, 4u, 0xf755u},
    {0xb4b63000u, 0xce800000u, 0x00008100u, 4u, 0x5db1u},
    {0xca094000u, 0x38a00000u, 0x00008002u, 4u, 0xd95du},
    {0x37fb0000u, 0xcdb00000u, 0x00008100u, 4u, 0xf165u},
    {0x46c00000u, 0xbe374000u, 0x00000002u, 4u, 0xec4bu},
    {0x48580000u, 0xbdf60000u, 0x00008822u, 4u, 0xf67du},
    {0x40186000u, 0x46c00000u, 0x00000001u, 4u, 0x7b25u},
    {0xcc500000u, 0x33bd0000u, 0x00008001u, 4u, 0xc4cdu},
    {0x00001889u, 0x45c00000u, 0x00008002u, 5u, 0x4acdu},
    {0x0000a007u, 0xc4c00000u, 0x00000001u, 5u, 0x4a0bu},
    {0x0000a87du, 0x42c00000u, 0x00000002u, 5u, 0xc2bbu},
    {0x00003018u, 0xc6680000u, 0x00000100u, 5u, 0xe76bu},
    {0x0000d61eu, 0x3dc00000u, 0x00008001u, 5u, 0xc897u},
    {0x00002888u, 0xc7480000u, 0x00008100u, 5u, 0xe715u},
    {0x0000c064u, 0xc0900000u, 0x00000001u, 5u, 0x48f1u},
    {0x000074deu, 0xbe200000u, 0x00000001u, 5u, 0xea15u},
    {0x0000d86du, 0xc1400000u, 0x00008100u, 5u, 0x66a3u},
    {0x48c00000u, 0x000018dfu, 0x00800000u, 6u, 0x634fu},
    {0x41e00000u, 0x0000a002u, 0x2f800000u, 6u, 0xb303u},
    {0xb2f61000u, 0x00004094u, 0xc23ed000u, 6u, 0xd1f7u},
    {0xb5200000u, 0x0000941eu, 0xc575b000u, 6u, 0xebadu},
    {0xc1600000u, 0x000003dau, 0x0d000000u, 6u, 0x92bdu},
    {0x4a500000u, 0x00001894u, 0x33800000u, 6u, 0x6f71u},
    {0x3ce00000u, 0x0000681au, 0xaf800000u, 6u, 0x532du},
    {0x43a00000u, 0x00002a02u, 0x34079c43u, 6u, 0x4b83u},
    {0x43c00000u, 0x0000c81bu, 0xb3800000u, 6u, 0xea29u},
    {0x1f000000u, 0x1f000000u, 0x3f801000u, 0u, 0x3c01u},
    {0x1f000000u, 0x9f000000u, 0x3f801000u, 0u, 0x3c00u},
    {0x00800000u, 0x3e800000u, 0x3f801000u, 0u, 0x3c01u},
    {0x33000000u, 0x3f800000u, 0x00800000u, 0u, 0x0001u},
    {0x33000000u, 0x3f800000u, 0x80800000u, 0u, 0x0000u},
    {0x477ff000u, 0x3f800000u, 0xa6800000u, 0u, 0x7bffu},
    {0x7f7fffffu, 0x40000000u, 0xff7fffffu, 0u, 0x7c00u},
    {0x00003c01u, 0x00004200u, 0x00800000u, 3u, 0x4202u},
}};

float Quarter(std::uint32_t index) {
    return static_cast<float>(static_cast<int>(index % 17u) - 8) / 4.0f;
}

std::uint16_t HalfBits(float value) {
    if (value == 0.0f) return std::signbit(value) ? 0x8000u : 0u;
    int exponent = 0;
    const float mantissa = std::frexp(std::fabs(value), &exponent);
    const auto fraction = static_cast<std::uint32_t>(std::ldexp(mantissa, 11));
    Require(exponent >= -13 && exponent <= 16 && std::ldexp(static_cast<float>(fraction), exponent - 11) == std::fabs(value), "mix precision: a model value is not a normal f16");
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
        words[2] = Pack(Quarter(tid * 11u + 9u), Quarter(tid * 13u + 4u));
        words[3] = std::bit_cast<std::uint32_t>(Quarter(tid * 2u + 7u));
    }
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void FillRoundingInput() {
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto& row = RoundingRows[tid];
        std::copy_n(std::array<std::uint32_t, 3>{row.a, row.b, row.c}.begin(), 3, Input.begin() + tid * Inputs);
    }
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
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
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto* in = &Input[tid * Inputs];
        const auto* out = &Output[tid * Results];
        const float d = std::bit_cast<float>(in[3]);
        const std::array<float, 6> expected{
            std::fma(Low(in[0]), Low(in[1]), Low(in[2])),
            std::fma(High(in[0]), High(in[1]), High(in[2])),
            Low(in[1]) - Low(in[2]),
            High(in[1]) - High(in[2]),
            std::fma(d, High(in[0]), d),
            std::fma(High(in[0]), std::fabs(Low(in[1])), Low(in[2])),
        };
        const auto where = [&](std::uint32_t result) { return "mix precision: thread " + std::to_string(tid) + " result " + std::to_string(result); };
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            const float actual = std::bit_cast<float>(out[j]);
            Require(actual == expected[j], where(j) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[j]));
        }
        Require((out[6] >> 16u) == 0xabcdu && Low(out[6]) == expected[0], where(6) + " is " + Hex(out[6]) + ": v_fma_mixlo_f16 must write the low half only");
        Require((out[7] & 0xffffu) == 0xabcdu && High(out[7]) == expected[1], where(7) + " is " + Hex(out[7]) + ": v_fma_mixhi_f16 must write the high half only");
        const float square = std::bit_cast<float>(out[8]);
        Require(square == std::fma(d, d, d), where(8) + " is " + std::to_string(square) + ", expected " + std::to_string(std::fma(d, d, d)));
    }
}

void CheckRounding() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto& row = RoundingRows[tid];
        const auto* out = &Output[tid * Results];
        const std::uint32_t expected = 0xabcd0000u | row.expected;
        const auto where = "mix rounding: thread " + std::to_string(tid) + " (" + Hex(row.a) + ", " + Hex(row.b) + ", " + Hex(row.c) + ") result " + std::to_string(row.result);
        Require(out[row.result] == expected, where + " is " + Hex(out[row.result]) + ", expected " + Hex(expected));
        if (row.result == 0u) {
            Require(out[1] == ((row.expected << 16u) | 0xabcdu), where + ": v_fma_mixhi_f16 is " + Hex(out[1]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto capabilities = device->Target().supportedCapabilities;
        if (std::find(capabilities.begin(), capabilities.end(), Float64Capability) == capabilities.end()) {
            std::puts("skipped, v_fma_mixlo_f16 and v_fma_mixhi_f16 are computed in f64 and the device has no shaderFloat64");
            return VulkanTestSkipped;
        }
        FillInput();
        Run(*device, MixCode);
        Check();
        FillRoundingInput();
        Run(*device, RoundingCode);
        CheckRounding();
        std::puts("mix precision tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
