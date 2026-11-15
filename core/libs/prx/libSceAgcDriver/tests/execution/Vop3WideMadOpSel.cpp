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
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 48> Code{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd773000au, 0x041a0b04u, 0xd773200bu,
    0x041a0b04u, 0xd773400cu, 0x041a0b04u, 0xd773780du, 0x041a0b04u, 0xd773200eu, 0x03fe0b04u, 0x12345678u,
    0xd773c00fu, 0x041a0b04u, 0xd773a010u, 0x041e0b04u, 0xd773e011u, 0x041e0b04u, 0xd7750012u, 0x041a0b04u,
    0xd7752013u, 0x041a0b04u, 0xd7754014u, 0x041a0b04u, 0xd7757815u, 0x041a0b04u, 0xd7752016u, 0x03fe0b04u,
    0x12345678u, 0xd775c017u, 0x041a0b04u, 0xd775a018u, 0x041e0b04u, 0xd775e019u, 0x041e0b04u, 0xe0781000u,
    0x80010a02u, 0xe0781010u, 0x80010e02u, 0xe0781020u, 0x80011202u, 0xe0781030u, 0x80011602u, 0xbf810000u,
};

constexpr std::array<const char*, Results> Names{
    "v_mad_u32_u16 v4, v5, v6",
    "v_mad_u32_u16 v4, v5, v6 op_sel:[0,0,1,0]",
    "v_mad_u32_u16 v4, v5, v6 op_sel:[0,0,0,1]",
    "v_mad_u32_u16 v4, v5, v6 op_sel:[1,1,1,1]",
    "v_mad_u32_u16 v4, v5, 0x12345678 op_sel:[0,0,1,0]",
    "v_mad_u32_u16 v4, v5, v6 op_sel:[0,0,0,1] clamp",
    "v_mad_u32_u16 v4, v5, v7 op_sel:[0,0,1,0] clamp",
    "v_mad_u32_u16 v4, v5, v7 op_sel:[0,0,1,1] clamp",
    "v_mad_i32_i16 v4, v5, v6",
    "v_mad_i32_i16 v4, v5, v6 op_sel:[0,0,1,0]",
    "v_mad_i32_i16 v4, v5, v6 op_sel:[0,0,0,1]",
    "v_mad_i32_i16 v4, v5, v6 op_sel:[1,1,1,1]",
    "v_mad_i32_i16 v4, v5, 0x12345678 op_sel:[0,0,1,0]",
    "v_mad_i32_i16 v4, v5, v6 op_sel:[0,0,0,1] clamp",
    "v_mad_i32_i16 v4, v5, v7 op_sel:[0,0,1,0] clamp",
    "v_mad_i32_i16 v4, v5, v7 op_sel:[0,0,1,1] clamp",
};

struct MadVector {
    std::uint32_t operands[Inputs];
    std::uint32_t expected[Results];
};

constexpr std::array<MadVector, 8> Vectors{{
    {{0x00030002u, 0x00050007u, 0x80000001u, 0x7fffffffu}, {0x8000000fu, 0x0000800eu, 0x000f0000u, 0x800f0000u, 0x00001242u, 0x000f0000u, 0x0000800du, 0x800d0000u, 0x8000000fu, 0x0000800eu, 0x000f0000u, 0x800f0000u, 0x00001242u, 0x000f0000u, 0x0000800du, 0x800d0000u}},
    {{0xffff0002u, 0x0003fffeu, 0x7fff8000u, 0xffffffffu}, {0x80017ffcu, 0x00027ffbu, 0x7ffc0000u, 0x7ffc0000u, 0x00021230u, 0x7ffc0000u, 0x0002fffbu, 0xfffb0000u, 0x7fff7ffcu, 0x00007ffbu, 0x7ffc0000u, 0x7ffc0000u, 0x00001230u, 0x7ffc0000u, 0x0000fffbu, 0xfffb0000u}},
    {{0x00020002u, 0x00020002u, 0xfffe0005u, 0x80000000u}, {0xfffe0009u, 0x00010002u, 0x00090000u, 0x00020000u, 0x00001238u, 0x00090000u, 0x00008004u, 0x80040000u, 0xfffe0009u, 0x00010002u, 0x00090000u, 0x00020000u, 0x00001238u, 0x00090000u, 0x00008004u, 0x80040000u}},
    {{0xffffffffu, 0xffffffffu, 0xffffffffu, 0x7fff7fffu}, {0xfffe0000u, 0xffff0000u, 0x00000000u, 0x00000000u, 0xfffe1235u, 0xffff0000u, 0xfffe8000u, 0x80000000u, 0x00000000u, 0x00010000u, 0x00000000u, 0x00000000u, 0x00001235u, 0x00000000u, 0x00008000u, 0x80000000u}},
    {{0x7fff7fffu, 0x7fff7fffu, 0x7fffffffu, 0x80008000u}, {0xbfff0000u, 0x3fff8000u, 0x00000000u, 0x80000000u, 0x3fff1235u, 0x00000000u, 0x3fff8001u, 0x80010000u, 0xbfff0000u, 0x3fff8000u, 0x00000000u, 0x80000000u, 0x3fff1235u, 0xffff0000u, 0x3fff8001u, 0x80010000u}},
    {{0x80008000u, 0x80008000u, 0x00000000u, 0xffff0000u}, {0x40000000u, 0x40000000u, 0x00000000u, 0x00000000u, 0x40001234u, 0x00000000u, 0x4000ffffu, 0xffff0000u, 0x40000000u, 0x40000000u, 0x00000000u, 0x00000000u, 0x40001234u, 0x00000000u, 0x4000ffffu, 0xffff0000u}},
    {{0x1234abcdu, 0xfedc5678u, 0x9abc0defu, 0x0000ffffu}, {0xd4c37407u, 0x3a0800d4u, 0x74070000u, 0xd76c0000u, 0x3a07784cu, 0x74070000u, 0x3a076618u, 0x66180000u, 0x7e4b7407u, 0xe39000d4u, 0x74070000u, 0xd76c0000u, 0xe38f784cu, 0x00000000u, 0xe38f6618u, 0x66180000u}},
    {{0x00000000u, 0x00000000u, 0x0000ffffu, 0x00010000u}, {0x0000ffffu, 0x00000000u, 0xffff0000u, 0x00000000u, 0x00001234u, 0xffff0000u, 0x00000001u, 0x00010000u, 0x0000ffffu, 0x00000000u, 0xffff0000u, 0x00000000u, 0x00001234u, 0xffff0000u, 0x00000001u, 0x00010000u}},
}};

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
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        std::copy(std::begin(Vectors[lane % Vectors.size()].operands), std::end(Vectors[lane % Vectors.size()].operands), Input.begin() + lane * Inputs);
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
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
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const auto& vector = Vectors[lane % Vectors.size()];
        for (std::uint32_t column = 0; column < Results; ++column) {
            const auto value = Output[lane * Results + column];
            Require(value == vector.expected[column], std::string(Names[column]) + " of " + Hex(vector.operands[0]) + ", " + Hex(vector.operands[1]) + ", " + Hex(vector.operands[2]) + ", " +
                                                          Hex(vector.operands[3]) + " is " + Hex(value) + ", expected " + Hex(vector.expected[column]));
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
        std::puts("vop3 wide mad op_sel tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
