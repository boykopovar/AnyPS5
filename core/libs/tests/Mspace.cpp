#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
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
void* APS5_VABI sceLibcMspaceAlignedAlloc_nid_postfix(void*, std::size_t, std::size_t);
void* APS5_VABI sceLibcMspaceReallocalign_nid_postfix(void*, void*, std::size_t, std::size_t);
int APS5_VABI sceLibcMspaceMallocStats_nid_postfix(void*, void*);
int APS5_VABI sceLibcMspaceMallocStatsFast_nid_postfix(void*, void*);
int APS5_VABI sceLibcMspaceIsHeapEmpty_nid_postfix(void*);
int APS5_VABI sceLibcPafMspaceIsHeapEmpty_nid_postfix(void*);
int APS5_VABI sceLibcMspaceIsHeapEmpty_nid_postfix(void*);
int APS5_VABI sceLibcPafMspaceIsHeapEmpty_nid_postfix(void*);
void* APS5_VABI sceLibcPafMspaceCreate_nid_postfix(const char*, void*, std::size_t, unsigned, const void*);
int APS5_VABI sceLibcPafMspaceDestroy_nid_postfix(void*);
void* APS5_VABI sceLibcPafMspaceMalloc_nid_postfix(void*, std::size_t, const void*);
void APS5_VABI sceLibcPafMspaceFree_nid_postfix(void*, void*, const void*);
void* APS5_VABI sceLibcPafMspaceCalloc_nid_postfix(void*, std::size_t, std::size_t, const void*);
void* APS5_VABI sceLibcPafMspaceRealloc_nid_postfix(void*, void*, std::size_t, const void*);
void* APS5_VABI sceLibcPafMspaceReallocalign_nid_postfix(void*, void*, std::size_t, std::size_t, const void*);
void* APS5_VABI sceLibcPafMspaceMemalign_nid_postfix(void*, std::size_t, std::size_t, const void*);
int APS5_VABI sceLibcPafMspacePosixMemalign_nid_postfix(void*, void**, std::size_t, std::size_t, const void*);
int APS5_VABI sceLibcPafMspaceMallocStats_nid_postfix(void*, void*);
int APS5_VABI sceLibcPafMspaceMallocStatsFast_nid_postfix(void*, void*);
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
static void Require(bool value) {
    if (!value) { std::fputs("Mspace check failed\n", stderr); std::abort(); }
}
static void CheckPaf() {
    alignas(64) static std::array<unsigned char, 65536> storage{};
    bool rejected = false;
    int options = 0;
    try { sceLibcPafMspaceCreate_nid_postfix("paf", storage.data(), storage.size(), 0, &options); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected);
    void* arena = sceLibcPafMspaceCreate_nid_postfix("paf", storage.data(), storage.size(), 0, nullptr);
    Require(arena == storage.data());
    const int owner = 0;
    auto* block = static_cast<unsigned char*>(sceLibcPafMspaceMalloc_nid_postfix(arena, 40, &owner));
    Require(block != nullptr && sceLibcMspaceMallocUsableSize_nid_postfix(block) >= 40);
    std::memset(block, 0x5a, 40);
    auto* zeroed = static_cast<unsigned char*>(sceLibcPafMspaceCalloc_nid_postfix(arena, 4, 8, arena));
    Require(zeroed != nullptr && zeroed[0] == 0 && zeroed[31] == 0);
    block = static_cast<unsigned char*>(sceLibcPafMspaceRealloc_nid_postfix(arena, block, 4000, &owner));
    Require(block != nullptr && block[0] == 0x5a && block[39] == 0x5a);
    block = static_cast<unsigned char*>(sceLibcPafMspaceReallocalign_nid_postfix(arena, block, 256, 64, &owner));
    Require(block != nullptr && reinterpret_cast<std::uintptr_t>(block) % 256 == 0 && block[39] == 0x5a);
    auto* aligned = static_cast<unsigned char*>(sceLibcPafMspaceMemalign_nid_postfix(arena, 128, 50, &owner));
    Require(aligned != nullptr && reinterpret_cast<std::uintptr_t>(aligned) % 128 == 0);
    Require(sceLibcPafMspaceMemalign_nid_postfix(arena, 0, 50, &owner) == nullptr && *__error_nid_postfix() == 22);
    Require(sceLibcPafMspaceMemalign_nid_postfix(arena, 15, 50, &owner) == nullptr && *__error_nid_postfix() == 22);
    void* posixAligned = nullptr;
    Require(sceLibcPafMspacePosixMemalign_nid_postfix(arena, &posixAligned, 256, 60, &owner) == 0);
    Require(posixAligned != nullptr && reinterpret_cast<std::uintptr_t>(posixAligned) % 256 == 0);
    Require(sceLibcPafMspacePosixMemalign_nid_postfix(arena, nullptr, 256, 60, &owner) == 22);
    Require(sceLibcPafMspacePosixMemalign_nid_postfix(arena, &posixAligned, 15, 60, &owner) == 22);
    MallocManagedSize viaPaf{}, viaMspace{}, fastPaf{}, fastMspace{};
    viaPaf.size = viaMspace.size = fastPaf.size = fastMspace.size = sizeof(MallocManagedSize);
    viaPaf.version = viaMspace.version = fastPaf.version = fastMspace.version = 1;
    Require(sceLibcPafMspaceMallocStats_nid_postfix(arena, &viaPaf) == sceLibcMspaceMallocStats_nid_postfix(arena, &viaMspace));
    Require(sceLibcPafMspaceMallocStatsFast_nid_postfix(arena, &fastPaf) == sceLibcMspaceMallocStatsFast_nid_postfix(arena, &fastMspace));
    Require(viaPaf.currentInuseSize == viaMspace.currentInuseSize && viaPaf.currentInuseSize > 0);
    Require(fastPaf.currentInuseSize == fastMspace.currentInuseSize);
    sceLibcPafMspaceFree_nid_postfix(arena, block, &owner);
    sceLibcPafMspaceFree_nid_postfix(arena, zeroed, arena);
    sceLibcPafMspaceFree_nid_postfix(arena, aligned, &owner);
    sceLibcPafMspaceFree_nid_postfix(arena, posixAligned, &owner);
    sceLibcPafMspaceFree_nid_postfix(arena, nullptr, &owner);
    Require(sceLibcPafMspaceDestroy_nid_postfix(arena) == 0);
    Require(sceLibcPafMspaceDestroy_nid_postfix(arena) == -1);
}
static void CheckAlignment() {
    alignas(32) static std::array<unsigned char, 65536 + 16> storage{};
    void* arena = sceLibcMspaceCreate_nid_postfix("aligned", storage.data() + 16, 65536, 0);
    Require(arena != nullptr);
    const auto aligned = [](const void* pointer) { return pointer && (reinterpret_cast<std::uintptr_t>(pointer) & 31u) == 0; };
    void* blocks[64] = {};
    for (std::size_t index = 0; index < 64; ++index) {
        blocks[index] = sceLibcMspaceMalloc_nid_postfix(arena, index + 1);
        Require(aligned(blocks[index]));
        std::memset(blocks[index], 0xa5, index + 1);
    }
    void* zeroed = sceLibcMspaceCalloc_nid_postfix(arena, 3, 13);
    Require(aligned(zeroed));
    blocks[0] = sceLibcMspaceRealloc_nid_postfix(arena, blocks[0], 600);
    Require(aligned(blocks[0]));
    void* small = sceLibcMspaceMemalign_nid_postfix(arena, 16, 40);
    Require(aligned(small));
    void* moved = sceLibcMspaceReallocalign_nid_postfix(arena, nullptr, 8, 40);
    Require(aligned(moved));
    void* posix = nullptr;
    Require(sceLibcMspacePosixMemalign_nid_postfix(arena, &posix, 8, 40) == 0 && aligned(posix));
    for (void* block : blocks) sceLibcMspaceFree_nid_postfix(arena, block);
    for (void* block : {zeroed, small, moved, posix}) sceLibcMspaceFree_nid_postfix(arena, block);
    Require(sceLibcMspaceDestroy_nid_postfix(arena) == 0);
}
static void CheckAdjacentBlocks() {
    alignas(32) static std::array<unsigned char, 65536> storage{};
    void* arena = sceLibcMspaceCreate_nid_postfix("adjacent", storage.data(), storage.size(), 0);
    Require(arena != nullptr);
    const auto end = [](void* block) { return static_cast<unsigned char*>(block) + sceLibcMspaceMallocUsableSize_nid_postfix(block); };
    std::array<void*, 96> blocks{};
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        blocks[index] = sceLibcMspaceMalloc_nid_postfix(arena, index % 48 + 1);
        Require(blocks[index] != nullptr);
        if (index > 0) Require(blocks[index] == end(blocks[index - 1]));
    }
    void* last = blocks.back();
    Require(sceLibcMspaceRealloc_nid_postfix(arena, last, 80) == last);
    void* after = sceLibcMspaceMalloc_nid_postfix(arena, 16);
    Require(after == end(last));
    MallocManagedSize stats{sizeof(MallocManagedSize), 1, 0, 0, 0, 0, 0};
    Require(sceLibcMspaceMallocStats_nid_postfix(arena, &stats) == 0);
    Require(stats.currentInuseSize == static_cast<std::size_t>(end(after) - static_cast<unsigned char*>(blocks[0])));
    Require(sceLibcMspaceDestroy_nid_postfix(arena) == 0);
}
static void CheckMovedBlockKeepsUsableBytes() {
    alignas(32) static std::array<unsigned char, 65536> storage{};
    void* arena = sceLibcMspaceCreate_nid_postfix("moved", storage.data(), storage.size(), 0);
    Require(arena != nullptr);
    const auto fill = [](void* block) {
        const auto usable = sceLibcMspaceMallocUsableSize_nid_postfix(block);
        Require(usable > 1);
        for (std::size_t index = 0; index < usable; ++index) static_cast<unsigned char*>(block)[index] = static_cast<unsigned char>(index + 1);
        return usable;
    };
    const auto kept = [](const void* block, std::size_t usable) {
        for (std::size_t index = 0; index < usable; ++index) if (static_cast<const unsigned char*>(block)[index] != static_cast<unsigned char>(index + 1)) return false;
        return true;
    };
    void* block = sceLibcMspaceMalloc_nid_postfix(arena, 1);
    const auto usable = fill(block);
    Require(sceLibcMspaceMalloc_nid_postfix(arena, 1) != nullptr);
    void* moved = sceLibcMspaceRealloc_nid_postfix(arena, block, 4096);
    Require(moved != nullptr && moved != block && kept(moved, usable));
    void* other = sceLibcMspaceMalloc_nid_postfix(arena, 1);
    const auto otherUsable = fill(other);
    Require(sceLibcMspaceMalloc_nid_postfix(arena, 1) != nullptr);
    void* realigned = sceLibcMspaceReallocalign_nid_postfix(arena, other, 256, 4096);
    Require(realigned != nullptr && realigned != other && kept(realigned, otherUsable));
    Require(sceLibcMspaceDestroy_nid_postfix(arena) == 0);
}
static void CheckHeapEmpty() {
    alignas(32) static std::array<unsigned char, 4096> storage{};
    void* arena = sceLibcMspaceCreate_nid_postfix("empty", storage.data(), storage.size(), 0);
    Require(arena != nullptr && sceLibcMspaceIsHeapEmpty_nid_postfix(arena) == 1);
    Require(sceLibcPafMspaceIsHeapEmpty_nid_postfix(arena) == 1);
    const std::array<void*, 2> invalidHandles{nullptr, reinterpret_cast<void*>(std::uintptr_t{1})};
    for (void* invalid : invalidHandles) {
        *__error_nid_postfix() = 0;
        bool rejected = false;
        try { sceLibcMspaceIsHeapEmpty_nid_postfix(invalid); }
        catch (const std::runtime_error&) { rejected = true; }
        Require(rejected && *__error_nid_postfix() == 22);
    }
    void* first = sceLibcMspaceMalloc_nid_postfix(arena, 16);
    void* second = sceLibcMspaceCalloc_nid_postfix(arena, 4, 8);
    Require(first != nullptr && second != nullptr && sceLibcMspaceIsHeapEmpty_nid_postfix(arena) == 0);
    Require(sceLibcPafMspaceIsHeapEmpty_nid_postfix(arena) == 0);
    sceLibcMspaceFree_nid_postfix(arena, first);
    Require(sceLibcMspaceIsHeapEmpty_nid_postfix(arena) == 0);
    Require(sceLibcMspaceRealloc_nid_postfix(arena, second, 0) == nullptr);
    Require(sceLibcMspaceIsHeapEmpty_nid_postfix(arena) == 1);
    Require(sceLibcPafMspaceIsHeapEmpty_nid_postfix(arena) == 1);
    Require(sceLibcMspaceDestroy_nid_postfix(arena) == 0);
}
int main() {
    CheckPaf();
    CheckAlignment();
    CheckAdjacentBlocks();
    CheckMovedBlockKeepsUsableBytes();
    CheckHeapEmpty();
    alignas(16) std::array<unsigned char, 65536> storage{};
    void* arena = sceLibcMspaceCreate_nid_postfix("test", storage.data(), storage.size(), 0);
    Require(arena != nullptr);
    Require(sceLibcMspaceCreate_nid_postfix("overlap", storage.data(), storage.size(), 0) == nullptr);
    auto* first = static_cast<unsigned char*>(sceLibcMspaceCalloc_nid_postfix(arena, 32, 4));
    Require(first > storage.data() && first + 128 <= storage.data() + storage.size());
    for (int i = 0; i < 128; ++i) { Require(first[i] == 0); first[i] = static_cast<unsigned char>(i); }
    void* blocker = sceLibcMspaceMalloc_nid_postfix(arena, 128);
    MallocManagedSize beforeOverflow{sizeof(MallocManagedSize), 1, 0, 0, 0, 0, 0};
    Require(sceLibcMspaceMallocStats_nid_postfix(arena, &beforeOverflow) == 0);
    for (std::size_t delta = 0; delta < 16; ++delta) {
        const auto size = std::numeric_limits<std::size_t>::max() - delta;
        *__error_nid_postfix() = 0;
        Require(sceLibcMspaceRealloc_nid_postfix(arena, first, size) == nullptr);
        Require(*__error_nid_postfix() == 12);
        *__error_nid_postfix() = 0;
        Require(sceLibcMspaceReallocalign_nid_postfix(arena, first, 16, size) == nullptr);
        Require(*__error_nid_postfix() == 12);
        Require(sceLibcMspaceMallocUsableSize_nid_postfix(first) == 128);
        for (int i = 0; i < 128; ++i) Require(first[i] == i);
        MallocManagedSize afterOverflow{sizeof(MallocManagedSize), 1, 0, 0, 0, 0, 0};
        Require(sceLibcMspaceMallocStats_nid_postfix(arena, &afterOverflow) == 0);
        Require(afterOverflow.currentInuseSize == beforeOverflow.currentInuseSize);
        Require(afterOverflow.maxInuseSize == beforeOverflow.maxInuseSize);
    }
    auto* grown = static_cast<unsigned char*>(sceLibcMspaceRealloc_nid_postfix(arena, first, 4096));
    Require(grown && grown != first);
    for (int i = 0; i < 128; ++i) Require(grown[i] == i);
    Require(sceLibcMspaceMallocUsableSize_nid_postfix(grown) >= 4096);
    Require(sceLibcMspaceRealloc_nid_postfix(arena, grown, storage.size()) == nullptr);
    Require(grown[127] == 127);
    void* aligned = nullptr;
    Require(sceLibcMspacePosixMemalign_nid_postfix(arena, &aligned, 4096, 1024) == 0);
    Require((reinterpret_cast<std::uintptr_t>(aligned) & 4095) == 0);
    void* unchanged = aligned;
    Require(sceLibcMspacePosixMemalign_nid_postfix(arena, &unchanged, 3, 8) == 22 && unchanged == aligned);
    Require(sceLibcMspaceCalloc_nid_postfix(arena, std::numeric_limits<std::size_t>::max(), 2) == nullptr);
    sceLibcMspaceFree_nid_postfix(arena, blocker);
    sceLibcMspaceFree_nid_postfix(arena, aligned);
    Require(sceLibcMspaceRealloc_nid_postfix(arena, grown, 0) == nullptr);
    void* large = sceLibcMspaceMalloc_nid_postfix(arena, storage.size() - 256);
    Require(large != nullptr); // freeing coalesced all the fragmented blocks
    sceLibcMspaceFree_nid_postfix(arena, large);
    void* head = sceLibcMspaceMalloc_nid_postfix(arena, 64);
    Require(head != nullptr && sceLibcMspaceRealloc_nid_postfix(arena, head, 1024) == head);
    Require(sceLibcMspaceMallocUsableSize_nid_postfix(head) == 1024);
    sceLibcMspaceFree_nid_postfix(arena, head);
    std::array<void*, 2000> small{};
    for (auto& pointer : small) {
        pointer = sceLibcMspaceMalloc_nid_postfix(arena, 16);
        Require(pointer != nullptr);
    }
    for (std::size_t i = 0; i < small.size(); i += 2) sceLibcMspaceFree_nid_postfix(arena, small[i]);
    Require(sceLibcMspaceMalloc_nid_postfix(arena, storage.size() - 256) == nullptr);
    for (std::size_t i = 1; i < small.size(); i += 2) sceLibcMspaceFree_nid_postfix(arena, small[i]);
    large = sceLibcMspaceMalloc_nid_postfix(arena, storage.size() - 256);
    Require(large != nullptr);
    sceLibcMspaceFree_nid_postfix(arena, large);
    std::array<std::thread, 4> workers;
    for (auto& worker : workers) worker = std::thread([&] {
        for (int i = 0; i < 1000; ++i) {
            void* pointer = sceLibcMspaceMalloc_nid_postfix(arena, 97);
            Require(pointer != nullptr);
            std::memset(pointer, 42, 97);
            Require(sceLibcMspaceMallocUsableSize_nid_postfix(pointer) >= 97);
            sceLibcMspaceFree_nid_postfix(arena, pointer);
        }
    });
    for (auto& worker : workers) worker.join();
    MallocManagedSize stats{sizeof(MallocManagedSize), 1, 0, 0, 0, 0, 0};
    Require(sceLibcMspaceMallocStats_nid_postfix(arena, &stats) == 0);
    Require(stats.currentSystemSize == storage.size() && stats.maxSystemSize == storage.size());
    Require(stats.currentInuseSize == 0 && stats.maxInuseSize >= storage.size() - 256);
    MallocManagedSize shortStats{8, 1, 0, 0, 0, 0, 0};
    Require(sceLibcMspaceMallocStatsFast_nid_postfix(arena, &shortStats) == 22);
    void* memaligned = sceLibcMspaceMemalign_nid_postfix(arena, 256, 100);
    Require(memaligned && (reinterpret_cast<std::uintptr_t>(memaligned) & 255) == 0);
    Require(sceLibcMspaceMemalign_nid_postfix(arena, 24, 100) == nullptr);
    void* alignedAlloc = sceLibcMspaceAlignedAlloc_nid_postfix(arena, 128, 256);
    Require(alignedAlloc && (reinterpret_cast<std::uintptr_t>(alignedAlloc) & 127) == 0);
    Require(sceLibcMspaceAlignedAlloc_nid_postfix(arena, 24, 100) == nullptr);
    unsigned char* realigned = static_cast<unsigned char*>(sceLibcMspaceReallocalign_nid_postfix(arena, nullptr, 64, 64));
    Require(realigned && (reinterpret_cast<std::uintptr_t>(realigned) & 63) == 0);
    for (int i = 0; i < 64; ++i) realigned[i] = static_cast<unsigned char>(i + 1);
    unsigned char* regrown = static_cast<unsigned char*>(sceLibcMspaceReallocalign_nid_postfix(arena, realigned, 64, 256));
    Require(regrown && (reinterpret_cast<std::uintptr_t>(regrown) & 63) == 0);
    for (int i = 0; i < 64; ++i) Require(regrown[i] == static_cast<unsigned char>(i + 1));
    *__error_nid_postfix() = 0;
    Require(sceLibcMspaceReallocalign_nid_postfix(arena, regrown, 0, 16) == regrown);
    Require(sceLibcMspaceReallocalign_nid_postfix(arena, regrown, 3, 16) == regrown);
    Require(sceLibcMspaceReallocalign_nid_postfix(arena, regrown, std::size_t{1} << 63, 16) == nullptr);
    Require(sceLibcMspaceReallocalign_nid_postfix(arena, regrown, 64, 0) == regrown && regrown[63] == 64);
    Require(*__error_nid_postfix() == 0);
    unsigned char* rounded = static_cast<unsigned char*>(sceLibcMspaceReallocalign_nid_postfix(arena, nullptr, 100, 0));
    Require(rounded && (reinterpret_cast<std::uintptr_t>(rounded) & 127) == 0);
    Require(sceLibcMspaceMallocStatsFast_nid_postfix(arena, &stats) == 0 && stats.currentInuseSize >= 100);
    sceLibcMspaceFree_nid_postfix(arena, memaligned);
    void* region = sceLibcMspaceMalloc_nid_postfix(arena, 8192);
    Require(region != nullptr);
    Require(sceLibcMspaceCreate_nid_postfix("outside", static_cast<unsigned char*>(region) + 8192, 4096, 0) == nullptr);
    void* nested = sceLibcMspaceCreate_nid_postfix("nested", region, 8192, 1);
    Require(nested == region);
    void* inner = sceLibcMspaceMalloc_nid_postfix(nested, 64);
    Require(inner > region && inner < static_cast<unsigned char*>(region) + 8192);
    Require(sceLibcMspaceMallocUsableSize_nid_postfix(inner) == 64 && sceLibcMspaceMallocUsableSize_nid_postfix(region) == 8192);
    Require(sceLibcMspaceCreate_nid_postfix("flags", storage.data(), storage.size(), 2) == nullptr);
    Require(sceLibcMspaceDestroy_nid_postfix(nested) == 0);
    sceLibcMspaceFree_nid_postfix(arena, region);
    Require(sceLibcMspaceDestroy_nid_postfix(arena) == 0);
    Require(sceLibcMspaceMalloc_nid_postfix(arena, 8) == nullptr);
    Require(sceLibcMspaceCreate_nid_postfix("reuse", storage.data(), storage.size(), 0) == arena);
    Require(sceLibcMspaceDestroy_nid_postfix(arena) == 0);
}
