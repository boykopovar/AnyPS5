#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
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
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Side = 128;
constexpr std::uint32_t Layers = 6;
constexpr std::size_t LayerBytes = 65536;
constexpr std::size_t SurfaceBytes = LayerBytes * Layers;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t Color32FloatInfo = (4u << 2u) | (7u << 8u);
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint32_t LayerExport = (1u << 18u) | (1u << 21u);
constexpr std::uint32_t ShaderViewportIndexLayerCapability = 5254;
constexpr float Filled = 0.25f;
constexpr float Passed = 0.75f;
constexpr std::size_t DepthSliceBytes = LayerBytes;
constexpr std::size_t BlockBytes = SurfaceBytes + DepthSliceBytes * Layers;
constexpr std::size_t HtileSliceBytes = ((Side + 1023u) / 1024u) * ((Side + 511u) / 512u) * 32768u;

alignas(256) std::array<std::uint32_t, HtileSliceBytes * Layers / 4u> Htile{};

alignas(256) constexpr std::array<std::uint32_t, 10> VertexCode{
    0xe0382000, 0x80000005, 0xe030201c, 0x80000405, 0xbf8c3f70, 0xf80000cf, 0x03020100, 0xf80008d4, 0x00040000, 0xbf810000,
};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 3> color;
    std::uint32_t layer;
};

alignas(256) std::array<std::array<std::uint32_t, 9>, Layers> LoadCodes{};
alignas(256) std::array<std::uint32_t, 11> FillPixelCode{};
alignas(256) std::array<std::uint32_t, 11> PassPixelCode{};
alignas(256) std::array<std::uint32_t, Threads> Output{};
alignas(256) std::array<Vertex, 6 * Layers> Vertices{};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::uint64_t DepthAddress = 0;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, LayerBytes));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        void* mapped = mmap(nullptr, BlockBytes + LayerBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + LayerBytes - 1) & ~(static_cast<std::uintptr_t>(LayerBytes) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + LayerBytes) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + LayerBytes - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "layered depth target: cannot allocate the color array");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

void BuildPixelCode(std::array<std::uint32_t, 11>& code, float value) {
    std::size_t at = 0;
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        code[at++] = 0x7e0002ffu | ((4u + channel) << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(value);
    }
    code[at++] = 0xf800180fu;
    code[at++] = 0x07060504u;
    code[at++] = 0xbf810000u;
}

void Quads(std::uint32_t layers, bool ramp, std::span<const float> depths) {
    Vertices = {};
    for (std::uint32_t layer = 0; layer < layers; ++layer) {
        const float right = ramp ? -0.5f : 1.0f;
        const float left = -1.0f;
        const float leftDepth = ramp ? 0.0f : depths[layer];
        const float rightDepth = ramp ? 1.0f : depths[layer];
        const std::array<std::array<float, 3>, 6> corners{{{left, -1.0f, leftDepth}, {right, -1.0f, rightDepth}, {left, 1.0f, leftDepth}, {right, -1.0f, rightDepth}, {right, 1.0f, rightDepth}, {left, 1.0f, leftDepth}}};
        for (std::uint32_t k = 0; k < corners.size(); ++k) Vertices[6u * layer + k] = {{corners[k][0], corners[k][1], corners[k][2], 1.0f}, {1.0f, 1.0f, 1.0f}, layers == 1 ? 0u : layer};
    }
}

AgcDriver::Graphics::ColorTarget DecodeColor(std::uint64_t address, std::uint32_t slice, std::uint32_t layers) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(address >> 8u);
    cx[0x31b] = slice | ((slice + layers - 1u) << 13u);
    cx[0x31c] = Color32FloatInfo;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Side - 1u) << 14u) | (Side - 1u);
    cx[0x3b8] = (TiledAttrib3 & ~(0x1fu << 14u)) | (TileR64KBX << 14u) | (Layers - 1u);
    cx[0x390] = static_cast<std::uint32_t>(address >> 40u);
    cx[0x207] = layers == 1 ? 0u : LayerExport;
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.layers == layers && color.arraySlice == slice && color.arrayLastSlice == Layers - 1u, "the color view did not decode as " + std::to_string(layers) + " layers of the six-slice array from slice " + std::to_string(slice));
    return color;
}

struct Pass {
    std::uint32_t slice;
    std::uint32_t layers;
    bool depthTest;
    bool depthWrite;
    VkCompareOp compare;
    std::span<const std::uint32_t> pixelCode;
};

void Draw(AgcDriver::VulkanDevice& device, std::uint64_t surface, const Pass& pass) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    const auto vertexCount = 6u * pass.layers;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(Vertices.data());
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

    const auto color = DecodeColor(surface, pass.slice, pass.layers);
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xf;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Side, Side};
    state.renderLayers = pass.layers;
    state.layerExports = pass.layers == 1 ? 0u : 1u << 18u;
    state.depth = AgcDriver::Graphics::DepthTarget{DepthAddress + pass.slice * DepthSliceBytes, 0, {Side, Side}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, AddressOf(Htile.data()) + pass.slice * HtileSliceBytes, false};
    state.depth->firstSlice = pass.slice;
    state.depth->lastSlice = pass.slice + pass.layers - 1u;
    state.depth->layers = pass.layers;
    state.depthTest = pass.depthTest;
    state.depthWrite = pass.depthWrite;
    state.depthCompare = pass.compare;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Side), static_cast<float>(Side), -static_cast<float>(Side), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Side, Side}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};

    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pass.pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, Float32Export}};
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{codeAddress, std::as_bytes(pass.pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, codeAddress, pass.pixelCode, 0, {}},
        {waveSize, 0, {}, std::nullopt, AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state)), std::nullopt, pixelMemory},
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

void FillDepth(AgcDriver::VulkanDevice& device, std::uint64_t surface, std::uint32_t slice, std::span<const float> depths) {
    const auto layers = static_cast<std::uint32_t>(depths.size());
    Quads(layers, false, depths);
    Draw(device, surface, {slice, layers, true, true, VK_COMPARE_OP_ALWAYS, FillPixelCode});
}

void TestDepth(AgcDriver::VulkanDevice& device, std::uint64_t surface, std::uint32_t slice, std::uint32_t layers) {
    const std::array<float, Layers> unused{};
    Quads(layers, false, unused);
    Draw(device, surface, {slice, layers, false, false, VK_COMPARE_OP_ALWAYS, FillPixelCode});
    Quads(layers, true, unused);
    Draw(device, surface, {slice, layers, true, false, VK_COMPARE_OP_LESS, PassPixelCode});
}

std::array<std::uint32_t, 8> ArrayDescriptor(std::uint64_t address) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2DArray << 28u),
        Layers - 1u,
        0u,
        0u,
        0u,
    };
}

void LoadLayer(AgcDriver::VulkanDevice& device, std::uint64_t surface, std::uint32_t layer) {
    static std::map<std::pair<std::uint64_t, std::uint32_t>, ShaderRecompiler::RecompileResult> compiled;
    Output.fill(0xdeadbeefu);
    auto& loadCode = LoadCodes[layer];
    loadCode = {0x7e020280, 0x7e040280u | layer, 0xf0001128, 0x00010300, 0x34080082, 0xbf8c3f70, 0xe0701000, 0x80000304, 0xbf810000};
    const std::span<const std::uint32_t> code(loadCode);
    auto found = compiled.find({surface, layer});
    if (found != compiled.end()) {
        device.Dispatch(found->second, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device.WaitIdle();
        return;
    }
    std::vector<std::uint32_t> userData(16, 0u);
    const auto output = AddressOf(Output.data());
    const std::array<std::uint32_t, 4> buffer{static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu), static_cast<std::uint32_t>(Output.size() * 4u), 0x31016facu};
    const auto texture = ArrayDescriptor(surface);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto& result = compiled.emplace(std::pair{surface, layer}, ShaderRecompiler::Recompile(request)).first->second;
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Expect(AgcDriver::VulkanDevice& device, std::uint64_t surface, std::uint32_t slice, std::uint32_t passed, const std::string& what) {
    LoadLayer(device, surface, slice);
    for (std::uint32_t x = 0; x < Threads; ++x) {
        const auto expected = std::bit_cast<std::uint32_t>(x < passed ? Passed : Filled);
        Require(Output[x] == expected, what + ": slice " + std::to_string(slice) + " texel (" + std::to_string(x) + ", 0) is " + std::to_string(std::bit_cast<float>(Output[x])) + ", expected " + std::to_string(std::bit_cast<float>(expected)) + " (the depth test of a ramp from 0 to 1 over 32 texels passes on the first " + std::to_string(passed) + ")");
    }
}

void Run(AgcDriver::VulkanDevice& device, std::uint8_t* texels) {
    const auto surface = AddressOf(texels);
    DepthAddress = surface + SurfaceBytes;
    std::fill_n(reinterpret_cast<std::uint32_t*>(texels), SurfaceBytes / 4u, 0u);
    AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Htile.fill(0xffffffffu);
    BuildPixelCode(FillPixelCode, Filled);
    BuildPixelCode(PassPixelCode, Passed);

    const std::array<float, 1> sliceTwo{0.375f};
    FillDepth(device, surface, 2, sliceTwo);
    TestDepth(device, surface, 0, Layers);
    for (std::uint32_t slice = 0; slice < Layers; ++slice) Expect(device, surface, slice, slice == 2 ? 12u : Threads, "a layered draw after a draw into depth slice 2 alone");

    std::array<float, Layers> depths{};
    for (std::uint32_t layer = 0; layer < Layers; ++layer) depths[layer] = static_cast<float>(layer + 1u) / 8.0f;
    FillDepth(device, surface, 0, depths);
    TestDepth(device, surface, 0, Layers);
    for (std::uint32_t slice = 0; slice < Layers; ++slice) Expect(device, surface, slice, 4u * (slice + 1u), "a layered draw writing (layer + 1) / 8 into each depth slice");

    AgcDriver::Graphics::NoteDepthMetadataFill(AddressOf(Htile.data()) + 4u * HtileSliceBytes, HtileSliceBytes, 0u);
    TestDepth(device, surface, 0, Layers);
    for (std::uint32_t slice = 0; slice < Layers; ++slice) Expect(device, surface, slice, slice == 4 ? Threads : 4u * (slice + 1u), "a layered draw after an HTILE clear of depth slice 4 alone");

    TestDepth(device, surface, 1, 1);
    Expect(device, surface, 1, 8u, "a draw into depth slice 1 alone after the layered draws");

    const std::array<float, 3> middle{0.5f, 0.625f, 0.75f};
    FillDepth(device, surface, 2, middle);
    TestDepth(device, surface, 0, Layers);
    const std::array<std::uint32_t, Layers> expected{4u, 8u, 16u, 20u, 24u, 24u};
    for (std::uint32_t slice = 0; slice < Layers; ++slice) Expect(device, surface, slice, expected[slice], "a six-layer draw after a three-layer draw into depth slices 2 to 4");

    for (std::uint32_t layer = 0; layer < Layers; ++layer) depths[layer] = static_cast<float>(layer + 1u) / 16.0f;
    FillDepth(device, surface, 0, depths);
    for (std::uint32_t slice = 0; slice < Layers; ++slice) {
        LoadLayer(device, DepthAddress, slice);
        for (std::uint32_t x = 0; x < Threads; ++x) Require(Output[x] == std::bit_cast<std::uint32_t>(depths[slice]), "depth slice " + std::to_string(slice) + " read as a layer of a 2D array texture after the six-layer draw: texel (" + std::to_string(x) + ", 0) is " + std::to_string(std::bit_cast<float>(Output[x])) + ", expected " + std::to_string(depths[slice]));
    }
    AgcDriver::Graphics::StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
    device.WaitIdle();
}

}

int main() {
    try {
        GuestBlock block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        if (std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), ShaderViewportIndexLayerCapability) == target.supportedCapabilities.end()) {
            std::puts("skipped, the device has no VK_EXT_shader_viewport_index_layer");
            return VulkanTestSkipped;
        }
        Run(*device, block.Data());
        std::puts("layered depth target tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
