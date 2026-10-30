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
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 48;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t Slices = 4;
constexpr std::uint32_t FirstSlice = 1;
constexpr std::uint32_t Layers = Slices - FirstSlice;
constexpr std::uint32_t BandWidth = Width / Layers;
constexpr std::uint8_t Kept = 0x40;
constexpr std::uint32_t LayerExport = (1u << 18u) | (1u << 21u);
constexpr std::uint32_t ShaderViewportIndexLayerCapability = 5254;

alignas(256) constexpr std::array<std::uint32_t, 10> VertexCode{
    0xe0382000, 0x80000005, 0xe030201c, 0x80000405, 0xbf8c3f70, 0xf80000cf, 0x03020100, 0xf80008d4, 0x00040000, 0xbf810000,
};

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

constexpr std::array<std::uint8_t, Layers> WhiteBands{255, 255, 255};
constexpr std::array<std::uint8_t, Layers> LayerFieldBands{0, 128, 255};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 3> color;
    std::uint32_t layer;
};

alignas(256) std::array<Vertex, 6 * Layers> Vertices{};

constexpr ShaderRecompiler::MeshConfiguration Subgroup{4u, 4u, 12u, 12u, 4u, 64u, 1024u, 0u, 4u};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

void BuildBands() {
    for (std::uint32_t layer = 0; layer < Layers; ++layer) {
        const float left = -1.0f + 2.0f * static_cast<float>(layer) / Layers;
        const float right = -1.0f + 2.0f * static_cast<float>(layer + 1u) / Layers;
        const std::array<std::array<float, 2>, 6> corners{{{left, -1.0f}, {right, -1.0f}, {left, 1.0f}, {right, -1.0f}, {right, 1.0f}, {left, 1.0f}}};
        for (std::uint32_t k = 0; k < corners.size(); ++k) Vertices[6u * layer + k] = {{corners[k][0], corners[k][1], 0.5f, 1.0f}, {1.0f, 1.0f, 1.0f}, layer};
    }
}

AgcDriver::Graphics::ColorTarget VolumeTarget(std::byte* volume, ColorTileMode tileMode = ColorTileMode::RenderTarget) {
    AgcDriver::Graphics::ColorTarget color{reinterpret_cast<std::uintptr_t>(volume), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, 0, 0xe4u};
    color.tileMode = tileMode;
    color.depth = Slices;
    color.depthSlice = FirstSlice;
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
        Require(block != nullptr, "cannot allocate the volume");
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

bool DeclaresLayer(std::span<const std::uint32_t> words) {
    for (std::size_t at = 5; at < words.size();) {
        const auto length = words[at] >> 16u;
        const auto opcode = words[at] & 0xffffu;
        if (length == 0 || at + length > words.size()) break;
        if (opcode == 71u && length == 4 && words[at + 2] == 11u && words[at + 3] == 9u) return true;
        if (opcode == 72u && length == 5 && words[at + 3] == 11u && words[at + 4] == 9u) return true;
        at += length;
    }
    return false;
}

void Run(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, bool mesh, std::span<const std::uint32_t> pixelCode, bool layerField) {
    const auto target = device.Target();
    const auto vertexBuffer = BufferDescriptor(Vertices.data(), sizeof(Vertex), static_cast<std::uint32_t>(Vertices.size()));
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Vertices.size()), 0, 1, 0, false};
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.paClVsOutCntl = LayerExport;
    std::vector<std::uint32_t> userData(mesh ? 12u : 4u, 0u);
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), userData.begin() + (mesh ? 8 : 0));
    const std::span<const std::uint32_t> code = mesh ? std::span<const std::uint32_t>(GeometryCode) : std::span<const std::uint32_t>(VertexCode);
    const auto stage = mesh ? ShaderStage::Mesh : ShaderStage::Vertex;
    const std::array<ShaderRecompiler::MemoryRegion, 1> geometryMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    ShaderRecompiler::RecompileRequest geometry{
        {stage, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, std::nullopt, std::nullopt, vertexInfo, geometryMemory},
        target,
        {0, 0, 0, mesh ? ShaderRecompiler::MeshDrawPushOffsetBytes : 64u}
    };
    if (mesh) geometry.graphics = ShaderRecompiler::GraphicsCompileContext{0, {}, Subgroup, std::nullopt, {draw.indexAddress, draw.indexCount, draw.indexSize, draw.instanceCount}};
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
        {stage, &geometryResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(geometryPush, target)}
    }};

    AgcDriver::Graphics::State state{};
    if (mesh) state.stages = {AgcDriver::Graphics::ShaderPath::Geometry, 0x20u, 64, 64, Subgroup, std::nullopt};
    else state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64, 64, std::nullopt, std::nullopt};
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
    if (mesh) {
        Require(DeclaresLayer(geometryResult.spirv), "the mesh program declares no Layer output");
        VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
        subgroup.subgroupSize = target.subgroupSize;
        subgroup.supportedStages = VK_SHADER_STAGE_ALL;
        subgroup.supportedOperations = 0xffu;
        subgroup.quadOperationsInAllStages = VK_TRUE;
        AgcDriver::Graphics::ValidateShaders(shaders, state, subgroup, false, false, false, false, false, false, false, false, true);
        return;
    }
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushAllPending("layered volume draw test");
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

void Check(const std::byte* volume, const AgcDriver::Graphics::ColorTarget& color, const std::string& what, const std::array<std::uint8_t, Layers>& bands) {
    using namespace AgcDriver::Graphics;
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::SurfaceForTarget(color));
    Require(geometry.thick == (color.tileMode == ColorTileMode::S4KBX), "unexpected volume addressing family");
    const auto tileMode = ColorTextureTileMode(color.tileMode);
    const auto thin = ThinBlockLayout(tileMode, 4);
    const auto block = geometry.thick ? ThickBlockExtent(tileMode, 4) : std::array<std::uint32_t, 3>{thin[1], thin[2], 1u};
    const auto& mip = geometry.mips.at(0);
    const auto* equation = FindTextureSwizzleEquation((geometry.thick ? 0x100u : 0u) | static_cast<std::uint32_t>(color.tileMode), 4);
    Require(equation != nullptr, "missing the volume's swizzle equation");
    for (std::uint32_t slice = 0; slice < Slices; ++slice) {
        const auto* base = volume + geometry.GuestLayerOffset(slice);
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                const bool drawn = slice >= FirstSlice && x / BandWidth == slice - FirstSlice;
                const auto expected = drawn ? bands[slice - FirstSlice] : Kept;
                const auto offset = (static_cast<std::uint64_t>(y / block[1]) * mip.blocksPerRow + x / block[0]) * thin[0] + BlockOffset(*equation, x, y, slice);
                const auto* texel = base + offset;
                for (std::uint32_t channel = 0; channel < 4; ++channel) {
                    const auto actual = std::to_integer<std::uint8_t>(texel[channel]);
                    Require(actual == expected || (drawn && expected == 128 && actual == 127), what + ": slice " + std::to_string(slice) + " texel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected) + (drawn ? " (the band of layer " + std::to_string(slice - FirstSlice) + ")" : slice < FirstSlice ? " (outside the color view)" : " (another layer's band)"));
                }
            }
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        const bool directReadback = std::getenv("APS5_TEST_DIRECT_READBACK") != nullptr;
        if (directReadback) {
            std::remove("private-readback.ready");
            char extent[64];
            std::snprintf(extent, sizeof(extent), "%ux%ux%u", Width, Height, Slices);
#ifdef _WIN32
            Require(_putenv_s("APS5_DUMP_STORAGE", extent) == 0 && _putenv_s("APS5_DUMP_STORAGE_GATE", "private-readback.ready") == 0, "cannot set private readback options");
#else
            Require(setenv("APS5_DUMP_STORAGE", extent, 1) == 0 && setenv("APS5_DUMP_STORAGE_GATE", "private-readback.ready", 1) == 0, "cannot set private readback options");
#endif
        }
        const bool vertexOnly = argc == 2 && std::strcmp(argv[1], "--vertex-only") == 0;
        Require(argc == 1 || vertexOnly, "usage: layered volume draw [--vertex-only]");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        if (std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), ShaderViewportIndexLayerCapability) == target.supportedCapabilities.end()) {
            std::puts("skipped, the device has no VK_EXT_shader_viewport_index_layer");
            return VulkanTestSkipped;
        }
        BuildBands();
        const GuestVolume volume((VolumeTarget(nullptr).bytes + 65535u) & ~std::size_t{65535u});
        const auto color = VolumeTarget(volume.Data());
        Run(*device, color, false, PixelCode, false);
        Check(volume.Data(), color, "vertex draw", WhiteBands);
        if (directReadback) {
            char name[80];
            std::snprintf(name, sizeof(name), "storage_direct_%llx_0.raw", static_cast<unsigned long long>(color.address));
            std::remove(name);
            AgcDriver::Graphics::StorageTexture::FlushDumpSelected("closed readback gate fixture");
            Require(!std::filesystem::exists(name), "private readback ignored its closed gate");
            auto* trigger = std::fopen("private-readback.ready", "wb");
            Require(trigger != nullptr, "cannot create private readback trigger");
            Require(std::fclose(trigger) == 0, "cannot close private readback trigger");
            AgcDriver::Graphics::StorageTexture::FlushDumpSelected("private readback fixture");
            std::remove("private-readback.ready");
            auto* file = std::fopen(name, "rb");
            Require(file != nullptr, "clean volume image was not exported by direct readback");
            std::array<std::uint32_t, 3> header{};
            Require(std::fread(header.data(), sizeof(header), 1, file) == 1, "private readback header is incomplete");
            Require(header[0] >= Width && header[1] == Height && header[2] == VK_FORMAT_R8G8B8A8_UNORM, "private readback header disagrees with the volume");
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(header[0]) * Height * Slices * 4u);
            Require(std::fread(pixels.data(), 1, pixels.size(), file) == pixels.size(), "private readback pixels are incomplete");
            Require(std::fgetc(file) == EOF, "private readback has unexpected trailing pixels");
            Require(std::fclose(file) == 0, "cannot close private readback");
            for (std::uint32_t z = 0; z < Slices; ++z) {
                for (std::uint32_t y = 0; y < Height; ++y) {
                    for (std::uint32_t x = 0; x < Width; ++x) {
                        const bool drawn = z >= FirstSlice && x / BandWidth == z - FirstSlice;
                        const auto expected = drawn ? WhiteBands[z - FirstSlice] : Kept;
                        for (std::uint32_t channel = 0; channel < 4; ++channel) Require(pixels[((z * Height + y) * header[0] + x) * 4u + channel] == expected, "private readback changed a volume texel");
                    }
                }
            }
            std::puts("private clean-volume direct readback pixels verified");
            return 0;
        }
        const GuestVolume fieldVolume((VolumeTarget(nullptr).bytes + 65535u) & ~std::size_t{65535u});
        const auto fieldColor = VolumeTarget(fieldVolume.Data());
        Run(*device, fieldColor, false, LayerFieldPixelCode, true);
        Check(fieldVolume.Data(), fieldColor, "a pixel shader reading the render target index of its packed ancillary word", LayerFieldBands);
        const GuestVolume thickVolume((VolumeTarget(nullptr, ColorTileMode::S4KBX).bytes + 65535u) & ~std::size_t{65535u});
        const auto thickColor = VolumeTarget(thickVolume.Data(), ColorTileMode::S4KBX);
        Run(*device, thickColor, false, PixelCode, false);
        Check(thickVolume.Data(), thickColor, "thick SW_4KB_S_X vertex draw and writeback", WhiteBands);
        if (vertexOnly) std::puts("layered volume vertex draw tests passed");
        else if (target.mesh.has_value()) Run(*device, color, true, PixelCode, false);
        else std::puts("mesh case skipped, the device has no VK_EXT_mesh_shader");
        std::puts("layered volume draw tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
