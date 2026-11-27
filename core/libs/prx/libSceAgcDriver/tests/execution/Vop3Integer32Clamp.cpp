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
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Checked = 12;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 36> Code{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0xd552800au, 0x041a0b04u,
    0xd553800bu, 0x041a0b04u,
    0xd555800cu, 0x041a0b04u,
    0xd556800du, 0x041a0b04u,
    0xd558800eu, 0x041a0b04u,
    0xd559800fu, 0x041a0b04u,
    0xd5520010u, 0x041a0b04u,
    0xd5530011u, 0x041a0b04u,
    0xd5550012u, 0x041a0b04u,
    0xd5560013u, 0x041a0b04u,
    0xd5580014u, 0x041a0b04u,
    0xd5590015u, 0x041a0b04u,
    0xe0781000u, 0x80010a03u,
    0xe0781010u, 0x80010e03u,
    0xe0781020u, 0x80011203u,
    0xbf810000u,
};

constexpr std::uint32_t Rows[Threads][Inputs] = {
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x00000002u, 0x00000003u, 0x00000000u},
    {0x00000003u, 0x00000002u, 0x00000001u, 0x00000000u},
    {0x00000002u, 0x00000003u, 0x00000001u, 0x00000000u},
    {0x00000001u, 0x00000003u, 0x00000002u, 0x00000000u},
    {0x00000003u, 0x00000001u, 0x00000002u, 0x00000000u},
    {0x00000002u, 0x00000001u, 0x00000003u, 0x00000000u},
    {0xffffffffu, 0x00000000u, 0x00000001u, 0x00000000u},
    {0x7fffffffu, 0x80000000u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x80000001u, 0x7fffffffu, 0x00000000u},
    {0xffffffffu, 0xfffffffeu, 0x00000000u, 0x00000000u},
    {0x12345678u, 0x9abcdef0u, 0x55555555u, 0x00000000u},
    {0xaaaaaaaau, 0x55555555u, 0x00000000u, 0x00000000u},
    {0x00000064u, 0x00000064u, 0x00000064u, 0x00000000u},
    {0x00000064u, 0x000000c8u, 0x00000064u, 0x00000000u},
    {0x000000c8u, 0x00000064u, 0x00000064u, 0x00000000u},
    {0x00000064u, 0x00000064u, 0x000000c8u, 0x00000000u},
    {0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u},
    {0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x00000000u},
    {0xffffffffu, 0xffffffffu, 0xffffffffu, 0x00000000u},
    {0x0000ffffu, 0xffff0000u, 0x12345678u, 0x00000000u},
    {0x80000000u, 0x00000000u, 0x7fffffffu, 0x00000000u},
    {0x80000001u, 0x80000002u, 0x80000003u, 0x00000000u},
    {0x7ffffffdu, 0x7ffffffeu, 0x7fffffffu, 0x00000000u},
    {0x00000000u, 0xffffffffu, 0x7fffffffu, 0x00000000u},
    {0x00000000u, 0x80000000u, 0xffffffffu, 0x00000000u},
    {0x00001000u, 0x00002000u, 0x00003000u, 0x00000000u},
    {0x00003000u, 0x00001000u, 0x00002000u, 0x00000000u},
    {0x55aa55aau, 0xaa55aa55u, 0x12345678u, 0x00000000u},
    {0xdeadbeefu, 0xcafebabeu, 0x8badf00du, 0x00000000u},
    {0x00000005u, 0x0000000au, 0x00000007u, 0x00000000u},
    {0x0000000au, 0x00000005u, 0x00000007u, 0x00000000u},
};

constexpr std::uint32_t Expected[Threads][Checked] = {
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x00000001u, 0x00000003u, 0x00000003u, 0x00000002u, 0x00000002u, 0x00000001u, 0x00000001u, 0x00000003u, 0x00000003u, 0x00000002u, 0x00000002u},
    {0x00000001u, 0x00000001u, 0x00000003u, 0x00000003u, 0x00000002u, 0x00000002u, 0x00000001u, 0x00000001u, 0x00000003u, 0x00000003u, 0x00000002u, 0x00000002u},
    {0x00000001u, 0x00000001u, 0x00000003u, 0x00000003u, 0x00000002u, 0x00000002u, 0x00000001u, 0x00000001u, 0x00000003u, 0x00000003u, 0x00000002u, 0x00000002u},
    {0x00000001u, 0x00000001u, 0x00000003u, 0x00000003u, 0x00000002u, 0x00000002u, 0x00000001u, 0x00000001u, 0x00000003u, 0x00000003u, 0x00000002u, 0x00000002u},
    {0x00000001u, 0x00000001u, 0x00000003u, 0x00000003u, 0x00000002u, 0x00000002u, 0x00000001u, 0x00000001u, 0x00000003u, 0x00000003u, 0x00000002u, 0x00000002u},
    {0x00000001u, 0x00000001u, 0x00000003u, 0x00000003u, 0x00000002u, 0x00000002u, 0x00000001u, 0x00000001u, 0x00000003u, 0x00000003u, 0x00000002u, 0x00000002u},
    {0xffffffffu, 0x00000000u, 0x00000001u, 0xffffffffu, 0x00000000u, 0x00000001u, 0xffffffffu, 0x00000000u, 0x00000001u, 0xffffffffu, 0x00000000u, 0x00000001u},
    {0x80000000u, 0x00000000u, 0x7fffffffu, 0x80000000u, 0x00000000u, 0x7fffffffu, 0x80000000u, 0x00000000u, 0x7fffffffu, 0x80000000u, 0x00000000u, 0x7fffffffu},
    {0x80000000u, 0x7fffffffu, 0x7fffffffu, 0x80000001u, 0x80000001u, 0x80000000u, 0x80000000u, 0x7fffffffu, 0x7fffffffu, 0x80000001u, 0x80000001u, 0x80000000u},
    {0xfffffffeu, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffffffu, 0xfffffffeu, 0xfffffffeu, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffffffu, 0xfffffffeu},
    {0x9abcdef0u, 0x12345678u, 0x55555555u, 0x9abcdef0u, 0x12345678u, 0x55555555u, 0x9abcdef0u, 0x12345678u, 0x55555555u, 0x9abcdef0u, 0x12345678u, 0x55555555u},
    {0xaaaaaaaau, 0x00000000u, 0x55555555u, 0xaaaaaaaau, 0x00000000u, 0x55555555u, 0xaaaaaaaau, 0x00000000u, 0x55555555u, 0xaaaaaaaau, 0x00000000u, 0x55555555u},
    {0x00000064u, 0x00000064u, 0x00000064u, 0x00000064u, 0x00000064u, 0x00000064u, 0x00000064u, 0x00000064u, 0x00000064u, 0x00000064u, 0x00000064u, 0x00000064u},
    {0x00000064u, 0x00000064u, 0x000000c8u, 0x000000c8u, 0x00000064u, 0x00000064u, 0x00000064u, 0x00000064u, 0x000000c8u, 0x000000c8u, 0x00000064u, 0x00000064u},
    {0x00000064u, 0x00000064u, 0x000000c8u, 0x000000c8u, 0x00000064u, 0x00000064u, 0x00000064u, 0x00000064u, 0x000000c8u, 0x000000c8u, 0x00000064u, 0x00000064u},
    {0x00000064u, 0x00000064u, 0x000000c8u, 0x000000c8u, 0x00000064u, 0x00000064u, 0x00000064u, 0x00000064u, 0x000000c8u, 0x000000c8u, 0x00000064u, 0x00000064u},
    {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu},
    {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu},
    {0xffff0000u, 0x0000ffffu, 0x12345678u, 0xffff0000u, 0x0000ffffu, 0x12345678u, 0xffff0000u, 0x0000ffffu, 0x12345678u, 0xffff0000u, 0x0000ffffu, 0x12345678u},
    {0x80000000u, 0x00000000u, 0x7fffffffu, 0x80000000u, 0x00000000u, 0x7fffffffu, 0x80000000u, 0x00000000u, 0x7fffffffu, 0x80000000u, 0x00000000u, 0x7fffffffu},
    {0x80000001u, 0x80000001u, 0x80000003u, 0x80000003u, 0x80000002u, 0x80000002u, 0x80000001u, 0x80000001u, 0x80000003u, 0x80000003u, 0x80000002u, 0x80000002u},
    {0x7ffffffdu, 0x7ffffffdu, 0x7fffffffu, 0x7fffffffu, 0x7ffffffeu, 0x7ffffffeu, 0x7ffffffdu, 0x7ffffffdu, 0x7fffffffu, 0x7fffffffu, 0x7ffffffeu, 0x7ffffffeu},
    {0xffffffffu, 0x00000000u, 0x7fffffffu, 0xffffffffu, 0x00000000u, 0x7fffffffu, 0xffffffffu, 0x00000000u, 0x7fffffffu, 0xffffffffu, 0x00000000u, 0x7fffffffu},
    {0x80000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffffffu, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffffffu, 0x80000000u},
    {0x00001000u, 0x00001000u, 0x00003000u, 0x00003000u, 0x00002000u, 0x00002000u, 0x00001000u, 0x00001000u, 0x00003000u, 0x00003000u, 0x00002000u, 0x00002000u},
    {0x00001000u, 0x00001000u, 0x00003000u, 0x00003000u, 0x00002000u, 0x00002000u, 0x00001000u, 0x00001000u, 0x00003000u, 0x00003000u, 0x00002000u, 0x00002000u},
    {0xaa55aa55u, 0x12345678u, 0x55aa55aau, 0xaa55aa55u, 0x12345678u, 0x55aa55aau, 0xaa55aa55u, 0x12345678u, 0x55aa55aau, 0xaa55aa55u, 0x12345678u, 0x55aa55aau},
    {0x8badf00du, 0x8badf00du, 0xdeadbeefu, 0xdeadbeefu, 0xcafebabeu, 0xcafebabeu, 0x8badf00du, 0x8badf00du, 0xdeadbeefu, 0xdeadbeefu, 0xcafebabeu, 0xcafebabeu},
    {0x00000005u, 0x00000005u, 0x0000000au, 0x0000000au, 0x00000007u, 0x00000007u, 0x00000005u, 0x00000005u, 0x0000000au, 0x0000000au, 0x00000007u, 0x00000007u},
    {0x00000005u, 0x00000005u, 0x0000000au, 0x0000000au, 0x00000007u, 0x00000007u, 0x00000005u, 0x00000005u, 0x0000000au, 0x0000000au, 0x00000007u, 0x00000007u},
};

constexpr const char* Names[Checked] = {
    "v_min3_i32 clamp",
    "v_min3_u32 clamp",
    "v_max3_i32 clamp",
    "v_max3_u32 clamp",
    "v_med3_i32 clamp",
    "v_med3_u32 clamp",
    "v_min3_i32",
    "v_min3_u32",
    "v_max3_i32",
    "v_max3_u32",
    "v_med3_i32",
    "v_med3_u32",
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
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
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void CheckModifiersReject() {
    constexpr std::uint32_t opcodes[6] = {
        0xd552800au, 0xd553800bu, 0xd555800cu,
        0xd556800du, 0xd558800eu, 0xd559800fu
    };
    for (const auto op : opcodes) {
        bool threwAbs = false;
        try {
            const std::array<std::uint32_t, 2> words{op | (1u << 8u), 0x041a0b04u};
            ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
        } catch (const std::invalid_argument&) {
            threwAbs = true;
        }
        Require(threwAbs, "VOP3 I32 abs modifier must throw");

        bool threwOpSel = false;
        try {
            const std::array<std::uint32_t, 2> words{op | (1u << 11u), 0x041a0b04u};
            ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
        } catch (const std::invalid_argument&) {
            threwOpSel = true;
        }
        Require(threwOpSel, "VOP3 I32 op_sel modifier must throw");

        bool threwNeg = false;
        try {
            const std::array<std::uint32_t, 2> words{op, 0x041a0b04u | (1u << 29u)};
            ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
        } catch (const std::invalid_argument&) {
            threwNeg = true;
        }
        Require(threwNeg, "VOP3 I32 neg modifier must throw");

        bool threwOmod = false;
        try {
            const std::array<std::uint32_t, 2> words{op, 0x041a0b04u | (1u << 27u)};
            ShaderRecompiler::DecodeRdnaInstruction(0u, words, 0u);
        } catch (const std::invalid_argument&) {
            threwOmod = true;
        }
        Require(threwOmod, "VOP3 I32 omod modifier must throw");
    }
}

}

int main() {
    try {
        CheckModifiersReject();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            for (std::uint32_t index = 0; index < Checked; ++index) {
                const std::uint32_t actual = Output[tid * Results + index];
                Require(actual == Expected[tid][index], std::string("VOP3 32-bit integer min/max/med clamp: lane ") + std::to_string(tid) + " " + Names[index] + " is " + Hex(actual) + ", expected " + Hex(Expected[tid][index]));
            }
        }
        std::puts("VOP3 32-bit integer ternary clamp tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
