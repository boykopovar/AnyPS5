#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "DepthFastClearHarness.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
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
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
using DepthFastClearHarness::BufferDescriptor;
using DepthFastClearHarness::PixelCode;
using DepthFastClearHarness::VertexCode;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Format16Unorm = 7;
constexpr std::uint32_t Format16Uint = 11;
constexpr std::uint32_t Format32Uint = 20;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t TypeCube = 11;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t Swizzle64KbS = 9;
constexpr std::uint32_t Swizzle64KbZX = 24;
constexpr std::uint64_t SliceBytes = 65536;
constexpr std::size_t BlockBytes = 8u << 20u;
constexpr std::size_t OutputOffset = 0;
constexpr std::size_t ArrayOffset = 0x10000;
constexpr std::size_t CubeOffset = 0x40000;
constexpr std::size_t IntegerOffset = 0xa0000;
constexpr std::size_t DepthBitsOffset = 0xb0000;
constexpr std::size_t OneSliceOffset = 0xd0000;
constexpr std::size_t FirstSliceOffset = 0xe0000;
constexpr std::size_t PaddedCubeOffset = 0x120000;
constexpr std::size_t SmallCubeOffset = 0x180000;
constexpr std::size_t LargeCubeOffset = 0x200000;
constexpr std::size_t GuestDepthBitsOffset = 0x480000;
constexpr std::size_t FrameCopiesOffset = 0x520000;
constexpr std::size_t ReusedArrayOffset = 0x680000;
constexpr std::size_t ReloadOffset = 0x700000;
constexpr std::uint32_t TableLanes = 64;
constexpr std::uint32_t TableTextures = 1500;
constexpr std::uint32_t TableStride = 32;
constexpr std::size_t TableSurfaceBytes = 256;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::size_t TableSrtOffset = 4096;
constexpr std::size_t TableInputOffset = 8192;
constexpr std::size_t TableOutputOffset = 12288;
constexpr std::size_t TableHeapOffset = 65536;
constexpr std::size_t TableDepthOffset = 0x20000;
constexpr std::size_t TableBlockBytes = 0x40000;
constexpr std::size_t TableSurfaceBlockBytes = (std::size_t{TableTextures} * TableSurfaceBytes + 65535u) & ~std::size_t{65535};
constexpr std::array<std::uint32_t, 25> TableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u,
    0x34080082u, 0x7e200500u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xbf8cc07fu,
    0xe0301000u, 0x80030504u, 0xf09c0f08u, 0x00450000u, 0xbf8c3f70u, 0x4a000105u,
    0xe0701000u, 0x80070004u, 0xbf810000u, 0u, 0u};
constexpr float DrawnDepth = 0.25f;
constexpr float DrawnD16Depth = 0.2f;
constexpr std::uint32_t DrawnD16Bits = 13107;

constexpr std::array<std::uint32_t, 3> LoadDims{1u, 3u, 5u};

std::array<std::uint32_t, 11> LoadCode(std::uint32_t dim) {
    return {
        0x7e02020d,
        0x7e04020c,                        // v_mov_b32 v2, s12 (slice or face)
        0xf0001100u | (dim << 3u),         // image_load v3, v[0:2], s[4:11] dmask:0x1 unorm, dim 2D / CUBE / 2D_ARRAY
        0x00010300,
        0xbf8c3f70,                        // s_waitcnt vmcnt(0)
        0x34080082,                        // v_lshlrev_b32 v4, 2, v0
        0xe0701000,                        // buffer_store_dword v3, v4, s[0:3], 0 offen
        0x80000304,
        0xbf810000,                        // s_endpgm
        0xbf810000,
        0xbf810000,
    };
}

alignas(256) std::array<std::uint32_t, 11> Load2D = LoadCode(LoadDims[0]);
alignas(256) std::array<std::uint32_t, 11> LoadCube = LoadCode(LoadDims[1]);
alignas(256) std::array<std::uint32_t, 11> LoadArray = LoadCode(LoadDims[2]);

alignas(256) std::array<std::byte, 320 * 320 * 4> Pixels{};

class GuestBlock {
public:
    explicit GuestBlock(std::size_t size = BlockBytes, bool gpu = true) : bytes(size) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, bytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + bytes != begin + bytes + alignment) munmap(reinterpret_cast<void*>(aligned + bytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
        }
#endif
        Require(block != nullptr, "depth plane sampling: cannot allocate the guest block");
        std::memset(block, 0, bytes);
        GuestAllocations::Mutation().Add(block, bytes, true, true, gpu);
        AgcDriver::GuestMemory::CollectWritesUncached(reinterpret_cast<std::uintptr_t>(block), bytes);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, bytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
#else
        munmap(block, bytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, bytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* At(std::size_t offset) { return block + offset; }

private:
    std::size_t bytes = 0;
    std::uint8_t* block = nullptr;
};

struct Shape {
    std::uint32_t format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t type;
    std::uint32_t lastArray;
    std::uint32_t swizzle;
    std::uint32_t baseArray = 0;
};

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, const Shape& shape) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (shape.format << 20u) | (((shape.width - 1u) & 3u) << 30u),
        ((shape.width - 1u) >> 2u) | ((shape.height - 1u) << 14u),
        0xfacu | (shape.swizzle << 20u) | (shape.type << 28u),
        shape.lastArray | (shape.baseArray << 16u),
        0u, 0u, 0u,
    };
}

std::uint32_t Bits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// Draws a full-target triangle at depth `z` into the depth surface at `address`, which makes the driver create the
// surface (the base slice a later plane copy reads from the surface instead of from guest memory).
void DrawDepth(AgcDriver::VulkanDevice& device, std::uint64_t address, VkExtent2D extent, VkFormat format, float z, std::uint32_t swizzle = 0) {
    static std::deque<std::vector<std::array<float, 4>>> vertexData;
    vertexData.push_back({{-1.0f, -1.0f, z, 1.0f}, {3.0f, -1.0f, z, 1.0f}, {-1.0f, 3.0f, z, 1.0f}});
    const auto& vertices = vertexData.back();
    const auto target = device.Target();
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(vertices.data(), 16u, static_cast<std::uint32_t>(vertices.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertexRequest{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {32, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertexRequest.useCache = false;
    const auto vertex = ShaderRecompiler::Recompile(vertexRequest);
    const auto vertexPush = static_cast<std::uint32_t>(vertex.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixelInfo{};
    pixelInfo.wave32 = true;
    pixelInfo.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixelInfo.posX = true;
    pixelInfo.posY = true;
    pixelInfo.targetOutputMode[0] = 9;
    pixelInfo.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(4, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
    ShaderRecompiler::RecompileRequest fragmentRequest{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {32, 0, pixelUserData, std::nullopt, pixelInfo, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragmentRequest.useCache = false;
    const auto pixel = ShaderRecompiler::Recompile(fragmentRequest);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertex, 0},
        {ShaderStage::Fragment, &pixel, PixelPushOffset(vertexPush, target)}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), extent, VK_FORMAT_R8G8B8A8_UNORM, AgcDriver::Graphics::ColorTargetLayout(extent.width, extent.height, AgcDriver::Graphics::ColorTileMode::Linear).Bytes(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = AgcDriver::Graphics::DepthTarget{address, 0, extent, format, 1.0f, 0};
    state.depth->depthSwizzle = swizzle;
    state.depthTest = true;
    state.depthWrite = true;
    state.depthCompare = VK_COMPARE_OP_ALWAYS;
    state.renderExtent = extent;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(extent.height), static_cast<float>(extent.width), -static_cast<float>(extent.height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, extent};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(vertices.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

ShaderRecompiler::RecompileResult CompileLoad(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Shape& shape, std::uint32_t slice, std::span<std::uint32_t> output, bool shaderImage2D = false, std::uint32_t row = 0) {
    const auto& code = shaderImage2D ? Load2D : shape.type == TypeCube ? LoadCube : shape.type == Type2DArray ? LoadArray : Load2D;
    std::vector<std::uint32_t> userData(16, 0u);
    const auto out = std::array<std::uint32_t, 4>{static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(output.data())), static_cast<std::uint32_t>((reinterpret_cast<std::uintptr_t>(output.data()) >> 32u) & 0xffffu), static_cast<std::uint32_t>(output.size_bytes()), 0x31016facu};
    const auto texture = TextureDescriptor(texels, shape);
    std::copy(out.begin(), out.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    userData[12] = slice;
    userData[13] = row;
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void DispatchLoad(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& result, const Shape& shape, std::span<std::uint32_t> output, bool shaderImage2D = false) {
    const auto& code = shaderImage2D ? Load2D : shape.type == TypeCube ? LoadCube : shape.type == Type2DArray ? LoadArray : Load2D;
    std::fill(output.begin(), output.end(), 0xdeadbeefu);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Load(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Shape& shape, std::uint32_t slice, std::span<std::uint32_t> output, bool shaderImage2D = false, std::uint32_t row = 0) {
    const auto result = CompileLoad(device, texels, shape, slice, output, shaderImage2D, row);
    DispatchLoad(device, result, shape, output, shaderImage2D);
}

void Expect(std::span<const std::uint32_t> output, std::uint32_t expected, const std::string& what) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        char text[160];
        std::snprintf(text, sizeof(text), "%s: texel %u read 0x%08x, expected 0x%08x", what.c_str(), tid, output[tid], expected);
        Require(output[tid] == expected, text);
    }
}

void FillSlice(std::uint8_t* slice, std::uint64_t bytes, std::uint32_t value, std::uint32_t elementBytes) {
    for (std::uint64_t offset = 0; offset < bytes; offset += elementBytes) std::memcpy(slice + offset, &value, elementBytes);
}

// A 3-slice D32 array: slice 0 from the live depth surface, slices 1 and 2 from guest memory (detiled).
void ArrayCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    auto* base = block.At(ArrayOffset);
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(base), {128, 128}, VK_FORMAT_D32_SFLOAT, DrawnDepth);
    FillSlice(base + SliceBytes, SliceBytes, Bits(0.5f), 4u);
    FillSlice(base + 2u * SliceBytes, SliceBytes, Bits(0.75f), 4u);
    const Shape shape{Format32Float, 128, 128, Type2DArray, 2, Swizzle64KbZX};
    const std::array<float, 3> expected{DrawnDepth, 0.5f, 0.75f};
    for (std::uint32_t slice = 0; slice < 3u; ++slice) {
        Load(device, base, shape, slice, output);
        Expect(output, Bits(expected[slice]), "D32 2D array slice " + std::to_string(slice));
    }
}

void ReusedArrayCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    auto* base = block.At(ReusedArrayOffset);
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(base), {128, 128}, VK_FORMAT_D32_SFLOAT, DrawnDepth);
    FillSlice(base + SliceBytes, SliceBytes, Bits(0.5f), 4u);
    FillSlice(base + 2u * SliceBytes, SliceBytes, Bits(0.75f), 4u);
    const Shape shape{Format32Float, 128, 128, Type2DArray, 2, Swizzle64KbZX};
    const auto first = CompileLoad(device, base, shape, 0, output);
    const auto second = CompileLoad(device, base, shape, 1, output);
    const auto before = AgcDriver::Graphics::SharedResourceCache().Size();
    DispatchLoad(device, first, shape, output);
    Expect(output, Bits(DrawnDepth), "reused D32 array slice 0");
    if (std::getenv("APS5_NO_PER_USE_TEXTURE_TEMPLATES") == nullptr) Require(AgcDriver::Graphics::SharedResourceCache().Size() > before, "a dispatch over a depth plane copy did not enter the resource cache");
    DispatchLoad(device, second, shape, output);
    Expect(output, Bits(0.5f), "reused D32 array slice 1");
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(base), {128, 128}, VK_FORMAT_D32_SFLOAT, 0.625f);
    DispatchLoad(device, first, shape, output);
    Expect(output, Bits(0.625f), "reused D32 array slice 0 after the surface was drawn again");
    FillSlice(base + SliceBytes, SliceBytes, Bits(0.875f), 4u);
    DispatchLoad(device, second, shape, output);
    Expect(output, Bits(0.875f), "reused D32 array slice 1 after its guest memory changed");
    DispatchLoad(device, first, shape, output);
    Expect(output, Bits(0.625f), "reused D32 array slice 0 dispatched again unchanged");
}

ShaderRecompiler::RecompileResult CompileTable(AgcDriver::VulkanDevice& device, GuestBlock& block) {
    const auto base = reinterpret_cast<std::uint64_t>(block.At(0));
    const auto srt = reinterpret_cast<std::uint64_t>(block.At(TableSrtOffset));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srt), static_cast<std::uint32_t>(srt >> 32u)};
    const std::span<const std::uint32_t> code(reinterpret_cast<const std::uint32_t*>(block.At(0)), TableCode.size());
    const std::array<ShaderRecompiler::MemoryRegion, 3> memory{{
        {base, std::as_bytes(code)},
        {srt, std::as_bytes(std::span<const std::uint32_t>(reinterpret_cast<const std::uint32_t*>(block.At(TableSrtOffset)), 64))},
        {reinterpret_cast<std::uint64_t>(block.At(TableHeapOffset)), std::as_bytes(std::span<const std::uint32_t>(reinterpret_cast<const std::uint32_t*>(block.At(TableHeapOffset)), static_cast<std::size_t>(TableTextures) * TableStride / 4u))},
    }};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{TableLanes, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, base, code, 0, {}},
        {TableLanes, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void TableOverBoundCase(AgcDriver::VulkanDevice& device) {
    auto& block = *new GuestBlock(TableBlockBytes, true);
    auto& surfaces = *new GuestBlock(TableSurfaceBlockBytes, false);
    std::memcpy(block.At(0), TableCode.data(), sizeof(TableCode));
    auto* depth = block.At(TableDepthOffset);
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(depth), {256, 128}, VK_FORMAT_D16_UNORM, DrawnD16Depth);
    const auto plane = TextureDescriptor(depth, {Format16Uint, 256, 128, Type2D, 0, Swizzle64KbS});
    std::memcpy(block.At(TableHeapOffset), plane.data(), sizeof(plane));
    for (std::uint32_t texture = 1; texture < TableTextures; ++texture) {
        const auto offset = static_cast<std::size_t>(texture) * TableSurfaceBytes;
        std::memset(surfaces.At(offset), static_cast<int>(texture & 0xffu), TableSurfaceBytes);
        const auto surface = reinterpret_cast<std::uint64_t>(surfaces.At(offset));
        const std::array<std::uint32_t, 8> words{static_cast<std::uint32_t>(surface >> 8u), static_cast<std::uint32_t>((surface >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 1u | (7u << 14u), 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
        std::memcpy(block.At(TableHeapOffset + static_cast<std::size_t>(texture) * TableStride), words.data(), sizeof(words));
    }
    auto* srt = reinterpret_cast<std::uint32_t*>(block.At(TableSrtOffset));
    const auto heap = BufferDescriptor(block.At(TableHeapOffset), TableStride, TableTextures);
    const auto input = BufferDescriptor(block.At(TableInputOffset), 0u, TableLanes * 4u);
    const auto output = BufferDescriptor(block.At(TableOutputOffset), 0u, TableLanes * 4u);
    std::copy(heap.begin(), heap.end(), srt);
    std::copy(input.begin(), input.end(), srt + 8);
    std::copy(output.begin(), output.end(), srt + 12);
    AgcDriver::GuestMemory::CollectWritesUncached(reinterpret_cast<std::uintptr_t>(block.At(0)), TableHeapOffset + std::size_t{TableTextures} * TableStride);
    AgcDriver::GuestMemory::CollectWritesUncached(reinterpret_cast<std::uintptr_t>(surfaces.At(0)), TableSurfaceBlockBytes);
    const auto result = CompileTable(device, block);
    const auto table = std::find_if(result.bindings.begin(), result.bindings.end(), [](const auto& binding) { return binding.role == ShaderRecompiler::DescriptorRole::ImageTable; });
    Require(table != result.bindings.end() && table->count == TableTextures, "table over the bound: the program does not bind an image table of " + std::to_string(TableTextures) + " textures");
    using AgcDriver::Graphics::ShaderResources;
    const bool templates = std::getenv("APS5_NO_PER_USE_TEXTURE_TEMPLATES") == nullptr;
    const bool partial = std::getenv("APS5_NO_PER_USE_PARTIAL_WALK") == nullptr;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const AgcDriver::Graphics::CompiledShader shader{ShaderStage::Compute, &result, 0};
    std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
    auto resources = std::make_shared<ShaderResources>(context, shader);
    const auto& sampled = resources->SampledTextures();
    const auto perUse = std::count_if(sampled.begin(), sampled.end(), [](const auto& texture) { return texture != nullptr && texture->RefreshedPerUse(); });
    Require(sampled.size() == TableTextures && perUse == 1, "table over the bound: the build holds " + std::to_string(sampled.size()) + " textures, " + std::to_string(perUse) + " of them refreshed per use, instead of " + std::to_string(TableTextures) + " with one");
    if (!templates) {
        Require(!resources->Reusable(), "table over the bound: APS5_NO_PER_USE_TEXTURE_TEMPLATES kept a build over a per-use texture reusable");
        return;
    }
    Require(resources->Reusable(), "table over the bound: a build over a per-use texture whose " + std::to_string(TableTextures) + " entries all have records is not reusable");
    const auto prove = [&](const char* what) {
        ShaderResources::ProofReport report;
        Require(resources->Revalidate(shader, &report), std::string("table over the bound: the cached build was not proved current ") + what);
        return report;
    };
    const auto path = [](ShaderResources::ProofPath value) { return value == ShaderResources::ProofPath::Fast ? std::string("the fast proof") : value == ShaderResources::ProofPath::OwnRefreshed ? std::string("the own refresh") : std::string("the full walk"); };
    const auto expected = partial ? ShaderResources::ProofPath::Fast : ShaderResources::ProofPath::Full;
    static_cast<void>(prove("after the build"));
    auto report = prove("with its memory unchanged");
    Require(report.path == expected, "table over the bound: a hit with its memory unchanged took " + path(report.path) + " instead of " + path(expected));
    gpu.unlock();
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(depth), {256, 128}, VK_FORMAT_D16_UNORM, 0.6f);
    gpu.lock();
    report = prove("after its depth plane was drawn again");
    Require(report.path == expected, "table over the bound: a hit after its depth plane was drawn again took " + path(report.path) + " instead of " + path(expected));
}

// A cube over six square D32 slices: face 0 from the live surface, faces 1-5 from guest memory.
void CubeCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    auto* base = block.At(CubeOffset);
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(base), {128, 128}, VK_FORMAT_D32_SFLOAT, DrawnDepth);
    for (std::uint32_t face = 1; face < 6u; ++face) FillSlice(base + face * SliceBytes, SliceBytes, Bits(static_cast<float>(face) / 8.0f), 4u);
    const Shape shape{Format32Float, 128, 128, TypeCube, 0, Swizzle64KbZX};
    for (std::uint32_t face = 0; face < 6u; ++face) {
        Load(device, base, shape, face, output);
        Expect(output, Bits(face == 0 ? DrawnDepth : static_cast<float>(face) / 8.0f), "D32 cube face " + std::to_string(face));
    }
}

// A D16 plane read through an R16_UINT view (a non-depth swizzle, so not flagged as depth bits): the raw depth value.
void IntegerCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    auto* base = block.At(IntegerOffset);
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(base), {256, 128}, VK_FORMAT_D16_UNORM, DrawnD16Depth);
    Load(device, base, {Format16Uint, 256, 128, Type2D, 0, Swizzle64KbS}, 0, output);
    Expect(output, DrawnD16Bits, "D16 plane as R16_UINT");
}

// A D16 2D array whose R16_UINT T# has a depth swizzle (flagged as depth bits): both slices' raw values.
void DepthBitsArrayCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    auto* base = block.At(DepthBitsOffset);
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(base), {256, 128}, VK_FORMAT_D16_UNORM, DrawnD16Depth);
    FillSlice(base + SliceBytes, SliceBytes, 0x1234u, 2u);
    const Shape shape{Format16Uint, 256, 128, Type2DArray, 1, Swizzle64KbZX};
    Load(device, base, shape, 0, output);
    Expect(output, DrawnD16Bits, "D16 depth-bits array slice 0");
    Load(device, base, shape, 1, output);
    Expect(output, 0x1234u, "D16 depth-bits array slice 1");
}

// A 2D shader image over a 2D-array T# reads the array's first slice: a one-slice array, which samples the depth
// image itself, and a 3-slice array, which samples the plane copy.
void DepthBitsBaseSliceCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    Shape shape{Format16Uint, 256, 128, Type2DArray, 1, Swizzle64KbZX};
    shape.baseArray = 1;
    Load(device, block.At(DepthBitsOffset), shape, 0, output);
    Expect(output, 0x1234u, "D16 depth-bits array from base slice 1");
}

void GuestDepthBitsCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    constexpr std::array<std::uint32_t, 9> values{0x00000001u, 0x007fffffu, 0x80000000u, 0x80000001u, 0x7f800001u, 0x7fc00001u, 0xffffffffu, 0x3e800000u, 0x12345678u};
    for (std::size_t index = 0; index < values.size(); ++index) {
        auto* base = block.At(GuestDepthBitsOffset + index * SliceBytes);
        FillSlice(base, SliceBytes, values[index], 4u);
        Load(device, base, {Format32Uint, 128, 128, Type2D, 0, Swizzle64KbZX}, 0, output, false, static_cast<std::uint32_t>(index * 13u));
        char what[96];
        std::snprintf(what, sizeof(what), "32-bit depth-bits read of guest memory holding 0x%08x", values[index]);
        Expect(output, values[index], what);
    }
}

void GuestWriteAfterDrawCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    auto* base = block.At(ReloadOffset);
    if (!AgcDriver::GuestMemory::WriteWatched() || !AgcDriver::GuestMemory::Watched(reinterpret_cast<std::uintptr_t>(base), SliceBytes)) {
        std::puts("guest memory has no write watch: the guest write after a draw is not checked");
        return;
    }
    const Shape shape{Format32Float, 128, 128, Type2D, 0, Swizzle64KbZX};
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(base), {128, 128}, VK_FORMAT_D32_SFLOAT, DrawnDepth, AgcDriver::Graphics::XorSwizzleMode(AgcDriver::Graphics::TextureTileMode::kZ64KBX));
    Load(device, base, shape, 0, output);
    Expect(output, Bits(DrawnDepth), "D32 plane sampled after a draw");
    FillSlice(base, SliceBytes, Bits(0.75f), 4u);
    Load(device, base, shape, 0, output);
    Expect(output, Bits(0.75f), "D32 plane the guest wrote after a draw, sampled before its surface is bound again");
}

void FirstSliceCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    auto* one = block.At(OneSliceOffset);
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(one), {128, 128}, VK_FORMAT_D32_SFLOAT, DrawnDepth);
    Load(device, one, {Format32Float, 128, 128, Type2DArray, 0, Swizzle64KbZX}, 0, output, true);
    Expect(output, Bits(DrawnDepth), "one-slice D32 2D array through a 2D shader image");
    auto* base = block.At(FirstSliceOffset);
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(base), {128, 128}, VK_FORMAT_D32_SFLOAT, DrawnDepth);
    FillSlice(base + SliceBytes, SliceBytes, Bits(0.5f), 4u);
    FillSlice(base + 2u * SliceBytes, SliceBytes, Bits(0.75f), 4u);
    Load(device, base, {Format32Float, 128, 128, Type2DArray, 2, Swizzle64KbZX}, 0, output, true);
    Expect(output, Bits(DrawnDepth), "3-slice D32 2D array through a 2D shader image");
}

void PaddedCubeCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    auto* base = block.At(PaddedCubeOffset);
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(base), {128, 128}, VK_FORMAT_D16_UNORM, DrawnD16Depth);
    for (std::uint32_t face = 1; face < 6u; ++face) FillSlice(base + face * SliceBytes, SliceBytes, 0x1000u + face, 2u);
    const Shape shape{Format16Uint, 128, 128, TypeCube, 0, Swizzle64KbZX};
    for (std::uint32_t face = 0; face < 6u; ++face) {
        Load(device, base, shape, face, output);
        Expect(output, face == 0 ? DrawnD16Bits : 0x1000u + face, "D16 cube with padded rows, face " + std::to_string(face));
    }
}

void SmallD16CubeCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    auto* base = block.At(SmallCubeOffset);
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(base), {32, 32}, VK_FORMAT_D16_UNORM, DrawnD16Depth);
    for (std::uint32_t face = 1; face < 6u; ++face) FillSlice(base + face * SliceBytes, SliceBytes, 0x1230u + face, 2u);
    const Shape shape{Format16Uint, 32, 32, TypeCube, 0, Swizzle64KbZX};
    for (std::uint32_t face = 0; face < 6u; ++face) {
        Load(device, base, shape, face, output);
        Expect(output, face == 0 ? DrawnD16Bits : 0x1230u + face, "small D16 cube face " + std::to_string(face));
    }
}

void PaddedD16CubeCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    constexpr std::uint32_t side = 320;
    const auto mips = AgcDriver::Graphics::ComputeElementMipLayout(AgcDriver::Graphics::TextureTileMode::kZ64KBX, 2u, side, side, 1u);
    Require(mips.size() == 1 && mips.front().pitchBytes == 1024 && mips.front().linearSize == 327680 && mips.front().tiledSize == 393216, "the 320 D16 regression must have a padded row pitch and face stride");
    const auto stride = AgcDriver::Graphics::DepthSliceBytes({side, side}, 2u);
    Require(stride == 393216 && LargeCubeOffset + 6u * stride <= BlockBytes, "the 320 D16 cube must fit its actual tiled face stride");
    auto* base = block.At(LargeCubeOffset);
    DrawDepth(device, reinterpret_cast<std::uintptr_t>(base), {side, side}, VK_FORMAT_D16_UNORM, DrawnD16Depth);
    for (std::uint32_t face = 1; face < 6; ++face) FillSlice(base + face * stride, stride, 0x1000u + face * 0x1000u, 2u);
    const Shape shape{Format16Unorm, side, side, TypeCube, 5u, Swizzle64KbZX};
    const auto check = [&](std::uint32_t face, std::uint32_t row, bool rewritten) {
        Load(device, base, shape, face, output, false, row);
        const auto bits = face == 0 ? DrawnD16Bits : rewritten && face == 3 ? 0x7000u : 0x1000u + face * 0x1000u;
        const auto expected = static_cast<float>(bits) / 65535.0f;
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            float actual = 0;
            std::memcpy(&actual, &output[tid], sizeof actual);
            char message[160];
            std::snprintf(message, sizeof message, "320 D16 cube face %u row %u texel %u: got %.8f, expected %.8f", face, row, tid, actual, expected);
            Require(std::abs(actual - expected) <= 2.0f / 65535.0f, message);
        }
    };
    for (std::uint32_t face = 0; face < 6; ++face) {
        for (const auto row : {0u, 1u, 127u, 319u}) check(face, row, false);
    }
    FillSlice(base + 3u * stride, stride, 0x7000u, 2u);
    for (std::uint32_t face = 0; face < 6; ++face) check(face, 319u, true);
}

void BaseSliceCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    auto* base = block.At(ArrayOffset);
    Shape shape{Format32Float, 128, 128, Type2DArray, 2, Swizzle64KbZX};
    shape.baseArray = 1;
    Load(device, base, shape, 0, output);
    Expect(output, Bits(0.5f), "D32 2D array from base slice 1, slice 0");
    Load(device, base, shape, 1, output);
    Expect(output, Bits(0.75f), "D32 2D array from base slice 1, slice 1");
    Load(device, base, shape, 0, output, true);
    Expect(output, Bits(0.5f), "D32 2D array from base slice 1 through a 2D shader image");
}

void FrameCopiesCase(AgcDriver::VulkanDevice& device, GuestBlock& block, std::span<std::uint32_t> output) {
    constexpr VkExtent2D extent{128, 256};
    constexpr VkDeviceSize budget = 1u << 20u;
    const auto stride = AgcDriver::Graphics::DepthSliceBytes(extent, 4u);
    Require(FrameCopiesOffset + 9u * stride <= BlockBytes, "the three 3-slice 128x256 D32 arrays must fit the guest block");
    const std::array<std::uint8_t*, 3> bases{block.At(FrameCopiesOffset), block.At(FrameCopiesOffset + 3u * stride), block.At(FrameCopiesOffset + 6u * stride)};
    const std::array<float, 3> expected{0.375f, 0.625f, 0.875f};
    for (std::size_t copy = 0; copy < bases.size(); ++copy) {
        DrawDepth(device, reinterpret_cast<std::uintptr_t>(bases[copy]), extent, VK_FORMAT_D32_SFLOAT, DrawnDepth);
        for (std::uint32_t slice = 1; slice < 3u; ++slice) FillSlice(bases[copy] + slice * stride, stride, Bits(expected[copy]), 4u);
    }
    const Shape shape{Format32Float, extent.width, extent.height, Type2DArray, 2, Swizzle64KbZX};
    const auto sample = [&](std::size_t copy) {
        Load(device, bases[copy], shape, 2, output);
        Expect(output, Bits(expected[copy]), "D32 2D array " + std::to_string(copy) + " slice 2");
    };
    AgcDriver::Graphics::Recorder::CountPresent();
    AgcDriver::Graphics::Recorder::CountPresent();
    sample(0);
    sample(1);
    sample(0);
    sample(1);
    const auto kept = AgcDriver::Graphics::DepthPlaneCopyCount();
    Require(kept == 2, std::to_string(kept) + " depth plane copies were kept after two copies over the 1 MiB budget were sampled in one present, expected both");
    const auto pair = AgcDriver::Graphics::DepthPlaneCopyBytes();
    Require(pair > budget && pair <= 2u * budget, "the two copies sampled in one present hold " + std::to_string(pair) + " bytes, the case needs more than the 1 MiB budget and no more than twice it");
    sample(2);
    const auto capped = AgcDriver::Graphics::DepthPlaneCopyBytes();
    Require(capped <= 2u * budget, "the depth plane copies sampled in one present hold " + std::to_string(capped) + " bytes in " + std::to_string(AgcDriver::Graphics::DepthPlaneCopyCount()) + " copies, more than twice the 1 MiB budget");
    Require(AgcDriver::Graphics::DepthPlaneCopyCount() == 2, std::to_string(AgcDriver::Graphics::DepthPlaneCopyCount()) + " depth plane copies were kept after a third copy took the ones sampled in one present past twice the budget, expected the two most recently used");
    AgcDriver::Graphics::Recorder::CountPresent();
    AgcDriver::Graphics::Recorder::CountPresent();
    sample(1);
    const auto left = AgcDriver::Graphics::DepthPlaneCopyCount();
    Require(left == 1, std::to_string(left) + " depth plane copies were kept two presents after the other copy was last sampled, expected only the newest");
}

void ExpectRefusal(AgcDriver::VulkanDevice& device, GuestBlock& block, std::size_t offset, const Shape& shape, std::uint32_t baseArray, const std::string& expected, const std::string& what) {
    const auto words = TextureDescriptor(block.At(offset), shape);
    auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    resource.baseArray = baseArray;
    try {
        static_cast<void>(AgcDriver::Graphics::DepthSurfaceTexture(AgcDriver::VulkanDeviceTestAccess::ContextOf(device), words, resource, {}));
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find(expected) != std::string::npos, what + " was refused with an unexpected message: " + error.what());
        return;
    }
    Require(false, what + " was not refused");
}

void RefusalCase(AgcDriver::VulkanDevice& device, GuestBlock& block) {
    ExpectRefusal(device, block, ArrayOffset, {Format32Float, 128, 128, Type2DArray, 2, Swizzle64KbZX}, 3, "base slice 3 is not implemented", "a depth plane array whose base slice is past its last slice");
    ExpectRefusal(device, block, CubeOffset, {Format32Float, 128, 128, TypeCube, 5, Swizzle64KbZX}, 1, "base slice 1 is not implemented", "a depth plane cube from base slice 1");
    ExpectRefusal(device, block, ArrayOffset, {Format32Float, 128, 128, Type2D, 0, Swizzle64KbZX}, 1, "slice 1 is not implemented", "a 2D depth plane view with a base slice");
}
}

int main(int argc, char** argv) {
    try {
        const bool evicting = argc > 1 && std::string(argv[1]) == "--evicting";
        GuestBlock block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const std::span<std::uint32_t> output(reinterpret_cast<std::uint32_t*>(block.At(OutputOffset)), Threads);
        const auto present = [&] {
            if (!evicting) return;
            AgcDriver::Graphics::Recorder::CountPresent();
            AgcDriver::Graphics::Recorder::CountPresent();
        };
        ArrayCase(*device, block, output);
        present();
        CubeCase(*device, block, output);
        present();
        ReusedArrayCase(*device, block, output);
        present();
        TableOverBoundCase(*device);
        present();
        IntegerCase(*device, block, output);
        present();
        DepthBitsArrayCase(*device, block, output);
        present();
        DepthBitsBaseSliceCase(*device, block, output);
        present();
        GuestDepthBitsCase(*device, block, output);
        present();
        GuestWriteAfterDrawCase(*device, block, output);
        present();
        FirstSliceCase(*device, block, output);
        present();
        PaddedCubeCase(*device, block, output);
        present();
        SmallD16CubeCase(*device, block, output);
        present();
        PaddedD16CubeCase(*device, block, output);
        present();
        BaseSliceCase(*device, block, output);
        RefusalCase(*device, block);
        if (evicting) {
            const auto kept = AgcDriver::Graphics::DepthPlaneCopyCount();
            Require(kept >= 1 && kept < 8, std::to_string(kept) + " depth plane copies were kept with a 1 MiB budget, expected at least the newest and fewer than the 8 an unbounded cache keeps");
            FrameCopiesCase(*device, block, output);
        }
        std::puts("depth plane sampling tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
