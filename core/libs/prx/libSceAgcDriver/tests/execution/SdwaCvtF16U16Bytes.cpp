#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 8;
constexpr std::uint32_t ResultsPerCase = 19;
constexpr std::uint32_t Results = Inputs * ResultsPerCase;
constexpr std::uint32_t Garbage = 0xabcd1234u;

alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

struct TestCase {
    std::uint32_t opcode;
    std::uint32_t input;
    std::uint32_t expectedFull;
    bool checkPinned;
    const char* name;
};

constexpr TestCase TestCases[Inputs] = {
    {0x50u, 0x00000001u, 0x00003c00u, true, "v_cvt_f16_u16 1"},
    {0x50u, 0x00000002u, 0x00004000u, true, "v_cvt_f16_u16 2"},
    {0x50u, 0x00000004u, 0x00004400u, true, "v_cvt_f16_u16 4"},
    {0x50u, 0x00000008u, 0x00004800u, true, "v_cvt_f16_u16 8"},
    {0x50u, 0x00000010u, 0x00004c00u, true, "v_cvt_f16_u16 16"},
    {0x50u, 0x00000100u, 0x00005c00u, true, "v_cvt_f16_u16 256"},
    {0x50u, 0x000000c8u, 0x00005a40u, true, "v_cvt_f16_u16 200"},
    {0x50u, 0x000003e8u, 0x000063d0u, true, "v_cvt_f16_u16 1000"},
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t FieldWidth(std::uint32_t selector) {
    return selector < 4u ? 8u : selector < 6u ? 16u : 32u;
}

std::uint32_t FieldOffset(std::uint32_t selector) {
    return selector < 4u ? selector * 8u : selector == 5u ? 16u : 0u;
}

std::uint32_t InsertResult(std::uint32_t bits, std::uint32_t destination, std::uint32_t unused) {
    const auto width = FieldWidth(destination);
    const auto offset = FieldOffset(destination);
    const auto mask = (1u << width) - 1u;
    auto field = bits & mask;
    if (unused == 1u && (field & (1u << (width - 1u)))) field |= ~mask;
    return (field << offset) | (unused == 2u ? Garbage & ~(mask << offset) : 0u);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void TestInvalidSdwaSelectors() {
    constexpr std::uint32_t CvtF16U16Sdwa = 0x7e0000f9u | (5u << 17u) | (0x50u << 9u);

    for (const auto opWord : {CvtF16U16Sdwa}) {
        {
            const std::array<std::uint32_t, 2> words{opWord, 4u | (0u << 8u) | (2u << 11u) | (4u << 16u)};
            const auto instruction = ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
            Require(instruction.family == ShaderRecompiler::RdnaInstructionFamily::VOP1, "instruction must be VOP1");
            Require(instruction.destination.sdwaSel == 0u, "destination sdwaSel must be 0");
            Require(instruction.destination.sdwaDstUnused == 2u, "destination sdwaDstUnused must be 2");
            Require(instruction.destination.explicitSdwaDst, "destination explicitSdwaDst must be true");
            Require(instruction.source0.sdwaSel == 4u, "source0 sdwaSel must be 4");
            Require(!instruction.source0.negate, "source0 negate must be false");
            Require(!instruction.source0.absolute, "source0 absolute must be false");
            Require(!instruction.destination.clamp, "destination clamp must be false");
            Require(instruction.destination.omod == 0u, "destination omod must be 0");
        }
        {
            const std::array<std::uint32_t, 2> words{opWord, 4u | (4u << 8u) | (1u << 11u) | (1u << 13u) | (1u << 14u) | (4u << 16u)};
            const auto instruction = ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
            Require(instruction.destination.sdwaSel == 4u, "destination sdwaSel must be 4");
            Require(instruction.destination.sdwaDstUnused == 1u, "destination sdwaDstUnused must be 1");
            Require(instruction.destination.clamp, "destination clamp must be true");
            Require(instruction.destination.omod == 1u, "destination omod must be 1");
            Require(!instruction.source0.negate, "source0 negate must be false");
            Require(!instruction.source0.absolute, "source0 absolute must be false");
        }
        {
            const std::array<std::uint32_t, 2> words{opWord, 4u | (4u << 8u) | (4u << 16u) | (1u << 20u)};
            bool threw = false;
            try {
                ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            Require(threw, "SDWA negate on u16 integer source must throw std::invalid_argument");
        }
        {
            const std::array<std::uint32_t, 2> words{opWord, 4u | (7u << 8u) | (4u << 16u)};
            bool threw = false;
            try {
                ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            Require(threw, "SDWA destination selector 7 must throw std::invalid_argument");
        }
        {
            const std::array<std::uint32_t, 2> words{opWord, 4u | (0u << 8u) | (3u << 11u) | (4u << 16u)};
            bool threw = false;
            try {
                ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            Require(threw, "SDWA destination unused 3 must throw std::invalid_argument");
        }
        {
            const std::array<std::uint32_t, 2> words{opWord, 4u | (6u << 8u) | (3u << 11u) | (4u << 16u)};
            bool threw = false;
            try {
                ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            Require(threw, "SDWA destination unused 3 on dword destination must throw std::invalid_argument");
        }
    }
}

void RunExecutionTests(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    for (std::uint32_t lane = 0u; lane < Threads; ++lane) {
        for (std::uint32_t c = 0u; c < Inputs; ++c) {
            Input[lane * Inputs + c] = TestCases[c].input;
        }
    }
    Output.fill(0xdeadbeefu);

    std::vector<std::uint32_t> words{
        0x160200ffu, Inputs, 0x160400ffu, Results,
    };

    std::uint32_t resultIndex = 0u;
    for (std::uint32_t c = 0u; c < Inputs; ++c) {
        words.insert(words.end(), {0xe0302000u | (c * 4u), 0x80000401u, 0xbf8c3f70u});
        words.insert(words.end(), {0x7e0002ffu | (5u << 17u), Garbage});
        words.push_back(0x7e000000u | (5u << 17u) | (TestCases[c].opcode << 9u) | 260u);
        words.insert(words.end(), {0xe0702000u | (resultIndex * 4u), 0x80010502u});
        ++resultIndex;

        for (std::uint32_t destination = 0u; destination < 6u; ++destination) {
            for (std::uint32_t unused = 0u; unused < 3u; ++unused, ++resultIndex) {
                words.insert(words.end(), {0x7e0002ffu | (5u << 17u), Garbage});
                const auto word0 = 0x7e0000f9u | (5u << 17u) | (TestCases[c].opcode << 9u);
                const auto word1 = 4u | (destination << 8u) | (unused << 11u) | (4u << 16u);
                words.insert(words.end(), {word0, word1});
                words.insert(words.end(), {0xe0702000u | (resultIndex * 4u), 0x80010502u});
            }
        }
    }
    words.push_back(0xbf810000u);

    std::vector<std::uint32_t> userData(8u);
    const auto inputDesc = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto outputDesc = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(inputDesc.begin(), inputDesc.end(), userData.begin());
    std::copy(outputDesc.begin(), outputDesc.end(), userData.begin() + 4);

    const std::span<const std::uint32_t> code(words);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();

    std::uint32_t checkIndex = 0u;
    for (std::uint32_t c = 0u; c < Inputs; ++c) {
        for (std::uint32_t lane = 0u; lane < Threads; ++lane) {
            const auto actualFull = Output[lane * Results + checkIndex];
            const auto expectedNonSdwa = (Garbage & 0xffff0000u) | (TestCases[c].expectedFull & 0xffffu);
            Require(actualFull == expectedNonSdwa,
                std::string(TestCases[c].name) + " full result lane " + std::to_string(lane) +
                " is " + Hex(actualFull) + ", expected " + Hex(expectedNonSdwa));
        }
        ++checkIndex;
        for (std::uint32_t destination = 0u; destination < 6u; ++destination) {
            for (std::uint32_t unused = 0u; unused < 3u; ++unused, ++checkIndex) {
                for (std::uint32_t lane = 0u; lane < Threads; ++lane) {
                    const auto expected = InsertResult(TestCases[c].expectedFull, destination, unused);
                    const auto actual = Output[lane * Results + checkIndex];
                    Require(actual == expected,
                        std::string(TestCases[c].name) + " lane " + std::to_string(lane) +
                        " wave" + std::to_string(waveSize) + " destination " +
                        std::to_string(destination) + " unused " + std::to_string(unused) +
                        " is " + Hex(actual) + ", expected " + Hex(expected));
                }
            }
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        TestInvalidSdwaSelectors();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(argc == 1 || (argc == 2 && std::string(argv[1]) == "wave64"), "expected no argument or wave64");
        const auto waveSize = argc == 2 ? 64u : 32u;
        RunExecutionTests(*device, waveSize);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
