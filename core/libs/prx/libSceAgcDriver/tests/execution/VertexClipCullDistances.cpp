#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 192;
constexpr std::uint32_t Height = 128;
constexpr std::uint32_t DistanceVector = 0x00400000u;
constexpr std::uint32_t CullDistanceEnables = 0x0000ff00u;
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 10> VertexCode{
    0xe0382000, 0x80000005, 0xe0382010, 0x80000405, 0xbf8c3f70,
    0xf80000cf, 0x03020100, 0xf80008df, 0x07060504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 106> MeshCode{
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
    0xf8000941, 0x0000002a, 0xbefe04c1, 0x7da8140e, 0xbf88000c, 0x34561485, 0xdbfc0400, 0x2c00002b,
    0xdbfc0410, 0x3000002b, 0xbf8cc07f, 0xf80000cf, 0x2f2e2d2c, 0xf80008df, 0x33323130, 0xf800020f,
    0x33323130, 0xbf810000,
};

constexpr ShaderRecompiler::MeshConfiguration Subgroup{4u, 4u, 12u, 12u, 4u, 64u, 1024u, 0u, 4u};

alignas(256) constexpr std::array<std::uint32_t, 7> PixelCode{
    0x7e0002f2, 0x7e0202f2, 0x7e0402f2, 0x7e0602f2, 0xf800080f, 0x03020100, 0xbf810000,
};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 4> distances;
};

constexpr std::array<float, 3> PositionX{-3.0f, 1.0f, 1.0f};
constexpr std::array<float, 3> PositionY{-1.0f, -1.0f, 3.0f};
constexpr std::array<float, 3> Negative{-1.0f, -1.0f, -1.0f};
constexpr std::array<float, 3> Mixed{-1.0f, 1.0f, -1.0f};

enum class Visible { All, None, X, Y, XAndY };

struct Row {
    const char* name;
    std::uint32_t control;
    Visible visible;
    std::array<float, 3> component0;
    std::array<float, 3> component1;
    std::array<float, 3> component2;
};

const std::array<Row, 7> Rows{{
    {"no enable bits", DistanceVector, Visible::All, Negative, Negative, Negative},
    {"clip plane 0 from component 0", DistanceVector | 0x1u, Visible::X, PositionX, Negative, Negative},
    {"clip plane 1 alone, from component 1", DistanceVector | 0x2u, Visible::Y, Negative, PositionY, Negative},
    {"clip planes 0x5, components 0 and 2", DistanceVector | 0x5u, Visible::XAndY, PositionX, Negative, PositionY},
    {"cull plane 0, every vertex negative", DistanceVector | 0x100u, Visible::None, Negative, Negative, Negative},
    {"cull plane 0, one vertex not negative", DistanceVector | 0x100u, Visible::All, Mixed, Negative, Negative},
    {"cull plane 0 from component 2, sparse 0x4", DistanceVector | 0x400u, Visible::None, Mixed, Negative, Negative},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

bool Expected(Visible visible, std::uint32_t pixel) {
    const double x = -1.0 + 2.0 * ((pixel % Width) + 0.5) / Width;
    const double y = 1.0 - 2.0 * ((pixel / Width) + 0.5) / Height;
    switch (visible) {
        case Visible::All: return true;
        case Visible::None: return false;
        case Visible::X: return x >= 0.0;
        case Visible::Y: return y >= 0.0;
        case Visible::XAndY: return x >= 0.0 && y >= 0.0;
    }
    return false;
}

void Draw(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const Row& row, bool mesh) {
    Pixels.fill(std::byte{0});
    std::array<Vertex, 3> vertices{};
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        vertices[i] = {{PositionX[i], PositionY[i], 0.5f, 1.0f}, {row.component0[i], row.component1[i], row.component2[i], 0.0f}};
    }

    std::vector<std::uint32_t> vertexUserData(mesh ? 12u : 4u, 0u);
    const auto vertexBuffer = BufferDescriptor(vertices.data(), sizeof(Vertex), static_cast<std::uint32_t>(vertices.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin() + (mesh ? 8 : 0));
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(vertices.size()), 0, 1, 0, false};
    const std::span<const std::uint32_t> code = mesh ? std::span<const std::uint32_t>(MeshCode) : std::span<const std::uint32_t>(VertexCode);
    const auto stage = mesh ? ShaderStage::Mesh : ShaderStage::Vertex;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.paClVsOutCntl = row.control;
    ShaderRecompiler::RecompileRequest vertex{
        {stage, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, vertexInfo, vertexMemory},
        device.Target(),
        {0, 0, 0, mesh ? ShaderRecompiler::MeshDrawPushOffsetBytes : 64u}
    };
    if (mesh) vertex.graphics = ShaderRecompiler::GraphicsCompileContext{0, {}, Subgroup, std::nullopt, {draw.indexAddress, draw.indexCount, draw.indexSize, draw.instanceCount}};
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = waveSize == 32u;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    const std::vector<std::uint32_t> pixelUserData(4, 0u);
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {waveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        device.Target(),
        PixelPushLayout(vertexPush, device.Target())
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {stage, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, device.Target())}
    }};

    AgcDriver::Graphics::State state{};
    if (mesh) state.stages = {AgcDriver::Graphics::ShaderPath::Geometry, 0x20u, waveSize, waveSize, Subgroup, std::nullopt};
    else state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.paClVsOutCntl = row.control;
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
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Check(const Row& row, std::uint32_t waveSize, bool mesh) {
    const auto what = std::string(mesh ? "mesh" : "vertex") + " clip and cull distances, " + row.name + ", wave" + std::to_string(waveSize);
    for (std::uint32_t pixel = 0; pixel < Width * Height; ++pixel) {
        const auto value = std::to_integer<std::uint8_t>(Pixels[pixel * 4u]);
        Require(value == 0u || value == 255u, what + ": pixel " + std::to_string(pixel) + " stored " + std::to_string(value));
        const bool written = value == 255u;
        const bool expected = Expected(row.visible, pixel);
        Require(written == expected, what + ": pixel (" + std::to_string(pixel % Width) + ", " + std::to_string(pixel / Width) + ") is " + (written ? "written" : "clear") + ", expected " + (expected ? "written" : "clear"));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32u) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        const bool cullDistance = TargetHasCapability(device->Target(), spv::CapabilityCullDistance);
        if (!cullDistance) std::puts("cull distance cases skipped, the device has no shaderCullDistance");
        const auto runs = [&](const Row& row) { return cullDistance || (row.control & CullDistanceEnables) == 0u; };
        for (const auto waveSize : {64u, 32u}) {
            for (const auto& row : Rows) {
                if (!runs(row)) continue;
                Draw(*device, waveSize, row, false);
                Check(row, waveSize, false);
            }
        }
        if (device->Target().mesh.has_value()) {
            for (const auto& row : Rows) {
                if (!runs(row)) continue;
                Draw(*device, 64u, row, true);
                Check(row, 64u, true);
            }
        } else {
            std::puts("mesh case skipped, the device has no VK_EXT_mesh_shader");
        }
        std::puts("vertex clip and cull distance tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
