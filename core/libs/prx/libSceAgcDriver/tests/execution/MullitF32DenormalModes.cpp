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
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads> Output{};

alignas(256) constexpr std::array<std::uint32_t, 11> MullitCode{
    0x34020084u, 0x34040082u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0xd550000au, 0x041a0b04u,
    0xe0701000u, 0x80010a02u, 0xbf810000u,
};

struct Vector {
    std::uint32_t lhs;
    std::uint32_t rhs;
    std::uint32_t limit;
    std::uint32_t expected[8];
};

constexpr Vector Vectors[] = {
    {0x00000000u, 0x00000000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u}},
    {0x00000000u, 0x00800000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u}},
    {0x00000001u, 0x80000001u, 0x00800000u, {0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u}},
    {0x00000001u, 0x3f800000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00000001u, 0x00000001u}},
    {0x00000001u, 0x3f800000u, 0x00800000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u}},
    {0x00000001u, 0xbf800000u, 0x00800000u, {0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80000001u, 0x80000001u}},
    {0x00000001u, 0x7f7fffffu, 0x00800000u, {0x00000000u, 0x00000000u, 0x34ffffffu, 0x34ffffffu, 0x00000000u, 0x00000000u, 0x34ffffffu, 0x34ffffffu}},
    {0x00800000u, 0x00000000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u}},
    {0x00800000u, 0x80000001u, 0x00800000u, {0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u}},
    {0x00800000u, 0x00800000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u}},
    {0x00800000u, 0x3f000000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00400000u, 0x00400000u}},
    {0x00800000u, 0x3f000000u, 0x00800000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00400000u, 0x00400000u, 0x00400000u, 0x00400000u}},
    {0x3f000000u, 0x007fffffu, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x00000000u, 0x00000000u, 0xff7fffffu, 0xff7fffffu, 0x00400000u, 0x00400000u}},
    {0x3f000000u, 0x007fffffu, 0x00800000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00400000u, 0x00400000u}},
    {0x3f000000u, 0x807fffffu, 0x00800000u, {0x00000000u, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80400000u, 0x80400000u}},
    {0x7f800001u, 0x00000001u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x7f800001u, 0x7fc00001u, 0xff7fffffu, 0xff7fffffu, 0x7f800001u, 0x7fc00001u}},
    {0x7f800001u, 0x00000001u, 0x00800000u, {0x00000000u, 0x00000000u, 0x7f800001u, 0x7fc00001u, 0x00000000u, 0x00000000u, 0x7f800001u, 0x7fc00001u}},
    {0x7f800001u, 0x00800000u, 0x00000001u, {0xff7fffffu, 0xff7fffffu, 0x7f800001u, 0x7fc00001u, 0xff7fffffu, 0xff7fffffu, 0x7f800001u, 0x7fc00001u}},
    {0x7f800001u, 0x00800000u, 0x00800000u, {0x7f800001u, 0x7fc00001u, 0x7f800001u, 0x7fc00001u, 0x7f800001u, 0x7fc00001u, 0x7f800001u, 0x7fc00001u}},
    {0x3f800000u, 0x40000000u, 0x3f800000u, {0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u, 0x40000000u}},
    {0x3f800000u, 0xff7fffffu, 0x3f800000u, {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu}},
    {0x3f800000u, 0x3f800000u, 0x80000000u, {0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu, 0xff7fffffu}},
    {0x7f7fffffu, 0x7f7fffffu, 0x3f800000u, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}},
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

void Run(AgcDriver::VulkanDevice& device, std::uint32_t denorm32, bool ieee) {
    Input.fill(0u);
    for (std::uint32_t row = 0; row < std::size(Vectors); ++row) {
        Input[row * Inputs] = Vectors[row].lhs;
        Input[row * Inputs + 1] = Vectors[row].rhs;
        Input[row * Inputs + 2] = Vectors[row].limit;
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(MullitCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{(denorm32 << 4u) | (3u << 6u), true, ieee, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(std::uint32_t denorm32, bool ieee) {
    for (std::uint32_t row = 0; row < std::size(Vectors); ++row) {
        const auto& vector = Vectors[row];
        const auto expected = vector.expected[denorm32 * 2u + (ieee ? 1u : 0u)];
        Require(Output[row] == expected, "v_mullit_f32 denorm32 " + std::to_string(denorm32) + (ieee ? " ieee" : " no ieee") + " inputs " + Hex(vector.lhs) + " " +
                                             Hex(vector.rhs) + " " + Hex(vector.limit) + " is " + Hex(Output[row]) + ", expected " + Hex(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t denorm32 = 0u; denorm32 < 4u; ++denorm32) {
            for (const bool ieee : {false, true}) {
                Run(*device, denorm32, ieee);
                Check(denorm32, ieee);
            }
        }
        std::puts("v_mullit_f32 denormal mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
