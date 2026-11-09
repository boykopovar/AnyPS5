#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
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
constexpr std::uint32_t IdentitySelect = 0xfacu;
constexpr std::uint32_t ReverseSelect = 0x977u;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, 12> Output{};

alignas(256) constexpr std::array<std::uint32_t, 7> VertexCode{
    0xbe822100, 0x34020a84, 0xe0781000, 0x80010801, 0xf80008cf, 0x0b0a0908, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.25f, 1.0f}, {-1.0f, 3.0f, 0.75f, 1.0f}
}};

alignas(256) constexpr std::array<std::uint16_t, 6> RestartIndices{0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};

std::array<std::uint32_t, 4> VertexBufferDescriptor(std::uint32_t select, std::uint32_t records) {
    const auto address = reinterpret_cast<std::uintptr_t>(Triangle.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), records, 0x0104d000u | select};
}

std::array<std::uint32_t, 4> OutputDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Output.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Output)), 0x31016facu};
}

ShaderRecompiler::RecompileRequest VertexRequest(const AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> userData, const ShaderRecompiler::ShaderVertexStageInfo& info, std::span<const ShaderRecompiler::MemoryRegion> memory) {
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, userData, std::nullopt, std::nullopt, info, memory},
        device.Target(),
        {0, 0, 0, 64}
    };
    request.useCache = false;
    return request;
}

ShaderRecompiler::ShaderVertexStageInfo VertexInfo(std::uint32_t select, std::int32_t registers, std::uint32_t records = static_cast<std::uint32_t>(Triangle.size())) {
    ShaderRecompiler::ShaderVertexStageInfo info{};
    info.resourcesNum = 1;
    info.resources[0].fields = VertexBufferDescriptor(select, records);
    info.resourcesDst[0] = {8, registers, 0, 0};
    info.fetchAttribReg = 8;
    info.fetchBufferReg = 10;
    info.fetchEmbedded = true;
    return info;
}

std::vector<std::uint32_t> UserData() {
    std::vector<std::uint32_t> userData(12, 0u);
    userData[0] = 0x10000000u;
    userData[1] = 0x00000001u;
    const auto output = OutputDescriptor();
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    return userData;
}

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t select, bool onlyRestart = false) {
    Pixels.fill(std::byte{0x40});
    Output.fill(0xdeadbeefu);
    const auto userData = UserData();
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    const auto vertexResult = ShaderRecompiler::Recompile(VertexRequest(device, userData, VertexInfo(select, 4, onlyRestart ? 0u : static_cast<std::uint32_t>(Triangle.size())), vertexMemory));
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

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
        PixelPushLayout(vertexPush, device.Target())
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, device.Target())}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = onlyRestart ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.primitiveRestart = onlyRestart;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const auto draw = onlyRestart ? AgcDriver::Pm4::DrawParameters{reinterpret_cast<std::uintptr_t>(RestartIndices.data()), static_cast<std::uint32_t>(RestartIndices.size()), 2, 1, 0, true} : AgcDriver::Pm4::DrawParameters{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Check(std::uint32_t select, const std::string& what) {
    for (std::uint32_t vertex = 0; vertex < Triangle.size(); ++vertex) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto source = select == ReverseSelect ? 3u - component : component;
            const auto expected = std::bit_cast<std::uint32_t>(Triangle[vertex][source]);
            const auto actual = Output[vertex * 4u + component];
            Require(actual == expected, what + ": vertex " + std::to_string(vertex) + " v" + std::to_string(8u + component) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
        }
    }
}

void CheckRegisterLimit(AgcDriver::VulkanDevice& device) {
    const auto userData = UserData();
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    try {
        static_cast<void>(ShaderRecompiler::Recompile(VertexRequest(device, userData, VertexInfo(IdentitySelect, 5), memory)));
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find("the inline model supports one to four") != std::string::npos, std::string("fetch-shader register limit: unexpected error: ") + error.what());
        return;
    }
    throw std::runtime_error("fetch-shader register limit: five destination registers compiled");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Draw(*device, IdentitySelect);
        Check(IdentitySelect, "fetch-shader call, identity dst_sel");
        Draw(*device, ReverseSelect);
        Check(ReverseSelect, "fetch-shader call, reversed dst_sel");
        CheckRegisterLimit(*device);
        Draw(*device, IdentitySelect, true);
        Require(std::all_of(Output.begin(), Output.end(), [](std::uint32_t word) { return word == 0xdeadbeefu; }) && std::all_of(Pixels.begin(), Pixels.end(), [](std::byte value) { return value == std::byte{0x40}; }), "an indexed draw of only restart indices ran its vertices or wrote pixels");
        std::puts("scalar swappc fetch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
