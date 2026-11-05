#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Pixels;
using DepthFastClearHarness::Shaders;
using DepthFastClearHarness::Surface;
using DepthFastClearHarness::Width;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Format8Uint = 5;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t TileLinear = 0;
constexpr std::uint32_t TileStandard256B = 1;
constexpr std::uint32_t TileZ64KBX = 0x18;
constexpr std::size_t BlockBytes = 2u << 20u;
constexpr std::size_t StencilOffset = 1u << 16u;
constexpr std::size_t OutputOffset = 2u << 16u;
constexpr std::size_t MultisampledOffset = 4u << 16u;
constexpr std::size_t MultisampledColorOffset = 5u << 16u;
constexpr std::size_t SampledOffset = 8u << 16u;
constexpr std::size_t LaterDepthOffset = 10u << 16u;
constexpr std::size_t ArrayDepthOffset = 12u << 16u;
constexpr std::size_t ArrayStencilOffset = 13u << 16u;
constexpr std::size_t ArrayOffset = 16u << 16u;
constexpr std::uint32_t ArrayLayers = 3;

alignas(256) constexpr std::array<std::uint32_t, 14> ScaleDepthCode{
    0x7e3c0300u, 0x7e3e0280u, 0x7e140280u, 0xf0001108u, 0x00010a1eu, 0xbf8c3f70u, 0x34060082u,
    0xe0701000u, 0x80000a03u, 0x1014140cu, 0xf0201108u, 0x00010a1eu, 0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 14> AddStencilCode{
    0x7e3c0300u, 0x7e3e0280u, 0x7e140280u, 0xf0001108u, 0x00010a1eu, 0xbf8c3f70u, 0x34060082u,
    0xe0701000u, 0x80000a03u, 0x4a141484u, 0xf0201108u, 0x00010a1eu, 0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 16> ScaleDepthLayerCode{
    0x7e3c0300u, 0x7e3e0280u, 0x7e400280u, 0x7e140280u, 0xf0001128u, 0x00010a1eu, 0xbf8c3f70u, 0x34060082u,
    0xe0701000u, 0x80000a03u, 0x1014140cu, 0xf0201128u, 0x00010a1eu, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 16> AddStencilLayerCode{
    0x7e3c0300u, 0x7e3e0280u, 0x7e400280u, 0x7e140280u, 0xf0001128u, 0x00010a1eu, 0xbf8c3f70u, 0x34060082u,
    0xe0701000u, 0x80000a03u, 0x4a141484u, 0xf0201128u, 0x00010a1eu, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 9> StoreCode{
    0x7e280d00u, 0x102828ffu, 0x3c800000u, 0x7e3c0300u, 0x7e3e0280u, 0xf0201108u, 0x0001141eu, 0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 10> StoreLayerCode{
    0x7e280d00u, 0x1028280eu, 0x7e3c0300u, 0x7e3e0280u, 0x7e40020cu, 0x7e42020du, 0xf0241128u, 0x0001141eu, 0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 10> StoreTwoViewsCode{
    0x7e280d00u, 0x102828ffu, 0x3c800000u, 0x7e3c0300u, 0x7e3e0280u, 0xf0201108u, 0x0001141eu, 0xf0201108u, 0x0003141eu, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 11> LoadLayerCode{
    0x7e020280u, 0x7e04020cu, 0xf0001128u, 0x00010300u, 0xbf8c3f70u, 0x34080082u, 0xe0701000u, 0x80000304u, 0xbf810000u, 0xbf810000u, 0xbf810000u,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(65536, BlockBytes));
#endif
        Require(block != nullptr, "depth storage dispatch: cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format, std::uint32_t type = Type2D, std::uint32_t tile = TileLinear, std::uint32_t lastArray = 0, std::uint32_t baseArray = 0) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (tile << 20u) | (type << 28u),
        lastArray | (baseArray << 16u), 0u, 0u, 0u,
    };
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

ShaderRecompiler::RecompileResult PlaneProgram(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<std::uint32_t> output, const std::uint8_t* plane, std::uint32_t format, float factor) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size_bytes()));
    const auto texture = TextureDescriptor(plane, format);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    userData[12] = std::bit_cast<std::uint32_t>(factor);
    return Compile(device, code, userData);
}

void RunPlane(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& program, std::span<const std::uint32_t> code, std::span<std::uint32_t> output, std::uint32_t expected, const std::string& what) {
    std::fill(output.begin(), output.end(), 0xdeadbeefu);
    device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Require(output[lane] == expected, what + ": the dispatch read " + std::to_string(output[lane]) + " at texel " + std::to_string(lane) + ", expected " + std::to_string(expected));
    }
}

void RequireRow(std::uint32_t covered, const std::string& what) {
    Require(covered == Threads, what + ": " + std::to_string(covered) + " pixels passed, expected the " + std::to_string(Threads) + " the dispatch stored");
    for (std::uint32_t x = 0; x < Threads; ++x) {
        Require(std::to_integer<std::uint8_t>(Pixels[x * 4u]) == 255u, what + ": pixel " + std::to_string(x) + " of the first row did not pass");
    }
}

template<typename TAction>
void ExpectRefusal(TAction action, const char* expected, const std::string& what) {
    try {
        action();
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find(expected) != std::string::npos, what + " threw another error: " + error.what());
        return;
    }
    throw std::runtime_error(what + " was accepted");
}

VkStencilOpState StencilEqual(std::uint32_t reference) {
    return {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffu, 0u, reference};
}

void StoreBackCase(AgcDriver::VulkanDevice& device, const Shaders& shaders, std::uint8_t* block) {
    auto* depth = block;
    auto* stencil = block + StencilOffset;
    const std::span<std::uint32_t> output(reinterpret_cast<std::uint32_t*>(block + OutputOffset), Threads);
    const Surface surface{reinterpret_cast<std::uintptr_t>(depth), reinterpret_cast<std::uintptr_t>(stencil), 0, false, VK_FORMAT_D32_SFLOAT_S8_UINT};
    const VkStencilOpState writeFive{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 5u};
    const auto bits = [](float value) { return std::bit_cast<std::uint32_t>(value); };
    Require(Draw(device, shaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .stencilTest = true, .stencil = writeFive}) == Covered, "the depth and stencil producer did not cover the surface");

    const auto raise = PlaneProgram(device, ScaleDepthCode, output, depth, Format32Float, 1.5f);
    const auto lower = PlaneProgram(device, ScaleDepthCode, output, depth, Format32Float, 0.5f);
    RunPlane(device, raise, ScaleDepthCode, output, bits(0.5f), "first depth dispatch");
    RequireRow(Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_LESS}), "depth test against the first dispatch's depth");
    RunPlane(device, lower, ScaleDepthCode, output, bits(0.75f), "second depth dispatch");
    RequireRow(Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_GREATER}), "depth test against the second dispatch's depth");
    Require(Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_LESS}) == 0, "the first dispatch's depth outlived the second dispatch");
    RunPlane(device, raise, ScaleDepthCode, output, bits(0.375f), "third depth dispatch, its program's second");
    RequireRow(Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_LESS}), "depth test against the third dispatch's depth");

    const auto add = PlaneProgram(device, AddStencilCode, output, stencil, Format8Uint, 0.0f);
    RunPlane(device, add, AddStencilCode, output, 5u, "first stencil dispatch");
    RequireRow(Draw(device, shaders, surface, {.depthTest = false, .stencilTest = true, .stencil = StencilEqual(9u)}), "stencil test against the first dispatch's stencil");
    RunPlane(device, add, AddStencilCode, output, 9u, "second stencil dispatch");
    RequireRow(Draw(device, shaders, surface, {.depthTest = false, .stencilTest = true, .stencil = StencilEqual(13u)}), "stencil test against the second dispatch's stencil");
    Require(Draw(device, shaders, surface, {.depthTest = false, .stencilTest = true, .stencil = StencilEqual(5u)}) == Covered - Threads, "a stencil dispatch changed stencil values it did not store");
    RequireRow(Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_LESS}), "depth test after the stencil dispatches");

    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(depth), Width * Height * sizeof(float), nullptr, "test");
    device.WaitIdle();
    for (std::uint32_t x = 0; x < Width; ++x) {
        float texel = 0.0f;
        std::memcpy(&texel, depth + x * sizeof(float), sizeof(texel));
        Require(texel == (x < Threads ? 0.5625f : 0.5f), "guest depth texel " + std::to_string(x) + " is " + std::to_string(texel) + " after the flush");
    }
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(stencil), Width * Height, nullptr, "test");
    device.WaitIdle();
    for (std::uint32_t x = 0; x < Width; ++x) {
        Require(stencil[x] == (x < Threads ? 13u : 5u), "guest stencil texel " + std::to_string(x) + " is " + std::to_string(stencil[x]) + " after the flush");
    }
}

void ArrayViewCase(AgcDriver::VulkanDevice& device, const Shaders& shaders, std::uint8_t* block) {
    auto* depth = block + ArrayDepthOffset;
    auto* stencil = block + ArrayStencilOffset;
    const std::span<std::uint32_t> output(reinterpret_cast<std::uint32_t*>(block + OutputOffset), Threads);
    const Surface surface{reinterpret_cast<std::uintptr_t>(depth), reinterpret_cast<std::uintptr_t>(stencil), 0, false, VK_FORMAT_D32_SFLOAT_S8_UINT};
    const VkStencilOpState writeFive{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 5u};
    Require(Draw(device, shaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .stencilTest = true, .stencil = writeFive}) == Covered, "the depth and stencil producer of the array views did not cover the surface");
    const auto layer = [&](std::span<const std::uint32_t> code, const std::uint8_t* plane, std::uint32_t format, float factor) {
        std::vector<std::uint32_t> userData(16, 0u);
        const auto buffer = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size_bytes()));
        const auto texture = TextureDescriptor(plane, format, Type2DArray);
        std::copy(buffer.begin(), buffer.end(), userData.begin());
        std::copy(texture.begin(), texture.end(), userData.begin() + 4);
        userData[12] = std::bit_cast<std::uint32_t>(factor);
        return Compile(device, code, userData);
    };
    RunPlane(device, layer(ScaleDepthLayerCode, depth, Format32Float, 1.5f), ScaleDepthLayerCode, output, std::bit_cast<std::uint32_t>(0.5f), "depth dispatch through a one-layer 2D array view");
    RequireRow(Draw(device, shaders, surface, {.depthCompare = VK_COMPARE_OP_LESS}), "depth test against the one-layer 2D array view's depth");
    RunPlane(device, layer(AddStencilLayerCode, stencil, Format8Uint, 0.0f), AddStencilLayerCode, output, 5u, "stencil dispatch through a one-layer 2D array view");
    RequireRow(Draw(device, shaders, surface, {.depthTest = false, .stencilTest = true, .stencil = StencilEqual(9u)}), "stencil test against the one-layer 2D array view's stencil");
}

ShaderRecompiler::RecompileResult LayerProgram(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<std::uint32_t> output, const std::uint8_t* layers, std::uint32_t layer, std::uint32_t baseArray) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size_bytes()));
    const auto texture = TextureDescriptor(layers, Format32Float, Type2DArray, TileZ64KBX, ArrayLayers - 1u, baseArray);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    userData[12] = layer;
    userData[14] = std::bit_cast<std::uint32_t>(1.0f / 128.0f);
    return Compile(device, code, userData);
}

void StoreLayer(AgcDriver::VulkanDevice& device, std::span<std::uint8_t> layers, std::span<std::uint32_t> output, std::uint32_t layer, std::uint32_t baseArray = 0) {
    const auto store = LayerProgram(device, StoreLayerCode, output, layers.data(), layer, baseArray);
    device.Dispatch(store, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreLayerCode.data()));
    device.WaitIdle();
}

void RequireGuestLayer(std::span<const std::uint8_t> layer, bool stored, float rest, float initial, const std::string& what) {
    std::vector<std::uint32_t> seen(Threads, 0u);
    std::uint32_t kept = 0;
    for (std::size_t offset = 0; offset < layer.size(); offset += sizeof(float)) {
        float texel = 0.0f;
        std::memcpy(&texel, layer.data() + offset, sizeof(texel));
        const auto lane = static_cast<std::uint32_t>(texel * 128.0f);
        if (stored && texel < 0.25f && texel != initial && static_cast<float>(lane) / 128.0f == texel) {
            ++seen[lane];
            continue;
        }
        if (texel == rest) {
            ++kept;
            continue;
        }
        Require(texel == initial, what + ": guest texel at offset " + std::to_string(offset) + " is " + std::to_string(texel) + ", expected " + std::to_string(rest) + " or the untouched " + std::to_string(initial));
    }
    const auto texels = static_cast<std::uint32_t>(layer.size() / sizeof(float));
    const auto expected = (rest == initial ? texels : Width * Height) - (stored ? Threads : 0u);
    Require(kept == expected, what + ": " + std::to_string(kept) + " guest texels hold " + std::to_string(rest) + ", expected " + std::to_string(expected));
    if (!stored) return;
    for (std::uint32_t lane = 0; lane < Threads; ++lane) Require(seen[lane] == 1u, what + ": the value stored by lane " + std::to_string(lane) + " is in guest memory " + std::to_string(seen[lane]) + " times");
}

void LayeredArrayViewCase(AgcDriver::VulkanDevice& device, const Shaders& shaders, std::uint8_t* block) {
    const auto sliceBytes = static_cast<std::size_t>(AgcDriver::Graphics::DepthSliceBytes({Width, Height}, 4u));
    Require(sliceBytes == 1u << 16u, "a depth slice of the test extent is not one 64 KiB block");
    const std::span<std::uint8_t> layers(block + ArrayOffset, sliceBytes * ArrayLayers);
    const std::span<std::uint32_t> output(reinterpret_cast<std::uint32_t*>(block + OutputOffset), Threads);
    const auto guestLayer = [&](std::uint32_t layer) { return layers.subspan(sliceBytes * layer, sliceBytes); };
    const float untouched = 0.875f;
    const float guestDepth = 0.75f;
    for (std::uint32_t layer = 0; layer < ArrayLayers; ++layer) {
        const float value = layer == 2 ? guestDepth : untouched;
        for (std::size_t offset = 0; offset < sliceBytes; offset += sizeof(float)) std::memcpy(guestLayer(layer).data() + offset, &value, sizeof(float));
    }
    const Surface first{reinterpret_cast<std::uintptr_t>(guestLayer(0).data()), 0, 0, false, VK_FORMAT_D32_SFLOAT};
    const Surface second{reinterpret_cast<std::uintptr_t>(guestLayer(1).data()), 0, 0, false, VK_FORMAT_D32_SFLOAT};
    Require(Draw(device, shaders, first, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the first layer's depth producer did not cover its surface");
    Require(Draw(device, shaders, second, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the second layer's depth producer did not cover its surface");

    StoreLayer(device, layers, output, 1);
    RequireRow(Draw(device, shaders, second, {.depthCompare = VK_COMPARE_OP_GREATER}), "depth test against the values a 2D array storage view stored to the second layer's surface");
    Require(Draw(device, shaders, first, {.depthCompare = VK_COMPARE_OP_GREATER}) == 0, "a store to the second layer changed the first layer's surface");
    StoreLayer(device, layers, output, 0, 2);
    RequireRow(Draw(device, shaders, second, {.depthCompare = VK_COMPARE_OP_GREATER}), "depth test against the second layer's surface after a store to the third layer through a view from base slice 2");

    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(layers.data()), layers.size(), nullptr, "test");
    device.WaitIdle();
    RequireGuestLayer(guestLayer(0), false, 0.5f, untouched, "the first layer in guest memory after the flush");
    RequireGuestLayer(guestLayer(1), true, 0.5f, untouched, "the second layer in guest memory after the flush");
    RequireGuestLayer(guestLayer(2), true, guestDepth, guestDepth, "the third layer in guest memory after the flush");
}

void MultisampledCase(AgcDriver::VulkanDevice& device, const Shaders& compiled, std::uint8_t* block) {
    auto* depth = block + MultisampledOffset;
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderRecompiler::ShaderStage::Vertex, &compiled.vertex, 0},
        {ShaderRecompiler::ShaderStage::Fragment, &compiled.pixel, PixelPushOffset(compiled.vertexPush, device.Target())}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(block + MultisampledColorOffset), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Width * Height * 16u, 0xe4u};
    state.color.samples = 4;
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = AgcDriver::Graphics::DepthTarget{reinterpret_cast<std::uintptr_t>(depth), 0, {Width, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0, false, 4};
    state.depthTest = true;
    state.depthWrite = true;
    state.depthCompare = VK_COMPARE_OP_ALWAYS;
    state.samples = 4;
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(DepthFastClearHarness::Triangles().size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(depth, Format32Float);
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const auto program = Compile(device, StoreCode, userData);
    ExpectRefusal([&] { device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreCode.data())); }, "storage image access to a multisampled depth surface", "a storage view of a multisampled depth plane");
}

void SampledCopyCase(AgcDriver::VulkanDevice& device, const Shaders& shaders, std::uint8_t* block) {
    auto* depth = block + SampledOffset;
    const std::span<std::uint32_t> output(reinterpret_cast<std::uint32_t*>(block + OutputOffset), Threads);
    const Surface surface{reinterpret_cast<std::uintptr_t>(depth), 0, 0, false, VK_FORMAT_D32_SFLOAT};
    const Surface later{reinterpret_cast<std::uintptr_t>(block + LaterDepthOffset), 0, 0, false, VK_FORMAT_D32_SFLOAT};
    Require(Draw(device, shaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the sampled depth producer did not cover the surface");
    Require(Draw(device, shaders, later, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the later depth producer did not cover its surface");

    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size_bytes()));
    const auto layers = TextureDescriptor(depth, Format32Float, Type2DArray, TileLinear, 1u);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(layers.begin(), layers.end(), userData.begin() + 4);
    const auto load = Compile(device, LoadLayerCode, userData);
    const auto raise = PlaneProgram(device, ScaleDepthCode, output, depth, Format32Float, 1.5f);
    RunPlane(device, load, LoadLayerCode, output, std::bit_cast<std::uint32_t>(0.5f), "first layer of the depth plane copy");
    RunPlane(device, load, LoadLayerCode, output, std::bit_cast<std::uint32_t>(0.5f), "first layer of the unchanged depth plane copy");
    RunPlane(device, raise, ScaleDepthCode, output, std::bit_cast<std::uint32_t>(0.5f), "depth dispatch between two plane copies");
    RunPlane(device, load, LoadLayerCode, output, std::bit_cast<std::uint32_t>(0.75f), "first layer of the depth plane copy after the dispatch stored to the plane");
}

void TwoViewsCase(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    std::vector<std::uint32_t> userData(20, 0u);
    const auto first = TextureDescriptor(block, Format32Float, Type2D, TileLinear);
    const auto second = TextureDescriptor(block, Format32Float, Type2D, TileStandard256B);
    std::copy(first.begin(), first.end(), userData.begin() + 4);
    std::copy(second.begin(), second.end(), userData.begin() + 12);
    const auto program = Compile(device, StoreTwoViewsCode, userData);
    ExpectRefusal([&] { device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreTwoViewsCode.data())); }, "through a view that another view of the plane", "a dispatch storing to two views of one depth plane");
}

}

int main() {
    try {
        GuestBlock block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto shaders = DepthFastClearHarness::Compile(*device);
        StoreBackCase(*device, shaders, block.Data());
        ArrayViewCase(*device, shaders, block.Data());
        LayeredArrayViewCase(*device, shaders, block.Data());
        MultisampledCase(*device, shaders, block.Data());
        TwoViewsCase(*device, block.Data());
        SampledCopyCase(*device, shaders, block.Data());
        std::puts("depth storage dispatch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
