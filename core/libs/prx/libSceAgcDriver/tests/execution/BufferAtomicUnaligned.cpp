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
constexpr std::uint32_t Sentinel = 0x5eed0001u;
constexpr std::size_t OffsetWord = 2;
alignas(256) std::array<std::uint32_t, Threads + 1> Buffer{};
alignas(256) std::array<std::uint32_t, Threads> Output{};

alignas(256) auto Code = std::to_array<std::uint32_t>({
    0x34020082,
    0x7e040281,
    0xe0c85000, 0x80000201,
    0xbf8c3f70,
    0xe0701000, 0x80010201,
    0xbf810000,
});

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t offset) {
    for (std::uint32_t lane = 0; lane < Threads; ++lane) Buffer[lane] = lane * 100u;
    Buffer[Threads] = Sentinel;
    Output.fill(0xdeadbeefu);
    Code[OffsetWord] = 0xe0c85000u | offset;
    std::vector<std::uint32_t> userData(8, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(buffer.begin(), buffer.end(), userData.begin());
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

void Check(std::uint32_t offset) {
    const auto what = "buffer_atomic_add at byte offset " + std::to_string(offset) + ": lane ";
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Require(Output[lane] == lane * 100u, what + std::to_string(lane) + " returned " + std::to_string(Output[lane]) + ", expected " + std::to_string(lane * 100u));
        Require(Buffer[lane] == lane * 100u + 1u, what + std::to_string(lane) + " left dword " + std::to_string(Buffer[lane]) + ", expected " + std::to_string(lane * 100u + 1u));
    }
    Require(Buffer[Threads] == Sentinel, what + "the dword after the last lane changed");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityInt64)) {
            std::puts("skipped, the device has no shaderInt64");
            return VulkanTestSkipped;
        }
        for (std::uint32_t offset = 0; offset < 4u; ++offset) {
            Run(*device, offset);
            Check(offset);
        }
        std::puts("buffer atomic unaligned tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
