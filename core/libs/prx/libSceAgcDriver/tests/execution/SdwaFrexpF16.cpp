#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t TotalResults = 144;
constexpr std::uint32_t Garbage = 0xabcd1234u;

alignas(256) std::array<std::uint32_t, MaxThreads> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * TotalResults> Output{};

constexpr std::array<std::uint16_t, 16> TestHalves{
    0x3c00u,
    0xbc00u,
    0x4000u,
    0xc000u,
    0x3800u,
    0xb800u,
    0x3400u,
    0xb400u,
    0x3e00u,
    0xbe00u,
    0xc200u,
    0x4200u,
    0x0000u,
    0x8000u,
    0x7c00u,
    0xfc00u,
};

void FillInput(std::uint32_t waveSize) {
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        const auto val0 = TestHalves[lane % TestHalves.size()];
        const auto val1 = TestHalves[(lane + 5u) % TestHalves.size()];
        Input[lane] = static_cast<std::uint32_t>(val0) | (static_cast<std::uint32_t>(val1) << 16u);
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t waveSize) {
    std::vector<std::uint32_t> userData(8u, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t waveSize) {
    Output.fill(0xdeadbeefu);
    const auto result = Compile(device, code, waveSize);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint16_t ComputeExpectedMant(std::uint16_t bits) {
    const std::uint32_t mag = bits & 0x7fffu;
    if (mag == 0u || mag >= 0x7c00u) {
        return (mag > 0x7c00u) ? static_cast<std::uint16_t>(bits | 0x0200u) : bits;
    }
    const std::uint16_t sign = bits & 0x8000u;
    const std::uint16_t mantissa = bits & 0x03ffu;
    return static_cast<std::uint16_t>(sign | 0x3800u | mantissa);
}

std::uint16_t ComputeExpectedExp(std::uint16_t bits) {
    const std::uint32_t mag = bits & 0x7fffu;
    if (mag == 0u || mag >= 0x7c00u) {
        return 0u;
    }
    const std::uint32_t exp = (bits >> 10u) & 0x1fu;
    const int unbiased = static_cast<int>(exp) - 14;
    return static_cast<std::uint16_t>(static_cast<std::int16_t>(unbiased));
}

std::uint32_t InsertExpected(std::uint16_t result16, std::uint32_t dstSel, std::uint32_t dstUnused) {
    const auto u32Val = static_cast<std::uint32_t>(result16);
    if (dstSel == 4u) {
        if (dstUnused == 0u) {
            return u32Val;
        }
        if (dstUnused == 1u) {
            return (result16 & 0x8000u) ? (0xffff0000u | u32Val) : u32Val;
        }
        return (Garbage & 0xffff0000u) | u32Val;
    }
    if (dstUnused == 0u || dstUnused == 1u) {
        return u32Val << 16u;
    }
    return (u32Val << 16u) | (Garbage & 0x0000ffffu);
}

void RunCombinations(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    std::vector<std::uint32_t> words{
        0x160200ffu, 1u,
        0x160400ffu, TotalResults,
        0xe0302000u, 0x80000401u,
        0xbf8c3f70u,
    };
    std::uint32_t index = 0u;
    for (const auto opcode : {0x59u, 0x5au}) {
        for (const auto srcSel : {4u, 5u, 6u}) {
            for (const auto neg : {0u, 1u}) {
                for (const auto abs : {0u, 1u}) {
                    for (const auto dstSel : {4u, 5u}) {
                        for (const auto dstUnused : {0u, 1u, 2u}) {
                            const auto dstReg = 5u + (index % 4u);
                            words.push_back(0x7e0002ffu | (dstReg << 17u));
                            words.push_back(Garbage);
                            words.push_back(0x7e0000f9u | (dstReg << 17u) | (opcode << 9u));
                            words.push_back(4u | (dstSel << 8u) | (dstUnused << 11u) | (srcSel << 16u) | (neg << 20u) | (abs << 21u));
                            if (index % 4u == 3u) {
                                words.push_back(0xe0782000u | ((index - 3u) * 4u));
                                words.push_back(0x80010502u);
                            }
                            ++index;
                        }
                    }
                }
            }
        }
    }
    words.push_back(0xbf810000u);
    Run(device, words, waveSize);

    index = 0u;
    for (const auto opcode : {0x59u, 0x5au}) {
        for (const auto srcSel : {4u, 5u, 6u}) {
            for (const auto neg : {0u, 1u}) {
                for (const auto abs : {0u, 1u}) {
                    for (const auto dstSel : {4u, 5u}) {
                        for (const auto dstUnused : {0u, 1u, 2u}) {
                            for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
                                const auto val0 = TestHalves[lane % TestHalves.size()];
                                const auto val1 = TestHalves[(lane + 5u) % TestHalves.size()];
                                const auto rawHalf = (srcSel == 5u) ? val1 : val0;
                                auto modHalf = rawHalf;
                                if (abs != 0u) modHalf = static_cast<std::uint16_t>(modHalf & 0x7fffu);
                                if (neg != 0u) modHalf = static_cast<std::uint16_t>(modHalf ^ 0x8000u);
                                const auto res16 = (opcode == 0x59u) ? ComputeExpectedMant(modHalf) : ComputeExpectedExp(modHalf);
                                const auto expected = InsertExpected(res16, dstSel, dstUnused);
                                const auto actual = Output[lane * TotalResults + index];
                                Require(actual == expected, "sdwa frexp lane " + std::to_string(lane) + " wave" + std::to_string(waveSize) +
                                    " op " + Hex(opcode) + " srcSel " + std::to_string(srcSel) + " neg " + std::to_string(neg) +
                                    " abs " + std::to_string(abs) + " dstSel " + std::to_string(dstSel) + " unused " + std::to_string(dstUnused) +
                                    ": got " + Hex(actual) + ", expected " + Hex(expected));
                            }
                            ++index;
                        }
                    }
                }
            }
        }
    }
}

void CheckRefused(AgcDriver::VulkanDevice& device) {
    struct RefusedCase {
        std::uint32_t word0;
        std::uint32_t word1;
        const char* expectedError;
    };
    const std::array<RefusedCase, 6> cases{{
        {0x7e14b2f9u, 0x00000404u, "is not supported"},
        {0x7e14b4f9u, 0x00000404u, "is not supported"},
        {0x7e14b2f9u, 0x00040004u, "is not supported"},
        {0x7e14b4f9u, 0x00040004u, "is not supported"},
        {0x7e14b2f9u, 0x00041804u, "is not supported"},
        {0x7e14b4f9u, 0x00044404u, "VOP1 SDWA output modifiers are not supported"},
    }};
    for (const auto& c : cases) {
        alignas(256) const std::array<std::uint32_t, 3> code{c.word0, c.word1, 0xbf810000u};
        std::string refusal;
        try {
            static_cast<void>(Compile(device, code, 32u));
        } catch (const std::exception& error) {
            refusal = error.what();
        }
        Require(refusal.find(c.expectedError) != std::string::npos, "expected refusal for instruction");
    }
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(argc == 1 || (argc == 2 && std::string(argv[1]) == "wave64"), "expected no argument or wave64");
        const auto waveSize = argc == 2 ? 64u : 32u;
        FillInput(waveSize);
        RunCombinations(*device, waveSize);
        CheckRefused(*device);
        std::printf("sdwa frexp f16 wave%u tests passed\n", waveSize);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
