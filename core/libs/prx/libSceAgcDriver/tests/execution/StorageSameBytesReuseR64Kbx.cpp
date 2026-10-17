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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using Clock = std::chrono::steady_clock;

constexpr std::size_t Block = 0x10000;
constexpr std::uint32_t Side = 32;
constexpr std::uint32_t Format8888Unorm = 56;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint8_t Before = 0xa5;
constexpr std::uint32_t Attempts = 8;
constexpr std::uint32_t Texel = 0x00ffffffu;
constexpr std::array<std::size_t, 2> LinkOffsets{0x0, 0x8};
constexpr std::size_t TriggerOffset = 0x8000;

alignas(256) constexpr std::array<std::uint32_t, 8> StoreCode{
    0x7e02020c, 0x7e0402f2, 0x7e0602f2, 0x7e0802f2, 0x7e0a0280, 0xf0201f08, 0x00010200, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::uint32_t Low(const volatile void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const volatile void* pointer) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)) >> 32u);
}

std::array<std::uint32_t, 8> TextureDescriptor(std::uint64_t address) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888Unorm << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

class GuestBlock {
public:
    explicit GuestBlock(bool watched = false) : watched(watched) {
        if (watched) {
#ifdef _WIN32
            bytes = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(Block, Block));
            if (bytes != nullptr) GuestArena::GuestArenaCommit_nid_postfix(bytes, Block, PAGE_READWRITE, Block);
#else
            void* mapped = mmap(nullptr, 2 * Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mapped != MAP_FAILED) {
                const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
                const auto aligned = (begin + Block - 1) & ~std::uintptr_t{Block - 1};
                if (aligned != begin) munmap(mapped, aligned - begin);
                if (aligned != begin + Block) munmap(reinterpret_cast<void*>(aligned + Block), begin + Block - aligned);
                bytes = reinterpret_cast<std::uint8_t*>(aligned);
                GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(bytes, Block);
            }
#endif
        } else {
#ifdef _WIN32
            bytes = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, Block, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
            bytes = static_cast<std::uint8_t*>(std::aligned_alloc(Block, Block));
#endif
        }
        Require(bytes != nullptr, "storage same-bytes reuse: cannot allocate a guest block");
        std::memset(bytes, Before, Block);
        GuestAllocations::Mutation().Add(bytes, Block, true, true, true);
    }
    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(bytes);
        if (watched) {
#ifdef _WIN32
            GuestArena::GuestArenaReset_nid_postfix(bytes, Block);
            GuestArena::GuestArenaRelease_nid_postfix(bytes, Block);
#else
            munmap(bytes, Block);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(bytes, Block);
#endif
            return;
        }
#ifdef _WIN32
        VirtualFree(bytes, 0, MEM_RELEASE);
#else
        std::free(bytes);
#endif
    }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint8_t* Data() const { return bytes; }
    std::uint64_t Address() const { return AddressOf(bytes); }

private:
    bool watched;
    std::uint8_t* bytes = nullptr;
};

void StoreThenLabel(const GuestBlock& image, volatile std::uint32_t* label, std::uint32_t value) {
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{Side, 1, 1};
    registers(0x207, shape);
    const auto code = AddressOf(StoreCode.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)};
    registers(0x20c, program);
    std::array<std::uint32_t, 12> userData{};
    const auto descriptor = TextureDescriptor(image.Address());
    std::copy(descriptor.begin(), descriptor.end(), userData.begin() + 4);
    const std::array<std::uint32_t, 1> layout{(static_cast<std::uint32_t>(userData.size()) << 1u) | 0x80u};
    registers(0x213, layout);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, Side, 1, 1, 0x41u});
    words.insert(words.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), Low(label), High(label), value, 0u, 0u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
}

bool AwaitLabel(const volatile std::uint32_t* label, std::uint32_t value) {
    const auto deadline = Clock::now() + std::chrono::seconds(10);
    while (*label != value) {
        if (Clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}

std::uint64_t Word(const GuestBlock& block, std::size_t offset) {
    std::uint64_t value = 0;
    std::memcpy(&value, block.Data() + offset, sizeof(value));
    return value;
}

std::uint64_t LinkValue(std::uint32_t attempt, std::size_t index) {
    return 0x0000000280000000ull + (static_cast<std::uint64_t>(attempt) << 16u) + index * 0x40u + 0x10u;
}

constexpr std::uint64_t TexelPair = (static_cast<std::uint64_t>(Texel) << 32u) | Texel;

bool Pending(const GuestBlock& image) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    return StorageTexture::FindPending(image.Address(), Block) != nullptr;
}

void Publish(const GuestBlock& image) {
    std::array<std::byte, 16> read{};
    AgcDriver::GuestMemory::Read(image.Address() + TriggerOffset, read);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    StorageTexture::FlushAllPending("storage same-bytes reuse fixture");
}

struct Counts {
    std::uint32_t kept = 0;
    std::uint32_t lostToTexels = 0;
    std::uint32_t lost = 0;
    std::uint32_t notPending = 0;
};

void Attempt(GuestBlock& image, GuestBlock& labels, std::uint32_t attempt, bool sameLinks, Counts& counts) {
    auto* label = reinterpret_cast<volatile std::uint32_t*>(labels.Data());
    const auto value = 100 + attempt;
    *label = 0;
    std::memset(image.Data(), Before, Block);
    for (std::size_t index = 0; index < LinkOffsets.size(); ++index) {
        const auto stale = LinkValue(attempt, index);
        std::memcpy(image.Data() + LinkOffsets[index], &stale, sizeof(stale));
    }
    StoreThenLabel(image, label, value);
    Require(AwaitLabel(label, value), "storage same-bytes reuse: a label never arrived");
    if (!Pending(image)) ++counts.notPending;
    for (std::size_t index = 0; index < LinkOffsets.size(); ++index) {
        const auto freed = sameLinks ? LinkValue(attempt, index) : LinkValue(attempt + 1000, index);
        std::memcpy(image.Data() + LinkOffsets[index], &freed, sizeof(freed));
    }
    Publish(image);
    bool kept = true;
    bool texels = true;
    for (std::size_t index = 0; index < LinkOffsets.size(); ++index) {
        const auto observed = Word(image, LinkOffsets[index]);
        kept = kept && observed == (sameLinks ? LinkValue(attempt, index) : LinkValue(attempt + 1000, index));
        texels = texels && observed == TexelPair;
    }
    if (kept) ++counts.kept;
    else if (texels) ++counts.lostToTexels;
    else ++counts.lost;
    std::printf("[%s %u] link0=0x%016llx link1=0x%016llx result=%s\n", sameLinks ? "same links" : "new links", attempt + 1, static_cast<unsigned long long>(Word(image, LinkOffsets[0])),
                static_cast<unsigned long long>(Word(image, LinkOffsets[1])), kept ? "KEPT" : texels ? "LOST_TO_TEXELS" : "LOST");
}

enum class Verdict { Pass, Skip, Fail };

Verdict Run(bool watchedMemory) {
    GuestBlock image(watchedMemory);
    GuestBlock labels;
    const char* kind = watchedMemory ? "watched" : "compared";
    const bool watched = AgcDriver::GuestMemory::Watched(image.Address(), Block);
    std::printf("[same-bytes reuse #5934] memory=%s surface=32x32 RGBA8 R64KBX watched=%d hostImport=%s attempts=%u+%u\n", kind, watched, std::getenv("APS5_NO_HOST_IMPORT") != nullptr ? "disabled" : "allowed",
                Attempts, Attempts);
    if (watched != watchedMemory) {
        std::printf("[same-bytes reuse #5934] memory=%s SKIP: the write watch does not cover the surface as asked\n", kind);
        return Verdict::Skip;
    }
    Counts changed;
    for (std::uint32_t attempt = 0; attempt < Attempts; ++attempt) Attempt(image, labels, attempt, false, changed);
    Counts same;
    for (std::uint32_t attempt = 0; attempt < Attempts; ++attempt) Attempt(image, labels, Attempts + attempt, true, same);
    std::printf("[same-bytes reuse #5934] memory=%s summary: new links kept/lostToTexels/lost=%u/%u/%u, same links kept/lostToTexels/lost=%u/%u/%u, notPendingAtLabel=%u\n", kind, changed.kept,
                changed.lostToTexels, changed.lost, same.kept, same.lostToTexels, same.lost, changed.notPending + same.notPending);
    const auto lost = changed.lostToTexels + changed.lost + same.lostToTexels + same.lost;
    if (lost != 0) {
        std::printf("[same-bytes reuse #5934] memory=%s FAIL: %u attempts lost the links the CPU stored at the free\n", kind, lost);
        return Verdict::Fail;
    }
    std::printf("[same-bytes reuse #5934] memory=%s PASS: the links stored at the free survived, new or identical (%u of %u results already in memory at their label)\n", kind,
                changed.notPending + same.notPending, 2 * Attempts);
    return Verdict::Pass;
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
        const bool watchedRun = std::getenv("APS5_TEST_WATCHED_SAME_BYTES") != nullptr;
        std::array<Verdict, 2> verdicts{Run(false), watchedRun ? Run(true) : Verdict::Skip};
        if (!watchedRun) std::puts("[same-bytes reuse #5934] memory=watched NOT RUN (APS5_TEST_WATCHED_SAME_BYTES=1 runs it)");
        const auto count = [&](Verdict verdict) { return std::count(verdicts.begin(), verdicts.end(), verdict); };
        std::printf("[same-bytes reuse #5934] PASS=%td SKIP=%td FAIL=%td\n", count(Verdict::Pass), count(Verdict::Skip), count(Verdict::Fail));
        if (count(Verdict::Fail) != 0) return 1;
        return count(Verdict::Pass) != 0 ? 0 : VulkanTestSkipped;
    } catch (const std::exception& error) {
        std::cerr << "[same-bytes reuse #5934] FAIL: " << error.what() << '\n';
        return 1;
    }
}
