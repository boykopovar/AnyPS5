#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

struct Compare {
    std::uint32_t opcode;
    const char* name;
    bool (*evaluate)(std::uint32_t value, std::uint16_t immediate);
};

constexpr std::int32_t Signed(std::uint32_t value) {
    return static_cast<std::int32_t>(value);
}

constexpr std::int32_t SignExtended(std::uint16_t immediate) {
    return static_cast<std::int16_t>(immediate);
}

constexpr std::uint32_t ZeroExtended(std::uint16_t immediate) {
    return immediate;
}

constexpr std::array<Compare, 12> Compares{{
    {0x03u, "s_cmpk_eq_i32", [](std::uint32_t value, std::uint16_t immediate) { return Signed(value) == SignExtended(immediate); }},
    {0x04u, "s_cmpk_lg_i32", [](std::uint32_t value, std::uint16_t immediate) { return Signed(value) != SignExtended(immediate); }},
    {0x05u, "s_cmpk_gt_i32", [](std::uint32_t value, std::uint16_t immediate) { return Signed(value) > SignExtended(immediate); }},
    {0x06u, "s_cmpk_ge_i32", [](std::uint32_t value, std::uint16_t immediate) { return Signed(value) >= SignExtended(immediate); }},
    {0x07u, "s_cmpk_lt_i32", [](std::uint32_t value, std::uint16_t immediate) { return Signed(value) < SignExtended(immediate); }},
    {0x08u, "s_cmpk_le_i32", [](std::uint32_t value, std::uint16_t immediate) { return Signed(value) <= SignExtended(immediate); }},
    {0x09u, "s_cmpk_eq_u32", [](std::uint32_t value, std::uint16_t immediate) { return value == ZeroExtended(immediate); }},
    {0x0au, "s_cmpk_lg_u32", [](std::uint32_t value, std::uint16_t immediate) { return value != ZeroExtended(immediate); }},
    {0x0bu, "s_cmpk_gt_u32", [](std::uint32_t value, std::uint16_t immediate) { return value > ZeroExtended(immediate); }},
    {0x0cu, "s_cmpk_ge_u32", [](std::uint32_t value, std::uint16_t immediate) { return value >= ZeroExtended(immediate); }},
    {0x0du, "s_cmpk_lt_u32", [](std::uint32_t value, std::uint16_t immediate) { return value < ZeroExtended(immediate); }},
    {0x0eu, "s_cmpk_le_u32", [](std::uint32_t value, std::uint16_t immediate) { return value <= ZeroExtended(immediate); }},
}};

constexpr std::array<std::uint32_t, 10> Values{
    0x00000000u, 0x00000001u, 0x00001234u, 0x00007fffu, 0x00008000u, 0x0000ffffu, 0x7fffffffu, 0x80000000u, 0xffff8000u, 0xffffffffu,
};

constexpr std::array<std::uint16_t, 6> Immediates{0x0000u, 0x0001u, 0x1234u, 0x7fffu, 0x8000u, 0xffffu};

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Results = 64;
constexpr std::uint32_t OutputRegister = 4;
constexpr std::uint32_t ValueRegister = 8;
constexpr std::uint32_t MaskRegister = 30;
constexpr std::uint32_t BitRegister = 31;
constexpr std::uint32_t ThreadVector = 0;
constexpr std::uint32_t IndexVector = 3;
constexpr std::uint32_t DataVector = 10;
constexpr std::uint32_t ConstantZero = 0x80u;
constexpr std::uint32_t ConstantSix = 0x86u;
constexpr std::uint32_t Literal = 0xffu;
constexpr std::uint32_t Endpgm = 0xbf810000u;
constexpr std::size_t CodeWords = 2u + Values.size() * Immediates.size() * (4u + Compares.size() * 4u);

alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

constexpr std::uint32_t Sop1(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source) {
    return 0xbe800000u | (destination << 16u) | (opcode << 8u) | source;
}

constexpr std::uint32_t Sop2(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source0, std::uint32_t source1) {
    return 0x80000000u | (opcode << 23u) | (destination << 16u) | (source1 << 8u) | source0;
}

constexpr std::uint32_t Sopk(std::uint32_t opcode, std::uint32_t reg, std::uint16_t immediate) {
    return 0xb0000000u | (opcode << 23u) | (reg << 16u) | immediate;
}

constexpr std::uint32_t Vop1(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source) {
    return 0x7e000000u | (destination << 17u) | (opcode << 9u) | source;
}

constexpr std::uint32_t Vop2(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source0, std::uint32_t source1) {
    return (opcode << 25u) | (destination << 17u) | (source1 << 9u) | source0;
}

constexpr std::array<std::uint32_t, CodeWords> BuildCode() {
    constexpr std::uint32_t sMovB32 = 0x03u;
    constexpr std::uint32_t sCselectB32 = 0x0au;
    constexpr std::uint32_t sOrB32 = 0x10u;
    constexpr std::uint32_t vMovB32 = 0x01u;
    constexpr std::uint32_t vLshlrevB32 = 0x1au;
    constexpr std::uint32_t bufferStoreDwordIndexed = 0xe0702000u;
    std::array<std::uint32_t, CodeWords> code{};
    std::size_t count = 0;
    code[count++] = Vop2(vLshlrevB32, IndexVector, ConstantSix, ThreadVector);
    for (std::uint32_t value = 0; value < Values.size(); ++value) {
        for (std::uint32_t immediate = 0; immediate < Immediates.size(); ++immediate) {
            code[count++] = Sop1(sMovB32, MaskRegister, ConstantZero);
            for (std::uint32_t compare = 0; compare < Compares.size(); ++compare) {
                code[count++] = Sopk(Compares[compare].opcode, ValueRegister + value, Immediates[immediate]);
                code[count++] = Sop2(sCselectB32, BitRegister, Literal, ConstantZero);
                code[count++] = 1u << compare;
                code[count++] = Sop2(sOrB32, MaskRegister, MaskRegister, BitRegister);
            }
            const auto result = static_cast<std::uint32_t>(value * Immediates.size() + immediate);
            code[count++] = Vop1(vMovB32, DataVector, MaskRegister);
            code[count++] = bufferStoreDwordIndexed | (result * 4u);
            code[count++] = (ConstantZero << 24u) | ((OutputRegister / 4u) << 16u) | (DataVector << 8u) | IndexVector;
        }
    }
    code[count++] = Endpgm;
    return code;
}

alignas(256) constexpr std::array<std::uint32_t, CodeWords> CompareCode = BuildCode();

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(ValueRegister + Values.size(), 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(output.begin(), output.end(), userData.begin() + OutputRegister);
    std::copy(Values.begin(), Values.end(), userData.begin() + ValueRegister);
    const std::span<const std::uint32_t> code(CompareCode);
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
        for (std::uint32_t value = 0; value < Values.size(); ++value) {
            for (std::uint32_t immediate = 0; immediate < Immediates.size(); ++immediate) {
                const std::uint32_t mask = Output[tid * Results + value * Immediates.size() + immediate];
                Require((mask >> Compares.size()) == 0u, "scalar sopk compare: thread " + std::to_string(tid) + " result for " + Hex(Values[value]) + ", " + Hex(Immediates[immediate]) + " is " + Hex(mask));
                for (std::uint32_t compare = 0; compare < Compares.size(); ++compare) {
                    const bool actual = ((mask >> compare) & 1u) != 0u;
                    const bool expected = Compares[compare].evaluate(Values[value], Immediates[immediate]);
                    Require(actual == expected, "scalar sopk compare: thread " + std::to_string(tid) + " " + Compares[compare].name + " " + Hex(Values[value]) + ", " + Hex(Immediates[immediate]) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
                }
            }
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        std::puts("scalar sopk compare tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
