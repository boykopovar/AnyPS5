#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::RuntimeSwitch;
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;
namespace RuntimeSwitches = AgcDriver::Graphics::RuntimeSwitches;

constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 2048;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t SurfaceBytes = 16 * BlockBytes;
constexpr std::size_t BlockWords = BlockBytes / 4u;
constexpr std::uint32_t Initial = 0x5a5a5a5au;
constexpr std::uint32_t Stored = 0x12345678u;
constexpr std::uint32_t StoredAgain = 0x0badf00du;
constexpr std::uint32_t Cpu = 0xc0ffee11u;

alignas(256) constexpr std::array<std::uint32_t, 5> StoreRowsCode{
    0x7e020210, 0x7e04020c, 0xf0201108, 0x00010200, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, BlockBytes));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
        constexpr std::uintptr_t alignment = BlockBytes;
        void* mapped = mmap(nullptr, SurfaceBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + SurfaceBytes != begin + SurfaceBytes + alignment) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, SurfaceBytes);
        }
#endif
        Require(block != nullptr, "memo pending collect: cannot allocate the guest surface");
        GuestAllocations::Mutation().Add(block, SurfaceBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, SurfaceBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, SurfaceBytes);
#else
        munmap(block, SurfaceBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, SurfaceBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint8_t* texels) {
    const auto address = AddressOf(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void StoreAll(AgcDriver::VulkanDevice& device, const std::uint8_t* texels, std::uint32_t value) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels);
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    userData[12] = value;
    const std::span<const std::uint32_t> code = StoreRowsCode;
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Width, 1, 1}, 0u, {false, true, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, Height, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
}

struct CpuStore {
    std::uint64_t block = 0;
    std::uint64_t generation = 0;
    bool reached = false;
    bool watched = false;
};

CpuStore cpuStore;

void StoreBeforePendingCollect(std::uint64_t address, std::size_t bytes) {
    if (cpuStore.reached || cpuStore.block < address || cpuStore.block + BlockBytes > address + bytes) return;
    cpuStore.watched = AgcDriver::GuestMemory::Watched(cpuStore.block, BlockBytes);
    std::fill_n(reinterpret_cast<std::uint32_t*>(cpuStore.block), BlockWords, Cpu);
    cpuStore.generation = AgcDriver::GuestMemory::TrackerGeneration();
    cpuStore.reached = true;
}

std::size_t Count(const std::uint32_t* words, std::uint32_t value) {
    return static_cast<std::size_t>(std::count(words, words + BlockWords, value));
}

bool Run(AgcDriver::VulkanDevice& device, std::uint8_t* texels, bool noMemo) {
    const auto surface = AddressOf(texels);
    if (!AgcDriver::GuestMemory::Watched(surface, SurfaceBytes)) {
        std::puts("skipped, the guest surface is not write-watched");
        return false;
    }
    Require(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(texels))).guestBytes == SurfaceBytes, "memo pending collect: unexpected surface size");
    const std::string mode = noMemo ? "with APS5_NO_MEMO_PENDING_COLLECT" : "without APS5_NO_MEMO_PENDING_COLLECT";
    auto* words = reinterpret_cast<std::uint32_t*>(texels);
    std::fill_n(words, SurfaceBytes / 4u, Initial);
    AgcDriver::GuestMemory::CollectWrites(surface, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    StoreAll(device, texels, Stored);
    Require(StorageTexture::FindPending(surface, SurfaceBytes) != nullptr, "memo pending collect: the first store left no pending image");
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
    device.WaitIdle();
    Require(Count(words, Stored) == BlockWords && Count(words + BlockWords, Stored) == BlockWords, "memo pending collect: the first results did not reach guest memory");
    RuntimeSwitches::Assign(RuntimeSwitch::NoMemoPendingCollect, noMemo);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const bool memoized = !noMemo && AgcDriver::GuestMemory::ThreadCollectEpoch() != 0;
    cpuStore = {surface + BlockBytes, 0, false};
    StorageTexture::SetPendingHookForTest(&StoreBeforePendingCollect);
    StoreAll(device, texels, StoredAgain);
    StorageTexture::SetPendingHookForTest(nullptr);
    const bool walked = cpuStore.reached && AgcDriver::GuestMemory::WrittenSince(cpuStore.block, BlockBytes, cpuStore.generation);
    const bool pending = StorageTexture::FindPending(surface, SurfaceBytes) != nullptr;
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
    device.WaitIdle();
    const auto gpuWords = Count(words, StoredAgain);
    const auto cpuWords = Count(words + BlockWords, Cpu);
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
    Require(cpuStore.reached && pending, "memo pending collect: the second store did not make its results pending through a collect of the surface");
    if (cpuStore.watched) {
        Require(walked || memoized, "memo pending collect " + mode + ": APS5_NO_MEMO_PENDING_COLLECT did not walk the units again when their results became pending");
        Require(!walked || !memoized, "memo pending collect " + mode + ": the collect that made the results pending walked the units again after the storage refresh had walked them in the same epoch");
    } else {
        std::puts("the host import of the surface is compared, not watched: only the results are checked");
    }
    Require(gpuWords == BlockWords, "memo pending collect " + mode + ": the results did not reach the block the CPU left alone (" + std::to_string(gpuWords) + " of " + std::to_string(BlockWords) + " words)");
    Require(cpuWords == BlockWords, "memo pending collect " + mode + ": the write-back stored the results over " + std::to_string(BlockWords - cpuWords) + " words of a block the CPU wrote between the storage refresh and the collect that made the results pending");
    return true;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const bool noMemo : {false, true}) {
            GuestBlock block;
            if (!Run(*device, block.Data(), noMemo)) return VulkanTestSkipped;
        }
        std::puts("memo pending collect tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
