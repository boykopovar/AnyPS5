#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 48;
alignas(256) std::array<float, Width * Height * 4> Pixels{};
constexpr std::array<std::array<float, 3>, 3> Corners{{{-0.8f, -0.8f, 1.0f}, {0.9f, -0.6f, 2.5f}, {-0.5f, 0.85f, 0.5f}}};

alignas(256) constexpr std::array<std::uint32_t, 8> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> ExecCopyPixelCode{
    0xbefe0a7e, 0xc8080000, 0xbe80047e, 0xbefe0400, 0xc8090001, 0xf800180f, 0x02020202, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> RestoredEntryExecPixelCode{
    0xbe80047e, 0xbefe0a7e, 0xc8080000, 0xc8090001, 0xbefe0400, 0xf800180f, 0x02020202, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> RepeatedWqmPixelCode{
    0xbefe0a7e, 0xc8080000, 0xbefe0a7e, 0xc8090001, 0xf800180f, 0x02020202, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 6> ExpandedExecPixelCode{
    0xc8080000, 0xbefe0a7e, 0xc8090001, 0xf800180f, 0x02020202, 0xbf810000,
};

std::array<std::array<float, 4>, 3> ClipPositions() {
    std::array<std::array<float, 4>, 3> positions{};
    for (std::size_t i = 0; i < Corners.size(); ++i) positions[i] = {Corners[i][0] * Corners[i][2], Corners[i][1] * Corners[i][2], 0.5f * Corners[i][2], Corners[i][2]};
    return positions;
}

struct Expected {
    bool inside;
    float value;
};

Expected Interpolated(std::uint32_t x, std::uint32_t y, bool snapped) {
    std::array<std::array<double, 2>, 3> window{};
    for (std::size_t i = 0; i < Corners.size(); ++i) window[i] = {(Corners[i][0] + 1.0) * Width / 2.0, (1.0 - Corners[i][1]) * Height / 2.0};
    if (snapped) {
        for (auto& corner : window) for (auto& coordinate : corner) coordinate = std::round(coordinate * 256.0) / 256.0;
    }
    const double px = x + 0.5;
    const double py = y + 0.5;
    const auto edge = [](const std::array<double, 2>& a, const std::array<double, 2>& b, double cx, double cy) { return (b[0] - a[0]) * (cy - a[1]) - (b[1] - a[1]) * (cx - a[0]); };
    const double area = edge(window[0], window[1], window[2][0], window[2][1]);
    const std::array<double, 3> weights{edge(window[1], window[2], px, py) / area, edge(window[2], window[0], px, py) / area, edge(window[0], window[1], px, py) / area};
    double denominator = 0.0;
    for (std::size_t i = 0; i < Corners.size(); ++i) denominator += weights[i] / Corners[i][2];
    const auto positions = ClipPositions();
    double value = 0.0;
    for (std::size_t i = 0; i < Corners.size(); ++i) value += weights[i] / Corners[i][2] / denominator * positions[i][0];
    return {std::min({weights[0], weights[1], weights[2]}) > 0.02, static_cast<float>(value)};
}

std::uint32_t CheckPixels() {
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = Interpolated(x, y, false);
            if (!expected.inside) continue;
            const auto snapped = Interpolated(x, y, true);
            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                const auto value = Pixels[(y * Width + x) * 4u + channel];
                Require(std::fabs(value - expected.value) <= 1e-4f || std::fabs(value - snapped.value) <= 1e-4f, "EXEC interpolation: incorrect pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel));
            }
            ++checked;
        }
    }
    Require(checked > 200u, "EXEC interpolation: too few interior pixels were checked");
    return checked;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    auto target = device.Target();
    target.fragmentShaderBarycentricEnabled = false;
    static const auto positions = ClipPositions();
    const auto address = reinterpret_cast<std::uintptr_t>(positions.data());
    const std::array<std::uint32_t, 4> vertexUserData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), 3u, 0x01016facu};
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target, {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = waveSize == 32u;
    pixel.interpolatorCount = 1;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveCenter);
    pixel.hasPerspectiveCenterVgpr = true;
    pixel.executeOnNoop = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const auto compile = [&](std::span<const std::uint32_t> code) {
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        ShaderRecompiler::RecompileRequest fragment{
            {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
            {waveSize, 0, {}, std::nullopt, pixel, std::nullopt, memory}, target, PixelPushLayout(vertexPush, target)
        };
        fragment.useCache = false;
        return ShaderRecompiler::Recompile(fragment);
    };
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(Pixels), 0xe4u, AgcDriver::Graphics::ColorTileMode::Linear, 16u};
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
    for (const auto code : {std::span<const std::uint32_t>(ExecCopyPixelCode), std::span<const std::uint32_t>(RepeatedWqmPixelCode), std::span<const std::uint32_t>(RestoredEntryExecPixelCode)}) {
        const auto result = compile(code);
        Require(!result.barycentricEmulation.active, "changing EXEC between v_interp_p1/p2 needed a generated stage");
        Pixels.fill(-7.0f);
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderStage::Vertex, &vertexResult, 0}, {ShaderStage::Fragment, &result, PixelPushOffset(vertexPush, target)}}};
        device.Draw(state, {0, 3, 0, 1, 0, false}, shaders);
        device.WaitIdle();
        std::printf("wave%u %zu words: host interpolation matched %u pixels\n", waveSize, code.size(), CheckPixels());
    }
    Require(compile(ExpandedExecPixelCode).barycentricEmulation.active, "a partial interpolation written before WQM incorrectly used host interpolation");
    std::printf("wave%u: expanded EXEC keeps barycentric emulation\n", waveSize);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto wave : {32u, 64u}) Run(*device, wave);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
