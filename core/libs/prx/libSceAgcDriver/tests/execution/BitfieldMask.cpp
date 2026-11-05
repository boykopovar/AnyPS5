#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32u;
alignas(256) std::array<std::uint32_t, Threads * 2u> Input{};
alignas(256) std::array<std::uint32_t, Threads * 4u> Output{};
alignas(256) constexpr std::array<std::uint32_t, 31> Code{
    0x34020083u, 0x34040084u, 0xe0341000u, 0x80000401u, 0xbf8c3f70u, 0xd7630008u, 0x00020b04u,
    0xbe8c0380u, 0xbe8d0380u, 0xf4240200u, 0x18000000u, 0xbf8cc07fu, 0xbf060d0du, 0x920a0908u,
    0x850b8081u, 0xd7610009u, 0x00001a0au, 0xd761000au, 0x00001a0bu, 0xbf070d0du, 0x920a0908u,
    0x850b8081u, 0xd761000bu, 0x00001a0bu, 0x800c880cu, 0x800d810du, 0xbf0aa00du, 0xbf85ffedu,
    0xe0781000u, 0x80010802u, 0xbf810000u
};

std::array<std::uint32_t, 4> bufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::uint32_t expectedMask(std::uint32_t width, std::uint32_t offset) {
    std::uint32_t mask = 0u;
    for (std::uint32_t bit = offset; bit < 32u && bit - offset < width; ++bit) mask |= 1u << bit;
    return mask;
}

void run(AgcDriver::VulkanDevice& device) {
    std::array<std::uint32_t, 8> userData{};
    const auto input = bufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = bufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0u, {}},
        {32u, 0u, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0u, 0u, 0u, 128u}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    for (const std::uint32_t upperBits : {0u, 32u, 0x80000000u, 0xffffffe0u}) {
        for (std::uint32_t width = 0u; width < 32u; ++width) {
            for (std::uint32_t offset = 0u; offset < Threads; ++offset) {
                Input[offset * 2u] = width | upperBits;
                Input[offset * 2u + 1u] = offset | upperBits;
            }
            Output.fill(0xdeadbeefu);
            device.Dispatch(result, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(code.data()));
            device.WaitIdle();
            for (std::uint32_t offset = 0u; offset < Threads; ++offset) {
                const auto expected = expectedMask(width, offset);
                const auto description = "bitfield mask width=" + std::to_string(width | upperBits) +
                    " offset=" + std::to_string(offset | upperBits);
                Require(Output[offset * 4u] == expected, description + " vector result " +
                    std::to_string(Output[offset * 4u]) + ", expected " + std::to_string(expected));
                Require(Output[offset * 4u + 1u] == expected, description + " scalar result " +
                    std::to_string(Output[offset * 4u + 1u]) + ", expected " + std::to_string(expected));
                Require(Output[offset * 4u + 2u] == 1u, description + " changed SCC=1");
                Require(Output[offset * 4u + 3u] == 0u, description + " changed SCC=0");
            }
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < Threads) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        run(*device);
        std::puts("4096 scalar/vector bitfield mask execution cases passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
