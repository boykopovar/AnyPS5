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
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t OpCount = 4;
constexpr std::uint32_t ModeCount = 7;

struct Case {
    std::uint32_t op;
    std::uint32_t omod;
    std::uint32_t input;
    std::array<std::uint32_t, ModeCount> expected;
};

constexpr std::array<Case, 50> Cases{{
    {0u, 1u, 0x7f7fffffu, {0x7f800000u, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu}},
    {0u, 1u, 0xff7fffffu, {0xff800000u, 0xff7fffffu, 0xff800000u, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu}},
    {0u, 1u, 0x7f000000u, {0x7f800000u, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu, 0x7f000000u, 0x7f000000u, 0x7f000000u}},
    {0u, 1u, 0xff000000u, {0xff800000u, 0xff7fffffu, 0xff800000u, 0xff7fffffu, 0xff000000u, 0xff000000u, 0xff000000u}},
    {0u, 1u, 0x7e800000u, {0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7e800000u, 0x7e800000u, 0x7e800000u}},
    {0u, 1u, 0xfe800000u, {0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u, 0xfe800000u, 0xfe800000u, 0xfe800000u}},
    {0u, 1u, 0x3f800000u, {0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x3f800000u, 0x3f800000u, 0x3f800000u}},
    {0u, 1u, 0xbf800000u, {0xc0000000u, 0xc0000000u, 0xc0000000u, 0xc0000000u, 0xbf800000u, 0xbf800000u, 0xbf800000u}},
    {0u, 2u, 0x7f7fffffu, {0x7f800000u, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu}},
    {0u, 2u, 0xff7fffffu, {0xff800000u, 0xff7fffffu, 0xff800000u, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu}},
    {0u, 2u, 0x7f000000u, {0x7f800000u, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu, 0x7f000000u, 0x7f000000u, 0x7f000000u}},
    {0u, 2u, 0xff000000u, {0xff800000u, 0xff7fffffu, 0xff800000u, 0xff7fffffu, 0xff000000u, 0xff000000u, 0xff000000u}},
    {0u, 2u, 0x7e800000u, {0x7f800000u, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu, 0x7e800000u, 0x7e800000u, 0x7e800000u}},
    {0u, 2u, 0xfe800000u, {0xff800000u, 0xff7fffffu, 0xff800000u, 0xff7fffffu, 0xfe800000u, 0xfe800000u, 0xfe800000u}},
    {0u, 2u, 0x3f800000u, {0x40800000u, 0x40800000u, 0x40800000u, 0x40800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u}},
    {0u, 2u, 0xbf800000u, {0xc0800000u, 0xc0800000u, 0xc0800000u, 0xc0800000u, 0xbf800000u, 0xbf800000u, 0xbf800000u}},
    {1u, 1u, 0x00800000u, {0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u}},
    {1u, 1u, 0x80800000u, {0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u}},
    {1u, 1u, 0x01000000u, {0x7e800000u, 0x7e800000u, 0x7e800000u, 0x7e800000u, 0x7e800000u, 0x7e800000u, 0x7e800000u}},
    {1u, 1u, 0x81000000u, {0xfe800000u, 0xfe800000u, 0xfe800000u, 0xfe800000u, 0xfe800000u, 0xfe800000u, 0xfe800000u}},
    {1u, 1u, 0x3f800000u, {0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u}},
    {1u, 1u, 0xbf800000u, {0xc0000000u, 0xc0000000u, 0xc0000000u, 0xc0000000u, 0xc0000000u, 0xc0000000u, 0xc0000000u}},
    {1u, 2u, 0x00800000u, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}},
    {1u, 2u, 0x80800000u, {0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u}},
    {1u, 2u, 0x007fffffu, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}},
    {1u, 2u, 0x807fffffu, {0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u}},
    {1u, 2u, 0x01000000u, {0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u}},
    {1u, 2u, 0x81000000u, {0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u}},
    {1u, 2u, 0x3f800000u, {0x40800000u, 0x40800000u, 0x40800000u, 0x40800000u, 0x40800000u, 0x40800000u, 0x40800000u}},
    {1u, 2u, 0xbf800000u, {0xc0800000u, 0xc0800000u, 0xc0800000u, 0xc0800000u, 0xc0800000u, 0xc0800000u, 0xc0800000u}},
    {2u, 1u, 0x00800000u, {0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u}},
    {2u, 1u, 0x80800000u, {0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u}},
    {2u, 1u, 0x01000000u, {0x7e800000u, 0x7e800000u, 0x7e800000u, 0x7e800000u, 0x7e800000u, 0x7e800000u, 0x7e800000u}},
    {2u, 1u, 0x81000000u, {0xfe800000u, 0xfe800000u, 0xfe800000u, 0xfe800000u, 0xfe800000u, 0xfe800000u, 0xfe800000u}},
    {2u, 1u, 0x3f800000u, {0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u}},
    {2u, 1u, 0xbf800000u, {0xc0000000u, 0xc0000000u, 0xc0000000u, 0xc0000000u, 0xc0000000u, 0xc0000000u, 0xc0000000u}},
    {2u, 2u, 0x00800000u, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}},
    {2u, 2u, 0x80800000u, {0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u}},
    {2u, 2u, 0x007fffffu, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}},
    {2u, 2u, 0x807fffffu, {0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u}},
    {2u, 2u, 0x01000000u, {0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u, 0x7f000000u}},
    {2u, 2u, 0x81000000u, {0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u, 0xff000000u}},
    {2u, 2u, 0x3f800000u, {0x40800000u, 0x40800000u, 0x40800000u, 0x40800000u, 0x40800000u, 0x40800000u, 0x40800000u}},
    {2u, 2u, 0xbf800000u, {0xc0800000u, 0xc0800000u, 0xc0800000u, 0xc0800000u, 0xc0800000u, 0xc0800000u, 0xc0800000u}},
    {3u, 1u, 0x42ff0000u, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}},
    {3u, 1u, 0x7f800000u, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}},
    {3u, 2u, 0x42ff0000u, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}},
    {3u, 2u, 0x42fe0000u, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}},
    {3u, 2u, 0x42fd0000u, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}},
    {3u, 2u, 0x7f800000u, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}},
}};

constexpr std::uint32_t Opcodes[OpCount] = {0xd510000au, 0xd5aa000au, 0xd5ab000au, 0xd5a5000au};
constexpr std::uint32_t Sources[OpCount] = {0x00020b04u, 0x00000104u, 0x00000104u, 0x00000104u};
constexpr const char* OpNames[OpCount] = {"v_max_f32", "v_rcp_f32", "v_rcp_iflag_f32", "v_exp_f32"};
constexpr std::uint32_t FloatModes[ModeCount] = {0xc0u, 0xc1u, 0xc2u, 0xc3u, 0xf1u, 0xf2u, 0xf3u};

alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) std::array<std::uint32_t, 11> Code{
    0x34020084u, 0x34060084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0xbf800000u, 0xd503000au, 0x00020b04u,
    0xe0781000u, 0x80010a03u, 0xbf810000u,
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

void Run(AgcDriver::VulkanDevice& device, std::uint32_t op, std::uint32_t omod, std::uint32_t declared, std::uint32_t mode, bool runtimeRounding) {
    Code[5] = runtimeRounding ? 0xbfa40000u | (FloatModes[mode] & 3u) : 0xbf800000u;
    Code[6] = Opcodes[op];
    Code[7] = Sources[op] | omod << 27u;
    Input.fill(0u);
    std::uint32_t rows = 0;
    for (const auto& entry : Cases) {
        if (entry.op != op || entry.omod != omod) continue;
        Input[rows * Inputs] = entry.input;
        Input[rows * Inputs + 1u] = entry.input;
        ++rows;
    }
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
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{declared, true, false, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    std::uint32_t row = 0;
    for (const auto& entry : Cases) {
        if (entry.op != op || entry.omod != omod) continue;
        const std::uint32_t actual = Output[row * Results];
        const std::uint32_t expected = entry.expected[mode];
        Require(actual == expected, std::string(OpNames[op]) + (omod == 1u ? " mul:2" : " mul:4") + " of " + Hex(entry.input) + " with FLOAT_MODE " + Hex(FloatModes[mode]) +
                                        (runtimeRounding ? " set by s_round_mode" : "") + " is " + Hex(actual) + ", expected the RDNA 2 value " + Hex(expected));
        ++row;
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t op = 0; op < OpCount; ++op) {
            for (std::uint32_t omod = 1; omod < 3u; ++omod) {
                for (std::uint32_t mode = 0; mode < ModeCount; ++mode) Run(*device, op, omod, FloatModes[mode], mode, false);
                for (std::uint32_t mode = 1; mode < 4u; ++mode) Run(*device, op, omod, FloatModes[0], mode, true);
            }
        }
        std::puts("f32 transcendental output modifier overflow tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
