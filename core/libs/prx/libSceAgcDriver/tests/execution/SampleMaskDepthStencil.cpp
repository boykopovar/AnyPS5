#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t WaveSize = 64;
constexpr std::uint32_t EarlyZThenLateZ = 0x10u;
constexpr std::uint8_t Reference = 0x5au;
constexpr std::byte Kept{0x40};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::byte, 128 * 128 * 4> DepthMemory{};
alignas(256) std::array<std::byte, 256 * 256> StencilMemory{};
alignas(256) std::array<std::uint32_t, Width * Height> Stores{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PlainPixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 12> StoringPixelCode{
    0x7e040f00, 0x7e060f01, 0x34060686, 0x4a040702, 0x34040482, 0x7e080281, 0xe0705000, 0x80000402,
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

using Triangle = std::array<std::array<float, 4>, 3>;
alignas(256) constexpr Triangle Middle{{{-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};
alignas(256) constexpr Triangle Near{{{-1.0f, -1.0f, 0.25f, 1.0f}, {3.0f, -1.0f, 0.25f, 1.0f}, {-1.0f, 3.0f, 0.25f, 1.0f}}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, stride == 0u ? 0x31016facu : 0x01016facu};
}

struct Pass {
    std::array<std::uint16_t, 4> quadSampleMask{1, 1, 1, 1};
    const Triangle* triangle = &Middle;
    std::span<const std::uint32_t> pixelCode = PlainPixelCode;
    std::uint32_t shaderControl = EarlyZThenLateZ;
    bool depthTest = false;
    bool depthWrite = false;
    VkCompareOp depthCompare = VK_COMPARE_OP_ALWAYS;
    VkStencilOpState stencil{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0u, 0u};
};

void Draw(AgcDriver::VulkanDevice& device, const Pass& pass) {
    Pixels.fill(Kept);
    Stores.fill(0);
    const auto target = device.Target();
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(pass.triangle->data(), 16u, static_cast<std::uint32_t>(pass.triangle->size()));
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

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, WaveSize, WaveSize, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = AgcDriver::Graphics::DepthTarget{reinterpret_cast<std::uintptr_t>(DepthMemory.data()), reinterpret_cast<std::uintptr_t>(StencilMemory.data()), {Width, Height}, VK_FORMAT_D32_SFLOAT_S8_UINT, 0.0f, 0};
    state.depthTest = pass.depthTest;
    state.depthWrite = pass.depthWrite;
    state.depthCompare = pass.depthCompare;
    state.stencilTest = true;
    state.stencilFront = pass.stencil;
    state.stencilBack = pass.stencil;
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
    state.quadSampleMask = pass.quadSampleMask;
    state.pixelEarlyZ = AgcDriver::Graphics::PixelEarlyZ(pass.shaderControl);

    const AgcDriver::Registers context{{0x1b3, 0x300u}, {0x1b4, 0x300u}, {0x1b6, 0u}, {0x1c5, 9u}, {0x203, pass.shaderControl}};
    auto pixel = AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state));
    Require(pixel.earlyZ == state.pixelEarlyZ, "the pixel stage and the decoded state disagree on early Z");
    pixel.quadPixelMask = AgcDriver::Graphics::QuadPixelMask(state);
    std::vector<std::uint32_t> pixelUserData(4, 0u);
    const auto stores = BufferDescriptor(Stores.data(), 0u, static_cast<std::uint32_t>(sizeof(Stores)));
    std::copy(stores.begin(), stores.end(), pixelUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pass.pixelCode.data()), std::as_bytes(pass.pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pass.pixelCode.data()), pass.pixelCode, 0, {}},
        {WaveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(pass.triangle->size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Check(const std::function<bool(std::uint32_t, std::uint32_t)>& drawn, const std::string& what) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = drawn(x, y) ? std::byte{255} : Kept;
            const auto actual = Pixels[(y * Width + x) * 4u];
            Require(actual == expected, what + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + std::to_string(static_cast<unsigned>(actual)) + ", expected " + (drawn(x, y) ? "drawn" : "kept"));
        }
    }
}

bool Covered(std::uint8_t quadPixelMask, std::uint32_t x, std::uint32_t y) {
    return ((quadPixelMask >> ((x & 1u) | ((y & 1u) << 1u))) & 1u) != 0u;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const VkStencilOpState replace{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, Reference};
        const VkStencilOpState equal{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffu, 0u, Reference};
        const VkStencilOpState always{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0u, 0u};
        const VkStencilOpState zero{VK_STENCIL_OP_ZERO, VK_STENCIL_OP_ZERO, VK_STENCIL_OP_ZERO, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 0u};
        for (const std::uint8_t quadPixelMask : {std::uint8_t{0x1}, std::uint8_t{0x2}, std::uint8_t{0x9}}) {
            const auto mask = "quad pixel mask " + std::to_string(quadPixelMask);
            Pass reset;
            reset.triangle = &Near;
            reset.depthTest = true;
            reset.depthWrite = true;
            reset.stencil = zero;
            Draw(*device, reset);
            Check([](std::uint32_t, std::uint32_t) { return true; }, mask + ", the full-screen draw that resets depth to 0.25 and stencil to 0");
            Pass gbuffer;
            for (std::uint32_t pixel = 0; pixel < 4; ++pixel) gbuffer.quadSampleMask[pixel] = static_cast<std::uint16_t>((quadPixelMask >> pixel) & 1u);
            gbuffer.depthTest = true;
            gbuffer.depthWrite = true;
            gbuffer.depthCompare = VK_COMPARE_OP_GREATER_OR_EQUAL;
            gbuffer.stencil = replace;
            Draw(*device, gbuffer);
            Check([&](std::uint32_t x, std::uint32_t y) { return Covered(quadPixelMask, x, y); }, mask + ", the early-Z GEQUAL draw with depth writes and stencil REPLACE");
            Pass stencilProbe;
            stencilProbe.stencil = equal;
            Draw(*device, stencilProbe);
            Check([&](std::uint32_t x, std::uint32_t y) { return Covered(quadPixelMask, x, y); }, mask + ", stencil equal to the replaced reference");
            Pass depthProbe;
            depthProbe.triangle = &Near;
            depthProbe.depthTest = true;
            depthProbe.depthCompare = VK_COMPARE_OP_LESS;
            depthProbe.stencil = always;
            Draw(*device, depthProbe);
            Check([&](std::uint32_t x, std::uint32_t y) { return Covered(quadPixelMask, x, y); }, mask + ", depth 0.25 less than the stored depth");
            depthProbe.depthCompare = VK_COMPARE_OP_GREATER_OR_EQUAL;
            Draw(*device, depthProbe);
            Check([&](std::uint32_t x, std::uint32_t y) { return !Covered(quadPixelMask, x, y); }, mask + ", depth 0.25 at least the stored depth");
        }
        Pass storing;
        storing.quadSampleMask = {1, 0, 0, 0};
        storing.pixelCode = StoringPixelCode;
        storing.depthTest = true;
        storing.depthCompare = VK_COMPARE_OP_ALWAYS;
        storing.stencil = always;
        try {
            Draw(*device, storing);
        } catch (const std::exception& error) {
            Require(std::string(error.what()).find("differs between the pixels of a quad") != std::string::npos, std::string("a storing early-Z pixel program under a quad mask failed for another reason: ") + error.what());
            storing.shaderControl = 0u;
            Draw(*device, storing);
            Check([](std::uint32_t x, std::uint32_t y) { return Covered(0x1, x, y); }, "the storing late-Z pixel program under quad pixel mask 1");
            for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
                const auto expected = Covered(0x1, pixel % Width, pixel / Width) ? 1u : 0u;
                Require(Stores[pixel] == expected, "the storing pixel program under quad pixel mask 1 stored " + std::to_string(Stores[pixel]) + " at pixel " + std::to_string(pixel) + ", expected " + std::to_string(expected));
            }
            std::puts("quad sample mask depth and stencil tests passed");
            return 0;
        }
        Require(false, "a quad-masked early-Z pixel program that stores to memory was drawn with depth tests");
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
