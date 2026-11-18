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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Targets = 6;
alignas(256) std::array<std::uint32_t, 16> Data{};

alignas(256) constexpr std::array<std::uint32_t, 13> Code{
    0xf51f0200u, 0xfa000000u,
    0xf5230240u, 0xfa000004u,
    0xf5270280u, 0xfa000008u,
    0xf52b02c0u, 0xfa00000cu,
    0xf52f0300u, 0xfa000010u,
    0xf5330340u, 0xfa000014u,
    0xbf810000u,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(14, 0u);
    const auto descriptor = BufferDescriptor(Data.data(), static_cast<std::uint32_t>(Data.size() * 4u));
    std::copy(descriptor.begin(), descriptor.end(), userData.begin());
    constexpr std::array<std::uint32_t, Targets> operands{0x0000ff00u, 0x12345678u, 0x000000f0u, 0x0f0f0f0fu, 3u, 5u};
    std::copy(operands.begin(), operands.end(), userData.begin() + 8);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 10240u, {false, false, false}, false, 1};
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
    Data = {0x0000f0f0u, 0xffffffffu, 0x0000000fu, 0xffff00ffu, 7u, 2u};
    const auto result = Compile(device, Code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

void Check() {
    constexpr std::array<const char*, Targets> names{"umax", "and", "or", "xor", "inc", "dec"};
    constexpr std::array<std::uint32_t, Targets> initial{0x0000f0f0u, 0xffffffffu, 0x0000000fu, 0xffff00ffu, 7u, 2u};
    constexpr std::array<std::uint32_t, Targets> operands{0x0000ff00u, 0x12345678u, 0x000000f0u, 0x0f0f0f0fu, 3u, 5u};
    const std::array<std::uint32_t, Targets> expected{
        std::max(initial[0], operands[0]),
        initial[1] & operands[1],
        initial[2] | operands[2],
        initial[3] ^ operands[3],
        initial[4] >= operands[4] ? 0u : initial[4] + 1u,
        (initial[5] == 0u || initial[5] > operands[5]) ? operands[5] : initial[5] - 1u,
    };
    for (std::uint32_t op = 0; op < Targets; ++op) {
        Require(Data[op] == expected[op], std::string("s_buffer_atomic_") + names[op] + " is " + Hex(Data[op]) + ", expected " + Hex(expected[op]));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        std::puts("s_buffer_atomic tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
