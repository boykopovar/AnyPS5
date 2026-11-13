#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
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

alignas(256) constexpr std::array<std::uint32_t, 8> ParameterVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> LayerVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0x7e080280, 0xf800020f, 0x03020100, 0xf80000cf, 0x03020100, 0xf80008d4, 0x04040404, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 10> SecondParameterVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf800021f, 0x03020100, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 10> ClipVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf80000cf, 0x03020100, 0xf80008d7, 0x03030303, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> ClipLayerVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0x7e080280, 0xf800020f, 0x03020100, 0xf80000cf, 0x03020100, 0xf80000d4, 0x04040404, 0xf80008e7, 0x03030303, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 16> FourParameterClipVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf800020f, 0x03020100, 0xf800021f, 0x03020100, 0xf800022f, 0x03020100, 0xf800023f, 0x03020100, 0xf80000cf, 0x03020100, 0xf80008d7, 0x03030303, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 19> FourParameterClipLayerVertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0x7e080280, 0xf800020f, 0x03020100, 0xf800021f, 0x03020100, 0xf800022f, 0x03020100, 0xf800023f, 0x03020100, 0xf80000cf, 0x03020100, 0xf80000d4, 0x04040404, 0xf80008e7, 0x03030303, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexValuePixelCode{
    0xc80a0000, 0xc80e0001, 0xc8120002, 0xf800180f, 0x04040302, 0xbf810000,
};

constexpr std::uint32_t MiscVector = 1u << 21u;
constexpr std::uint32_t ClipVector = 1u << 22u;
constexpr std::uint32_t ThreeClipDistances = 0x7u;
constexpr std::uint32_t VertexLayer = 1u << 18u;

struct VertexProgram {
    const char* name;
    std::span<const std::uint32_t> code;
    std::uint32_t paClVsOutCntl;
};

const VertexProgram Parameter{"param0 and position", ParameterVertexCode, 0u};
const VertexProgram Layer{"param0, position and layer", LayerVertexCode, MiscVector | VertexLayer};
const VertexProgram SecondParameter{"param0, param1 and position", SecondParameterVertexCode, 0u};
const VertexProgram Clip{"param0, position and three clip distances", ClipVertexCode, ClipVector | ThreeClipDistances};
const VertexProgram ClipLayer{"param0, position, three clip distances and layer", ClipLayerVertexCode, MiscVector | VertexLayer | ClipVector | ThreeClipDistances};
const VertexProgram FourParameterClip{"param0 to param3, position and three clip distances", FourParameterClipVertexCode, ClipVector | ThreeClipDistances};
const VertexProgram FourParameterClipLayer{"param0 to param3, position, three clip distances and layer", FourParameterClipLayerVertexCode, MiscVector | VertexLayer | ClipVector | ThreeClipDistances};

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

void Draw(AgcDriver::VulkanDevice& device, const VertexProgram& program) {
    Pixels.fill(Unwritten);
    auto target = device.Target();
    target.fragmentShaderBarycentricEnabled = false;
    target.fragmentBarycentricBuiltins = false;
    static const auto positions = ClipPositions();
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(positions.data(), 16u, static_cast<std::uint32_t>(positions.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(program.code.data()), std::as_bytes(program.code)}}};
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.paClVsOutCntl = program.paClVsOutCntl;
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(program.code.data()), program.code, 0, {}},
        {64u, 0, vertexUserData, std::nullopt, std::nullopt, vertexInfo, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.interpolatorCount = 1;
    pixel.inputAddr = PixelInputBit(PixelInput::PerspectiveCenter);
    pixel.hasPerspectiveCenterVgpr = true;
    pixel.executeOnNoop = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData;
    const std::span<const std::uint32_t> pixelCode(VertexValuePixelCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    Require(pixelResult.barycentricEmulation.active && pixelResult.fragmentParameters.size() == 1 && pixelResult.fragmentParameters[0].perVertex, std::string(program.name) + ": v_interp_mov of P10 and P20 did not emulate per-vertex parameters");
    const auto patches = ShaderRecompiler::BuildBarycentricTessellationShaders(vertexResult, pixelResult, target, device.MaxFragmentInputComponents(), 0u);
    const std::array<AgcDriver::Graphics::CompiledShader, 4> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::TessellationControl, &patches.control, 0},
        {ShaderStage::TessellationEvaluation, &patches.evaluation, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)},
    }};

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
    state.provokingVertexMode = VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    device.Draw(state, AgcDriver::Pm4::DrawParameters{0, static_cast<std::uint32_t>(positions.size()), 0, 1, 0, false}, shaders);
    device.WaitIdle();
}

double Edge(const std::array<double, 2>& a, const std::array<double, 2>& b, double x, double y) {
    return (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
}

std::array<double, 3> Weights(std::uint32_t x, std::uint32_t y) {
    std::array<std::array<double, 2>, 3> window{};
    for (std::size_t i = 0; i < Corners.size(); ++i) window[i] = {(Corners[i].x + 1.0) * Width / 2.0, (1.0 - Corners[i].y) * Height / 2.0};
    const double px = x + 0.5;
    const double py = y + 0.5;
    const double area = Edge(window[0], window[1], window[2][0], window[2][1]);
    return {Edge(window[1], window[2], px, py) / area, Edge(window[2], window[0], px, py) / area, Edge(window[0], window[1], px, py) / area};
}

std::string Check(const std::string& what) {
    const auto positions = ClipPositions();
    const auto p0 = positions[0][0];
    const std::array<float, 3> expected{positions[1][0] - p0, positions[2][0] - p0, p0};
    std::uint32_t inside = 0;
    std::uint32_t wrong = 0;
    std::uint32_t missing = 0;
    std::uint32_t outside = 0;
    std::string first;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto weights = Weights(x, y);
            const auto low = std::min({weights[0], weights[1], weights[2]});
            const auto* pixel = &Pixels[(y * Width + x) * 4u];
            const bool written = std::any_of(pixel, pixel + 4, [](float value) { return value != Unwritten; });
            if (low > 0.02) {
                ++inside;
                if (!written) {
                    ++missing;
                    if (first.empty()) first = "pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") inside the triangle is unwritten";
                    continue;
                }
                for (std::uint32_t channel = 0; channel < 3; ++channel) {
                    if (pixel[channel] == expected[channel]) continue;
                    ++wrong;
                    if (first.empty()) first = "pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(pixel[channel]) + ", expected " + std::to_string(expected[channel]);
                    break;
                }
            } else if (low < -0.02 && written) {
                ++outside;
                if (first.empty()) first = "pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") outside the triangle is written";
            }
        }
    }
    Require(inside > 200u, what + ": too few interior pixels were checked");
    if (missing == 0u && wrong == 0u && outside == 0u) return {};
    return what + ": " + std::to_string(missing) + " of " + std::to_string(inside) + " interior pixels unwritten, " + std::to_string(wrong) + " with wrong per-vertex values, " + std::to_string(outside) + " written outside the triangle; first: " + first;
}

std::unique_ptr<AgcDriver::VulkanDevice> OpenDevice() {
    auto device = OpenVulkanTestDevice();
    Require(device != nullptr, "the Vulkan device closed after the first open");
    return device;
}

std::uint32_t RunCase(const VertexProgram& seed, const VertexProgram& drawn) {
    const std::string name = std::string("a pipeline for ") + drawn.name + " after one for " + seed.name;
    std::uint32_t failures = 0;
    {
        auto fresh = OpenDevice();
        Draw(*fresh, drawn);
        const auto error = Check(name + ", fresh pipeline cache");
        if (!error.empty()) {
            std::cerr << error << '\n';
            ++failures;
        }
    }
    auto device = OpenDevice();
    Draw(*device, seed);
    auto error = Check(name + ", the seeding draw");
    if (!error.empty()) {
        std::cerr << error << '\n';
        ++failures;
    }
    Draw(*device, drawn);
    error = Check(name + ", pipeline cache seeded");
    if (!error.empty()) {
        std::cerr << error << '\n';
        ++failures;
    }
    if (failures == 0u) std::printf("barycentric patch pipeline passed for %s\n", name.c_str());
    return failures;
}

}

int main() {
    try {
        {
            const auto probe = OpenVulkanTestDevice();
            if (!probe) return VulkanTestSkipped;
            if (!probe->Target().tessellation.has_value()) {
                std::puts("skipped, the device lacks tessellationShader");
                return VulkanTestSkipped;
            }
        }
        std::uint32_t failures = 0;
        const std::array<std::array<const VertexProgram*, 2>, 4> pairs{{{&Layer, &Parameter}, {&SecondParameter, &Parameter}, {&ClipLayer, &Clip}, {&FourParameterClipLayer, &FourParameterClip}}};
        for (const auto& [extra, plain] : pairs) {
            failures += RunCase(*extra, *plain);
            failures += RunCase(*plain, *extra);
        }
        if (failures != 0u) {
            std::cerr << failures << " barycentric patch pipeline checks failed\n";
            return 1;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
