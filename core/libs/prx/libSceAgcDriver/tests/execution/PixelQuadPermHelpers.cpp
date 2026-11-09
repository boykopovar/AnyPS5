#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::uint32_t Width = 192;
constexpr std::uint32_t Height = 128;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};
alignas(256) std::array<std::uint32_t, Width * Height> Results{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

std::vector<std::uint32_t> PixelCode(std::uint32_t waveSize, bool fetchInactive) {
    const bool wave32 = waveSize == 32u;
    const std::uint32_t fi = fetchInactive ? 0x00040000u : 0u;
    return {
        wave32 ? 0xbe8a037eu : 0xbe8a047eu, wave32 ? 0xbefe097eu : 0xbefe0a7eu, 0x7e040f00, 0x7e060f01, 0xd5430006, 0x0409ff03, 0x000000c0,
        0x7e0802fa, 0xff00b106 | fi, 0x7e0a02fa, 0xff004e06 | fi, wave32 ? 0xbefe030au : 0xbefe040au, 0xd76f0005, 0x04112105,
        0xe0702000, 0x80010506, 0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::vector<std::array<float, 4>> Triangles() {
    std::vector<std::array<float, 4>> vertices;
    constexpr std::uint32_t columns = 6;
    constexpr std::uint32_t rows = 4;
    for (std::uint32_t cell = 0; cell < columns * rows; ++cell) {
        const float width = 2.0f / columns;
        const float height = 2.0f / rows;
        const float x = -1.0f + width * static_cast<float>(cell % columns);
        const float y = -1.0f + height * static_cast<float>(cell / columns);
        const float skew = 0.07f * static_cast<float>(cell % 5u);
        vertices.push_back({x + (0.05f + skew) * width, y + 0.05f * height, 0.5f, 1.0f});
        vertices.push_back({x + 0.93f * width, y + (0.11f + skew) * height, 0.5f, 1.0f});
        vertices.push_back({x + (0.37f - skew * 0.5f) * width, y + 0.91f * height, 0.5f, 1.0f});
    }
    return vertices;
}

void Draw(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> pixelCode, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target) {
    static const auto triangles = Triangles();
    Pixels.fill(std::byte{0});
    Results.fill(Sentinel);

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
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(8, 0u);
    const auto results = BufferDescriptor(Results.data(), 4u, static_cast<std::uint32_t>(Results.size()));
    std::copy(results.begin(), results.end(), pixelUserData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
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

bool Covered(std::uint32_t pixel) {
    return std::to_integer<std::uint8_t>(Pixels[pixel * 4u]) == 255u;
}

std::uint32_t Check(const std::string& what) {
    std::uint32_t covered = 0;
    std::uint32_t helperReads = 0;
    for (std::uint32_t pixel = 0; pixel < Results.size(); ++pixel) {
        if (!Covered(pixel)) {
            Require(Results[pixel] == Sentinel, what + ": uncovered pixel " + std::to_string(pixel) + " stored " + std::to_string(Results[pixel]));
            continue;
        }
        ++covered;
        const auto x = pixel % Width;
        const auto y = pixel / Width;
        const auto across = y * Width + (x ^ 1u);
        const auto down = (y ^ 1u) * Width + x;
        Require(Results[pixel] != Sentinel, what + ": covered pixel " + std::to_string(pixel) + " stored nothing");
        Require((Results[pixel] & 0xffffu) == across, what + ": pixel " + std::to_string(pixel) + " read " + std::to_string(Results[pixel] & 0xffffu) + " from quad_perm [1,0,3,2], its " + (Covered(across) ? "covered" : "helper") + " neighbour is pixel " + std::to_string(across));
        Require((Results[pixel] >> 16u) == down, what + ": pixel " + std::to_string(pixel) + " read " + std::to_string(Results[pixel] >> 16u) + " from quad_perm [2,3,0,1], its " + (Covered(down) ? "covered" : "helper") + " neighbour is pixel " + std::to_string(down));
        helperReads += (Covered(across) ? 0u : 1u) + (Covered(down) ? 0u : 1u);
    }
    Require(covered > 1000, what + ": the triangles covered only " + std::to_string(covered) + " pixels");
    Require(helperReads > 100, what + ": only " + std::to_string(helperReads) + " reads had a helper neighbour");
    return helperReads;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        if (!TargetHasCapability(target, spv::CapabilityGroupNonUniformQuad)) {
            std::puts("skipped, the device has no subgroup quad operations");
            return VulkanTestSkipped;
        }
        if (target.subgroupSize < 32u) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", target.subgroupSize);
            return VulkanTestSkipped;
        }
        for (const auto waveSize : {64u, 32u}) {
            for (const bool fetchInactive : {false, true}) {
                const auto code = PixelCode(waveSize, fetchInactive);
                const auto what = "pixel quad_perm wave" + std::to_string(waveSize) + (fetchInactive ? " fi" : "");
                std::uint32_t helperReads = 0;
                for (std::uint32_t pass = 0; pass < 4; ++pass) {
                    Draw(*device, code, waveSize, target);
                    helperReads += Check(what);
                }
                std::printf("%s: %u reads from helper neighbours over 4 draws\n", what.c_str(), helperReads);
            }
        }
        std::puts("pixel quad_perm helper tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
