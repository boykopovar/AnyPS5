#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 14> Code{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e1402ffu, 0x5a5a5a5au, 0x7e1602ffu,
    0x5a5a5a5au, 0x7e140b04u, 0x7e160d05u, 0xe0781000u, 0x80010a02u, 0xbf810000u,
};

constexpr std::uint32_t Values[32] = {
    0x00000000u, 0x00000001u, 0x00000002u, 0x00000003u, 0x00000005u, 0x00000008u, 0x0000000fu, 0x00000010u,
    0x00000018u, 0x0000001fu, 0x00000020u, 0x00000021u, 0x0000003fu, 0x00007fffu, 0x00008000u, 0x0000ffffu,
    0x00010000u, 0x007fffffu, 0x00800000u, 0x00ffffffu, 0x01000000u, 0x12345678u, 0x7fffffffu, 0x80000000u,
    0x80000001u, 0xfffffffeu, 0xffffffffu, 0xdeadbeefu, 0x3f800000u, 0x00ff00ffu, 0xffff8000u, 0x0000807fu,
};

constexpr std::array<std::array<std::uint32_t, 2>, 32> Expected[3] = {
    {{
        {{0x00000000u, 0x00000000u}}, {{0x3f800000u, 0x3f800000u}}, {{0x40000000u, 0x40000000u}}, {{0x40400000u, 0x40400000u}},
        {{0x40a00000u, 0x40a00000u}}, {{0x41000000u, 0x41000000u}}, {{0x41700000u, 0x41700000u}}, {{0x41800000u, 0x41800000u}},
        {{0x41c00000u, 0x41c00000u}}, {{0x41f80000u, 0x41f80000u}}, {{0x42000000u, 0x42000000u}}, {{0x42040000u, 0x42040000u}},
        {{0x427c0000u, 0x427c0000u}}, {{0x46fffe00u, 0x46fffe00u}}, {{0x47000000u, 0x47000000u}}, {{0x477fff00u, 0x477fff00u}},
        {{0x47800000u, 0x47800000u}}, {{0x4afffffeu, 0x4afffffeu}}, {{0x4b000000u, 0x4b000000u}}, {{0x4b7fffffu, 0x4b7fffffu}},
        {{0x4b800000u, 0x4b800000u}}, {{0x4d91a2b4u, 0x4d91a2b4u}}, {{0x4f000000u, 0x4f000000u}}, {{0xcf000000u, 0x4f000000u}},
        {{0xceffffffu, 0x4f000001u}}, {{0xc0000000u, 0x4f800000u}}, {{0xbf800000u, 0x4f800000u}}, {{0xce054904u, 0x4f5eadbfu}},
        {{0x4e7e0000u, 0x4e7e0000u}}, {{0x4b7f00ffu, 0x4b7f00ffu}}, {{0xc7000000u, 0x4f7fff80u}}, {{0x47007f00u, 0x47007f00u}},
    }},
    {{
        {{0x00000000u, 0x00000000u}}, {{0x3f800000u, 0x3f800000u}}, {{0x40000000u, 0x40000000u}}, {{0x40400000u, 0x40400000u}},
        {{0x40a00000u, 0x40a00000u}}, {{0x41000000u, 0x41000000u}}, {{0x41700000u, 0x41700000u}}, {{0x41800000u, 0x41800000u}},
        {{0x41c00000u, 0x41c00000u}}, {{0x41f80000u, 0x41f80000u}}, {{0x42000000u, 0x42000000u}}, {{0x42040000u, 0x42040000u}},
        {{0x427c0000u, 0x427c0000u}}, {{0x46fffe00u, 0x46fffe00u}}, {{0x47000000u, 0x47000000u}}, {{0x477fff00u, 0x477fff00u}},
        {{0x47800000u, 0x47800000u}}, {{0x4afffffeu, 0x4afffffeu}}, {{0x4b000000u, 0x4b000000u}}, {{0x4b7fffffu, 0x4b7fffffu}},
        {{0x4b800000u, 0x4b800000u}}, {{0x4d91a2b3u, 0x4d91a2b3u}}, {{0x4effffffu, 0x4effffffu}}, {{0xcf000000u, 0x4f000000u}},
        {{0xcf000000u, 0x4f000000u}}, {{0xc0000000u, 0x4f7fffffu}}, {{0xbf800000u, 0x4f7fffffu}}, {{0xce054905u, 0x4f5eadbeu}},
        {{0x4e7e0000u, 0x4e7e0000u}}, {{0x4b7f00ffu, 0x4b7f00ffu}}, {{0xc7000000u, 0x4f7fff80u}}, {{0x47007f00u, 0x47007f00u}},
    }},
    {{
        {{0x00000000u, 0x00000000u}}, {{0x3f800000u, 0x3f800000u}}, {{0x40000000u, 0x40000000u}}, {{0x40400000u, 0x40400000u}},
        {{0x40a00000u, 0x40a00000u}}, {{0x41000000u, 0x41000000u}}, {{0x41700000u, 0x41700000u}}, {{0x41800000u, 0x41800000u}},
        {{0x41c00000u, 0x41c00000u}}, {{0x41f80000u, 0x41f80000u}}, {{0x42000000u, 0x42000000u}}, {{0x42040000u, 0x42040000u}},
        {{0x427c0000u, 0x427c0000u}}, {{0x46fffe00u, 0x46fffe00u}}, {{0x47000000u, 0x47000000u}}, {{0x477fff00u, 0x477fff00u}},
        {{0x47800000u, 0x47800000u}}, {{0x4afffffeu, 0x4afffffeu}}, {{0x4b000000u, 0x4b000000u}}, {{0x4b7fffffu, 0x4b7fffffu}},
        {{0x4b800000u, 0x4b800000u}}, {{0x4d91a2b3u, 0x4d91a2b3u}}, {{0x4effffffu, 0x4effffffu}}, {{0xcf000000u, 0x4f000000u}},
        {{0xceffffffu, 0x4f000000u}}, {{0xc0000000u, 0x4f7fffffu}}, {{0xbf800000u, 0x4f7fffffu}}, {{0xce054904u, 0x4f5eadbeu}},
        {{0x4e7e0000u, 0x4e7e0000u}}, {{0x4b7f00ffu, 0x4b7f00ffu}}, {{0xc7000000u, 0x4f7fff80u}}, {{0x47007f00u, 0x47007f00u}},
    }},
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t words) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), words * 4u, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t rounding) {
    Input.fill(0u);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Input[lane * Inputs] = Values[lane];
        Input[lane * Inputs + 1] = Values[lane];
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
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
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xf0u | (rounding << 2u) | rounding, true, true, false};
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        for (std::uint32_t column = 0; column < 2u; ++column) {
            const auto expected = Expected[rounding - 1u][lane][column];
            const auto actual = Output[lane * Results + column];
            Require(actual == expected, "integer to f32 rounding: mode " + std::to_string(rounding) + " lane " + std::to_string(lane) + " column " + std::to_string(column) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t rounding = 1u; rounding < 4u; ++rounding) {
            Run(*device, rounding);
        }
        std::puts("integer to f32 rounding tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
