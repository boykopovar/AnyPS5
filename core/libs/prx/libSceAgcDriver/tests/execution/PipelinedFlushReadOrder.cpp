#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
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
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <semaphore>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawPipeline;
using AgcDriver::Graphics::Require;

constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 120;
constexpr std::uint32_t FormatRgba16Float = 71;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Standard64KB = 0x09;
constexpr std::size_t SurfaceBytes = 0x20000;

alignas(256) constexpr std::array<std::uint32_t, 11> WriteCode{0xd7460004u, 0x0401060eu, 0xd7460005u, 0x0405060fu, 0x7e100d04u, 0x7e120d05u, 0x7e140c0du, 0x7e1602f2u, 0xf0200f08u, 0x00000804u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 22> MultisampledSingleClearCode{0xbe800303u, 0xbe830382u, 0xf40c0401u, 0xfa000000u, 0xd7460000u, 0x04010609u, 0xd7460001u, 0x0405060au, 0x7e060208u, 0x7e0c020bu, 0x7e0e0280u, 0xd5690004u, 0x02000100u, 0x7e000205u, 0xd5690005u, 0x02000901u, 0xd7000001u, 0x02000c80u, 0xbf8cc07fu, 0xf0201f38u, 0x00040004u, 0xbf810000u};

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

void AppendRegisters(std::vector<std::uint32_t>& words, std::uint32_t first, std::span<const std::uint32_t> values) {
    words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
    words.push_back(first);
    words.insert(words.end(), values.begin(), values.end());
}

void DispatchImageWrite(std::uint8_t* texels) {
    std::vector<std::uint32_t> data;
    const auto texture = TextureDescriptor(texels);
    data.insert(data.end(), texture.begin(), texture.end());
    data.resize(14, 0u);
    data[13] = 7u;
    std::vector<std::uint32_t> words;
    const std::array<std::uint32_t, 3> shape{8, 8, 1};
    AppendRegisters(words, 0x207, shape);
    const auto code = reinterpret_cast<std::uintptr_t>(WriteCode.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)};
    AppendRegisters(words, 0x20c, program);
    const std::array<std::uint32_t, 1> resource{(static_cast<std::uint32_t>(data.size()) << 1u) | (7u << 7u) | (1u << 11u)};
    AppendRegisters(words, 0x213, resource);
    AppendRegisters(words, 0x240, data);
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
        Require(block != nullptr, "pipelined flush read order: cannot allocate the guest surface");
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

void TestCommitterHeldReadsIgnoresFutureItems() {
    auto& pipeline = DrawPipeline::Queue0();
    std::binary_semaphore firstEntered{0};
    std::binary_semaphore releaseFirst{0};
    bool frontHeld = false;
    bool futureHeld = true;
    bool secondRan = false;
    pipeline.Enqueue(
        [&] {
            firstEntered.release();
            releaseFirst.acquire();
            frontHeld = DrawPipeline::HeldReads(0x1000, 4);
            futureHeld = DrawPipeline::HeldReads(0x2000, 4);
        },
        {}, 0, {}, {{0x1000, 0x1010}});
    pipeline.Enqueue(
        [&] { secondRan = true; },
        {}, 0, {}, {{0x2000, 0x2010}});
    firstEntered.acquire();
    const bool externalFront = DrawPipeline::HeldReads(0x1000, 4);
    const bool externalFuture = DrawPipeline::HeldReads(0x2000, 4);
    releaseFirst.release();
    pipeline.Drain(DrawPipeline::DrainReason::Flush);
    Require(externalFront, "pipelined flush read order: external held read missed running item");
    Require(externalFuture, "pipelined flush read order: external held read missed queued item");
    Require(frontHeld, "pipelined flush read order: committer held read missed running item");
    Require(!futureHeld, "pipelined flush read order: committer held read blocked on future queued item");
    Require(secondRan, "pipelined flush read order: queued second item did not run");
}

void TestCommitterFlushesPastFutureQueuedRead() {
    GuestSurface surface;
    const auto texelsAddress = reinterpret_cast<std::uintptr_t>(surface.Data());
    DispatchImageWrite(surface.Data());
    Require(AgcDriver::Graphics::StorageTexture::AnyPending(), "pipelined flush read order: image write left no pending storage texture");
    Require(std::all_of(surface.Data(), surface.Data() + SurfaceBytes, [](std::uint8_t byte) { return byte == 0; }), "pipelined flush read order: image write reached memory early");

    std::binary_semaphore committerEntered{0};
    std::binary_semaphore releaseCommitter{0};
    bool flushed = false;
    bool futureRan = false;
    DrawPipeline::Queue0().Enqueue(
        [&] {
            committerEntered.release();
            releaseCommitter.acquire();
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            flushed = AgcDriver::Graphics::StorageTexture::FlushPending(texelsAddress, SurfaceBytes, nullptr, "pipelined flush test");
        },
        {});
    DrawPipeline::Queue0().Enqueue(
        [&] { futureRan = true; },
        {}, 0, {}, {{texelsAddress, texelsAddress + SurfaceBytes}});

    committerEntered.acquire();
    const bool heldExternally = DrawPipeline::HeldReads(texelsAddress, SurfaceBytes);
    releaseCommitter.release();
    DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Flush);
    AgcDriverWaitIdle_nid_postfix();

    Require(heldExternally, "pipelined flush read order: future queued read was not visible outside the committer");
    Require(flushed, "pipelined flush read order: a future queued item blocked the running item's storage flush");
    Require(futureRan, "pipelined flush read order: future queued item did not run");
    const bool wroteAny = std::any_of(surface.Data(), surface.Data() + SurfaceBytes, [](std::uint8_t byte) { return byte != 0; });
    Require(wroteAny, "pipelined flush read order: flushed storage texture left guest memory unchanged");
}

void TestMultisampledSingleClearDrainsDrawPipeline() {
    constexpr std::uintptr_t FixedBase = 0x20f7f0000ull;
    constexpr std::size_t FixedBytes = 65536;
#ifdef _WIN32
    auto* mapped = static_cast<std::uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(FixedBase), FixedBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (mapped == nullptr) return;
#else
#ifdef MAP_FIXED_NOREPLACE
    auto* mapped = static_cast<std::uint8_t*>(mmap(reinterpret_cast<void*>(FixedBase), FixedBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0));
    if (mapped == MAP_FAILED) return;
#else
    return;
#endif
#endif
    struct FixedGuard {
        std::uint8_t* base;
        std::size_t bytes;
        ~FixedGuard() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(base);
            }
#ifdef _WIN32
            VirtualFree(base, 0, MEM_RELEASE);
#else
            munmap(base, bytes);
#endif
        }
    };
    {
        std::memset(mapped, 0, FixedBytes);
        GuestAllocations::Mutation mutation;
        mutation.Add(mapped, FixedBytes, true, true, true);
    }
    const FixedGuard guard{mapped, FixedBytes};

    auto release = std::make_shared<std::atomic<bool>>(false);
    DrawPipeline::Queue0().Enqueue([release] {
        while (!release->load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }, {});

    const std::array<std::uint32_t, 9> userData{0u, 0u, 0x0f7f0008u, 8u, 8u, 0xff00ffccu, 0u, 0u, 0u};
    std::vector<std::uint32_t> words;
    const std::array<std::uint32_t, 3> shape{8, 8, 1};
    AppendRegisters(words, 0x207, shape);
    const auto code = reinterpret_cast<std::uintptr_t>(MultisampledSingleClearCode.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)};
    AppendRegisters(words, 0x20c, program);
    const std::array<std::uint32_t, 1> resource{(static_cast<std::uint32_t>(userData.size()) << 1u) | (1u << 7u) | (1u << 11u)};
    AppendRegisters(words, 0x213, resource);
    AppendRegisters(words, 0x240, userData);
    words.insert(words.end(), {0xc0031500u, 1u, 1u, 1u, 0x41u});

    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    const auto drainsBefore = DrawPipeline::DrainRequestsByOpcode(0x15);
    auto submitted = std::async(std::launch::async, [&] { AgcDriver::Submit(&packet, 0); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (DrawPipeline::DrainRequestsByOpcode(0x15) == drainsBefore && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const bool drained = DrawPipeline::DrainRequestsByOpcode(0x15) > drainsBefore;
    release->store(true, std::memory_order_release);
    Require(submitted.wait_for(std::chrono::seconds(20)) == std::future_status::ready, "pipelined flush read order: multisampled clear submission hung");
    submitted.get();
    AgcDriverWaitIdle_nid_postfix();
    Require(drained, "pipelined flush read order: multisampled comp-to-single clear bypassed DrawPipeline::Queue0().Drain");
}

}

int main() {
    if (std::getenv("APS5_PIPELINED_DRAWS") == nullptr) {
#ifdef _WIN32
        _putenv_s("APS5_PIPELINED_DRAWS", "32");
#else
        setenv("APS5_PIPELINED_DRAWS", "32", 1);
#endif
    }
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        TestCommitterHeldReadsIgnoresFutureItems();
        TestCommitterFlushesPastFutureQueuedRead();
        TestMultisampledSingleClearDrainsDrawPipeline();
        AgcDriverShutdown_nid_postfix();
        std::puts("pipelined flush read order tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}

