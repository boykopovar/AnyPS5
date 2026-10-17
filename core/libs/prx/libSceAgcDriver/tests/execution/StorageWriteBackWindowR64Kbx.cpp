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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <random>
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
constexpr std::uint32_t Attempts = 96;
constexpr std::uint32_t Texel = 0x00ffffffu;
constexpr std::array<std::size_t, 2> LinkOffsets{0x0, 0x8};
constexpr std::size_t TriggerOffset = 0x8000;
constexpr std::size_t ResultOffset = 0x10;

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
        Require(bytes != nullptr, "storage write-back window: cannot allocate a guest block");
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
    StorageTexture::FlushAllPending("storage write-back window fixture");
}

enum class Phase { BeforeTrigger, DuringTrigger, AfterTrigger };

const char* PhaseName(Phase phase) {
    return phase == Phase::BeforeTrigger ? "before-trigger" : phase == Phase::DuringTrigger ? "during-trigger" : "after-trigger";
}

struct Tally {
    std::array<std::uint32_t, 3> kept{};
    std::array<std::uint32_t, 3> lost{};
    std::uint32_t notPending = 0;
    std::uint32_t publishedOverLinks = 0;
    std::uint32_t wonInsideStore = 0;
};

bool Control(GuestBlock& image, GuestBlock& labels) {
    auto* label = reinterpret_cast<volatile std::uint32_t*>(labels.Data());
    *label = 0;
    std::memset(image.Data(), Before, Block);
    StoreThenLabel(image, label, 1);
    Require(AwaitLabel(label, 1), "storage write-back window: the control label never arrived");
    const bool pending = Pending(image);
    Publish(image);
    bool covered = true;
    for (const auto offset : LinkOffsets) covered = covered && Word(image, offset) == TexelPair;
    std::printf("[control] pendingAfterLabel=%d link0=0x%016llx link1=0x%016llx coveredByTexels=%d\n", pending,
                static_cast<unsigned long long>(Word(image, LinkOffsets[0])), static_cast<unsigned long long>(Word(image, LinkOffsets[1])), covered);
    return covered;
}

void Attempt(GuestBlock& image, GuestBlock& labels, std::uint32_t attempt, std::chrono::nanoseconds delay, Tally& tally) {
    auto* label = reinterpret_cast<volatile std::uint32_t*>(labels.Data());
    const auto value = 100 + attempt;
    *label = 0;
    std::memset(image.Data(), Before, Block);
    StoreThenLabel(image, label, value);
    Require(AwaitLabel(label, value), "storage write-back window: a label never arrived");
    if (!Pending(image)) ++tally.notPending;
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    Clock::time_point triggerStart, triggerEnd, linksStored;
    const auto triggerDelay = delay < std::chrono::nanoseconds::zero() ? -delay : std::chrono::nanoseconds::zero();
    const auto storeDelay = delay < std::chrono::nanoseconds::zero() ? std::chrono::nanoseconds::zero() : delay;
    std::thread driver([&] {
        ready.fetch_add(1);
        while (!go.load(std::memory_order_acquire)) {}
        const auto until = Clock::now() + triggerDelay;
        while (Clock::now() < until) {}
        triggerStart = Clock::now();
        std::array<std::byte, 16> read{};
        AgcDriver::GuestMemory::Read(image.Address() + TriggerOffset, read);
        triggerEnd = Clock::now();
    });
    std::thread allocator([&] {
        ready.fetch_add(1);
        while (!go.load(std::memory_order_acquire)) {}
        const auto until = Clock::now() + storeDelay;
        while (Clock::now() < until) {}
        for (std::size_t index = 0; index < LinkOffsets.size(); ++index) {
            const auto link = LinkValue(attempt, index);
            std::memcpy(image.Data() + LinkOffsets[index], &link, sizeof(link));
        }
        linksStored = Clock::now();
    });
    const auto wonBefore = StorageTexture::CpuWonStores();
    while (ready.load() != 2) {}
    go.store(true, std::memory_order_release);
    driver.join();
    allocator.join();
    Publish(image);
    const auto phase = linksStored < triggerStart ? Phase::BeforeTrigger : linksStored <= triggerEnd ? Phase::DuringTrigger : Phase::AfterTrigger;
    bool kept = true;
    bool texels = true;
    for (std::size_t index = 0; index < LinkOffsets.size(); ++index) {
        const auto observed = Word(image, LinkOffsets[index]);
        kept = kept && observed == LinkValue(attempt, index);
        texels = texels && observed == TexelPair;
    }
    auto& counts = kept ? tally.kept : tally.lost;
    ++counts[static_cast<std::size_t>(phase)];
    const bool resultsAround = Word(image, ResultOffset) == TexelPair;
    const bool won = StorageTexture::CpuWonStores() != wonBefore;
    if (kept && won) ++tally.wonInsideStore;
    if (!kept && texels) ++tally.publishedOverLinks;
    const auto micros = [](Clock::duration duration) { return std::chrono::duration<double, std::micro>(duration).count(); };
    std::printf("[attempt %u] delay=%.1fus trigger=[0, %.1fus] linksStoredAt=%.1fus phase=%s link0=0x%016llx link1=0x%016llx resultsAround=%d cpuWonStore=%d result=%s\n", attempt + 1, micros(delay),
                micros(triggerEnd - triggerStart), micros(linksStored - triggerStart), PhaseName(phase), static_cast<unsigned long long>(Word(image, LinkOffsets[0])),
                static_cast<unsigned long long>(Word(image, LinkOffsets[1])), resultsAround, won, kept ? "KEPT" : texels ? "LOST_TO_TEXELS" : "LOST");
}

enum class Verdict { Pass, Skip, Fail };

Verdict Run(bool watchedMemory) {
    GuestBlock image(watchedMemory);
    GuestBlock labels;
    const char* kind = watchedMemory ? "watched" : "compared";
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(image.Address()));
    Require(resource.width == Side && resource.height == Side && resource.format == Format8888Unorm && resource.tileMode == AgcDriver::Graphics::TextureTileMode::kR64KBX &&
                AgcDriver::Graphics::DescribeSurface(resource).guestBytes == Block,
            "storage write-back window: the descriptor is not a 64 KiB 32x32 RGBA8 R64KBX surface");
    const bool watched = AgcDriver::GuestMemory::Watched(image.Address(), Block);
    std::printf("[write-back window #5934] memory=%s surface=32x32 RGBA8 R64KBX footprint=0x%zx watched=%d hostImport=%s attempts=%u\n", kind, Block, watched,
                std::getenv("APS5_NO_HOST_IMPORT") != nullptr ? "disabled" : "allowed", Attempts);
    if (watched != watchedMemory) {
        std::printf("[write-back window #5934] memory=%s SKIP: the write watch does not cover the surface as asked\n", kind);
        return Verdict::Skip;
    }
    if (!Control(image, labels)) {
        std::printf("[write-back window #5934] memory=%s SKIP: the published result does not cover the link offsets\n", kind);
        return Verdict::Skip;
    }
    std::mt19937 random(5934);
    Tally tally;
    for (std::uint32_t attempt = 0; attempt < Attempts; ++attempt) {
        const auto delay = std::chrono::nanoseconds(std::uniform_int_distribution<std::int64_t>(-300'000, 1'300'000)(random));
        Attempt(image, labels, attempt, delay, tally);
    }
    std::printf("[write-back window #5934] memory=%s summary: kept before/during/after=%u/%u/%u lost before/during/after=%u/%u/%u lostToTexels=%u wonInsideStore=%u notPendingAtLabel=%u\n", kind,
                tally.kept[0], tally.kept[1], tally.kept[2], tally.lost[0], tally.lost[1], tally.lost[2], tally.publishedOverLinks, tally.wonInsideStore, tally.notPending);
    const auto lost = tally.lost[0] + tally.lost[1] + tally.lost[2];
    if (lost != 0) {
        std::printf("[write-back window #5934] memory=%s FAIL: %u of %u attempts lost free-list links the CPU stored after the guest observed the dispatch complete\n", kind, lost, Attempts);
        return Verdict::Fail;
    }
    if (tally.notPending == Attempts) {
        std::printf("[write-back window #5934] memory=%s PASS: every result reached memory before its label, so no CPU store could follow the label into a write-back\n", kind);
        return Verdict::Pass;
    }
    if (tally.kept[0] == 0 || tally.wonInsideStore == 0) {
        std::printf("[write-back window #5934] memory=%s SKIP: no CPU store landed before the trigger, or none after the store chose its blocks\n", kind);
        return Verdict::Skip;
    }
    std::printf("[write-back window #5934] memory=%s PASS: every CPU store survived, including %u made after the write-back chose its blocks\n", kind, tally.wonInsideStore);
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
        std::array<Verdict, 2> verdicts{Run(false), Run(true)};
        const auto count = [&](Verdict verdict) { return std::count(verdicts.begin(), verdicts.end(), verdict); };
        std::printf("[write-back window #5934] PASS=%td SKIP=%td FAIL=%td\n", count(Verdict::Pass), count(Verdict::Skip), count(Verdict::Fail));
        if (count(Verdict::Fail) != 0) return 1;
        return count(Verdict::Pass) != 0 ? 0 : VulkanTestSkipped;
    } catch (const std::exception& error) {
        std::cerr << "[write-back window #5934] FAIL: " << error.what() << '\n';
        return 1;
    }
}
