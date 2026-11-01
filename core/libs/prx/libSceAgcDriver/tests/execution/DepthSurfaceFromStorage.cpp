#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
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

using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::HtileBytes;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Width;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Format16Unorm = 7;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t TileDepth64KB = 0x18;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::size_t PlaneBytes = 65536;
constexpr std::size_t BlockBytes = 11 * PlaneBytes;
constexpr std::uint32_t RowPasses = 15;

alignas(256) constexpr std::array<std::uint32_t, 9> StoreRowCode{
    0x7e280d00, 0x102828ff, 0x3cf80000, 0x7e3c0300, 0x7e3e0280, 0xf0201108, 0x0001141e, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 10> StoreRowLayerCode{
    0x7e280d00, 0x102828ff, 0x3cf80000, 0x7e3c0300, 0x7e3e0280, 0x7e400280, 0xf0201128, 0x0001141e, 0xbf810000, 0xbf810000,
};

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
        Require(block != nullptr, "depth surface from storage: cannot allocate the guest block");
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

    std::uint8_t* Plane(std::size_t index) { return block + index * PlaneBytes; }

private:
    std::uint8_t* block = nullptr;
};

std::uint64_t Address(const std::uint8_t* plane) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(plane));
}

std::array<std::uint32_t, 8> StorageView(std::uint64_t address, std::uint32_t format, std::uint32_t layers, std::uint32_t baseLevel = 0, std::uint32_t lastLevel = 0) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (baseLevel << 12u) | (lastLevel << 16u) | (TileDepth64KB << 20u) | ((layers > 1u ? Type2DArray : Type2D) << 28u),
        layers - 1u, lastLevel << 4u, 0u, 0u,
    };
}

void Fill(std::uint8_t* plane, VkFormat format) {
    if (format == VK_FORMAT_D16_UNORM) {
        const std::uint16_t quarter = 0x4000u;
        for (std::size_t offset = 0; offset < PlaneBytes; offset += sizeof(quarter)) std::memcpy(plane + offset, &quarter, sizeof(quarter));
        return;
    }
    const float quarter = 0.25f;
    for (std::size_t offset = 0; offset < PlaneBytes; offset += sizeof(quarter)) std::memcpy(plane + offset, &quarter, sizeof(quarter));
}

void StoreRow(AgcDriver::VulkanDevice& device, std::uint8_t* plane, VkFormat format, std::uint32_t layers, std::uint32_t level = 0) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto view = StorageView(Address(plane), format == VK_FORMAT_D16_UNORM ? Format16Unorm : Format32Float, layers, level, level);
    std::copy(view.begin(), view.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code = layers > 1u ? std::span<const std::uint32_t>(StoreRowLayerCode) : std::span<const std::uint32_t>(StoreRowCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto program = ShaderRecompiler::Recompile(request);
    device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

DepthFastClearHarness::Surface Plane(std::uint8_t* plane, VkFormat format, std::uint64_t htile = 0) {
    return {Address(plane), 0, htile, false, format};
}

std::vector<std::string> failures;

void RequireCount(std::uint32_t covered, std::uint32_t expected, const std::string& what) {
    if (covered != expected) failures.push_back(what + ": " + std::to_string(covered) + " pixels passed, expected " + std::to_string(expected));
}

void NewSurfaceCase(AgcDriver::VulkanDevice& device, const DepthFastClearHarness::Shaders& shaders, std::uint8_t* plane, VkFormat format, bool flushed, const std::string& what, std::uint32_t layers = 1) {
    for (std::uint32_t layer = 0; layer < layers; ++layer) Fill(plane + layer * PlaneBytes, format);
    StoreRow(device, plane, format, layers);
    if (layers > 1u) {
        Require(AgcDriver::Graphics::StorageTexture::FindPending(Address(plane), PlaneBytes) != nullptr && AgcDriver::Graphics::StorageTexture::FindLive(Address(plane), PlaneBytes) == nullptr, what + ": the stored row is not pending in a storage image larger than the plane, or a cached image of the plane's size holds it");
    } else if (flushed) {
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(plane), PlaneBytes, nullptr, "test");
        device.WaitIdle();
        Require(AgcDriver::Graphics::StorageTexture::FindPending(Address(plane), PlaneBytes) == nullptr && AgcDriver::Graphics::StorageTexture::FindLive(Address(plane), PlaneBytes) != nullptr, what + ": the stored row is not held by a flushed, live storage image");
    } else {
        Require(AgcDriver::Graphics::StorageTexture::FindPending(Address(plane), PlaneBytes) != nullptr, what + ": the stored row is not pending in a storage image");
    }
    RequireCount(DepthFastClearHarness::Draw(device, shaders, Plane(plane, format), {}), RowPasses, what);
}

void PendingChainCase(AgcDriver::VulkanDevice& device, const DepthFastClearHarness::Shaders& shaders, std::uint8_t* plane) {
    Fill(plane, VK_FORMAT_D32_SFLOAT);
    StoreRow(device, plane, VK_FORMAT_D32_SFLOAT, 1, 1);
    AgcDriver::Graphics::StorageTexture::FlushPending(Address(plane), PlaneBytes, nullptr, "test");
    device.WaitIdle();
    Require(AgcDriver::Graphics::StorageTexture::FindLive(Address(plane), PlaneBytes) != nullptr, "the two-level chain's storage image is not held after its first write was flushed");
    static_cast<void>(DepthFastClearHarness::Draw(device, shaders, Plane(plane, VK_FORMAT_D32_SFLOAT), {}));
    Require(AgcDriver::Graphics::DepthSurfaceAt(Address(plane)), "the drawn depth surface is not live");
    StoreRow(device, plane, VK_FORMAT_D32_SFLOAT, 1, 1);
    Require(AgcDriver::Graphics::StorageTexture::FindPending(Address(plane), 1) != nullptr, "level 1 of a two-level chain over the depth plane is not pending in a storage image");
    const auto words = StorageView(Address(plane), Format32Float, 1, 0, 1);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    std::shared_ptr<AgcDriver::Graphics::Texture> texture;
    try {
        texture = AgcDriver::Graphics::DepthSurfaceTexture(AgcDriver::VulkanDeviceTestAccess::ContextOf(device), words, resource, {});
    } catch (const std::runtime_error& error) {
        Require(false, std::string("a two-level view of a depth plane that a pending storage write covers was refused: ") + error.what());
    }
    Require(texture == nullptr, "a two-level view of a depth plane that a pending storage write covers was served from the depth plane");
    Require(!AgcDriver::Graphics::DepthSurfaceAt(Address(plane)), "a pending storage write over the depth plane, then a two-level view: the depth surface stayed live");
}

struct ChainAfterRetire {
    std::vector<std::uint8_t> level0;
    std::vector<std::uint8_t> level1;
    bool disjoint = false;
};

ChainAfterRetire RetireOverPendingChain(AgcDriver::VulkanDevice& device, const DepthFastClearHarness::Shaders& shaders, std::uint8_t* plane, bool depthWrite) {
    Fill(plane, VK_FORMAT_D32_SFLOAT);
    StoreRow(device, plane, VK_FORMAT_D32_SFLOAT, 1, 1);
    AgcDriver::Graphics::StorageTexture::FlushPending(Address(plane), PlaneBytes, nullptr, "test");
    device.WaitIdle();
    static_cast<void>(DepthFastClearHarness::Draw(device, shaders, Plane(plane, VK_FORMAT_D32_SFLOAT), {.depthTest = true, .depthWrite = depthWrite, .depthCompare = VK_COMPARE_OP_ALWAYS}));
    Require(AgcDriver::Graphics::DepthSurfaceAt(Address(plane)), "the drawn depth surface of the retire case is not live");
    StoreRow(device, plane, VK_FORMAT_D32_SFLOAT, 1, 1);
    const auto words = StorageView(Address(plane), Format32Float, 1, 0, 1);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    const auto texture = AgcDriver::Graphics::DepthSurfaceTexture(AgcDriver::VulkanDeviceTestAccess::ContextOf(device), words, resource, {});
    Require(texture == nullptr && !AgcDriver::Graphics::DepthSurfaceAt(Address(plane)), "a two-level view over a drawn depth plane under a pending storage write did not retire the surface");
    AgcDriver::Graphics::StorageTexture::FlushPending(Address(plane), PlaneBytes, nullptr, "test");
    device.WaitIdle();
    AgcDriver::GuestMemory::FlushGpuWrites(Address(plane), PlaneBytes);
    const auto geometry = AgcDriver::Graphics::DescribeSurface(resource);
    const auto& first = geometry.mips.at(0);
    const auto& second = geometry.mips.at(1);
    ChainAfterRetire result;
    result.level0.assign(plane + first.tiledOffset, plane + first.tiledOffset + first.tiledSize);
    result.level1.assign(plane + second.tiledOffset, plane + second.tiledOffset + second.tiledSize);
    result.disjoint = first.tiledOffset + first.tiledSize <= second.tiledOffset || second.tiledOffset + second.tiledSize <= first.tiledOffset;
    return result;
}

void RetireStoresDepthCase(AgcDriver::VulkanDevice& device, const DepthFastClearHarness::Shaders& shaders, std::uint8_t* control, std::uint8_t* drawn) {
    const auto kept = RetireOverPendingChain(device, shaders, control, false);
    const auto stored = RetireOverPendingChain(device, shaders, drawn, true);
    Require(stored.level1 == kept.level1, "a retire that stored depth drawn only on the GPU changed level 1 of the chain a pending storage write produced");
    if (!stored.disjoint) return;
    std::size_t half = 0;
    for (std::size_t offset = 0; offset + sizeof(float) <= stored.level0.size(); offset += sizeof(float)) {
        float value = 0.0f;
        std::memcpy(&value, stored.level0.data() + offset, sizeof(value));
        half += value == 0.5f;
    }
    Require(half == stored.level0.size() / sizeof(float), "the retire left " + std::to_string(stored.level0.size() / sizeof(float) - half) + " of the drawn depth plane's texels without the depth drawn on the GPU");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto shaders = DepthFastClearHarness::Compile(*device);
        GuestBlock block;
        NewSurfaceCase(*device, shaders, block.Plane(0), VK_FORMAT_D32_SFLOAT, false, "a new D32 depth surface over a pending R32F storage write");
        NewSurfaceCase(*device, shaders, block.Plane(1), VK_FORMAT_D16_UNORM, false, "a new D16 depth surface over a pending R16 unorm storage write");
        NewSurfaceCase(*device, shaders, block.Plane(2), VK_FORMAT_D32_SFLOAT, true, "a new D32 depth surface over a flushed R32F storage write");
        NewSurfaceCase(*device, shaders, block.Plane(7), VK_FORMAT_D32_SFLOAT, false, "a new D32 depth surface over a pending R32F storage write through a two-layer 2D array view", 2);

        auto* cleared = block.Plane(3);
        const auto htile = Address(block.Plane(4));
        Fill(cleared, VK_FORMAT_D32_SFLOAT);
        StoreRow(*device, cleared, VK_FORMAT_D32_SFLOAT, 1);
        AgcDriver::Graphics::NoteHtileDepthClear(htile, DepthFastClearHarness::HtileBytes);
        RequireCount(DepthFastClearHarness::Draw(*device, shaders, Plane(cleared, VK_FORMAT_D32_SFLOAT, htile), {}), Covered, "a new D32 depth surface whose HTILE was cleared after a storage write");

        auto* stenciled = block.Plane(5);
        const DepthFastClearHarness::Surface depthStencil{Address(stenciled), Address(block.Plane(6)), 0, false, VK_FORMAT_D32_SFLOAT_S8_UINT};
        constexpr VkStencilOpState equalZero{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffu, 0u, 0u};
        Fill(stenciled, VK_FORMAT_D32_SFLOAT);
        StoreRow(*device, stenciled, VK_FORMAT_D32_SFLOAT, 1);
        RequireCount(DepthFastClearHarness::Draw(*device, shaders, depthStencil, {}), RowPasses, "a new D32S8 depth surface over a pending R32F storage write");
        RequireCount(DepthFastClearHarness::Draw(*device, shaders, depthStencil, {.depthTest = false, .stencilTest = true, .stencil = equalZero}), Covered, "the stencil plane of a new D32S8 depth surface that took a storage write, tested EQUAL to its clear value 0");
        PendingChainCase(*device, shaders, block.Plane(8));
        RetireStoresDepthCase(*device, shaders, block.Plane(9), block.Plane(10));

        if (!failures.empty()) {
            std::string text;
            for (const auto& failure : failures) text += failure + "\n";
            throw std::runtime_error(text);
        }
        std::puts("depth surface from storage tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
