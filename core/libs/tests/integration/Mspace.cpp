#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <thread>

extern "C" {
int* APS5_VABI __error_nid_postfix();
void* APS5_VABI sceLibcMspaceCreate_nid_postfix(const char*, void*, std::size_t, unsigned);
int APS5_VABI sceLibcMspaceDestroy_nid_postfix(void*);
void* APS5_VABI sceLibcMspaceMalloc_nid_postfix(void*, std::size_t);
void* APS5_VABI sceLibcMspaceCalloc_nid_postfix(void*, std::size_t, std::size_t);
void* APS5_VABI sceLibcMspaceRealloc_nid_postfix(void*, void*, std::size_t);
void APS5_VABI sceLibcMspaceFree_nid_postfix(void*, void*);
int APS5_VABI sceLibcMspacePosixMemalign_nid_postfix(void*, void**, std::size_t, std::size_t);
std::size_t APS5_VABI sceLibcMspaceMallocUsableSize_nid_postfix(const void*);
void* APS5_VABI sceLibcMspaceMemalign_nid_postfix(void*, std::size_t, std::size_t);
void* APS5_VABI sceLibcMspaceReallocalign_nid_postfix(void*, void*, std::size_t, std::size_t);
int APS5_VABI sceLibcMspaceMallocStats_nid_postfix(void*, void*);
int APS5_VABI sceLibcMspaceMallocStatsFast_nid_postfix(void*, void*);
}

struct MallocManagedSize {
    std::uint16_t size;
    std::uint16_t version;
    std::uint32_t reserved;
    std::size_t maxSystemSize;
    std::size_t currentSystemSize;
    std::size_t maxInuseSize;
    std::size_t currentInuseSize;
};

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::size_t storageBytes = 65536;
constexpr std::size_t largeBytes = storageBytes - 256;
constexpr int einval = 22;
constexpr int enomem = 12;

class ArenaFixture {
public:
    ArenaFixture() : handle(sceLibcMspaceCreate_nid_postfix("test", storage.data(), storage.size(), 0)) {
        Require(handle != nullptr, "create the arena");
    }

    ~ArenaFixture() {
        sceLibcMspaceDestroy_nid_postfix(handle);
    }

    ArenaFixture(const ArenaFixture&) = delete;
    ArenaFixture& operator=(const ArenaFixture&) = delete;

    alignas(16) std::array<unsigned char, storageBytes> storage{};
    void* const handle;
};

class NestedArena {
public:
    explicit NestedArena(void* handle) : handle(handle) {}

    ~NestedArena() {
        if (handle != nullptr) sceLibcMspaceDestroy_nid_postfix(handle);
    }

    NestedArena(const NestedArena&) = delete;
    NestedArena& operator=(const NestedArena&) = delete;

    void* Get() const { return handle; }

    int Destroy() {
        const int result = sceLibcMspaceDestroy_nid_postfix(handle);
        handle = nullptr;
        return result;
    }

private:
    void* handle;
};

MallocManagedSize Stats(void* arena) {
    MallocManagedSize stats{sizeof(MallocManagedSize), 1, 0, 0, 0, 0, 0};
    RequireEqual(sceLibcMspaceMallocStats_nid_postfix(arena, &stats), 0, "malloc stats");
    return stats;
}

std::uintptr_t Address(const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer);
}

struct FilledBlock {
    unsigned char* first;
    void* blocker;
};

FilledBlock AllocateFilledBlock(ArenaFixture& arena) {
    auto* first = static_cast<unsigned char*>(sceLibcMspaceCalloc_nid_postfix(arena.handle, 32, 4));
    Require(first != nullptr, "calloc 32 x 4");
    for (int index = 0; index < 128; ++index) first[index] = static_cast<unsigned char>(index);
    void* blocker = sceLibcMspaceMalloc_nid_postfix(arena.handle, 128);
    Require(blocker != nullptr, "allocate the blocking neighbour");
    return {first, blocker};
}

void RequireSequence(const unsigned char* block, int count, int offset, const char* message) {
    for (int index = 0; index < count; ++index) {
        RequireEqual(block[index], static_cast<unsigned char>(index + offset), std::string(message) + " byte " + std::to_string(index));
    }
}

void RequireUnchangedAfterOverflow(void* arena, const unsigned char* first, const MallocManagedSize& before, const std::string& input) {
    RequireEqual(*__error_nid_postfix(), enomem, input + " errno");
    RequireEqual(sceLibcMspaceMallocUsableSize_nid_postfix(first), std::size_t{128}, input + " usable size");
    RequireSequence(first, 128, 0, input.c_str());
    const MallocManagedSize after = Stats(arena);
    RequireEqual(after.currentInuseSize, before.currentInuseSize, input + " current in-use size");
    RequireEqual(after.maxInuseSize, before.maxInuseSize, input + " peak in-use size");
}

unsigned char* GrowPastBlocker(ArenaFixture& arena) {
    const FilledBlock block = AllocateFilledBlock(arena);
    auto* grown = static_cast<unsigned char*>(sceLibcMspaceRealloc_nid_postfix(arena.handle, block.first, 4096));
    Require(grown != nullptr, "realloc to 4096");
    return grown;
}

const Case createArena{"MspaceCreate_AlignedStorage_ReturnsArena", [] {
    ArenaFixture arena;
    Require(arena.handle != nullptr, "arena handle");
}};

const Case overlap{"MspaceCreate_StorageOfLiveArena_Fails", [] {
    ArenaFixture arena;
    Require(sceLibcMspaceCreate_nid_postfix("overlap", arena.storage.data(), arena.storage.size(), 0) == nullptr, "overlapping create");
}};

const Case callocZeroed{"MspaceCalloc_SmallBlock_ReturnsZeroedMemoryInsideStorage", [] {
    ArenaFixture arena;
    auto* first = static_cast<unsigned char*>(sceLibcMspaceCalloc_nid_postfix(arena.handle, 32, 4));
    Require(first != nullptr, "calloc 32 x 4");
    Require(first > arena.storage.data() && first + 128 <= arena.storage.data() + arena.storage.size(), "block inside storage");
    for (int index = 0; index < 128; ++index) RequireEqual(first[index], static_cast<unsigned char>(0), "byte " + std::to_string(index));
}};

const Case reallocOverflow{"MspaceRealloc_SizeNearMaximum_FailsWithEnomemAndKeepsBlock", [] {
    ArenaFixture arena;
    const FilledBlock block = AllocateFilledBlock(arena);
    const MallocManagedSize before = Stats(arena.handle);
    for (std::size_t delta = 0; delta < 16; ++delta) {
        const std::string input = "SIZE_MAX - " + std::to_string(delta);
        *__error_nid_postfix() = 0;
        Require(sceLibcMspaceRealloc_nid_postfix(arena.handle, block.first, std::numeric_limits<std::size_t>::max() - delta) == nullptr,
            input + " result");
        RequireUnchangedAfterOverflow(arena.handle, block.first, before, input);
    }
}};

const Case reallocalignOverflow{"MspaceReallocalign_SizeNearMaximum_FailsWithEnomemAndKeepsBlock", [] {
    ArenaFixture arena;
    const FilledBlock block = AllocateFilledBlock(arena);
    const MallocManagedSize before = Stats(arena.handle);
    for (std::size_t delta = 0; delta < 16; ++delta) {
        const std::string input = "SIZE_MAX - " + std::to_string(delta);
        *__error_nid_postfix() = 0;
        Require(sceLibcMspaceReallocalign_nid_postfix(arena.handle, block.first, std::numeric_limits<std::size_t>::max() - delta, 16) == nullptr,
            input + " result");
        RequireUnchangedAfterOverflow(arena.handle, block.first, before, input);
    }
}};

const Case reallocMoves{"MspaceRealloc_GrowWithUsedNeighbour_MovesAndKeepsContents", [] {
    ArenaFixture arena;
    const FilledBlock block = AllocateFilledBlock(arena);
    auto* grown = static_cast<unsigned char*>(sceLibcMspaceRealloc_nid_postfix(arena.handle, block.first, 4096));
    Require(grown != nullptr, "realloc to 4096");
    Require(grown != block.first, "block moved");
    RequireSequence(grown, 128, 0, "grown");
    Require(sceLibcMspaceMallocUsableSize_nid_postfix(grown) >= 4096, "usable size at least 4096");
}};

const Case reallocTooLarge{"MspaceRealloc_LargerThanArena_FailsAndKeepsBlock", [] {
    ArenaFixture arena;
    unsigned char* grown = GrowPastBlocker(arena);
    Require(sceLibcMspaceRealloc_nid_postfix(arena.handle, grown, arena.storage.size()) == nullptr, "realloc to the storage size");
    RequireEqual(grown[127], static_cast<unsigned char>(127), "last original byte");
}};

const Case posixMemalign{"MspacePosixMemalign_PageAlignment_ReturnsAlignedBlock", [] {
    ArenaFixture arena;
    GrowPastBlocker(arena);
    void* aligned = nullptr;
    RequireEqual(sceLibcMspacePosixMemalign_nid_postfix(arena.handle, &aligned, 4096, 1024), 0, "posix_memalign");
    RequireEqual(Address(aligned) & 4095, std::uintptr_t{0}, "alignment");
}};

const Case posixMemalignInvalid{"MspacePosixMemalign_AlignmentBelowPointerSize_ReturnsEinvalAndKeepsOutput", [] {
    ArenaFixture arena;
    void* aligned = nullptr;
    RequireEqual(sceLibcMspacePosixMemalign_nid_postfix(arena.handle, &aligned, 4096, 1024), 0, "posix_memalign");
    void* unchanged = aligned;
    RequireEqual(sceLibcMspacePosixMemalign_nid_postfix(arena.handle, &unchanged, 3, 8), einval, "posix_memalign alignment 3");
    Require(unchanged == aligned, "output pointer untouched");
}};

const Case callocOverflow{"MspaceCalloc_OverflowingProduct_ReturnsNull", [] {
    ArenaFixture arena;
    Require(sceLibcMspaceCalloc_nid_postfix(arena.handle, std::numeric_limits<std::size_t>::max(), 2) == nullptr, "calloc SIZE_MAX x 2");
}};

const Case coalesce{"MspaceFree_FragmentedBlocks_CoalesceForLargeAllocation", [] {
    ArenaFixture arena;
    const FilledBlock block = AllocateFilledBlock(arena);
    void* grown = sceLibcMspaceRealloc_nid_postfix(arena.handle, block.first, 4096);
    Require(grown != nullptr, "realloc to 4096");
    void* aligned = nullptr;
    RequireEqual(sceLibcMspacePosixMemalign_nid_postfix(arena.handle, &aligned, 4096, 1024), 0, "posix_memalign");
    sceLibcMspaceFree_nid_postfix(arena.handle, block.blocker);
    sceLibcMspaceFree_nid_postfix(arena.handle, aligned);
    Require(sceLibcMspaceRealloc_nid_postfix(arena.handle, grown, 0) == nullptr, "realloc to zero frees and returns null");
    void* large = sceLibcMspaceMalloc_nid_postfix(arena.handle, largeBytes);
    Require(large != nullptr, "large allocation after freeing everything");
}};

const Case reallocInPlace{"MspaceRealloc_GrowIntoFreeNeighbour_KeepsAddress", [] {
    ArenaFixture arena;
    void* head = sceLibcMspaceMalloc_nid_postfix(arena.handle, 64);
    Require(head != nullptr, "malloc 64");
    Require(sceLibcMspaceRealloc_nid_postfix(arena.handle, head, 1024) == head, "realloc in place");
    RequireEqual(sceLibcMspaceMallocUsableSize_nid_postfix(head), std::size_t{1024}, "usable size");
}};

std::array<void*, 2000> AllocateSmallBlocks(ArenaFixture& arena) {
    std::array<void*, 2000> small{};
    for (std::size_t index = 0; index < small.size(); ++index) {
        small[index] = sceLibcMspaceMalloc_nid_postfix(arena.handle, 16);
        Require(small[index] != nullptr, "small block " + std::to_string(index));
    }
    for (std::size_t index = 0; index < small.size(); index += 2) sceLibcMspaceFree_nid_postfix(arena.handle, small[index]);
    return small;
}

const Case fragmented{"MspaceMalloc_HalfOfSmallBlocksFreed_CannotAllocateLargeBlock", [] {
    ArenaFixture arena;
    AllocateSmallBlocks(arena);
    Require(sceLibcMspaceMalloc_nid_postfix(arena.handle, largeBytes) == nullptr, "large allocation while fragmented");
}};

const Case defragmented{"MspaceMalloc_AllSmallBlocksFreed_AllocatesLargeBlock", [] {
    ArenaFixture arena;
    const auto small = AllocateSmallBlocks(arena);
    Require(sceLibcMspaceMalloc_nid_postfix(arena.handle, largeBytes) == nullptr, "large allocation while fragmented");
    for (std::size_t index = 1; index < small.size(); index += 2) sceLibcMspaceFree_nid_postfix(arena.handle, small[index]);
    Require(sceLibcMspaceMalloc_nid_postfix(arena.handle, largeBytes) != nullptr, "large allocation after freeing everything");
}};

const Case concurrent{"MspaceMalloc_ConcurrentThreads_AllocateWriteAndFree", [] {
    ArenaFixture arena;
    std::atomic<int> failedAllocations{0};
    std::atomic<int> smallUsableSizes{0};
    std::array<std::thread, 4> workers;
    for (auto& worker : workers) worker = std::thread([&] {
        for (int iteration = 0; iteration < 1000; ++iteration) {
            void* pointer = sceLibcMspaceMalloc_nid_postfix(arena.handle, 97);
            if (pointer == nullptr) {
                ++failedAllocations;
                continue;
            }
            std::memset(pointer, 42, 97);
            if (sceLibcMspaceMallocUsableSize_nid_postfix(pointer) < 97) ++smallUsableSizes;
            sceLibcMspaceFree_nid_postfix(arena.handle, pointer);
        }
    });
    for (auto& worker : workers) worker.join();
    RequireEqual(failedAllocations.load(), 0, "failed allocations");
    RequireEqual(smallUsableSizes.load(), 0, "usable sizes below 97");
    RequireEqual(Stats(arena.handle).currentInuseSize, std::size_t{0}, "current in-use size after the threads");
}};

const Case statsAfterLarge{"MspaceMallocStats_AfterLargeAllocationFreed_ReportsSystemAndPeakSizes", [] {
    ArenaFixture arena;
    void* large = sceLibcMspaceMalloc_nid_postfix(arena.handle, largeBytes);
    Require(large != nullptr, "large allocation");
    sceLibcMspaceFree_nid_postfix(arena.handle, large);
    const MallocManagedSize result = Stats(arena.handle);
    RequireEqual(result.currentSystemSize, arena.storage.size(), "current system size");
    RequireEqual(result.maxSystemSize, arena.storage.size(), "peak system size");
    RequireEqual(result.currentInuseSize, std::size_t{0}, "current in-use size");
    Require(result.maxInuseSize >= largeBytes, "peak in-use size covers the large allocation: " + std::to_string(result.maxInuseSize));
}};

const Case statsShort{"MspaceMallocStatsFast_ShortStructure_ReturnsEinval", [] {
    ArenaFixture arena;
    MallocManagedSize shortStats{8, 1, 0, 0, 0, 0, 0};
    RequireEqual(sceLibcMspaceMallocStatsFast_nid_postfix(arena.handle, &shortStats), einval, "stats fast");
}};

const Case memalignAligned{"MspaceMemalign_PowerOfTwoAlignment_ReturnsAlignedBlock", [] {
    ArenaFixture arena;
    void* memaligned = sceLibcMspaceMemalign_nid_postfix(arena.handle, 256, 100);
    Require(memaligned != nullptr, "memalign 256");
    RequireEqual(Address(memaligned) & 255, std::uintptr_t{0}, "alignment");
}};

const Case memalignInvalid{"MspaceMemalign_NonPowerOfTwoAlignment_ReturnsNull", [] {
    ArenaFixture arena;
    Require(sceLibcMspaceMemalign_nid_postfix(arena.handle, 24, 100) == nullptr, "memalign 24");
}};

const Case reallocalignNull{"MspaceReallocalign_NullPointer_AllocatesAlignedBlock", [] {
    ArenaFixture arena;
    void* realigned = sceLibcMspaceReallocalign_nid_postfix(arena.handle, nullptr, 64, 64);
    Require(realigned != nullptr, "reallocalign null");
    RequireEqual(Address(realigned) & 63, std::uintptr_t{0}, "alignment");
}};

unsigned char* AllocateRealigned(ArenaFixture& arena) {
    auto* realigned = static_cast<unsigned char*>(sceLibcMspaceReallocalign_nid_postfix(arena.handle, nullptr, 64, 64));
    Require(realigned != nullptr, "reallocalign null");
    for (int index = 0; index < 64; ++index) realigned[index] = static_cast<unsigned char>(index + 1);
    return realigned;
}

const Case reallocalignGrow{"MspaceReallocalign_Grow_KeepsAlignmentAndContents", [] {
    ArenaFixture arena;
    unsigned char* realigned = AllocateRealigned(arena);
    auto* regrown = static_cast<unsigned char*>(sceLibcMspaceReallocalign_nid_postfix(arena.handle, realigned, 256, 64));
    Require(regrown != nullptr, "reallocalign 256");
    RequireEqual(Address(regrown) & 63, std::uintptr_t{0}, "alignment");
    RequireSequence(regrown, 64, 1, "regrown");
}};

const Case reallocalignInvalid{"MspaceReallocalign_InvalidAlignment_ReturnsNull", [] {
    ArenaFixture arena;
    unsigned char* realigned = AllocateRealigned(arena);
    for (const std::size_t alignment : {std::size_t{0}, std::size_t{3}}) {
        Require(sceLibcMspaceReallocalign_nid_postfix(arena.handle, realigned, 16, alignment) == nullptr,
            "alignment " + std::to_string(alignment));
    }
}};

const Case reallocalignZero{"MspaceReallocalign_ZeroSize_FreesAndReturnsNull", [] {
    ArenaFixture arena;
    unsigned char* realigned = AllocateRealigned(arena);
    Require(sceLibcMspaceReallocalign_nid_postfix(arena.handle, realigned, 0, 16) == nullptr, "reallocalign to zero");
    RequireEqual(Stats(arena.handle).currentInuseSize, std::size_t{0}, "block released");
}};

const Case statsFast{"MspaceMallocStatsFast_LiveAllocation_ReportsInUseSize", [] {
    ArenaFixture arena;
    void* memaligned = sceLibcMspaceMemalign_nid_postfix(arena.handle, 256, 100);
    Require(memaligned != nullptr, "memalign 256");
    MallocManagedSize result{sizeof(MallocManagedSize), 1, 0, 0, 0, 0, 0};
    RequireEqual(sceLibcMspaceMallocStatsFast_nid_postfix(arena.handle, &result), 0, "stats fast");
    Require(result.currentInuseSize >= 100, "current in-use size covers the allocation: " + std::to_string(result.currentInuseSize));
}};

const Case createOutside{"MspaceCreate_FreeSpaceOfLiveArena_Fails", [] {
    ArenaFixture arena;
    auto* region = static_cast<unsigned char*>(sceLibcMspaceMalloc_nid_postfix(arena.handle, 8192));
    Require(region != nullptr, "allocate the region");
    Require(sceLibcMspaceCreate_nid_postfix("outside", region + 8192, 4096, 0) == nullptr, "create past the region");
}};

const Case nested{"MspaceCreate_InsideAllocation_CreatesNestedArena", [] {
    ArenaFixture arena;
    auto* region = static_cast<unsigned char*>(sceLibcMspaceMalloc_nid_postfix(arena.handle, 8192));
    Require(region != nullptr, "allocate the region");
    NestedArena inner(sceLibcMspaceCreate_nid_postfix("nested", region, 8192, 1));
    Require(inner.Get() == region, "nested arena handle");
    auto* allocation = static_cast<unsigned char*>(sceLibcMspaceMalloc_nid_postfix(inner.Get(), 64));
    Require(allocation > region && allocation < region + 8192, "nested allocation inside the region");
    RequireEqual(sceLibcMspaceMallocUsableSize_nid_postfix(allocation), std::size_t{64}, "nested allocation usable size");
    RequireEqual(sceLibcMspaceMallocUsableSize_nid_postfix(region), std::size_t{8192}, "region usable size");
}};

const Case unknownFlags{"MspaceCreate_UnknownFlags_Fails", [] {
    ArenaFixture arena;
    Require(sceLibcMspaceCreate_nid_postfix("flags", arena.storage.data(), arena.storage.size(), 2) == nullptr, "create with flag 2");
}};

const Case destroyNested{"MspaceDestroy_NestedArena_Succeeds", [] {
    ArenaFixture arena;
    void* region = sceLibcMspaceMalloc_nid_postfix(arena.handle, 8192);
    Require(region != nullptr, "allocate the region");
    NestedArena inner(sceLibcMspaceCreate_nid_postfix("nested", region, 8192, 1));
    Require(inner.Get() == region, "nested arena handle");
    RequireEqual(inner.Destroy(), 0, "destroy the nested arena");
}};

const Case destroyArena{"MspaceDestroy_LiveArena_SucceedsAndRejectsLaterAllocations", [] {
    ArenaFixture arena;
    RequireEqual(sceLibcMspaceDestroy_nid_postfix(arena.handle), 0, "destroy");
    Require(sceLibcMspaceMalloc_nid_postfix(arena.handle, 8) == nullptr, "malloc after destroy");
}};

const Case reuse{"MspaceCreate_StorageOfDestroyedArena_ReturnsSameHandle", [] {
    ArenaFixture arena;
    RequireEqual(sceLibcMspaceDestroy_nid_postfix(arena.handle), 0, "destroy");
    Require(sceLibcMspaceCreate_nid_postfix("reuse", arena.storage.data(), arena.storage.size(), 0) == arena.handle, "recreate");
    RequireEqual(sceLibcMspaceDestroy_nid_postfix(arena.handle), 0, "destroy the recreated arena");
}};

} // namespace
