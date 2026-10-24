#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

// Layered draws (a vertex shader exporting the render target index) into a color view of several
// slices: (a) slices 1 to 3 of a four-slice 2D array, (b) slices 0 to 2 of a surface of one slice.
// Each layer is filled with its own value; after the write-back every slice of guest memory must hold
// its layer's value, and the slices outside the view their initial bytes.

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Side = 128;
constexpr std::uint32_t Slices = 4;
constexpr std::size_t SliceBytes = 65536;
constexpr std::size_t SurfaceBytes = SliceBytes * Slices;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Color32FloatInfo = (4u << 2u) | (7u << 8u);
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint8_t Float32Export = 9;
constexpr std::uint32_t LayerExport = (1u << 18u) | (1u << 21u);
constexpr std::uint32_t ShaderViewportIndexLayerCapability = 5254;
constexpr std::uint32_t Kept = 0x5a5a5a5au;

// Position from the buffer at offset 0, the layer from offset 28 into the render target index.
alignas(256) constexpr std::array<std::uint32_t, 10> VertexCode{
    0xe0382000, 0x80000005, 0xe030201c, 0x80000405, 0xbf8c3f70, 0xf80000cf, 0x03020100, 0xf80008d4, 0x00040000, 0xbf810000,
};

// 1 + layer / 2, the layer read from the render target index field of the ancillary word.
alignas(256) constexpr std::array<std::uint32_t, 7> LayerValuePixelCode{
    0x2c020090, 0x7e020d01, 0x100202f0, 0x060202f2, 0xf800180f, 0x01010101, 0xbf810000,
};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 3> color;
    std::uint32_t layer;
};

alignas(256) std::array<Vertex, 6 * Slices> Vertices{};

float LayerValue(std::uint32_t layer) {
    return 1.0f + static_cast<float>(layer) / 2.0f;
}

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, SliceBytes));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
        void* mapped = mmap(nullptr, SurfaceBytes + SliceBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + SliceBytes - 1) & ~(static_cast<std::uintptr_t>(SliceBytes) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + SurfaceBytes != begin + SurfaceBytes + SliceBytes) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + SliceBytes - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, SurfaceBytes);
        }
#endif
        Require(block != nullptr, "layered array color draw: cannot allocate the color array");
        GuestAllocations::Mutation().Add(block, SurfaceBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, SurfaceBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, SurfaceBytes);
#else
        munmap(block, SurfaceBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, SurfaceBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

void BuildQuads(std::uint32_t layers) {
    Vertices = {};
    const std::array<std::array<float, 2>, 6> corners{{{-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
    for (std::uint32_t layer = 0; layer < layers; ++layer) {
        for (std::uint32_t k = 0; k < corners.size(); ++k) Vertices[6u * layer + k] = {{corners[k][0], corners[k][1], 0.5f, 1.0f}, {1.0f, 1.0f, 1.0f}, layer};
    }
}

// A CB_COLOR_VIEW of slices first..first + layers - 1 of a surface whose last array slice is lastArray.
AgcDriver::Graphics::ColorTarget DecodeColor(std::uint64_t address, std::uint32_t lastArray, std::uint32_t first, std::uint32_t layers) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(address >> 8u);
    cx[0x31b] = first | ((first + layers - 1u) << 13u);
    cx[0x31c] = Color32FloatInfo;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Side - 1u) << 14u) | (Side - 1u);
    cx[0x3b8] = (TiledAttrib3 & ~((0x1fu << 14u) | 0x1fffu)) | (TileR64KBX << 14u) | lastArray;
    cx[0x390] = static_cast<std::uint32_t>(address >> 40u);
    cx[0x207] = LayerExport;
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    const auto arrayLast = std::max(lastArray, first + layers - 1u);
    Require(color.layers == layers && color.arraySlice == first && color.arrayLastSlice == arrayLast && color.arrayAddress == address, "the color view did not decode as " + std::to_string(layers) + " layers from slice " + std::to_string(first) + " of an array whose last slice is " + std::to_string(arrayLast));
    return color;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    const auto vertexCount = 6u * color.layers;
    BuildQuads(color.layers);
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = AddressOf(Vertices.data());
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (static_cast<std::uint32_t>(sizeof(Vertex)) << 16u), vertexCount, 0x01016facu};
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.paClVsOutCntl = LayerExport;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, vertexInfo, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xf;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Side, Side};
    state.renderLayers = color.layers;
    state.layerExports = LayerExport;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Side), static_cast<float>(Side), -static_cast<float>(Side), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Side, Side}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.targetOutputMode[0] = Float32Export;
    pixel.targetExportMapping = AgcDriver::Graphics::ExportMappings(state);
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::Ancillary);
    pixel.ancillary = true;
    pixel.layerExported = true;
    const std::span<const std::uint32_t> pixelCode(LayerValuePixelCode);
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{codeAddress, std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, codeAddress, pixelCode, 0, {}},
        {waveSize, 0, {}, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, vertexCount, 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

// Draws into slices first..first + layers - 1 and checks every slice of the four in guest memory.
void Run(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::uint32_t lastArray, std::uint32_t first, std::uint32_t layers, const std::string& what) {
    const auto surface = AddressOf(texels);
    std::fill_n(reinterpret_cast<std::uint32_t*>(texels), SurfaceBytes / 4u, Kept);
    AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Draw(device, DecodeColor(surface, lastArray, first, layers));
    AgcDriver::Graphics::StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "layered array color draw test");
    device.WaitIdle();
    for (std::uint32_t slice = 0; slice < Slices; ++slice) {
        const bool drawn = slice >= first && slice < first + layers;
        const auto expected = drawn ? std::bit_cast<std::uint32_t>(LayerValue(slice - first)) : Kept;
        const auto* words = reinterpret_cast<const std::uint32_t*>(texels + slice * SliceBytes);
        for (std::size_t word = 0; word < SliceBytes / 4u; ++word) {
            if (words[word] == expected) continue;
            char actual[16];
            std::snprintf(actual, sizeof(actual), "0x%08x", words[word]);
            Require(false, what + ": slice " + std::to_string(slice) + " word " + std::to_string(word) + " is " + actual + ", expected " + (drawn ? std::to_string(LayerValue(slice - first)) + " (layer " + std::to_string(slice - first) + ")" : "its initial bytes (outside the color view)"));
        }
    }
}

}

int main() {
    try {
        GuestBlock array;
        GuestBlock single;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        if (std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), ShaderViewportIndexLayerCapability) == target.supportedCapabilities.end()) {
            std::puts("skipped, the device has no VK_EXT_shader_viewport_index_layer");
            return VulkanTestSkipped;
        }
        Run(*device, array.Data(), Slices - 1u, 1, 3, "a layered draw into slices 1 to 3 of a four-slice 2D array");
        Run(*device, single.Data(), 0, 0, 3, "a layered draw into slices 0 to 2 of a surface of one slice");
        std::puts("layered array color draw tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
