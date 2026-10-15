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
#include <string_view>
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

alignas(256) constexpr std::array<std::uint32_t, 104> MeshCode{
    0x8f6a9003, 0x94fe6ac1, 0xbf88000b, 0xd7650006, 0x000100c1, 0xd7660006, 0x00020cc1, 0x93ebff03,
    0x00040018, 0xd7460006, 0x04190c6b, 0x340c0c82, 0xd8340000, 0x00000506, 0xbf8cc07f, 0xbefe04c1,
    0xbf8a0000, 0x938dff02, 0x00090016, 0x9382ff03, 0x00040018, 0x938cff03, 0x00080008, 0xd7650009,
    0x000100c1, 0xd7660009, 0x000212c1, 0xd746000a, 0x04250c02, 0x7da8120c, 0xbf88002e, 0x361600ff,
    0x0000ffff, 0x2c180090, 0x361a02ff, 0x0000ffff, 0xd8d80000, 0x0b00000b, 0xd8d80000, 0x0c00000c,
    0xd8d80000, 0x0d00000d, 0xbf8cc07f, 0xe0382000, 0x8002100b, 0xe0382010, 0x8002140b, 0xe0382000,
    0x8002180c, 0xe0382010, 0x80021c0c, 0xe0382000, 0x8002200d, 0xe0382010, 0x8002240d, 0x161c1483,
    0x161e14ff, 0x00000060, 0xbf8c3f70, 0xdb7c0400, 0x0000100f, 0xdb7c0410, 0x0000140f, 0xdb7c0420,
    0x0000180f, 0xdb7c0430, 0x00001c0f, 0xdb7c0440, 0x0000200f, 0xdb7c0450, 0x0000240f, 0x4a501c81,
    0x4a521c82, 0x3450508a, 0x34525294, 0xd772002a, 0x04a6510e, 0xbf8cc07f, 0xbefe04c1, 0xbf8a0000,
    0x930e830d, 0xbf078002, 0xbf850003, 0x8f0f8c0d, 0x887c0f0e, 0xbf900009, 0x7da8140d, 0xbf880002,
    0xf8000941, 0x0000002a, 0xbefe04c1, 0x7da8140e, 0xbf88000a, 0x34561485, 0xdbfc0400, 0x2c00002b,
    0xdbfc0410, 0x3000002b, 0xbf8cc07f, 0xf80008cf, 0x2f2e2d2c, 0xf800020f, 0x33323130, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 3> BarycentricPixelCode{
    0xf800180f, 0x01000100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexValuePixelCode{
    0xc80a0000, 0xc80e0001, 0xc8120002, 0xf800180f, 0x04040302, 0xbf810000,
};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 4> color;
};

struct Corner {
    float x;
    float y;
    float w;
};

constexpr std::array<float, 3> VertexValues{0.25f, -3.5f, 7.75f};

constexpr std::array<Corner, 3> Corners{{{-0.8f, -0.8f, 1.0f}, {0.9f, -0.6f, 2.5f}, {-0.5f, 0.85f, 0.5f}}};

alignas(256) std::array<Vertex, 3> Vertices{};

std::array<std::uint32_t, 4> VertexBufferDescriptor(const void* vertices, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(vertices);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (32u << 16u), count, 0x01016facu};
}

constexpr ShaderRecompiler::MeshConfiguration SmallSubgroup{4u, 4u, 12u, 12u, 4u, 64u, 1024u, 0u, 4u};

template<typename TAction>
void ExpectRejected(TAction action, const std::string& reason) {
    try {
        action();
    } catch (const std::exception& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("the validator rejected the draw for another reason than \"") + reason + "\": " + error.what());
        return;
    }
    Require(false, "the validator accepted a draw it must reject (" + reason + ")");
}

std::uint32_t DecorationWord(const ShaderRecompiler::RecompileResult& result, spv::Decoration decoration) {
    const auto& words = result.spirv.Words();
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        if ((words[cursor] & 0xffffu) == spv::OpDecorate && count == 3u && words[cursor + 2] == decoration) return static_cast<std::uint32_t>(cursor + 2);
        cursor += count;
    }
    return 0u;
}

void CheckValidator(AgcDriver::VulkanDevice& device, const AgcDriver::Pm4::DrawParameters& draw, const AgcDriver::Graphics::State& state, const ShaderRecompiler::RecompileResult& meshResult, const ShaderRecompiler::RecompileResult& pixelResult, std::uint32_t pixelOffset) {
    const auto rejected = [&](const ShaderRecompiler::RecompileResult& mesh, const ShaderRecompiler::RecompileResult& pixel, const std::string& reason) {
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderStage::Mesh, &mesh, 0}, {ShaderStage::Fragment, &pixel, pixelOffset}}};
        ExpectRejected([&] { device.Draw(state, draw, shaders); }, reason);
    };
    auto unreserved = pixelResult;
    unreserved.barycentricEmulation.active = false;
    rejected(meshResult, unreserved, "per-primitive");
    auto shifted = pixelResult;
    shifted.fragmentParameters.push_back({0u, 0u, false, false});
    rejected(meshResult, shifted, "per-primitive");
    auto plain = pixelResult;
    const auto decorate = DecorationWord(plain, spv::DecorationPerPrimitiveEXT);
    Require(decorate != 0u, "the pixel module has no per-primitive input");
    plain.spirv[decorate] = spv::DecorationFlat;
    rejected(meshResult, plain, "mesh shading");
}

ShaderRecompiler::RecompileResult Draw(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> pixelCode, std::uint32_t inputs) {
    Pixels.fill(Unwritten);
    auto target = device.Target();
    target.fragmentShaderBarycentricEnabled = false;

    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Vertices.size()), 0, 1, 0, false};
    std::vector<std::uint32_t> meshUserData(12, 0u);
    const auto index = AgcDriver::Graphics::MeshIndexBufferDescriptor(draw);
    std::copy(index.begin(), index.end(), meshUserData.begin() + ShaderRecompiler::MeshIndexBufferUserWord);
    const auto vertexBuffer = VertexBufferDescriptor(Vertices.data(), static_cast<std::uint32_t>(Vertices.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), meshUserData.begin() + 8);
    const std::array<ShaderRecompiler::MemoryRegion, 1> meshMemory{{{reinterpret_cast<std::uintptr_t>(MeshCode.data()), std::as_bytes(std::span(MeshCode))}}};
    ShaderRecompiler::RecompileRequest mesh{
        {ShaderStage::Mesh, reinterpret_cast<std::uintptr_t>(MeshCode.data()), MeshCode, 0, {}},
        {64u, 0, meshUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, meshMemory},
        target,
        {0, 0, 0, ShaderRecompiler::MeshDrawPushOffsetBytes},
        ShaderRecompiler::GraphicsCompileContext{0, {}, SmallSubgroup, std::nullopt, {draw.indexAddress, draw.indexCount, draw.indexSize, draw.instanceCount}}
    };
    mesh.useCache = false;
    const auto meshResult = ShaderRecompiler::Recompile(mesh);
    const auto meshPush = static_cast<std::uint32_t>(meshResult.pushConstants.size());

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
        PixelPushLayout(meshPush, target),
        ShaderRecompiler::GraphicsCompileContext{0, {}, SmallSubgroup, std::nullopt, {draw.indexAddress, draw.indexCount, draw.indexSize, draw.instanceCount}}
    };
    fragment.useCache = false;
    auto pixelResult = ShaderRecompiler::Recompile(fragment);
    Require(pixelResult.barycentricEmulation.active, "the pixel shader did not emulate barycentrics");
    mesh.graphics->meshBarycentrics = ShaderRecompiler::MeshBarycentricLayoutOf(pixelResult);
    const auto pairedMesh = ShaderRecompiler::Recompile(mesh);
    if (pixelCode.data() == BarycentricPixelCode.data()) {
        auto last = mesh;
        last.graphics->mesh->provokingVertex = 1u;
        ExpectRejected([&] { static_cast<void>(ShaderRecompiler::Recompile(last)); }, "first provoking vertex");
        auto wide = mesh;
        wide.graphics->mesh->maxVertices = target.mesh->maxSharedMemoryBytes / 16u + 1u;
        ExpectRejected([&] { static_cast<void>(ShaderRecompiler::Recompile(wide)); }, "mesh shared memory");
    }
    Require(pairedMesh.meshBarycentrics == mesh.graphics->meshBarycentrics, "the paired mesh does not record the layout it was compiled for");
    Require(pairedMesh.pushConstants.size() == meshPush && pairedMesh.parameterExports == meshResult.parameterExports, "the paired mesh changed the push constants or exports the pixel shader was built for");

    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Mesh, &pairedMesh, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(meshPush, target)}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Geometry, 0x20u, 64u, 64u, SmallSubgroup, std::nullopt};
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
    if (pixelCode.data() == BarycentricPixelCode.data()) CheckValidator(device, draw, state, pairedMesh, pixelResult, PixelPushOffset(meshPush, target));
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

bool MatchesEither(float value, float unsnapped, float snapped) {
    return std::fabs(value - unsnapped) <= 1e-4f || std::fabs(value - snapped) <= 1e-4f;
}

float Pixel(std::uint32_t x, std::uint32_t y, std::uint32_t channel) {
    return Pixels[(y * Width + x) * 4u + channel];
}

std::uint32_t CheckVertexValues() {
    const std::array<float, 3> expected{VertexValues[1] - VertexValues[0], VertexValues[2] - VertexValues[0], VertexValues[0]};
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            if (!Barycentrics(x, y, false).inside) continue;
            for (std::uint32_t channel = 0; channel < 3; ++channel) {
                Require(Pixel(x, y, channel) == expected[channel], "v_interp_mov: pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(Pixel(x, y, channel)) + ", expected " + std::to_string(expected[channel]));
            }
            ++checked;
        }
    }
    Require(checked > 200u, "v_interp_mov: too few interior pixels were checked");
    return checked;
}

std::uint32_t CheckBarycentrics(const std::string& what, bool perspective) {
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = Barycentrics(x, y, false);
            if (!expected.inside) continue;
            const auto snapped = Barycentrics(x, y, true);
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

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!device->Target().mesh.has_value()) {
            std::puts("skipped, the device has no VK_EXT_mesh_shader");
            return VulkanTestSkipped;
        }
        if (device->DeviceName().starts_with("llvmpipe")) {
            std::puts("skipped, the device has no mesh shader compiler that handles these programs (llvmpipe)");
            return VulkanTestSkipped;
        }
        for (std::size_t i = 0; i < Corners.size(); ++i) {
            const auto& corner = Corners[i];
            Vertices[i] = {{corner.x * corner.w, corner.y * corner.w, 0.5f * corner.w, corner.w}, {VertexValues[i], 0.0f, 0.0f, 1.0f}};
        }
        const auto perspective = PixelInputBit(PixelInput::PerspectiveCenter);
        const auto linear = PixelInputBit(PixelInput::LinearCenter);
        auto result = Draw(*device, BarycentricPixelCode, perspective);
        Require(result.barycentricEmulation.smooth && !result.barycentricEmulation.linear, "reading PERSP_CENTER I/J did not emulate perspective barycentrics");
        const auto smooth = CheckBarycentrics("mesh PERSP_CENTER I/J", true);
        result = Draw(*device, BarycentricPixelCode, linear);
        Require(!result.barycentricEmulation.smooth && result.barycentricEmulation.linear, "reading LINEAR_CENTER I/J did not emulate linear barycentrics");
        const auto noPerspective = CheckBarycentrics("mesh LINEAR_CENTER I/J", false);
        result = Draw(*device, VertexValuePixelCode, perspective);
        Require(result.barycentricEmulation.active && result.fragmentParameters.size() == 1 && result.fragmentParameters[0].perVertex, "v_interp_mov of P10 and P20 did not emulate per-vertex parameters");
        const auto vertexValues = CheckVertexValues();
        std::printf("mesh barycentric emulation tests passed: %u perspective and %u linear I/J pixels, %u raw vertex value pixels\n", smooth, noPerspective, vertexValues);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
