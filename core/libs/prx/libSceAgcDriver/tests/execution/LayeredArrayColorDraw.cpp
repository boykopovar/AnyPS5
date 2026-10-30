#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
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
#include <mutex>
#include <optional>
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
constexpr float ClearValue = 0.25f;

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

constexpr std::size_t CmaskSliceBytes = AgcDriver::Graphics::CmaskLayout::Alignment;
alignas(4096) std::array<std::uint8_t, Slices * CmaskSliceBytes> Cmask{};

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
AgcDriver::Graphics::ColorTarget DecodeColor(std::uint64_t address, std::uint32_t lastArray, std::uint32_t first, std::uint32_t layers, bool fastClear = false) {
    AgcDriver::Registers cx;
    if (fastClear) {
        cx[0x31f] = static_cast<std::uint32_t>(AddressOf(Cmask.data()) >> 8u);
        cx[0x398] = static_cast<std::uint32_t>(AddressOf(Cmask.data()) >> 40u);
    }
    cx[0x318] = static_cast<std::uint32_t>(address >> 8u);
    cx[0x31b] = first | ((first + layers - 1u) << 13u);
    cx[0x31c] = Color32FloatInfo | (fastClear ? 0x2000u : 0u);
    if (fastClear) cx[0x323] = std::bit_cast<std::uint32_t>(ClearValue);
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

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, std::uint32_t drawnLayers = 0, bool coverNothing = false, std::optional<VkRect2D> scissor = std::nullopt) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    if (drawnLayers == 0) drawnLayers = color.layers;
    const auto vertexCount = 6u * drawnLayers;
    BuildQuads(drawnLayers);
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
    state.scissor = coverNothing ? VkRect2D{{0, 0}, {0, 0}} : scissor.value_or(VkRect2D{{0, 0}, {Side, Side}});
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

// A fast-cleared two-layer view (slices first and first + 1) must clear its second layer exactly as the
// single-slice view of that slice does: the CMASK of the second layer is written with that slice's own
// swizzle, and neither draw covers a texel, so only the fast clears reach the second layer's memory.
void RunCmaskClear(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::uint32_t first, const std::string& what) {
    using AgcDriver::Graphics::CmaskLayout;
    constexpr std::array<std::array<std::uint32_t, 2>, 4> clearedTiles{{{0, 0}, {5, 3}, {15, 15}, {9, 12}}};
    const auto surface = AddressOf(texels);
    const CmaskLayout cmaskLayout(Side, Side);
    Require(cmaskLayout.Bytes() == CmaskSliceBytes, what + ": the CMASK of one slice is not " + std::to_string(CmaskSliceBytes) + " bytes");
    const auto second = first + 1u;
    std::vector<std::byte> mask(Cmask.size(), std::byte{0xff});
    for (const auto& tile : clearedTiles) {
        const auto nibble = cmaskLayout.Nibble(tile[0], tile[1], second);
        auto& value = mask[second * CmaskSliceBytes + nibble / 2u];
        value &= std::byte{static_cast<std::uint8_t>((nibble % 2u) == 0 ? 0xf0u : 0x0fu)};
    }
    const auto run = [&](std::uint32_t firstSlice, std::uint32_t layers) {
        std::fill_n(reinterpret_cast<std::uint32_t*>(texels), SurfaceBytes / 4u, Kept);
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            AgcDriver::GuestMemory::Write(AddressOf(Cmask.data()), mask);
        }
        AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        const auto color = DecodeColor(surface, Slices - 1u, firstSlice, layers, true);
        Require(color.cmaskFastClear && color.cmaskAddress == AddressOf(Cmask.data()) + firstSlice * CmaskSliceBytes && color.cmaskSlice == firstSlice && color.cmaskSliceBytes == CmaskSliceBytes && color.clearWords[0] == std::bit_cast<std::uint32_t>(ClearValue), what + ": the fast-clear color view of " + std::to_string(layers) + " slice(s) from " + std::to_string(firstSlice) + " did not decode");
        Draw(device, color, 1, true);
        AgcDriver::Graphics::StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "layered array cmask clear test");
        device.WaitIdle();
        return std::vector<std::uint32_t>(reinterpret_cast<const std::uint32_t*>(texels), reinterpret_cast<const std::uint32_t*>(texels) + SurfaceBytes / 4u);
    };
    const auto layered = run(first, 2);
    const auto single = run(second, 1);
    const auto words = SliceBytes / 4u;
    const auto cleared = [&](const std::vector<std::uint32_t>& image, std::uint32_t slice) {
        return std::count(image.begin() + slice * words, image.begin() + (slice + 1u) * words, std::bit_cast<std::uint32_t>(ClearValue));
    };
    Require(cleared(single, second) == static_cast<std::ptrdiff_t>(clearedTiles.size() * 64u), what + ": the single-slice view of slice " + std::to_string(second) + " cleared " + std::to_string(cleared(single, second)) + " texels, expected " + std::to_string(clearedTiles.size() * 64u));
    for (std::uint32_t slice = 0; slice < Slices; ++slice) {
        if (slice == second) {
            Require(std::equal(layered.begin() + slice * words, layered.begin() + (slice + 1u) * words, single.begin() + slice * words), what + ": the second layer (slice " + std::to_string(second) + ") of the two-layer view holds " + std::to_string(cleared(layered, slice)) + " cleared texels, in other tiles than the single-slice view of that slice clears");
        } else {
            Require(std::all_of(layered.begin() + slice * words, layered.begin() + (slice + 1u) * words, [](std::uint32_t word) { return word == Kept; }), what + ": slice " + std::to_string(slice) + " was changed by the two-layer view");
        }
    }
}

std::vector<std::size_t> ChangedWords(const std::uint8_t* texels, std::uint32_t slice) {
    std::vector<std::size_t> changed;
    const auto* words = reinterpret_cast<const std::uint32_t*>(texels + slice * SliceBytes);
    for (std::size_t word = 0; word < SliceBytes / 4u; ++word) {
        if (words[word] != Kept) changed.push_back(word);
    }
    return changed;
}

void RunTileClearMatchesDraw(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::uint32_t slice, std::uint32_t tileX, std::uint32_t tileY, const std::string& what) {
    using AgcDriver::Graphics::CmaskLayout;
    const auto surface = AddressOf(texels);
    const auto fill = [&] {
        std::fill_n(reinterpret_cast<std::uint32_t*>(texels), SurfaceBytes / 4u, Kept);
        AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes);
        AgcDriver::GuestMemory::BumpCollectEpoch();
    };
    fill();
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        AgcDriver::GuestMemory::Write(AddressOf(Cmask.data()), std::vector<std::byte>(Cmask.size(), std::byte{0xff}));
    }
    Draw(device, DecodeColor(surface, Slices - 1u, slice, 1), 1, false, VkRect2D{{static_cast<std::int32_t>(tileX * 8u), static_cast<std::int32_t>(tileY * 8u)}, {8, 8}});
    AgcDriver::Graphics::StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "tile clear against draw test");
    device.WaitIdle();
    const auto drawn = ChangedWords(texels, slice);
    Require(drawn.size() == 64u, what + ": the drawn tile changed " + std::to_string(drawn.size()) + " words of its slice, expected 64");
    for (std::uint32_t other = 0; other < Slices; ++other) Require(other == slice || ChangedWords(texels, other).empty(), what + ": the draw changed slice " + std::to_string(other));

    fill();
    const CmaskLayout cmaskLayout(Side, Side);
    std::vector<std::byte> mask(Cmask.size(), std::byte{0xff});
    const auto nibble = cmaskLayout.Nibble(tileX, tileY, slice);
    mask[slice * CmaskSliceBytes + nibble / 2u] &= std::byte{static_cast<std::uint8_t>((nibble % 2u) == 0 ? 0xf0u : 0x0fu)};
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        AgcDriver::GuestMemory::Write(AddressOf(Cmask.data()), mask);
    }
    const auto color = DecodeColor(surface, Slices - 1u, slice, 1, true);
    Draw(device, color, 1, true);
    AgcDriver::Graphics::StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "tile clear against draw test");
    device.WaitIdle();
    const auto cleared = ChangedWords(texels, slice);
    for (std::uint32_t other = 0; other < Slices; ++other) Require(other == slice || ChangedWords(texels, other).empty(), what + ": the clear changed slice " + std::to_string(other));
    Require(cleared == drawn, what + ": the clear changed " + std::to_string(cleared.size()) + " words, the draw " + std::to_string(drawn.size()) + ", and they are not the same words");
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
        GuestAllocations::Mutation().Add(Cmask.data(), Cmask.size(), true, true, true);
        std::string failures;
        for (const std::uint32_t first : {0u, 1u}) {
            try {
                RunCmaskClear(*device, array.Data(), first, "a fast-cleared two-layer view of slices " + std::to_string(first) + " and " + std::to_string(first + 1u) + ", drawn on its first layer");
            } catch (const std::exception& error) {
                failures += std::string(error.what()) + "\n";
            }
        }
        Require(failures.empty(), "\n" + failures);
        std::size_t cases = 0;
        std::size_t differing = 0;
        for (std::uint32_t slice = 0; slice < Slices; ++slice) {
            for (const auto& tile : std::array<std::array<std::uint32_t, 2>, 2>{{{5, 3}, {0, 0}}}) {
                ++cases;
                try {
                    RunTileClearMatchesDraw(*device, array.Data(), slice, tile[0], tile[1], "a fast-cleared tile (" + std::to_string(tile[0]) + ", " + std::to_string(tile[1]) + ") of slice " + std::to_string(slice));
                } catch (const std::exception& error) {
                    ++differing;
                    failures += std::string(error.what()) + "\n";
                }
            }
        }
        std::printf("tile clear against draw: %zu cases, %zu differ\n", cases, differing);
        Require(failures.empty(), "\n" + failures);
        GuestAllocations::Mutation().Remove(Cmask.data());
        std::puts("layered array color draw tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
