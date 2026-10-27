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
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <thread>
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
constexpr std::size_t SettledImportBytes = 64 * Block;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Stored = 0x12345678u;
constexpr std::uint32_t Filled = 0x9abcdef0u;
constexpr std::uint32_t Written = 0x0badf00du;

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, Stored, 0xf0200108, 0x00020102, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> FillCode{
    0xd7460004, 0x04010c08, 0x7e000204, 0x7e020205, 0x7e040206, 0x7e060207, 0xe01c2000, 0x80000004, 0xe01c2000, 0x80020004, 0xbf810000,
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

void Dispatch(std::vector<std::uint32_t>& words, std::span<const std::uint32_t> code, std::uint32_t threads, std::uint32_t resource, std::span<const std::uint32_t> userData, std::uint32_t groups) {
    const auto registers = [&](const std::uint32_t first, const std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{threads, 1, 1};
    registers(0x207, shape);
    const auto address = AddressOf(code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> layout{resource};
    registers(0x213, layout);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, groups, 1, 1, 0x41u});
}

void Store(std::vector<std::uint32_t>& words, const std::uint32_t* texels) {
    std::array<std::uint32_t, 16> userData{};
    const auto texture = TextureDescriptor(texels);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    Dispatch(words, StoreCode, 8, static_cast<std::uint32_t>(userData.size()) << 1u, userData, 1);
}

void Fill(std::vector<std::uint32_t>& words, const std::uint32_t* texels, const std::uint32_t* copied) {
    const auto address = AddressOf(texels);
    const auto other = AddressOf(copied);
    const std::array<std::uint32_t, 12> userData{
        static_cast<std::uint32_t>(address),
        static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u),
        static_cast<std::uint32_t>(SurfaceBytes / 16u),
        0x4bfacu,
        Filled,
        Filled,
        Filled,
        Filled,
        static_cast<std::uint32_t>(other),
        static_cast<std::uint32_t>((other >> 32u) & 0xffffu) | (16u << 16u),
        static_cast<std::uint32_t>(SurfaceBytes / 16u),
        0x4bfacu,
    };
    Dispatch(words, FillCode, 64, (static_cast<std::uint32_t>(userData.size()) << 1u) | 0x80u, userData, static_cast<std::uint32_t>(SurfaceBytes / 1024u));
}

class Surface {
public:
    Surface() {
#ifdef _WIN32
        region = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, Block));
        if (region != nullptr) GuestArena::GuestArenaCommit_nid_postfix(region, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
        void* mapped = mmap(nullptr, SurfaceBytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + Block - 1) & ~std::uintptr_t{Block - 1};
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned != begin + Block) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + Block - aligned);
            region = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(region, SurfaceBytes);
        }
#endif
        Require(region != nullptr, "direct write completion: cannot allocate the guest surface");
        texels = reinterpret_cast<std::uint32_t*>(region);
        std::fill(texels, texels + SurfaceBytes / 4u, Untouched);
        GuestAllocations::Mutation().Add(region, SurfaceBytes, true, true, true);
    }

    ~Surface() {
        GuestAllocations::Mutation().Remove(region);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(region, SurfaceBytes);
        GuestArena::GuestArenaRelease_nid_postfix(region, SurfaceBytes);
#else
        munmap(region, SurfaceBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(region, SurfaceBytes);
#endif
    }

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    std::uint32_t* Texels() const { return texels; }

private:
    std::uint8_t* region = nullptr;
    std::uint32_t* texels = nullptr;
};

enum class Outcome { Kept, Dropped, Missed };

void WriteOnceRecorded(const Surface& surface) {
    const auto address = AddressOf(surface.Texels());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (;;) {
        {
            std::unique_lock hold(AgcDriver::GuestMemory::GpuMutex());
            if (StorageTexture::FindPending(address, SurfaceBytes) != nullptr) {
                surface.Texels()[0] = Written;
                return;
            }
        }
        Require(std::chrono::steady_clock::now() < deadline, "direct write completion: the image store was never recorded");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

Outcome StoreAfterFill(const Surface& surface, bool cpuWrite) {
    std::vector<std::uint32_t> copied(SurfaceBytes / 4u, Untouched);
    GuestAllocations::Mutation().Add(copied.data(), SurfaceBytes, true, true, true);
    struct Unregister {
        void* data;
        ~Unregister() { GuestAllocations::Mutation().Remove(data); }
    } unregister{copied.data()};
    std::vector<std::uint32_t> words;
    Fill(words, surface.Texels(), copied.data());
    Store(words, surface.Texels());
    alignas(8) static std::uint32_t gate = 0;
    gate = 0;
    if (cpuWrite) {
        const auto at = AddressOf(&gate);
        words.insert(words.end(), {0xc0053c00u, 0x13u, static_cast<std::uint32_t>(at), static_cast<std::uint32_t>(at >> 32u), 1u, 0xffffffffu, 0x19u});
    }
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    if (cpuWrite) {
        WriteOnceRecorded(surface);
        std::atomic_ref<std::uint32_t>(gate).store(1);
    }
    AgcDriverWaitIdle_nid_postfix();
    std::vector<std::byte> landed(SurfaceBytes);
    AgcDriver::GuestMemory::Read(AddressOf(copied.data()), landed);
    const auto address = AddressOf(surface.Texels());
    std::unique_lock hold(AgcDriver::GuestMemory::GpuMutex());
    const auto image = StorageTexture::FindPending(address, SurfaceBytes);
    if (image == nullptr) return Outcome::Missed;
    const bool current = image->Refresh();
    const bool pending = StorageTexture::FindPending(address, SurfaceBytes) == image;
    StorageTexture::FlushPending(address, SurfaceBytes, nullptr, "direct write completion fixture");
    return current && pending ? Outcome::Kept : Outcome::Dropped;
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
            Surface probe;
            Require(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(probe.Texels()))).guestBytes == SurfaceBytes, "direct write completion: unexpected surface size");
        }
        {
            constexpr std::size_t Runs = SettledImportBytes / SurfaceBytes - 1;
            std::vector<std::unique_ptr<Surface>> surfaces;
            for (std::size_t index = 0; index <= 2 * Runs + 1; ++index) surfaces.push_back(std::make_unique<Surface>());
            static_cast<void>(StoreAfterFill(*surfaces[0], false));
            std::size_t kept = 0;
            std::size_t dropped = 0;
            for (std::size_t run = 1; run <= Runs; ++run) {
                const auto outcome = StoreAfterFill(*surfaces[run], false);
                kept += outcome == Outcome::Kept;
                dropped += outcome == Outcome::Dropped;
            }
            AgcDriverWaitIdle_nid_postfix();
            std::printf("[direct write completion] %zu of %zu runs held the image results pending, %zu dropped them\n", kept + dropped, Runs, dropped);
            Require(kept + dropped != 0, "direct write completion: no run held the image results pending");
            Require(dropped == 0, "direct write completion: the completion of a buffer write dropped image results recorded after it");
            static_cast<void>(StoreAfterFill(*surfaces[Runs + 1], false));
            std::size_t keptOverWrite = 0;
            std::size_t lostWrites = 0;
            for (std::size_t run = Runs + 2; run <= 2 * Runs + 1; ++run) {
                keptOverWrite += StoreAfterFill(*surfaces[run], true) == Outcome::Kept;
                lostWrites += surfaces[run]->Texels()[0] != Written;
            }
            AgcDriverWaitIdle_nid_postfix();
            std::printf("[direct write completion] %zu of %zu runs with a CPU write before the completion kept the image results, %zu lost the write\n", keptOverWrite, Runs, lostWrites);
            Require(keptOverWrite == 0, "direct write completion: image results were kept over a CPU write made before the completion");
            Require(lostWrites == 0, "direct write completion: a CPU write made before the completion was lost");
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("direct write completion tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
