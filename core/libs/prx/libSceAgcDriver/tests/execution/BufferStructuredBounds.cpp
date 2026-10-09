#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Stride = 16;
constexpr std::uint32_t Records = 4;
constexpr std::uint32_t Loads = 8;
alignas(256) std::array<std::uint32_t, 64> Input{};
alignas(256) std::array<std::uint32_t, Threads * Loads> Output{};

std::array<std::uint32_t, 4> StructuredDescriptor(const void* data) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (Stride << 16u), Records, 0x01016facu};
}

std::array<std::uint32_t, 4> RawDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t index = 0; index < Input.size(); ++index) Input[index] = 0x1000u + index;
        Output.fill(0xdeadbeefu);
        std::vector<std::uint32_t> code{0x36020087u, 0x34060085u};
        for (std::uint32_t load = 0; load < Loads; ++load) code.insert(code.end(), {0xe0302000u | (load * 4u), 0x80000001u | ((10u + load) << 8)});
        code.insert(code.end(), {0xbf8c0070u, 0xe0781000u, 0x80010a03u, 0xe0781010u, 0x80010e03u, 0xbf810000u});
        std::vector<std::uint32_t> userData(8, 0u);
        const auto input = StructuredDescriptor(Input.data());
        const auto output = RawDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(output.begin(), output.end(), userData.begin() + 4);
        const std::span<const std::uint32_t> words(code);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
        ShaderRecompiler::RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0, {}},
            {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
            device->Target(),
            {0, 0, 0, 128}
        };
        request.useCache = false;
        const auto result = ShaderRecompiler::Recompile(request);
        device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device->WaitIdle();
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            const std::uint32_t element = lane & 7u;
            for (std::uint32_t load = 0; load < Loads; ++load) {
                const std::uint32_t offset = load * 4u;
                const bool inBounds = element < Records && offset < Stride;
                const std::uint32_t expected = inBounds ? Input[(element * Stride + offset) / 4u] : 0u;
                const std::uint32_t actual = Output[lane * Loads + load];
                Require(actual == expected, "buffer structured bounds: lane " + std::to_string(lane) + " index " + std::to_string(element) + " offset " + std::to_string(offset) + " is " + Hex(actual) + ", expected " + Hex(expected));
            }
        }
        std::puts("buffer structured bounds tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
