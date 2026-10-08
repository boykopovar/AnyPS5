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
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace AgcDriver::GuestMemory;

constexpr std::size_t Block = 65536;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void* AllocateWatched(std::size_t bytes) {
#ifdef _WIN32
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block);
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
    void* raw = mmap(nullptr, bytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) throw std::runtime_error("cannot map the watched block");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (begin + Block - 1) & ~static_cast<std::uintptr_t>(Block - 1);
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + bytes != begin + bytes + Block) munmap(reinterpret_cast<void*>(aligned + bytes), begin + Block - aligned);
    void* block = reinterpret_cast<void*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    Require(Watched(reinterpret_cast<std::uint64_t>(block), bytes), "the test block is not watched");
    return block;
}

void CheckSharedBlock() {
    void* memory = AllocateWatched(3 * Block);
    const auto base = reinterpret_cast<std::uint64_t>(memory);
    const auto boundary = base + 2 * Block - 0xb800;
    const auto firstBegin = base;
    const auto secondEnd = base + 3 * Block;
    const auto sharedFirst = boundary - (base + Block);
    const auto sharedSecond = base + 2 * Block - boundary;
    std::memset(memory, 0x11, 3 * Block);

    const auto synced = CollectWrites(base, 3 * Block);
    Require(synced != 0, "the test block is not collected");

    Require(MarkWritten(firstBegin, static_cast<std::size_t>(boundary - firstBegin)) != 0, "a driver store into watched memory is not stamped");
    std::array<std::uint64_t, 2> generations{synced, synced};
    std::array<std::uint8_t, 2> changed{};
    Require(ChangedBlocks(base + Block, 2 * Block, generations, changed) && changed[0] == BlockWritten && changed[1] == BlockUnchanged, "the shared block does not read as written");
    Require(!StoredOver(boundary, static_cast<std::size_t>(sharedSecond), synced), "a store of the neighbour's bytes counts for the second surface's part of the shared block");
    Require(StoredOver(base + Block, static_cast<std::size_t>(sharedFirst), synced), "the first surface's own store is not seen over its part of the shared block");
    Require(!StoredOver(boundary, static_cast<std::size_t>(secondEnd - boundary), synced), "the second surface reads as stored over");

    const auto touched = MarkWritten(boundary + 0x100, 4);
    Require(StoredOver(boundary, static_cast<std::size_t>(sharedSecond), synced), "a store over the second surface's bytes is not seen");
    Require(!StoredOver(boundary, static_cast<std::size_t>(sharedSecond), touched), "a store older than the generation is seen");

    const auto whole = TrackerGeneration();
    MarkWritten(base + Block, Block);
    Require(StoredOver(boundary, static_cast<std::size_t>(sharedSecond), whole), "a store over the whole block is not seen");
    const auto afterWhole = TrackerGeneration();
    MarkWritten(base + Block, static_cast<std::size_t>(sharedFirst));
    Require(!StoredOver(boundary, static_cast<std::size_t>(sharedSecond), afterWhole), "a partial store after a whole-block store is taken for the whole block");

    const auto many = TrackerGeneration();
    for (int store = 0; store < 6; ++store) MarkWritten(base + Block + static_cast<std::uint64_t>(store) * 64, 4);
    Require(StoredOver(boundary, static_cast<std::size_t>(sharedSecond), many), "forgotten driver stores are taken as missing the range");

    const auto beforeCpu = CollectWrites(base, 3 * Block);
    static_cast<volatile std::uint8_t*>(memory)[Block + 8] = 0x22;
    CollectWritesUncached(base, 3 * Block);
    Require(!StoredOver(boundary, static_cast<std::size_t>(sharedSecond), beforeCpu), "a CPU store outside the shared range counts for it");

    std::array<std::uint8_t, 64> unwatched{};
    const auto outside = reinterpret_cast<std::uint64_t>(unwatched.data());
    if (!Watched(outside, unwatched.size())) Require(StoredOver(outside, unwatched.size(), TrackerGeneration()), "an unwatched range reads as not stored over");
}

void CheckOwnStore() {
    void* memory = AllocateWatched(2 * Block);
    const auto base = reinterpret_cast<std::uint64_t>(memory);
    const auto second = base + Block;
    std::memset(memory, 0x11, 2 * Block);
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

    static_cast<volatile std::uint8_t*>(memory)[Block + 3 * 4096 + 8] = 0x22;
    const std::array<std::byte, 64> label{};
    Write(second + 3 * 4096 + 64, label);
    Require(!UnchangedSinceCollected(second + 3 * 4096, 4096, stored), "a CPU write before a driver store in its page is not stamped as one");

    const auto beforeCpu = TrackerGeneration();
    static_cast<volatile std::uint8_t*>(memory)[16] = 0x33;
    CollectWritesUncached(base, 2 * Block);
    Require(!UnchangedSince(base, 64, beforeCpu), "a CPU write after the driver store is not seen");
}

void CheckUnwatch() {
    void* memory = AllocateWatched(3 * Block);
    const auto base = reinterpret_cast<std::uint64_t>(memory);
    const auto imported = base + Block;
    std::memset(memory, 0x11, 3 * Block);
    BumpCollectEpoch();
    const auto before = CollectWrites(base, 3 * Block);
    Require(before != 0 && UnchangedSince(imported, Block, before), "the import range was not initially watched");
    Unwatch(imported, Block);
    Require(!Watched(imported, Block) && !UnchangedSince(imported, Block, before), "unwatch kept trusting import stamps");
    Require(CollectWrites(base, 3 * Block) == 0 && CollectWritesUncached(imported, Block) == 0, "unwatch did not invalidate a cached collect");
    Require(Watched(base, Block) && Watched(base + 2 * Block, Block), "unwatch disabled unrelated memory");
    Require(MarkWritten(imported, Block) == 0, "an unwatched import still produces trusted stamps");
    std::array<std::uint64_t, 1> generations{before};
    std::array<std::uint8_t, 1> changed{};
    Require(!ChangedBlocks(imported, Block, generations, changed), "an unwatched import still uses block stamps");
    Unwatch(imported, Block);
    const auto adjacent = CollectWrites(base, Block);
    Require(adjacent != 0, "unwatch broke the adjacent block's collect");
    static_cast<volatile std::uint8_t*>(memory)[0] = 0x33;
    CollectWritesUncached(base, Block);
    Require(!UnchangedSince(base, Block, adjacent), "unwatch hid a CPU edit in unrelated memory");
}

#ifdef _WIN32
void CheckPrivateMappingReuse() {
    void* memory = AllocateWatched(3 * Block);
    const auto base = reinterpret_cast<std::uint64_t>(memory);
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
    GuestArena::GuestArenaReset_nid_postfix(memory, 3 * Block);
    GuestArena::GuestArenaRelease_nid_postfix(memory, 3 * Block);
    auto* replacement = GuestArena::GuestArenaAllocateAtOrAbove_nid_postfix(base, 3 * Block, Block);
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
    GuestArena::GuestArenaReset_nid_postfix(replacement, 3 * Block);
    GuestArena::GuestArenaRelease_nid_postfix(replacement, 3 * Block);
}
#endif
}

int main() {
    try {
        if (!WriteWatched()) {
            std::cout << "no write watching: skipped\n";
            return 77;
        }
        CheckSharedBlock();
        CheckOwnStore();
        CheckUnwatch();
#ifdef _WIN32
        CheckPrivateMappingReuse();
#endif
    } catch (const std::exception& error) {
        std::cerr << "write tracking test failed: " << error.what() << "\n";
        return 1;
    }
    std::cout << "write tracking tests passed\n";
    return 0;
}
