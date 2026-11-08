#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Garbage = 0xabcd1234u;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 81> SdwaCode{
    0x34020082, 0x34060084, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0x7e1402ff, 0xabcd1234, 0x7e1602ff, 0xabcd1234, 0x7e1802ff, 0xabcd1234, 0x7e1a02ff, 0xabcd1234,
    0x7e1c02ff, 0xabcd1234, 0x7e1e02ff, 0xabcd1234, 0x7e2002ff, 0xabcd1234, 0x7e2202ff, 0xabcd1234,
    0x7e2402ff, 0xabcd1234, 0xbf8c3f70, 0x7e1414f9, 0x00060604, 0x7e161504,
    0x7e1814f9, 0x00061505, 0x7e1a14f9, 0x00060604, 0x7e1c14f9, 0x00060605, 0xd746000d, 0x0435210e,
    0x641e0cf9, 0x05040606, 0x7e2002f9, 0x00061004, 0x7e2202f9, 0x00061204, 0x7e2402f9, 0x00060104,
    0x7e2602ff, 0xabcd1234, 0x7e266ef9, 0x00061404,
    0x7e2802ff, 0xabcd1234, 0x7e28a0f9, 0x00031404, 0x7e2a02ff, 0xabcd1234, 0x7e2aa2f9, 0x000b1404,
    0xe0702000, 0x80010a03, 0xe0702004, 0x80010b03, 0xe0702008, 0x80010c03,
    0xe070200c, 0x80010d03, 0xe0702010, 0x80010f03, 0xe0702014, 0x80011003, 0xe0702018, 0x80011103,
    0xe070201c, 0x80011203, 0xe0702020, 0x80011303, 0xe0702024, 0x80011403, 0xe0702028, 0x80011503, 0xbf810000,
};

float Quarter(std::uint32_t index) {
    return static_cast<float>(static_cast<int>(index % 17u) - 8) / 4.0f;
}

std::uint32_t HalfBits(float value) {
    if (value == 0.0f) return std::signbit(value) ? 0x8000u : 0u;
    int exponent = 0;
    const float mantissa = std::frexp(std::fabs(value), &exponent);
    const auto fraction = static_cast<std::uint32_t>(std::ldexp(mantissa, 11));
    Require(exponent >= -13 && exponent <= 16 && std::ldexp(static_cast<float>(fraction), exponent - 11) == std::fabs(value), "sdwa destination: a model value is not a normal f16");
    return (std::signbit(value) ? 0x8000u : 0u) | (static_cast<std::uint32_t>(exponent + 14) << 10u) | (fraction & 0x3ffu);
}

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        words[0] = std::bit_cast<std::uint32_t>(Quarter(tid * 3u + 1u));
        words[1] = std::bit_cast<std::uint32_t>(Quarter(tid * 5u + 2u));
        words[2] = HalfBits(Quarter(tid * 7u + 3u)) | (HalfBits(Quarter(tid * 11u + 4u)) << 16u);
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
    const std::span<const std::uint32_t> code(SdwaCode);
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
        const auto a = HalfBits(std::bit_cast<float>(in[0]));
        const auto b = HalfBits(std::bit_cast<float>(in[1]));
        const auto sum = HalfBits(Quarter(tid * 7u + 3u) + Quarter(tid * 11u + 4u));
        const auto where = [&](std::uint32_t result) { return "sdwa destination: thread " + std::to_string(tid) + " result " + std::to_string(result) + " is " + Hex(out[result]); };
        Require(out[0] == a, where(0) + ", expected " + Hex(a) + ": v_cvt_f16_f32_sdwa dst_sel:DWORD dst_unused:UNUSED_PAD must zero the high half");
        Require(out[1] == ((Garbage & 0xffff0000u) | a), where(1) + ", expected " + Hex((Garbage & 0xffff0000u) | a) + ": v_cvt_f16_f32_e32 must keep the high half");
        Require(out[2] == ((b << 16u) | (Garbage & 0xffffu)), where(2) + ", expected " + Hex((b << 16u) | (Garbage & 0xffffu)) + ": dst_sel:WORD_1 dst_unused:UNUSED_PRESERVE must keep the low half");
        Require(out[3] == (a | (b << 16u)), where(3) + ", expected " + Hex(a | (b << 16u)) + ": two padded conversions packed with v_lshl_add_u32");
        Require(out[4] == sum, where(4) + ", expected " + Hex(sum) + ": v_add_f16_sdwa dst_sel:DWORD dst_unused:UNUSED_PAD must zero the high half");
        const auto low = in[0] & 0xffu;
        Require(out[5] == ((Garbage & 0xffffff00u) | low), where(5) + ", expected " + Hex((Garbage & 0xffffff00u) | low) + ": v_mov_b32_sdwa dst_sel:BYTE_0 dst_unused:UNUSED_PRESERVE must keep the other bytes");
        Require(out[6] == ((Garbage & 0xff00ffffu) | (low << 16u)), where(6) + ", expected " + Hex((Garbage & 0xff00ffffu) | (low << 16u)) + ": v_mov_b32_sdwa dst_sel:BYTE_2 dst_unused:UNUSED_PRESERVE must keep the other bytes");
        Require(out[7] == (low << 8u), where(7) + ", expected " + Hex(low << 8u) + ": v_mov_b32_sdwa dst_sel:BYTE_1 dst_unused:UNUSED_PAD must zero the other bytes");
        const auto inverted = ~in[0] & 0xffffu;
        Require(out[8] == ((Garbage & 0xffff0000u) | inverted), where(8) + ", expected " + Hex((Garbage & 0xffff0000u) | inverted) + ": v_not_b32_sdwa dst_sel:WORD_0 dst_unused:UNUSED_PRESERVE must keep the high half");
        const auto top = in[0] >> 24u;
        const auto unsignedHalf = HalfBits(static_cast<float>(top));
        Require(out[9] == ((Garbage & 0xffff0000u) | unsignedHalf), where(9) + ", expected " + Hex((Garbage & 0xffff0000u) | unsignedHalf) + ": v_cvt_f16_u16_sdwa dst_sel:WORD_0 dst_unused:UNUSED_PRESERVE src0_sel:BYTE_3 must convert the zero-extended top byte");
        const auto signedHalf = HalfBits(static_cast<float>(static_cast<std::int8_t>(top)));
        Require(out[10] == ((Garbage & 0xffff0000u) | signedHalf), where(10) + ", expected " + Hex((Garbage & 0xffff0000u) | signedHalf) + ": v_cvt_f16_i16_sdwa dst_sel:WORD_0 dst_unused:UNUSED_PRESERVE src0_sel:BYTE_3 src0_sext must convert the sign-extended top byte");
    }
}

constexpr std::uint32_t ConversionInputs = 14u;
constexpr std::uint32_t ConversionResults = ConversionInputs * 6u * 3u;
constexpr std::array<std::int32_t, 16> Integers{
    0, 1, -1, 74565, 1193046, -74565, -1193046, 127,
    -128, 255, -256, 32767, -32768, 65535, -65536, 0x123456,
};
alignas(256) std::array<std::uint32_t, Threads * ConversionInputs> ConversionInput{};
alignas(256) std::array<std::uint32_t, Threads * ConversionResults> ConversionOutput{};

std::uint32_t FieldWidth(std::uint32_t selector) {
    return selector < 4u ? 8u : selector < 6u ? 16u : 32u;
}

std::uint32_t FieldOffset(std::uint32_t selector) {
    return selector < 4u ? selector * 8u : selector == 5u ? 16u : 0u;
}

std::uint32_t SelectInteger(std::uint32_t word, std::uint32_t selector, bool sign) {
    if (selector == 6u) return word;
    const auto width = FieldWidth(selector);
    const auto mask = (1u << width) - 1u;
    const auto field = (word >> FieldOffset(selector)) & mask;
    return sign && (field & (1u << (width - 1u))) ? field | ~mask : field;
}

std::uint32_t ExactFloatBits(std::uint32_t value, bool signedInteger) {
    const bool negative = signedInteger && (value & 0x80000000u);
    const auto magnitude = negative ? 0u - value : value;
    if (magnitude == 0u) return 0u;
    const auto msb = static_cast<std::uint32_t>(std::bit_width(magnitude) - 1u);
    Require(msb <= 23u || (magnitude & ((1u << (msb - 23u)) - 1u)) == 0u, "integer-to-f32 model requires an exactly representable integer");
    const auto significand = msb <= 23u ? magnitude << (23u - msb) : magnitude >> (msb - 23u);
    return (negative ? 0x80000000u : 0u) | ((msb + 127u) << 23u) | (significand & 0x7fffffu);
}

std::uint32_t InsertResult(std::uint32_t bits, std::uint32_t destination, std::uint32_t unused) {
    const auto width = FieldWidth(destination);
    const auto offset = FieldOffset(destination);
    const auto mask = (1u << width) - 1u;
    auto field = bits & mask;
    if (unused == 1u && (field & (1u << (width - 1u)))) field |= ~mask;
    return (field << offset) | (unused == 2u ? Garbage & ~(mask << offset) : 0u);
}

void RunConversions(AgcDriver::VulkanDevice& device, bool signedInteger, std::uint32_t waveSize) {
    Require(ExactFloatBits(74565u, false) == 0x4791a280u && ExactFloatBits(1193046u, false) == 0x4991a2b0u,
        "integer-to-f32 model anchors");
    for (std::uint32_t lane = 0u; lane < Threads; ++lane) {
        for (std::uint32_t source = 0u; source < 7u; ++source) {
            for (std::uint32_t sign = 0u; sign < 2u; ++sign) {
                const auto integer = Integers[lane % Integers.size()];
                auto word = static_cast<std::uint32_t>(integer);
                if (!signedInteger) {
                    if (source == 6u) word = static_cast<std::uint32_t>(integer < 0 ? -integer : integer);
                    else if (sign != 0u && source < 4u) word &= ~(0x80u << FieldOffset(source));
                    else if (sign != 0u) word &= ~(0x8000u << FieldOffset(source));
                }
                ExactFloatBits(SelectInteger(word, source, sign != 0u), signedInteger);
                ConversionInput[lane * ConversionInputs + source * 2u + sign] = word;
            }
        }
    }
    ConversionOutput.fill(0xdeadbeefu);
    std::vector<std::uint32_t> words{
        0x160200ffu, ConversionInputs, 0x160400ffu, ConversionResults,
    };
    std::uint32_t index = 0u;
    for (std::uint32_t source = 0u; source < 7u; ++source) {
        for (std::uint32_t sign = 0u; sign < 2u; ++sign) {
            words.insert(words.end(), {0xe0302000u | ((source * 2u + sign) * 4u), 0x80000401u, 0xbf8c3f70u});
            for (std::uint32_t destination = 0u; destination < 6u; ++destination) {
                for (std::uint32_t unused = 0u; unused < 3u; ++unused, ++index) {
                    const auto destinationRegister = 5u + index % 4u;
                    words.insert(words.end(), {
                        0x7e0002ffu | (destinationRegister << 17u), Garbage,
                        0x7e0000f9u | (destinationRegister << 17u) | ((signedInteger ? 5u : 6u) << 9u),
                        4u | (destination << 8u) | (unused << 11u) | (source << 16u) | (sign << 19u),
                    });
                    if (index % 4u == 3u) words.insert(words.end(), {0xe0782000u | ((index - 3u) * 4u), 0x80010502u});
                }
            }
        }
    }
    words.push_back(0xbf810000u);
    std::vector<std::uint32_t> userData(8u);
    const auto input = BufferDescriptor(ConversionInput.data(), static_cast<std::uint32_t>(ConversionInput.size()));
    const auto output = BufferDescriptor(ConversionOutput.data(), static_cast<std::uint32_t>(ConversionOutput.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
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
    index = 0u;
    for (std::uint32_t source = 0u; source < 7u; ++source) {
        for (std::uint32_t sign = 0u; sign < 2u; ++sign) {
            for (std::uint32_t destination = 0u; destination < 6u; ++destination) {
                for (std::uint32_t unused = 0u; unused < 3u; ++unused, ++index) {
                    for (std::uint32_t lane = 0u; lane < Threads; ++lane) {
                        const auto word = ConversionInput[lane * ConversionInputs + source * 2u + sign];
                        const auto bits = ExactFloatBits(SelectInteger(word, source, sign != 0u), signedInteger);
                        const auto expected = InsertResult(bits, destination, unused);
                        const auto actual = ConversionOutput[lane * ConversionResults + index];
                        Require(actual == expected, "sdwa integer-to-f32: lane " + std::to_string(lane) +
                            (signedInteger ? " i32" : " u32") + " wave" + std::to_string(waveSize) + " source " +
                            std::to_string(source) + " sext " + std::to_string(sign) + " destination " +
                            std::to_string(destination) + " unused " + std::to_string(unused) + " is " +
                            Hex(actual) + ", expected " + Hex(expected));
                    }
                }
            }
        }
    }
    std::printf("sdwa integer-to-f32 %s wave%u: %u combinations passed\n", signedInteger ? "i32" : "u32", waveSize, ConversionResults);
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(argc == 1 || (argc == 2 && std::string(argv[1]) == "wave64"), "expected no argument or wave64");
        const auto waveSize = argc == 2 ? 64u : 32u;
        for (const bool signedInteger : {false, true}) RunConversions(*device, signedInteger, waveSize);
        FillInput();
        Run(*device);
        Check();
        std::puts("sdwa destination tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
