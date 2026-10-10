#include <Testing/Test.hpp>
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace AgcDriver::GuestMemory;
using Testing::Case;
using Testing::Require;

constexpr std::size_t Block = 65536;

void RequireWriteWatching() {
    if (!WriteWatched()) Testing::Skip("no write watching");
}

void* AllocateWatched(std::size_t bytes, bool crossLeaf) {
#ifdef _WIN32
    static_cast<void>(crossLeaf);
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block);
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
    const std::size_t alignment = crossLeaf ? std::size_t{1} << 32 : Block;
    const auto prefix = crossLeaf ? Block : 0;
    void* raw = mmap(nullptr, bytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) Testing::Fail("cannot map the watched block");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = ((begin + prefix + alignment - 1) & ~static_cast<std::uintptr_t>(alignment - 1)) - prefix;
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + bytes != begin + bytes + alignment) munmap(reinterpret_cast<void*>(aligned + bytes), begin + alignment - aligned);
    void* block = reinterpret_cast<void*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    return block;
}

enum class Unregistration {
    Unregister,
    Unwatch,
};

class WatchedBlock {
public:
    explicit WatchedBlock(std::size_t size, bool crossLeaf = false, Unregistration mode = Unregistration::Unregister)
        : bytes(size), unregistration(mode), memory(static_cast<std::uint8_t*>(AllocateWatched(size, crossLeaf))) {
        if (Watched(Base(), bytes)) return;
        Free();
        Testing::Fail("the test block is not watched");
    }

    ~WatchedBlock() {
        Free();
    }

    WatchedBlock(const WatchedBlock&) = delete;
    WatchedBlock& operator=(const WatchedBlock&) = delete;

    std::uint8_t* Data() const noexcept {
        return memory;
    }

    std::uint64_t Base() const noexcept {
        return reinterpret_cast<std::uint64_t>(memory);
    }

    void Adopt(void* replacement) noexcept {
        memory = static_cast<std::uint8_t*>(replacement);
    }

    void Free() noexcept {
        if (memory == nullptr) return;
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(memory, bytes);
        GuestArena::GuestArenaRelease_nid_postfix(memory, bytes);
#else
        if (unregistration == Unregistration::Unwatch) {
            Unwatch(Base(), bytes);
        } else {
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(memory, bytes);
        }
        munmap(memory, bytes);
#endif
        memory = nullptr;
    }

private:
    std::size_t bytes;
    Unregistration unregistration;
    std::uint8_t* memory;
};

struct SharedBlock {
    SharedBlock() : block(3 * Block) {
        std::memset(block.Data(), 0x11, 3 * Block);
        synced = CollectWrites(Base(), 3 * Block);
        Require(synced != 0, "the test block is not collected");
    }

    std::uint64_t Base() const noexcept {
        return block.Base();
    }

    std::uint64_t Boundary() const noexcept {
        return Base() + 2 * Block - 0xb800;
    }

    std::size_t SharedFirst() const noexcept {
        return static_cast<std::size_t>(Boundary() - (Base() + Block));
    }

    std::size_t SharedSecond() const noexcept {
        return static_cast<std::size_t>(Base() + 2 * Block - Boundary());
    }

    std::size_t SecondSurface() const noexcept {
        return static_cast<std::size_t>(Base() + 3 * Block - Boundary());
    }

    WatchedBlock block;
    std::uint64_t synced = 0;
};

const Case firstSurfaceStore{"StoredOver_FirstSurfaceStoreInSharedBlock_CountsOnlyForTheFirstSurface", [] {
    RequireWriteWatching();
    const SharedBlock shared;
    Require(MarkWritten(shared.Base(), static_cast<std::size_t>(shared.Boundary() - shared.Base())) != 0, "a driver store into watched memory is not stamped");
    std::array<std::uint64_t, 2> generations{shared.synced, shared.synced};
    std::array<std::uint8_t, 2> changed{};
    Require(ChangedBlocks(shared.Base() + Block, 2 * Block, generations, changed) && changed[0] == BlockWritten && changed[1] == BlockUnchanged, "the shared block does not read as written");
    Require(!StoredOver(shared.Boundary(), shared.SharedSecond(), shared.synced), "a store of the neighbour's bytes counts for the second surface's part of the shared block");
    Require(StoredOver(shared.Base() + Block, shared.SharedFirst(), shared.synced), "the first surface's own store is not seen over its part of the shared block");
    Require(!StoredOver(shared.Boundary(), shared.SecondSurface(), shared.synced), "the second surface reads as stored over");
}};

const Case secondSurfaceStore{"StoredOver_StoreOverSecondSurfaceBytes_IsSeenOnlySinceOlderGenerations", [] {
    RequireWriteWatching();
    const SharedBlock shared;
    MarkWritten(shared.Base(), static_cast<std::size_t>(shared.Boundary() - shared.Base()));
    const auto touched = MarkWritten(shared.Boundary() + 0x100, 4);
    Require(StoredOver(shared.Boundary(), shared.SharedSecond(), shared.synced), "a store over the second surface's bytes is not seen");
    Require(!StoredOver(shared.Boundary(), shared.SharedSecond(), touched), "a store older than the generation is seen");
}};

const Case wholeBlockStore{"StoredOver_WholeBlockThenPartialStore_TracksEachStoreRange", [] {
    RequireWriteWatching();
    const SharedBlock shared;
    const auto whole = TrackerGeneration();
    MarkWritten(shared.Base() + Block, Block);
    Require(StoredOver(shared.Boundary(), shared.SharedSecond(), whole), "a store over the whole block is not seen");
    const auto afterWhole = TrackerGeneration();
    MarkWritten(shared.Base() + Block, shared.SharedFirst());
    Require(!StoredOver(shared.Boundary(), shared.SharedSecond(), afterWhole), "a partial store after a whole-block store is taken for the whole block");
}};

const Case forgottenStores{"StoredOver_TooManyDriverStoresInABlock_IsConservative", [] {
    RequireWriteWatching();
    const SharedBlock shared;
    const auto many = TrackerGeneration();
    for (int store = 0; store < 6; ++store) MarkWritten(shared.Base() + Block + static_cast<std::uint64_t>(store) * 64, 4);
    Require(StoredOver(shared.Boundary(), shared.SharedSecond(), many), "forgotten driver stores are taken as missing the range");
}};

void StoreAcrossTheSharedBlock(const SharedBlock& shared) {
    MarkWritten(shared.Base(), static_cast<std::size_t>(shared.Boundary() - shared.Base()));
    MarkWritten(shared.Boundary() + 0x100, 4);
    MarkWritten(shared.Base() + Block, Block);
    MarkWritten(shared.Base() + Block, shared.SharedFirst());
    for (int store = 0; store < 6; ++store) MarkWritten(shared.Base() + Block + static_cast<std::uint64_t>(store) * 64, 4);
}

const Case cpuStoreOutside{"StoredOver_CpuStoreAfterDriverStoresOutsideTheSharedRange_DoesNotCount", [] {
    RequireWriteWatching();
    const SharedBlock shared;
    StoreAcrossTheSharedBlock(shared);
    const auto beforeCpu = CollectWrites(shared.Base(), 3 * Block);
    static_cast<volatile std::uint8_t*>(shared.block.Data())[Block + 8] = 0x22;
    CollectWritesUncached(shared.Base(), 3 * Block);
    Require(!StoredOver(shared.Boundary(), shared.SharedSecond(), beforeCpu), "a CPU store outside the shared range counts for it");
}};

const Case unwatchedRange{"StoredOver_UnwatchedRange_ReadsAsStoredOver", [] {
    RequireWriteWatching();
    std::array<std::uint8_t, 64> unwatched{};
    const auto outside = reinterpret_cast<std::uint64_t>(unwatched.data());
    if (!Watched(outside, unwatched.size())) Require(StoredOver(outside, unwatched.size(), TrackerGeneration()), "an unwatched range reads as not stored over");
}};

const Case ownStore{"Write_DriverStore_IsStampedAsADriverStoreNotACpuWrite", [] {
    RequireWriteWatching();
    const WatchedBlock block(2 * Block);
    const auto base = block.Base();
    const auto second = base + Block;
    auto* memory = static_cast<volatile std::uint8_t*>(block.Data());
    std::memset(block.Data(), 0x11, 2 * Block);
    const auto synced = CollectWrites(base, 2 * Block);
    Require(synced != 0, "the own-store block is not collected");

    std::array<std::byte, 8192> bytes{};
    bytes.fill(std::byte{0x44});
    Write(second, bytes);
    const auto stored = TrackerGeneration();
    Require(!UnchangedSince(second, bytes.size(), synced), "the driver store is not stamped");
    CollectWritesUncached(base, 2 * Block);
    Require(UnchangedSince(second, Block, stored), "a walk after a driver store reports the store again as a newer write");
    Require(UnchangedSinceCollected(second, bytes.size(), synced), "the driver store's own page faults are stamped as a CPU write");

    memory[Block + 3 * 4096 + 8] = 0x22;
    const std::array<std::byte, 64> label{};
    Write(second + 3 * 4096 + 64, label);
    Require(!UnchangedSinceCollected(second + 3 * 4096, 4096, stored), "a CPU write before a driver store in its page is not stamped as one");

    const auto beforeCpu = TrackerGeneration();
    memory[16] = 0x33;
    CollectWritesUncached(base, 2 * Block);
    Require(!UnchangedSince(base, 64, beforeCpu), "a CPU write after the driver store is not seen");
}};

const Case unwatch{"Unwatch_ImportedBlock_StopsTrustingStampsOnlyThere", [] {
    RequireWriteWatching();
    const WatchedBlock block(3 * Block);
    const auto base = block.Base();
    const auto imported = base + Block;
    std::memset(block.Data(), 0x11, 3 * Block);
    BumpCollectEpoch();
    const auto before = CollectWrites(base, 3 * Block);
    Require(before != 0 && UnchangedSince(imported, Block, before), "the import range was not initially watched");
    Unwatch(imported, Block);
    Require(!Watched(imported, Block) && !UnchangedSince(imported, Block, before), "unwatch kept trusting import stamps");
    Require(CollectWrites(base, 3 * Block) == 0 && CollectWritesUncached(imported, Block) == 0, "unwatch did not invalidate a cached collect");
    Require(Watched(base, Block) && Watched(base + 2 * Block, Block), "unwatch disabled unrelated memory");
    const auto beforeDriverWrite = TrackerGeneration();
    Require(MarkWritten(imported, Block) == 0, "an unwatched import still produces trusted stamps");
    Require(TrackerGeneration() > beforeDriverWrite, "an unwatched driver write did not invalidate byte comparisons");
    const auto beforeOwnWrite = TrackerGeneration();
    StoreOwnBytes(imported, 1, [&] { *reinterpret_cast<std::uint8_t*>(imported) = 0x42; });
    Require(TrackerGeneration() > beforeOwnWrite, "an unwatched CPU store did not invalidate byte comparisons");
    std::array<std::uint64_t, 1> generations{before};
    std::array<std::uint8_t, 1> changed{};
    Require(!ChangedBlocks(imported, Block, generations, changed), "an unwatched import still uses block stamps");
    Unwatch(imported, Block);
    const auto adjacent = CollectWrites(base, Block);
    Require(adjacent != 0, "unwatch broke the adjacent block's collect");
    static_cast<volatile std::uint8_t*>(block.Data())[0] = 0x33;
    CollectWritesUncached(base, Block);
    Require(!UnchangedSince(base, Block, adjacent), "unwatch hid a CPU edit in unrelated memory");
}};

#ifdef _WIN32
const Case privateMappingReuse{"Unwatch_FreshPrivateBackingAtAnImportedAddress_IsWatchedWithNewGenerations", [] {
    RequireWriteWatching();
    WatchedBlock block(3 * Block);
    void* memory = block.Data();
    const auto base = block.Base();
    std::memset(memory, 0x11, 3 * Block);
    BumpCollectEpoch();
    const auto before = CollectWrites(base, 3 * Block);
    Require(before != 0, "the original backing was not collected");
    Unwatch(base + Block, Block);
    Require(!Watched(base + Block, Block), "the imported block stayed watched");
    GuestArena::GuestArenaCommit_nid_postfix(memory, 3 * Block, PAGE_READWRITE, 3 * Block);
    Require(!Watched(base + Block, Block), "a protection-only commit restored an imported block");
    const auto neighbour = CollectWrites(base, Block);
    Require(neighbour != 0, "the neighbouring block was not collected before remapping");
    block.Free();
    auto* replacement = GuestArena::GuestArenaAllocateAtOrAbove_nid_postfix(base, 3 * Block, Block);
    block.Adopt(replacement);
    Require(replacement == memory, "the released arena address was not reused");
    GuestArena::GuestArenaCommit_nid_postfix(replacement, 3 * Block, PAGE_READWRITE, 3 * Block);
    Require(Watched(base, 3 * Block), "fresh backing at an excluded address is not watched");
    Require(!UnchangedSince(base + Block, Block, before), "fresh backing reused an old import generation");
    Require(!UnchangedSince(base, Block, neighbour), "fresh backing reused an adjacent block's old generation");
    const auto after = CollectWrites(base, 3 * Block);
    Require(after > before && UnchangedSince(base, 3 * Block, after), "fresh backing was not collected");
    static_cast<volatile std::uint8_t*>(replacement)[Block + 8] = 0x22;
    CollectWritesUncached(base, 3 * Block);
    Require(!UnchangedSince(base + Block, Block, after), "a CPU edit in fresh backing was missed");
    Unwatch(base + Block, Block);
    Require(!Watched(base + Block, Block) && CollectWrites(base + Block, Block) == 0, "a second import of fresh backing stayed watched");
}};
#endif

const Case versionRanges{"UnchangedSince_RandomWritesAndQueries_AgreeWithTheWriteHistory", [] {
    RequireWriteWatching();
    constexpr std::size_t count = 80;
    constexpr auto bytes = count * Block;
    const WatchedBlock block(bytes, true);
    auto* memory = block.Data();
    const auto base = block.Base();
    Require(UnchangedSince(base, bytes, TrackerGeneration()), "a new range has newer version stamps");
    std::memset(memory, 17, bytes);
    const auto initial = CollectWritesUncached(base, bytes);
    Require(initial != 0, "version ranges could not be collected");
    struct Write {
        std::size_t block;
        std::uint64_t generation;
    };
    std::vector<Write> history;
    std::vector<std::uint64_t> checkpoints{initial};
    std::uint32_t random = 0x61387a29;
    const auto next = [&] { random = random * 1664525u + 1013904223u; return random; };
    const auto expected = [&](const UnchangedQuery& query) {
        if (query.generation == 0) return false;
        for (const auto& write : history) {
            const auto begin = base + write.block * Block;
            if (write.generation > query.generation && begin < query.address + query.bytes && query.address < begin + Block) return false;
        }
        return true;
    };
    for (unsigned step = 0; step < 512; ++step) {
        const auto written = step < 3 ? step : next() % count;
        std::uint64_t generation;
        if (step % 3 == 0) {
            memory[written * Block + 9] ^= 1;
            generation = CollectWritesUncached(base + written * Block, Block);
        } else {
            generation = MarkWritten(base + written * Block + 13, 4);
        }
        Require(generation > checkpoints.back(), "version range write did not advance the tracker at step " + std::to_string(step));
        history.push_back({written, generation});
        checkpoints.push_back(generation);
        for (unsigned check = 0; check < 16; ++check) {
            std::array<UnchangedQuery, 3> queries;
            bool all = true;
            for (auto& query : queries) {
                const auto offset = next() % bytes;
                query = {base + offset, 1 + next() % (bytes - offset), checkpoints[next() % checkpoints.size()]};
                const auto same = expected(query);
                Require(UnchangedSince(query.address, query.bytes, query.generation) == same, "version range disagrees with write history at step " + std::to_string(step));
                all = all && same;
            }
            Require(UnchangedSinceAll(queries) == all, "batched version ranges disagree with write history at step " + std::to_string(step));
        }
    }
    Require(!UnchangedSince(base, bytes, 0), "an unknown generation was accepted");
    Require(UnchangedSince(base, bytes, checkpoints.back()), "the latest checkpoint was rejected");
}};


const Case threadWithoutEpochs{"CollectWrites_ThreadWithoutCollectEpochs_ObservesEveryWrite", [] {
    RequireWriteWatching();
    const WatchedBlock block(3 * Block, false, Unregistration::Unwatch);
    std::memset(block.Data(), 17, 3 * Block);
    auto* memory = block.Data();
    const auto base = block.Base();
    bool unboundedObserved = false;
    std::thread observer([&] {
        const auto before = CollectWrites(base + 7, 1);
        memory[7] = 19;
        const auto after = CollectWrites(base + 7, 1);
        unboundedObserved = before != 0 && after > before && !UnchangedSince(base + 7, 1, before);
    });
    observer.join();
    Require(unboundedObserved, "a thread without collect epochs reused a stale observation");
}};

const Case collectEpochs{"CollectWrites_AcrossEpochsAndUncachedOrDriverWrites_SeesEveryChange", [] {
    RequireWriteWatching();
    const WatchedBlock block(3 * Block, false, Unregistration::Unwatch);
    std::memset(block.Data(), 17, 3 * Block);
    auto* memory = block.Data();
    const auto base = block.Base();
    for (const auto offset : {7u, 4096u + 3u, static_cast<unsigned>(Block - 8), static_cast<unsigned>(Block + 3)}) {
        const auto at = " at offset " + std::to_string(offset);
        BumpCollectEpoch();
        const auto before = CollectWrites(base + offset, 16);
        Require(before != 0, "an epoch lost watched coverage" + at);
        memory[offset] ^= 1;
        BumpCollectEpoch();
        Require(CollectWrites(base + offset, 16) > before && !UnchangedSince(base + offset, 16, before), "a new epoch missed a CPU write" + at);
        const auto observed = TrackerGeneration();
        memory[offset + 1] ^= 1;
        Require(CollectWritesUncached(base + offset, 16) > observed && !UnchangedSince(base + offset, 16, observed), "an uncached observation reused an epoch result" + at);
        const auto stored = MarkWritten(base + offset + 2, 1);
        Require(CollectWrites(base + offset, 16) >= stored && !UnchangedSince(base + offset, 16, observed), "a memoized observation hid a driver write" + at);
    }
}};

#ifndef _WIN32
const Case partialBlock{"CollectWrites_UnwatchedNeighbourPage_KeepsCollectingTheWatchedPrefix", [] {
    RequireWriteWatching();
    const WatchedBlock block(3 * Block, false, Unregistration::Unwatch);
    std::memset(block.Data(), 17, 3 * Block);
    auto* memory = block.Data();
    const auto base = block.Base();
    BumpCollectEpoch();
    Require(CollectWrites(base, 1) != 0, "initial partial-block observation failed");
    Unwatch(base + 4096, 4096);
    Require(CollectWrites(base + 4096, 1) == 0, "an unwatched neighbor retained an epoch observation");
    BumpCollectEpoch();
    const auto before = CollectWrites(base, 16);
    Require(before != 0, "an unwatched neighbor prevented collecting a watched prefix");
    memory[0] ^= 1;
    BumpCollectEpoch();
    Require(CollectWrites(base, 16) > before && !UnchangedSince(base, 16, before), "partial-block collection missed a CPU write");
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(memory + 4096, 4096);
    Require(CollectWrites(base + 4096, 1) != 0, "a re-registered neighbor stayed unwatched");
}};
#endif

} // namespace
