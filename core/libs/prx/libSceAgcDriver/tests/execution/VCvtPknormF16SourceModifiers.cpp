#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 2;
constexpr std::uint32_t RowCount = 8;
constexpr std::uint32_t VariantCount = 8;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) std::array<std::uint32_t, 14> Code{
    0x34020084u, 0x34060083u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd712000au, 0x00020b04u,
    0xd713000bu, 0x00020b04u, 0xe0701000u, 0x80010a03u, 0xe0701004u, 0x80010b03u, 0xbf810000u,
};

struct Variant {
    const char* operands;
    std::uint32_t abs;
    std::uint32_t neg;
    std::uint32_t opSel;
};

constexpr Variant Variants[VariantCount] = {
    {"|v4|, v5", 1, 0, 0},
    {"v4, |v5|", 2, 0, 0},
    {"-v4, v5", 0, 1, 0},
    {"v4, -v5", 0, 2, 0},
    {"-|v4|, v5", 1, 1, 0},
    {"v4, -|v5|", 2, 2, 0},
    {"v4, v5 op_sel:[1,0,0]", 0, 0, 1},
    {"v4, v5 op_sel:[0,1,0]", 0, 0, 2},
};

constexpr std::uint32_t Rows[RowCount][2] = {
    {0x00000400u, 0x00000400u},
    {0x000083ffu, 0x000083ffu},
    {0x00003555u, 0x0000d640u},
    {0x0000bc00u, 0x00003c00u},
    {0x00007bffu, 0x00000400u},
    {0xbeef0000u, 0xbeef3c00u},
    {0xbeef8000u, 0xbeef0000u},
    {0xbeef0000u, 0xbeeffc00u},
};

constexpr std::uint32_t Expected[VariantCount][Results][RowCount] = {
    {
        {0x00020002u, 0xfffe0002u, 0x80012aa8u, 0x7fff7fffu, 0x00027fffu, 0x7fff0000u, 0x00000000u, 0x80010000u},
        {0x00040004u, 0x00000004u, 0x00005550u, 0xffffffffu, 0x0004ffffu, 0xffff0000u, 0x00000000u, 0x00000000u},
    },
    {
        {0x00020002u, 0x0002fffeu, 0x7fff2aa8u, 0x7fff8001u, 0x00027fffu, 0x7fff0000u, 0x00000000u, 0x7fff0000u},
        {0x00040004u, 0x00040000u, 0xffff5550u, 0xffff0000u, 0x0004ffffu, 0xffff0000u, 0x00000000u, 0xffff0000u},
    },
    {
        {0x0002fffeu, 0xfffe0002u, 0x8001d558u, 0x7fff7fffu, 0x00028001u, 0x7fff0000u, 0x00000000u, 0x80010000u},
        {0x00040000u, 0x00000004u, 0x00000000u, 0xffffffffu, 0x00040000u, 0xffff0000u, 0x00000000u, 0x00000000u},
    },
    {
        {0xfffe0002u, 0x0002fffeu, 0x7fff2aa8u, 0x80018001u, 0xfffe7fffu, 0x80010000u, 0x00000000u, 0x7fff0000u},
        {0x00000004u, 0x00040000u, 0xffff5550u, 0x00000000u, 0x0000ffffu, 0x00000000u, 0x00000000u, 0xffff0000u},
    },
    {
        {0x0002fffeu, 0xfffefffeu, 0x8001d558u, 0x7fff8001u, 0x00028001u, 0x7fff0000u, 0x00000000u, 0x80010000u},
        {0x00040000u, 0x00000000u, 0x00000000u, 0xffff0000u, 0x00040000u, 0xffff0000u, 0x00000000u, 0x00000000u},
    },
    {
        {0xfffe0002u, 0xfffefffeu, 0x80012aa8u, 0x80018001u, 0xfffe7fffu, 0x80010000u, 0x00000000u, 0x80010000u},
        {0x00000004u, 0x00000000u, 0x00005550u, 0x00000000u, 0x0000ffffu, 0x00000000u, 0x00000000u, 0x00000000u},
    },
    {
        {0x00020000u, 0xfffe0000u, 0x80010000u, 0x7fff0000u, 0x00020000u, 0x7fff8001u, 0x00008001u, 0x80018001u},
        {0x00040000u, 0x00000000u, 0x00000000u, 0xffff0000u, 0x00040000u, 0xffff0000u, 0x00000000u, 0x00000000u},
    },
    {
        {0x00000002u, 0x0000fffeu, 0x00002aa8u, 0x00008001u, 0x00007fffu, 0x80010000u, 0x80010000u, 0x80010000u},
        {0x00000004u, 0x00000000u, 0x00005550u, 0x00000000u, 0x0000ffffu, 0x00000000u, 0x00000000u, 0x00000000u},
    },
};

constexpr const char* Opcodes[Results] = {"v_cvt_pknorm_i16_f16_e64 v10, ", "v_cvt_pknorm_u16_f16_e64 v11, "};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, ShaderRecompiler::ShaderFloatMode mode, const Variant& variant) {
    const std::uint32_t first = 0xd7120000u | variant.opSel << 11u | variant.abs << 8u | 10u;
    const std::uint32_t second = 0xd7130000u | variant.opSel << 11u | variant.abs << 8u | 11u;
    const std::uint32_t sources = 0x00020b04u | variant.neg << 29u;
    Code[5] = first;
    Code[6] = sources;
    Code[7] = second;
    Code[8] = sources;
    for (std::uint32_t index = 0; index < RowCount; ++index) {
        std::copy(std::begin(Rows[index]), std::end(Rows[index]), &Input[index * Inputs]);
    }
    for (std::uint32_t index = RowCount * Inputs; index < Input.size(); ++index) Input[index] = 0u;
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
    request.context.floatMode = mode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const char* modeName, std::uint32_t variantIndex) {
    for (std::uint32_t slot = 0; slot < Results; ++slot) {
        for (std::uint32_t index = 0; index < RowCount; ++index) {
            const std::uint32_t actual = Output[index * Results + slot];
            const std::uint32_t expected = Expected[variantIndex][slot][index];
            Require(actual == expected,
                    std::string("v_cvt_pknorm f16 source modifiers: ") + modeName + " row " + std::to_string(index) + " " +
                        Opcodes[slot] + Variants[variantIndex].operands + " of " + Hex(Rows[index][0]) + " and " +
                        Hex(Rows[index][1]) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t index = 0; index < VariantCount; ++index) {
            Run(*device, ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}, Variants[index]);
            Check("FLOAT_MODE 0xf0", index);
            Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}, Variants[index]);
            Check("FLOAT_MODE 0xc0", index);
        }
        std::puts("v_cvt_pknorm f16 source modifiers tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
