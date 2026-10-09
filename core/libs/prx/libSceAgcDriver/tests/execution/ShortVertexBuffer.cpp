#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t Vertices = 3;
constexpr std::uint32_t BoundsChecked = 3u;
constexpr std::uint32_t Unchecked = 2u;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Vertices * 8u> Output{};
alignas(256) std::array<std::uint32_t, 4> Record{};

alignas(256) constexpr std::array<std::uint32_t, 9> VertexCode{
    0xbe822100, 0x34020a85, 0xe0781000, 0x80010801, 0xe0781010, 0x80010c01, 0xf80008cf, 0x0f0e0d0c, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::array<float, 4>, Vertices> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.25f, 1.0f}, {-1.0f, 3.0f, 0.75f, 1.0f}
}};

alignas(256) constexpr std::array<float, 4> Constant{1.0f, 2.0f, 3.0f, 4.0f};

std::array<std::uint32_t, 4> Descriptor(std::uintptr_t address, std::uint32_t stride, std::uint32_t records, std::uint32_t outOfBounds) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, 0x0104dfacu | (outOfBounds << 28u)};
}

std::array<std::uint32_t, 4> OutputDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Output.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Output)), 0x31016facu};
}

struct Case {
    std::string name;
    std::uintptr_t address;
    std::uint32_t records;
    std::uint32_t outOfBounds;
    bool indirect;
    std::array<float, 4> expected;
};

void Draw(AgcDriver::VulkanDevice& device, const Case& value) {
    Pixels.fill(std::byte{0x40});
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(12, 0u);
    userData[0] = 0x10000000u;
    userData[1] = 0x00000001u;
    const auto output = OutputDescriptor();
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    ShaderRecompiler::ShaderVertexStageInfo info{};
    info.resourcesNum = 2;
    info.resources[0].fields = Descriptor(value.address, 0u, value.records, value.outOfBounds);
    info.resources[1].fields = Descriptor(reinterpret_cast<std::uintptr_t>(Triangle.data()), 16u, Vertices, BoundsChecked);
    info.resourcesDst[0] = {8, 4, 0, 0};
    info.resourcesDst[1] = {12, 4, 1, 0};
    info.fetchAttribReg = 8;
    info.fetchBufferReg = 10;
    info.fetchEmbedded = true;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertexRequest{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, userData, std::nullopt, std::nullopt, info, vertexMemory},
        device.Target(),
        {0, 0, 0, 64}
    };
    vertexRequest.useCache = false;
    const auto vertex = ShaderRecompiler::Recompile(vertexRequest);
    Require(vertex.vertexAttributes.size() == 2 && vertex.vertexAttributes[0].location == 0, "the fetch-shader call did not produce two vertex attributes");
    const auto vertexPush = static_cast<std::uint32_t>(vertex.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        device.Target(),
        {0, 0, vertexPush, 128 - vertexPush}
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::Fragment, &pixelResult, vertexPush}}};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
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
    AgcDriver::Pm4::DrawParameters draw{0, Vertices, 0, 1, 0, false};
    if (value.indirect) {
        Record = {Vertices, 1u, 0u, 0u};
        draw.indirect = AgcDriver::Pm4::DrawParameters::IndirectDraw{reinterpret_cast<std::uintptr_t>(Record.data()), 0x24u, 16u, 16u, 1u, false, 0u, 0x280u, 0x280u, 0x280u, false, 0u};
    }
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Check(const Case& value) {
    for (std::uint32_t vertex = 0; vertex < Vertices; ++vertex) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto shortValue = Output[vertex * 8u + component];
            Require(shortValue == std::bit_cast<std::uint32_t>(value.expected[component]), value.name + ": vertex " + std::to_string(vertex) + " short attribute component " + std::to_string(component) + " is " + std::to_string(shortValue));
            const auto normalValue = Output[vertex * 8u + 4u + component];
            Require(normalValue == std::bit_cast<std::uint32_t>(Triangle[vertex][component]), value.name + ": vertex " + std::to_string(vertex) + " strided attribute component " + std::to_string(component) + " is " + std::to_string(normalValue));
        }
    }
}

void CheckAlias(AgcDriver::VulkanDevice& device) {
    try {
        Draw(device, {"aliasing the render target", reinterpret_cast<std::uintptr_t>(Pixels.data()), 8u, BoundsChecked, false, {}});
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find("aliases the render target") != std::string::npos, std::string("short buffer over the render target: unexpected error: ") + error.what());
        return;
    }
    throw std::runtime_error("a short zero-stride buffer over the render target was read");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto constant = reinterpret_cast<std::uintptr_t>(Constant.data());
        const std::array<Case, 7> cases{{
            {"range checked, two dwords in range", constant, 8u, BoundsChecked, false, {1.0f, 2.0f, 0.0f, 0.0f}},
            {"range checked, partial dword", constant, 6u, BoundsChecked, false, {1.0f, 0.0f, 0.0f, 0.0f}},
            {"range checked, one byte", constant, 1u, BoundsChecked, false, {0.0f, 0.0f, 0.0f, 0.0f}},
            {"no range check, one byte", constant, 1u, Unchecked, false, {1.0f, 2.0f, 3.0f, 4.0f}},
            {"no range check, empty", constant, 0u, Unchecked, false, {0.0f, 0.0f, 0.0f, 0.0f}},
            {"indirect, range checked", constant, 8u, BoundsChecked, true, {1.0f, 2.0f, 0.0f, 0.0f}},
            {"indirect, no range check", constant, 1u, Unchecked, true, {1.0f, 2.0f, 3.0f, 4.0f}},
        }};
        for (const auto& value : cases) {
            Draw(*device, value);
            Check(value);
        }
        CheckAlias(*device);
        std::puts("short vertex buffer tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
