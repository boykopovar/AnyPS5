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
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::PixelInput;
using ShaderRecompiler::PixelInputBit;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 48;
constexpr float Unwritten = -7.0f;
alignas(256) std::array<float, Width * Height * 4> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 8> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 3> BarycentricPixelCode{
    0xf800180f, 0x01000100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexValuePixelCode{
    0xc80a0000, 0xc80e0001, 0xc8120002, 0xf800180f, 0x04040302, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 5> InterpolatedPixelCode{
    0xc8080000, 0xc8090001, 0xf800180f, 0x02020202, 0xbf810000,
};

struct Vertex {
    float x;
    float y;
    float w;
};

constexpr std::array<Vertex, 3> Corners{{{-0.8f, -0.8f, 1.0f}, {0.9f, -0.6f, 2.5f}, {-0.5f, 0.85f, 0.5f}}};

std::array<std::array<float, 4>, 3> ClipPositions() {
    std::array<std::array<float, 4>, 3> positions{};
    for (std::size_t i = 0; i < Corners.size(); ++i) positions[i] = {Corners[i].x * Corners[i].w, Corners[i].y * Corners[i].w, 0.5f * Corners[i].w, Corners[i].w};
    return positions;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

ShaderRecompiler::RecompileResult Draw(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> pixelCode, std::uint32_t inputs) {
    Pixels.fill(Unwritten);
    auto target = device.Target();
    target.fragmentShaderBarycentricEnabled = false;
    static const auto positions = ClipPositions();

    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(positions.data(), 16u, static_cast<std::uint32_t>(positions.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.interpolatorCount = 1;
    pixel.inputAddr = inputs;
    pixel.hasPerspectiveCenterVgpr = (inputs & PixelInputBit(PixelInput::PerspectiveCenter)) != 0u;
    pixel.noPerspective = (inputs & PixelInputBit(PixelInput::LinearCenter)) != 0u;
    pixel.executeOnNoop = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData;
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    auto pixelResult = ShaderRecompiler::Recompile(fragment);
    std::vector<AgcDriver::Graphics::CompiledShader> shaders{{ShaderStage::Vertex, &vertexResult, 0}};
    ShaderRecompiler::RecompileResult geometry;
    if (pixelResult.barycentricEmulation.active) {
        geometry = ShaderRecompiler::BuildBarycentricGeometryShader(vertexResult, pixelResult, target, device.GeometryLimits());
        shaders.push_back({ShaderStage::Geometry, &geometry, 0});
    }
    shaders.push_back({ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)});

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(positions.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    return pixelResult;
}

struct Expected {
    bool inside;
    std::array<float, 3> linear;
    std::array<float, 3> perspective;
};

Expected Barycentrics(std::uint32_t x, std::uint32_t y, bool snapped) {
    std::array<std::array<double, 2>, 3> window{};
    for (std::size_t i = 0; i < Corners.size(); ++i) window[i] = {(Corners[i].x + 1.0) * Width / 2.0, (1.0 - Corners[i].y) * Height / 2.0};
    if (snapped) {
        for (auto& corner : window) {
            for (auto& coordinate : corner) coordinate = std::round(coordinate * 256.0) / 256.0;
        }
    }
    const double px = x + 0.5;
    const double py = y + 0.5;
    const auto edge = [](const std::array<double, 2>& a, const std::array<double, 2>& b, double cx, double cy) { return (b[0] - a[0]) * (cy - a[1]) - (b[1] - a[1]) * (cx - a[0]); };
    const double area = edge(window[0], window[1], window[2][0], window[2][1]);
    const std::array<double, 3> b{edge(window[1], window[2], px, py) / area, edge(window[2], window[0], px, py) / area, edge(window[0], window[1], px, py) / area};
    Expected expected{std::min({b[0], b[1], b[2]}) > 0.02, {}, {}};
    double sum = 0.0;
    for (std::size_t i = 0; i < 3; ++i) sum += b[i] / Corners[i].w;
    for (std::size_t i = 0; i < 3; ++i) {
        expected.linear[i] = static_cast<float>(b[i]);
        expected.perspective[i] = static_cast<float>(b[i] / Corners[i].w / sum);
    }
    return expected;
}

Expected BarycentricsUnsnapped(std::uint32_t x, std::uint32_t y) {
    return Barycentrics(x, y, false);
}

Expected BarycentricsSnapped(std::uint32_t x, std::uint32_t y) {
    return Barycentrics(x, y, true);
}

bool MatchesEither(float value, float unsnapped, float snapped) {
    return std::fabs(value - unsnapped) <= 1e-4f || std::fabs(value - snapped) <= 1e-4f;
}

float Pixel(std::uint32_t x, std::uint32_t y, std::uint32_t channel) {
    return Pixels[(y * Width + x) * 4u + channel];
}

std::uint32_t CheckBarycentrics(const std::string& what, bool perspective) {
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = BarycentricsUnsnapped(x, y);
            if (!expected.inside) continue;
            const auto snapped = BarycentricsSnapped(x, y);
            const auto& weights = perspective ? expected.perspective : expected.linear;
            const auto& snappedWeights = perspective ? snapped.perspective : snapped.linear;
            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                const auto value = Pixel(x, y, channel);
                const auto reference = weights[1u + (channel & 1u)];
                const auto snappedReference = snappedWeights[1u + (channel & 1u)];
                Require(MatchesEither(value, reference, snappedReference), what + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(value) + ", expected " + std::to_string(reference) + " or " + std::to_string(snappedReference));
            }
            ++checked;
        }
    }
    Require(checked > 200u, what + ": too few interior pixels were checked");
    return checked;
}

void CheckVertexValues() {
    const auto positions = ClipPositions();
    const auto p0 = positions[0][0];
    const std::array<float, 3> expected{positions[1][0] - p0, positions[2][0] - p0, p0};
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            if (!BarycentricsUnsnapped(x, y).inside) continue;
            for (std::uint32_t channel = 0; channel < 3; ++channel) {
                Require(Pixel(x, y, channel) == expected[channel], "v_interp_mov: pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(Pixel(x, y, channel)) + ", expected " + std::to_string(expected[channel]));
            }
            ++checked;
        }
    }
    Require(checked > 200u, "v_interp_mov: too few interior pixels were checked");
}

void CheckInterpolated() {
    const auto positions = ClipPositions();
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = BarycentricsUnsnapped(x, y);
            if (!expected.inside) continue;
            const auto snapped = BarycentricsSnapped(x, y);
            float reference = 0.0f;
            float snappedReference = 0.0f;
            for (std::size_t i = 0; i < 3; ++i) {
                reference += expected.perspective[i] * positions[i][0];
                snappedReference += snapped.perspective[i] * positions[i][0];
            }
            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                Require(MatchesEither(Pixel(x, y, channel), reference, snappedReference), "v_interp_p1/p2: pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(Pixel(x, y, channel)) + ", expected " + std::to_string(reference) + " or " + std::to_string(snappedReference));
            }
            ++checked;
        }
    }
    Require(checked > 200u, "v_interp_p1/p2: too few interior pixels were checked");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!device->GeometryLimits()) {
            std::puts("skipped, the device lacks geometryShader");
            return VulkanTestSkipped;
        }
        const auto perspective = PixelInputBit(PixelInput::PerspectiveCenter);
        const auto linear = PixelInputBit(PixelInput::LinearCenter);
        auto result = Draw(*device, BarycentricPixelCode, perspective);
        Require(result.barycentricEmulation.active && result.barycentricEmulation.smooth && !result.barycentricEmulation.linear, "reading PERSP_CENTER I/J did not emulate perspective barycentrics");
        const auto smooth = CheckBarycentrics("PERSP_CENTER I/J", true);
        result = Draw(*device, BarycentricPixelCode, linear);
        Require(result.barycentricEmulation.active && !result.barycentricEmulation.smooth && result.barycentricEmulation.linear, "reading LINEAR_CENTER I/J did not emulate linear barycentrics");
        const auto noPerspective = CheckBarycentrics("LINEAR_CENTER I/J", false);
        result = Draw(*device, VertexValuePixelCode, perspective);
        Require(result.barycentricEmulation.active && result.fragmentParameters.size() == 1 && result.fragmentParameters[0].perVertex, "v_interp_mov of P10 and P20 did not emulate per-vertex parameters");
        CheckVertexValues();
        result = Draw(*device, InterpolatedPixelCode, perspective);
        Require(!result.barycentricEmulation.active, "plain v_interp_p1/p2 needed a geometry stage");
        CheckInterpolated();
        std::printf("barycentric emulation tests passed: %u perspective and %u linear I/J pixels, raw vertex values and host interpolation\n", smooth, noPerspective);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
