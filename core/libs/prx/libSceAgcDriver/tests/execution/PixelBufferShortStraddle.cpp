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

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t Pixels = Width * Height;
constexpr std::uint32_t Slot = 8;
constexpr std::uint32_t Records = Pixels * Slot;
constexpr std::array<std::uint8_t, 2> Fills{0x11u, 0xeeu};
constexpr std::uint32_t Pattern = 0xc35au;
constexpr std::uint32_t RawFormat = 0x31016facu;
constexpr std::uint32_t StoreShort = 0x1au;
constexpr std::uint32_t StoreShortD16Hi = 0x1bu;
alignas(256) std::array<std::byte, Pixels * 4> Target{};
alignas(256) std::array<std::uint8_t, Records + 16> Output{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangles{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

std::vector<std::uint32_t> PixelCode(std::uint32_t opcode) {
    std::vector<std::uint32_t> code{0x7e040f00, 0x7e060f01, 0xd5430006, 0x0409ff03, Width};
    const auto vop2 = [&](std::uint32_t op, std::uint32_t destination, std::uint32_t source, std::uint32_t vector) {
        code.push_back(op | (destination << 17u) | (vector << 9u) | source);
    };
    vop2(0x36000000, 7, 0x83, 6);
    vop2(0x34000000, 8, 0x83, 6);
    vop2(0x4a000000, 8, 0x107, 8);
    vop2(0x16000000, 9, 0xff, 6);
    code.push_back(0x0101u);
    vop2(0x3a000000, 9, 0xff, 9);
    code.push_back(Pattern);
    if (opcode == StoreShortD16Hi) vop2(0x34000000, 9, 0x90, 9);
    code.push_back(0xe0001000u | (opcode << 18u));
    code.push_back(0x80010908u);
    code.insert(code.end(), {0xbf8c3f70, 0x7e1602f2, 0xf800180f, 0x0b0b0b0b, 0xbf810000});
    return code;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t records, std::uint32_t format) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, format};
}

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::uint32_t opcode, std::uint32_t misalignment, std::uint8_t fill) {
    const auto pixelCode = PixelCode(opcode);
    Target.fill(std::byte{0});
    Output.fill(fill);
    const auto target = device.Target();

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(Triangles.data(), 16u, static_cast<std::uint32_t>(Triangles.size()), 0x01016facu);
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
    const auto output = BufferDescriptor(Output.data() + misalignment, 0u, Records, RawFormat);
    std::copy(output.begin(), output.end(), pixelUserData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(std::span(pixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
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
    state.color = {reinterpret_cast<std::uintptr_t>(Target.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Target.size(), 0xe4u};
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangles.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

std::uint32_t Check(std::uint32_t waveSize, std::uint32_t opcode, std::uint32_t misalignment, std::uint8_t fill) {
    std::uint32_t failures = 0;
    std::array<std::uint32_t, 4> byOffset{};
    for (std::uint32_t index = 0; index < Output.size(); ++index) {
        auto expected = fill;
        if (index >= misalignment && index < misalignment + Records) {
            const auto relative = index - misalignment;
            const auto pixel = relative / Slot;
            const auto start = pixel * Slot + (pixel & 3u);
            const auto value = (pixel * 0x0101u) ^ Pattern;
            if (relative == start) expected = static_cast<std::uint8_t>(value);
            if (relative == start + 1u) expected = static_cast<std::uint8_t>(value >> 8u);
        }
        if (Output[index] == expected) continue;
        const auto pixel = index >= misalignment ? (index - misalignment) / Slot : 0u;
        ++byOffset[pixel & 3u];
        if (++failures > 4u) continue;
        std::fprintf(stderr, "wave=%u opcode=0x%02x misalignment=%u fill=%02x pixel=%u offset=%u byte=%u actual=%02x expected=%02x\n", waveSize, opcode, misalignment, fill, pixel, pixel & 3u, index, Output[index], expected);
    }
    if (failures != 0u) std::fprintf(stderr, "wave=%u opcode=0x%02x misalignment=%u fill=%02x wrong bytes by offset 0/1/2/3: %u/%u/%u/%u\n", waveSize, opcode, misalignment, fill, byOffset[0], byOffset[1], byOffset[2], byOffset[3]);
    return failures;
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
        std::uint32_t failures = 0;
        for (const auto waveSize : {64u, 32u}) {
            for (const auto opcode : {StoreShort, StoreShortD16Hi}) {
                for (std::uint32_t misalignment = 0; misalignment < 4u; ++misalignment) {
                    for (const auto fill : Fills) {
                        Draw(*device, waveSize, opcode, misalignment, fill);
                        failures += Check(waveSize, opcode, misalignment, fill);
                    }
                }
            }
        }
        Require(failures == 0u, "pixel shader 16-bit buffer stores lost bytes");
        std::puts("pixel shader 16-bit buffer stores write both bytes at every offset in a dword, wave64/wave32");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
