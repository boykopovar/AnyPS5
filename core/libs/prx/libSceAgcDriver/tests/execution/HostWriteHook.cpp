#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
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
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;

constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 120;
constexpr std::uint32_t FormatRgba16Float = 71;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Standard64KB = 0x09;
constexpr std::size_t SurfaceBytes = 0x20000;
constexpr std::size_t BlockBytes = 0x10000;

alignas(256) constexpr std::array<std::uint32_t, 11> WriteCode{0xd7460004u, 0x0401060eu, 0xd7460005u, 0x0405060fu, 0x7e100d04u, 0x7e120d05u, 0x7e140c0du, 0x7e1602f2u, 0xf0200f08u, 0x00000804u, 0xbf810000u};

std::array<std::uint32_t, 8> TextureDescriptor(const void* texels) {
    const auto address = reinterpret_cast<std::uintptr_t>(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (FormatRgba16Float << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (Standard64KB << 20u) | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

void Dispatch(std::uint8_t* texels) {
    std::vector<std::uint32_t> data;
    const auto texture = TextureDescriptor(texels);
    data.insert(data.end(), texture.begin(), texture.end());
    data.resize(14, 0u);
    data[13] = 7u;
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
    AgcDriverWaitIdle_nid_postfix();
}

class GuestSurface {
public:
    GuestSurface() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, Alignment));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
        void* mapped = mmap(nullptr, SurfaceBytes + Alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + Alignment - 1) & ~(static_cast<std::uintptr_t>(Alignment) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + SurfaceBytes != begin + SurfaceBytes + Alignment) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + Alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, SurfaceBytes);
        }
#endif
        Require(block != nullptr, "host write hook: cannot allocate the guest surface");
        std::memset(block, 0, SurfaceBytes);
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

private:
    static constexpr std::size_t Alignment = 65536;
    std::uint8_t* block = nullptr;
};

std::uint64_t Address(const std::uint8_t* guest) {
    return reinterpret_cast<std::uint64_t>(guest);
}

void HostWriteWaits(GuestSurface& pending, GuestSurface& idle) {
    auto* guest = pending.Data();
    Dispatch(guest);
    StorageTexture::FlushPending(Address(guest), SurfaceBytes, nullptr, "sampled texture");
    Require(std::all_of(guest, guest + SurfaceBytes, [](std::uint8_t byte) { return byte == 0; }), "host write waits: the image's store landed when it was made, so none was recorded over the range");
    const auto before = AgcDriver::GuestMemory::HostWritesCounted();
    {
        const GuestArena::HostWrite scope(guest, BlockBytes);
        std::memset(guest, 0x33, BlockBytes);
    }
    const auto recorded = AgcDriver::GuestMemory::HostWritesCounted();
    Require(recorded.calls == before.calls + 1 && recorded.waited == before.waited + 1 && recorded.waitedMs > before.waitedMs, "host write waits: a host write over a recorded store counted " + std::to_string(recorded.calls - before.calls) + " call(s), " + std::to_string(recorded.waited - before.waited) + " that waited");
    {
        const GuestArena::HostWrite scope(idle.Data(), BlockBytes);
        std::memset(idle.Data(), 0x44, BlockBytes);
    }
    const auto after = AgcDriver::GuestMemory::HostWritesCounted();
    Require(after.calls == recorded.calls + 1 && after.waited == recorded.waited, "host write waits: a host write over an idle range counted " + std::to_string(after.calls - recorded.calls) + " call(s), " + std::to_string(after.waited - recorded.waited) + " that waited");
    std::cout << AgcDriver::GuestMemory::HostWriteReport();
    std::puts("host write waits: the write over the recorded store waited, the one over the idle range did not");
}

void HostWriteUnderGpuLock(GuestSurface& surface) {
    AgcDriverWaitIdle_nid_postfix();
    const auto before = AgcDriver::GuestMemory::LockedHostWrites();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const GuestArena::HostWrite scope(surface.Data(), BlockBytes);
        std::memset(surface.Data(), 0x11, BlockBytes);
    }
    const auto locked = AgcDriver::GuestMemory::LockedHostWrites();
    Require(locked == before + 1, "host write under the GPU mutex: the count moved by " + std::to_string(locked - before) + ", not 1");
    {
        const GuestArena::HostWrite scope(surface.Data(), BlockBytes);
        std::memset(surface.Data(), 0x22, BlockBytes);
    }
    Require(AgcDriver::GuestMemory::LockedHostWrites() == locked, "host write under the GPU mutex: a host write made without the mutex was counted");
    std::puts("host write under the GPU mutex: it returned and was counted once");
}

std::atomic<std::uint64_t> flushesAfterShutdown{0};

void CountFlush(std::uint64_t, std::size_t) {
    flushesAfterShutdown.fetch_add(1, std::memory_order_relaxed);
}

void HostWriteAfterShutdown(GuestSurface& surface) {
    auto* guest = surface.Data();
    Dispatch(guest);
    StorageTexture::FlushPending(Address(guest), SurfaceBytes, nullptr, "sampled texture");
    Require(std::all_of(guest, guest + SurfaceBytes, [](std::uint8_t byte) { return byte == 0; }), "host write after shutdown: the image's store landed when it was made, so none was recorded over the range");
    AgcDriverShutdown_nid_postfix();
    AgcDriver::GuestMemory::SetFlushHook(&CountFlush);
    {
        const GuestArena::HostWrite scope(guest, BlockBytes);
        std::memset(guest, 0x5a, BlockBytes);
    }
    const auto reached = flushesAfterShutdown.load(std::memory_order_relaxed);
    Require(reached == 0, "host write after shutdown: a host write over a range with a recorded store reached the driver's flush hook " + std::to_string(reached) + " time(s) after Driver::Shutdown");
    std::puts("host write after shutdown: the host write did not reach the stopped driver");
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_PROFILE_DRAW", "1");
#else
        setenv("APS5_PROFILE_DRAW", "1", 1);
#endif
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        GuestSurface pending;
        GuestSurface idle;
        HostWriteWaits(pending, idle);
        GuestSurface locked;
        HostWriteUnderGpuLock(locked);
        GuestSurface surface;
        HostWriteAfterShutdown(surface);
        std::puts("host write hook tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
