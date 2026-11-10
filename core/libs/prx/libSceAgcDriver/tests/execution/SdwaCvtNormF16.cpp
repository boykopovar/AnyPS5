#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 1;
constexpr std::uint32_t Results = 72;
constexpr std::uint32_t Garbage = 0xabcd1234u;

alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

constexpr std::array<std::uint16_t, 16> TestF16s{
    0x3800u,
    0xb800u,
    0x3c00u,
    0xbc00u,
    0x3400u,
    0xb400u,
    0x3a00u,
    0xba00u,
    0x4000u,
    0xc000u,
    0x0000u,
    0x8000u,
    0x7c00u,
    0xfc00u,
    0x7e00u,
    0x0200u,
};

struct TestCase {
    std::uint32_t sourceSelector;
    bool sourceNeg;
    bool sourceAbs;
    std::uint32_t destinationSelector;
    std::uint32_t unused;
};

std::vector<TestCase> GetTestCases() {
    std::vector<TestCase> cases;
    for (const std::uint32_t srcSel : {4u, 5u, 6u}) {
        for (const bool srcAbs : {false, true}) {
            for (const bool srcNeg : {false, true}) {
                for (const std::uint32_t dstSel : {4u, 5u}) {
                    for (const std::uint32_t unused : {0u, 1u, 2u}) {
                        cases.push_back({srcSel, srcNeg, srcAbs, dstSel, unused});
                    }
                }
            }
        }
    }
    return cases;
}

std::uint32_t NormF16Model(std::uint32_t bits, bool signedValue) {
    const std::uint32_t magnitude = bits & 0x7fffu;
    const bool negative = (bits & 0x8000u) != 0u;
    const bool nan = magnitude > 0x7c00u;
    if (nan) {
        return 0u;
    }
    const std::uint32_t scale = signedValue ? 32767u : 65535u;
    if (magnitude >= 0x3c00u) {
        if (signedValue) {
            return (negative ? (0u - scale) : scale) & 0xffffu;
        }
        return negative ? 0u : scale;
    }
    const std::uint32_t exponent = magnitude >> 10u;
    const bool normal = exponent != 0u;
    const std::uint32_t mantissa = (magnitude & 0x3ffu) | (normal ? 0x400u : 0u);
    const std::uint32_t shift = 25u - (normal ? exponent : 1u);
    const std::uint32_t product = mantissa * scale;
    const std::uint32_t truncated = product >> shift;
    const std::uint32_t remainder = product & ((1u << shift) - 1u);
    const std::uint32_t half = 1u << (shift - 1u);
    const bool roundUp = (remainder > half) || (remainder == half && (truncated & 1u) != 0u);
    std::uint32_t value = truncated + (roundUp ? 1u : 0u);
    if (signedValue) {
        value = negative ? (0u - value) : value;
    } else {
        value = negative ? 0u : value;
    }
    return value & 0xffffu;
}

std::uint32_t FormatDestination(std::uint32_t field, std::uint32_t destinationSelector, std::uint32_t unused) {
    const std::uint32_t offset = (destinationSelector == 5u) ? 16u : 0u;
    const std::uint32_t masked = field & 0xffffu;
    switch (unused) {
        case 0u:
            return masked << offset;
        case 1u: {
            const std::uint32_t extended = (masked & 0x8000u) ? (masked | 0xffff0000u) : masked;
            return extended << offset;
        }
        case 2u: {
            const std::uint32_t mask = 0xffffu << offset;
            return (Garbage & ~mask) | (masked << offset);
        }
        default:
            return 0u;
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

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto low = static_cast<std::uint32_t>(TestF16s[tid % TestF16s.size()]);
        const auto high = static_cast<std::uint32_t>(TestF16s[(tid + 5u) % TestF16s.size()]);
        Input[tid] = (high << 16u) | low;
    }
}

void RunNorm(AgcDriver::VulkanDevice& device, bool signedValue, std::uint32_t waveSize) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);

    const auto testCases = GetTestCases();
    Require(testCases.size() == Results, "mismatch in test cases count");

    std::vector<std::uint32_t> words{
        0x160200ffu, Inputs,
        0x160400ffu, Results,
        0xe0302000u, 0x80000401u,
        0xbf8c3f70u,
    };
    const std::uint32_t opcode = signedValue ? 0x63u : 0x64u;
    for (std::size_t i = 0; i < testCases.size(); ++i) {
        const auto& tc = testCases[i];
        const std::uint32_t modifier = 4u | (tc.destinationSelector << 8u) | (tc.unused << 11u) |
            (tc.sourceSelector << 16u) | (tc.sourceNeg ? (1u << 20u) : 0u) | (tc.sourceAbs ? (1u << 21u) : 0u);
        words.insert(words.end(), {
            0x7e0a02ffu, Garbage,
            0x7e0a00f9u | (opcode << 9u), modifier,
            0xe0702000u | static_cast<std::uint32_t>(i * 4u), 0x80010502u,
        });
    }
    words.push_back(0xbf810000u);

    const std::span<const std::uint32_t> code(words);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(bool signedValue, std::uint32_t waveSize) {
    const auto testCases = GetTestCases();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto inWord = Input[tid * Inputs];
        for (std::size_t i = 0; i < testCases.size(); ++i) {
            const auto& tc = testCases[i];
            std::uint32_t raw = 0u;
            if (tc.sourceSelector == 4u) {
                raw = inWord & 0xffffu;
            } else if (tc.sourceSelector == 5u) {
                raw = (inWord >> 16u) & 0xffffu;
            } else {
                raw = inWord & 0xffffu;
            }
            if (tc.sourceAbs) raw &= 0x7fffu;
            if (tc.sourceNeg) raw ^= 0x8000u;
            const auto norm = NormF16Model(raw, signedValue);
            const auto expected = FormatDestination(norm, tc.destinationSelector, tc.unused);
            const auto actual = Output[tid * Results + i];
            Require(actual == expected, "sdwa " + std::string(signedValue ? "v_cvt_norm_i16_f16" : "v_cvt_norm_u16_f16") +
                " tid " + std::to_string(tid) + " wave" + std::to_string(waveSize) + " case " + std::to_string(i) +
                " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

void CheckModelAnchors() {
    Require(NormF16Model(0x3c00u, true) == 32767u && NormF16Model(0xbc00u, true) == 0x8001u, "norm i16 1.0/-1.0 model anchors");
    Require(NormF16Model(0x3c00u, false) == 65535u && NormF16Model(0xbc00u, false) == 0u, "norm u16 1.0/-1.0 model anchors");
    Require(NormF16Model(0x3800u, true) == 0x4000u && NormF16Model(0x3800u, false) == 0x8000u, "norm f16 0.5 model anchors");
    Require(NormF16Model(0x0000u, true) == 0u && NormF16Model(0x8000u, true) == 0u, "norm f16 0.0/-0.0 model anchors");
    Require(NormF16Model(0x7c00u, true) == 32767u && NormF16Model(0x7c00u, false) == 65535u, "norm f16 +inf model anchors");
    Require(NormF16Model(0xfc00u, true) == 0x8001u && NormF16Model(0xfc00u, false) == 0u, "norm f16 -inf model anchors");
    Require(NormF16Model(0x7e00u, true) == 0u && NormF16Model(0x7e00u, false) == 0u, "norm f16 nan model anchors");
    Require(NormF16Model(0x0200u, true) == 1u && NormF16Model(0x0200u, false) == 2u, "norm f16 subnormal model anchors");
}

void CheckRefused(AgcDriver::VulkanDevice& device) {
    struct RefusedCase {
        std::uint32_t opcode;
        std::uint32_t modifier;
        const char* expectedError;
        const char* description;
    };
    const std::array<RefusedCase, 2> cases{{
        {0x63u, 4u | (4u << 8u) | (3u << 11u) | (4u << 16u), "VOP1 SDWA destination selector 4 (unused 3", "unused 3 on i16"},
        {0x64u, 4u | (4u << 8u) | (3u << 11u) | (4u << 16u), "VOP1 SDWA destination selector 4 (unused 3", "unused 3 on u16"},
    }};
    for (const auto& c : cases) {
        const std::array<std::uint32_t, 3> code{
            0x7e0a00f9u | (c.opcode << 9u), c.modifier,
            0xbf810000u
        };
        const std::span<const std::uint32_t> codeSpan(code);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(codeSpan.data()), std::as_bytes(codeSpan)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{1, 1, 1}, 0, {false, false, false}, false, 1};
        ShaderRecompiler::RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(codeSpan.data()), codeSpan, 0, {}},
            {32, 0, {}, compute, std::nullopt, std::nullopt, memory},
            device.Target(),
            {0, 0, 0, 128}
        };
        request.useCache = false;
        std::string refusal;
        try {
            static_cast<void>(ShaderRecompiler::Recompile(request));
        } catch (const std::exception& error) {
            refusal = error.what();
        }
        Require(refusal.find(c.expectedError) != std::string::npos, std::string(c.description) + " was not refused with " + c.expectedError);
    }
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(argc == 1 || (argc == 2 && std::string(argv[1]) == "wave64"), "expected no argument or wave64");
        const auto waveSize = argc == 2 ? 64u : 32u;
        CheckModelAnchors();
        CheckRefused(*device);
        FillInput();
        for (const bool signedValue : {true, false}) {
            RunNorm(*device, signedValue, waveSize);
            Check(signedValue, waveSize);
        }
        std::puts("sdwa cvt norm f16 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
