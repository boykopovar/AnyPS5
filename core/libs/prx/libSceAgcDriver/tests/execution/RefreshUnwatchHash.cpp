#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
constexpr std::uint32_t Width = 512;
constexpr std::uint32_t Height = 1024;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t Page = 0x1000;
constexpr std::size_t Block = 0x10000;
constexpr std::size_t SurfaceBytes = std::size_t{Width} * Height * 4u;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Stored = 0x12345678u;

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, Stored, 0xf0200108, 0x00020102, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* texels) {
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

void Store(const void* texels) {
    std::vector<std::uint32_t> words;
    const auto registers = [&](const std::uint32_t first, const std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    std::array<std::uint32_t, 16> userData{};
    const auto texture = TextureDescriptor(texels);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    const std::array<std::uint32_t, 3> shape{8, 1, 1};
    registers(0x207, shape);
    const auto code = AddressOf(StoreCode.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> layout{static_cast<std::uint32_t>(userData.size()) << 1u};
    registers(0x213, layout);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
}

class WatchedBlock {
public:
    explicit WatchedBlock(std::size_t size) : bytes(size) {
#ifdef _WIN32
        data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block));
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
#else
        void* mapped = mmap(nullptr, bytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + Block - 1) & ~std::uintptr_t{Block - 1};
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned != begin + Block) munmap(reinterpret_cast<void*>(aligned + bytes), begin + Block - aligned);
            data = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
        }
#endif
        Require(data != nullptr, "refresh unwatch hash: cannot allocate a watched block");
        GuestAllocations::Mutation().Add(data, bytes, true, true, true);
    }

    ~WatchedBlock() {
        GuestAllocations::Mutation().Remove(data);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(data, bytes);
        GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
#else
        munmap(data, bytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, bytes);
#endif
    }

    WatchedBlock(const WatchedBlock&) = delete;
    WatchedBlock& operator=(const WatchedBlock&) = delete;

    std::uint8_t* Data() const { return data; }
    std::uint64_t Address() const { return AddressOf(data); }

private:
    std::size_t bytes;
    std::uint8_t* data = nullptr;
};

std::uint8_t Pattern(std::size_t offset) {
    return static_cast<std::uint8_t>(0x40u + offset % 0x3bu);
}

struct Unwatching {
    std::uint64_t address = 0;
    bool reached = false;
    bool watchedBefore = false;
};

Unwatching unwatching;

void UnwatchDuringRefresh(std::uint64_t address, std::size_t bytes) {
    if (unwatching.reached || address != unwatching.address || bytes != SurfaceBytes) return;
    unwatching.reached = true;
    unwatching.watchedBefore = AgcDriver::GuestMemory::Watched(address, bytes);
    AgcDriver::GuestMemory::Unwatch(address, bytes);
}

bool Run() {
    WatchedBlock surface(SurfaceBytes);
    WatchedBlock other(Block);
    struct WaitForWork {
        ~WaitForWork() {
            StorageTexture::SetRefreshUnchangedHookForTest(nullptr);
            AgcDriverWaitIdle_nid_postfix();
        }
    } waitForWork;
    const auto address = surface.Address();
    if (!AgcDriver::GuestMemory::Watched(address, SurfaceBytes)) {
        std::puts("skipped, the surface is not write-watched");
        return false;
    }
    auto* texels = reinterpret_cast<std::uint32_t*>(surface.Data());
    std::fill(texels, texels + SurfaceBytes / 4u, Untouched);
    Store(texels);
    AgcDriverWaitIdle_nid_postfix();
    std::unique_lock hold(AgcDriver::GuestMemory::GpuMutex());
    StorageTexture::FlushAllPending("refresh unwatch hash fixture");
    hold.unlock();
    {
        std::vector<std::byte> settled(SurfaceBytes);
        AgcDriver::GuestMemory::Read(address, settled);
    }
    Require(static_cast<const volatile std::uint32_t*>(texels)[0] == Stored, "refresh unwatch hash: the first results did not reach guest memory");
    if (!AgcDriver::GuestMemory::Watched(address, SurfaceBytes)) {
        std::puts("skipped, the surface's host import is compared, not write-watched");
        return false;
    }
    hold.lock();
    const auto image = StorageTexture::FindLive(address, SurfaceBytes);
    Require(image != nullptr, "refresh unwatch hash: the surface has no storage image");
    Require(StorageTexture::FindPending(address, SurfaceBytes) == nullptr, "refresh unwatch hash: the first results are still pending");
    hold.unlock();
    for (std::size_t round = 0; round < 4; ++round) {
        static_cast<volatile std::uint8_t*>(other.Data())[round] = static_cast<std::uint8_t>(round + 1);
        Require(AgcDriver::GuestMemory::CollectWritesUncached(other.Address(), Block) != 0, "refresh unwatch hash: the other block is not watched");
    }
    unwatching = {address, false, false};
    StorageTexture::SetRefreshUnchangedHookForTest(&UnwatchDuringRefresh);
    hold.lock();
    image->Refresh();
    StorageTexture::SetRefreshUnchangedHookForTest(nullptr);
    Require(unwatching.reached && unwatching.watchedBefore, "refresh unwatch hash: the refresh did not find the watched surface unchanged");
    Require(!AgcDriver::GuestMemory::Watched(address, SurfaceBytes), "refresh unwatch hash: the surface is still write-watched");
    image->MarkDirty();
    Require(StorageTexture::FindPending(address, SurfaceBytes) == image, "refresh unwatch hash: the results are not pending");
    hold.unlock();
    auto* written = surface.Data() + 2 * Block;
    for (std::size_t offset = 0; offset < Page; ++offset) static_cast<volatile std::uint8_t*>(written)[offset] = Pattern(offset);
    hold.lock();
    StorageTexture::FlushAllPending("refresh unwatch hash fixture");
    hold.unlock();
    std::vector<std::byte> whole(SurfaceBytes);
    AgcDriver::GuestMemory::Read(address, whole);
    Require(static_cast<const volatile std::uint32_t*>(texels)[0] == Stored, "refresh unwatch hash: the results did not reach a block the CPU left alone");
    std::size_t lost = 0;
    for (std::size_t offset = 0; offset < Page; ++offset) {
        if (static_cast<const volatile std::uint8_t*>(written)[offset] != Pattern(offset)) ++lost;
    }
    Require(lost == 0, "refresh unwatch hash: the write-back stored the results over " + std::to_string(lost) + " bytes the CPU wrote while they were pending");
    return true;
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;
        {
            WatchedBlock probe(SurfaceBytes);
            Require(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(probe.Data()))).guestBytes == SurfaceBytes, "refresh unwatch hash: unexpected surface size");
        }
        if (!Run()) return VulkanTestSkipped;
        AgcDriverShutdown_nid_postfix();
        std::puts("refresh unwatch hash tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
