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

constexpr std::uint32_t Threads = 16;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t RowCount = 16;
constexpr std::uint32_t ModeCount = 3;
constexpr std::uint32_t AccumulatorHigh = 0xabcd0000u;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) std::array<std::uint32_t, 11> Code{
    0x34020084u, 0x34060084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0x7e140306u, 0xd536000au, 0x042a0b04u,
    0xe0781000u, 0x80010a03u, 0xbf810000u,
};

struct Row {
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t accumulator;
};

constexpr Row Rows[RowCount] = {
    {0x3c00u, 0x4000u, 0x3c00u},
    {0x3c00u, 0x4000u, 0xbc00u},
    {0x4000u, 0x4200u, 0xc600u},
    {0x0000u, 0xbc00u, 0x0000u},
    {0x3555u, 0x4200u, 0xbc00u},
    {0x3c00u, 0x3c00u, 0x7c00u},
    {0x7c00u, 0x3c00u, 0xfc00u},
    {0x3c00u, 0x3c00u, 0x7e01u},
    {0x7e01u, 0x3c00u, 0x3c00u},
    {0x3c00u, 0x0001u, 0x0000u},
    {0x4000u, 0x3800u, 0xbc00u},
    {0x3c01u, 0x3c01u, 0xbc00u},
    {0x7bffu, 0x4000u, 0xfbffu},
    {0x5c00u, 0x5c00u, 0x0000u},
    {0x3800u, 0x3800u, 0x3400u},
    {0xbc00u, 0x4000u, 0x4000u},
};

constexpr std::uint32_t ExpectedM1V0[RowCount] = {
    0xabcd4200u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd8c00u, 0xabcd7c00u, 0xabcdfe00u, 0xabcd7e01u,
    0xabcd7e01u, 0xabcd0001u, 0xabcd0000u, 0xabcd1800u,
    0xabcd7bffu, 0xabcd7c00u, 0xabcd3800u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM2V0[RowCount] = {
    0xabcd4200u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd8c00u, 0xabcd7c00u, 0xabcdfe00u, 0xabcd7e01u,
    0xabcd7e01u, 0xabcd0001u, 0xabcd0000u, 0xabcd1800u,
    0xabcd7bffu, 0xabcd7c00u, 0xabcd3800u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM3V0[RowCount] = {
    0xabcd4200u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd8c00u, 0xabcd7c00u, 0xabcdfe00u, 0xabcd7e01u,
    0xabcd7e01u, 0xabcd0000u, 0xabcd0000u, 0xabcd1800u,
    0xabcd7bffu, 0xabcd7c00u, 0xabcd3800u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM1V1[RowCount] = {
    0xabcd0000u, 0xabcd0000u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd0000u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd0000u, 0xabcd0000u, 0xabcd3c00u,
};
constexpr std::uint32_t ExpectedM2V1[RowCount] = {
    0xabcd0000u, 0xabcd0000u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd0000u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd0000u, 0xabcd0000u, 0xabcd3c00u,
};
constexpr std::uint32_t ExpectedM3V1[RowCount] = {
    0xabcd0000u, 0xabcd0000u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd0000u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd0000u, 0xabcd0000u, 0xabcd3c00u,
};
constexpr std::uint32_t ExpectedM1V2[RowCount] = {
    0xabcdbc00u, 0xabcdc200u, 0xabcdca00u, 0xabcd0000u,
    0xabcdc000u, 0xabcd7c00u, 0xabcdfc00u, 0xabcd7e01u,
    0xabcdfe01u, 0xabcd8001u, 0xabcdc000u, 0xabcdc001u,
    0xabcdfc00u, 0xabcdfc00u, 0xabcd0000u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM2V2[RowCount] = {
    0xabcdbc00u, 0xabcdc200u, 0xabcdca00u, 0xabcd0000u,
    0xabcdc000u, 0xabcd7c00u, 0xabcdfc00u, 0xabcd7e01u,
    0xabcdfe01u, 0xabcd8001u, 0xabcdc000u, 0xabcdc001u,
    0xabcdfc00u, 0xabcdfc00u, 0xabcd0000u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM3V2[RowCount] = {
    0xabcdc400u, 0xabcdca00u, 0xabcdd200u, 0xabcd0000u,
    0xabcdc800u, 0xabcd7c00u, 0xabcdfc00u, 0xabcd7e01u,
    0xabcdfe01u, 0xabcd0000u, 0xabcdc800u, 0xabcdc801u,
    0xabcdfc00u, 0xabcdfc00u, 0xabcd0000u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM1V3[RowCount] = {
    0xabcd4200u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd8c00u, 0xabcd7c00u, 0xabcdfe00u, 0xabcd7e01u,
    0xabcd7e01u, 0xabcd0001u, 0xabcd0000u, 0xabcd1800u,
    0xabcd7bffu, 0xabcd7c00u, 0xabcd3800u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM2V3[RowCount] = {
    0xabcd4200u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd8c00u, 0xabcd7c00u, 0xabcdfe00u, 0xabcd7e01u,
    0xabcd7e01u, 0xabcd0001u, 0xabcd0000u, 0xabcd1800u,
    0xabcd7bffu, 0xabcd7c00u, 0xabcd3800u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM3V3[RowCount] = {
    0xabcd4600u, 0xabcd4000u, 0xabcd0000u, 0xabcd0000u,
    0xabcd9000u, 0xabcd7c00u, 0xabcdfe00u, 0xabcd7e01u,
    0xabcd7e01u, 0xabcd0000u, 0xabcd0000u, 0xabcd1c00u,
    0xabcd7c00u, 0xabcd7c00u, 0xabcd3c00u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM1V4[RowCount] = {
    0xabcd3c00u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd0001u, 0xabcd0000u, 0xabcd1800u,
    0xabcd3c00u, 0xabcd3c00u, 0xabcd3800u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM2V4[RowCount] = {
    0xabcd3c00u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd0001u, 0xabcd0000u, 0xabcd1800u,
    0xabcd3c00u, 0xabcd3c00u, 0xabcd3800u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM3V4[RowCount] = {
    0xabcd3c00u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd3c00u, 0xabcd0000u, 0xabcd0000u,
    0xabcd0000u, 0xabcd0000u, 0xabcd0000u, 0xabcd1800u,
    0xabcd3c00u, 0xabcd3c00u, 0xabcd3800u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM1V5[RowCount] = {
    0xabcd3c00u, 0xabcd4200u, 0xabcd4a00u, 0xabcd8000u,
    0xabcd4000u, 0xabcdfc00u, 0xabcd7c00u, 0xabcdfe01u,
    0xabcd7e01u, 0xabcd0001u, 0xabcd4000u, 0xabcd4001u,
    0xabcd7c00u, 0xabcd7c00u, 0xabcd0000u, 0xabcdc400u,
};
constexpr std::uint32_t ExpectedM2V5[RowCount] = {
    0xabcd3c00u, 0xabcd4200u, 0xabcd4a00u, 0xabcd8000u,
    0xabcd4000u, 0xabcdfc00u, 0xabcd7c00u, 0xabcdfe01u,
    0xabcd7e01u, 0xabcd0001u, 0xabcd4000u, 0xabcd4001u,
    0xabcd7c00u, 0xabcd7c00u, 0xabcd0000u, 0xabcdc400u,
};
constexpr std::uint32_t ExpectedM3V5[RowCount] = {
    0xabcd3c00u, 0xabcd4200u, 0xabcd4a00u, 0xabcd8000u,
    0xabcd4000u, 0xabcdfc00u, 0xabcd7c00u, 0xabcdfe01u,
    0xabcd7e01u, 0xabcd0000u, 0xabcd4000u, 0xabcd4001u,
    0xabcd7c00u, 0xabcd7c00u, 0xabcd0000u, 0xabcdc400u,
};
constexpr std::uint32_t ExpectedM1V6[RowCount] = {
    0xabcd4200u, 0xabcd4200u, 0xabcd4a00u, 0xabcd0000u,
    0xabcd4000u, 0xabcd7c00u, 0xabcd7c00u, 0xabcd7e01u,
    0xabcd7e01u, 0xabcd0001u, 0xabcd4000u, 0xabcd4001u,
    0xabcd7c00u, 0xabcd7c00u, 0xabcd3800u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM2V6[RowCount] = {
    0xabcd4200u, 0xabcd4200u, 0xabcd4a00u, 0xabcd0000u,
    0xabcd4000u, 0xabcd7c00u, 0xabcd7c00u, 0xabcd7e01u,
    0xabcd7e01u, 0xabcd0001u, 0xabcd4000u, 0xabcd4001u,
    0xabcd7c00u, 0xabcd7c00u, 0xabcd3800u, 0xabcd0000u,
};
constexpr std::uint32_t ExpectedM3V6[RowCount] = {
    0xabcd4200u, 0xabcd4200u, 0xabcd4a00u, 0xabcd0000u,
    0xabcd4000u, 0xabcd7c00u, 0xabcd7c00u, 0xabcd7e01u,
    0xabcd7e01u, 0xabcd0000u, 0xabcd4000u, 0xabcd4001u,
    0xabcd7c00u, 0xabcd7c00u, 0xabcd3800u, 0xabcd0000u,
};

struct Variant {
    const char* name;
    std::uint32_t abs;
    std::uint32_t neg;
    std::uint32_t clamp;
    std::uint32_t omod;
    std::array<const std::uint32_t*, ModeCount> expected;
};

constexpr Variant Variants[] = {
    {"plain", 0u, 0u, 0u, 0u, {ExpectedM1V0, ExpectedM2V0, ExpectedM3V0}},
    {"-a |b| clamp div:2", 2u, 1u, 1u, 3u, {ExpectedM1V1, ExpectedM2V1, ExpectedM3V1}},
    {"-|a| |b| mul:4", 3u, 1u, 0u, 2u, {ExpectedM1V2, ExpectedM2V2, ExpectedM3V2}},
    {"mul:2", 0u, 0u, 0u, 1u, {ExpectedM1V3, ExpectedM2V3, ExpectedM3V3}},
    {"clamp", 0u, 0u, 1u, 0u, {ExpectedM1V4, ExpectedM2V4, ExpectedM3V4}},
    {"-acc", 0u, 4u, 0u, 0u, {ExpectedM1V5, ExpectedM2V5, ExpectedM3V5}},
    {"|acc|", 4u, 0u, 0u, 0u, {ExpectedM1V6, ExpectedM2V6, ExpectedM3V6}},
};

struct Mode {
    const char* name;
    ShaderRecompiler::ShaderFloatMode floatMode;
};

const Mode Modes[ModeCount] = {
    {"M1", ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}},
    {"M2", ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}},
    {"M3", ShaderRecompiler::ShaderFloatMode{0x00u, true, false, false}},
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

bool IsNan16(std::uint32_t bits) {
    return (bits & 0x7fffu) > 0x7c00u;
}

bool HostNan(std::uint32_t actual, std::uint32_t expected) {
    const std::uint32_t half = expected & 0xffffu;
    return (actual >> 16u) == (expected >> 16u) && IsNan16(half) && half != 0xfe00u && IsNan16(actual & 0xffffu);
}

void Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::ShaderFloatMode& floatMode, const Variant& variant) {
    Code[6] = 0xd536000au | variant.abs << 8u | variant.clamp << 15u;
    Code[7] = 0x042a0b04u | variant.omod << 27u | variant.neg << 29u;
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < RowCount; ++tid) {
        Input[tid * Inputs] = Rows[tid].a;
        Input[tid * Inputs + 1u] = Rows[tid].b;
        Input[tid * Inputs + 2u] = AccumulatorHigh | Rows[tid].accumulator;
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = floatMode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::uint32_t Check(const char* mode, const Variant& variant, const std::uint32_t* expected) {
    std::uint32_t hostNans = 0u;
    for (std::uint32_t tid = 0; tid < RowCount; ++tid) {
        const std::uint32_t actual = Output[tid * Results];
        const bool hostNan = actual != expected[tid] && HostNan(actual, expected[tid]);
        if (hostNan) ++hostNans;
        Require(actual == expected[tid] || hostNan, std::string("v_fmac_f16 ") + variant.name + " " + mode + ": lane " + std::to_string(tid) + " of " +
                                                        Hex(Rows[tid].a) + " * " + Hex(Rows[tid].b) + " + " + Hex(Rows[tid].accumulator) + " is " + Hex(actual) +
                                                        ", expected " + Hex(expected[tid]));
    }
    return hostNans;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::uint32_t hostNans = 0u;
        for (const Variant& variant : Variants) {
            for (std::uint32_t mode = 0; mode < ModeCount; ++mode) {
                Run(*device, Modes[mode].floatMode, variant);
                hostNans += Check(Modes[mode].name, variant, variant.expected[mode]);
            }
        }
        std::printf("v_fmac_f16 modifiers tests passed (%u propagated NaN results with the host's own NaN bits)\n", hostNans);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
