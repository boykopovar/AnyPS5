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
constexpr std::uint32_t Inputs = 8;
constexpr std::uint32_t Results = 2;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 12> Code{
    0x34020085u, 0x34040083u, 0xe0381000u, 0x80000401u, 0xe0341010u, 0x80000801u, 0xbf8c3f70u, 0xd560000au,
    0x04220d04u, 0xe0741000u, 0x80010a02u, 0xbf810000u,
};

constexpr std::uint64_t Values[ValueCount] = {
    0x0000000000000000ull, 0x8000000000000000ull, 0x0000000000000001ull, 0x800fffffffffffffull, 0x0010000000000000ull,
    0x3ff0000000000000ull, 0xbff0000000000000ull, 0x7fefffffffffffffull, 0x7ff0000000000000ull, 0x7ff8000000000000ull,
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

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%016llx", static_cast<unsigned long long>(value));
    return text;
}

std::array<std::uint64_t, 3> Sources(std::uint32_t tid) {
    const std::uint32_t row = tid < ValueCount * ValueCount * ValueCount ? tid : 0u;
    return {Values[row / (ValueCount * ValueCount)], Values[row / ValueCount % ValueCount], Values[row % ValueCount]};
}

void Run(AgcDriver::VulkanDevice& device, const Mode& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sources = Sources(tid);
        for (std::uint32_t index = 0; index < sources.size(); ++index) {
            Input[tid * Inputs + index * 2u] = static_cast<std::uint32_t>(sources[index]);
            Input[tid * Inputs + index * 2u + 1u] = static_cast<std::uint32_t>(sources[index] >> 32u);
        }
        Input[tid * Inputs + 6u] = 0u;
        Input[tid * Inputs + 7u] = 0u;
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
    request.context.floatMode = mode.value;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::uint64_t Fixup(std::uint64_t quotient, std::uint64_t denominator, std::uint64_t numerator, bool flushed, bool ieee) {
    constexpr std::uint64_t Magnitude = 0x7fffffffffffffffull;
    constexpr std::uint64_t Infinity = 0x7ff0000000000000ull;
    const auto flush = [&](std::uint64_t value) { return flushed && (value & Infinity) == 0u ? value & ~Magnitude : value; };
    denominator = flush(denominator);
    numerator = flush(numerator);
    const auto nan = [&](std::uint64_t value) { return (value & Magnitude) > Infinity; };
    const auto infinite = [&](std::uint64_t value) { return (value & Magnitude) == Infinity; };
    const auto zero = [&](std::uint64_t value) { return (value & Magnitude) == 0u; };
    const auto exponent = [](std::uint64_t value) { return static_cast<std::int32_t>((value >> 52u) & 0x7ffu); };
    const std::uint64_t sign = (denominator ^ numerator) & ~Magnitude;
    if (nan(numerator)) return ieee ? numerator | 0x0008000000000000ull : numerator;
    if (nan(denominator)) return ieee ? denominator | 0x0008000000000000ull : denominator;
    if ((zero(denominator) && zero(numerator)) || (infinite(denominator) && infinite(numerator))) return 0xfff8000000000000ull;
    if (zero(denominator) || infinite(numerator)) return sign | Infinity;
    if (infinite(denominator) || zero(numerator)) return sign;
    if (exponent(numerator) - exponent(denominator) < -1075) return sign;
    if (nan(quotient)) return sign | Infinity;
    return sign | (quotient & Magnitude);
}

void Check(const Mode& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto sources = Sources(tid);
        const std::uint64_t expected = Fixup(sources[0], sources[1], sources[2], mode.flushed, mode.ieee);
        const std::uint64_t actual = Output[tid * Results] | (static_cast<std::uint64_t>(Output[tid * Results + 1u]) << 32u);
        Require(actual == expected, std::string("v_div_fixup_f64 ") + mode.name + " of " + Hex(sources[0]) + ", " + Hex(sources[1]) + ", " + Hex(sources[2]) + " is " + Hex(actual) + ", expected " + Hex(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const Mode modes[] = {
            {"without a known float mode", std::nullopt, false, false},
            {"f64 denormal mode 0, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x30u, true, true, false}, true, true},
            {"f64 denormal mode 0, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x30u, true, false, false}, true, false},
            {"f64 denormal mode 1, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0x70u, true, true, false}, false, true},
            {"f64 denormal mode 1, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0x70u, true, false, false}, false, false},
            {"f64 denormal mode 2, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xb0u, true, true, false}, true, true},
            {"f64 denormal mode 2, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xb0u, true, false, false}, true, false},
            {"f64 denormal mode 3, IEEE mode on", ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}, false, true},
            {"f64 denormal mode 3, IEEE mode off", ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false}, false, false},
        };
        for (const Mode& mode : modes) {
            Run(*device, mode);
            Check(mode);
        }
        std::puts("v_div_fixup_f64 denormal mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
