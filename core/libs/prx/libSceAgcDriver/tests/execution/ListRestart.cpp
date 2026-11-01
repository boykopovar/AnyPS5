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
constexpr std::uint32_t IdentitySelect = 0xfacu;
constexpr std::uint32_t NoLocation = 0x280u;
constexpr std::byte Background{0x40};
constexpr std::byte Covered{0xff};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, 24> Output{};

alignas(256) constexpr std::array<std::uint32_t, 7> VertexCode{
    0xbe822100, 0x34020a84, 0xe0781000, 0x80010801, 0xf80008cf, 0x0b0a0908, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::array<float, 4>, 6> Vertices{{
    {-0.75f, -0.75f, 0.5f, 1.0f}, {-0.25f, -0.75f, 0.5f, 1.0f}, {-0.5f, 0.75f, 0.5f, 1.0f},
    {0.25f, -0.75f, 0.5f, 1.0f}, {0.75f, -0.75f, 0.5f, 1.0f}, {0.5f, 0.75f, 0.5f, 1.0f},
}};

alignas(256) constexpr std::array<std::uint16_t, 10> Indices{0, 0xffff, 3, 4, 5, 0, 1, 2, 3, 4};

alignas(256) constexpr std::array<std::uint32_t, 10> IndirectRecords{
    4u, 1u, 1u, 0u, 0u,
    5u, 1u, 5u, 0u, 0u,
};

std::array<std::uint32_t, 4> VertexBufferDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Vertices.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(Vertices.size()), 0x0104d000u | IdentitySelect};
}

std::array<std::uint32_t, 4> OutputDescriptor() {
    const auto address = reinterpret_cast<std::uintptr_t>(Output.data());
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Output)), 0x31016facu};
}

std::vector<std::uint32_t> UserData() {
    std::vector<std::uint32_t> userData(12, 0u);
    userData[0] = 0x10000000u;
    userData[1] = 0x00000001u;
    const auto output = OutputDescriptor();
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    return userData;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Pm4::DrawParameters& draw) {
    Pixels.fill(Background);
    Output.fill(0xdeadbeefu);
    const auto userData = UserData();
    ShaderRecompiler::ShaderVertexStageInfo info{};
    info.resourcesNum = 1;
    info.resources[0].fields = VertexBufferDescriptor();
    info.resourcesDst[0] = {8, 4, 0, 0};
    info.fetchAttribReg = 8;
    info.fetchBufferReg = 10;
    info.fetchEmbedded = true;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, userData, std::nullopt, std::nullopt, info, vertexMemory},
        device.Target(),
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
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
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.primitiveRestart = true;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

bool CoveredAt(std::uint32_t x, std::uint32_t y) {
    const auto* pixel = Pixels.data() + (static_cast<std::size_t>(y) * Width + x) * 4u;
    const bool covered = std::all_of(pixel, pixel + 4, [](std::byte value) { return value == Covered; });
    Require(covered || std::all_of(pixel, pixel + 4, [](std::byte value) { return value == Background; }), "pixel " + std::to_string(x) + "," + std::to_string(y) + " is neither the background nor the triangle colour");
    return covered;
}

void Check(const std::string& what, bool left, bool right) {
    Require(CoveredAt(16, 20) == left, what + ": the left triangle (0 1 2) is " + (left ? "missing" : "drawn"));
    Require(CoveredAt(48, 20) == right, what + ": the right triangle (3 4 5) is " + (right ? "missing" : "drawn"));
    Require(!CoveredAt(32, 24) && !CoveredAt(32, 2), what + ": a pixel between or above the triangles was drawn");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::cout << "device list restart: " << (device->PrimitiveListRestart() ? "yes" : "no") << '\n';
        const auto indexAddress = reinterpret_cast<std::uintptr_t>(Indices.data());
        Draw(*device, AgcDriver::Pm4::DrawParameters{indexAddress, static_cast<std::uint32_t>(Indices.size()), 2, 1, 0, true});
        Check("direct triangle list with a restart and an incomplete tail", true, true);

        AgcDriver::Pm4::DrawParameters indirect{indexAddress, static_cast<std::uint32_t>(Indices.size()), 2, 0, 0, true};
        indirect.indirect = AgcDriver::Pm4::DrawParameters::IndirectDraw{reinterpret_cast<std::uintptr_t>(IndirectRecords.data()), 0x25u, 20u, 20u, 1u, false, 0u, NoLocation, NoLocation, NoLocation, false, 0u};
        Draw(*device, indirect);
        Check("indirect record starting at the restart", false, true);
        indirect.indirect->arguments = reinterpret_cast<std::uintptr_t>(IndirectRecords.data() + 5);
        Draw(*device, indirect);
        Check("indirect record over 0 1 2 3 4", true, false);
        indirect.indirect->arguments = reinterpret_cast<std::uintptr_t>(IndirectRecords.data());
        indirect.indirect->count = 2u;
        Draw(*device, indirect);
        Check("two indirect records", true, true);
        if (!device->PrimitiveListRestart()) {
            indirect.indexCount = 0;
            Draw(*device, indirect);
            Check("indirect records over an empty index range", false, false);
        }
        std::puts("list restart tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
