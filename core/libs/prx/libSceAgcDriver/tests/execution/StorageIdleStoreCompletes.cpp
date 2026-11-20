#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "Recompiler.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
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
constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 128;
constexpr std::size_t SurfaceBytes = 0x20000;
constexpr int Rounds = 8;
alignas(256) constexpr std::array<std::uint32_t, 11> WriteCode{
    0x7e000280u, 0x7e020280u, 0x7e0402ffu, 0x11111111u, 0xf0200108u, 0x00010200u,
    0x7e0202ffu, 0x00000064u, 0xf0200108u, 0x00010200u, 0xbf810000u,
};

class GuestSurface {
public:
    GuestSurface() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, SurfaceBytes));
        GuestArena::GuestArenaCommit_nid_postfix(block, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
        void* mapped = mmap(nullptr, SurfaceBytes * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "idle store completion: cannot allocate the guest surface");
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

void Produce(AgcDriver::VulkanDevice& device, GuestSurface& surface) {
    std::memset(surface.Data(), 0, SurfaceBytes);
    const auto address = surface.Address();
    std::vector<std::uint32_t> data(12, 0u);
    data[4] = static_cast<std::uint32_t>(address >> 8u);
    data[5] = static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (20u << 20u) | (((Width - 1u) & 3u) << 30u);
    data[6] = ((Width - 1u) >> 2u) | ((Height - 1u) << 14u);
    data[7] = 0xfacu | (9u << 28u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(WriteCode.data()), std::as_bytes(std::span(WriteCode))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(WriteCode.data()), WriteCode, 0, {}},
        {64u, 0, data, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto shader = ShaderRecompiler::Recompile(request);
    device.Dispatch(shader, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(WriteCode.data()));
    device.WaitIdle();
    Require(StorageTexture::FindPending(surface.Address(), SurfaceBytes) != nullptr, "idle store completion: the GPU producer left no pending image");
}


}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestSurface surface;
        std::size_t overwritten = 0;
        for (int round = 0; round < Rounds; ++round) {
            Produce(*device, surface);
            {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                auto* recorder = AgcDriver::Graphics::Recorder::Active();
                Require(recorder != nullptr, "idle store completion: the flushing thread has no active recorder");
                StorageTexture::FlushAllPending("idle");
                const bool pending = recorder->PendingWriteOverlaps(surface.Address(), SurfaceBytes);
                if (pending) device->WaitIdle();
                Require(!pending, "idle store completion: the idle flush returned with an outstanding recorder write for the stored range");
            }
            std::memset(surface.Data(), 0xa5, SurfaceBytes);
            device->WaitIdle();
            overwritten += static_cast<std::size_t>(std::count_if(surface.Data(), surface.Data() + SurfaceBytes, [](std::uint8_t byte) { return byte != 0xa5u; }));
        }
        std::printf("idle store completion: %zu bytes of CPU stores made after the idle flush returned were overwritten in %d rounds\n", overwritten, Rounds);
        Require(overwritten == 0, "idle store completion: the idle flush returned before its write-back landed, and the write-back overwrote later CPU stores");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
