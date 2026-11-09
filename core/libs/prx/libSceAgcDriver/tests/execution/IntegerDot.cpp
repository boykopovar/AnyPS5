#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Literal = 0x80ff7f01u;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 51> DotCode{
    0x34020082, 0x34060084, 0xe0302000, 0x80000401, 0xe0302004, 0x80000501, 0xe0302008, 0x80000601,
    0xbf8c3f70, 0xcc14400a, 0x1c1a0b04, 0xcc15400b, 0x1c1a0b04, 0xcc16400c, 0x1c1a0b04, 0xcc17400d,
    0x1c1a0b04, 0xcc18400e, 0x1c1a0b04, 0xcc19400f, 0x1c1a0b04, 0x7e200306, 0x1a200b04, 0xcc144811,
    0x141a0b04, 0xcc174012, 0x1c19ff04, Literal, 0xcc184013, 0x1c1a0ac1, 0xe0702000, 0x80010a03,
    0xe0702004, 0x80010b03, 0xe0702008, 0x80010c03, 0xe070200c, 0x80010d03, 0xe0702010, 0x80010e03,
    0xe0702014, 0x80010f03, 0xe0702018, 0x80011003, 0xe070201c, 0x80011103, 0xe0702020, 0x80011203,
    0xe0702024, 0x80011303, 0xbf810000,
};

constexpr std::array<std::array<std::uint32_t, 3>, 12> Edges{{
    {0x80008000u, 0x80008000u, 0u}, {0x80008000u, 0x80008000u, 0x80000000u}, {0xffffffffu, 0xffffffffu, 1u},
    {0xffffffffu, 0xffffffffu, 0xffffffffu}, {0x80808080u, 0x80808080u, 0x7fffffffu}, {0x7f7f7f7fu, 0x80808080u, 0x80000000u},
    {0x88888888u, 0x88888888u, 0x7ffffe00u}, {0x77777777u, 0x88888888u, 0x12345678u}, {0x7fff8000u, 0x80007fffu, 0xffff0000u},
    {0u, 0xffffffffu, 0xdeadbeefu}, {0x01020304u, 0xfffefdfcu, 0u}, {0x0001ffffu, 0xffff0001u, 0x00010000u},
}};

void FillInput() {
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        for (std::uint32_t j = 0; j < 3u; ++j) {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            words[j] = tid < Edges.size() ? Edges[tid][j] : static_cast<std::uint32_t>(state >> 32u);
        }
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

auto Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device) {
    Output.fill(0xdeadbeefu);
    const std::span<const std::uint32_t> code(DotCode);
    const auto result = Compile(device, code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void CheckRefused(AgcDriver::VulkanDevice& device, std::uint32_t word0, std::uint32_t word1, const std::string& reason, const std::string& what) {
    alignas(256) const std::array<std::uint32_t, 3> code{word0, word1, 0xbf810000u};
    std::string refusal;
    try {
        static_cast<void>(Compile(device, code));
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(reason) != std::string::npos, what + " was not refused");
}

void CheckModifiersRefused(AgcDriver::VulkanDevice& device) {
    const std::string modifiers = "VOP3P integer dot source modifiers are not implemented";
    for (std::uint32_t opcode = 0x14u; opcode <= 0x19u; ++opcode) {
        const std::uint32_t word0 = 0xcc00400au | (opcode << 16u);
        const std::string name = "VOP3P opcode " + Hex(opcode);
        CheckRefused(device, word0 | 0x8000u, 0x1c1a0b04u, "VOP3P integer clamp is not implemented", name + " with clamp");
        CheckRefused(device, word0, 0x3c1a0b04u, modifiers, name + " with neg");
        CheckRefused(device, word0 | 0x100u, 0x1c1a0b04u, modifiers, name + " with neg_hi");
        CheckRefused(device, word0 | 0x2000u, 0x1c1a0b04u, modifiers, name + " with op_sel on the addend");
        CheckRefused(device, word0 & ~0x4000u, 0x1c1a0b04u, modifiers, name + " without op_sel_hi on the addend");
    }
    for (std::uint32_t opcode = 0x16u; opcode <= 0x19u; ++opcode) {
        const std::uint32_t word0 = 0xcc00400au | (opcode << 16u);
        const std::string name = "VOP3P opcode " + Hex(opcode);
        CheckRefused(device, word0 | 0x800u, 0x1c1a0b04u, modifiers, name + " with op_sel");
        CheckRefused(device, word0, 0x141a0b04u, modifiers, name + " without op_sel_hi");
    }
    CheckRefused(device, 0xd50d000au, 0x00020b04u, "VOP3 opcode is not implemented", "VOP3-encoded v_dot4c_i32_i8");
}

std::uint32_t Element(std::uint32_t value, std::uint32_t offset, std::uint32_t bits, bool sign) {
    const std::uint32_t field = (value >> offset) & ((1u << bits) - 1u);
    const std::uint32_t signBit = 1u << (bits - 1u);
    return sign ? (field ^ signBit) - signBit : field;
}

std::uint32_t Dot(std::uint32_t lhs, std::uint32_t rhs, std::uint32_t bits, bool sign) {
    std::uint32_t sum = 0u;
    for (std::uint32_t offset = 0; offset < 32u; offset += bits) {
        sum += Element(lhs, offset, bits, sign) * Element(rhs, offset, bits, sign);
    }
    return sum;
}

void Check() {
    constexpr std::array<const char*, 10> names{
        "v_dot2_i32_i16", "v_dot2_u32_u16", "v_dot4_i32_i8", "v_dot4_u32_u8", "v_dot8_i32_i4", "v_dot8_u32_u4", "v_dot4c_i32_i8",
        "v_dot2_i32_i16 with swapped halves", "v_dot4_u32_u8 with a literal", "v_dot8_i32_i4 with an inline constant",
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto* in = &Input[tid * Inputs];
        const std::uint32_t a = in[0];
        const std::uint32_t b = in[1];
        const std::uint32_t c = in[2];
        const std::array<std::uint32_t, 10> expected{
            Dot(a, b, 16u, true) + c,
            Dot(a, b, 16u, false) + c,
            Dot(a, b, 8u, true) + c,
            Dot(a, b, 8u, false) + c,
            Dot(a, b, 4u, true) + c,
            Dot(a, b, 4u, false) + c,
            Dot(a, b, 8u, true) + c,
            Dot((a >> 16u) | (a << 16u), b, 16u, true) + c,
            Dot(a, Literal, 8u, false) + c,
            Dot(0xffffffffu, b, 4u, true) + c,
        };
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            const std::uint32_t actual = Output[tid * Results + j];
            Require(actual == expected[j], std::string(names[j]) + ": thread " + std::to_string(tid) + " (" + Hex(a) + ", " + Hex(b) + ", " + Hex(c) + ") is " + Hex(actual) + ", expected " + Hex(expected[j]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        Run(*device);
        Check();
        CheckModifiersRefused(*device);
        std::puts("integer dot tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
