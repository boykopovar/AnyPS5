#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 120;
constexpr std::size_t SurfaceBytes = 0x20000;
constexpr int Rounds = 20;
alignas(256) constexpr std::array<std::uint32_t, 11> WriteCode{
    0xd7460004u, 0x0401060eu, 0xd7460005u, 0x0405060fu, 0x7e100d04u, 0x7e120d05u,
    0x7e140c0du, 0x7e1602f2u, 0xf0200f08u, 0x00000804u, 0xbf810000u,
};

class GuestSurface {
public:
    GuestSurface() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, SurfaceBytes));
        GuestArena::GuestArenaCommit_nid_postfix(block, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
        void* mapped = mmap(nullptr, SurfaceBytes * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "idle store read: cannot allocate the guest surface");
        const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
        const auto aligned = (begin + SurfaceBytes - 1) & ~(std::uintptr_t{SurfaceBytes - 1});
        if (aligned != begin) munmap(mapped, aligned - begin);
        const auto tail = begin + SurfaceBytes * 2 - aligned - SurfaceBytes;
        if (tail != 0) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), tail);
        block = reinterpret_cast<std::uint8_t*>(aligned);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, SurfaceBytes);
#endif
        GuestAllocations::Mutation mutation;
        mutation.Add(block, SurfaceBytes, true, true, true);
    }

    ~GuestSurface() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(block);
        }
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, SurfaceBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, SurfaceBytes);
#else
        munmap(block, SurfaceBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, SurfaceBytes);
#endif
    }

    GuestSurface(const GuestSurface&) = delete;
    GuestSurface& operator=(const GuestSurface&) = delete;
    std::uint8_t* Data() { return block; }
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }

private:
    std::uint8_t* block = nullptr;
};

void Produce(GuestSurface& surface) {
    std::memset(surface.Data(), 0, SurfaceBytes);
    const auto address = surface.Address();
    const std::array<std::uint32_t, 14> data{
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (71u << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (0x09u << 20u) | (9u << 28u),
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 7u,
    };
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{8, 8, 1};
    registers(0x207, shape);
    const auto code = reinterpret_cast<std::uintptr_t>(WriteCode.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{(static_cast<std::uint32_t>(data.size()) << 1u) | (7u << 7u) | (1u << 11u)};
    registers(0x213, resource);
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, Width / 8u, Height / 8u, 1u, 0x41u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverSuspendPoint_nid_postfix();
    AgcDriverWaitIdle_nid_postfix();
    Require(StorageTexture::FindPending(surface.Address(), SurfaceBytes) != nullptr, "idle store read: the GPU producer left no pending image");
}

bool AllZero(std::span<const std::byte> bytes) {
    return std::none_of(bytes.begin(), bytes.end(), [](std::byte value) { return value != std::byte{}; });
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        GuestSurface surface;
        int stale = 0;
        for (int round = 0; round < Rounds; ++round) {
            Produce(surface);
            std::atomic<bool> started{false};
            std::thread idle([&] {
                started = true;
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                StorageTexture::FlushAllPending("idle");
            });
            while (!started) {}
            while (StorageTexture::FindPending(surface.Address(), SurfaceBytes) != nullptr) {}
            std::vector<std::byte> seen(SurfaceBytes);
            AgcDriver::GuestMemory::Read(surface.Address(), seen);
            idle.join();
            stale += AllZero(seen);
            std::vector<std::byte> stored(SurfaceBytes);
            AgcDriver::GuestMemory::Read(surface.Address(), stored);
            Require(!AllZero(stored), "idle store read: the GPU producer wrote only zeros");
        }
        std::printf("idle store read: %d of %d host reads saw the memory before the store\n", stale, Rounds);
        Require(stale == 0, "idle store read: a host read did not wait for the image another thread was storing");
        AgcDriverShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
