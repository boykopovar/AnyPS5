#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/MallocManagedSize.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <new>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t SpanBytes = 1u << 20u;
std::atomic<std::size_t> invalidated{0};
std::atomic<bool> liveInvalidated{false};
const std::vector<unsigned char*>* watched = nullptr;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

MallocManagedSize Stats() {
    MallocManagedSize stats{};
    stats.size = sizeof(stats);
    Require(GuestHeap::GuestHeapStatsFast_nid_postfix(&stats) == 0, "heap statistics unavailable");
    return stats;
}

void Invalidate(std::uintptr_t begin, std::size_t bytes) {
    if (bytes != SpanBytes) return;
    ++invalidated;
    if (watched == nullptr) return;
    for (const auto* pointer : *watched) {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        if (pointer != nullptr && address >= begin && address - begin < bytes) liveInvalidated = true;
    }
}

struct Observe {
    Observe() { GuestAllocations::GuestAllocationsSetInvalidator_nid_postfix(Invalidate); }
    ~Observe() {
        GuestAllocations::GuestAllocationsSetInvalidator_nid_postfix(nullptr);
        watched = nullptr;
    }
};

void Check(const unsigned char* pointer, std::size_t bytes, unsigned char value) {
    for (std::size_t offset = 0; offset < bytes; ++offset) Require(pointer[offset] == value, "live heap data changed");
}

std::vector<unsigned char*> Allocate(std::size_t count, std::size_t bytes) {
    std::vector<unsigned char*> blocks;
    blocks.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        auto* pointer = static_cast<unsigned char*>(GuestHeap::GuestHeapAllocate_nid_postfix(bytes));
        std::memset(pointer, static_cast<int>(index % 251 + 1), bytes);
        blocks.push_back(pointer);
    }
    return blocks;
}

void Release(std::vector<unsigned char*>& blocks, std::size_t bytes) {
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        if (blocks[index] == nullptr) continue;
        Check(blocks[index], bytes, static_cast<unsigned char>(index % 251 + 1));
        auto* pointer = blocks[index];
        blocks[index] = nullptr;
        GuestHeap::GuestHeapFree_nid_postfix(pointer);
    }
}

void Reclaim(bool expected) {
    const auto before = Stats();
    Observe observe;
    auto blocks = Allocate(32768, 4096);
    const auto allocated = Stats();
    Release(blocks, 4096);
    const auto freed = Stats();
    Require(freed.currentInuseSize == before.currentInuseSize, "freed objects remain live");
    if (expected) {
        Require(freed.currentSystemSize <= before.currentSystemSize + SpanBytes, "empty small spans remain committed");
        Require(invalidated > 0, "released small spans were not invalidated");
    } else {
        Require(freed.currentSystemSize == allocated.currentSystemSize, "default small heap retention changed");
        Require(invalidated == 0, "default small heap decommitted a span");
    }
    std::printf("{\"before\":%zu,\"allocated\":%zu,\"after_free\":%zu,\"inuse\":%zu,\"invalidated_spans\":%zu}\n", before.currentSystemSize, allocated.currentSystemSize, freed.currentSystemSize, freed.currentInuseSize, invalidated.load());
}

void LiveSpans() {
    const auto before = Stats();
    auto blocks = Allocate(8192, 4096);
    Observe observe;
    watched = &blocks;
    const auto allocated = Stats();
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        if (index % 17 == 0 || index + 1 == blocks.size()) continue;
        auto* pointer = blocks[index];
        blocks[index] = nullptr;
        GuestHeap::GuestHeapFree_nid_postfix(pointer);
    }
    Require(Stats().currentSystemSize == allocated.currentSystemSize, "a span containing live objects was released");
    Release(blocks, 4096);
    Require(!liveInvalidated, "invalidation covered a live allocation");
    Require(Stats().currentInuseSize == before.currentInuseSize, "live-span test leaked an object");
}

void Reuse() {
    const auto before = Stats();
    for (unsigned round = 0; round < 4; ++round) {
        auto blocks = Allocate(4096, 4096);
        std::vector<std::size_t> order(blocks.size());
        for (std::size_t index = 0; index < order.size(); ++index) order[index] = index;
        std::shuffle(order.begin(), order.end(), std::mt19937(17u + round));
        for (const auto index : order) {
            Check(blocks[index], 4096, static_cast<unsigned char>(index % 251 + 1));
            GuestHeap::GuestHeapFree_nid_postfix(blocks[index]);
        }
        Require(Stats().currentInuseSize == before.currentInuseSize, "reuse left a live allocation");
    }
}

void Alignment() {
    const auto before = Stats();
    for (const auto alignment : {32u, 64u, 256u, 4096u, 16384u}) {
        std::vector<unsigned char*> blocks;
        for (unsigned index = 0; index < 512; ++index) {
            auto* pointer = static_cast<unsigned char*>(GuestHeap::GuestHeapAlign_nid_postfix(alignment, 128));
            Require(reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0, "allocation alignment changed");
            std::memset(pointer, 0x5a, 128);
            blocks.push_back(pointer);
        }
        for (auto* pointer : blocks) {
            auto* moved = static_cast<unsigned char*>(GuestHeap::GuestHeapRealign_nid_postfix(pointer, 256, alignment));
            Require(reinterpret_cast<std::uintptr_t>(moved) % alignment == 0, "realignment changed");
            Check(moved, 128, 0x5a);
            Require(GuestHeap::GuestHeapUsableSize_nid_postfix(moved) == 256, "usable size changed");
            GuestHeap::GuestHeapFree_nid_postfix(moved);
        }
    }
    auto* pointer = GuestHeap::GuestHeapAllocate_nid_postfix(4096);
    Require(GuestHeap::GuestHeapReallocate_nid_postfix(pointer, 0) == pointer, "zero-size realloc no longer retains the block");
    Require(GuestHeap::GuestHeapUsableSize_nid_postfix(pointer) == 0, "zero-size realloc has nonzero usable size");
    auto others = Allocate(4096, 4096);
    Release(others, 4096);
    GuestHeap::GuestHeapFree_nid_postfix(pointer);
    Require(Stats().currentInuseSize == before.currentInuseSize, "alignment test leaked an allocation");
}

void Threads() {
    const auto before = Stats();
    std::atomic<bool> failed{false};
    std::vector<std::thread> workers;
    for (unsigned index = 0; index < 8; ++index) {
        workers.emplace_back([&] {
            try {
                for (unsigned round = 0; round < 8; ++round) {
                    auto blocks = Allocate(128, 4096);
                    Release(blocks, 4096);
                }
            } catch (...) { failed = true; }
        });
    }
    for (auto& worker : workers) worker.join();
    Require(!failed, "concurrent allocation or free failed");
    Require(Stats().currentInuseSize == before.currentInuseSize, "concurrent operations leaked an allocation");
}

void Boundaries() {
    const auto before = Stats();
    for (const auto bytes : {0u, 1u, 16u, 31u, 32u, 63u, 64u, 127u, 128u, 255u, 256u, 511u, 512u, 1023u, 4095u, 4096u, 16384u, 32768u, 65488u, 65536u}) {
        auto blocks = Allocate(std::min<std::size_t>(32768, 2 * SpanBytes / (bytes + 64)), bytes);
        Release(blocks, bytes);
        Require(Stats().currentInuseSize == before.currentInuseSize, "boundary allocation remained live");
    }
}

void FailCommit(std::uintptr_t, std::size_t) { throw std::bad_alloc(); }

void Rollback() {
    const auto before = Stats();
    GuestArena::GuestArenaSetAccessObserver_nid_postfix(FailCommit);
    bool rejected = false;
    try {
        static_cast<void>(GuestHeap::GuestHeapAllocate_nid_postfix(4096));
    } catch (const std::bad_alloc&) { rejected = true; }
    GuestArena::GuestArenaSetAccessObserver_nid_postfix(nullptr);
    Require(rejected, "commit failure was not propagated");
    Require(Stats().currentSystemSize == before.currentSystemSize && Stats().currentInuseSize == before.currentInuseSize, "failed span allocation changed heap accounting");
    auto blocks = Allocate(512, 4096);
    Release(blocks, 4096);
}

std::vector<std::uintptr_t> quarantined;

void FailInvalidation(std::uintptr_t address, std::size_t bytes) {
    if (bytes != SpanBytes) return;
    quarantined.push_back(address);
    throw std::runtime_error("injected span invalidation failure");
}

void Quarantine() {
    const auto before = Stats();
    auto blocks = Allocate(1024, 4096);
    GuestAllocations::GuestAllocationsSetInvalidator_nid_postfix(FailInvalidation);
    std::size_t failures = 0;
    for (auto* pointer : blocks) {
        try {
            GuestHeap::GuestHeapFree_nid_postfix(pointer);
        } catch (const std::runtime_error&) { ++failures; }
    }
    GuestAllocations::GuestAllocationsSetInvalidator_nid_postfix(nullptr);
    Require(failures != 0 && failures == quarantined.size(), "span invalidation failures were not propagated");
    Require(Stats().currentSystemSize == before.currentSystemSize + SpanBytes, "decommitted spans remain counted as committed");
    auto next = Allocate(1024, 4096);
    for (const auto* pointer : next) {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        for (const auto begin : quarantined)
            Require(address < begin || address - begin >= SpanBytes, "quarantined span was reused");
    }
    Release(next, 4096);
    Require(Stats().currentInuseSize == before.currentInuseSize, "failed invalidation left live objects");
}

GuestAllocations::Lease pinned;
unsigned char* pinnedPointer = nullptr;
bool waited = false;

bool FinishPinned(std::uintptr_t address, std::size_t bytes) {
    Require(address == reinterpret_cast<std::uintptr_t>(pinnedPointer) && bytes == 4096, "unexpected pinned range");
    Check(pinnedPointer, bytes, 0x63);
    pinned.clear();
    waited = true;
    return true;
}

void Pinning() {
    auto blocks = Allocate(1024, 4096);
    pinnedPointer = blocks.back();
    blocks.pop_back();
    std::memset(pinnedPointer, 0x63, 4096);
    Release(blocks, 4096);
    GuestAllocations::Mapped mapped{GuestAllocations::GuestAllocationsLookup_nid_postfix(reinterpret_cast<std::uintptr_t>(pinnedPointer), false)};
    pinned = GuestAllocations::GuestAllocationsAcquireMapped_nid_no_patch(mapped);
    Require(!pinned.empty(), "heap allocation was not leased");
    GuestAllocations::GuestAllocationsSetPinWaiter_nid_postfix(FinishPinned);
    GuestHeap::GuestHeapFree_nid_postfix(pinnedPointer);
    GuestAllocations::GuestAllocationsSetPinWaiter_nid_postfix(nullptr);
    Require(waited, "free did not wait for a live allocation lease");
    Require(Stats().currentInuseSize == 0, "pinning test leaked an allocation");
}

void Phases() {
    const auto before = Stats();
    auto first = Allocate(32768, 4096);
    Release(first, 4096);
    const auto between = Stats();
    auto second = Allocate(16384, 8192);
    const auto peak = Stats();
    Release(second, 8192);
    const auto after = Stats();
    Require(after.currentInuseSize == before.currentInuseSize, "phased workload left live allocations");
    std::printf("{\"requested_each_phase\":134217728,\"before\":%zu,\"after_first_free\":%zu,\"peak_system\":%zu,\"second_live_system\":%zu,\"after_second_free\":%zu,\"inuse\":%zu}\n", before.currentSystemSize, between.currentSystemSize, peak.maxSystemSize, peak.currentSystemSize, after.currentSystemSize, after.currentInuseSize);
}

void Sparse(bool expected) {
    const auto before = Stats();
    const std::vector<std::size_t> sizes{1, 32, 64, 96, 128, 160, 192, 224, 256, 320, 400, 500, 640, 800, 1024, 1280, 1600, 2048, 2560, 3200, 4096, 5120, 6400, 8192, 10240, 12800, 16384, 20480, 25600, 32768, 40000, 50000, 60000};
    std::vector<unsigned char*> blocks;
    for (std::size_t index = 0; index < sizes.size(); ++index) {
        auto* pointer = static_cast<unsigned char*>(GuestHeap::GuestHeapAllocate_nid_postfix(sizes[index]));
        std::memset(pointer, static_cast<int>(index + 1), sizes[index]);
        blocks.push_back(pointer);
    }
    const auto live = Stats();
    for (std::size_t index = 0; index < sizes.size(); ++index) {
        Check(blocks[index], sizes[index], static_cast<unsigned char>(index + 1));
        GuestHeap::GuestHeapFree_nid_postfix(blocks[index]);
    }
    const auto after = Stats();
    Require(after.currentInuseSize == before.currentInuseSize, "sparse allocations remain live");
    std::printf("{\"before\":%zu,\"live_system\":%zu,\"after_free\":%zu,\"inuse\":%zu}\n", before.currentSystemSize, live.currentSystemSize, after.currentSystemSize, after.currentInuseSize);
    if (expected)
        Require(live.currentSystemSize - before.currentSystemSize <= sizes.size() * 65536, "lightly used classes commit whole spans");
}

void GrowthRollback() {
    auto blocks = Allocate(8, 4096);
    GuestArena::GuestArenaSetAccessObserver_nid_postfix(FailCommit);
    bool rejected = false;
    for (unsigned attempt = 0; attempt < 256; ++attempt) {
        const auto before = Stats();
        try {
            auto* pointer = static_cast<unsigned char*>(GuestHeap::GuestHeapAllocate_nid_postfix(4096));
            std::memset(pointer, static_cast<int>(blocks.size() % 251 + 1), 4096);
            blocks.push_back(pointer);
        } catch (const std::bad_alloc&) {
            rejected = true;
            Require(Stats().currentSystemSize == before.currentSystemSize && Stats().currentInuseSize == before.currentInuseSize, "failed growth changed heap accounting");
            break;
        }
    }
    GuestArena::GuestArenaSetAccessObserver_nid_postfix(nullptr);
    Require(rejected, "span growth did not reach the injected commit failure");
    auto* next = static_cast<unsigned char*>(GuestHeap::GuestHeapAllocate_nid_postfix(4096));
    std::memset(next, static_cast<int>(blocks.size() % 251 + 1), 4096);
    blocks.push_back(next);
    Release(blocks, 4096);
    Require(Stats().currentInuseSize == 0, "failed span growth leaked live objects");
}

void FailGrowthInvalidation(std::uintptr_t, std::size_t bytes) {
    if (bytes == 65536) throw std::runtime_error("injected growth rollback failure");
}

void GrowthQuarantine() {
    auto blocks = Allocate(8, 4096);
    const auto begin = reinterpret_cast<std::uintptr_t>(reinterpret_cast<void**>(blocks.front())[-2]);
    GuestArena::GuestArenaSetAccessObserver_nid_postfix(FailCommit);
    GuestAllocations::GuestAllocationsSetInvalidator_nid_postfix(FailGrowthInvalidation);
    bool rejected = false;
    for (unsigned attempt = 0; attempt < 256; ++attempt) {
        const auto before = Stats();
        try {
            auto* pointer = static_cast<unsigned char*>(GuestHeap::GuestHeapAllocate_nid_postfix(4096));
            std::memset(pointer, static_cast<int>(blocks.size() % 251 + 1), 4096);
            blocks.push_back(pointer);
        } catch (const std::runtime_error&) {
            rejected = true;
            Require(Stats().currentSystemSize == before.currentSystemSize && Stats().currentInuseSize == before.currentInuseSize, "rollback invalidation failure changed successful-commit accounting");
            break;
        }
    }
    GuestArena::GuestArenaSetAccessObserver_nid_postfix(nullptr);
    GuestAllocations::GuestAllocationsSetInvalidator_nid_postfix(nullptr);
    Require(rejected, "growth rollback failure was not propagated");
    auto next = Allocate(1024, 4096);
    for (const auto* pointer : next) {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        Require(address < begin || address - begin >= SpanBytes, "quarantined live span was reused");
    }
    Release(blocks, 4096);
    Release(next, 4096);
    Require(Stats().currentInuseSize == 0, "quarantining a live span leaked registered objects");
}

}

int main(int argc, char** argv) {
    try {
        Require(GuestArena::GuestArenaAvailable_nid_postfix(), "guest arena unavailable");
        const std::string mode = argc > 1 ? argv[1] : "reclaim";
        if (mode == "reclaim") Reclaim(true);
        else if (mode == "default") Reclaim(false);
        else if (mode == "live") LiveSpans();
        else if (mode == "reuse") Reuse();
        else if (mode == "alignment") Alignment();
        else if (mode == "threads") Threads();
        else if (mode == "boundaries") Boundaries();
        else if (mode == "rollback") Rollback();
        else if (mode == "quarantine") Quarantine();
        else if (mode == "pinning") Pinning();
        else if (mode == "phases") Phases();
        else if (mode == "sparse") Sparse(true);
        else if (mode == "sparse-probe") Sparse(false);
        else if (mode == "growth-rollback") GrowthRollback();
        else if (mode == "growth-quarantine") GrowthQuarantine();
        else throw std::runtime_error("unknown small-span test mode");
        std::puts("small heap span tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
