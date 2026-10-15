#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "PoisonedBufferPool.hpp"
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

constexpr std::uint32_t WaveSize = 64;
constexpr std::uint32_t Width = 64;
constexpr std::uint32_t DataWords = 16;
constexpr std::uint32_t OutputWords = 8;
constexpr std::uint32_t Fill = 0xdeadbeefu;
alignas(256) std::array<std::byte, Width * 4u> Pixels{};
alignas(256) std::array<std::uint32_t, DataWords> Data{};
alignas(256) std::array<std::uint32_t, OutputWords> Output{};
alignas(256) std::array<std::array<float, 4>, 3> Vertices{{{-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000u, 0x80000005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 33> PixelCode{
    0xe0300000u, 0x80000600u,
    0xe0300004u, 0x80000200u,
    0xe0200004u, 0x80000300u,
    0xe0280004u, 0x80000400u,
    0xe0000004u, 0x80000900u,
    0xbf8c3f70u,
    0xe0700000u, 0x80010600u,
    0xe0700004u, 0x80010200u,
    0xe0700008u, 0x80010300u,
    0xe070000cu, 0x80010400u,
    0xe0700010u, 0x80010900u,
    0x7e0e02ffu, 0x77777777u,
    0xe0700004u, 0x80000700u,
    0xe0680004u, 0x80000700u,
    0x7e1002f2u,
    0xf800180fu, 0x08080808u,
    0xbf810000u,
};

struct Case {
    std::uint32_t oobMode;
    std::uint32_t stride;
    std::uint32_t records;
};

struct Memory {
    std::array<std::uint32_t, OutputWords> output;
    std::array<std::uint32_t, DataWords> data;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t records, std::uint32_t oobMode) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, 0x01016facu | (oobMode << 28u)};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::string Describe(const Case& test, bool registration, bool robust) {
    return "buffer direct bounds graphics oob " + std::to_string(test.oobMode) + " stride " + std::to_string(test.stride) + " records " + std::to_string(test.records) + (registration ? " registration module" : " specialized module") + (robust ? " robustBufferAccess2" : "");
}

std::uint32_t Pattern(std::uint32_t word) {
    return 0x10203040u + word * 0x01010101u;
}

void InitialData(const Case& test) {
    for (std::uint32_t word = 0; word < DataWords; ++word) Data[word] = Pattern(word);
    for (auto byte = test.stride == 0u ? test.records : test.stride * test.records; byte % 4u != 0u; ++byte) Data[byte / 4u] &= ~(0xffu << (byte % 4u * 8u));
}

Memory Draw(AgcDriver::VulkanDevice& device, const Case& test, bool registration, bool robust) {
    InitialData(test);
    Output.fill(Fill);
    Pixels.fill(std::byte{0});
    auto target = device.Target();
    target.robustBufferAccess2 = robust;
    if (!registration) target.registrationWordLimit = 1u;

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(Vertices.data(), 16u, static_cast<std::uint32_t>(Vertices.size()), 0u);
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {WaveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData;
    for (const auto& descriptor : {BufferDescriptor(Data.data(), test.stride, test.records, test.oobMode), BufferDescriptor(Output.data(), 0u, OutputWords * 4u, 3u)}) {
        pixelUserData.insert(pixelUserData.end(), descriptor.begin(), descriptor.end());
    }
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {WaveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const auto run = Describe(test, registration, robust);
    Require(pixelResult.registrationModule == registration, run + ": the pixel shader was not built in the requested module form");
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, WaveSize, WaveSize, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, 1u}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, 1u};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0.0f, 1.0f, static_cast<float>(Width), -1.0f, 0.0f, 1.0f};
    state.scissor = {{0, 0}, {1u, 1u}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15u;
    state.blends = {state.blend};
    const AgcDriver::Pm4::DrawParameters draw{0u, static_cast<std::uint32_t>(Vertices.size()), 0u, 1u, 0u, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    Require(std::ranges::all_of(std::span(Pixels).first(4), [](std::byte value) { return value == std::byte{255}; }), run + ": the pixel shader did not cover the scissored pixel");
    return {Output, Data};
}

void CheckSame(const Case& test, const std::string& run, const Memory& memory, const Memory& reference) {
    for (std::uint32_t word = 0; word < OutputWords; ++word) {
        Require(memory.output[word] == reference.output[word], run + ": result dword " + std::to_string(word) + " is " + Hex(memory.output[word]) + ", the specialized checked module gives " + Hex(reference.output[word]));
    }
    for (std::uint32_t word = 0; word < DataWords; ++word) {
        Require(memory.data[word] == reference.data[word], run + ": buffer dword " + std::to_string(word) + " is " + Hex(memory.data[word]) + ", the specialized checked module gives " + Hex(reference.data[word]));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        if (!target.registrationModules && !target.robustBufferAccess2) {
            std::puts("skipped, the device builds neither registration modules nor robustBufferAccess2 modules");
            return VulkanTestSkipped;
        }
        const auto referenceDevice = target.robustBufferAccess2 ? OpenVulkanTestDeviceWithoutRobustBufferAccess2() : nullptr;
        if (target.robustBufferAccess2) {
            if (!referenceDevice) return VulkanTestSkipped;
            Require(!referenceDevice->Target().robustBufferAccess2, "buffer direct bounds graphics: the reference device enables robustBufferAccess2");
        }
        for (const auto& test : {Case{0u, 5u, 1u}, Case{1u, 6u, 1u}, Case{2u, 5u, 1u}, Case{3u, 0u, 5u}, Case{3u, 0u, 6u}}) {
            auto& referenceRunner = referenceDevice ? *referenceDevice : *device;
            PoisonedBufferPool::PoisonPool(referenceRunner);
            const auto reference = Draw(referenceRunner, test, false, false);
            Require(reference.output[0] == Pattern(0), Describe(test, false, false) + ": the in-bounds load is " + Hex(reference.output[0]) + ", expected " + Hex(Pattern(0)));
            for (const bool registration : {false, true}) {
                if (registration && !target.registrationModules) continue;
                for (const bool robust : {false, true}) {
                    if ((robust && !target.robustBufferAccess2) || (!registration && !robust)) continue;
                    PoisonedBufferPool::PoisonPool(*device);
                    CheckSame(test, Describe(test, registration, robust), Draw(*device, test, registration, robust), reference);
                }
            }
        }
        std::puts("buffer direct bounds graphics tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
