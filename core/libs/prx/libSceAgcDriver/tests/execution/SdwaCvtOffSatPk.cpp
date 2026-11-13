#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Selectors = 7;
constexpr std::uint32_t Garbage = 0xabcd1234u;

alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Inputs> Output{};

alignas(256) std::array<std::uint32_t, 12> SdwaCode{
    0x34020082, 0x34060082,
    0xe0302000, 0x80000401,
    0x7e1402ff, Garbage,
    0xbf8c3f70,
    0x7e141cf9, 0x00060604,
    0xe0702000, 0x80010a03,
    0xbf810000,
};

constexpr std::array<std::uint32_t, 17> Vectors{{
    0xfe0cffffu,
    0x00ff0000u,
    0x000000ffu,
    0x03e801f4u,
    0x00c8002au,
    0x00320064u,
    0x7fff8000u,
    0x80007fffu,
    0x0080fff6u,
    0xfff6007fu,
    0x00020001u,
    0x00ff0100u,
    0x008100feu,
    0x80008000u,
    0x75307530u,
    0x00b1004du,
    0x0b090700u,
}};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t ExtractSource(std::uint32_t input, std::uint32_t srcSel, bool signExtend) {
    if (srcSel == 6u) return input;
    const auto width = srcSel < 4u ? 8u : 16u;
    const auto offset = srcSel < 4u ? srcSel * 8u : (srcSel == 5u ? 16u : 0u);
    const auto mask = (1u << width) - 1u;
    auto field = (input >> offset) & mask;
    if (signExtend && (field & (1u << (width - 1u))) != 0u) {
        field |= ~mask;
    }
    return field;
}

std::uint32_t CvtOffF32I4Bits(std::uint32_t src) {
    auto nibble = static_cast<std::int32_t>(src & 0xfu);
    if ((nibble & 0x8) != 0) {
        nibble |= ~0xf;
    }
    const float value = static_cast<float>(nibble);
    const float result = value * (1.0f / 16.0f);
    return std::bit_cast<std::uint32_t>(result);
}

std::uint32_t SatPkU8I16Bits(std::uint32_t input) {
    const auto s0 = static_cast<std::int16_t>(input & 0xffffu);
    const auto s1 = static_cast<std::int16_t>((input >> 16u) & 0xffffu);
    const auto u0 = static_cast<std::uint32_t>(std::clamp<std::int32_t>(s0, 0, 255));
    const auto u1 = static_cast<std::uint32_t>(std::clamp<std::int32_t>(s1, 0, 255));
    return u0 | (u1 << 8u);
}

std::uint32_t InsertSdwaDst(std::uint32_t result, std::uint32_t dstSel, std::uint32_t dstUnused) {
    if (dstSel == 6u) return result;
    const auto width = dstSel < 4u ? 8u : 16u;
    const auto offset = dstSel < 4u ? dstSel * 8u : (dstSel == 5u ? 16u : 0u);
    const auto mask = (1u << width) - 1u;
    auto field = result & mask;
    if (dstUnused == 1u && (field & (1u << (width - 1u))) != 0u) {
        field |= ~mask;
    }
    return (field << offset) | (dstUnused == 2u ? (Garbage & ~(mask << offset)) : 0u);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * Inputs] = Vectors[tid % Vectors.size()];
    }
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::uint32_t opcodeWord, std::uint32_t destination, std::uint32_t unused, std::uint32_t selector, bool signExtend) {
    SdwaCode[7] = opcodeWord;
    SdwaCode[8] = 4u | (destination << 8u) | (unused << 11u) | (selector << 16u) | (signExtend ? (1u << 19u) : 0u);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(SdwaCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0, {false, false, false}, false, 1};
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

void CheckCvtOff(std::uint32_t waveSize, std::uint32_t destination, std::uint32_t unused, std::uint32_t selector, bool signExtend) {
    for (std::uint32_t tid = 0; tid < waveSize; ++tid) {
        const auto val = Input[tid * Inputs];
        const auto src = ExtractSource(val, selector, signExtend);
        const auto fbits = CvtOffF32I4Bits(src);
        const auto expected = InsertSdwaDst(fbits, destination, unused);
        const auto actual = Output[tid * Inputs];
        Require(actual == expected, "v_cvt_off_f32_i4 mismatch: lane=" + std::to_string(tid) +
                                        " dst=" + std::to_string(destination) +
                                        " unused=" + std::to_string(unused) +
                                        " sel=" + std::to_string(selector) +
                                        " sext=" + std::to_string(signExtend ? 1 : 0) +
                                        " actual=" + Hex(actual) +
                                        " expected=" + Hex(expected));
    }
}

void CheckSatPk(std::uint32_t waveSize, std::uint32_t destination, std::uint32_t unused) {
    for (std::uint32_t tid = 0; tid < waveSize; ++tid) {
        const auto val = Input[tid * Inputs];
        const auto packed = SatPkU8I16Bits(val);
        const auto expected = InsertSdwaDst(packed, destination, unused);
        const auto actual = Output[tid * Inputs];
        Require(actual == expected, "v_sat_pk_u8_i16 mismatch: lane=" + std::to_string(tid) +
                                        " dst=" + std::to_string(destination) +
                                        " unused=" + std::to_string(unused) +
                                        " actual=" + Hex(actual) +
                                        " expected=" + Hex(expected));
    }
}

void CheckRejections() {
    constexpr std::uint32_t SatPkOpcodeWord = 0x7e14c4f9u;
    constexpr std::uint32_t CvtOffOpcodeWord = 0x7e141cf9u;

    const auto expectInvalid = [](std::uint32_t opWord, std::uint32_t modifier) {
        const std::array<std::uint32_t, 2> words{opWord, modifier};
        bool threw = false;
        try {
            static_cast<void>(ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u));
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw, "expected std::invalid_argument for word0=" + Hex(opWord) + " modifier=" + Hex(modifier));
    };

    for (std::uint32_t srcSel = 0; srcSel < 6u; ++srcSel) {
        for (const std::uint32_t dst : {4u, 5u, 6u}) {
            expectInvalid(SatPkOpcodeWord, 4u | (dst << 8u) | (0u << 11u) | (srcSel << 16u));
        }
    }

    for (const std::uint32_t dst : {4u, 5u, 6u}) {
        expectInvalid(SatPkOpcodeWord, 4u | (dst << 8u) | (0u << 11u) | (7u << 16u));
        expectInvalid(SatPkOpcodeWord, 4u | (dst << 8u) | (3u << 11u) | (6u << 16u));
    }
    expectInvalid(SatPkOpcodeWord, 4u | (7u << 8u) | (0u << 11u) | (6u << 16u));

    for (std::uint32_t byteDst = 0; byteDst < 4u; ++byteDst) {
        expectInvalid(SatPkOpcodeWord, 4u | (byteDst << 8u) | (0u << 11u) | (6u << 16u));
    }

    for (std::uint32_t dst = 0; dst <= 6u; ++dst) {
        expectInvalid(CvtOffOpcodeWord, 4u | (dst << 8u) | (0u << 11u) | (7u << 16u));
        expectInvalid(CvtOffOpcodeWord, 4u | (dst << 8u) | (3u << 11u) | (6u << 16u));
    }
    expectInvalid(CvtOffOpcodeWord, 4u | (7u << 8u) | (0u << 11u) | (6u << 16u));

    for (const auto opWord : {SatPkOpcodeWord, CvtOffOpcodeWord}) {
        expectInvalid(opWord, 4u | (6u << 8u) | (0u << 11u) | (6u << 16u) | (1u << 20u));
        expectInvalid(opWord, 4u | (6u << 8u) | (0u << 11u) | (6u << 16u) | (1u << 21u));
    }
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 1 || (argc == 2 && std::string(argv[1]) == "wave64"), "expected no argument or wave64");
        const auto waveSize = argc == 2 ? 64u : 32u;
        CheckRejections();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        constexpr std::uint32_t CvtOffOpcodeWord = 0x7e141cf9u;
        constexpr std::uint32_t SatPkOpcodeWord = 0x7e14c4f9u;

        for (const bool signExtend : {false, true}) {
            for (std::uint32_t selector = 0; selector < Selectors; ++selector) {
                Run(*device, waveSize, CvtOffOpcodeWord, 6u, 0u, selector, signExtend);
                CheckCvtOff(waveSize, 6u, 0u, selector, signExtend);
            }
        }
        for (std::uint32_t destination = 0; destination < 6; ++destination) {
            for (std::uint32_t unused = 0; unused < 3; ++unused) {
                for (const bool signExtend : {false, true}) {
                    for (std::uint32_t selector = 0; selector < Selectors; ++selector) {
                        Run(*device, waveSize, CvtOffOpcodeWord, destination, unused, selector, signExtend);
                        CheckCvtOff(waveSize, destination, unused, selector, signExtend);
                    }
                }
            }
        }

        Run(*device, waveSize, SatPkOpcodeWord, 6u, 0u, 6u, false);
        CheckSatPk(waveSize, 6u, 0u);
        for (const std::uint32_t destination : {4u, 5u}) {
            for (std::uint32_t unused = 0; unused < 3u; ++unused) {
                Run(*device, waveSize, SatPkOpcodeWord, destination, unused, 6u, false);
                CheckSatPk(waveSize, destination, unused);
            }
        }

        std::printf("sdwa cvt off sat pk wave%u tests passed\n", waveSize);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
