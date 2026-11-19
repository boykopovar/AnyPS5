#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
constexpr std::size_t CounterBytes = 65536;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 12> PixelCode{
    0xbe8803ffu, 0x00000001u, 0xf6080202u, 0xfa000000u, 0xf4840000u, 0x00000000u,
    0xbf8cc07fu, 0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u, 0xbf800000u,
};

alignas(256) constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-0.963125f, 0.943125f, 0.5f, 1.0f}, {-0.943125f, 0.943125f, 0.5f, 1.0f}, {-0.953125f, 0.963125f, 0.5f, 1.0f},
}};

class GuestCounter {
public:
    GuestCounter() {
#ifdef _WIN32
        memory = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, CounterBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        memory = static_cast<std::uint32_t*>(std::aligned_alloc(CounterBytes, CounterBytes));
#endif
        Require(memory != nullptr, "smem atomic pixel helper: cannot allocate the guest counter");
        std::memset(memory, 0, CounterBytes);
        GuestAllocations::Mutation().Add(memory, CounterBytes, true, true, true);
    }

    ~GuestCounter() {
        GuestAllocations::Mutation().Remove(memory);
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        std::free(memory);
#endif
    }

    GuestCounter(const GuestCounter&) = delete;
    GuestCounter& operator=(const GuestCounter&) = delete;

    bool Import(AgcDriver::VulkanDevice& device) const {
        GuestAllocations::Mapped ranges;
        for (const auto& range : GuestAllocations::GuestAllocationsAcquire_nid_postfix()) {
            if (range->address == reinterpret_cast<std::uintptr_t>(memory)) ranges.push_back(range);
        }
        Require(ranges.size() == 1, "smem atomic pixel helper: the guest counter is not registered");
        const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        if (device.ImportGuestMemory(ranges, generation, false)) return true;
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        return device.ImportGuestMemory(ranges, generation, true);
    }

    std::uint32_t* memory = nullptr;
};

std::array<std::uint32_t, 4> VertexDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::array<std::uint32_t, 4> RawDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Draw(AgcDriver::VulkanDevice& device, const GuestCounter& counterMemory, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target) {
    Pixels.fill(std::byte{0});
    counterMemory.memory[0] = 0u;

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = VertexDescriptor(Triangle.data(), 16u, static_cast<std::uint32_t>(Triangle.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = waveSize == 32u;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(8, 0u);
    const auto counter = RawDescriptor(counterMemory.memory, static_cast<std::uint32_t>(CounterBytes));
    std::copy(counter.begin(), counter.end(), pixelUserData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {waveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Check(const GuestCounter& counterMemory, std::uint32_t waveSize) {
    const auto what = "smem atomic pixel helper wave" + std::to_string(waveSize);
    std::uint32_t covered = 0;
    for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
        if (std::to_integer<std::uint8_t>(Pixels[pixel * 4u]) == 255u) ++covered;
    }
    Require(covered == 1u && std::to_integer<std::uint8_t>(Pixels[(1u * Width + 1u) * 4u]) == 255u, what + ": the triangle covered " + std::to_string(covered) + " pixels, expected only pixel (1, 1)");
    Require(counterMemory.memory[0] == 1u, what + ": s_buffer_atomic_add from a wave whose first lane is a helper added " + std::to_string(counterMemory.memory[0]) + " times, expected once");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32u) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        GuestCounter counter;
        if (!counter.Import(*device)) {
            std::puts("skipped, the device does not import guest memory");
            return VulkanTestSkipped;
        }
        for (const auto waveSize : {64u, 32u}) {
            Draw(*device, counter, waveSize, device->Target());
            Check(counter, waveSize);
        }
        std::puts("smem atomic pixel helper tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
