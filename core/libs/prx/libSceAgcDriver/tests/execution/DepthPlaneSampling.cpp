#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
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
#include <cstdio>
#include <cstring>
#include <deque>
#include <iostream>
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
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "depth plane sampling: cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(reinterpret_cast<std::uintptr_t>(block), BlockBytes);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* At(std::size_t offset) { return block + offset; }

private:
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
void DrawDepth(AgcDriver::VulkanDevice& device, std::uint64_t address, VkExtent2D extent, VkFormat format, float z) {
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

void Load(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Shape& shape, std::uint32_t slice, std::span<std::uint32_t> output, bool shaderImage2D = false, std::uint32_t row = 0) {
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
    std::fill(output.begin(), output.end(), 0xdeadbeefu);
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
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
        IntegerCase(*device, block, output);
        present();
        DepthBitsArrayCase(*device, block, output);
        present();
        DepthBitsBaseSliceCase(*device, block, output);
        present();
        GuestDepthBitsCase(*device, block, output);
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
