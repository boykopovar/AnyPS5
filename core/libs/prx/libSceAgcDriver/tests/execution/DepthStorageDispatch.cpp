#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
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
#include <cmath>
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
constexpr std::uint32_t Format16Unorm = 7;
constexpr std::uint32_t Format16Snorm = 8;
constexpr std::uint32_t Format16Uint = 11;
constexpr std::uint32_t Format16Sint = 12;
constexpr std::uint32_t Format16Sfloat = 13;
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
constexpr std::size_t AliasOffset = 18u << 16u;
constexpr std::size_t AliasControlOffset = 19u << 16u;
constexpr std::size_t AliasWriteOffset = 20u << 16u;
constexpr std::size_t ForcedD24Offset = 21u << 16u;
constexpr std::size_t ForcedD32Offset = 22u << 16u;
constexpr std::size_t ForcedListOffset = 23u << 16u;
constexpr std::size_t ForcedRefusalOffset = 24u << 16u;
constexpr std::size_t ForcedFallbackOffset = 25u << 16u;
constexpr std::uint32_t ArrayLayers = 3;

struct FormatFilter {
    VkPhysicalDevice real = VK_NULL_HANDLE;
    PFN_vkGetPhysicalDeviceFormatProperties format = nullptr;
    PFN_vkGetPhysicalDeviceImageFormatProperties image = nullptr;
    VkFormat hiddenA = VK_FORMAT_UNDEFINED;
    VkFormat hiddenB = VK_FORMAT_UNDEFINED;
    bool refuseAliasImages = false;
};

FormatFilter formatFilter;

VKAPI_ATTR void VKAPI_CALL FilteredFormat(VkPhysicalDevice, VkFormat format, VkFormatProperties* properties) {
    formatFilter.format(formatFilter.real, format, properties);
    if (format == formatFilter.hiddenA || format == formatFilter.hiddenB) *properties = {};
}

VKAPI_ATTR VkResult VKAPI_CALL ForwardImageFormat(VkPhysicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* properties) {
    if (formatFilter.refuseAliasImages && (format == VK_FORMAT_R16_SNORM || format == VK_FORMAT_R16_SINT || format == VK_FORMAT_R16_SFLOAT)) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    return formatFilter.image(formatFilter.real, format, type, tiling, usage, flags, properties);
}

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
    state.color = {reinterpret_cast<std::uintptr_t>(block + MultisampledColorOffset), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, 1u << 16u, 0xe4u};
    state.color.tileMode = AgcDriver::Graphics::ColorTileMode::RenderTarget;
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

void AliasStorageColorControl(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    auto* texels = block + AliasControlOffset;
    const std::span<std::uint32_t> output(reinterpret_cast<std::uint32_t*>(block + OutputOffset), Threads);
    const auto context = device.graphicsContext();
    for (const std::uint32_t format : {8u, 11u, 13u}) {
        const std::uint16_t raw = format == 8u ? 0x7fffu : format == 11u ? 0xffffu : 0x3c00u;
        const auto expected = format == 11u ? 0xffffu : std::bit_cast<std::uint32_t>(1.0f);
        std::vector<std::uint16_t> values(static_cast<std::size_t>(Width) * Height, raw);
        std::memcpy(texels, values.data(), values.size() * sizeof(std::uint16_t));
        const auto program = PlaneProgram(device, ScaleDepthCode, output, texels, format, 1.0f);
        if (!AgcDriver::Graphics::StorageFormatAvailable(context, format)) continue;
        RunPlane(device, program, ScaleDepthCode, output, expected, "ordinary colour R16 storage read");
    }
}

void AliasStorageSampledControl(AgcDriver::VulkanDevice& device, const Shaders& shaders, std::uint8_t* block) {
    auto* texels = block + AliasOffset;
    const std::span<std::uint32_t> output(reinterpret_cast<std::uint32_t*>(block + OutputOffset), Threads);
    const auto context = device.graphicsContext();
    const auto address = reinterpret_cast<std::uintptr_t>(texels);
    std::vector<std::uint16_t> sentinel(static_cast<std::size_t>(Width) * Height, 0x3c00u);
    std::memcpy(texels, sentinel.data(), sentinel.size() * sizeof(std::uint16_t));
    const Surface surface{address, 0, 0, false, VK_FORMAT_D16_UNORM_S8_UINT};
    Require(Draw(device, shaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the resident depth alias producer did not cover the surface");
    Require(AgcDriver::Graphics::DepthSurfaceAt(address), "the alias address has no resident depth surface");
    if (context.hostImportAlignment != 0u) {
        Require(AgcDriver::Graphics::HostImportFor(context, address, static_cast<std::size_t>(Width) * Height * sizeof(std::uint16_t)) != nullptr, "the sampled alias was not host-imported");
        Require(AgcDriver::Graphics::HostImportCovers(context, address, static_cast<std::size_t>(Width) * Height * sizeof(std::uint16_t)), "the sampled alias host import does not cover its surface");
    }
    for (const std::uint32_t format : {7u, 8u, 11u, 13u}) {
        const auto words = TextureDescriptor(texels, format);
        const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
        const auto depthStorage = AgcDriver::Graphics::DepthSurfaceStorage(context, words, resource, 0);
        Require(depthStorage != nullptr, "the resident depth alias storage was not selected");
        std::vector<std::uint32_t> userData(16, 0u);
        const auto buffer = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size_bytes()));
        const auto texture = TextureDescriptor(texels, format, Type2DArray);
        std::copy(buffer.begin(), buffer.end(), userData.begin());
        std::copy(texture.begin(), texture.end(), userData.begin() + 4);
        userData[12] = 0u;
        userData[14] = std::bit_cast<std::uint32_t>(1.0f / 128.0f);
        const auto program = Compile(device, LoadLayerCode, userData);
        bool storage = false;
        for (const auto& binding : program.bindings) storage = storage || binding.kind == ShaderRecompiler::DescriptorKind::StorageImage;
        Require(!storage, "the sampled alias control compiled a storage binding");
        std::fill(output.begin(), output.end(), 0xdeadbeefu);
        device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(LoadLayerCode.data()));
        device.WaitIdle();
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            if (format == 11u) {
                Require(output[lane] == 0x8000u, "the sampled R16_UINT control differs at texel " + std::to_string(lane));
                continue;
            }
            const auto value = std::bit_cast<float>(output[lane]);
            if (format == 7u) {
                Require(std::abs(value - 0.5f) <= 1e-5f, "the sampled UNORM control differs at texel " + std::to_string(lane));
            } else if (format == 8u) {
                Require(std::abs(value - (0x3c00u / 32767.0f)) <= 1e-4f, "the sampled SNORM control read the resident depth instead of guest RAM at texel " + std::to_string(lane));
            } else {
                Require(value == 1.0f, "the sampled SFLOAT control read the resident depth instead of guest RAM at texel " + std::to_string(lane));
            }
        }
        Require(texels[0] == 0x00u && texels[1] == 0x3cu, "the sampled control changed guest RAM");
    }
}

void AliasStorageWriteCase(AgcDriver::VulkanDevice& device, const Shaders& shaders, std::uint8_t* block) {
    auto* texels = block + AliasWriteOffset;
    const std::span<std::uint32_t> output(reinterpret_cast<std::uint32_t*>(block + OutputOffset), Threads);
    std::vector<std::uint16_t> sentinel(static_cast<std::size_t>(Width) * Height, 0x1111u);
    std::memcpy(texels, sentinel.data(), sentinel.size() * sizeof(std::uint16_t));
    const Surface surface{reinterpret_cast<std::uintptr_t>(texels), 0, 0, false, VK_FORMAT_D16_UNORM_S8_UINT};
    Require(Draw(device, shaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the resident alias write producer did not cover the surface");
    Require(AgcDriver::Graphics::DepthSurfaceAt(reinterpret_cast<std::uintptr_t>(texels)), "the alias write address has no resident depth surface");
    Require(texels[0] == 0x11u && texels[1] == 0x11u, "the resident alias write read the guest sentinel before its dispatch");
    const auto rawRead = [&] {
        std::vector<std::uint32_t> loadData(16, 0u);
        const auto rawTexture = TextureDescriptor(texels, Format16Uint, Type2DArray);
        const auto buffer = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size_bytes()));
        std::copy(buffer.begin(), buffer.end(), loadData.begin());
        std::copy(rawTexture.begin(), rawTexture.end(), loadData.begin() + 4);
        loadData[12] = 0u;
        loadData[14] = std::bit_cast<std::uint32_t>(1.0f / 128.0f);
        const auto load = Compile(device, LoadLayerCode, loadData);
        std::fill(output.begin(), output.end(), 0xdeadbeefu);
        device.Dispatch(load, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(LoadLayerCode.data()));
        device.WaitIdle();
    };
    for (const std::uint32_t format : {7u, 8u, 11u, 12u, 13u}) {
        Require(Draw(device, shaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == Covered, "the resident alias write producer did not cover the surface again");
        rawRead();
        for (std::uint32_t lane = 0; lane < Threads; ++lane) Require(output[lane] == 0x8000u, "the resident alias write started from raw depth " + std::to_string(output[lane]) + " instead of the resident 0x8000 at texel " + std::to_string(lane));
        std::vector<std::uint32_t> userData(16, 0u);
        const auto texture = TextureDescriptor(texels, format, Type2D, TileZ64KBX);
        std::copy(texture.begin(), texture.end(), userData.begin() + 4);
        const auto store = Compile(device, StoreCode, userData);
        bool storage = false;
        bool uintStorage = false;
        for (const auto& binding : store.bindings) {
            if (binding.kind != ShaderRecompiler::DescriptorKind::StorageImage) continue;
            storage = true;
            uintStorage = binding.binding == ShaderRecompiler::RuntimeAbi::ImageStorageTableBinding ? (binding.imageUintStorage.size() > 0u && binding.imageUintStorage[0]) : ShaderRecompiler::RuntimeAbi::UintStorageImageBinding(binding.binding);
        }
        Require(storage, "the resident alias store compiled no storage binding");
        Require(uintStorage == (format == 11u || format == 12u || format == 13u), "the resident alias store selected the wrong storage kind");
        device.Dispatch(store, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreCode.data()));
        device.WaitIdle();
        rawRead();
        Require(output[0] == 0u, "the resident alias store did not write depth format " + std::to_string(format) + " at texel 0");
        AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), Width * Height * sizeof(std::uint16_t), nullptr, "test");
        device.WaitIdle();
        Require(texels[0] == 0u && texels[1] == 0u, "the resident alias store did not save depth format " + std::to_string(format) + " to guest memory");
    }
}

AgcDriver::Graphics::Context ForcedContext(AgcDriver::VulkanDevice& device, VkFormat hiddenA, VkFormat hiddenB, bool refuseAliasImages, bool imageFormatList) {
    static std::uintptr_t serial = 0x1000u;
    const auto base = device.graphicsContext();
    auto context = base;
    context.physical = reinterpret_cast<VkPhysicalDevice>(serial);
    serial += 0x1000u;
    context.formatProperties = FilteredFormat;
    context.imageFormatProperties = ForwardImageFormat;
    context.memoryProperties2 = nullptr;
    context.imageFormatList = imageFormatList;
    formatFilter.real = base.physical;
    formatFilter.format = base.formatProperties;
    formatFilter.image = base.imageFormatProperties;
    formatFilter.hiddenA = hiddenA;
    formatFilter.hiddenB = hiddenB;
    formatFilter.refuseAliasImages = refuseAliasImages;
    return context;
}

void ForcedHostDepthCase(AgcDriver::VulkanDevice& device, std::uint8_t* block, std::size_t offset, VkFormat expectedHost) {
    VkFormatProperties properties{};
    device.graphicsContext().formatProperties(device.graphicsContext().physical, expectedHost, &properties);
    if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0u) return;
    const auto context = ForcedContext(device, VK_FORMAT_D16_UNORM_S8_UINT, expectedHost == VK_FORMAT_D24_UNORM_S8_UINT ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_D24_UNORM_S8_UINT, false, true);
    Require(AgcDriver::Graphics::HostDepthFormat(context, VK_FORMAT_D16_UNORM_S8_UINT) == expectedHost, "the forced depth host format was not selected");
    auto* texels = block + offset;
    const auto address = reinterpret_cast<std::uintptr_t>(texels);
    Require(AgcDriver::Graphics::DepthSurfaceView(context, {address, 0, {Width, Height}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u}) != VK_NULL_HANDLE, "the forced host depth surface was not created");
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels, Format32Float, Type2D, TileZ64KBX);
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const auto store = Compile(device, StoreCode, userData);
    bool storage = false;
    bool uintStorage = true;
    for (const auto& binding : store.bindings) {
        if (binding.kind != ShaderRecompiler::DescriptorKind::StorageImage) continue;
        storage = true;
        uintStorage = ShaderRecompiler::RuntimeAbi::UintStorageImageBinding(binding.binding);
    }
    Require(storage && !uintStorage, "the forced host depth store did not compile a typed storage binding");
    device.Dispatch(store, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreCode.data()));
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(address, Width * Height * sizeof(std::uint32_t), nullptr, "test");
    device.WaitIdle();
    std::uint32_t bits = 0xdeadbeefu;
    std::memcpy(&bits, texels, sizeof(bits));
    Require(bits == 0u, "the forced host depth store did not save zero depth");
}

void ForcedFormatListCase(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    auto context = ForcedContext(device, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, false, false);
    auto* texels = block + ForcedListOffset;
    const auto address = reinterpret_cast<std::uintptr_t>(texels);
    Require(AgcDriver::Graphics::DepthSurfaceView(context, {address, 0, {Width, Height}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u}) != VK_NULL_HANDLE, "the no-list depth surface was not created");
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels, Format16Sfloat, Type2D, TileZ64KBX);
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const auto store = Compile(device, StoreCode, userData);
    bool storage = false;
    bool uintStorage = false;
    for (const auto& binding : store.bindings) {
        if (binding.kind != ShaderRecompiler::DescriptorKind::StorageImage) continue;
        storage = true;
        uintStorage = ShaderRecompiler::RuntimeAbi::UintStorageImageBinding(binding.binding);
    }
    Require(storage && uintStorage, "the no-list SFLOAT write did not compile the raw UINT storage binding");
    device.Dispatch(store, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreCode.data()));
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(address, Width * Height * sizeof(std::uint16_t), nullptr, "test");
    device.WaitIdle();
    Require(texels[0] == 0u && texels[1] == 0u, "the no-list SFLOAT write did not save zero depth");
    const auto words = TextureDescriptor(texels, Format16Sfloat, Type2D, TileZ64KBX);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    const auto exported = AgcDriver::Graphics::DepthSurfaceStorage(context, words, resource, 0);
    Require(exported != nullptr, "the no-list SFLOAT storage was not exported");
    Require(exported->StorageView(0, false, false) != VK_NULL_HANDLE, "the no-list SFLOAT storage has no typed view");
    Require(exported->StorageView(0, false, true) != VK_NULL_HANDLE, "the no-list SFLOAT storage has no raw UINT view");
}

void ForcedRefusalCase(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    auto context = ForcedContext(device, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, true, true);
    auto* refused = block + ForcedRefusalOffset;
    const auto refusedAddress = reinterpret_cast<std::uintptr_t>(refused);
    Require(AgcDriver::Graphics::DepthSurfaceView(context, {refusedAddress, 0, {Width, Height}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u}) != VK_NULL_HANDLE, "the refused depth surface was not created");
    std::vector<std::uint32_t> userData(16, 0u);
    const auto refusedTexture = TextureDescriptor(refused, Format16Snorm, Type2D, TileZ64KBX);
    std::copy(refusedTexture.begin(), refusedTexture.end(), userData.begin() + 4);
    const auto refusedStore = Compile(device, StoreCode, userData);
    const std::span<std::uint32_t> output(reinterpret_cast<std::uint32_t*>(block + OutputOffset), Threads);
    std::fill(output.begin(), output.end(), 0xdeadbeefu);
    ExpectRefusal([&] { device.Dispatch(refusedStore, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreCode.data())); }, "raw depth storage has no typed shader view", "a storage write to SNORM depth without a typed storage view");
    for (const auto value : output) Require(value == 0xdeadbeefu, "a refused SNORM depth storage write reached its output before refusing");
    auto* fallback = block + ForcedFallbackOffset;
    const auto fallbackAddress = reinterpret_cast<std::uintptr_t>(fallback);
    Require(AgcDriver::Graphics::DepthSurfaceView(context, {fallbackAddress, 0, {Width, Height}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u}) != VK_NULL_HANDLE, "the raw fallback depth surface was not created");
    std::vector<std::uint32_t> fallbackData(16, 0u);
    const auto fallbackTexture = TextureDescriptor(fallback, Format16Sfloat, Type2D, TileZ64KBX);
    std::copy(fallbackTexture.begin(), fallbackTexture.end(), fallbackData.begin() + 4);
    const auto fallbackStore = Compile(device, StoreCode, fallbackData);
    bool storage = false;
    bool uintStorage = false;
    for (const auto& binding : fallbackStore.bindings) {
        if (binding.kind != ShaderRecompiler::DescriptorKind::StorageImage) continue;
        storage = true;
        uintStorage = ShaderRecompiler::RuntimeAbi::UintStorageImageBinding(binding.binding);
    }
    Require(storage && uintStorage, "the image-query-refused SFLOAT write did not compile the raw UINT storage binding");
    device.Dispatch(fallbackStore, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreCode.data()));
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(fallbackAddress, Width * Height * sizeof(std::uint16_t), nullptr, "test");
    device.WaitIdle();
    Require(fallback[0] == 0u && fallback[1] == 0u, "the image-query-refused SFLOAT write did not save zero depth");
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
        AliasStorageColorControl(*device, block.Data());
        AliasStorageSampledControl(*device, shaders, block.Data());
        AliasStorageWriteCase(*device, shaders, block.Data());
        TwoViewsCase(*device, block.Data());
        SampledCopyCase(*device, shaders, block.Data());
        ForcedHostDepthCase(*device, block.Data(), ForcedD24Offset, VK_FORMAT_D24_UNORM_S8_UINT);
        ForcedHostDepthCase(*device, block.Data(), ForcedD32Offset, VK_FORMAT_D32_SFLOAT_S8_UINT);
        ForcedFormatListCase(*device, block.Data());
        ForcedRefusalCase(*device, block.Data());
        std::puts("depth storage dispatch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
