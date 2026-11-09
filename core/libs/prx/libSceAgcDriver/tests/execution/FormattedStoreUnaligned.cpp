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
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t LaneBytes = 64;
constexpr std::uint32_t Fill = 0xaaaaaaaau;

struct Store {
    std::uint32_t format;
    std::uint32_t width;
    std::uint32_t channels;
    std::uint32_t components;
    std::uint32_t offset;
};

constexpr std::array<Store, 8> Stores{{
    {5, 1, 1, 1, 1},
    {11, 2, 1, 1, 3},
    {60, 1, 4, 4, 6},
    {60, 1, 4, 3, 14},
    {11, 2, 1, 1, 19},
    {27, 2, 2, 2, 26},
    {69, 2, 4, 4, 33},
    {18, 1, 2, 2, 45},
}};

alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint8_t, Threads * LaneBytes> Output{};

std::vector<std::uint32_t> Program() {
    std::vector<std::uint32_t> code{0x34020084, 0x34060086};
    for (std::uint32_t component = 0; component < Inputs; ++component) code.insert(code.end(), {0xe0301000u | (component * 4u), 0x80000401u | (component << 8u)});
    code.push_back(0xbf8c3f70);
    code.insert(code.end(), {0x7e3c02ff, Fill});
    for (std::uint32_t offset = 0; offset < LaneBytes; offset += 4) code.insert(code.end(), {0xe0701000u | offset, 0x80011e03});
    code.push_back(0xbf8c3f70);
    for (const auto& store : Stores) {
        const std::uint32_t op = 3u + store.components;
        code.insert(code.end(), {0xe8001000u | (store.format << 19u) | (op << 16u) | store.offset, 0x80010403});
    }
    code.push_back(0xbf810000);
    return code;
}

std::uint32_t Value(std::uint32_t tid, std::uint32_t component) {
    const auto mixed = (tid + 1u) * 2654435761u + component * 40503u;
    return tid % 5u == 3u ? mixed | 0x10000u : mixed >> 3u & 0xffffu;
}

std::array<std::uint8_t, LaneBytes> Expected(std::uint32_t tid) {
    std::array<std::uint8_t, LaneBytes> bytes{};
    bytes.fill(static_cast<std::uint8_t>(Fill & 0xffu));
    for (const auto& store : Stores) {
        const std::uint32_t width = store.width;
        const std::uint32_t limit = width == 1u ? 0xffu : 0xffffu;
        for (std::uint32_t component = 0; component < store.channels; ++component) {
            const auto value = component < store.components ? std::min(Value(tid, component), limit) : 0u;
            for (std::uint32_t byte = 0; byte < width; ++byte) bytes[store.offset + component * width + byte] = static_cast<std::uint8_t>(value >> (8u * byte));
        }
    }
    return bytes;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t component = 0; component < Inputs; ++component) Input[tid * Inputs + component] = Value(tid, component);
    }
    Output.fill(0xdeu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    static const auto code = Program();
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(words.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(words.data()), words, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(words.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = Expected(tid);
        for (std::uint32_t byte = 0; byte < LaneBytes; ++byte) {
            const auto actual = Output[tid * LaneBytes + byte];
            Require(actual == expected[byte], "formatted store unaligned: lane " + std::to_string(tid) + " byte " + std::to_string(byte) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[byte]));
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
        std::puts("formatted store unaligned tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
