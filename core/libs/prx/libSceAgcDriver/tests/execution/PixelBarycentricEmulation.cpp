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

alignas(256) constexpr std::array<std::uint32_t, 4> FlatBarycentricPixelCode{
    0xc8120002, 0xf800180f, 0x04040100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 5> InterpolatedPixelCode{
    0xc8080000, 0xc8090001, 0xf800180f, 0x02020202, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint16_t, 7> RestartIndices{2, 0xffff, 0, 1, 2, 0, 1};

struct Vertex {
    float x;
    float y;
    float w;
};

constexpr std::array<Vertex, 3> TriangleCorners{{{-0.8f, -0.8f, 1.0f}, {0.9f, -0.6f, 2.5f}, {-0.5f, 0.85f, 0.5f}}};
constexpr std::array<Vertex, 3> RectangleCorners{{{-0.8f, -0.8f, 1.5f}, {0.7f, -0.8f, 1.5f}, {-0.8f, 0.6f, 1.5f}}};

enum class Emulation {
    Geometry,
    Tessellation,
    RectList,
};

const std::array<Vertex, 3>& Corners(Emulation emulation) {
    return emulation == Emulation::RectList ? RectangleCorners : TriangleCorners;
}

std::array<std::array<float, 4>, 3> ClipPositions(Emulation emulation) {
    const auto& corners = Corners(emulation);
    std::array<std::array<float, 4>, 3> positions{};
    for (std::size_t i = 0; i < corners.size(); ++i) positions[i] = {corners[i].x * corners[i].w, corners[i].y * corners[i].w, 0.5f * corners[i].w, corners[i].w};
    return positions;
}

constexpr std::array<Vertex, 4> StripCorners{{{-0.8f, -0.8f, 1.0f}, {0.8f, -0.8f, 1.0f}, {-0.8f, 0.8f, 1.0f}, {0.8f, 0.8f, 1.0f}}};

std::array<std::array<float, 4>, 4> StripPositions() {
    std::array<std::array<float, 4>, 4> positions{};
    for (std::size_t i = 0; i < StripCorners.size(); ++i) positions[i] = {StripCorners[i].x, StripCorners[i].y, 0.5f, 1.0f};
    return positions;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

ShaderRecompiler::RecompileResult Draw(AgcDriver::VulkanDevice& device, Emulation emulation, std::span<const std::uint32_t> pixelCode, std::uint32_t inputs, VkCullModeFlags cullMode = VK_CULL_MODE_NONE, bool flat = false, VkProvokingVertexModeEXT provoking = VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT, bool strip = false, bool builtins = false, bool restart = false, std::span<const std::uint16_t> indices = {}) {
    Pixels.fill(Unwritten);
    auto target = device.Target();
    target.fragmentShaderBarycentricEnabled = false;
    target.fragmentBarycentricBuiltins = builtins;
    static const auto trianglePositions = ClipPositions(Emulation::Geometry);
    static const auto rectanglePositions = ClipPositions(Emulation::RectList);
    static const auto stripPositions = StripPositions();
    const auto positions = strip ? std::span<const std::array<float, 4>>(stripPositions) : std::span<const std::array<float, 4>>(emulation == Emulation::RectList ? rectanglePositions : trianglePositions);

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
    if (flat) pixel.interpolatorSettings[0] = 0x400u;
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
    ShaderRecompiler::BarycentricTessellationShaders patches;
    ShaderRecompiler::RectListShaders rectangle;
    if (emulation == Emulation::RectList) {
        rectangle = ShaderRecompiler::BuildRectListShaders(vertexResult, pixelResult, target);
        shaders.push_back({ShaderStage::TessellationControl, &rectangle.control, 0});
        shaders.push_back({ShaderStage::TessellationEvaluation, &rectangle.evaluation, 0});
    } else if (pixelResult.barycentricEmulation.active && emulation == Emulation::Geometry) {
        geometry = ShaderRecompiler::BuildBarycentricGeometryShader(vertexResult, pixelResult, target, device.GeometryLimits());
        shaders.push_back({ShaderStage::Geometry, &geometry, 0});
    } else if (pixelResult.barycentricEmulation.active) {
        patches = ShaderRecompiler::BuildBarycentricTessellationShaders(vertexResult, pixelResult, target, device.MaxFragmentInputComponents(), provoking == VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT ? 2u : 0u);
        shaders.push_back({ShaderStage::TessellationControl, &patches.control, 0});
        shaders.push_back({ShaderStage::TessellationEvaluation, &patches.evaluation, 0});
    }
    shaders.push_back({ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)});

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(Pixels), 0xe4u, AgcDriver::Graphics::ColorTileMode::Linear, 16u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = strip ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : emulation == Emulation::RectList ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.rectList = emulation == Emulation::RectList;
    state.primitiveRestart = restart;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = cullMode;
    state.provokingVertexMode = provoking;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const auto draw = indices.empty() ? AgcDriver::Pm4::DrawParameters{0, static_cast<std::uint32_t>(positions.size()), 0, 1, 0, false} : AgcDriver::Pm4::DrawParameters{reinterpret_cast<std::uintptr_t>(indices.data()), static_cast<std::uint32_t>(indices.size()), 2, 1, 0, true};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    return pixelResult;
}

struct Expected {
    bool inside;
    std::array<float, 3> linear;
    std::array<float, 3> perspective;
};

Expected Barycentrics(Emulation emulation, std::uint32_t x, std::uint32_t y, bool snapped) {
    const auto& corners = Corners(emulation);
    std::array<std::array<double, 2>, 3> window{};
    for (std::size_t i = 0; i < corners.size(); ++i) window[i] = {(corners[i].x + 1.0) * Width / 2.0, (1.0 - corners[i].y) * Height / 2.0};
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
    const bool inside = emulation == Emulation::RectList ? std::min(b[1], b[2]) > 0.02 && std::max(b[1], b[2]) < 0.98 : std::min({b[0], b[1], b[2]}) > 0.02;
    Expected expected{inside, {}, {}};
    double sum = 0.0;
    for (std::size_t i = 0; i < 3; ++i) sum += b[i] / corners[i].w;
    for (std::size_t i = 0; i < 3; ++i) {
        expected.linear[i] = static_cast<float>(b[i]);
        expected.perspective[i] = static_cast<float>(b[i] / corners[i].w / sum);
    }
    return expected;
}

Expected BarycentricsUnsnapped(Emulation emulation, std::uint32_t x, std::uint32_t y) {
    return Barycentrics(emulation, x, y, false);
}

Expected BarycentricsSnapped(Emulation emulation, std::uint32_t x, std::uint32_t y) {
    return Barycentrics(emulation, x, y, true);
}

bool MatchesEither(float value, float unsnapped, float snapped) {
    return std::fabs(value - unsnapped) <= 1e-4f || std::fabs(value - snapped) <= 1e-4f;
}

float Pixel(std::uint32_t x, std::uint32_t y, std::uint32_t channel) {
    return Pixels[(y * Width + x) * 4u + channel];
}

std::uint32_t CheckBarycentrics(Emulation emulation, const std::string& what, bool perspective) {
    std::uint32_t checked = 0;
    std::uint32_t beyond = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = BarycentricsUnsnapped(emulation, x, y);
            if (!expected.inside) continue;
            const auto snapped = BarycentricsSnapped(emulation, x, y);
            if (expected.linear[0] < -0.02f) ++beyond;
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
    Require(emulation != Emulation::RectList || beyond > 200u, what + ": too few pixels past the rectangle's diagonal were checked");
    return checked;
}

void CheckVertexValues(Emulation emulation) {
    const auto positions = ClipPositions(emulation);
    const auto p0 = positions[0][0];
    const std::array<float, 3> expected{positions[1][0] - p0, positions[2][0] - p0, p0};
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            if (!BarycentricsUnsnapped(emulation, x, y).inside) continue;
            for (std::uint32_t channel = 0; channel < 3; ++channel) {
                Require(Pixel(x, y, channel) == expected[channel], "v_interp_mov: pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(Pixel(x, y, channel)) + ", expected " + std::to_string(expected[channel]));
            }
            ++checked;
        }
    }
    Require(checked > 200u, "v_interp_mov: too few interior pixels were checked");
}

void CheckInterpolated(Emulation emulation) {
    const auto positions = ClipPositions(emulation);
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = BarycentricsUnsnapped(emulation, x, y);
            if (!expected.inside) continue;
            const auto snapped = BarycentricsSnapped(emulation, x, y);
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

void CheckFlat(const std::string& what, std::size_t provoking) {
    const auto positions = ClipPositions(Emulation::Geometry);
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = BarycentricsUnsnapped(Emulation::Geometry, x, y);
            if (!expected.inside) continue;
            const auto snapped = BarycentricsSnapped(Emulation::Geometry, x, y);
            for (std::uint32_t channel = 0; channel < 2; ++channel) {
                Require(MatchesEither(Pixel(x, y, channel), expected.perspective[1u + channel], snapped.perspective[1u + channel]), what + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") weight " + std::to_string(channel) + " is " + std::to_string(Pixel(x, y, channel)));
            }
            for (std::uint32_t channel = 2; channel < 4; ++channel) {
                Require(Pixel(x, y, channel) == positions[provoking][0], what + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") flat channel " + std::to_string(channel) + " is " + std::to_string(Pixel(x, y, channel)) + ", expected vertex " + std::to_string(provoking) + "'s " + std::to_string(positions[provoking][0]));
            }
            ++checked;
        }
    }
    Require(checked > 200u, what + ": too few interior pixels were checked");
}

bool Drawn() {
    return std::any_of(Pixels.begin(), Pixels.end(), [](float value) { return value != Unwritten; });
}

void CheckWinding(AgcDriver::VulkanDevice& device, Emulation emulation, const std::string& name) {
    const auto perspective = PixelInputBit(PixelInput::PerspectiveCenter);
    for (const auto cullMode : {VK_CULL_MODE_BACK_BIT, VK_CULL_MODE_FRONT_BIT}) {
        Draw(device, emulation, InterpolatedPixelCode, perspective, cullMode);
        const bool host = Drawn();
        Draw(device, emulation, BarycentricPixelCode, perspective, cullMode);
        Require(Drawn() == host, name + ": culling the " + (cullMode == VK_CULL_MODE_BACK_BIT ? "back" : "front") + " face does not match the draw without emulation");
    }
}

struct StripPixel {
    bool inside;
    std::array<std::size_t, 3> order;
    std::array<double, 3> weights;
};

StripPixel StripBarycentrics(std::uint32_t x, std::uint32_t y, bool snapped) {
    std::array<std::array<double, 2>, 4> window{};
    for (std::size_t i = 0; i < StripCorners.size(); ++i) {
        window[i] = {(StripCorners[i].x + 1.0) * Width / 2.0, (1.0 - StripCorners[i].y) * Height / 2.0};
        if (snapped) {
            for (auto& coordinate : window[i]) coordinate = std::round(coordinate * 256.0) / 256.0;
        }
    }
    const double px = x + 0.5;
    const double py = y + 0.5;
    const auto edge = [](const std::array<double, 2>& a, const std::array<double, 2>& b, double cx, double cy) { return (b[0] - a[0]) * (cy - a[1]) - (b[1] - a[1]) * (cx - a[0]); };
    for (const std::array<std::size_t, 3> order : {std::array<std::size_t, 3>{0, 1, 2}, std::array<std::size_t, 3>{1, 3, 2}}) {
        const auto& a = window[order[0]];
        const auto& b = window[order[1]];
        const auto& c = window[order[2]];
        const double area = edge(a, b, c[0], c[1]);
        const std::array<double, 3> weights{edge(b, c, px, py) / area, edge(c, a, px, py) / area, edge(a, b, px, py) / area};
        if (std::min({weights[0], weights[1], weights[2]}) > 0.02) return {true, order, weights};
    }
    return {false, {}, {}};
}

void CheckStrip(AgcDriver::VulkanDevice& device, Emulation emulation, const std::string& name, bool restart = false) {
    const std::string strip = restart ? " triangle strip with primitive restart" : " triangle strip";
    Draw(device, emulation, BarycentricPixelCode, PixelInputBit(PixelInput::LinearCenter), VK_CULL_MODE_NONE, false, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT, true, false, restart);
    std::array<std::uint32_t, 2> checked{};
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = StripBarycentrics(x, y, false);
            if (!expected.inside) continue;
            const auto snapped = StripBarycentrics(x, y, true);
            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                const auto reference = static_cast<float>(expected.weights[1u + (channel & 1u)]);
                const auto snappedReference = snapped.inside && snapped.order == expected.order ? static_cast<float>(snapped.weights[1u + (channel & 1u)]) : reference;
                Require(MatchesEither(Pixel(x, y, channel), reference, snappedReference), name + strip + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(Pixel(x, y, channel)) + ", expected " + std::to_string(reference));
            }
            ++checked[expected.order[0]];
        }
    }
    Require(checked[0] > 200u && checked[1] > 200u, name + strip + ": too few pixels of each triangle were checked");
    static const auto positions = StripPositions();
    Draw(device, emulation, VertexValuePixelCode, PixelInputBit(PixelInput::PerspectiveCenter), VK_CULL_MODE_NONE, false, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT, true, false, restart);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = StripBarycentrics(x, y, false);
            if (!expected.inside) continue;
            const auto first = positions[expected.order[0]][0];
            const std::array<float, 3> values{positions[expected.order[1]][0] - first, positions[expected.order[2]][0] - first, first};
            for (std::uint32_t channel = 0; channel < 3; ++channel) {
                Require(Pixel(x, y, channel) == values[channel], name + strip + " v_interp_mov: pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(Pixel(x, y, channel)) + ", expected " + std::to_string(values[channel]));
            }
        }
    }
    std::printf("barycentric emulation through the %s passed for a%s: %u and %u pixels of its even and odd triangle\n", name.c_str(), strip.c_str(), checked[0], checked[1]);
}

void CheckStripFlatLast(AgcDriver::VulkanDevice& device, Emulation emulation, const std::string& name) {
    Draw(device, emulation, FlatBarycentricPixelCode, PixelInputBit(PixelInput::PerspectiveCenter), VK_CULL_MODE_NONE, true, VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT, true);
    const auto positions = StripPositions();
    std::array<std::uint32_t, 2> checked{};
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = StripBarycentrics(x, y, false);
            if (!expected.inside) continue;
            const auto triangle = expected.order[0];
            const auto provoking = triangle == 0u ? 2u : 3u;
            for (std::uint32_t channel = 2; channel < 4; ++channel) {
                Require(Pixel(x, y, channel) == positions[provoking][0], name + " flat last-provoking strip: pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") flat channel " + std::to_string(channel) + " is " + std::to_string(Pixel(x, y, channel)) + ", expected vertex " + std::to_string(provoking) + " value " + std::to_string(positions[provoking][0]));
            }
            ++checked[triangle];
        }
    }
    Require(checked[0] > 200u && checked[1] > 200u, name + " flat last-provoking strip: too few pixels of each triangle were checked");
    std::printf("flat last-provoking strip through %s passed: %u and %u pixels of its even and odd triangle\n", name.c_str(), checked[0], checked[1]);
}

void RunEmulation(AgcDriver::VulkanDevice& device, Emulation emulation) {
    const std::string name = emulation == Emulation::Geometry ? "geometry stage" : emulation == Emulation::Tessellation ? "tessellation stages" : "rect-list stages";
    const auto perspective = PixelInputBit(PixelInput::PerspectiveCenter);
    const auto linear = PixelInputBit(PixelInput::LinearCenter);
    auto result = Draw(device, emulation, BarycentricPixelCode, perspective);
    Require(result.barycentricEmulation.active && result.barycentricEmulation.smooth && !result.barycentricEmulation.linear, "reading PERSP_CENTER I/J did not emulate perspective barycentrics");
    const auto smooth = CheckBarycentrics(emulation, name + " PERSP_CENTER I/J", true);
    result = Draw(device, emulation, BarycentricPixelCode, linear);
    Require(result.barycentricEmulation.active && !result.barycentricEmulation.smooth && result.barycentricEmulation.linear, "reading LINEAR_CENTER I/J did not emulate linear barycentrics");
    const auto noPerspective = CheckBarycentrics(emulation, name + " LINEAR_CENTER I/J", false);
    result = Draw(device, emulation, VertexValuePixelCode, perspective);
    Require(result.barycentricEmulation.active && result.fragmentParameters.size() == 1 && result.fragmentParameters[0].perVertex, "v_interp_mov of P10 and P20 did not emulate per-vertex parameters");
    CheckVertexValues(emulation);
    result = Draw(device, emulation, InterpolatedPixelCode, perspective);
    Require(!result.barycentricEmulation.active, "plain v_interp_p1/p2 needed a generated stage");
    CheckInterpolated(emulation);
    if (emulation != Emulation::RectList) {
        CheckWinding(device, emulation, name);
        result = Draw(device, emulation, FlatBarycentricPixelCode, perspective, VK_CULL_MODE_NONE, true);
        Require(result.barycentricEmulation.active && std::any_of(result.fragmentParameters.begin(), result.fragmentParameters.end(), [](const auto& parameter) { return parameter.flat && !parameter.perVertex; }), "a flat P0 read beside raw I/J did not pass a flat parameter through the emulation");
        CheckFlat(name + " flat input, first provoking vertex", 0u);
        if (emulation == Emulation::Tessellation) {
            Draw(device, emulation, FlatBarycentricPixelCode, perspective, VK_CULL_MODE_NONE, true, VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT);
            CheckFlat(name + " flat input, last provoking vertex", 2u);
            CheckStripFlatLast(device, emulation, name);
            CheckStrip(device, emulation, name, true);
            if (!device.PrimitiveListRestart()) {
                Draw(device, emulation, BarycentricPixelCode, perspective, VK_CULL_MODE_NONE, false, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT, false, false, true);
                const auto restartList = CheckBarycentrics(emulation, name + " triangle list with primitive restart", true);
                Draw(device, emulation, BarycentricPixelCode, perspective, VK_CULL_MODE_NONE, false, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT, false, false, true, RestartIndices);
                const auto compactedList = CheckBarycentrics(emulation, name + " indexed triangle list with a restart index", true);
                std::printf("barycentric emulation through the %s passed for a triangle list with primitive restart: %u pixels without indices and %u with a compacted restart index\n", name.c_str(), restartList, compactedList);
            }
            bool refused = false;
            try {
                Draw(device, emulation, BarycentricPixelCode, linear, VK_CULL_MODE_NONE, false, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT, true, false, true, RestartIndices);
            } catch (const std::exception& error) {
                refused = std::string(error.what()).find("an indexed triangle strip with primitive restart through emulated barycentric patches is unsupported") != std::string::npos;
            }
            Require(refused, name + ": an indexed triangle strip with primitive restart was not refused");
        }
        CheckStrip(device, emulation, name);
    }
    std::printf("barycentric emulation through the %s passed: %u perspective and %u linear I/J pixels, raw vertex values, host interpolation%s\n", name.c_str(), smooth, noPerspective, emulation == Emulation::RectList ? "" : ", face culling, flat inputs and triangle strips");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().fragmentBarycentricBuiltins) {
            const auto perspective = PixelInputBit(PixelInput::PerspectiveCenter);
            const auto linear = PixelInputBit(PixelInput::LinearCenter);
            auto builtin = Draw(*device, Emulation::Tessellation, BarycentricPixelCode, perspective, VK_CULL_MODE_NONE, false, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT, false, true);
            Require(!builtin.barycentricEmulation.active, "reading PERSP_CENTER I/J with barycentric built-ins needed generated stages");
            const auto builtinSmooth = CheckBarycentrics(Emulation::Tessellation, "PERSP_CENTER I/J from BaryCoordKHR", true);
            builtin = Draw(*device, Emulation::Tessellation, BarycentricPixelCode, linear, VK_CULL_MODE_NONE, false, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT, false, true);
            Require(!builtin.barycentricEmulation.active, "reading LINEAR_CENTER I/J with barycentric built-ins needed generated stages");
            const auto builtinLinear = CheckBarycentrics(Emulation::Tessellation, "LINEAR_CENTER I/J from BaryCoordNoPerspKHR", false);
            std::printf("barycentric built-in tests passed: %u perspective and %u linear I/J pixels\n", builtinSmooth, builtinLinear);
        }
        const bool geometry = device->GeometryLimits().has_value();
        const bool tessellation = device->Target().tessellation.has_value();
        if (!geometry && !tessellation) {
            std::puts("skipped, the device lacks geometryShader and tessellationShader");
            return VulkanTestSkipped;
        }
        if (geometry) RunEmulation(*device, Emulation::Geometry);
        else std::puts("geometry stage skipped, the device lacks geometryShader");
        if (tessellation) {
            RunEmulation(*device, Emulation::Tessellation);
            RunEmulation(*device, Emulation::RectList);
        } else {
            std::puts("tessellation and rect-list stages skipped, the device lacks tessellationShader");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
