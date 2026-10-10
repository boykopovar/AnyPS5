#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <array>
#include "SceTypes.hpp"
#include <chrono>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <thread>
#include <utility>
#include <vector>
#if defined(__linux__)
#include <fcntl.h>
#include <fstream>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

extern "C" {
void* APS5_VABI mmap_nid_postfix(void*, std::size_t, int, int, int, std::int64_t) noexcept;
int APS5_VABI munmap_nid_postfix(void*, std::size_t) noexcept;
int APS5_VABI mprotect_nid_postfix(void*, std::size_t, int) noexcept;
int APS5_VABI madvise_nid_postfix(void*, std::size_t, int);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sceKernelMapNamedFlexibleMemory(void**, std::size_t, int, int, const char*);
int APS5_VABI sceKernelMapNamedFlexibleMemoryInternal(void**, std::size_t, int, int, const char*);
int APS5_VABI sceKernelAvailableFlexibleMemorySize(std::size_t*);
int APS5_VABI sceKernelMapFlexibleMemory(void**, std::size_t, int, int);
int APS5_VABI sceKernelMunmap(void*, std::size_t);
int APS5_VABI sceKernelReleaseFlexibleMemory(void*, std::size_t);
int APS5_VABI sceKernelMprotect(const void*, std::size_t, int);
int APS5_VABI sceKernelMtypeprotect(const void*, std::size_t, int, int);
int APS5_VABI sceKernelBatchMap2(KernelBatchMapEntry*, int, int*, int);
int APS5_VABI sceKernelVirtualQuery(const void*, int, VirtualQueryInfo*, std::uint64_t);
int APS5_VABI sceKernelSetVirtualRangeName(const void*, std::uint64_t, const char*);
int APS5_VABI sceKernelClearVirtualRangeName(const void*, std::uint64_t);
int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
int APS5_VABI sceKernelMapDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t);
int APS5_VABI sceKernelReleaseDirectMemory(std::int64_t, std::size_t);
int APS5_VABI sceKernelCheckedReleaseDirectMemory(std::int64_t, std::size_t);
int APS5_VABI sceKernelReserveVirtualRange(void**, std::size_t, int, std::size_t);
int APS5_VABI sceKernelMemoryPoolReserve(void*, std::size_t, std::size_t, int, void**);
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
std::int64_t APS5_VABI sceKernelPread(int, void*, std::size_t, std::int64_t);
int APS5_VABI sceKernelAioInitializeImpl(void*, std::int32_t);
int APS5_VABI sceKernelAioSubmitReadCommands(KernelAioRwRequest*, std::int32_t, std::int32_t, std::int32_t*);
int APS5_VABI sceKernelAioWaitRequest(std::int32_t, std::int32_t*, std::uint32_t*);
int APS5_VABI sceKernelAioDeleteRequest(std::int32_t, std::int32_t*);
int APS5_VABI sceKernelMlock_nid_postfix(void*, std::uint64_t);
int APS5_VABI sceKernelGetDirectMemoryType(std::int64_t, int*, std::int64_t*, std::int64_t*);
int APS5_VABI sceKernelBatchMap2(KernelBatchMapEntry*, int, int*, int);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Guest memory check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

static const char* NameAt(const void* address) {
    static VirtualQueryInfo info;
    Require(sceKernelVirtualQuery(address, 0, &info, sizeof(info)) == 0);
    return info.name;
}

static void CheckReleaseFlexibleMemory() {
    constexpr std::size_t length = 0x10000;
    std::size_t before = 0;
    std::size_t available = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&before) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapped, length, 3, 0) == 0 && mapped != nullptr);
    static_cast<volatile unsigned char*>(mapped)[length - 1] = 1;
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - length);
    Require(sceKernelReleaseFlexibleMemory(mapped, length) == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before);
    void* again = mapped;
    Require(sceKernelMapFlexibleMemory(&again, length, 3, 0x90) == 0 && again == mapped);
    Require(static_cast<volatile unsigned char*>(again)[length - 1] == 0);
    Require(sceKernelMunmap(again, length) == 0);
}

static void CheckNamedAndHintedMappings() {
    constexpr std::size_t length = 0x10000;
    void* first = nullptr;
    Require(sceKernelMapNamedFlexibleMemory(&first, length, 3, 0, "first mapping") == 0);
    Require(std::strcmp(NameAt(first), "first mapping") == 0);
    auto* middle = static_cast<unsigned char*>(first) + 0x4000;
    Require(sceKernelSetVirtualRangeName(middle, 0x4000, "middle") == 0);
    Require(std::strcmp(NameAt(first), "first mapping") == 0);
    Require(std::strcmp(NameAt(middle), "middle") == 0);
    Require(sceKernelClearVirtualRangeName(first, length) == 0);
    Require(NameAt(middle)[0] == '\0');
    Require(sceKernelSetVirtualRangeName(nullptr, length, "x") != 0);
    void* hinted = first;
    Require(sceKernelMapFlexibleMemory(&hinted, length, 3, 0) == 0);
    Require(hinted > first && (reinterpret_cast<std::uintptr_t>(hinted) & 0x3fff) == 0);
    static_cast<volatile unsigned char*>(hinted)[length - 1] = 1;
    void* overwrite = first;
    Require(sceKernelMapFlexibleMemory(&overwrite, 0x4000, 3, 0x90) == static_cast<int>(0x8002000cu));
    Require(overwrite == first);
    Require(sceKernelMunmap(hinted, length) == 0);
    void* exclusive = hinted;
    Require(sceKernelMapFlexibleMemory(&exclusive, length, 3, 0x90) == 0);
    Require(exclusive == hinted);
    static_cast<volatile unsigned char*>(exclusive)[0] = 1;
    Require(sceKernelMunmap(exclusive, length) == 0);
    Require(sceKernelMunmap(first, length) == 0);
}

static void CheckAudioCoprocessorProtection() {
    constexpr std::size_t length = 0x4000;
    void* writable = nullptr;
    Require(sceKernelMapFlexibleMemory(&writable, length, 0x200, 0) == 0);
    static_cast<volatile unsigned char*>(writable)[length - 1] = 7;
    Require(static_cast<volatile unsigned char*>(writable)[length - 1] == 7);
    Require(sceKernelMprotect(writable, length, 0x100) == 0);
    Require(static_cast<volatile unsigned char*>(writable)[length - 1] == 7);
    Require(sceKernelMprotect(writable, length, 0x3f2) == 0);
    static_cast<volatile unsigned char*>(writable)[0] = 9;
    Require(sceKernelMunmap(writable, length) == 0);
    bool rejected = false;
    void* undefined = nullptr;
    try { sceKernelMapFlexibleMemory(&undefined, length, 0x400, 0); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected && undefined == nullptr);
}

static void CheckInternalNamedFlexibleMapping() {
    constexpr std::size_t length = 0x10000;
    std::size_t before = 0;
    std::size_t available = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&before) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapNamedFlexibleMemoryInternal(&mapped, length, 3, 0, "internal mapping") == 0 && mapped != nullptr);
    Require(std::strcmp(NameAt(mapped), "internal mapping") == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - length);
    Require(sceKernelMunmap(mapped, length) == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before);
    void* flagged = nullptr;
    Require(sceKernelMapNamedFlexibleMemoryInternal(&flagged, length, 3, 0x8000, "internal flag") == 0 && flagged != nullptr);
    Require(std::strcmp(NameAt(flagged), "internal flag") == 0);
    static_cast<volatile unsigned char*>(flagged)[length - 1] = 1;
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - length);
    Require(sceKernelMunmap(flagged, length) == 0);
    bool rejected = false;
    void* unknown = nullptr;
    try { sceKernelMapNamedFlexibleMemoryInternal(&unknown, length, 3, 0x20000, "internal mapping"); } catch (const std::exception&) { rejected = true; }
    Require(rejected && unknown == nullptr);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before);
}

static void CheckBatchMapStopsAtInvalidEntry() {
    constexpr std::size_t page = 0x4000;
    constexpr int mapFlexible = 3;
    constexpr int unmap = 1;
    constexpr int protect = 2;
    const auto flexible = [&] { return KernelBatchMapEntry{nullptr, 0, page, 3, 0, 0, mapFlexible}; };
    for (const std::int32_t operation : {5, 6, -1, std::numeric_limits<std::int32_t>::max()}) {
        KernelBatchMapEntry entries[3] = {flexible(), flexible(), flexible()};
        entries[1].operation = operation;
        int processed = -1;
        Require(sceKernelBatchMap2(entries, 3, &processed, 0) == SCE_KERNEL_ERROR_EINVAL);
        Require(processed == 1);
        Require(entries[0].start != nullptr && entries[1].start == nullptr && entries[2].start == nullptr);
        Require(sceKernelMunmap(entries[0].start, page) == 0);
    }
    KernelBatchMapEntry entries[3] = {flexible(), flexible(), flexible()};
    entries[1].operation = protect;
    entries[1].length = 0;
    int processed = -1;
    Require(sceKernelBatchMap2(entries, 3, &processed, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(processed == 1 && entries[0].start != nullptr);
    Require(entries[1].start == nullptr && entries[2].start == nullptr);
    KernelBatchMapEntry unmaps[2] = {entries[0], entries[0]};
    unmaps[0].operation = unmap;
    unmaps[1].operation = unmap;
    unmaps[1].length = 0;
    processed = -1;
    Require(sceKernelBatchMap2(unmaps, 2, &processed, 0) == SCE_KERNEL_ERROR_EINVAL && processed == 1);
    Require(sceKernelBatchMap2(entries, 1, &processed, 0) == 0 && processed == 1);
    Require(sceKernelMunmap(entries[0].start, page) == 0);
}

static void CheckCheckedReleaseDirectMemory() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    Require(sceKernelCheckedReleaseDirectMemory(phys + 1, page) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCheckedReleaseDirectMemory(phys, page + 1) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCheckedReleaseDirectMemory(phys, 0) == 0);
    Require(sceKernelCheckedReleaseDirectMemory(phys, page * 3) == SCE_KERNEL_ERROR_ENOENT);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 2, 3, 0, phys, 0) == 0);
    Require(sceKernelMunmap(mapped, page * 2) == 0);
    Require(sceKernelCheckedReleaseDirectMemory(phys + page, page) == 0);
    Require(sceKernelCheckedReleaseDirectMemory(phys, page * 2) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelCheckedReleaseDirectMemory(phys, page) == 0);
    Require(sceKernelCheckedReleaseDirectMemory(phys, page) == SCE_KERNEL_ERROR_ENOENT);
}

static void CheckDirectMemoryFollowsPhysicalPages() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    void* first = nullptr;
    Require(sceKernelMapDirectMemory(&first, page * 2, 3, 0, phys, 0) == 0);
    static_cast<unsigned char*>(first)[0] = 11;
    static_cast<unsigned char*>(first)[page + 5] = 22;
    void* alias = nullptr;
    Require(sceKernelMapDirectMemory(&alias, page, 3, 0, phys + page, 0) == 0);
    Require(alias != first && static_cast<unsigned char*>(alias)[5] == 22);
    static_cast<unsigned char*>(alias)[5] = 37;
    Require(static_cast<unsigned char*>(first)[page + 5] == 37);
    static_cast<unsigned char*>(first)[page + 6] = 48;
    Require(static_cast<unsigned char*>(alias)[6] == 48);
    Require(sceKernelMprotect(alias, page, 1) == 0);
    static_cast<unsigned char*>(first)[page + 5] = 59;
    Require(static_cast<unsigned char*>(alias)[5] == 59);
    Require(sceKernelMprotect(alias, page, 3) == 0);
    static_cast<unsigned char*>(alias)[5] = 22;
    Require(static_cast<unsigned char*>(first)[page + 5] == 22);
    Require(sceKernelMunmap(alias, page) == 0);
    static_cast<unsigned char*>(first)[page + 5] = 22;
    Require(sceKernelMunmap(first, page * 2) == 0);
    void* filler = nullptr;
    Require(sceKernelMapFlexibleMemory(&filler, page * 2, 3, 0) == 0);
    void* second = nullptr;
    Require(sceKernelMapDirectMemory(&second, page, 3, 0, phys + page, 0) == 0);
    Require(second != first && static_cast<unsigned char*>(second)[5] == 22);
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, page, 0, 0) == 0);
    void* fixed = reserved;
    Require(sceKernelMapDirectMemory(&fixed, page, 1, 0x10, phys, 0) == 0);
    Require(fixed == reserved && static_cast<unsigned char*>(fixed)[0] == 11);
    Require(sceKernelMunmap(fixed, page) == 0);
    Require(sceKernelMunmap(second, page) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
    std::int64_t again = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &again) == 0 && again == phys);
    void* fresh = nullptr;
    Require(sceKernelMapDirectMemory(&fresh, page * 2, 3, 0, again, 0) == 0);
    Require(static_cast<unsigned char*>(fresh)[0] == 0 && static_cast<unsigned char*>(fresh)[page + 5] == 0);
    Require(sceKernelMunmap(fresh, page * 2) == 0);
    Require(sceKernelMunmap(filler, page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(again, page * 2) == 0);
}

static void CheckReleaseDirectMemoryClearsMappings() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 2, 3, 0, phys, 0) == 0);
    VirtualQueryInfo before{};
    Require(sceKernelVirtualQuery(mapped, 0, &before, sizeof(before)) == 0);
    Require(before.is_direct && before.offset == static_cast<std::uint64_t>(phys));
    Require(sceKernelReleaseDirectMemory(phys + page, page) == 0);
    VirtualQueryInfo split{};
    Require(sceKernelVirtualQuery(mapped, 0, &split, sizeof(split)) == 0);
    Require(split.is_direct && split.offset == static_cast<std::uint64_t>(phys));
    VirtualQueryInfo dropped{};
    Require(sceKernelVirtualQuery(static_cast<unsigned char*>(mapped) + page, 0, &dropped, sizeof(dropped)) == 0);
    Require(!dropped.is_direct && dropped.offset == 0);
    Require(sceKernelReleaseDirectMemory(phys, page) == 0);
    VirtualQueryInfo cleared{};
    Require(sceKernelVirtualQuery(mapped, 0, &cleared, sizeof(cleared)) == 0);
    Require(!cleared.is_direct && cleared.offset == 0);
    Require(sceKernelMunmap(mapped, page * 2) == 0);
}

static void CheckFixedMappingReplacesPartialOverlap() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 3, 0, 0, &phys) == 0);
    void* reserved = nullptr;
    Require(sceKernelMapDirectMemory(&reserved, page * 3, 3, 0, phys, 0) == 0);
    Require(sceKernelMunmap(reserved, page * 3) == 0);
    void* head = reserved;
    Require(sceKernelMapDirectMemory(&head, page, 3, 0x10, phys, 0) == 0 && head == reserved);
    void* fixed = reserved;
    Require(sceKernelMapDirectMemory(&fixed, page * 3, 3, 0x10, phys, 0) == 0);
    Require(fixed == reserved);
    static_cast<unsigned char*>(fixed)[page * 2 + 7] = 0x5c;
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(fixed, 0, &info, sizeof(info)) == 0);
    Require(info.is_direct && info.offset == static_cast<std::uint64_t>(phys));
    Require(static_cast<unsigned char*>(fixed)[page * 2 + 7] == 0x5c);
    Require(sceKernelMunmap(fixed, page * 3) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 3) == 0);
}

static void CheckGetDirectMemoryType() {
    constexpr std::size_t page = 0x4000;
    std::int64_t first = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 3, &first) == 0);
    std::int64_t second = 0;
    Require(sceKernelAllocateDirectMemory(first + page * 2, first + page * 3, page, 0, 1, &second) == 0 && second == first + page * 2);
    int type = -1;
    std::int64_t start = -1;
    std::int64_t end = -1;
    Require(sceKernelGetDirectMemoryType(first, &type, &start, &end) == 0);
    Require(type == 3 && start == first && end == first + page * 2);
    type = -1;
    Require(sceKernelGetDirectMemoryType(first + page * 2 - 1, &type, &start, &end) == 0);
    Require(type == 3 && start == first && end == first + page * 2);
    Require(sceKernelGetDirectMemoryType(first + page * 2, &type, &start, &end) == 0);
    Require(type == 1 && start == second && end == second + page);
    Require(sceKernelGetDirectMemoryType(first, nullptr, &start, &end) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetDirectMemoryType(first, &type, nullptr, &end) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetDirectMemoryType(first, &type, &start, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelReleaseDirectMemory(first, page) == 0);
    type = -1;
    start = -1;
    end = -1;
    Require(sceKernelGetDirectMemoryType(first, &type, &start, &end) == SCE_KERNEL_ERROR_ENOENT);
    Require(type == -1 && start == -1 && end == -1);
    Require(sceKernelGetDirectMemoryType(-1, &type, &start, &end) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelGetDirectMemoryType(first + page, &type, &start, &end) == 0);
    Require(type == 3 && start == first + page && end == first + page * 2);
    Require(sceKernelReleaseDirectMemory(first + page, page * 2) == 0);
    Require(sceKernelGetDirectMemoryType(second, &type, &start, &end) == SCE_KERNEL_ERROR_ENOENT);
}

static void CheckMtypeprotect() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 3, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 3, 3, 0, phys, 0) == 0);
    auto* bytes = static_cast<unsigned char*>(mapped);
    Require(sceKernelMtypeprotect(bytes + page + 1, 1, 3, 1) == 0);
    const auto check = [&](std::size_t index, int expectedType, int expectedProtection) {
        VirtualQueryInfo info{};
        Require(sceKernelVirtualQuery(bytes + page * index, 0, &info, sizeof(info)) == 0);
        Require(info.is_direct && info.memory_type == expectedType && info.protection == expectedProtection);
        Require(info.start == reinterpret_cast<std::uintptr_t>(bytes + page * index) && info.end == info.start + page);
        Require(info.offset == static_cast<std::uint64_t>(phys) + page * index);
        int type = -1;
        std::int64_t start = -1;
        std::int64_t end = -1;
        Require(sceKernelGetDirectMemoryType(phys + static_cast<std::int64_t>(page * index), &type, &start, &end) == 0);
        Require(type == expectedType && start == phys + static_cast<std::int64_t>(page * index) && end == start + static_cast<std::int64_t>(page));
    };
    check(0, 0, 3);
    check(1, 3, 1);
    check(2, 0, 3);
    KernelBatchMapEntry entry{};
    entry.start = bytes + page * 2;
    entry.length = page;
    entry.protection = 3;
    entry.type = 5;
    entry.operation = 4;
    int processed = -1;
    Require(sceKernelBatchMap2(&entry, 1, &processed, 0) == 0 && processed == 1);
    check(0, 0, 3);
    check(1, 3, 1);
    check(2, 5, 3);
    Require(sceKernelMtypeprotect(bytes, page * 3, 2, 3) == 0);
    VirtualQueryInfo whole{};
    Require(sceKernelVirtualQuery(bytes, 0, &whole, sizeof(whole)) == 0);
    Require(whole.memory_type == 2 && whole.protection == 3);
    int type = -1;
    std::int64_t start = -1;
    std::int64_t end = -1;
    Require(sceKernelGetDirectMemoryType(phys + static_cast<std::int64_t>(page * 2), &type, &start, &end) == 0 && type == 2);
    Require(sceKernelMunmap(mapped, page * 3) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 3) == 0);
}

static void CheckDirectMemoryGpuProtBits() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page, 0, 0, &phys) == 0);
    void* mapping = nullptr;
    Require(sceKernelMapDirectMemory(&mapping, page, 0x3f2, 0, phys, 0) == 0);
    static_cast<unsigned char*>(mapping)[0] = 11;
    Require(static_cast<unsigned char*>(mapping)[0] == 11);
    Require(sceKernelMunmap(mapping, page) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page) == 0);
}

static void CheckFixedVirtualReservation() {
    constexpr std::size_t page = 0x4000;
    void* probe = nullptr;
    Require(sceKernelReserveVirtualRange(&probe, page * 4, 0, 0) == 0);
    Require(sceKernelMunmap(probe, page * 4) == 0);
    void* const requested = static_cast<unsigned char*>(probe) + page;
    void* fixed = requested;
    Require(sceKernelReserveVirtualRange(&fixed, page * 2, 0x400010, 0) == 0);
    Require(fixed == requested);
    void* again = requested;
    Require(sceKernelReserveVirtualRange(&again, page * 2, 0x400010, 0) == 0);
    Require(again == requested);
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    void* mapped = requested;
    Require(sceKernelMapDirectMemory(&mapped, page * 2, 3, 0x10, phys, 0) == 0);
    Require(mapped == requested);
    static_cast<unsigned char*>(mapped)[0] = 11;
    VirtualQueryInfo before{};
    Require(sceKernelVirtualQuery(mapped, 0, &before, sizeof(before)) == 0);
    Require(before.is_direct);
    void* reserved = requested;
    Require(sceKernelReserveVirtualRange(&reserved, page * 2, 0x10, 0) == 0);
    Require(reserved == requested);
    VirtualQueryInfo after{};
    Require(sceKernelVirtualQuery(reserved, 0, &after, sizeof(after)) == 0);
    Require(!after.is_committed && !after.is_direct && after.protection == 0);
    void* remapped = requested;
    Require(sceKernelMapDirectMemory(&remapped, page * 2, 3, 0x10, phys, 0) == 0);
    Require(remapped == requested);
    VirtualQueryInfo revived{};
    Require(sceKernelVirtualQuery(remapped, 0, &revived, sizeof(revived)) == 0);
    Require(revived.is_direct);
    bool refused = false;
    try {
        sceKernelReserveVirtualRange(&reserved, page * 2, 0x90, 0);
    } catch (const std::exception&) {
        refused = true;
    }
    Require(refused);
    Require(sceKernelMunmap(reserved, page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
    void* pooled = nullptr;
    Require(sceKernelMemoryPoolReserve(requested, page * 2, 0, 0x10, &pooled) == 0);
    Require(pooled == requested);
    Require(sceKernelMunmap(pooled, page * 2) == 0);
}

static void CheckReservedRangeIsNotCommitted() {
    constexpr std::size_t page = 0x4000;
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, page * 2, 0, 0) == 0);
    const auto start = reinterpret_cast<std::uintptr_t>(reserved);
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(reserved, 0, &info, sizeof(info)) == 0);
    Require(!info.is_committed && !info.is_direct && !info.is_flexible && info.protection == 0);
    Require(info.start == start && info.end == start + page * 2);
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page, 0, 0, &phys) == 0);
    void* fixed = reserved;
    Require(sceKernelMapDirectMemory(&fixed, page, 3, 0x10, phys, 0) == 0);
    Require(fixed == reserved);
    static_cast<unsigned char*>(fixed)[0] = 7;
    Require(sceKernelVirtualQuery(reserved, 0, &info, sizeof(info)) == 0);
    Require(info.is_committed && info.is_direct && info.protection == 3);
    Require(info.start == start && info.end == start + page);
    Require(sceKernelVirtualQuery(static_cast<unsigned char*>(reserved) + page, 0, &info, sizeof(info)) == 0);
    Require(!info.is_committed && info.protection == 0);
    Require(info.start == start + page && info.end == start + page * 2);
    Require(sceKernelMunmap(reserved, page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page) == 0);
}

static void CheckNoOverwriteRefusesLiveMapping() {
    constexpr std::size_t page = 0x4000;
    constexpr int outOfMemory = static_cast<int>(0x8002000cu);
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page, 3, 0, phys, 0) == 0);
    static_cast<unsigned char*>(mapped)[0] = 13;
    void* again = mapped;
    Require(sceKernelMapDirectMemory(&again, page, 3, 0x90, phys + page, 0) == outOfMemory);
    Require(again == mapped);
    void* flexible = mapped;
    Require(sceKernelMapFlexibleMemory(&flexible, page, 3, 0x90) == outOfMemory);
    Require(static_cast<unsigned char*>(mapped)[0] == 13);
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(mapped, 0, &info, sizeof(info)) == 0);
    Require(info.is_direct && info.offset == static_cast<std::uint64_t>(phys));
    Require(sceKernelMunmap(mapped, page) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
}

#ifdef _WIN32
static void CheckNoOverwriteRejectsHostOccupiedMapping() {
    constexpr std::size_t page = 0x4000;
    void* reservation = nullptr;
    Require(sceKernelReserveVirtualRange(&reservation, page * 4, 0, 0) == 0);
    Require(sceKernelMunmap(reservation, page * 4) == 0);
    void* target = static_cast<unsigned char*>(reservation) + page;
    GuestArena::GuestArenaCommit_nid_postfix(target, page, PAGE_READWRITE, page);
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page, 0, 0, &phys) == 0);
    void* fixed = target;
    bool rejected = false;
    try {
        rejected = sceKernelMapDirectMemory(&fixed, page, 3, 0x90, phys, 0) != 0;
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected);
    GuestArena::GuestArenaReset_nid_postfix(target, page);
    Require(sceKernelReleaseDirectMemory(phys, page) == 0);
}

static void CheckFixedMappingsReachTheApplicationAreaEnd() {
    constexpr std::size_t page = 0x4000;
    constexpr std::uintptr_t applicationAreaEnd = 0xFC00000000ull;
    std::uintptr_t base = 0;
    std::size_t size = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &size);
    Require(base == 0x200000000ull && base + size == applicationAreaEnd);
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    void* const requested = reinterpret_cast<void*>(applicationAreaEnd - page * 2);
    void* mapped = requested;
    Require(sceKernelMapDirectMemory(&mapped, page * 2, 3, 0x90, phys, 0) == 0);
    Require(mapped == requested);
    static_cast<volatile unsigned char*>(mapped)[page * 2 - 1] = 7;
    void* alias = nullptr;
    Require(sceKernelMapDirectMemory(&alias, page, 3, 0, phys + page, 0) == 0);
    Require(static_cast<volatile unsigned char*>(alias)[page - 1] == 7);
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(mapped, 0, &info, sizeof(info)) == 0 && info.is_direct && info.end == applicationAreaEnd);
    void* beyond = reinterpret_cast<void*>(applicationAreaEnd);
    bool refused = false;
    try {
        refused = sceKernelMapDirectMemory(&beyond, page, 3, 0x10, phys, 0) != 0;
    } catch (const std::exception&) {
        refused = true;
    }
    Require(refused && beyond == reinterpret_cast<void*>(applicationAreaEnd));
    Require(sceKernelMunmap(alias, page) == 0);
    Require(sceKernelMunmap(mapped, page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
}
#endif

#if defined(__linux__)
static std::size_t LockedKilobytes() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmLck:", 0) == 0) return std::strtoull(line.c_str() + 6, nullptr, 10);
    }
    return 0;
}
#endif

static void CheckMlock() {
    constexpr std::size_t page = 0x4000;
#ifdef _WIN32
    constexpr std::size_t length = 0x400000;
#else
    constexpr std::size_t length = 0x10000;
#endif
    constexpr int outOfMemory = static_cast<int>(0x8002000cu);
    constexpr int invalid = static_cast<int>(0x80020016u);
    void* mapped = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapped, length, 3, 0) == 0);
    auto* bytes = static_cast<unsigned char*>(mapped);
    Require(sceKernelMlock_nid_postfix(mapped, 0) == 0);
#if defined(__linux__)
    const auto lockedBefore = LockedKilobytes();
#endif
    Require(sceKernelMlock_nid_postfix(bytes + 1, length - page) == 0);
#ifdef _WIN32
    SIZE_T minimum = 0;
    SIZE_T maximum = 0;
    DWORD limits = 0;
    Require(GetProcessWorkingSetSizeEx(GetCurrentProcess(), &minimum, &maximum, &limits) && minimum >= length && maximum > minimum);
    Require(VirtualUnlock(mapped, length));
    Require(!VirtualUnlock(mapped, length) && GetLastError() == ERROR_NOT_LOCKED);
#elif defined(__linux__)
    Require(LockedKilobytes() - lockedBefore == length / 1024);
#endif
    Require(sceKernelMlock_nid_postfix(mapped, length) == 0);
    Require(sceKernelMlock_nid_postfix(mapped, length) == 0);
    bytes[length - 1] = 7;
    Require(sceKernelMlock_nid_postfix(reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - page + 1), page * 2) == invalid);
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, page, 0, 0) == 0);
    Require(sceKernelMlock_nid_postfix(reserved, page) == outOfMemory);
    Require(sceKernelMunmap(reserved, page) == 0);
    Require(sceKernelMunmap(mapped, length) == 0);
    Require(sceKernelMlock_nid_postfix(mapped, page) == outOfMemory);
}

static void CheckSharedDirectMemoryLifecycle() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 3, 0, 0, &phys) == 0);
    void* first = nullptr;
    void* second = nullptr;
    Require(sceKernelMapDirectMemory(&first, page * 3, 3, 0, phys, 0) == 0);
    Require(sceKernelMapDirectMemory(&second, page * 3, 3, 0, phys, 0) == 0);
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(second, 0, &info, sizeof(info)) == 0);
    Require(info.is_direct && !info.is_flexible && info.offset == static_cast<std::uint64_t>(phys));
    auto* left = static_cast<unsigned char*>(first);
    auto* right = static_cast<unsigned char*>(second);
    left[0] = 31;
    right[page] = 47;
    left[page * 2] = 63;
    Require(right[0] == 31 && left[page] == 47 && right[page * 2] == 63);
    void* inaccessible = nullptr;
    Require(sceKernelMapDirectMemory(&inaccessible, page, 0, 0, phys + page, 0) == 0);
    left[page] = 48;
    Require(sceKernelMprotect(inaccessible, page, 1) == 0);
    Require(static_cast<const unsigned char*>(inaccessible)[0] == 48);
    Require(sceKernelMunmap(inaccessible, page) == 0);
    Require(sceKernelMunmap(left + page, page) == 0);
    right[page] = 79;
    Require(left[0] == 31 && left[page * 2] == 63);
    void* middle = left + page;
    Require(sceKernelMapDirectMemory(&middle, page, 3, 0x10, phys + page, 0) == 0);
    Require(left[page] == 79);
    Require(sceKernelVirtualQuery(middle, 0, &info, sizeof(info)) == 0);
    Require(info.offset == static_cast<std::uint64_t>(phys) + page && info.start == reinterpret_cast<std::uintptr_t>(middle));
    Require(sceKernelMprotect(second, page * 3, 0) == 0);
    left[page] = 95;
    Require(sceKernelMprotect(second, page * 3, 1) == 0);
    Require(right[page] == 95);
    Require(sceKernelMunmap(first, page) == 0);
    Require(sceKernelMunmap(left + page * 2, page) == 0);
    Require(sceKernelMunmap(middle, page) == 0);
    Require(sceKernelMprotect(second, page * 3, 3) == 0);
    right[page * 2] = 111;
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, page * 3, 0, 0) == 0);
    void* fixed = static_cast<unsigned char*>(reserved) + page;
    Require(sceKernelMapDirectMemory(&fixed, page, 3, 0x10, phys + page * 2, 0) == 0);
    Require(static_cast<unsigned char*>(fixed)[0] == 111);
    static_cast<unsigned char*>(fixed)[0] = 127;
    Require(right[page * 2] == 127);
    Require(sceKernelMapFlexibleMemory(&fixed, page, 3, 0x10) == 0);
    Require(static_cast<unsigned char*>(fixed)[0] == 0);
    Require(sceKernelVirtualQuery(fixed, 0, &info, sizeof(info)) == 0);
    Require(!info.is_direct && info.is_flexible && info.offset == 0);
    static_cast<unsigned char*>(fixed)[0] = 143;
    Require(right[page * 2] == 127);
    Require(sceKernelMunmap(reserved, page * 3) == 0);
    Require(sceKernelMunmap(second, page * 3) == 0);
    Require(sceKernelReleaseDirectMemory(phys + page, page) == 0);
    std::int64_t replacement = 0;
    Require(sceKernelAllocateDirectMemory(phys + page, phys + page * 2, page, 0, 0, &replacement) == 0);
    Require(replacement == phys + page);
    void* mixed = nullptr;
    Require(sceKernelMapDirectMemory(&mixed, page * 3, 3, 0, phys, 0) == 0);
    const auto* data = static_cast<const unsigned char*>(mixed);
    Require(data[0] == 31 && data[page] == 0 && data[page * 2] == 127);
    Require(sceKernelMunmap(mixed, page * 3) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 3) == 0);
}

static void CheckHeapAlignment() {
    std::vector<std::pair<unsigned char*, std::size_t>> blocks;
    for (std::size_t bytes = 1; bytes <= 600; ++bytes) blocks.emplace_back(static_cast<unsigned char*>(GuestHeap::GuestHeapAllocate_nid_postfix(bytes)), bytes);
    for (const std::size_t bytes : {std::size_t{4000}, std::size_t{70000}, std::size_t{0x30000}}) blocks.emplace_back(static_cast<unsigned char*>(GuestHeap::GuestHeapAllocate_nid_postfix(bytes)), bytes);
    auto* grown = static_cast<unsigned char*>(GuestHeap::GuestHeapReallocate_nid_postfix(GuestHeap::GuestHeapAllocate_nid_postfix(8), 333));
    blocks.emplace_back(grown, 333);
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const auto [pointer, bytes] = blocks[index];
        Require((reinterpret_cast<std::uintptr_t>(pointer) & 31u) == 0);
        std::memset(pointer, static_cast<int>(index & 0xffu), bytes);
    }
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const auto [pointer, bytes] = blocks[index];
        for (std::size_t at = 0; at < bytes; ++at) Require(pointer[at] == static_cast<unsigned char>(index & 0xffu));
        GuestHeap::GuestHeapFree_nid_postfix(pointer);
    }
}

static void CheckHeapAfterMappingReuse() {
    constexpr std::size_t bytes = 0x30000;
    auto* pointer = static_cast<unsigned char*>(GuestHeap::GuestHeapAllocate_nid_postfix(bytes));
    std::memset(pointer, 0x5a, bytes);
    Require(pointer[0] == 0x5a && pointer[bytes - 1] == 0x5a);
    GuestHeap::GuestHeapFree_nid_postfix(pointer);
    pointer = static_cast<unsigned char*>(GuestHeap::GuestHeapAllocate_nid_postfix(bytes));
    std::memset(pointer, 0xa5, bytes);
    Require(pointer[0] == 0xa5 && pointer[bytes - 1] == 0xa5);
    GuestHeap::GuestHeapFree_nid_postfix(pointer);
}

static void CheckSharedWriteTracking() {
#ifdef _WIN32
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 3, 0, 0, &phys) == 0);
    void* first = nullptr;
    void* second = nullptr;
    Require(sceKernelMapDirectMemory(&first, page * 3, 3, 0, phys, 0) == 0);
    Require(sceKernelMapDirectMemory(&second, page * 3, 3, 0, phys, 0) == 0);
    const auto collect = [](void* address, std::size_t bytes, bool clear = true) {
        std::array<void*, 32> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(address), bytes, pages.data(), &count, clear));
        return count;
    };
    Require(collect(first, page * 3) == 12);
    Require(collect(second, page * 3) == 12);
    Require(collect(first, page * 3) == 0);
    Require(collect(second, page * 3) == 0);
    auto* left = static_cast<volatile unsigned char*>(first);
    auto* right = static_cast<volatile unsigned char*>(second);
    left[page + 5] = 21;
    Require(right[page + 5] == 21);
    Require(collect(first, page * 3, false) == 4);
    Require(collect(first, page * 3, false) == 4);
    Require(collect(first, page * 3) == 4);
    Require(collect(second, page * 3) == 4);
    Require(collect(first, page * 3) == 0);
    right[page * 2] = 42;
    Require(collect(first, page * 3) == 4);
    Require(collect(second, page * 3) == 4);
    Require(collect(second, page * 3) == 0);
    Require(sceKernelMprotect(first, page * 3, 1) == 0);
    collect(first, page * 3);
    collect(second, page * 3);
    right[0] = 63;
    Require(left[0] == 63 && collect(first, page * 3) == 4);
    Require(sceKernelMprotect(first, page * 3, 3) == 0);
    collect(first, page * 3);
    left[0] = 84;
    Require(right[0] == 84 && collect(second, page * 3) != 0);
    void* third = nullptr;
    Require(sceKernelMapDirectMemory(&third, page, 3, 0, phys + page, 0) == 0);
    collect(first, page * 3);
    collect(second, page * 3);
    static_cast<volatile unsigned char*>(third)[0] = 105;
    Require(collect(first, page * 3) == 4 && collect(second, page * 3) == 4);
    Require(sceKernelMunmap(third, page) == 0);
    Require(sceKernelMunmap(first, page * 3) == 0);
    Require(sceKernelMunmap(second, page * 3) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 3) == 0);
#endif
}

static void CheckPinnedSharedPages() {
#ifdef _WIN32
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 3, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 3, 3, 0, phys, 0) == 0);
    auto* bytes = static_cast<volatile unsigned char*>(mapped);
    const auto collect = [&] {
        std::array<void*, 32> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), page * 3, pages.data(), &count, true));
        return count;
    };
    const auto protection = [&](std::size_t offset) {
        MEMORY_BASIC_INFORMATION info{};
        Require(VirtualQuery(const_cast<unsigned char*>(bytes + offset), &info, sizeof(info)) == sizeof(info));
        return info.Protect;
    };
    collect();
    Require(collect() == 0);
    Require(protection(0) == PAGE_READONLY && protection(page) == PAGE_READONLY);
    GuestArena::GuestArenaPinWritable_nid_postfix(const_cast<unsigned char*>(bytes + page), page);
    Require(protection(page) == PAGE_READWRITE && protection(0) == PAGE_READONLY && protection(page * 2) == PAGE_READONLY);
    Require(collect() == 4);
    Require(collect() == 4);
    Require(protection(page) == PAGE_READWRITE);
    bytes[page + 8] = 7;
    bytes[0] = 9;
    Require(collect() == 8);
    Require(collect() == 4);
    Require(protection(0) == PAGE_READONLY && protection(page) == PAGE_READWRITE);
    GuestArena::GuestArenaUnpinWritable_nid_postfix(const_cast<unsigned char*>(bytes + page), page);
    collect();
    Require(collect() == 0);
    Require(protection(page) == PAGE_READONLY);
    bytes[page + 8] = 11;
    Require(collect() == 4);
    Require(sceKernelMunmap(mapped, page * 3) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 3) == 0);
#endif
}

static void CheckReadsIntoSharedWriteTracking() {
#ifdef _WIN32
    constexpr std::size_t page = 0x4000;
    const auto path = std::filesystem::path("anyps5-tracked-read-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
    std::vector<char> seed(page * 2);
    for (std::size_t i = 0; i < seed.size(); ++i) seed[i] = static_cast<char>(i * 7 + 1);
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write(seed.data(), static_cast<std::streamsize>(seed.size()));
    }
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 2, 3, 0, phys, 0) == 0);
    auto* bytes = static_cast<unsigned char*>(mapped);
    const auto collect = [&] {
        std::array<void*, 32> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), page * 2, pages.data(), &count, true));
        return count;
    };
    const int fd = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(fd >= 0);
    collect();
    Require(collect() == 0);
    Require(sceKernelPread(fd, mapped, page * 2, 0) == static_cast<std::int64_t>(page * 2));
    Require(std::memcmp(bytes, seed.data(), page * 2) == 0);
    Require(collect() == 8);
    const int sequential = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(sequential >= 0);
    Require(sceKernelRead(sequential, bytes + page, page) == static_cast<std::int64_t>(page));
    Require(sceKernelClose(sequential) == 0);
    Require(std::memcmp(bytes + page, seed.data(), page) == 0);
    Require(collect() == 4);
    Require(sceKernelAioInitializeImpl(nullptr, 0) == 0);
    KernelAioResult result{-1, 0};
    KernelAioRwRequest request{static_cast<std::int64_t>(page), page, bytes, &result, fd};
    std::int32_t id = 0;
    std::int32_t state = 0;
    Require(sceKernelAioSubmitReadCommands(&request, 1, 0, &id) == 0);
    Require(sceKernelAioWaitRequest(id, &state, nullptr) == 0);
    Require(result.state == 3 && result.return_value == static_cast<std::int64_t>(page));
    Require(std::memcmp(bytes, seed.data() + page, page) == 0);
    Require(collect() == 4);
    std::int32_t deleted = -1;
    Require(sceKernelAioDeleteRequest(id, &deleted) == 0);
    Require(sceKernelMprotect(mapped, page * 2, 1) == 0);
    bool refused = false;
    try {
        sceKernelPread(fd, mapped, page, 0);
    } catch (const std::exception&) {
        refused = true;
    }
    Require(refused);
    KernelAioResult refusedResult{-1, 0};
    KernelAioRwRequest refusedRequest{0, page, bytes, &refusedResult, fd};
    Require(sceKernelAioSubmitReadCommands(&refusedRequest, 1, 0, &id) == 0);
    Require(sceKernelAioWaitRequest(id, &state, nullptr) == 0);
    Require(refusedResult.return_value == static_cast<std::int64_t>(SCE_KERNEL_ERROR_EFAULT));
    Require(sceKernelAioDeleteRequest(id, &deleted) == 0);
    Require(sceKernelClose(fd) == 0);
    Require(sceKernelMunmap(mapped, page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
    std::filesystem::remove(path);
#endif
}

#if defined(__linux__)
using PageRuns = std::vector<std::pair<std::uintptr_t, std::uintptr_t>>;

static bool CollectRuns(const void* base, std::size_t offset, std::size_t bytes, PageRuns& runs) {
    runs.clear();
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    std::pair<std::uintptr_t, PageRuns*> context{address, &runs};
    return GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(address + offset, bytes, [](void* context, std::uintptr_t begin, std::uintptr_t end) {
        auto& [origin, into] = *static_cast<std::pair<std::uintptr_t, PageRuns*>*>(context);
        if (!into->empty() && into->back().second == (begin - origin) / 4096) into->back().second = (end - origin) / 4096;
        else into->emplace_back((begin - origin) / 4096, (end - origin) / 4096);
    }, &context);
}

static bool Written(const void* base, std::size_t bytes, PageRuns expected) {
    PageRuns runs;
    const bool complete = CollectRuns(base, 0, bytes, runs);
    if (complete && runs == expected) return true;
    std::fprintf(stderr, "write watch collect %s, written pages:", complete ? "complete" : "incomplete");
    for (const auto& [first, last] : runs) std::fprintf(stderr, " [%zu, %zu)", static_cast<std::size_t>(first), static_cast<std::size_t>(last));
    std::fputs("\n", stderr);
    return false;
}

static void CheckWriteWatch() {
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::puts("write watch unavailable: not tested");
        return;
    }
    constexpr std::size_t length = 0x100000;
    constexpr std::size_t small = 4096;
    void* mapping = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapping, length, 3, 0) == 0);
    auto* bytes = static_cast<volatile unsigned char*>(mapping);
    const auto address = reinterpret_cast<std::uintptr_t>(mapping);
    Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address, length));
    Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address + small, small));
    Require(!GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address, length + small));
    Require(!GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(&length), sizeof(length)));
    Require(Written(mapping, length, {{0, length / small}}));
    Require(Written(mapping, length, {}));
    bytes[5 * small + 17] = 1;
    Require(Written(mapping, length, {{5, 6}}));
    Require(Written(mapping, length, {}));
    static_cast<void>(bytes[10 * small]);
    Require(Written(mapping, length, {}));
    bytes[7 * small] = 1;
    bytes[9 * small] = 1;
    PageRuns runs;
    Require(CollectRuns(mapping, 8 * small, small, runs) && runs.empty());
    Require(CollectRuns(mapping, 7 * small + 100, 1, runs) && runs == PageRuns{{7, 8}});
    Require(Written(mapping, length, {{9, 10}}));
    int pipe[2];
    Require(::pipe(pipe) == 0);
    Require(::write(pipe[1], "kernel", 6) == 6);
    Require(::read(pipe[0], const_cast<unsigned char*>(bytes + 20 * small + 8), 6) == 6);
    ::close(pipe[0]);
    ::close(pipe[1]);
    Require(bytes[20 * small + 8] == 'k' && Written(mapping, length, {{20, 21}}));
    std::thread([&] { bytes[30 * small + 5] = 3; }).join();
    Require(Written(mapping, length, {{30, 31}}));
    std::vector<unsigned char> source(2 * small, 0xab);
    std::memcpy(const_cast<unsigned char*>(bytes + 40 * small + 2048), source.data(), source.size());
    Require(Written(mapping, length, {{40, 43}}));
    Require(sceKernelMprotect(mapping, length, 1) == 0);
    Require(sceKernelMprotect(mapping, length, 3) == 0);
    Require(Written(mapping, length, {}));
    bytes[50 * small] = 1;
    Require(Written(mapping, length, {{50, 51}}));
    constexpr std::size_t guestPage = 0x4000;
    auto* middle = const_cast<unsigned char*>(bytes + 4 * guestPage);
    Require(sceKernelMunmap(middle, guestPage) == 0);
    Require(!GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address, length));
    Require(!CollectRuns(mapping, 0, length, runs) && runs.empty());
    void* fixed = middle;
    Require(sceKernelMapFlexibleMemory(&fixed, guestPage, 3, 0x10) == 0 && fixed == middle);
    Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address, length));
    Require(Written(mapping, length, {{16, 20}}));
    Require(Written(mapping, length, {}));
    middle[1] = 1;
    Require(Written(mapping, length, {{16, 17}}));
    Require(sceKernelMunmap(mapping, length) == 0);
    Require(!GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address, small));
    constexpr std::size_t tableSpan = 0x200000;
    constexpr std::size_t spanned = 2 * tableSpan;
    void* raw = mmap(nullptr, spanned + tableSpan, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(raw != MAP_FAILED);
    const auto rawAddress = reinterpret_cast<std::uintptr_t>(raw);
    void* region = reinterpret_cast<void*>((rawAddress + tableSpan - 1) & ~(tableSpan - 1));
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(region, spanned);
    Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(region), spanned));
    Require(Written(region, spanned, {{0, spanned / small}}));
    Require(Written(region, spanned, {}));
    static_cast<volatile unsigned char*>(region)[tableSpan + 3 * small] = 1;
    static_cast<volatile unsigned char*>(region)[7 * small] = 1;
    Require(Written(region, spanned, {{7, 8}, {tableSpan / small + 3, tableSpan / small + 4}}));
    Require(Written(region, spanned, {}));
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(region, spanned);
    Require(munmap(raw, spanned + tableSpan) == 0);
}

static std::string BackingOf(const void* address) {
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        if (std::strtoull(line.c_str(), nullptr, 16) != reinterpret_cast<std::uintptr_t>(address)) continue;
        const auto path = line.find('/');
        return path == std::string::npos ? std::string() : line.substr(path);
    }
    return {};
}

static void CheckDirectMemoryBackingNeedsNoFilesystem() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page, 3, 0, phys, 0) == 0);
    const auto backing = BackingOf(mapped);
    if (backing.rfind("/memfd:", 0) != 0) std::fprintf(stderr, "direct memory backing: %s\n", backing.c_str());
    Require(backing.rfind("/memfd:", 0) == 0);
    Require(sceKernelMunmap(mapped, page) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page) == 0);
}

static void CheckDirectMemorySharedBacking() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 3, 0, 0, &phys) == 0);
    void* first = nullptr;
    Require(sceKernelMapDirectMemory(&first, page * 2, 3, 0, phys + page, 0) == 0);
    const auto address = reinterpret_cast<std::uintptr_t>(first);
    static_cast<volatile unsigned char*>(first)[page + 5] = 0x5a;
    int file = -1;
    std::uint64_t offset = 0;
    Require(GuestArena::GuestArenaSharedBacking_nid_postfix(address + page, page, &file, &offset));
    Require(offset == page * 2);
    unsigned char byte = 0;
    Require(pread(file, &byte, 1, static_cast<off_t>(offset + 5)) == 1 && byte == 0x5a);
    const int seals = fcntl(file, F_GET_SEALS);
    Require(seals >= 0 && (seals & F_SEAL_SHRINK) != 0 && (seals & F_SEAL_GROW) != 0);
    Require((fcntl(file, F_GETFD) & FD_CLOEXEC) != 0);
    close(file);
    Require(GuestArena::GuestArenaSharedBacking_nid_postfix(address, page * 2, &file, &offset) && offset == page);
    close(file);
    Require(GuestArena::GuestArenaSharedBacking_nid_postfix(address + page + 0x1000, 0x1000, &file, &offset) && offset == page * 2 + 0x1000);
    close(file);
    Require(!GuestArena::GuestArenaSharedBacking_nid_postfix(address, page * 3, &file, &offset));
    Require(!GuestArena::GuestArenaSharedBacking_nid_postfix(address, 0, &file, &offset));
    void* flexible = nullptr;
    Require(sceKernelMapFlexibleMemory(&flexible, page * 2, 3, 0) == 0);
    const auto flexibleAddress = reinterpret_cast<std::uintptr_t>(flexible);
    Require(!GuestArena::GuestArenaSharedBacking_nid_postfix(flexibleAddress, page, &file, &offset));
    void* low = flexible;
    Require(sceKernelMapDirectMemory(&low, page, 3, 0x10, phys, 0) == 0);
    void* high = static_cast<unsigned char*>(flexible) + page;
    Require(sceKernelMapDirectMemory(&high, page, 3, 0x10, phys + page, 0) == 0);
    Require(GuestArena::GuestArenaSharedBacking_nid_postfix(flexibleAddress, page * 2, &file, &offset) && offset == 0);
    close(file);
    Require(sceKernelMapDirectMemory(&high, page, 3, 0x10, phys + page * 2, 0) == 0);
    Require(!GuestArena::GuestArenaSharedBacking_nid_postfix(flexibleAddress, page * 2, &file, &offset));
    Require(GuestArena::GuestArenaSharedBacking_nid_postfix(flexibleAddress + page, page, &file, &offset) && offset == page * 2);
    close(file);
    Require(sceKernelMunmap(high, page) == 0);
    Require(!GuestArena::GuestArenaSharedBacking_nid_postfix(flexibleAddress + page, page, &file, &offset));
    Require(sceKernelMunmap(flexible, page) == 0);
    Require(sceKernelMunmap(first, page * 2) == 0);
    Require(!GuestArena::GuestArenaSharedBacking_nid_postfix(address, page, &file, &offset));
    Require(sceKernelReleaseDirectMemory(phys, page * 3) == 0);
}

static void CheckDirectMemoryWriteWatch() {
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::puts("write watch unavailable: direct memory not tested");
        return;
    }
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t small = 4096;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 3, 0, 0, &phys) == 0);
    void* first = nullptr;
    Require(sceKernelMapDirectMemory(&first, page * 3, 3, 0, phys, 0) == 0);
    const auto address = reinterpret_cast<std::uintptr_t>(first);
    auto* bytes = static_cast<volatile unsigned char*>(first);
    Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address, page * 3));
    Require(Written(first, page * 3, {{0, page * 3 / small}}));
    Require(Written(first, page * 3, {}));
    bytes[page + 5] = 1;
    Require(Written(first, page * 3, {{page / small, page / small + 1}}));
    void* alias = nullptr;
    Require(sceKernelMapDirectMemory(&alias, page, 3, 0, phys + page, 0) == 0);
    const auto aliasAddress = reinterpret_cast<std::uintptr_t>(alias);
    Require(!GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(aliasAddress, page));
    Require(!GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address + page, page));
    Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address, page) && GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address + page * 2, page));
    PageRuns runs;
    Require(!CollectRuns(first, 0, page * 3, runs));
    bytes[0] = 2;
    static_cast<volatile unsigned char*>(alias)[7] = 3;
    Require(bytes[page + 7] == 3);
    Require(CollectRuns(first, 0, page, runs) && runs == PageRuns{{0, 1}});
    Require(CollectRuns(first, page * 2, page, runs) && runs.empty());
    Require(sceKernelMunmap(alias, page) == 0);
    Require(sceKernelMunmap(first, page * 3) == 0);
    Require(!GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address, page));
    void* flexible = nullptr;
    Require(sceKernelMapFlexibleMemory(&flexible, page * 3, 3, 0) == 0);
    Require(Written(flexible, page * 3, {{0, page * 3 / small}}));
    Require(Written(flexible, page * 3, {}));
    void* fixed = static_cast<unsigned char*>(flexible) + page;
    Require(sceKernelMapDirectMemory(&fixed, page, 3, 0x10, phys + page * 2, 0) == 0);
    Require(fixed == static_cast<unsigned char*>(flexible) + page);
    Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(flexible), page * 3));
    Require(Written(flexible, page * 3, {{page / small, page * 2 / small}}));
    Require(Written(flexible, page * 3, {}));
    static_cast<volatile unsigned char*>(fixed)[9] = 4;
    Require(Written(flexible, page * 3, {{page / small, page / small + 1}}));
    Require(sceKernelMunmap(flexible, page * 3) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 3) == 0);
}
#endif

static void CheckVirtualQuerySplitFlexibleRanges() {
    constexpr std::size_t page = 0x4000;
    void* mapped = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapped, page * 3, 3, 0) == 0);
    auto* bytes = static_cast<unsigned char*>(mapped);
    bytes[0] = 17;
    bytes[page * 2] = 29;
    Require(sceKernelMprotect(bytes + page, page, 1) == 0);
    for (std::size_t index = 0; index < 3; ++index) {
        VirtualQueryInfo info{};
        Require(sceKernelVirtualQuery(bytes + page * index, 0, &info, sizeof(info)) == 0);
        Require(info.start == reinterpret_cast<std::uintptr_t>(bytes + page * index));
        Require(info.end == info.start + page);
        Require(info.protection == (index == 1 ? 1 : 3));
        Require(info.is_committed && info.is_flexible && !info.is_direct);
    }
    Require(sceKernelMunmap(bytes + page, page) == 0);
    for (std::size_t index : {std::size_t{0}, std::size_t{2}}) {
        VirtualQueryInfo info{};
        Require(sceKernelVirtualQuery(bytes + page * index, 0, &info, sizeof(info)) == 0);
        Require(info.start == reinterpret_cast<std::uintptr_t>(bytes + page * index));
        Require(info.end == info.start + page);
    }
    VirtualQueryInfo next{};
    Require(sceKernelVirtualQuery(bytes + page, 1, &next, sizeof(next)) == 0);
    Require(next.start == reinterpret_cast<std::uintptr_t>(bytes + page * 2));
    Require(next.end == next.start + page && next.protection == 3);
    Require(bytes[0] == 17 && bytes[page * 2] == 29);
    Require(sceKernelMunmap(bytes, page) == 0);
    Require(sceKernelMunmap(bytes + page * 2, page) == 0);
}

static void CheckVirtualQueryForNonReadableGuestRange() {
    constexpr std::size_t page = 0x4000;
    auto* mapped = static_cast<unsigned char*>(mmap_nid_postfix(nullptr, page, 3, 0x1002, -1, 0));
    const auto failed = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    Require(mapped != failed);
    Require(mprotect_nid_postfix(mapped, page, 0) == 0);
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(mapped, 0, &info, sizeof(info)) == 0);
    Require(info.start == reinterpret_cast<std::uintptr_t>(mapped));
    Require(info.end == info.start + page && info.protection == 0);
    Require(sceKernelMunmap(mapped, page) == 0);
}

static void CheckVirtualQueryPartialMunmap() {
    constexpr std::size_t page = 0x4000;
    const auto failed = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    void* raw = mmap_nid_postfix(nullptr, page * 3, 3, 0x1002, -1, 0);
    Require(raw != failed);
    auto* memory = static_cast<unsigned char*>(raw);
    VirtualQueryInfo before{};
    Require(sceKernelVirtualQuery(memory, 0, &before, sizeof(before)) == 0);
    Require(before.start == reinterpret_cast<std::uintptr_t>(memory));
    Require(before.end == reinterpret_cast<std::uintptr_t>(memory) + page * 3);
    Require(munmap_nid_postfix(memory + page, page) == 0);
    VirtualQueryInfo first{};
    Require(sceKernelVirtualQuery(memory, 0, &first, sizeof(first)) == 0);
    Require(first.start == reinterpret_cast<std::uintptr_t>(memory));
    Require(first.end == reinterpret_cast<std::uintptr_t>(memory) + page);
    VirtualQueryInfo middle{};
    Require(sceKernelVirtualQuery(memory + page, 0, &middle, sizeof(middle)) == SCE_KERNEL_ERROR_EACCES);
    VirtualQueryInfo next{};
    Require(sceKernelVirtualQuery(memory + page, 1, &next, sizeof(next)) == 0);
    Require(next.start == reinterpret_cast<std::uintptr_t>(memory) + page * 2);
    Require(next.end == reinterpret_cast<std::uintptr_t>(memory) + page * 3);
    VirtualQueryInfo tail{};
    Require(sceKernelVirtualQuery(memory + page * 2, 0, &tail, sizeof(tail)) == 0);
    Require(tail.start == reinterpret_cast<std::uintptr_t>(memory) + page * 2);
    Require(tail.end == reinterpret_cast<std::uintptr_t>(memory) + page * 3);
    Require(munmap_nid_postfix(memory, page) == 0);
    Require(munmap_nid_postfix(memory + page * 2, page) == 0);
}

int main() {
    CheckVirtualQuerySplitFlexibleRanges();
    CheckVirtualQueryForNonReadableGuestRange();
    CheckReleaseFlexibleMemory();
    CheckVirtualQueryPartialMunmap();
    CheckNamedAndHintedMappings();
    CheckInternalNamedFlexibleMapping();
    CheckBatchMapStopsAtInvalidEntry();
    CheckCheckedReleaseDirectMemory();
    CheckAudioCoprocessorProtection();
    CheckDirectMemoryFollowsPhysicalPages();
    CheckReleaseDirectMemoryClearsMappings();
    CheckFixedMappingReplacesPartialOverlap();
    CheckDirectMemoryGpuProtBits();
    CheckFixedVirtualReservation();
    CheckReservedRangeIsNotCommitted();
    CheckNoOverwriteRefusesLiveMapping();
    CheckMlock();
    CheckSharedDirectMemoryLifecycle();
    CheckGetDirectMemoryType();
    CheckMtypeprotect();
    CheckHeapAlignment();
    CheckHeapAfterMappingReuse();
#ifdef _WIN32
    CheckNoOverwriteRejectsHostOccupiedMapping();
    CheckFixedMappingsReachTheApplicationAreaEnd();
#endif
    CheckSharedWriteTracking();
    CheckReadsIntoSharedWriteTracking();
    CheckPinnedSharedPages();
#if defined(__linux__)
    CheckWriteWatch();
    CheckDirectMemoryWriteWatch();
    CheckDirectMemoryBackingNeedsNoFilesystem();
    CheckDirectMemorySharedBacking();
#endif
    constexpr std::size_t page = 0x4000;
    const auto failed = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    const auto reject = [&](std::size_t length, int protection, int flags, int fd,
                            std::int64_t offset, int error) {
        *__error_nid_postfix() = 0;
        Require(mmap_nid_postfix(nullptr, length, protection, flags, fd, offset) == failed);
        Require(*__error_nid_postfix() == error);
    };
    reject(0, 3, 0x1002, -1, 0, 22);
    reject(std::numeric_limits<std::size_t>::max(), 3, 0x1002, -1, 0, 22);
    reject(page, 8, 0x1002, -1, 0, 22);
    reject(page, 3, 0x1002, 0, 0, 22);
    reject(page, 3, 0x1002, -1, 1, 22);
    reject(page, 3, 0x1001, -1, 0, 45); // shared
    reject(page, 3, 0x1012, -1, 0, 45); // fixed
    reject(page, 3, 0x2, 0, 0, 45);    // file-backed
    reject(page, 3, 0x22, -1, 0, 45);  // Linux MAP_ANON is not guest MAP_ANON

    auto* memory = static_cast<unsigned char*>(mmap_nid_postfix(nullptr, page * 3 - 1, 3, 0x1002, -1, 0));
    Require(memory != failed && (reinterpret_cast<std::uintptr_t>(memory) & (page - 1)) == 0);
    for (std::size_t i = 0; i < page * 3; ++i) Require(memory[i] == 0);
    memory[0] = 42;
    memory[page * 2] = 73;
    {
        GuestAllocations::Mutation mutation;
        const auto range = mutation.Find(memory);
        Require(range.bytes == page * 3 && range.readable && range.writable);
    }
    Require(mprotect_nid_postfix(memory, 0, 1) == 0);
    Require(mprotect_nid_postfix(nullptr, page, 1) == -1 && *__error_nid_postfix() == 22);
    Require(mprotect_nid_postfix(memory + 1, 1, 1) == 0);
    {
        GuestAllocations::Mutation mutation;
        const auto range = mutation.Find(memory);
        Require(range.readable && !range.writable);
    }
    Require(memory[0] == 42);
    Require(mprotect_nid_postfix(memory, page, 3) == 0);
    {
        GuestAllocations::Mutation mutation;
        Require(mutation.Find(memory).writable);
    }
    for (int advice = 0; advice <= 9; ++advice) Require(madvise_nid_postfix(memory, page * 3, advice) == 0);
    Require(memory[0] == 42 && memory[page * 2] == 73);
    Require(madvise_nid_postfix(memory + 1, 0, 4) == 0);
    Require(madvise_nid_postfix(memory, page, 11) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(madvise_nid_postfix(memory, page, -1) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(madvise_nid_postfix(memory, 0, 11) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(madvise_nid_postfix(memory, std::numeric_limits<std::size_t>::max(), 0) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(madvise_nid_postfix(reinterpret_cast<void*>(0x800000000000), 1, 0) == -1 && *__error_nid_postfix() == 22);
    Require(madvise_nid_postfix(reinterpret_cast<void*>(0x7fffffffc000), 0x4000, 4) == 0);
    Require(munmap_nid_postfix(memory + 1, page) == -1 && *__error_nid_postfix() == 22);
    Require(munmap_nid_postfix(memory, 0) == -1 && *__error_nid_postfix() == 22);
    Require(memory[0] == 42);
    Require(munmap_nid_postfix(memory + page, 1) == 0); // round to one guest page
    Require(memory[0] == 42 && memory[page * 2] == 73);
    Require(munmap_nid_postfix(memory, page) == 0);
    Require(memory[page * 2] == 73);
    Require(munmap_nid_postfix(memory + page * 2, page) == 0);
    Require(munmap_nid_postfix(memory, page) == -1);
    Require(madvise_nid_postfix(memory, page, 4) == 0);
    *__error_nid_postfix() = 0;
    Require(mprotect_nid_postfix(memory, page, 1) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(mprotect_nid_postfix(memory, std::numeric_limits<std::size_t>::max(), 1) == -1 &&
            *__error_nid_postfix() == 22);
    for (int protection : {0, 1, 3, 5}) {
        void* mapped = mmap_nid_postfix(memory, 1, protection, 0x1002, -1, 0);
        Require(mapped != failed);
        {
            GuestAllocations::Mutation mutation;
            const auto range = mutation.Find(mapped);
            Require(range.readable == ((protection & 3) != 0));
            Require(range.writable == ((protection & 2) != 0));
        }
        Require(munmap_nid_postfix(mapped, 1) == 0);
    }
}
