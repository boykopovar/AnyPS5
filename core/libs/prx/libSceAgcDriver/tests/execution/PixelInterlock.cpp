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
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t Layers = 64;
constexpr std::size_t GuestBytes = 65536;
constexpr std::uint32_t FaultColumns = 8;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Width * Height> Counters{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> PixelCode{
    0x7e040f00, 0x7e060f01, 0x34060686, 0x4a040702, 0x34040482,
    0xe030d000, 0x80000402, 0xbf8c3f70, 0x4a080881, 0xe0705000, 0x80000402,
    0xbbfd0000, 0xbf900007, 0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 30> FaultPixelCode{
    0x7e040f00, 0x7e060f01, 0x36100487, 0x34060686, 0x4a040702, 0x34040482, 0x7e0a02ff, 0x00008000,
    0x7e0c02ff, 0x00010000, 0x7d841080, 0x02160d05, 0x4a161702, 0xdc319000, 0x0a04000b, 0xe030d000,
    0x80000402, 0xbf8c3f70, 0x4a080881, 0x4a08090a, 0xe0705000, 0x80000402, 0xdce08000, 0x00040402,
    0xbbfd0000, 0xbf900007, 0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

class GuestCounters {
public:
    GuestCounters() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 2 * GuestBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(GuestBytes, 2 * GuestBytes));
#endif
        Require(block != nullptr, "pixel interlock: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, GuestBytes, true, true);
    }

    ~GuestCounters() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestCounters(const GuestCounters&) = delete;
    GuestCounters& operator=(const GuestCounters&) = delete;

    void Clear() { std::memset(block, 0, 2 * GuestBytes); }
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }
    std::uint32_t Word(std::size_t index) const {
        std::uint32_t value = 0;
        std::memcpy(&value, block + index * 4u, sizeof(value));
        return value;
    }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, stride == 0u ? 0x31016facu : 0x01016facu};
}

const std::vector<std::array<float, 4>>& Triangles() {
    static const auto triangles = [] {
        std::vector<std::array<float, 4>> vertices;
        for (std::uint32_t layer = 0; layer < Layers; ++layer) {
            vertices.push_back({-1.0f, -1.0f, 0.5f, 1.0f});
            vertices.push_back({3.0f, -1.0f, 0.5f, 1.0f});
            vertices.push_back({-1.0f, 3.0f, 0.5f, 1.0f});
        }
        return vertices;
    }();
    return triangles;
}

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::span<const std::uint32_t> pixelCode, std::span<const std::uint32_t> pixelUserData) {
    Pixels.fill(std::byte{0});
    Counters.fill(0);
    const auto target = device.Target();
    const auto& triangles = Triangles();

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(triangles.data(), 16u, static_cast<std::uint32_t>(triangles.size()));
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
    pixel.executeOnNoop = true;
    pixel.orderedPixelShader = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {waveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        {0, 0, vertexPush, 128 - vertexPush}
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, vertexPush}
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(triangles.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

std::vector<std::uint32_t> CounterUserData() {
    std::vector<std::uint32_t> userData(4, 0u);
    const auto counters = BufferDescriptor(Counters.data(), 0u, static_cast<std::uint32_t>(sizeof(Counters)));
    std::copy(counters.begin(), counters.end(), userData.begin());
    return userData;
}

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    Draw(device, waveSize, PixelCode, CounterUserData());
}

void DrawFault(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, GuestCounters& guest) {
    const auto what = "pixel interlock wave" + std::to_string(waveSize) + " BDA fault";
    guest.Clear();
    auto pixelUserData = CounterUserData();
    pixelUserData.push_back(static_cast<std::uint32_t>(guest.Address()));
    pixelUserData.push_back(static_cast<std::uint32_t>(guest.Address() >> 32u));
    std::string failure;
    try {
        Draw(device, waveSize, FaultPixelCode, pixelUserData);
    } catch (const std::exception& error) {
        failure = error.what();
    }
    device.WaitIdle();
    Require(failure.find("BDA access failed") != std::string::npos && failure.find("reason=1") != std::string::npos, what + ": the draw reported \"" + failure + "\", not an unmapped read");
    for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
        const auto expected = pixel % FaultColumns == 0 ? 0u : Layers;
        Require(guest.Word(pixel) == expected, what + ": pixel " + std::to_string(pixel) + " counted " + std::to_string(guest.Word(pixel)) + " of " + std::to_string(expected) + " overlapping read-modify-writes");
        Require(guest.Word(GuestBytes / 8u + pixel) == 0u && guest.Word(GuestBytes / 4u + pixel) == 0u, what + ": pixel " + std::to_string(pixel) + " stored into its read range");
    }
}

void Check(std::uint32_t waveSize, std::uint32_t pass) {
    const auto what = "pixel interlock wave" + std::to_string(waveSize) + " pass " + std::to_string(pass);
    for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
        for (std::uint32_t channel = 0; channel < 4; ++channel) {
            Require(Pixels[pixel * 4u + channel] == std::byte{255}, what + ": missing color at pixel " + std::to_string(pixel));
        }
        Require(Counters[pixel] == Layers, what + ": pixel " + std::to_string(pixel) + " counted " + std::to_string(Counters[pixel]) + " of " + std::to_string(Layers) + " overlapping read-modify-writes");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityFragmentShaderPixelInterlockEXT)) {
            std::puts("skipped, the device lacks fragmentShaderPixelInterlock");
            return VulkanTestSkipped;
        }
        GuestCounters guest;
        for (const auto waveSize : {64u, 32u}) {
            for (std::uint32_t pass = 0; pass < 3; ++pass) {
                Draw(*device, waveSize);
                Check(waveSize, pass);
            }
            DrawFault(*device, waveSize, guest);
            Draw(*device, waveSize);
            Check(waveSize, 3);
        }
        std::printf("pixel interlock tests passed: 10 draws of %u overlapping triangles over %u pixels, 2 of them with a killed BDA fault in every %uth column\n", Layers, Width * Height, FaultColumns);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
