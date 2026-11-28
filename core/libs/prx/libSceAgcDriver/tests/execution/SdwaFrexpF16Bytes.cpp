#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t TotalResults = 300;
constexpr std::uint32_t Garbage = 0xabcd1234u;
constexpr std::uint32_t FrexpMantOpcode = 0x59u;
constexpr std::uint32_t FrexpExpOpcode = 0x5au;

alignas(256) std::array<std::uint32_t, MaxThreads> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * TotalResults> Output{};

constexpr std::array<std::uint16_t, 16> TestHalves{
    0x3c81u,
    0xbc81u,
    0x3c55u,
    0xbc55u,
    0x42ffu,
    0xc201u,
    0x3800u,
    0xb800u,
    0x3400u,
    0xb400u,
    0x0480u,
    0x7bffu,
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

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void ExpectDecodeThrows(const std::array<std::uint32_t, 2>& words, const char* message) {
    bool threw = false;
    try {
        static_cast<void>(ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, message);
}

void TestDecoderSdwaSelectors() {
    constexpr std::uint32_t MantSdwa = 0x7e0000f9u | (5u << 17u) | (FrexpMantOpcode << 9u);
    constexpr std::uint32_t ExpSdwa = 0x7e0000f9u | (5u << 17u) | (FrexpExpOpcode << 9u);

    for (const auto opWord : {MantSdwa, ExpSdwa}) {
        for (std::uint32_t dstSel = 0u; dstSel < 6u; ++dstSel) {
            for (std::uint32_t unused = 0u; unused < 3u; ++unused) {
                const std::array<std::uint32_t, 2> words{opWord, 4u | (dstSel << 8u) | (unused << 11u) | (4u << 16u)};
                const auto instruction = ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
                Require(instruction.family == ShaderRecompiler::RdnaInstructionFamily::VOP1, "instruction must be VOP1");
                Require(instruction.destination.sdwaSel == dstSel, "destination sdwaSel mismatch");
                Require(instruction.destination.sdwaDstUnused == unused, "destination sdwaDstUnused mismatch");
                Require(instruction.destination.explicitSdwaDst, "destination explicitSdwaDst must be true");
                Require(instruction.source0.sdwaSel == 4u, "source0 sdwaSel must be 4");
                Require(!instruction.source0.negate, "source0 negate must be false");
                Require(!instruction.source0.absolute, "source0 absolute must be false");
                Require(!instruction.destination.clamp, "destination clamp must be false");
                Require(instruction.destination.omod == 0u, "destination omod must be 0");
            }
        }

        {
            const std::array<std::uint32_t, 2> words{opWord, 4u | (3u << 8u) | (1u << 11u) | (5u << 16u) | (1u << 20u) | (1u << 21u)};
            const auto instruction = ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
            Require(instruction.destination.sdwaSel == 3u, "destination sdwaSel must be 3");
            Require(instruction.destination.sdwaDstUnused == 1u, "destination sdwaDstUnused must be 1");
            Require(instruction.source0.sdwaSel == 5u, "source0 sdwaSel must be 5");
            Require(instruction.source0.negate, "source0 negate must be true");
            Require(instruction.source0.absolute, "source0 absolute must be true");
        }

        {
            const std::array<std::uint32_t, 2> words{opWord, 4u | (1u << 8u) | (2u << 11u) | (6u << 16u)};
            const auto instruction = ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
            Require(instruction.destination.sdwaSel == 1u, "destination sdwaSel must be 1");
            Require(instruction.destination.sdwaDstUnused == 2u, "destination sdwaDstUnused must be 2");
            Require(instruction.source0.sdwaSel == 6u, "source0 sdwaSel must be 6");
        }

        ExpectDecodeThrows({opWord, 4u | (7u << 8u) | (4u << 16u)},
            "SDWA destination selector 7 must throw std::invalid_argument");
        ExpectDecodeThrows({opWord, 4u | (0u << 8u) | (3u << 11u) | (4u << 16u)},
            "SDWA destination unused 3 on byte destination must throw std::invalid_argument");
        ExpectDecodeThrows({opWord, 4u | (6u << 8u) | (3u << 11u) | (4u << 16u)},
            "SDWA destination unused 3 on dword destination must throw std::invalid_argument");
        ExpectDecodeThrows({opWord, 4u | (0u << 8u) | (0u << 16u)},
            "SDWA byte source selector must throw std::invalid_argument");
    }

    {
        const std::array<std::uint32_t, 2> words{MantSdwa, 4u | (0u << 8u) | (1u << 13u) | (4u << 16u)};
        const auto instruction = ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
        Require(instruction.destination.clamp, "v_frexp_mant_f16 clamp on byte destination must be accepted");
    }
    {
        const std::array<std::uint32_t, 2> words{MantSdwa, 4u | (0u << 8u) | (1u << 14u) | (4u << 16u)};
        const auto instruction = ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
        Require(instruction.destination.omod == 1u, "v_frexp_mant_f16 omod on byte destination must be accepted");
    }
    ExpectDecodeThrows({ExpSdwa, 4u | (0u << 8u) | (1u << 13u) | (4u << 16u)},
        "SDWA clamp on v_frexp_exp_i16_f16 byte destination must throw std::invalid_argument");
    ExpectDecodeThrows({ExpSdwa, 4u | (0u << 8u) | (1u << 14u) | (4u << 16u)},
        "SDWA omod on v_frexp_exp_i16_f16 byte destination must throw std::invalid_argument");
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

std::uint16_t ClampExpectedMant(std::uint16_t mant) {
    if ((mant & 0x8000u) != 0u) return 0u;
    if ((mant & 0x7fffu) > 0x3c00u) return 0x3c00u;
    return mant;
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

std::uint32_t InsertResult(std::uint16_t result16, std::uint32_t dstSel, std::uint32_t dstUnused) {
    const auto width = dstSel < 4u ? 8u : 16u;
    const auto offset = dstSel < 4u ? dstSel * 8u : (dstSel == 5u ? 16u : 0u);
    const auto mask = (1u << width) - 1u;
    auto field = static_cast<std::uint32_t>(result16) & mask;
    if (dstUnused == 1u && (field & (1u << (width - 1u))) != 0u) {
        field |= ~mask;
    }
    return (field << offset) | (dstUnused == 2u ? (Garbage & ~(mask << offset)) : 0u);
}

void RunExecutionTests(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    FillInput(waveSize);
    Output.fill(0xdeadbeefu);

    std::vector<std::uint32_t> words{
        0x160200ffu, 1u,
        0x160400ffu, TotalResults,
        0xe0302000u, 0x80000401u,
        0xbf8c3f70u,
    };

    std::uint32_t index = 0u;
    for (const auto opcode : {FrexpMantOpcode, FrexpExpOpcode}) {
        for (const auto srcSel : {4u, 5u, 6u}) {
            for (const auto neg : {0u, 1u}) {
                for (const auto abs : {0u, 1u}) {
                    for (std::uint32_t dstSel = 0u; dstSel < 4u; ++dstSel) {
                        for (std::uint32_t dstUnused = 0u; dstUnused < 3u; ++dstUnused, ++index) {
                            words.insert(words.end(), {
                                0x7e0a02ffu, Garbage,
                                0x7e0a00f9u | (opcode << 9u),
                                4u | (dstSel << 8u) | (dstUnused << 11u) | (srcSel << 16u) | (neg << 20u) | (abs << 21u),
                                0xe0702000u | (index * 4u), 0x80010502u,
                            });
                        }
                    }
                }
            }
        }
    }

    for (std::uint32_t dstSel = 0u; dstSel < 4u; ++dstSel) {
        for (std::uint32_t dstUnused = 0u; dstUnused < 3u; ++dstUnused, ++index) {
            words.insert(words.end(), {
                0x7e0a02ffu, Garbage,
                0x7e0a00f9u | (FrexpMantOpcode << 9u),
                4u | (dstSel << 8u) | (dstUnused << 11u) | (1u << 13u) | (4u << 16u),
                0xe0702000u | (index * 4u), 0x80010502u,
            });
        }
    }
    Require(index == TotalResults, "result count mismatch");
    words.push_back(0xbf810000u);

    std::vector<std::uint32_t> userData(8u, 0u);
    const auto inputDesc = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto outputDesc = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(inputDesc.begin(), inputDesc.end(), userData.begin());
    std::copy(outputDesc.begin(), outputDesc.end(), userData.begin() + 4);

    const std::span<const std::uint32_t> code(words);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0u, {false, false, false}, false, 1};
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

    index = 0u;
    for (const auto opcode : {FrexpMantOpcode, FrexpExpOpcode}) {
        for (const auto srcSel : {4u, 5u, 6u}) {
            for (const auto neg : {0u, 1u}) {
                for (const auto abs : {0u, 1u}) {
                    for (std::uint32_t dstSel = 0u; dstSel < 4u; ++dstSel) {
                        for (std::uint32_t dstUnused = 0u; dstUnused < 3u; ++dstUnused, ++index) {
                            for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
                                const auto rawHalf = static_cast<std::uint16_t>(srcSel == 5u ? (Input[lane] >> 16u) : (Input[lane] & 0xffffu));
                                auto modHalf = rawHalf;
                                if (abs != 0u) modHalf = static_cast<std::uint16_t>(modHalf & 0x7fffu);
                                if (neg != 0u) modHalf = static_cast<std::uint16_t>(modHalf ^ 0x8000u);
                                const auto res16 = (opcode == FrexpMantOpcode) ? ComputeExpectedMant(modHalf) : ComputeExpectedExp(modHalf);
                                const auto expected = InsertResult(res16, dstSel, dstUnused);
                                const auto actual = Output[lane * TotalResults + index];
                                Require(actual == expected, "sdwa frexp bytes lane " + std::to_string(lane) +
                                    " wave" + std::to_string(waveSize) + " op " + Hex(opcode) +
                                    " srcSel " + std::to_string(srcSel) + " neg " + std::to_string(neg) +
                                    " abs " + std::to_string(abs) + " dstSel " + std::to_string(dstSel) +
                                    " unused " + std::to_string(dstUnused) + ": got " + Hex(actual) +
                                    ", expected " + Hex(expected));
                            }
                        }
                    }
                }
            }
        }
    }

    for (std::uint32_t dstSel = 0u; dstSel < 4u; ++dstSel) {
        for (std::uint32_t dstUnused = 0u; dstUnused < 3u; ++dstUnused, ++index) {
            for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
                const auto rawHalf = static_cast<std::uint16_t>(Input[lane] & 0xffffu);
                const auto res16 = ClampExpectedMant(ComputeExpectedMant(rawHalf));
                const auto expected = InsertResult(res16, dstSel, dstUnused);
                const auto actual = Output[lane * TotalResults + index];
                Require(actual == expected, "sdwa frexp_mant clamp bytes lane " + std::to_string(lane) +
                    " wave" + std::to_string(waveSize) + " dstSel " + std::to_string(dstSel) +
                    " unused " + std::to_string(dstUnused) + ": got " + Hex(actual) +
                    ", expected " + Hex(expected));
            }
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        TestDecoderSdwaSelectors();
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
