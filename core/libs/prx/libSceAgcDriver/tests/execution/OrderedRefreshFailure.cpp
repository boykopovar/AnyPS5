#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
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
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t RowThreads = 32;
constexpr std::uint32_t TiledSide = 128;
constexpr std::uint32_t LinearWidth = 256;
constexpr std::uint32_t LinearHeight = 64;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t TileLinear = 0;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Initial = 0x5a5a5a5au;
constexpr std::uint32_t Stored = 0x12345678u;

alignas(256) constexpr std::array<std::uint32_t, 5> StoreRowsCode{
    0x7e020210, 0x7e04020c, 0xf0201108, 0x00010200, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> LoadRowCode{
    0x7e020280, 0xf0001108, 0x00010300, 0x34080082, 0xbf8c3f70, 0xe0701000, 0x80000304, 0x7e0202bf, 0xf0201108, 0x00010300, 0xbf810000,
};

constexpr std::uint32_t StoredBackRow = 63;

alignas(256) std::array<std::uint32_t, RowThreads> Output{};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

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
        Require(block != nullptr, "ordered refresh failure: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
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

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint8_t* texels, std::uint32_t width, std::uint32_t height, std::uint32_t tile) {
    const auto address = AddressOf(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((width - 1u) & 3u) << 30u),
        ((width - 1u) >> 2u) | ((height - 1u) << 14u),
        0xfacu | (tile << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Dispatch(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, std::uint32_t threads, std::uint32_t rows) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {false, rows > 1, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, rows, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
}

void StoreAll(AgcDriver::VulkanDevice& device, const std::uint8_t* texels) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels, TiledSide, TiledSide, TileR64KBX);
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    userData[12] = Stored;
    Dispatch(device, StoreRowsCode, userData, TiledSide, TiledSide);
}

void LoadRow(AgcDriver::VulkanDevice& device, const std::uint8_t* texels) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    const auto texture = TextureDescriptor(texels, LinearWidth, LinearHeight, TileLinear);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    Dispatch(device, LoadRowCode, userData, RowThreads, 1);
}

}

int main() {
    try {
        GuestBlock block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestAllocations::Mutation().Add(Output.data(), sizeof(Output), true, true, true);
        auto* texels = block.Data();
        const auto surface = AddressOf(texels);
        std::fill_n(reinterpret_cast<std::uint32_t*>(texels), BlockBytes / 4u, Initial);
        AgcDriver::GuestMemory::CollectWrites(surface, BlockBytes);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            LoadRow(*device, texels);
            StorageTexture::FlushPending(surface, BlockBytes, nullptr, "test");
            device->WaitIdle();
            Require(std::all_of(Output.begin(), Output.end(), [](std::uint32_t value) { return value == Initial; }), "ordered refresh failure: the linear view did not read the initial guest bytes");
            StoreAll(*device, texels);
            Require(StorageTexture::FindPending(surface, BlockBytes) != nullptr, "ordered refresh failure: the tiled store left no pending image");
            static_cast<void>(StorageTexture::TakeOrderedSkipCounts());
            static_cast<void>(StorageTexture::TakeFlushFailureForTest());
            StorageTexture::SetFlushFailureForTest(true);
            try {
                LoadRow(*device, texels);
            } catch (const std::exception& error) {
                Require(std::strstr(error.what(), "injected failure") != nullptr, std::string("ordered refresh failure: the load failed for another reason: ") + error.what());
            }
            StorageTexture::SetFlushFailureForTest(false);
            const auto fired = StorageTexture::TakeFlushFailureForTest();
            Require(fired != 0, "ordered refresh failure: the load over the pending image never reached the injected flush failure");
            Require(fired == 2, "ordered refresh failure: the injected flush failure fired outside an armed ordered scope");
            Require(!StorageTexture::OrderedFlushArmedForTest(), "ordered refresh failure: the failed refresh left the ordered flush armed on its thread");
            Require(StorageTexture::FindPending(surface, BlockBytes) != nullptr, "ordered refresh failure: the failed flush dropped the pending image");
            static_cast<void>(StorageTexture::TakeOrderedSkipCounts());
            StorageTexture::FlushPending(surface, BlockBytes, nullptr, "test");
            Require(StorageTexture::TakeOrderedSkipCounts().flushes == 0, "ordered refresh failure: a flush after the failed refresh skipped its writer wait");
            device->WaitIdle();
        }
        const auto* words = reinterpret_cast<const std::uint32_t*>(texels);
        for (std::size_t index = 0; index < BlockBytes / 4u; ++index) {
            if (index / LinearWidth == StoredBackRow && index % LinearWidth < RowThreads) continue;
            Require(words[index] == Stored, "ordered refresh failure: guest memory does not hold the tiled store after the flush at dword " + std::to_string(index));
        }
        GuestAllocations::Mutation().Remove(Output.data());
        AgcDriver::Graphics::ClearCachedTextures(device->Device());
        std::puts("ordered refresh failure tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
