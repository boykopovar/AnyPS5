#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t Layers = 2;
constexpr std::uint8_t Kept = 0x40;
constexpr std::uint32_t LayerExport = (1u << 18u) | (1u << 21u);
constexpr std::uint32_t ShaderViewportIndexLayerCapability = 5254;

alignas(256) constexpr std::array<std::uint32_t, 106> GeometryCode{
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
    0xdbfc0410, 0x3000002b, 0xbf8cc07f, 0xf80000cf, 0x2f2e2d2c, 0xf80008d4, 0x00330000, 0xf800020f,
    0x33323130, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 6> LayerFieldPixelCode{
    0x2c020090, 0x7e020d01, 0x100202f0, 0xf800180f, 0x01010101, 0xbf810000,
};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 3> color;
    std::uint32_t layer;
};

alignas(256) std::array<Vertex, 6> Vertices{};

void BuildQuad() {
    const std::array<std::array<float, 2>, 6> corners{{{-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
    for (std::uint32_t k = 0; k < corners.size(); ++k) Vertices[k] = {{corners[k][0], corners[k][1], 0.5f, 1.0f}, {1.0f, 1.0f, 1.0f}, k % 3u == 2u ? 1u : 0u};
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

AgcDriver::Graphics::ColorTarget LayeredTarget(std::byte* memory) {
    AgcDriver::Graphics::ColorTarget color{reinterpret_cast<std::uintptr_t>(memory), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, 0, 0xe4u};
    color.tileMode = ColorTileMode::RenderTarget;
    color.depth = Layers;
    color.depthSlice = 0;
    color.layers = Layers;
    color.bytes = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::SurfaceForTarget(color)).guestBytes;
    return color;
}

class GuestVolume {
public:
    explicit GuestVolume(std::size_t bytes) {
#ifdef _WIN32
        block = static_cast<std::byte*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::byte*>(std::aligned_alloc(65536, bytes));
#endif
        Require(block != nullptr, "cannot allocate the layered target");
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
        std::memset(block, Kept, bytes);
    }

    ~GuestVolume() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestVolume(const GuestVolume&) = delete;
    GuestVolume& operator=(const GuestVolume&) = delete;
    std::byte* Data() const { return block; }

private:
    std::byte* block = nullptr;
};

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, bool last, std::span<const std::uint32_t> pixelCode, bool layerField) {
    const auto target = device.Target();
    const auto vertexBuffer = BufferDescriptor(Vertices.data(), sizeof(Vertex), static_cast<std::uint32_t>(Vertices.size()));
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Vertices.size()), 0, 1, 0, false};
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.paClVsOutCntl = LayerExport;
    std::vector<std::uint32_t> userData(12u, 0u);
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), userData.begin() + 8);
    const ShaderRecompiler::MeshConfiguration mesh{4u, 4u, 12u, 12u, 4u, 64u, 1024u, last ? 2u : 0u, 4u};
    const std::array<ShaderRecompiler::MemoryRegion, 1> geometryMemory{{{reinterpret_cast<std::uintptr_t>(GeometryCode.data()), std::as_bytes(std::span(GeometryCode))}}};
    ShaderRecompiler::RecompileRequest geometry{
        {ShaderStage::Mesh, reinterpret_cast<std::uintptr_t>(GeometryCode.data()), GeometryCode, 0, {}},
        {64, 0, userData, std::nullopt, std::nullopt, vertexInfo, geometryMemory},
        target,
        {0, 0, 0, ShaderRecompiler::MeshDrawPushOffsetBytes}
    };
    geometry.graphics = ShaderRecompiler::GraphicsCompileContext{0, {}, mesh, std::nullopt, {draw.indexAddress, draw.indexCount, draw.indexSize, draw.instanceCount}};
    geometry.useCache = false;
    const auto geometryResult = ShaderRecompiler::Recompile(geometry);
    const auto geometryPush = static_cast<std::uint32_t>(geometryResult.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    if (layerField) {
        pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::Ancillary);
        pixel.ancillary = true;
        pixel.layerExported = true;
    }
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(geometryPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Mesh, &geometryResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(geometryPush, target)}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Geometry, 0x20u, 64, 64, mesh, std::nullopt};
    state.provokingVertexMode = last ? VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT : VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;
    state.color = color;
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.renderLayers = Layers;
    state.layerExports = LayerExport;
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
    AgcDriver::Graphics::StorageTexture::FlushAllPending("mesh provoking layer test");
    device.WaitIdle();
}

std::uint64_t BlockOffset(const AgcDriver::Graphics::TextureSwizzleEquation& equation, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    std::uint64_t offset = 0;
    for (std::uint32_t bit = 0; bit < equation.bits.size(); ++bit) {
        const auto mask = equation.bits[bit];
        const auto selected = (x & (mask & 0xfffu)) ^ ((y << 12u) & (mask & 0xfff000u)) ^ ((z << 24u) & (mask & 0xff000000u));
        offset |= static_cast<std::uint64_t>(std::popcount(selected) & 1u) << bit;
    }
    return offset;
}

void Check(const std::byte* memory, const AgcDriver::Graphics::ColorTarget& color, std::uint32_t drawnLayer, std::uint8_t drawn, const std::string& what) {
    using namespace AgcDriver::Graphics;
    const auto geometry = DescribeSurface(SurfaceForTarget(color));
    const auto tileMode = ColorTextureTileMode(color.tileMode);
    const auto thin = ThinBlockLayout(tileMode, 4);
    const auto& mip = geometry.mips.at(0);
    const auto* equation = FindTextureSwizzleEquation(static_cast<std::uint32_t>(color.tileMode), 4);
    Require(equation != nullptr, "missing the target's swizzle equation");
    for (std::uint32_t layer = 0; layer < Layers; ++layer) {
        const auto* base = memory + geometry.GuestLayerOffset(layer);
        const auto expected = layer == drawnLayer ? drawn : Kept;
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                const auto* texel = base + (static_cast<std::uint64_t>(y / thin[2]) * mip.blocksPerRow + x / thin[1]) * thin[0] + BlockOffset(*equation, x, y, layer);
                for (std::uint32_t channel = 0; channel < 4; ++channel) {
                    const auto actual = std::to_integer<std::uint8_t>(texel[channel]);
                    Require(actual == expected || (layer == drawnLayer && expected == 128 && actual == 127), what + ": layer " + std::to_string(layer) + " texel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected) + (layer == drawnLayer ? " (drawn)" : " (untouched)"));
                }
            }
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        if (!target.mesh.has_value() || std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), ShaderViewportIndexLayerCapability) == target.supportedCapabilities.end()) {
            std::puts("skipped, the device has no VK_EXT_mesh_shader or VK_EXT_shader_viewport_index_layer");
            return VulkanTestSkipped;
        }
        if (device->DeviceName().starts_with("llvmpipe")) {
            std::puts("skipped, the device has no mesh shader compiler that handles these programs (llvmpipe)");
            return VulkanTestSkipped;
        }
        if (!device->ProvokingVertexLast()) {
            std::puts("skipped, the device has no last provoking vertex mode");
            return VulkanTestSkipped;
        }
        BuildQuad();
        const auto bytes = (LayeredTarget(nullptr).bytes + 65535u) & ~std::size_t{65535u};
        for (const bool layerField : {false, true}) {
            for (const bool last : {false, true}) {
                const GuestVolume memory(bytes);
                const auto color = LayeredTarget(memory.Data());
                Draw(*device, color, last, layerField ? std::span<const std::uint32_t>(LayerFieldPixelCode) : std::span<const std::uint32_t>(PixelCode), layerField);
                const auto drawn = static_cast<std::uint8_t>(layerField ? (last ? 128u : 0u) : 255u);
                Check(memory.Data(), color, last ? 1u : 0u, drawn, std::string(last ? "last provoking vertex: the layer of each triangle's third vertex" : "first provoking vertex: the layer of each triangle's first vertex") + (layerField ? ", read by the pixel shader" : ""));
            }
        }
        std::puts("mesh provoking layer tests passed: first mode draws each triangle into its first vertex's layer and last mode into its third vertex's");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
