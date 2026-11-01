#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
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
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using AgcDriver::Graphics::TextureDimension;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Side = 128;
constexpr std::uint32_t Layers = 4;
constexpr std::size_t LayerBytes = 65536;
constexpr std::size_t SurfaceBytes = LayerBytes * Layers;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t Color32FloatInfo = (4u << 2u) | (7u << 8u);
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint32_t Initial = 0x5a5a5a5au;
constexpr std::uint32_t ScissorWidth = 16;
constexpr float Drawn = 0.25f;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 9>, Layers> LoadCodes{};
alignas(256) constexpr std::array<std::uint32_t, 13> Sample2DCode{
    0x7e020d00, 0x060202f0, 0x100202ff, 0x3c000000, 0x7e0402ff, 0x3b800000, 0xf09c0108, 0x00610301,
    0x34080082, 0xbf8c3f70, 0xe0701000, 0x80000304, 0xbf810000,
};
alignas(256) std::array<std::uint32_t, 16> PixelCode{};
alignas(256) std::array<std::uint32_t, Threads> Output{};

void SampleSliceZero(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::uint32_t swizzleMode);
void OverlappingArrays(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::uint32_t swizzleMode, bool cpuWrite = false);
void OverlappingArrayLastSlice(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::uint32_t swizzleMode);

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

class GuestBlock {
public:
    explicit GuestBlock(std::size_t registeredBytes = SurfaceBytes, std::size_t registeredOffset = 0, bool commitAll = true) : registeredOffset(registeredOffset) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, LayerBytes));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block + (commitAll ? 0 : registeredOffset), commitAll ? SurfaceBytes : registeredBytes, PAGE_READWRITE, commitAll ? SurfaceBytes : registeredBytes);
#else
        void* mapped = mmap(nullptr, SurfaceBytes + LayerBytes, commitAll ? PROT_READ | PROT_WRITE : PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + LayerBytes - 1) & ~(static_cast<std::uintptr_t>(LayerBytes) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + SurfaceBytes != begin + SurfaceBytes + LayerBytes) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + LayerBytes - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (!commitAll) Require(mprotect(block + registeredOffset, registeredBytes, PROT_READ | PROT_WRITE) == 0, "cannot commit the active array slice");
            if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block + (commitAll ? 0 : registeredOffset), commitAll ? SurfaceBytes : registeredBytes);
        }
#endif
        Require(block != nullptr, "array layer target: cannot allocate the guest block");
        if (!commitAll) GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(block), SurfaceBytes);
        GuestAllocations::Mutation().Add(block + registeredOffset, registeredBytes, true, true, true);
    }

    ~GuestBlock() {
        if (registered) GuestAllocations::Mutation().Remove(block + registeredOffset);
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
    void Unregister() {
        GuestAllocations::Mutation().Remove(block + registeredOffset);
        registered = false;
    }

private:
    std::uint8_t* block = nullptr;
    std::size_t registeredOffset = 0;
    bool registered = true;
};

std::array<std::uint32_t, 8> SurfaceDescriptor(std::uint64_t address, std::uint32_t swizzleMode, std::uint32_t type, std::uint32_t lastLayer) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (swizzleMode << 20u) | (type << 28u),
        lastLayer,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Dispatch(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Load(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& texture) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    const std::array<std::uint32_t, 4> pointSampler{0u, 0xfffu << 12u, 1u << 26u, 0u};
    std::copy(pointSampler.begin(), pointSampler.end(), userData.begin() + 12);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    Dispatch(device, code, userData);
}

void LoadLayer(AgcDriver::VulkanDevice& device, std::uint64_t surface, std::uint32_t layer, std::uint32_t swizzleMode, std::uint32_t lastLayer = Layers - 1u) {
    auto& code = LoadCodes[layer];
    code = {0x7e020280, 0x7e040280u | layer, 0xf0001128, 0x00010300, 0x34080082, 0xbf8c3f70, 0xe0701000, 0x80000304, 0xbf810000};
    Load(device, code, SurfaceDescriptor(surface, swizzleMode, Type2DArray, lastLayer));
}

void Sample2D(AgcDriver::VulkanDevice& device, std::uint64_t surface, std::uint32_t swizzleMode) {
    Load(device, Sample2DCode, SurfaceDescriptor(surface, swizzleMode, Type2D, 0u));
}

void RequireOutput(std::uint32_t inside, std::uint32_t outside, const std::string& what) {
    for (std::uint32_t x = 0; x < Threads; ++x) {
        const auto expected = x < ScissorWidth ? inside : outside;
        Require(Output[x] == expected, what + ": texel (" + std::to_string(x) + ", 0) reads " + Hex(Output[x]) + ", expected " + Hex(expected));
    }
}

AgcDriver::Graphics::ColorTarget DecodeTarget(std::uint64_t address, std::uint32_t slice, std::uint32_t swizzleMode, bool collapsed, std::uint32_t lastSlice) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(address >> 8u);
    cx[0x31b] = slice | (slice << 13u);
    cx[0x31c] = Color32FloatInfo;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Side - 1u) << 14u) | (Side - 1u);
    cx[0x3b8] = (TiledAttrib3 & ~(0x1fu << 14u)) | (swizzleMode << 14u) | (collapsed ? 0u : lastSlice);
    cx[0x390] = static_cast<std::uint32_t>(address >> 40u);
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R32_SFLOAT && color.bytes == LayerBytes && color.address == address + slice * LayerBytes, "the COLOR_32 float view did not decode as a 128x128 R32 slice of the array");
    if (collapsed) Require(color.arrayAddress == 0 && color.arrayLastSlice == 0, "a collapsed slice view unexpectedly describes a whole array");
    else Require(color.arrayAddress == address && color.arrayLastSlice == lastSlice && color.arraySlice == slice, "the color view did not keep its array and slice");
    return color;
}

void DrawSlice(AgcDriver::VulkanDevice& device, std::uint64_t surface, std::uint32_t slice, std::uint32_t swizzleMode, bool collapsed, std::uint32_t lastSlice = Layers - 1u, std::uint32_t width = ScissorWidth) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    std::size_t at = 0;
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        PixelCode[at++] = 0x7e0002ffu | ((4u + channel) << 17u);
        PixelCode[at++] = std::bit_cast<std::uint32_t>(Drawn);
    }
    PixelCode[at++] = 0xf800180fu;
    PixelCode[at++] = 0x07060504u;
    PixelCode[at++] = 0xbf810000u;
    const std::span<const std::uint32_t> pixelCode(PixelCode.data(), at);
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(Triangle.data());
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(Triangle.size()), 0x01016facu};
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    const auto color = DecodeTarget(surface, slice, swizzleMode, collapsed, lastSlice);
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xf;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Side, Side};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Side), static_cast<float>(Side), -static_cast<float>(Side), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {width, Side}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};

    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, Float32Export}};
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{codeAddress, std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, codeAddress, pixelCode, 0, {}},
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Run(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::uint32_t swizzleMode, bool collapsed = false, std::uint32_t slice = 1) {
    const auto surface = AddressOf(texels);
    std::fill_n(reinterpret_cast<std::uint32_t*>(texels), SurfaceBytes / 4u, Initial);
    AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto drawn = std::bit_cast<std::uint32_t>(Drawn);
    DrawSlice(device, surface, slice, swizzleMode, collapsed);
    const auto pending = StorageTexture::FindPending(collapsed ? surface + slice * LayerBytes : surface, collapsed ? LayerBytes : SurfaceBytes);
    if (collapsed) Require(pending != nullptr && pending->Descriptor().dimension == TextureDimension::k2D && pending->Descriptor().baseAddress == surface + slice * LayerBytes, "a collapsed slice did not render into its shifted surface");
    else Require(pending != nullptr && pending->Descriptor().dimension == TextureDimension::k2DArray && pending->Descriptor().baseAddress == surface && pending->Descriptor().depthOrLastArray == Layers - 1u, "the draw into slice 1 did not render into the image of its array");
    LoadLayer(device, surface, slice, swizzleMode);
    RequireOutput(drawn, Initial, "slice " + std::to_string(slice) + " read through the array after the draw");
    for (std::uint32_t other = 0; other < Layers; ++other) {
        if (other == slice) continue;
        LoadLayer(device, surface, other, swizzleMode);
        RequireOutput(Initial, Initial, "slice " + std::to_string(other) + " after the draw into slice " + std::to_string(slice));
    }
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
    device.WaitIdle();
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
    AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    LoadLayer(device, surface, slice, swizzleMode);
    RequireOutput(drawn, Initial, "slice " + std::to_string(slice) + " uploaded again from guest memory");
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
    SampleSliceZero(device, texels, swizzleMode);
    OverlappingArrays(device, texels, swizzleMode);
    OverlappingArrays(device, texels, swizzleMode, true);
    OverlappingArrayLastSlice(device, texels, swizzleMode);
}

void SampleSliceZero(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::uint32_t swizzleMode) {
    const auto surface = AddressOf(texels);
    std::fill_n(reinterpret_cast<std::uint32_t*>(texels), SurfaceBytes / 4u, Initial);
    AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    DrawSlice(device, surface, 0, swizzleMode, false);
    const auto pending = StorageTexture::FindPending(surface, LayerBytes);
    Require(pending != nullptr && pending->Descriptor().dimension == TextureDimension::k2DArray, "the draw into slice 0 did not render into the image of its array");
    const auto plain = SurfaceDescriptor(surface, swizzleMode, Type2D, 0u);
    Require(AgcDriver::Graphics::Texture::CanCopyFrom(*pending, AgcDriver::Graphics::DecodeTextureResource(plain)), "a 2D texture over slice 0 cannot view the array image's first layer");
    Sample2D(device, surface, swizzleMode);
    RequireOutput(std::bit_cast<std::uint32_t>(Drawn), Initial, "slice 0 sampled as a 2D texture after the draw");
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
    device.WaitIdle();
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
}

void OverlappingArrayLastSlice(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::uint32_t swizzleMode) {
    const auto neighbourAddress = AddressOf(texels);
    const auto surface = neighbourAddress + LayerBytes;
    const auto cpuValue = std::bit_cast<std::uint32_t>(0.125f);
    const auto drawn = std::bit_cast<std::uint32_t>(Drawn);
    std::fill_n(reinterpret_cast<std::uint32_t*>(texels), SurfaceBytes / sizeof(std::uint32_t), Initial);
    AgcDriver::GuestMemory::CollectWrites(neighbourAddress, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    DrawSlice(device, surface, 1u, swizzleMode, false, 1u);
    DrawSlice(device, neighbourAddress, 1u, swizzleMode, false, 1u, Side);
    const auto neighbour = StorageTexture::FindPending(neighbourAddress, 2u * LayerBytes);
    Require(neighbour != nullptr && neighbour->Descriptor().baseAddress == neighbourAddress, "the preceding target has no pending results");
    std::fill_n(reinterpret_cast<std::uint32_t*>(texels + 2u * LayerBytes), LayerBytes / sizeof(std::uint32_t), cpuValue);
    AgcDriver::GuestMemory::CollectWrites(surface + LayerBytes, LayerBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    DrawSlice(device, surface, 1u, swizzleMode, false, 1u);
    Require(StorageTexture::FindPending(neighbourAddress, 2u * LayerBytes) == neighbour, "CPU refresh of the last slice stored the preceding target's results");
    LoadLayer(device, surface, 1u, swizzleMode, 1u);
    RequireOutput(drawn, cpuValue, "the refreshed last slice keeps the CPU's untouched pixels");
    LoadLayer(device, surface, 0u, swizzleMode, 1u);
    RequireOutput(drawn, drawn, "the first slice sees the preceding target's pending results");
    StorageTexture::FlushPending(neighbourAddress, SurfaceBytes, nullptr, "test");
    device.WaitIdle();
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
}

void UnregisteredUnusedLayer(AgcDriver::VulkanDevice& device, std::uint32_t slice) {
    GuestBlock block(LayerBytes, slice * LayerBytes);
    auto* texels = block.Data();
    const auto surface = AddressOf(texels);
    const auto active = surface + slice * LayerBytes;
    std::fill_n(reinterpret_cast<std::uint32_t*>(texels), SurfaceBytes / 4u, Initial);
    AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    DrawSlice(device, surface, slice, TileR64KBX, false);
    Require(StorageTexture::FindPending(surface, SurfaceBytes) != nullptr, "the registered slice was not pending before its CPU write-back");
    StorageTexture::FlushPending(active, LayerBytes, nullptr, "test");
    device.WaitIdle();
    const auto* first = reinterpret_cast<std::uint32_t*>(active);
    const auto* last = reinterpret_cast<std::uint32_t*>(active + LayerBytes);
    const auto unchanged = [](std::uint32_t value) { return value == Initial; };
    const auto drawn = std::count(first, last, std::bit_cast<std::uint32_t>(Drawn));
    const bool untouched = std::all_of(reinterpret_cast<const std::uint32_t*>(texels), first, unchanged) && std::all_of(last, reinterpret_cast<const std::uint32_t*>(texels + SurfaceBytes), unchanged);
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
    Require(drawn == ScissorWidth * Side, "CPU write-back discarded the registered slice because an unused array slice was unregistered");
    Require(untouched, "CPU write-back modified an unregistered unused slice");
}

void RetiredArrayLayer(AgcDriver::VulkanDevice& device) {
    GuestBlock block(LayerBytes);
    auto* texels = block.Data();
    const auto surface = AddressOf(texels);
    std::fill_n(reinterpret_cast<std::uint32_t*>(texels), SurfaceBytes / 4u, Initial);
    AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    DrawSlice(device, surface, 0u, TileR64KBX, false);
    Require(StorageTexture::FindPending(surface, SurfaceBytes) != nullptr, "the retired slice was not pending before it was unregistered");
    block.Unregister();
    StorageTexture::FlushPending(surface, LayerBytes, nullptr, "test");
    device.WaitIdle();
    const bool untouched = std::all_of(reinterpret_cast<const std::uint32_t*>(texels), reinterpret_cast<const std::uint32_t*>(texels + SurfaceBytes), [](std::uint32_t value) { return value == Initial; });
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
    Require(untouched, "CPU write-back stored results over a retired allocation");
}

void UncommittedUnusedLayers(AgcDriver::VulkanDevice& device, std::uint32_t slice) {
    if (!AgcDriver::GuestMemory::WriteWatched()) return;
    GuestBlock block(LayerBytes, slice * LayerBytes, false);
    const auto surface = AddressOf(block.Data());
    const auto active = surface + slice * LayerBytes;
    auto* texels = reinterpret_cast<std::uint32_t*>(active);
    std::fill_n(texels, LayerBytes / sizeof(std::uint32_t), Initial);
    Require(AgcDriver::GuestMemory::CollectWrites(active, LayerBytes) != 0, "the committed array slice has no write history");
    Require(AgcDriver::GuestMemory::CollectWritesUncached(surface, SurfaceBytes) == 0, "the uncommitted array fixture unexpectedly has complete backend write history");
    Require(AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes) == 0, "the unused array slices unexpectedly have complete write history");
    AgcDriver::GuestMemory::BumpCollectEpoch();
    DrawSlice(device, surface, slice, TileR64KBX, false);
    const auto pending = StorageTexture::FindPending(surface, SurfaceBytes);
    Require(pending != nullptr, "the committed array slice has no pending draw results");
    Require(pending->GenerationFor(active, LayerBytes) != 0, "an uncommitted unused slice erased the active slice's write history");
    const auto version = pending->Version();
    for (unsigned repeat = 0; repeat < 3; ++repeat) {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Require(pending->RefreshLayers(slice, 1), "an unchanged committed slice was uploaded again");
        Require(pending->GenerationFor(active, LayerBytes) != 0 && pending->Version() == version, "repeated refresh erased the committed slice's write history");
        Require(StorageTexture::FindPending(surface, SurfaceBytes) == pending, "refresh settled unchanged GPU results for an uncommitted unused slice");
    }
    const auto cpu = std::bit_cast<std::uint32_t>(0.125f);
    std::fill_n(texels, LayerBytes / sizeof(std::uint32_t), cpu);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(!pending->RefreshLayers(slice, 1), "the active array slice ignored a genuine CPU write");
    LoadLayer(device, surface, slice, TileR64KBX);
    RequireOutput(cpu, cpu, "a committed array slice after a genuine CPU write");
    DrawSlice(device, surface, slice, TileR64KBX, false);
    StorageTexture::FlushPending(active, LayerBytes, nullptr, "test");
    device.WaitIdle();
    Require(std::count(texels, texels + LayerBytes / sizeof(std::uint32_t), std::bit_cast<std::uint32_t>(Drawn)) == ScissorWidth * Side, "write-back lost results in the committed array slice");
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
}

void OverlappingArrays(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::uint32_t swizzleMode, bool cpuWrite) {
    const auto surface = AddressOf(texels);
    const auto next = surface + LayerBytes;
    const auto drawn = std::bit_cast<std::uint32_t>(Drawn);
    std::fill_n(reinterpret_cast<std::uint32_t*>(texels), SurfaceBytes / 4u, Initial);
    AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    DrawSlice(device, surface, 0, swizzleMode, false, 1u);
    DrawSlice(device, next, 0, swizzleMode, false, 1u, Side);
    const auto neighbour = StorageTexture::FindPending(next, 2u * LayerBytes);
    Require(neighbour != nullptr && neighbour->Descriptor().baseAddress == next && neighbour->Descriptor().dimension == TextureDimension::k2DArray, "the next target did not render into the image of its own array");
    if (cpuWrite) {
        std::fill_n(reinterpret_cast<std::uint32_t*>(texels), LayerBytes / sizeof(std::uint32_t), std::bit_cast<std::uint32_t>(0.125f));
        AgcDriver::GuestMemory::CollectWrites(surface, LayerBytes);
        AgcDriver::GuestMemory::BumpCollectEpoch();
    }
    DrawSlice(device, surface, 0, swizzleMode, false, 1u);
    Require(StorageTexture::FindPending(next, 2u * LayerBytes) == neighbour, "a draw into slice 0 of the first array stored the next target's results, which only its unused second slice overlaps");
    Sample2D(device, surface, swizzleMode);
    RequireOutput(drawn, cpuWrite ? std::bit_cast<std::uint32_t>(0.125f) : Initial, "slice 0 of the first array sampled as a 2D texture after both draws");
    Require(StorageTexture::FindPending(next, 2u * LayerBytes) == neighbour, "a 2D texture over slice 0 of the first array stored the next target's results, which only its unused second slice overlaps");
    LoadLayer(device, next, 0, swizzleMode, 1u);
    RequireOutput(drawn, drawn, "slice 0 of the next array after the draw into the first");
    LoadLayer(device, surface, 1, swizzleMode, 1u);
    RequireOutput(drawn, drawn, "slice 1 of the first array (the next target's slice 0) read while the next target's results were pending");
    LoadLayer(device, surface, 0, swizzleMode, 1u);
    RequireOutput(drawn, cpuWrite ? std::bit_cast<std::uint32_t>(0.125f) : Initial, "slice 0 of the first array after both draws");
    LoadLayer(device, next, 1, swizzleMode, 1u);
    RequireOutput(Initial, Initial, "slice 1 of the next array, which nothing drew");
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
    device.WaitIdle();
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
}

}

int main() {
    try {
        GuestBlock block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, block.Data(), TileR64KBX);
        Run(*device, block.Data(), static_cast<std::uint32_t>(AgcDriver::Graphics::ColorTileMode::D64KBX));
        for (std::uint32_t slice = 1; slice < Layers; ++slice) Run(*device, block.Data(), TileR64KBX, true, slice);
        UnregisteredUnusedLayer(*device, 0u);
        UnregisteredUnusedLayer(*device, Layers - 1u);
        RetiredArrayLayer(*device);
        UncommittedUnusedLayers(*device, 0u);
        UncommittedUnusedLayers(*device, Layers - 1u);
        std::puts("array layer target tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
