#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

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

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(MixCode);
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


const Testing::Case mixedFma{"MixPrecision_MixedPrecisionFma_MatchesReferenceResults", [] {
    auto& device = SharedVulkanTestDevice();
    FillInput();
    Run(device);
    Check();
}};

} // namespace
