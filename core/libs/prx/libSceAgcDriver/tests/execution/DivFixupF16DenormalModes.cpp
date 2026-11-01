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

constexpr std::uint32_t ValueCount = 10;
constexpr std::uint32_t Threads = 1024;
constexpr std::uint32_t Inputs = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads> Output{};

constexpr std::uint32_t Values[ValueCount] = {0x0000u, 0x8000u, 0x0001u, 0x83ffu, 0x0200u, 0x0400u, 0x3c00u, 0xfc00u, 0x7e00u, 0x7d00u};

alignas(256) constexpr std::array<std::uint32_t, 11> Code{
    0x34020084u, 0x34040082u, 0xe03c1000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0xd75f000au, 0x041a0b04u,
    0xe0701000u, 0x80010a02u, 0xbf810000u,
};

struct Mode {
    const char* name;
    std::optional<ShaderRecompiler::ShaderFloatMode> value;
    bool flushed;
    bool ieee;
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

void Fill() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t row = tid < ValueCount * ValueCount * ValueCount ? tid : 0u;
        Input[tid * Inputs] = Values[row / (ValueCount * ValueCount)];
        Input[tid * Inputs + 1u] = Values[row / ValueCount % ValueCount];
        Input[tid * Inputs + 2u] = Values[row % ValueCount];
        Input[tid * Inputs + 3u] = 0u;
    }
}

void Run(AgcDriver::VulkanDevice& device, const Mode& mode) {
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
    request.context.floatMode = mode.value;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::uint32_t Fixup(std::uint32_t quotient, std::uint32_t denominator, std::uint32_t numerator, bool flushed, bool ieee) {
    const auto magnitude = [](std::uint32_t half) { return half & 0x7fffu; };
    const auto flush = [&](std::uint32_t half) { return flushed && magnitude(half) < 0x0400u ? half & 0x8000u : half; };
    denominator = flush(denominator);
    numerator = flush(numerator);
    const auto nan = [&](std::uint32_t half) { return magnitude(half) > 0x7c00u; };
    const auto infinite = [&](std::uint32_t half) { return magnitude(half) == 0x7c00u; };
    const auto zero = [&](std::uint32_t half) { return magnitude(half) == 0u; };
    const std::uint32_t sign = (denominator ^ numerator) & 0x8000u;
    if (nan(numerator)) return ieee ? numerator | 0x0200u : numerator;
    if (nan(denominator)) return ieee ? denominator | 0x0200u : denominator;
    if ((zero(denominator) && zero(numerator)) || (infinite(denominator) && infinite(numerator))) return 0xfe00u;
    if (zero(denominator) || infinite(numerator)) return sign | 0x7c00u;
    if (infinite(denominator) || zero(numerator)) return sign;
    if (nan(quotient)) return sign | 0x7c00u;
    return sign | magnitude(quotient);
}

void Check(const Mode& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t expected = Fixup(in[0], in[1], in[2], mode.flushed, mode.ieee);
        Require(Output[tid] == expected, std::string("v_div_fixup_f16 ") + mode.name + " of " + Hex(in[0]) + ", " + Hex(in[1]) + ", " + Hex(in[2]) + " is " + Hex(Output[tid]) + ", expected " + Hex(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Fill();
        const Mode modes[] = {
            {"without a known float mode", std::nullopt, false, false},
            {"f16 denormal mode 0, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x30u, true, true, false}, true, true},
            {"f16 denormal mode 0, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x30u, true, false, false}, true, false},
            {"f16 denormal mode 1, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x70u, true, true, false}, false, true},
            {"f16 denormal mode 1, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x70u, true, false, false}, false, false},
            {"f16 denormal mode 2, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xb0u, true, true, false}, true, true},
            {"f16 denormal mode 2, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xb0u, true, false, false}, true, false},
            {"f16 denormal mode 3, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}, false, true},
            {"f16 denormal mode 3, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false}, false, false},
        };
        for (const Mode& mode : modes) {
            Run(*device, mode);
            Check(mode);
        }
        std::puts("v_div_fixup_f16 denormal mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
