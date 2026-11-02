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
#include <exception>
#include <iostream>
#include <mutex>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t Block = 0x10000;
constexpr std::size_t SurfaceBytes = 4 * Block;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Stored = 0x12345678u;
constexpr std::uint32_t StoredAgain = 0x0badf00du;
constexpr auto IdleStoreWindow = std::chrono::milliseconds(500);

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, Stored, 0xf0200108, 0x00020102, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> StoreAgainCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, StoredAgain, 0xf0200108, 0x00020102, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint32_t* texels) {
    const auto address = AddressOf(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Store(const std::uint32_t* texels, std::span<const std::uint32_t> code) {
    std::array<std::uint32_t, 16> userData{};
    const auto texture = TextureDescriptor(texels);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::vector<std::uint32_t> words;
    const auto registers = [&](const std::uint32_t first, const std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{8, 1, 1};
    registers(0x207, shape);
    const auto address = AddressOf(code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> layout{static_cast<std::uint32_t>(userData.size()) << 1u};
    registers(0x213, layout);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
}

std::uint32_t* MapSurface() {
#ifdef _WIN32
    auto* texels = static_cast<std::uint32_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, Block));
    if (texels != nullptr) GuestArena::GuestArenaCommit_nid_postfix(texels, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
    return texels;
#else
    void* mapped = mmap(nullptr, SurfaceBytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED) return nullptr;
    const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
    const auto aligned = (begin + Block - 1) & ~std::uintptr_t{Block - 1};
    if (aligned != begin) munmap(mapped, aligned - begin);
    if (aligned != begin + Block) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + Block - aligned);
    auto* texels = reinterpret_cast<std::uint32_t*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(texels, SurfaceBytes);
    return texels;
#endif
}

void ReleaseSurface(std::uint32_t* texels) {
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(texels, SurfaceBytes);
    GuestArena::GuestArenaRelease_nid_postfix(texels, SurfaceBytes);
#else
    munmap(texels, SurfaceBytes);
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(texels, SurfaceBytes);
#endif
}

bool Holds(const std::uint32_t* texels, std::uint32_t value) {
    const auto* words = static_cast<const volatile std::uint32_t*>(texels);
    for (std::size_t index = 0; index < SurfaceBytes / 4u; ++index) {
        if (words[index] == value) return true;
    }
    return false;
}

bool HoldLive(std::unique_lock<AgcDriver::GuestMemory::GpuMutexType>& hold, const std::uint32_t* texels, std::uint32_t result) {
    const auto address = AddressOf(texels);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (true) {
        hold.lock();
        const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> range{{{address, address + SurfaceBytes}}};
        if (StorageTexture::FindLive(address, SurfaceBytes) != nullptr && StorageTexture::AnyPendingOverlaps(range)) return true;
        hold.unlock();
        if (Holds(texels, result)) return false;
        Require(std::chrono::steady_clock::now() < deadline, "idle store over unmap: the result neither became pending nor reached guest memory");
        std::this_thread::yield();
    }
}

enum class Outcome { Settled, Missed, NotRun };

Outcome Run() {
    auto* texels = MapSurface();
    Require(texels != nullptr, "idle store over unmap: cannot allocate the guest surface");
    std::fill(texels, texels + SurfaceBytes / 4u, Untouched);
    GuestAllocations::Mutation().Add(texels, SurfaceBytes, true, true, true);
    bool mapped = true;
    struct Unmapped {
        std::uint32_t* texels;
        bool& mapped;
        ~Unmapped() {
            if (!mapped) return;
            AgcDriverWaitIdle_nid_postfix();
            GuestAllocations::Mutation().Remove(texels);
            ReleaseSurface(texels);
        }
    } unmapped{texels, mapped};
    Require(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(texels))).guestBytes == SurfaceBytes, "idle store over unmap: unexpected surface size");
    const auto address = AddressOf(texels);
    if (!AgcDriver::GuestMemory::Watched(address, SurfaceBytes)) {
        std::printf("skipped, the surface is not write-watched\n");
        return Outcome::NotRun;
    }
    AgcDriver::GuestMemory::Unwatch(address, SurfaceBytes);
    Require(!AgcDriver::GuestMemory::Watched(address, SurfaceBytes), "idle store over unmap: the surface is still write-watched");
    std::unique_lock hold(AgcDriver::GuestMemory::GpuMutex(), std::defer_lock);
    Store(texels, StoreCode);
    if (!HoldLive(hold, texels, Stored)) return Outcome::Missed;
    StorageTexture::FlushAllPending("idle store over unmap fixture");
    hold.unlock();
    {
        std::vector<std::byte> first(SurfaceBytes);
        AgcDriver::GuestMemory::Read(address, first);
    }
    Require(static_cast<const volatile std::uint32_t*>(texels)[0] == Stored, "idle store over unmap: the first result was not stored");
    hold.lock();
    const auto image = StorageTexture::FindLive(address, SurfaceBytes);
    const bool saved = image != nullptr && image->GuestSnapshotValid();
    hold.unlock();
    Require(image != nullptr, "idle store over unmap: the surface has no storage image");
    if (!saved) {
        std::printf("skipped, the first store left no saved guest bytes to compare with\n");
        return Outcome::NotRun;
    }
    Store(texels, StoreAgainCode);
    if (!HoldLive(hold, texels, StoredAgain)) return Outcome::Missed;
    {
        GuestAllocations::Mutation mutation;
        mutation.Unmap(texels, SurfaceBytes, [](const void* piece, std::size_t, const void*, bool) {
            ReleaseSurface(static_cast<std::uint32_t*>(const_cast<void*>(piece)));
        });
        mapped = false;
        hold.unlock();
        std::this_thread::sleep_for(IdleStoreWindow);
        Require(StorageTexture::AnyPending(), "idle store over unmap: the queue's idle store ran while a guest unmap of the surface was in progress");
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (StorageTexture::AnyPending() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(!StorageTexture::AnyPending(), "idle store over unmap: the queue's idle store did not settle the results of the unmapped surface");
    AgcDriverWaitIdle_nid_postfix();
    return Outcome::Settled;
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
        auto outcome = Outcome::Missed;
        for (std::uint32_t attempt = 0; attempt < 64u && outcome == Outcome::Missed; ++attempt) outcome = Run();
        Require(outcome != Outcome::Missed, "idle store over unmap: the result was never held pending");
        if (outcome == Outcome::NotRun) return VulkanTestSkipped;
        std::puts("idle store over unmap tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
