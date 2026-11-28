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

constexpr std::uint32_t Threads = 64u;
constexpr std::uint32_t Inputs = 14u;
constexpr std::uint32_t Results = Inputs * 19u;
constexpr std::uint32_t Garbage = 0xabcd1234u;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

struct TestCase {
    std::uint32_t opcode;
    std::uint32_t input;
    std::uint32_t result;
    std::uint32_t byteInput;
    std::uint32_t byteResult;
};

constexpr std::array<TestCase, 12> Cases{{
    {0x54u, 0x1800u, 0x6000u, 0x40u, 0x7c00u},
    {0x55u, 0x3d10u, 0x3c80u, 0x40u, 0x1800u},
    {0x56u, 0x4400u, 0x3800u, 0x40u, 0x6000u},
    {0x57u, 0x0040u, 0xcc80u, 0x40u, 0xcc80u},
    {0x58u, 0x0000u, 0x3c00u, 0x00u, 0x3c00u},
    {0x5bu, 0xd3d0u, 0xd3e0u, 0x80u, 0x0000u},
    {0x5cu, 0x53d0u, 0x53e0u, 0x80u, 0x3c00u},
    {0x5du, 0xd3d0u, 0xd3c0u, 0x80u, 0x0000u},
    {0x5eu, 0x53d0u, 0x53c0u, 0x80u, 0x0000u},
    {0x5fu, 0x3f00u, 0x3a00u, 0x80u, 0x0080u},
    {0x60u, 0x0000u, 0x0000u, 0x00u, 0x0000u},
    {0x61u, 0x0000u, 0x3c00u, 0x00u, 0x3c00u},
}};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

std::uint32_t Offset(std::uint32_t selector) {
    return selector < 4u ? selector * 8u : selector == 5u ? 16u : 0u;
}

std::uint32_t Place(std::uint32_t bits, std::uint32_t destination, std::uint32_t unused) {
    if (destination == 6u) return bits;
    const auto width = destination < 4u ? 8u : 16u;
    const auto mask = (1u << width) - 1u;
    auto field = bits & mask;
    if (unused == 1u && (field & (1u << (width - 1u)))) field |= ~mask;
    return (field << Offset(destination)) | (unused == 2u ? Garbage & ~(mask << Offset(destination)) : 0u);
}

void Run(AgcDriver::VulkanDevice& device, const TestCase& test, std::uint32_t waveSize, bool nanSources) {
    for (std::uint32_t lane = 0u; lane < Threads; ++lane) {
        for (std::uint32_t source = 0u; source < 7u; ++source) {
            for (std::uint32_t sign = 0u; sign < 2u; ++sign) {
                const auto field = nanSources && source < 4u && sign != 0u ? 0x80u : source < 4u ? test.byteInput : test.input;
                const auto mask = source < 4u ? 0xffu : 0xffffu;
                const auto noise = 0xa5c39678u ^ (lane * 0x01010101u);
                Input[lane * Inputs + source * 2u + sign] = (noise & ~(mask << Offset(source))) | (field << Offset(source));
            }
        }
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> words{0x160200ffu, Inputs, 0x160400ffu, Results};
    std::uint32_t index = 0u;
    for (std::uint32_t source = 0u; source < 7u; ++source) {
        for (std::uint32_t sign = 0u; sign < 2u; ++sign) {
            words.insert(words.end(), {0xe0302000u | ((source * 2u + sign) * 4u), 0x80000401u, 0xbf8c3f70u});
            for (std::uint32_t destination = 0u; destination < 7u; ++destination) {
                for (std::uint32_t unused = 0u; unused < (destination == 6u ? 1u : 3u); ++unused, ++index) {
                    words.insert(words.end(), {
                        0x7e0a02ffu, Garbage, 0x7e0a00f9u | (test.opcode << 9u),
                        4u | (destination << 8u) | (unused << 11u) | (source << 16u) | (sign << 19u),
                        0xe0702000u | (index * 4u), 0x80010502u,
                    });
                }
            }
        }
    }
    words.push_back(0xbf810000u);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
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

void Check(const TestCase& test, bool nanSources) {
    std::uint32_t index = 0u;
    for (std::uint32_t source = 0u; source < 7u; ++source) {
        for (std::uint32_t sign = 0u; sign < 2u; ++sign) {
            for (std::uint32_t destination = 0u; destination < 7u; ++destination) {
                for (std::uint32_t unused = 0u; unused < (destination == 6u ? 1u : 3u); ++unused, ++index) {
                    const auto expected = Place(source < 4u ? test.byteResult : test.result, destination, unused);
                    for (std::uint32_t lane = 0u; lane < Threads; ++lane) {
                        const auto actual = Output[lane * Results + index];
                        if ((nanSources || (test.byteInput & 0x80u) != 0u) && source < 4u && sign != 0u) {
                            if (destination >= 4u) {
                                const auto half = (actual >> Offset(destination)) & 0xffffu;
                                Require((half & 0x7c00u) == 0x7c00u && (half & 0x03ffu) != 0u,
                                    "sign-extended byte must be consumed as an f16 NaN for opcode " + Hex(test.opcode));
                            }
                            continue;
                        }
                        Require(actual == expected, "sdwa unary f16 opcode " + Hex(test.opcode) + " source " +
                            std::to_string(source) + " sext " + std::to_string(sign) + " destination " +
                            std::to_string(destination) + " unused " + std::to_string(unused) + " lane " +
                            std::to_string(lane) + " is " + Hex(actual) + ", expected " + Hex(expected));
                    }
                }
            }
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(argc == 1 || argc == 4, "expected no arguments or case index, wave size and source mode");
        const auto testIndex = argc == 1 ? 0u : static_cast<std::uint32_t>(std::stoul(argv[1]));
        const auto waveSize = argc == 1 ? 32u : static_cast<std::uint32_t>(std::stoul(argv[2]));
        const auto sourceMode = argc == 1 ? 0u : static_cast<std::uint32_t>(std::stoul(argv[3]));
        Require(testIndex < Cases.size() && (waveSize == 32u || waveSize == 64u) && sourceMode < 2u,
            "invalid case index, wave size or source mode");
        const auto& test = Cases[testIndex];
        Run(*device, test, waveSize, sourceMode != 0u);
        Check(test, sourceMode != 0u);
        std::printf("sdwa unary f16 bytes opcode 0x%x wave%u: %u combinations across %u lanes passed\n",
            test.opcode, waveSize, Results, Threads);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
