#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 16;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t RowCount = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) std::array<std::uint32_t, 12> Code{
    0x34020084u, 0x34060084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0x7e1402ffu, 0x5a5a5a5au, 0xd5d3000au, 0x00000104u,
    0xe0781000u, 0x80010a03u, 0xbf810000u,
};

struct Variant {
    const char* modifiers;
    std::uint32_t clamp;
    std::uint32_t omod;
};

constexpr Variant Variants[] = {
    {"mul:2", 0, 1},
    {"mul:4", 0, 2},
    {"div:2", 0, 3},
    {"clamp", 1, 0},
    {"clamp mul:2", 1, 1},
};

constexpr std::uint32_t Rows[RowCount] = {
    0x00003c00u,
    0x0000bc00u,
    0x0000c100u,
    0x00004200u,
    0x00006400u,
    0x00005640u,
    0x0000d640u,
    0x00007bffu,
    0x0000fbffu,
    0x00007e00u,
    0xbeef3c00u,
    0xbeefc100u,
    0x00007c00u,
    0x0000fc00u,
    0x00007c01u,
    0x00003e00u,
};

constexpr std::uint32_t Expected[RowCount] = {
    0x5a5a0001u, 0x5a5affffu, 0x5a5afffeu, 0x5a5a0003u,
    0x5a5a0400u, 0x5a5a0064u, 0x5a5aff9cu, 0x5a5a7fffu,
    0x5a5a8000u, 0x5a5a0000u, 0x5a5a0001u, 0x5a5afffeu,
    0x5a5a7fffu, 0x5a5a8000u, 0x5a5a0000u, 0x5a5a0001u,
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

void Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::ShaderFloatMode& floatMode, const Variant& variant) {
    Code[7] = 0xd5d3000au | variant.clamp << 15u;
    Code[8] = 0x00000104u | variant.omod << 27u;
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < RowCount; ++tid) Input[tid * Inputs] = Rows[tid];
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
    request.context.floatMode = floatMode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const char* mode, const Variant& variant) {
    for (std::uint32_t tid = 0; tid < RowCount; ++tid) {
        const std::uint32_t actual = Output[tid * Results];
        Require(actual == Expected[tid], std::string("v_cvt_i16_f16 ") + variant.modifiers + " " + mode + ": lane " + std::to_string(tid) +
                                             " of " + Hex(Rows[tid]) + " is " + Hex(actual) + ", expected " + Hex(Expected[tid]));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const Variant& variant : Variants) {
            Run(*device, ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}, variant);
            Check("M1", variant);
            Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}, variant);
            Check("M2", variant);
        }
        std::puts("v_cvt_i16_f16 output modifiers tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
