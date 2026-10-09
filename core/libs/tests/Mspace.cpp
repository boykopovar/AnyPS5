#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
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
static void Require(bool value) {
    if (!value) { std::fputs("Mspace check failed\n", stderr); std::abort(); }
}
int main() {
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
        Require(sceLibcMspaceReallocalign_nid_postfix(arena, first, size, 16) == nullptr);
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
    unsigned char* realigned = static_cast<unsigned char*>(sceLibcMspaceReallocalign_nid_postfix(arena, nullptr, 64, 64));
    Require(realigned && (reinterpret_cast<std::uintptr_t>(realigned) & 63) == 0);
    for (int i = 0; i < 64; ++i) realigned[i] = static_cast<unsigned char>(i + 1);
    unsigned char* regrown = static_cast<unsigned char*>(sceLibcMspaceReallocalign_nid_postfix(arena, realigned, 256, 64));
    Require(regrown && (reinterpret_cast<std::uintptr_t>(regrown) & 63) == 0);
    for (int i = 0; i < 64; ++i) Require(regrown[i] == static_cast<unsigned char>(i + 1));
    Require(sceLibcMspaceReallocalign_nid_postfix(arena, regrown, 16, 0) == nullptr);
    Require(sceLibcMspaceReallocalign_nid_postfix(arena, regrown, 16, 3) == nullptr);
    Require(sceLibcMspaceReallocalign_nid_postfix(arena, regrown, 0, 16) == nullptr);
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
