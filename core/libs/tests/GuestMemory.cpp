#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include <algorithm>
#include <array>
#include <atomic>
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
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
#if defined(__linux__)
#include <fcntl.h>
#include <sys/stat.h>
#include <fstream>
#include <string>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <fcntl.h>
#include <io.h>
#endif

extern "C" {
void* APS5_VABI mmap_nid_postfix(void*, std::size_t, int, int, int, std::int64_t) noexcept;
int APS5_VABI munmap_nid_postfix(void*, std::size_t) noexcept;
int APS5_VABI sceKernelMmap(void*, std::size_t, int, int, int, std::int64_t, void**);
int APS5_VABI mprotect_nid_postfix(void*, std::size_t, int) noexcept;
int APS5_VABI madvise_nid_postfix(void*, std::size_t, int);
int APS5_VABI mincore_nid_postfix(const void*, std::size_t, char*);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sceKernelMapNamedFlexibleMemory(void**, std::size_t, int, int, const char*);
int APS5_VABI sceKernelMapNamedFlexibleMemoryInternal(void**, std::size_t, int, int, const char*);
int APS5_VABI sceKernelAvailableFlexibleMemorySize(std::size_t*);
int APS5_VABI sceKernelInternalMemoryGetAvailableSize(std::size_t*);
int APS5_VABI sceKernelMapFlexibleMemory(void**, std::size_t, int, int);
int APS5_VABI sceKernelMunmap(void*, std::size_t);
int APS5_VABI sceKernelReleaseFlexibleMemory(void*, std::size_t);
int APS5_VABI sceKernelMprotect(const void*, std::size_t, int);
int APS5_VABI sceKernelMtypeprotect(const void*, std::size_t, int, int);
int APS5_VABI sceKernelBatchMap2(KernelBatchMapEntry*, int, int*, int);
int APS5_VABI sceKernelMemoryPoolBatch(const KernelMemoryPoolBatchEntry*, int, int*, int);
int APS5_VABI sceKernelVirtualQuery(const void*, int, VirtualQueryInfo*, std::uint64_t);
int APS5_VABI sceKernelSetVirtualRangeName(const void*, std::uint64_t, const char*);
int APS5_VABI sceKernelClearVirtualRangeName(const void*, std::uint64_t);
int APS5_VABI sceKernelGetPageTableStats(int*, int*, int*, int*);
int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
int APS5_VABI sceKernelMapDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t);
int APS5_VABI sceKernelEnableDmemAliasing();
int APS5_VABI sceKernelReleaseDirectMemory(std::int64_t, std::size_t);
int APS5_VABI sceKernelAvailableDirectMemorySize(std::int64_t, std::int64_t, std::size_t, std::int64_t*, std::size_t*);
int APS5_VABI sceKernelCheckedReleaseDirectMemory(std::int64_t, std::size_t);
int APS5_VABI sceKernelReserveVirtualRange(void**, std::size_t, int, std::size_t);
int APS5_VABI sceKernelMemoryPoolReserve(void*, std::size_t, std::size_t, int, void**);
int APS5_VABI sceKernelMemoryPoolCommit(void*, std::size_t, int, int, int);
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
std::int64_t APS5_VABI sceKernelPread(int, void*, std::size_t, std::int64_t);
std::int64_t APS5_VABI sceKernelPwrite(int, const void*, std::size_t, std::int64_t);
int APS5_VABI sceKernelAioInitializeImpl(void*, std::int32_t);
int APS5_VABI sceKernelAioSubmitReadCommands(KernelAioRwRequest*, std::int32_t, std::int32_t, std::int32_t*);
int APS5_VABI sceKernelAioWaitRequest(std::int32_t, std::int32_t*, std::uint32_t*);
int APS5_VABI sceKernelAioDeleteRequest(std::int32_t, std::int32_t*);
int APS5_VABI sceKernelMlock_nid_postfix(void*, std::uint64_t);
int APS5_VABI sceKernelGetDirectMemoryType(std::int64_t, int*, std::int64_t*, std::int64_t*);
int APS5_VABI sceKernelBatchMap2(KernelBatchMapEntry*, int, int*, int);
void* APS5_VABI dlopen_nid_postfix(const char*, int);
void* APS5_VABI dlsym_nid_postfix(void*, const char*);
int APS5_VABI dlclose_nid_postfix(void*);
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

static void CheckFlexibleMappingIgnoresFlag8000() {
    constexpr std::size_t length = 0x10000;
    std::size_t before = 0;
    std::size_t available = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&before) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapped, length, 3, 0x8000) == 0 && mapped != nullptr);
    static_cast<volatile unsigned char*>(mapped)[length - 1] = 1;
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - length);
    Require(sceKernelMunmap(mapped, length) == 0);
    void* again = mapped;
    Require(sceKernelMapFlexibleMemory(&again, length, 3, 0x8090) == 0 && again == mapped);
    Require(static_cast<volatile unsigned char*>(again)[length - 1] == 0);
    Require(sceKernelMunmap(again, length) == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before);
    bool rejected = false;
    void* unknown = nullptr;
    try { sceKernelMapFlexibleMemory(&unknown, length, 3, 0x20000); } catch (const std::exception&) { rejected = true; }
    Require(rejected && unknown == nullptr);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before);
}

static void CheckPageTableStats() {
    int cpuTotal = -1, cpuAvailable = -1, gpuTotal = -1, gpuAvailable = -1;
    Require(sceKernelGetPageTableStats(&cpuTotal, &cpuAvailable, &gpuTotal, &gpuAvailable) == 0);
    Require(cpuTotal > 0 && gpuTotal > 0);
    Require(cpuAvailable >= 0 && cpuAvailable <= cpuTotal);
    Require(gpuAvailable >= 0 && gpuAvailable <= gpuTotal);
    Require(sceKernelGetPageTableStats(nullptr, &cpuAvailable, &gpuTotal, &gpuAvailable) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGetPageTableStats(&cpuTotal, nullptr, &gpuTotal, &gpuAvailable) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGetPageTableStats(&cpuTotal, &cpuAvailable, nullptr, &gpuAvailable) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGetPageTableStats(&cpuTotal, &cpuAvailable, &gpuTotal, nullptr) == SCE_KERNEL_ERROR_EFAULT);
}

static void CheckInternalMemoryAvailableSize() {
    std::size_t available = 1;
    bool threw = false;
    try {
        sceKernelInternalMemoryGetAvailableSize(&available);
    } catch (const std::logic_error&) {
        threw = true;
    }
    Require(threw && available == 1);
    Require(sceKernelInternalMemoryGetAvailableSize(nullptr) == -1);
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

static void CheckMprotectIgnoresUndefinedBits() {
    constexpr std::size_t length = 0x4000;
    const auto check = [&](void* mapping, int protection) {
        Require(sceKernelMprotect(mapping, length, protection) == 0);
        VirtualQueryInfo info{};
        Require(sceKernelVirtualQuery(mapping, 0, &info, sizeof(info)) == 0);
        int expected = protection & 0x3f7;
        if ((expected & 0x2) != 0) expected |= 0x1;
        if ((expected & 0x80) != 0) expected |= 0x40;
        if ((expected & 0x200) != 0) expected |= 0x100;
        if (info.protection != expected)
            std::fprintf(stderr, "Memory protection 0x%x reported 0x%x, expected 0x%x\n", static_cast<unsigned>(protection), static_cast<unsigned>(info.protection), static_cast<unsigned>(expected));
        Require(info.protection == expected);
        auto* bytes = static_cast<volatile unsigned char*>(mapping);
        if ((protection & 0x222) != 0) {
            bytes[0] = 5;
            bytes[length - 1] = 7;
            Require(bytes[0] == 5 && bytes[length - 1] == 7);
        } else if ((protection & 0x117) != 0) {
            Require(bytes[0] == 5 && bytes[length - 1] == 7);
        }
    };
    void* mapping = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapping, length, 3, 0) == 0);
    for (int byte = 0x80; byte <= 0xff; ++byte) check(mapping, byte - 0x100);
    for (const int protection : {0, 1, 2, 3, 4, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x3f7}) {
        for (const auto undefined : {0u, 8u, 0x400u, 0x80000000u, 0xfffffc08u})
            check(mapping, static_cast<int>(static_cast<std::uint32_t>(protection) | undefined));
    }
    Require(sceKernelMunmap(mapping, length) == 0);
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, length, 0, 0, &physical) == 0);
    mapping = nullptr;
    Require(sceKernelMapDirectMemory(&mapping, length, 0xf2, 0, physical, 0) == 0);
    check(mapping, -14);
    check(mapping, 0x400);
    check(mapping, 0x100);
    check(mapping, 0x200);
    check(mapping, 0x3f7);
    Require(sceKernelMunmap(mapping, length) == 0);
    Require(sceKernelReleaseDirectMemory(physical, length) == 0);
}

static void CheckStrictMemoryProtectionBits() {
    constexpr std::size_t length = 0x4000;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, length, 0, 0, &physical) == 0);
    void* mapping = nullptr;
    Require(sceKernelMapDirectMemory(&mapping, length, 3, 0, physical, 0) == 0);
    auto* bytes = static_cast<volatile unsigned char*>(mapping);
    bytes[0] = 5;
    bytes[length - 1] = 7;
    const auto rejected = [&](auto operation) {
        bool threw = false;
        try { operation(); } catch (const std::invalid_argument&) { threw = true; }
        Require(threw);
    };
    std::size_t available = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0);
    for (const int protection : {-256, -129, -128, -14, -1, 8, 0x400, std::numeric_limits<int>::min()}) {
        void* invalid = nullptr;
        rejected([&] { sceKernelMapFlexibleMemory(&invalid, length, protection, 0); });
        Require(invalid == nullptr);
        std::size_t after = 0;
        Require(sceKernelAvailableFlexibleMemorySize(&after) == 0 && after == available);
        rejected([&] { sceKernelMapDirectMemory(&invalid, length, protection, 0, physical, 0); });
        Require(invalid == nullptr);
        rejected([&] { sceKernelMtypeprotect(mapping, length, 2, protection); });
        VirtualQueryInfo info{};
        Require(sceKernelVirtualQuery(mapping, 0, &info, sizeof(info)) == 0);
        Require(info.protection == 3 && info.memory_type == 0);
        int type = -1;
        std::int64_t start = -1;
        std::int64_t end = -1;
        Require(sceKernelGetDirectMemoryType(physical, &type, &start, &end) == 0 && type == 0);
        Require(bytes[0] == 5 && bytes[length - 1] == 7);
    }
    Require(sceKernelMunmap(mapping, length) == 0);
    Require(sceKernelReleaseDirectMemory(physical, length) == 0);
}

static void CheckPosixMprotectIgnoresUndefinedBits() {
    constexpr std::size_t length = 0x4000;
    auto* mapping = static_cast<volatile unsigned char*>(mmap_nid_postfix(nullptr, length, 3, 0x1002, -1, 0));
    Require(mapping != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    Require(mprotect_nid_postfix(const_cast<unsigned char*>(mapping), length, static_cast<int>(0xfffffff2u)) == 0);
    mapping[length - 1] = 5;
    Require(mapping[length - 1] == 5);
    Require(mprotect_nid_postfix(const_cast<unsigned char*>(mapping), length, 0x8 | 0x3) == 0);
    Require(munmap_nid_postfix(const_cast<unsigned char*>(mapping), length) == 0);
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

static void CheckFlexibleFixedReplacement() {
    constexpr std::size_t page = 0x4000;
    std::size_t before = 0;
    std::size_t available = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&before) == 0);
    void* base = nullptr;
    Require(sceKernelMapFlexibleMemory(&base, page * 4, 3, 0) == 0 && base != nullptr);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - page * 4);
    auto* bytes = static_cast<unsigned char*>(base);
    for (std::size_t index = 0; index < 4; ++index) bytes[index * page] = static_cast<unsigned char>(17 + index);
    void* middle = bytes + page;
    void* replacement = middle;
    Require(sceKernelMapFlexibleMemory(&replacement, page, 3, 0x10) == 0 && replacement == middle);
    Require(bytes[0] == 17 && bytes[page] == 0 && bytes[page * 2] == 19 && bytes[page * 3] == 20);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - page * 4);
    Require(sceKernelMunmap(base, page * 4) == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before);
}

static void CheckMunmapScopedFlexibleRelease() {
    constexpr std::size_t page = 0x4000;
    std::size_t before = 0;
    std::size_t available = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&before) == 0);
    void* base = nullptr;
    Require(sceKernelMapFlexibleMemory(&base, page * 2, 3, 0) == 0 && base != nullptr);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - page * 2);
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page, 0, 0, &phys) == 0);
    void* direct = static_cast<unsigned char*>(base) + page;
    Require(sceKernelMapDirectMemory(&direct, page, 3, 0x10, phys, 0) == 0 && direct == static_cast<unsigned char*>(base) + page);
    std::size_t mid = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&mid) == 0);
    Require(sceKernelMunmap(direct, page) == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == mid);
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, page * 2, 0, 0) == 0 && reserved != nullptr);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == mid);
    Require(sceKernelMunmap(reserved, page * 2) == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == mid);
    Require(sceKernelMunmap(base, page) == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == mid + page);
    Require(sceKernelReleaseDirectMemory(phys, page) == 0);
    void* fresh = nullptr;
    Require(sceKernelMapFlexibleMemory(&fresh, page * 4, 3, 0) == 0 && fresh != nullptr);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - page * 4);
    Require(sceKernelMunmap(static_cast<unsigned char*>(fresh) + page, page * 2) == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - page * 2);
    Require(sceKernelMunmap(fresh, page) == 0);
    Require(sceKernelMunmap(static_cast<unsigned char*>(fresh) + page * 3, page) == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before);
}

static void CheckGuestModuleUsesFlexibleMemory() {
    constexpr std::size_t moduleData = 0x10000;
    const auto path = std::filesystem::relative(GUEST_MEMORY_MODULE).generic_string();
    std::size_t before = 0;
    std::size_t available = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&before) == 0);
    void* module = dlopen_nid_postfix(path.c_str(), 2);
    Require(module != nullptr);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available + moduleData <= before);
    void* mapped = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapped, before, 3, 0) == SCE_KERNEL_ERROR_ENOMEM && mapped == nullptr);
    Require(dlclose_nid_postfix(module) == 0);
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

static void CheckVirtualQueryFindsNeighbours() {
    constexpr std::size_t page = 0x4000;
    void* low = nullptr;
    void* high = nullptr;
    Require(sceKernelMapNamedFlexibleMemory(&low, page * 4, 3, 0, "neighbours") == 0 && low != nullptr);
    Require(sceKernelMapFlexibleMemory(&high, page, 3, 0) == 0 && high != nullptr);
    auto* lowBytes = static_cast<unsigned char*>(low);
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(lowBytes + page, 0, &info, sizeof(info)) == 0);
    Require(info.start == reinterpret_cast<std::uintptr_t>(low) && info.end == reinterpret_cast<std::uintptr_t>(low) + page * 4);
    Require(sceKernelMprotect(lowBytes + page, page, 1) == 0);
    Require(sceKernelVirtualQuery(lowBytes + page, 0, &info, sizeof(info)) == 0);
    Require(info.start == reinterpret_cast<std::uintptr_t>(low) + page && info.end == reinterpret_cast<std::uintptr_t>(low) + page * 2 && info.protection == 1);
    Require(sceKernelVirtualQuery(lowBytes + page * 2, 0, &info, sizeof(info)) == 0);
    Require(info.start == reinterpret_cast<std::uintptr_t>(low) + page * 2 && info.end == reinterpret_cast<std::uintptr_t>(low) + page * 4 && info.protection == 3);
    Require(sceKernelMunmap(lowBytes + page * 2, page) == 0);
    Require(sceKernelVirtualQuery(lowBytes + page * 2, 0, &info, sizeof(info)) != 0);
    Require(sceKernelVirtualQuery(lowBytes + page * 2, 1, &info, sizeof(info)) == 0);
    Require(info.start == reinterpret_cast<std::uintptr_t>(low) + page * 3 && info.end == reinterpret_cast<std::uintptr_t>(low) + page * 4);
    Require(sceKernelVirtualQuery(lowBytes + page * 3, 0, &info, sizeof(info)) == 0 && info.start == reinterpret_cast<std::uintptr_t>(low) + page * 3);
    Require(sceKernelMunmap(low, page * 2) == 0);
    Require(sceKernelMunmap(lowBytes + page * 3, page) == 0);
    Require(sceKernelMunmap(high, page) == 0);
}

static void CheckReleaseFindsEveryDirectMapping() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t pieces = 48;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * pieces, 0, 0, &phys) == 0);
    void* mapped[pieces] = {};
    for (std::size_t index = 0; index < pieces; ++index) {
        const auto piece = pieces - 1 - index;
        Require(sceKernelMapDirectMemory(&mapped[piece], page, 3, 0, phys + static_cast<std::int64_t>(piece * page), 0) == 0);
        static_cast<unsigned char*>(mapped[piece])[0] = static_cast<unsigned char>(piece);
    }
    void* alias = nullptr;
    Require(sceKernelMapDirectMemory(&alias, page * 2, 3, 0, phys + static_cast<std::int64_t>(page * 10), 0) == 0);
    Require(static_cast<unsigned char*>(alias)[0] == 10 && static_cast<unsigned char*>(alias)[page] == 11);
    Require(sceKernelReleaseDirectMemory(phys + static_cast<std::int64_t>(page * 10), page * 3) == 0);
    VirtualQueryInfo info{};
    for (std::size_t index = 0; index < pieces; ++index) {
        const bool released = index >= 10 && index < 13;
        Require((sceKernelVirtualQuery(mapped[index], 0, &info, sizeof(info)) == 0) != released);
    }
    Require(sceKernelVirtualQuery(alias, 0, &info, sizeof(info)) != 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 10) == 0);
    Require(sceKernelReleaseDirectMemory(phys + static_cast<std::int64_t>(page * 13), page * (pieces - 13)) == 0);
    for (std::size_t index = 0; index < pieces; ++index) Require(sceKernelVirtualQuery(mapped[index], 0, &info, sizeof(info)) != 0);
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
    Require(sceKernelEnableDmemAliasing() == 0);
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

static void CheckSparseDirectMemoryBacking() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t size = 256u * 1024u * 1024u;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, size, 0, 0, &physical) == 0);
    void* first = nullptr;
    void* last = nullptr;
    Require(sceKernelMapDirectMemory(&first, page, 3, 0, physical, 0) == 0);
    Require(sceKernelMapDirectMemory(&last, page, 3, 0, physical + size - page, 0) == 0);
    auto* firstBytes = static_cast<unsigned char*>(first);
    auto* lastBytes = static_cast<unsigned char*>(last);
    Require(firstBytes[0] == 0 && firstBytes[page - 1] == 0 && lastBytes[0] == 0 && lastBytes[page - 1] == 0);
    firstBytes[0] = 17;
    lastBytes[page - 1] = 29;
    void* alias = nullptr;
    Require(sceKernelMapDirectMemory(&alias, page, 3, 0, physical + size - page, 0) == 0);
    auto* aliasBytes = static_cast<unsigned char*>(alias);
    Require(aliasBytes[page - 1] == 29 && aliasBytes[0] == 0);
    aliasBytes[page - 1] = 41;
    Require(lastBytes[page - 1] == 41 && firstBytes[0] == 17);
    Require(sceKernelMunmap(last, page) == 0 && sceKernelMunmap(alias, page) == 0);
    last = nullptr;
    Require(sceKernelMapDirectMemory(&last, page, 3, 0, physical + size - page, 0) == 0);
    Require(static_cast<unsigned char*>(last)[page - 1] == 41);
    Require(sceKernelMunmap(first, page) == 0 && sceKernelMunmap(last, page) == 0);
    Require(sceKernelReleaseDirectMemory(physical, size) == 0);
}

static void CheckSavedDirectMemorySurvivesCachePressure() {
#ifdef _WIN32
    constexpr std::size_t bytes = 0x200000;
    std::array<std::int64_t, 18> retained{};
    for (std::size_t index = 0; index < retained.size(); ++index) {
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &retained[index]) == 0);
        void* mapped = nullptr;
        Require(sceKernelMapDirectMemory(&mapped, bytes, 3, 0, retained[index], bytes) == 0);
        std::memset(mapped, static_cast<int>(index + 1), bytes);
        Require(sceKernelMunmap(mapped, bytes) == 0);
    }
    const auto temporary = [] {
        constexpr std::size_t small = 0x10000;
        for (unsigned char round = 1; round <= 24; ++round) {
            std::int64_t physical = 0;
            Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, small, 0, 0, &physical) == 0);
            void* mapped = nullptr;
            Require(sceKernelMapDirectMemory(&mapped, small, 3, 0, physical, 0) == 0);
            auto* data = static_cast<unsigned char*>(mapped);
            Require(std::all_of(data, data + small, [](unsigned char value) { return value == 0; }));
            data[7] = round;
            data[small - 1] = static_cast<unsigned char>(round + 0x40);
            Require(sceKernelMunmap(mapped, small) == 0);
            mapped = nullptr;
            Require(sceKernelMapDirectMemory(&mapped, small, 3, 0, physical, 0) == 0);
            data = static_cast<unsigned char*>(mapped);
            for (std::size_t at = 0; at < small; ++at) Require(data[at] == (at == 7 ? round : at == small - 1 ? static_cast<unsigned char>(round + 0x40) : 0));
            Require(sceKernelMunmap(mapped, small) == 0);
            Require(sceKernelReleaseDirectMemory(physical, small) == 0);
        }
    };
    temporary();
    for (std::size_t index = 0; index < retained.size(); ++index) {
        void* mapped = nullptr;
        Require(sceKernelMapDirectMemory(&mapped, bytes, 3, 0, retained[index], bytes) == 0);
        const auto* data = static_cast<const unsigned char*>(mapped);
        Require(std::all_of(data, data + bytes, [&](unsigned char value) { return value == index + 1; }));
        Require(sceKernelMunmap(mapped, bytes) == 0);
        Require(sceKernelReleaseDirectMemory(retained[index], bytes) == 0);
    }
    temporary();
#endif
}

static void CheckReusedDirectMemorySections() {
#ifdef _WIN32
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t bytes = page * 4;
    const bool pooled = std::getenv("APS5_NO_DIRECT_MEMORY_SECTION_POOL") == nullptr;
    const auto allocate = [] {
        std::int64_t physical = 0;
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0);
        return physical;
    };
    const auto map = [](std::int64_t physical) {
        void* mapped = nullptr;
        Require(sceKernelMapDirectMemory(&mapped, bytes, 3, 0, physical, 0) == 0);
        return static_cast<unsigned char*>(mapped);
    };
    const auto blank = [](const unsigned char* data) { return std::all_of(data, data + bytes, [](unsigned char value) { return value == 0; }); };
    const auto handles = [] {
        DWORD count = 0;
        Require(GetProcessHandleCount(GetCurrentProcess(), &count) != 0);
        return count;
    };
    auto physical = allocate();
    auto* data = map(physical);
    std::memset(data, 0x5a, bytes);
    Require(sceKernelMunmap(data, bytes) == 0);
    auto before = handles();
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0);
    const auto released = handles();
    Require(released == (pooled ? before : before - 1));
    physical = allocate();
    Require(handles() == (pooled ? released : released + 1));
    data = map(physical);
    Require(blank(data));
    data[5] = 17;
    data[bytes - 1] = 29;
    void* second = nullptr;
    Require(sceKernelMapDirectMemory(&second, bytes, 3, 0, physical, 0) == 0);
    auto* alias = static_cast<unsigned char*>(second);
    Require(alias != data && alias[5] == 17 && alias[6] == 0 && alias[bytes - 1] == 29);
    alias[page + 3] = 41;
    Require(data[page + 3] == 41);
    data[page * 2] = 53;
    Require(alias[page * 2] == 53);
    Require(sceKernelMunmap(alias, bytes) == 0);
    Require(data[5] == 17 && data[page + 3] == 41 && data[page * 2] == 53);
    Require(sceKernelMunmap(data, bytes) == 0);
    data = map(physical);
    Require(data[5] == 17 && data[page + 3] == 41 && data[page * 2] == 53 && data[bytes - 1] == 29);
    Require(sceKernelMunmap(data, bytes) == 0);
    before = handles();
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0);
    Require(handles() == before - 1);
    for (unsigned char round = 1; round <= 8; ++round) {
        physical = allocate();
        data = map(physical);
        Require(blank(data));
        std::memset(data, round, bytes);
        if (round % 2 == 0) {
            second = nullptr;
            Require(sceKernelMapDirectMemory(&second, page, 3, 0, physical + static_cast<std::int64_t>(page), 0) == 0);
            Require(static_cast<unsigned char*>(second)[page - 1] == round);
            Require(sceKernelMunmap(second, page) == 0);
        }
        Require(sceKernelMunmap(data, bytes) == 0);
        Require(sceKernelReleaseDirectMemory(physical, bytes) == 0);
    }
#endif
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
    const int droppedResult = sceKernelVirtualQuery(static_cast<unsigned char*>(mapped) + page, 0, &dropped, sizeof(dropped));
    Require(droppedResult != 0 || !dropped.is_direct);
    void* alias = nullptr;
    Require(sceKernelMapDirectMemory(&alias, page, 3, 0, phys, 0) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page) == 0);
    for (void* view : {mapped, alias}) {
        VirtualQueryInfo cleared{};
        Require(sceKernelVirtualQuery(view, 0, &cleared, sizeof(cleared)) != 0);
        VirtualQueryInfo next{};
        if (sceKernelVirtualQuery(view, 1, &next, sizeof(next)) == 0) Require(next.start != reinterpret_cast<std::uintptr_t>(view));
    }
}

static void CheckReleaseDirectMemoryRejectsInvalidRanges() {
    constexpr std::size_t page = 0x4000;
    Require(sceKernelReleaseDirectMemory(-1, page) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelReleaseDirectMemory(0, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelReleaseDirectMemory(0, 1) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelReleaseDirectMemory(0, page - 1) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelReleaseDirectMemory(1, page) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelReleaseDirectMemory(0, std::numeric_limits<std::size_t>::max()) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelReleaseDirectMemory(0, DIRECT_MEMORY_SIZE + page) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelReleaseDirectMemory(DIRECT_MEMORY_SIZE - page, page * 2) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelReleaseDirectMemory(DIRECT_MEMORY_SIZE, page) == SCE_KERNEL_ERROR_EINVAL);
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

static void CheckFixedMappingOverHoleInsideAllocation() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 4, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 4, 3, 0, phys, 0) == 0);
    auto* bytes = static_cast<unsigned char*>(mapped);
    bytes[7] = 0x11;
    bytes[page * 3 + 7] = 0x44;
    Require(sceKernelMunmap(bytes + page, page) == 0);
    void* fixed = bytes + page;
    Require(sceKernelMapDirectMemory(&fixed, page * 2, 3, 0x10, phys + static_cast<std::int64_t>(page), 0) == 0);
    Require(fixed == bytes + page);
    bytes[page * 2 + 7] = 0x33;
    Require(bytes[7] == 0x11 && bytes[page * 2 + 7] == 0x33 && bytes[page * 3 + 7] == 0x44);
    Require(sceKernelMunmap(bytes + page * 2, page * 2) == 0);
    Require(sceKernelMunmap(mapped, page * 4) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 4) == 0);
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
    KernelMemoryPoolBatchEntry poolEntry{};
    poolEntry.op = 4;
    poolEntry.type_protect.addr = bytes + page * 2;
    poolEntry.type_protect.len = page;
    poolEntry.type_protect.prot = 1;
    poolEntry.type_protect.type = 4;
    processed = -1;
    Require(sceKernelMemoryPoolBatch(&poolEntry, 1, &processed, 0) == 0 && processed == 1);
    check(1, 3, 1);
    check(2, 4, 1);
    Require(sceKernelMemoryPoolCommit(bytes + page * 2, page, 6, 3, 0) == 0);
    check(2, 6, 3);
    check(1, 3, 1);
    KernelMemoryPoolBatchEntry commitEntry{};
    commitEntry.op = 1;
    commitEntry.commit.addr = bytes + page * 2;
    commitEntry.commit.len = page;
    commitEntry.commit.prot = 1;
    commitEntry.commit.type = 7;
    processed = -1;
    Require(sceKernelMemoryPoolBatch(&commitEntry, 1, &processed, 0) == 0 && processed == 1);
    check(1, 3, 1);
    check(2, 7, 1);
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

static bool GpuMapped(const void* pointer) {
    GuestAllocations::Mutation mutation;
    return mutation.Find(pointer).gpu;
}

static void CheckGpuAccessFollowsProtection() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    void* cpu = nullptr;
    Require(sceKernelMapDirectMemory(&cpu, page, 3, 0, phys, 0) == 0);
    Require(!GpuMapped(cpu));
    void* gpu = nullptr;
    Require(sceKernelMapDirectMemory(&gpu, page, 0x32, 0, phys + static_cast<std::int64_t>(page), 0) == 0);
    Require(GpuMapped(gpu));
    Require(sceKernelMprotect(cpu, page, 0x13) == 0);
    Require(GpuMapped(cpu));
    Require(sceKernelMprotect(gpu, page, 3) == 0);
    Require(!GpuMapped(gpu));
    static int imageProbe = 0;
    const auto probe = reinterpret_cast<std::uintptr_t>(&imageProbe);
    {
        GuestAllocations::Mutation mutation;
        mutation.RegisterMainImage();
    }
    {
        const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        const auto image = std::find_if(lease.begin(), lease.end(), [&](const auto& range) { return probe >= range->address && probe - range->address < range->bytes; });
        Require(image != lease.end() && !(*image)->releasable && !(*image)->gpu);
    }
    Require(sceKernelMunmap(cpu, page) == 0);
    Require(sceKernelMunmap(gpu, page) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
    void* flexible = nullptr;
    Require(sceKernelMapNamedFlexibleMemory(&flexible, page, 3, 0, "cpu") == 0);
    Require(!GpuMapped(flexible));
    Require(sceKernelMunmap(flexible, page) == 0);
    Require(sceKernelMapNamedFlexibleMemory(&flexible, page, 0x33, 0, "gpu") == 0);
    Require(GpuMapped(flexible));
    Require(sceKernelMunmap(flexible, page) == 0);
}

static std::vector<std::pair<std::uintptr_t, std::size_t>> gpuMapped;

static std::atomic<bool> observerEntered{false};
static std::atomic<bool> observerDone{false};

static void CheckClearingTheObserverWaitsForItsCalls() {
    GuestAllocations::GuestAllocationsSetGpuMapObserver_nid_postfix([](const GuestAllocations::Mapped&, std::uint64_t) {
        observerEntered = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        observerDone = true;
    });
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page, 0, 0, &phys) == 0);
    void* gpu = nullptr;
    std::thread mapper([&] { Require(sceKernelMapDirectMemory(&gpu, page, 0x32, 0, phys, 0) == 0); });
    while (!observerEntered) std::this_thread::yield();
    GuestAllocations::GuestAllocationsSetGpuMapObserver_nid_postfix(nullptr);
    Require(observerDone);
    mapper.join();
    Require(sceKernelMunmap(gpu, page) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page) == 0);
}

static void CheckGpuMapsAreObserved() {
    GuestAllocations::GuestAllocationsSetGpuMapObserver_nid_postfix([](const GuestAllocations::Mapped& mapped, std::uint64_t) {
        const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        for (const auto& weak : mapped) {
            const auto range = weak.lock();
            if (range == nullptr) continue;
            const bool registered = std::any_of(lease.begin(), lease.end(), [&](const auto& current) { return current == range; });
            gpuMapped.emplace_back(registered && range->gpu ? range->address : 0, range->bytes);
        }
    });
    constexpr std::size_t page = 0x4000;
    void* heap = GuestHeap::GuestHeapAlign_nid_postfix(page, page);
    Require(heap != nullptr && gpuMapped.empty());
    GuestHeap::GuestHeapFree_nid_postfix(heap);
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    void* cpu = nullptr;
    Require(sceKernelMapDirectMemory(&cpu, page, 3, 0, phys, 0) == 0);
    Require(gpuMapped.empty());
    void* gpu = nullptr;
    Require(sceKernelMapDirectMemory(&gpu, page, 0x32, 0, phys + static_cast<std::int64_t>(page), 0) == 0);
    Require(gpuMapped.size() == 1 && gpuMapped.front().first == reinterpret_cast<std::uintptr_t>(gpu) && gpuMapped.front().second == page);
    Require(sceKernelMprotect(cpu, page, 0x13) == 0);
    Require(gpuMapped.size() == 1);
    std::int64_t remapPhys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &remapPhys) == 0);
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, page * 2, 0, 0) == 0);
    void* cpuRemap = reserved;
    Require(sceKernelMapDirectMemory(&cpuRemap, page, 3, 0x10, remapPhys, 0) == 0 && cpuRemap == reserved);
    Require(gpuMapped.size() == 1);
    void* gpuRemap = static_cast<unsigned char*>(reserved) + page;
    void* const gpuTarget = gpuRemap;
    Require(sceKernelMapDirectMemory(&gpuRemap, page, 0x32, 0x10, remapPhys + static_cast<std::int64_t>(page), 0) == 0 && gpuRemap == gpuTarget);
    Require(gpuMapped.size() == 2 && gpuMapped.back().first == reinterpret_cast<std::uintptr_t>(gpuTarget) && gpuMapped.back().second == page);
    GuestAllocations::GuestAllocationsSetGpuMapObserver_nid_postfix(nullptr);
    Require(sceKernelMunmap(reserved, page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(remapPhys, page * 2) == 0);
    Require(sceKernelMunmap(cpu, page) == 0);
    Require(sceKernelMunmap(gpu, page) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
}

static void CheckHintedVirtualReservation() {
    constexpr std::size_t page = 0x4000;
    void* probe = nullptr;
    Require(sceKernelReserveVirtualRange(&probe, page * 8, 0, 0) == 0);
    Require(sceKernelMunmap(probe, page * 8) == 0);
    void* const hint = static_cast<unsigned char*>(probe) + page * 2;
    void* placed = hint;
    Require(sceKernelReserveVirtualRange(&placed, page * 2, 0, 0) == 0);
    Require(placed == hint);
    void* above = hint;
    Require(sceKernelReserveVirtualRange(&above, page * 2, 0, 0) == 0);
    Require(above > hint);
    Require(sceKernelMunmap(above, page * 2) == 0);
    Require(sceKernelMunmap(placed, page * 2) == 0);
}

static void CheckReservationHintWithoutRoomAbove() {
    constexpr std::size_t page = 0x4000;
    std::uintptr_t base = 0;
    std::size_t size = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &size);
    const auto end = base + size;
    void* aboveTheArena = reinterpret_cast<void*>(end + 0x40000000);
    Require(sceKernelReserveVirtualRange(&aboveTheArena, page * 2, 0, 0) == 0);
    Require(GuestArena::GuestArenaContains_nid_postfix(aboveTheArena, page * 2));
    void* const lastPage = reinterpret_cast<void*>(end - page);
    void* belowTheHint = lastPage;
    Require(sceKernelReserveVirtualRange(&belowTheHint, page * 2, 0, 0) == 0);
    Require(belowTheHint < lastPage && GuestArena::GuestArenaContains_nid_postfix(belowTheHint, page * 2));
    Require(sceKernelMunmap(belowTheHint, page * 2) == 0);
    Require(sceKernelMunmap(aboveTheArena, page * 2) == 0);
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

static void CheckHintInsideReservation() {
    constexpr std::size_t page = 0x4000;
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, page * 4, 0, 0) == 0);
    auto* base = static_cast<unsigned char*>(reserved);
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 3, 0, 0, &phys) == 0);
    void* direct = base + page;
    Require(sceKernelMapDirectMemory(&direct, page, 3, 0, phys, 0) == 0);
    Require(direct == base + page);
    static_cast<unsigned char*>(direct)[0] = 21;
    void* flexible = base + page * 2;
    Require(sceKernelMapFlexibleMemory(&flexible, page, 3, 0) == 0);
    Require(flexible == base + page * 2);
    static_cast<unsigned char*>(flexible)[0] = 22;
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(direct, 0, &info, sizeof(info)) == 0);
    Require(info.is_direct && info.offset == static_cast<std::uint64_t>(phys));
    Require(sceKernelVirtualQuery(flexible, 0, &info, sizeof(info)) == 0);
    Require(info.is_flexible);
    Require(sceKernelVirtualQuery(base + page * 3, 0, &info, sizeof(info)) == 0);
    Require(!info.is_committed && info.protection == 0);
    void* overlapping = base + page;
    Require(sceKernelMapDirectMemory(&overlapping, page * 2, 3, 0, phys + page, 0) == 0);
    Require(overlapping != base + page);
    Require(static_cast<unsigned char*>(direct)[0] == 21 && static_cast<unsigned char*>(flexible)[0] == 22);
    Require(sceKernelMunmap(overlapping, page * 2) == 0);
    Require(sceKernelMunmap(reserved, page * 4) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 3) == 0);
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
constexpr std::uint32_t ReadWriteProtection = PAGE_READWRITE;
#else
constexpr std::uint32_t ReadWriteProtection = 0x04;
#endif

static void CheckNoOverwriteRejectsHostOccupiedMapping() {
    constexpr std::size_t page = 0x4000;
    void* reservation = nullptr;
    Require(sceKernelReserveVirtualRange(&reservation, page * 4, 0, 0) == 0);
    Require(sceKernelMunmap(reservation, page * 4) == 0);
    void* target = static_cast<unsigned char*>(reservation) + page;
    GuestArena::GuestArenaCommit_nid_postfix(target, page, ReadWriteProtection, page);
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

#ifdef _WIN32
static DWORD ImageProtection(const void* pointer) {
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(pointer, &memory, sizeof(memory)) == sizeof(memory));
    return memory.Protect & 0xffu;
}
#endif

static unsigned char* GuestModulePage(void* module, std::size_t page) {
    auto* data = static_cast<unsigned char*>(dlsym_nid_postfix(module, "guestMemoryModuleData"));
    Require(data != nullptr);
    auto* target = reinterpret_cast<unsigned char*>((reinterpret_cast<std::uintptr_t>(data) + page - 1) & ~(page - 1));
    Require(target + page <= data + 0x10000);
    return target;
}

static bool RegisteredGuestRange(const void* pointer, std::size_t bytes) {
    GuestAllocations::Mutation mutation;
    return mutation.Overlaps(pointer, bytes);
}

#ifdef _WIN32
static void CheckGuestModuleImageProtection() {
    constexpr std::size_t page = 0x4000;
    const auto path = std::filesystem::relative(GUEST_MEMORY_ELF0_MODULE).generic_string();
    void* module = dlopen_nid_postfix(path.c_str(), 2);
    Require(module != nullptr);
    auto* target = GuestModulePage(module, page);
    Require(!RegisteredGuestRange(target, page));
    Require(sceKernelMprotect(target, page, 1) == 0);
    Require(RegisteredGuestRange(target, page));
    Require(ImageProtection(target) == PAGE_READONLY);
    Require(sceKernelMprotect(target, page, 3) == 0);
    Require(ImageProtection(target) == PAGE_READWRITE || ImageProtection(target) == PAGE_WRITECOPY);
    target[1] = 7;
    Require(target[1] == 7);
    Require(sceKernelMprotect(target, page, 1) == 0);
    Require(ImageProtection(target) == PAGE_READONLY);

    {
        auto held = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        bool failed = false;
        try {
            dlclose_nid_postfix(module);
        } catch (const std::runtime_error&) {
            failed = true;
        }
        Require(failed);
        Require(GuestModulePage(module, page) == target);
        Require(RegisteredGuestRange(target, page));
        Require(ImageProtection(target) == PAGE_READONLY);
    }

    auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    std::atomic<bool> closed{false};
    std::thread closer([&] {
        Require(dlclose_nid_postfix(module) == 0);
        closed = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(!closed);
    Require(ImageProtection(target) == PAGE_READONLY);
    lease.clear();
    closer.join();
    Require(!RegisteredGuestRange(target, page));
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(target, page, true, true, false);
        mutation.Remove(target);
    }

    module = dlopen_nid_postfix(path.c_str(), 2);
    Require(module != nullptr);
    target = GuestModulePage(module, page);
    Require(!RegisteredGuestRange(target, page));
    Require(sceKernelMprotect(target, page, 1) == 0);
    Require(ImageProtection(target) == PAGE_READONLY);
    Require(RegisteredGuestRange(target, page));
    Require(dlclose_nid_postfix(module) == 0);
    Require(!RegisteredGuestRange(target, page));

    bool rejected = false;
    try {
        sceKernelMprotect(GetModuleHandleA("kernel32.dll"), page, 1);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected);

    rejected = false;
    try {
        const auto libkernel = GetModuleHandleA("libkernel.prx");
        Require(libkernel != nullptr);
        sceKernelMprotect(libkernel, page, 1);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected);

    const auto prxPath = std::filesystem::path(path).parent_path() / "guest_memory_module_foreign.prx";
    std::filesystem::copy_file(path, prxPath, std::filesystem::copy_options::overwrite_existing);
    void* foreignModule = dlopen_nid_postfix(prxPath.generic_string().c_str(), 2);
    Require(foreignModule != nullptr);
    auto* foreignTarget = GuestModulePage(foreignModule, page);
    Require(!RegisteredGuestRange(foreignTarget, page));
    Require(sceKernelMprotect(foreignTarget, page, 1) == 0);
    Require(ImageProtection(foreignTarget) == PAGE_READONLY);
    Require(RegisteredGuestRange(foreignTarget, page));
    Require(sceKernelMprotect(foreignTarget, page, 3) == 0);
    Require(ImageProtection(foreignTarget) == PAGE_READWRITE || ImageProtection(foreignTarget) == PAGE_WRITECOPY);
    Require(dlclose_nid_postfix(foreignModule) == 0);
    Require(!RegisteredGuestRange(foreignTarget, page));
    std::filesystem::remove(prxPath);

    const auto plainPath = std::filesystem::relative(GUEST_MEMORY_MODULE).generic_string();
    void* plainModule = dlopen_nid_postfix(plainPath.c_str(), 2);
    Require(plainModule != nullptr);
    auto* plainTarget = GuestModulePage(plainModule, page);
    rejected = false;
    try {
        sceKernelMprotect(plainTarget, page, 1);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected);
    Require(!RegisteredGuestRange(plainTarget, page));
    Require(dlclose_nid_postfix(plainModule) == 0);
}
#elif defined(__linux__)
static std::string MappingPermissions(const void* pointer) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        unsigned long begin = 0;
        unsigned long end = 0;
        char permissions[5]{};
        if (std::sscanf(line.c_str(), "%lx-%lx %4s", &begin, &end, permissions) == 3 && address >= begin && address < end) return permissions;
    }
    return {};
}

static void CheckGuestModuleImageProtection() {
    constexpr std::size_t page = 0x4000;
    const auto path = std::filesystem::relative(GUEST_MEMORY_MODULE).generic_string();
    void* module = dlopen_nid_postfix(path.c_str(), 2);
    Require(module != nullptr);
    auto* target = GuestModulePage(module, page);
    Require(!RegisteredGuestRange(target, page));
    Require(sceKernelMprotect(target, page, 1) == 0);
    Require(RegisteredGuestRange(target, page));
    Require(MappingPermissions(target).starts_with("r--"));
    Require(sceKernelMprotect(target, page, 3) == 0);
    Require(MappingPermissions(target).starts_with("rw-"));
    target[1] = 7;
    Require(target[1] == 7);
    Require(dlclose_nid_postfix(module) == 0);
    Require(!RegisteredGuestRange(target, page));

    module = dlopen_nid_postfix(path.c_str(), 2);
    Require(module != nullptr);
    target = GuestModulePage(module, page);
    Require(!RegisteredGuestRange(target, page));
    Require(sceKernelMprotect(target, page, 1) == 0);
    Require(RegisteredGuestRange(target, page));
    Require(dlclose_nid_postfix(module) == 0);
    Require(!RegisteredGuestRange(target, page));

    bool rejected = false;
    try {
        sceKernelMprotect(reinterpret_cast<const void*>(&::getpid), page, 1);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected);
}
#endif

static void CheckUnhintedKernelMappingsLandInTheWindow() {
    constexpr std::size_t page = 0x4000;
    std::uintptr_t windowStart = 0;
    std::size_t windowSize = 0;
    GuestArena::GuestArenaRange_nid_postfix(&windowStart, &windowSize);
    const auto windowEnd = windowStart + windowSize;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page, 0, 0, &phys) == 0);
    void* direct = nullptr;
    Require(sceKernelMapDirectMemory(&direct, page, 3, 0, phys, 0) == 0);
    Require(direct != nullptr);
    const auto directAddress = reinterpret_cast<std::uintptr_t>(direct);
    Require(directAddress >= windowStart);
    Require(directAddress + page <= windowEnd);
    void* flexible = nullptr;
    Require(sceKernelMapNamedFlexibleMemory(&flexible, page, 3, 0, "arena-window-test") == 0);
    Require(flexible != nullptr);
    const auto flexibleAddress = reinterpret_cast<std::uintptr_t>(flexible);
    Require(flexibleAddress >= windowStart);
    Require(flexibleAddress + page <= windowEnd);
    Require(sceKernelMunmap(direct, page) == 0);
    Require(sceKernelMunmap(flexible, page) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page) == 0);
}

#if defined(__linux__)
static void CheckUnmappedArenaRangeStaysReserved() {
    constexpr std::size_t page = 0x4000;
    void* address = nullptr;
    Require(sceKernelMapNamedFlexibleMemory(&address, page, 3, 0, "reserved-after-unmap") == 0);
    Require(address != nullptr);
    Require(sceKernelMunmap(address, page) == 0);
    void* hostile = mmap(address, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    Require(hostile == MAP_FAILED && errno == EEXIST);
    void* again = nullptr;
    Require(sceKernelMapNamedFlexibleMemory(&again, page, 3, 0, "reserved-after-unmap-2") == 0);
    Require(again != nullptr);
    Require(sceKernelMunmap(again, page) == 0);
}
#endif

static void CheckArenaPlacesUnhintedMappingsInsideTheWindow() {
    constexpr std::size_t page = 0x4000;
    std::uintptr_t base = 0;
    std::size_t size = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &size);
    Require(size != 0);
    auto* placed = GuestArena::GuestArenaAllocate_nid_postfix(page, page);
    Require(placed != nullptr);
    const auto address = reinterpret_cast<std::uintptr_t>(placed);
    Require(address >= base);
    Require(address + page <= base + size);
    GuestArena::GuestArenaRelease_nid_postfix(placed, page);
}

static void CheckArenaReusesAFreedRange() {
    constexpr std::size_t page = 0x4000;
    auto* first = GuestArena::GuestArenaAllocate_nid_postfix(page, page);
    Require(first != nullptr);
    GuestArena::GuestArenaRelease_nid_postfix(first, page);
    auto* second = GuestArena::GuestArenaAllocate_nid_postfix(page, page);
    Require(second == first);
    GuestArena::GuestArenaRelease_nid_postfix(second, page);
}

static void CheckArenaHonoursHintsWithoutGoingBelowThem() {
    constexpr std::size_t page = 0x4000;
    std::uintptr_t base = 0;
    std::size_t size = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &size);
    const auto hint = base + page;
    auto* placed = GuestArena::GuestArenaAllocateAtOrAbove_nid_postfix(hint, page, page);
    Require(placed != nullptr);
    Require(reinterpret_cast<std::uintptr_t>(placed) >= hint);
    Require(GuestArena::GuestArenaContains_nid_postfix(placed, page));
    GuestArena::GuestArenaRelease_nid_postfix(placed, page);
}

static void CheckFixedMappingsReachTheApplicationAreaEnd() {
    constexpr std::size_t page = 0x4000;
    std::uintptr_t base = 0;
    std::size_t size = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &size);
    constexpr std::uintptr_t expectedOffset =
#ifdef ANYPS5_ENABLE_ASAN
        0x0000100000000000ull;
#else
        0;
#endif
    constexpr std::uintptr_t applicationAreaEnd = 0xFC00000000ull + expectedOffset;
    constexpr std::uintptr_t expectedBase =
#ifdef __APPLE__
        0x200000000ull + expectedOffset;
#else
        expectedOffset == 0 ? 0x10000ull : 0x200000000ull + expectedOffset;
#endif
    Require(base == expectedBase && base + size == applicationAreaEnd);
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
#ifdef _WIN32

static void CheckAliasOfReorderedSharedPages() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 2, 3, 0, phys, 0) == 0);
    auto* bytes = static_cast<volatile unsigned char*>(mapped);
    bytes[5] = 0x11;
    bytes[page + 5] = 0x22;
    void* low = mapped;
    Require(sceKernelMapDirectMemory(&low, page, 3, 0x10, phys + page, 0) == 0);
    void* high = static_cast<unsigned char*>(mapped) + page;
    Require(sceKernelMapDirectMemory(&high, page, 3, 0x10, phys, 0) == 0);
    Require(bytes[5] == 0x22 && bytes[page + 5] == 0x11);
    auto* alias = static_cast<volatile unsigned char*>(GuestArena::GuestArenaMapAlias_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), page * 2));
    Require(alias != nullptr && alias[5] == 0x22 && alias[page + 5] == 0x11);
    alias[6] = 0x33;
    alias[page + 6] = 0x44;
    Require(bytes[6] == 0x33 && bytes[page + 6] == 0x44);
    GuestArena::GuestArenaUnmapAlias_nid_postfix(const_cast<unsigned char*>(alias));
    Require(bytes[5] == 0x22 && bytes[page + 5] == 0x11);
    Require(sceKernelMunmap(mapped, page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
}

static void CheckAliasAcrossDirectAllocations() {
    constexpr std::size_t page = 0x4000;
    std::int64_t first = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &first) == 0);
    std::int64_t second = 0;
    Require(sceKernelAllocateDirectMemory(first + page * 2, first + page * 3, page, 0, 0, &second) == 0 && second == first + page * 2);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 3, 3, 0, first, 0) == 0);
    auto* bytes = static_cast<volatile unsigned char*>(mapped);
    for (std::size_t index = 0; index < 3; ++index) bytes[page * index + 5] = static_cast<unsigned char>(0x11 * (index + 1));
    auto* alias = static_cast<unsigned char*>(GuestArena::GuestArenaMapAlias_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), page * 3));
    Require(alias != nullptr);
    MEMORY_BASIC_INFORMATION run{};
    Require(VirtualQuery(alias, &run, sizeof(run)) == sizeof(run) && run.RegionSize == page * 2);
    Require(VirtualQuery(alias + page * 2, &run, sizeof(run)) == sizeof(run) && run.AllocationBase == alias + page * 2 && run.RegionSize == page);
    auto* through = static_cast<volatile unsigned char*>(alias);
    for (std::size_t index = 0; index < 3; ++index) {
        Require(through[page * index + 5] == 0x11 * (index + 1));
        through[page * index + 6] = static_cast<unsigned char>(0x44 + index);
        Require(bytes[page * index + 6] == 0x44 + index);
    }
    GuestArena::GuestArenaUnmapAlias_nid_postfix(alias);
    for (std::size_t offset = 0; offset < page * 3; offset += page) {
        MEMORY_BASIC_INFORMATION released{};
        Require(VirtualQuery(alias + offset, &released, sizeof(released)) == sizeof(released) && released.State == MEM_FREE);
    }
    Require(bytes[5] == 0x11 && bytes[page + 5] == 0x22 && bytes[page * 2 + 5] == 0x33);
    Require(sceKernelMunmap(mapped, page * 3) == 0);
    Require(sceKernelReleaseDirectMemory(first, page * 3) == 0);
}

static void CheckAliasLeavesResidentRunsUnderAHostWrite() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 2, 3, 0, phys, 0) == 0);
    auto* bytes = static_cast<volatile unsigned char*>(mapped);
    bytes[5] = 0x11;
    {
        const GuestArena::HostWrite open(static_cast<unsigned char*>(mapped) + page, page);
        Require(open.Open());
        Require(GuestArena::GuestArenaMapAlias_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), page * 2) == nullptr);
        bytes[page + 5] = 0x22;
    }
    auto* alias = static_cast<volatile unsigned char*>(GuestArena::GuestArenaMapAlias_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), page * 2));
    Require(alias != nullptr && alias[5] == 0x11 && alias[page + 5] == 0x22);
    GuestArena::GuestArenaUnmapAlias_nid_postfix(const_cast<unsigned char*>(alias));
    Require(bytes[5] == 0x11 && bytes[page + 5] == 0x22);
    Require(sceKernelMunmap(mapped, page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
}

static void CheckAliasRefusesPagesThatAreNotSharedViews() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page, 0, 0, &phys) == 0);
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, page * 2, 0, 0) == 0);
    void* shared = reserved;
    Require(sceKernelMapDirectMemory(&shared, page, 3, 0x10, phys, 0) == 0);
    void* privatePage = static_cast<unsigned char*>(reserved) + page;
    Require(sceKernelMapFlexibleMemory(&privatePage, page, 3, 0x10) == 0);
    bool refused = false;
    try { GuestArena::GuestArenaMapAlias_nid_postfix(reinterpret_cast<std::uintptr_t>(reserved), page * 2); } catch (const std::runtime_error&) { refused = true; }
    Require(refused);
    Require(sceKernelMunmap(reserved, page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page) == 0);
}
#endif

#if defined(__linux__) && !defined(ANYPS5_ENABLE_ASAN)
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
#if defined(__linux__) && !defined(ANYPS5_ENABLE_ASAN)
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
#elif defined(__linux__) && !defined(ANYPS5_ENABLE_ASAN)
    Require(LockedKilobytes() - lockedBefore == length / 1024);
#endif
    Require(sceKernelMlock_nid_postfix(mapped, length) == 0);
    Require(sceKernelMlock_nid_postfix(mapped, length) == 0);
    bytes[length - 1] = 7;
    Require(sceKernelMlock_nid_postfix(reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - page + 1), page * 2) == invalid);
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, page, 0, 0) == 0);
#ifndef ANYPS5_ENABLE_ASAN
    Require(sceKernelMlock_nid_postfix(reserved, page) == outOfMemory);
#endif
    Require(sceKernelMunmap(reserved, page) == 0);
    Require(sceKernelMunmap(mapped, length) == 0);
#ifndef ANYPS5_ENABLE_ASAN
    Require(sceKernelMlock_nid_postfix(mapped, page) == outOfMemory);
#endif
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

#ifdef _WIN32
static int g_accessViolations = 0;

static LONG WINAPI CountAccessViolations(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) ++g_accessViolations;
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

static void CheckSingleDirectMappingTracksWritesWithoutFaults() {
#ifdef _WIN32
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 4, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 4, 3, 0, phys, 0) == 0);
    const auto collect = [](void* address, std::size_t bytes) {
        std::array<void*, 32> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(address), bytes, pages.data(), &count, true));
        return count;
    };
    auto* bytes = static_cast<volatile unsigned char*>(mapped);
    Require(collect(mapped, page * 4) == 16);
    Require(collect(mapped, page * 4) == 0);
    void* const handler = AddVectoredExceptionHandler(1, CountAccessViolations);
    Require(handler != nullptr);
    bytes[page + 0x1000] = 7;
    bytes[page * 3] = 9;
    Require(RemoveVectoredExceptionHandler(handler) != 0);
    Require(g_accessViolations == 0);
    Require(collect(mapped, page * 4) == 2);
    Require(collect(mapped, page * 4) == 0);
    Require(sceKernelMunmap(static_cast<unsigned char*>(mapped) + page * 2, page) == 0);
    Require(bytes[page + 0x1000] == 7 && bytes[page * 3] == 9);
    bytes[0] = 5;
    Require(collect(mapped, page * 2) != 0);
    Require(collect(mapped, page * 2) == 0);
    void* second = nullptr;
    Require(sceKernelMapDirectMemory(&second, page, 3, 0, phys + page, 0) == 0);
    Require(static_cast<unsigned char*>(second)[0x1000] == 7);
    static_cast<volatile unsigned char*>(second)[0x1001] = 11;
    Require(bytes[page + 0x1001] == 11);
    Require(collect(mapped, page * 2) != 0);
    Require(sceKernelMunmap(second, page) == 0);
    Require(sceKernelMunmap(mapped, page * 2) == 0);
    Require(sceKernelMunmap(static_cast<unsigned char*>(mapped) + page * 3, page) == 0);
    void* again = nullptr;
    Require(sceKernelMapDirectMemory(&again, page * 4, 3, 0, phys, 0) == 0);
    const auto* data = static_cast<const unsigned char*>(again);
    Require(data[0] == 5 && data[page + 0x1000] == 7 && data[page + 0x1001] == 11 && data[page * 2] == 0 && data[page * 3] == 9);
    Require(sceKernelMunmap(again, page * 4) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 4) == 0);
#endif
}

static void CheckLargeResidentWritesStopFaulting() {
#ifdef _WIN32
    constexpr std::size_t bytes = 0x200000;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, bytes, 3, 0, physical, bytes) == 0);
    const auto collect = [&] {
        std::array<void*, bytes / 4096> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped), bytes, pages.data(), &count, true));
        return count;
    };
    Require(collect() == bytes / 4096);
    Require(collect() == 0 && collect() == 0);
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(mapped, &memory, sizeof(memory)) == sizeof(memory) && memory.Protect == PAGE_READONLY);
    auto* data = static_cast<unsigned char*>(mapped);
    const auto path = std::filesystem::path("anyps5-resident-read-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
    const std::vector<char> input(65536, 89);
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write(input.data(), static_cast<std::streamsize>(input.size()));
    }
    const int fd = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(fd >= 0);
    Require(sceKernelPread(fd, mapped, input.size(), 0) == static_cast<std::int64_t>(input.size()));
    Require(data[0] == 89 && data[input.size() - 1] == 89);
    Require(collect() == input.size() / 4096 && collect() == 0);
    Require(sceKernelRead(fd, data + input.size(), input.size()) == static_cast<std::int64_t>(input.size()));
    Require(data[input.size()] == 89 && data[input.size() * 2 - 1] == 89);
    Require(collect() == input.size() / 4096 && collect() == 0);
    Require(sceKernelClose(fd) == 0);
    std::filesystem::remove(path);
    {
        GuestArena::HostImportAccess access(mapped, bytes);
        Require(VirtualQuery(mapped, &memory, sizeof(memory)) == sizeof(memory) && memory.Protect == PAGE_READWRITE);
        Require(collect() == 0 && collect() == 0);
        Require(VirtualQuery(mapped, &memory, sizeof(memory)) == sizeof(memory) && memory.Protect == PAGE_READWRITE);
        data[17] = 3;
        Require(collect() == 1 && collect() == 0);
    }
    for (unsigned int round = 0; round < 16; ++round) Require(collect() == 0);
    Require(VirtualQuery(mapped, &memory, sizeof(memory)) == sizeof(memory) && memory.Protect == PAGE_READONLY);
    const auto before = g_accessViolations;
    const auto handler = AddVectoredExceptionHandler(1, CountAccessViolations);
    Require(handler != nullptr);
    for (unsigned int round = 0; round < 16; ++round) {
        for (std::size_t at = 0; at < bytes; at += 4096) static_cast<volatile unsigned char*>(mapped)[at] = static_cast<unsigned char>(round);
        Require(collect() == bytes / 4096 && collect() == 0);
    }
    Require(RemoveVectoredExceptionHandler(handler));
    Require(g_accessViolations == before + 1);
    Require(VirtualQuery(mapped, &memory, sizeof(memory)) == sizeof(memory) && memory.Protect == PAGE_READWRITE);
    Require(mprotect_nid_postfix(mapped, bytes, 1) == 0);
    Require(collect() == 0 && collect() == 0);
    Require(mprotect_nid_postfix(mapped, bytes, 3) == 0);
    data[0x10000] = 97;
    Require(collect() == 1 && collect() == 0);
    Require(sceKernelMunmap(mapped, bytes) == 0);
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0);
#endif
}

static void CheckImportAccessDoesNotInventSharedWrites() {
#ifdef _WIN32
    constexpr std::size_t bytes = 0x100000;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0);
    void* first = nullptr;
    void* second = nullptr;
    Require(sceKernelMapDirectMemory(&first, bytes, 3, 0, physical, 0) == 0);
    Require(sceKernelMapDirectMemory(&second, bytes, 3, 0, physical, 0) == 0);
    const auto collect = [&](void* pointer) {
        std::array<void*, bytes / 4096> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(pointer), bytes, pages.data(), &count, true));
        return count;
    };
    collect(first);
    collect(second);
    for (unsigned int round = 0; round < 16; ++round) {
        {
            GuestArena::HostImportAccess access(first, bytes);
            Require(collect(first) == 0 && collect(second) == 0);
        }
        Require(collect(first) == 0 && collect(second) == 0);
    }
    {
        GuestArena::HostImportAccess access(first, bytes);
        static_cast<volatile unsigned char*>(first)[4097] = 73;
        Require(collect(first) == 1 && collect(second) == 1);
    }
    Require(static_cast<unsigned char*>(second)[4097] == 73);
    Require(collect(first) == 0 && collect(second) == 0);
    Require(sceKernelMunmap(first, bytes) == 0);
    Require(sceKernelMunmap(second, bytes) == 0);
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0);
#endif
}

static void CheckResidentCollectPreservesPartialWrites() {
#ifdef _WIN32
    constexpr std::size_t bytes = 0x210000;
    constexpr std::size_t small = 0x1000;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, bytes, 3, 0, physical, 0x200000) == 0);
    const auto address = reinterpret_cast<std::uintptr_t>(mapped);
    const auto collect = [&](std::size_t offset, std::size_t length, std::size_t capacity, bool clear, bool armed) {
        unsigned char sentinel = 0;
        std::vector<void*> pages(capacity + 2, &sentinel);
        std::size_t count = capacity;
        const auto function = armed ? &GuestArena::GuestArenaCollectArmedWrites_nid_postfix : &GuestArena::GuestArenaCollectWrites_nid_postfix;
        Require(function(address + offset, length, pages.data() + 1, &count, clear));
        Require(count <= capacity && pages.front() == &sentinel && pages.back() == &sentinel);
        std::vector<std::uintptr_t> found;
        for (std::size_t at = 0; at < count; ++at) found.push_back(reinterpret_cast<std::uintptr_t>(pages[at + 1]) - address);
        return found;
    };
    Require(collect(0, bytes, 0, true, false).empty());
    Require(collect(0, bytes, bytes / small, true, false).size() == bytes / small);
    Require(collect(0, bytes, bytes / small, true, true).empty());
    auto* data = static_cast<volatile unsigned char*>(mapped);
    data[3] = 1;
    data[small + 8] = 2;
    data[0x200000 + 9] = 3;
    Require(collect(0, bytes, 0, true, false).empty());
    Require(collect(1, small - 1, 1, false, true) == std::vector<std::uintptr_t>{1});
    Require(collect(0, small, 1, false, false) == std::vector<std::uintptr_t>{0});
    Require(collect(0, small, 1, true, true) == std::vector<std::uintptr_t>{0});
    Require(collect(0, small, 1, true, false).empty());
    Require(collect(small, bytes - small, 1, true, false) == std::vector<std::uintptr_t>{small});
    Require(collect(small * 2, bytes - small * 2, bytes / small, true, true) == std::vector<std::uintptr_t>{0x200000});
    Require(collect(0, bytes, bytes / small, true, false).empty());
    Require(sceKernelMunmap(mapped, bytes) == 0);
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0);
#endif
}

static void CheckResidentCollectionDuringHostWrites() {
#ifdef _WIN32
    constexpr std::size_t bytes = 0x100000;
    constexpr std::size_t small = 0x1000;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, bytes, 3, 0, physical, 0x200000) == 0);
    auto* data = static_cast<unsigned char*>(mapped);
    const auto address = reinterpret_cast<std::uintptr_t>(mapped);
    const auto collect = [&] {
        std::array<void*, bytes / small> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(address, bytes, pages.data(), &count, true));
        return std::vector<void*>(pages.begin(), pages.begin() + count);
    };
    collect();
    Require(collect().empty());
    Require(GuestArena::GuestArenaBeginHostWrite_nid_postfix(data + small, small));
    data[small + 7] = 1;
    Require(collect() == std::vector<void*>{data + small});
    data[small + 8] = 2;
    GuestArena::GuestArenaEndHostWrite_nid_postfix(data + small, small);
    Require(collect() == std::vector<void*>{data + small});
    Require(collect().empty());
    GuestArena::GuestArenaPinWritable_nid_postfix(data + small * 2, small);
    collect();
    static_cast<volatile unsigned char*>(mapped)[small * 2 + 9] = 3;
    Require(collect() == std::vector<void*>{data + small * 2});
    GuestArena::GuestArenaUnpinWritable_nid_postfix(data + small * 2, small);
    collect();
    std::atomic<unsigned int> written{0};
    std::atomic<unsigned int> consumed{0};
    std::thread writer([&] {
        for (unsigned int round = 1; round <= 128; ++round) {
            while (consumed.load(std::memory_order_acquire) != round - 1) std::this_thread::yield();
            static_cast<volatile unsigned char*>(mapped)[small * 3 + 7] = static_cast<unsigned char>(round);
            written.store(round, std::memory_order_release);
        }
    });
    for (unsigned int round = 1; round <= 128; ++round) {
        const auto racingPages = collect();
        while (written.load(std::memory_order_acquire) != round) std::this_thread::yield();
        const auto pages = collect();
        Require(std::find(pages.begin(), pages.end(), data + small * 3) != pages.end() || std::find(racingPages.begin(), racingPages.end(), data + small * 3) != racingPages.end());
        Require(data[small * 3 + 7] == static_cast<unsigned char>(round));
        Require(collect().empty());
        consumed.store(round, std::memory_order_release);
    }
    writer.join();
    void* alias = nullptr;
    Require(sceKernelMapDirectMemory(&alias, 0x4000, 3, 0, physical, 0) == 0);
    collect();
    static_cast<volatile unsigned char*>(alias)[small + 11] = 4;
    Require(data[small + 11] == 4);
    const auto pages = collect();
    Require(std::find(pages.begin(), pages.end(), data + small) != pages.end());
    Require(sceKernelMunmap(alias, 0x4000) == 0);
    Require(sceKernelMunmap(mapped, bytes) == 0);
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0);
#endif
}

static void CheckReleaseKeepsPhysicalUntilViewsUnmapped(decltype(&sceKernelReleaseDirectMemory) release) {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 4, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 4, 3, 0, phys, 0) == 0);
    auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    Require(std::count_if(lease.begin(), lease.end(), [&](const auto& range) { return range->address == reinterpret_cast<std::uintptr_t>(mapped); }) == 1);
    std::atomic<bool> started{false};
    std::atomic<bool> finished{false};
    std::thread releaser([&] {
        started = true;
        Require(release(phys, page * 4) == 0);
        finished = true;
    });
    while (!started) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(!finished);
    std::int64_t again = 0;
    Require(sceKernelAllocateDirectMemory(phys, phys + static_cast<std::int64_t>(page * 4), page * 4, 0, 0, &again) != 0);
    lease.clear();
    releaser.join();
    Require(finished);
    Require(sceKernelAllocateDirectMemory(phys, phys + static_cast<std::int64_t>(page * 4), page * 4, 0, 0, &again) == 0 && again == phys);
    Require(sceKernelReleaseDirectMemory(again, page * 4) == 0);
}

static void CheckConcurrentCheckedReleasesUnmapViewsOnce() {
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 4, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 4, 3, 0, phys, 0) == 0);
    auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    std::atomic<int> started{0};
    int results[2] = {-1, -1};
    std::thread releasers[2];
    for (int index = 0; index < 2; index++) {
        releasers[index] = std::thread([&, index] {
            started++;
            results[index] = sceKernelCheckedReleaseDirectMemory(phys, page * 4);
        });
    }
    while (started != 2) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    lease.clear();
    for (auto& releaser : releasers) releaser.join();
    Require((results[0] == 0 && results[1] == SCE_KERNEL_ERROR_ENOENT) || (results[0] == SCE_KERNEL_ERROR_ENOENT && results[1] == 0));
    std::int64_t again = 0;
    Require(sceKernelAllocateDirectMemory(phys, phys + static_cast<std::int64_t>(page * 4), page * 4, 0, 0, &again) == 0 && again == phys);
    Require(sceKernelReleaseDirectMemory(again, page * 4) == 0);
}

static void CheckSecondDirectMappingWaitsForLeases() {
#ifdef _WIN32
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 4, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 4, 3, 0, phys, 0) == 0);
    auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    Require(std::count_if(lease.begin(), lease.end(), [&](const auto& range) { return range->address == reinterpret_cast<std::uintptr_t>(mapped); }) == 1);
    std::atomic<bool> released{false};
    std::thread holder([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        released = true;
        lease.clear();
    });
    void* second = nullptr;
    Require(sceKernelMapDirectMemory(&second, page, 3, 0, phys + page, 0) == 0);
    Require(released);
    holder.join();
    static_cast<volatile unsigned char*>(second)[8] = 3;
    Require(static_cast<unsigned char*>(mapped)[page + 8] == 3);
    static_cast<volatile unsigned char*>(mapped)[page * 2 + 8] = 4;
    Require(sceKernelMunmap(second, page) == 0);
    Require(sceKernelMunmap(mapped, page * 4) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 4) == 0);
#endif
}

#ifdef _WIN32
static void RequireResidentMapping(const void* address) {
    MEMORY_BASIC_INFORMATION info{};
    Require(VirtualQuery(address, &info, sizeof(info)) == sizeof(info));
    Require(info.State == MEM_COMMIT && info.Type == MEM_PRIVATE);
}
#endif

static void CheckBreakingAResidentRunKeepsConcurrentAccesses() {
#ifdef _WIN32
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t chunk = 0x200000;
    constexpr std::size_t chunks = 4;
    for (int round = 0; round < 20; ++round) {
        const bool unmap = round % 2 != 0;
        std::int64_t phys = 0;
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, chunk * chunks, 0, 0, &phys) == 0);
        void* mapped = nullptr;
        Require(sceKernelMapDirectMemory(&mapped, chunk * chunks, 3, 0, phys, 0) == 0);
        RequireResidentMapping(mapped);
        auto* memory = static_cast<volatile unsigned char*>(mapped);
        const auto value = [&](std::size_t at) { return static_cast<unsigned char>((at * 31 + static_cast<std::size_t>(round)) | 1); };
        const auto skipped = [&](std::size_t offset) { return unmap && offset >= page && offset < page * 2; };
        std::atomic<bool> go{false};
        std::atomic<bool> done{false};
        std::atomic<bool> consistent{true};
        std::thread writer([&] {
            while (!go) {}
            for (std::size_t offset = 0; offset < chunk; ++offset) {
                if (skipped(offset)) continue;
                for (std::size_t index = 0; index < chunks; ++index) memory[index * chunk + offset] = value(index * chunk + offset);
            }
        });
        std::thread reader([&] {
            while (!go) {}
            while (!done) {
                for (std::size_t offset = 0; offset < chunk && !done; offset += 61) {
                    if (skipped(offset)) continue;
                    for (std::size_t index = 0; index < chunks; ++index) {
                        const unsigned char seen = memory[index * chunk + offset];
                        if (seen != 0 && seen != value(index * chunk + offset)) consistent = false;
                    }
                }
            }
        });
        std::array<void*, chunks> second{};
        go = true;
        for (std::size_t index = 0; index < chunks; ++index) {
            if (unmap) Require(sceKernelMunmap(static_cast<unsigned char*>(mapped) + index * chunk + page, page) == 0);
            else Require(sceKernelMapDirectMemory(&second[index], page, 3, 0, phys + static_cast<std::int64_t>(index * chunk + page), 0) == 0);
        }
        writer.join();
        done = true;
        reader.join();
        Require(consistent);
        for (std::size_t index = 0; index < chunks; ++index) {
            for (std::size_t offset = 0; offset < chunk; ++offset) {
                if (!skipped(offset)) Require(memory[index * chunk + offset] == value(index * chunk + offset));
            }
            if (unmap) continue;
            for (std::size_t offset = 0; offset < page; ++offset) Require(static_cast<unsigned char*>(second[index])[offset] == value(index * chunk + page + offset));
            Require(sceKernelMunmap(second[index], page) == 0);
        }
        for (std::size_t index = 0; index < chunks; ++index) {
            auto* base = static_cast<unsigned char*>(mapped) + index * chunk;
            if (!unmap) {
                Require(sceKernelMunmap(base, chunk) == 0);
                continue;
            }
            Require(sceKernelMunmap(base, page) == 0);
            Require(sceKernelMunmap(base + page * 2, chunk - page * 2) == 0);
        }
        Require(sceKernelReleaseDirectMemory(phys, chunk * chunks) == 0);
    }
#endif
}

static void CheckRemappedDirectMemoryIsChargedOnce() {
#ifdef _WIN32
    constexpr std::size_t bytes = 0x10000000;
    const auto committed = [] {
        PERFORMANCE_INFORMATION info{};
        info.cb = sizeof(info);
        Require(K32GetPerformanceInfo(&info, sizeof(info)) != 0);
        return static_cast<std::int64_t>(info.CommitTotal) * static_cast<std::int64_t>(info.PageSize);
    };
    bool once = false;
    for (int attempt = 0; attempt < 3 && !once; ++attempt) {
        const auto before = committed();
        std::int64_t phys = 0;
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &phys) == 0);
        void* mapped = nullptr;
        Require(sceKernelMapDirectMemory(&mapped, bytes, 3, 0, phys, 0) == 0);
        RequireResidentMapping(mapped);
        std::memset(mapped, 0xa5, bytes);
        Require(sceKernelMunmap(mapped, bytes) == 0);
        void* again = nullptr;
        Require(sceKernelMapDirectMemory(&again, bytes, 3, 0, phys, 0) == 0);
        const auto* data = static_cast<const unsigned char*>(again);
        Require(data[0] == 0xa5 && data[bytes / 2] == 0xa5 && data[bytes - 1] == 0xa5);
        once = committed() - before < static_cast<std::int64_t>(bytes + bytes / 2);
        Require(sceKernelMunmap(again, bytes) == 0);
        Require(sceKernelReleaseDirectMemory(phys, bytes) == 0);
    }
    Require(once);
#endif
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
    Require(collect(first, page * 3, false) == 1);
    Require(collect(first, page * 3, false) == 1);
    Require(collect(first, page * 3) == 1);
    Require(collect(second, page * 3) == 1);
    Require(collect(first, page * 3) == 0);
    right[page * 2] = 42;
    Require(collect(first, page * 3) == 1);
    Require(collect(second, page * 3) == 1);
    Require(collect(second, page * 3) == 0);
    Require(sceKernelMprotect(first, page * 3, 1) == 0);
    collect(first, page * 3);
    collect(second, page * 3);
    right[0] = 63;
    Require(left[0] == 63 && collect(first, page * 3) == 1);
    Require(sceKernelMprotect(first, page * 3, 3) == 0);
    collect(first, page * 3);
    left[0] = 84;
    Require(right[0] == 84 && collect(second, page * 3) != 0);
    void* third = nullptr;
    Require(sceKernelMapDirectMemory(&third, page, 3, 0, phys + page, 0) == 0);
    collect(first, page * 3);
    collect(second, page * 3);
    static_cast<volatile unsigned char*>(third)[0] = 105;
    Require(collect(first, page * 3) == 1 && collect(second, page * 3) == 1);
    Require(sceKernelMunmap(third, page) == 0);
    Require(sceKernelMunmap(first, page * 3) == 0);
    Require(sceKernelMunmap(second, page * 3) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 3) == 0);
#endif
}

static void CheckSharedWritePages() {
#ifdef _WIN32
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t small = 0x1000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 2, 3, 0, phys, 0) == 0);
    auto* bytes = static_cast<unsigned char*>(mapped);
    const auto collect = [&](std::size_t offset, std::size_t size) {
        std::array<void*, 32> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(mapped) + offset, size, pages.data(), &count, true));
        Require(count == 0 || reinterpret_cast<std::uintptr_t>(pages[0]) >= reinterpret_cast<std::uintptr_t>(mapped) + offset);
        return count;
    };
    collect(0, page * 2);
    Require(collect(0, page * 2) == 0);
    static_cast<volatile unsigned char*>(bytes)[small + 3] = 1;
    Require(collect(small * 2, small * 2) == 0);
    Require(collect(0, page * 2) == 1);
    Require(collect(0, page * 2) == 0);
    std::memset(bytes + small * 2 - 8, 7, 16);
    Require(collect(0, page * 2) == 2);
    Require(collect(0, page * 2) == 0);
    const auto path = std::filesystem::path("anyps5-shared-pages-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
    {
        std::ofstream stream(path, std::ios::binary);
        const std::vector<char> seed(small, 5);
        stream.write(seed.data(), static_cast<std::streamsize>(seed.size()));
    }
    const int fd = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(fd >= 0);
    Require(sceKernelPread(fd, bytes + page + small, small, 0) == static_cast<std::int64_t>(small));
    Require(sceKernelClose(fd) == 0);
    Require(bytes[page + small] == 5 && bytes[page + small * 2 - 1] == 5);
    Require(collect(page, small) == 0);
    Require(collect(0, page * 2) == 1);
    Require(sceKernelMunmap(mapped, page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
    std::filesystem::remove(path);
#endif
}

static void CheckSharedWritesDuringHostWrite() {
#ifdef _WIN32
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t small = 0x1000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page, 0, 0, &phys) == 0);
    void* first = nullptr;
    void* second = nullptr;
    Require(sceKernelMapDirectMemory(&first, page, 3, 0, phys, 0) == 0);
    Require(sceKernelMapDirectMemory(&second, page, 3, 0, phys, 0) == 0);
    auto* left = static_cast<volatile unsigned char*>(first);
    auto* right = static_cast<volatile unsigned char*>(second);
    const auto collect = [](void* address) {
        std::array<void*, 32> pages{};
        std::size_t count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(address), page, pages.data(), &count, true));
        std::vector<std::size_t> offsets;
        for (std::size_t i = 0; i < count; ++i) offsets.push_back(reinterpret_cast<std::uintptr_t>(pages[i]) - reinterpret_cast<std::uintptr_t>(address));
        return offsets;
    };
    const std::vector<std::size_t> open{small, small * 2};
    collect(first);
    collect(second);
    Require(collect(first).empty() && collect(second).empty());
    left[small] = 1;
    auto* target = const_cast<unsigned char*>(left + small * 2);
    Require(GuestArena::GuestArenaBeginHostWrite_nid_postfix(target, small));
    Require(collect(first) == open);
    Require(collect(second) == open);
    left[small + 8] = 2;
    Require(collect(first) == open);
    Require(collect(second) == open);
    GuestArena::GuestArenaEndHostWrite_nid_postfix(target, small);
    Require(collect(first) == open && collect(second) == open);
    Require(collect(first).empty() && collect(second).empty());
    left[small + 16] = 3;
    Require(right[small + 16] == 3 && collect(second) == std::vector<std::size_t>{small});
    Require(sceKernelMunmap(first, page) == 0);
    Require(sceKernelMunmap(second, page) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page) == 0);
#endif
}

static void CheckFailedCollectKeepsWrites() {
#ifdef _WIN32
    constexpr std::size_t page = 0x4000;
    const auto collect = [](void* address, std::size_t bytes, std::size_t& count) {
        std::array<void*, 32> pages{};
        count = pages.size();
        return GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(address), bytes, pages.data(), &count, true);
    };
    std::size_t count = 0;
    auto* block = static_cast<unsigned char*>(GuestArena::GuestArenaAllocate_nid_postfix(page * 3, page));
    GuestArena::GuestArenaCommit_nid_postfix(block, page, PAGE_READWRITE, page);
    GuestArena::GuestArenaCommit_nid_postfix(block + page * 2, page, PAGE_READWRITE, page);
    Require(collect(block, page, count) && collect(block + page * 2, page, count));
    static_cast<volatile unsigned char*>(block)[8] = 1;
    Require(!collect(block, page * 3, count));
    Require(collect(block, page, count) && count == 1);
    GuestArena::GuestArenaRelease_nid_postfix(block, page * 3);

    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 2, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 2, 3, 0, phys, 0) == 0);
    auto* shared = static_cast<volatile unsigned char*>(mapped);
    Require(collect(mapped, page * 2, count));
    shared[8] = 2;
    Require(sceKernelMprotect(const_cast<unsigned char*>(shared + page), page, 0) == 0);
    Require(!collect(mapped, page * 2, count));
    Require(collect(mapped, page, count) && count == 1);
    Require(sceKernelMunmap(mapped, page * 2) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 2) == 0);
#endif
}

static void CheckSplitPrivateCollect() {
#ifdef _WIN32
    constexpr std::size_t granule = 0x4000;
    constexpr std::size_t regions = 8;
    constexpr std::size_t bytes = granule * regions;
    constexpr std::size_t pages = bytes / 4096;
    auto* block = static_cast<unsigned char*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, granule));
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, granule);
    const auto address = reinterpret_cast<std::uintptr_t>(block);
    const auto collect = [&](std::size_t capacity) {
        std::vector<std::uintptr_t> found;
        std::array<void*, pages> buffer{};
        for (auto cursor = address; cursor < address + bytes;) {
            std::size_t count = capacity;
            Require(GuestArena::GuestArenaCollectWrites_nid_postfix(cursor, address + bytes - cursor, buffer.data(), &count, true));
            for (std::size_t i = 0; i < count; ++i) found.push_back(reinterpret_cast<std::uintptr_t>(buffer[i]));
            if (count < capacity) break;
            cursor = found.back() + 4096;
        }
        return found;
    };
    collect(pages);
    for (const std::size_t capacity : {pages, std::size_t{5}, std::size_t{1}}) {
        for (std::size_t page = 0; page < pages; ++page) static_cast<volatile unsigned char*>(block)[page * 4096 + 7] = static_cast<unsigned char>(capacity + page);
        const auto found = collect(capacity);
        Require(found.size() == pages);
        for (std::size_t page = 0; page < pages; ++page) Require(found[page] == address + page * 4096);
        Require(collect(pages).empty());
    }
    static_cast<volatile unsigned char*>(block)[granule * 3 + 4096 + 1] = 9;
    const auto one = collect(pages);
    Require(one.size() == 1 && one[0] == address + granule * 3 + 4096);
    GuestArena::GuestArenaReset_nid_postfix(block, bytes);
    GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
#endif
}

static void CheckPinnedSharedPages() {
#ifdef _WIN32
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 3, 0, 0, &phys) == 0);
    void* mapped = nullptr;
    void* alias = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, page * 3, 3, 0, phys, 0) == 0);
    Require(sceKernelMapDirectMemory(&alias, page * 3, 3, 0, phys, 0) == 0);
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
    Require(collect() == 5);
    Require(collect() == 4);
    Require(protection(0) == PAGE_READONLY && protection(page) == PAGE_READWRITE);
    GuestArena::GuestArenaUnpinWritable_nid_postfix(const_cast<unsigned char*>(bytes + page), page);
    collect();
    Require(collect() == 0);
    Require(protection(page) == PAGE_READONLY);
    bytes[page + 8] = 11;
    Require(collect() == 1);
    Require(sceKernelMunmap(alias, page * 3) == 0);
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
    Require(sceKernelPread(fd, mapped, page, 0) == static_cast<std::int64_t>(SCE_KERNEL_ERROR_EFAULT));
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

static bool CollectArmedRuns(const void* base, std::size_t offset, std::size_t bytes, PageRuns& runs) {
    runs.clear();
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    std::pair<std::uintptr_t, PageRuns*> context{address, &runs};
    return GuestWriteWatch::GuestWriteWatchCollectArmed_nid_postfix(address + offset, bytes, [](void* context, std::uintptr_t begin, std::uintptr_t end) {
        auto& [origin, into] = *static_cast<std::pair<std::uintptr_t, PageRuns*>*>(context);
        if (!into->empty() && into->back().second == (begin - origin) / 4096) into->back().second = (end - origin) / 4096;
        else into->emplace_back((begin - origin) / 4096, (end - origin) / 4096);
    }, &context);
}

static void CheckWriteWatchArmedCollect() {
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) return;
    constexpr std::size_t length = 0x100000;
    constexpr std::size_t small = 4096;
    void* mapping = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapping, length, 3, 0) == 0);
    auto* bytes = static_cast<volatile unsigned char*>(mapping);
    PageRuns runs;
    Require(CollectRuns(mapping, 0, length / 2, runs) && runs == PageRuns{{0, length / small / 2}});
    bytes[5 * small] = 1;
    bytes[200 * small] = 1;
    Require(CollectArmedRuns(mapping, 0, length, runs) && runs == PageRuns{{5, 6}});
    Require(Written(mapping, length, {{length / small / 2, length / small}}));
    Require(Written(mapping, length, {}));
    Require(sceKernelMunmap(mapping, length) == 0);
}

static bool CollectFreshRuns(const void* base, std::size_t offset, std::size_t bytes, PageRuns& runs) {
    runs.clear();
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    std::pair<std::uintptr_t, PageRuns*> context{address, &runs};
    return GuestWriteWatch::GuestWriteWatchCollectFresh_nid_postfix(address + offset, bytes, [](void* context, std::uintptr_t begin, std::uintptr_t end) {
        auto& [origin, into] = *static_cast<std::pair<std::uintptr_t, PageRuns*>*>(context);
        if (!into->empty() && into->back().second == (begin - origin) / 4096) into->back().second = (end - origin) / 4096;
        else into->emplace_back((begin - origin) / 4096, (end - origin) / 4096);
    }, &context);
}

static void CheckWriteWatchFreshCollect() {
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) return;
    constexpr std::size_t length = 0x100000;
    constexpr std::size_t small = 4096;
    void* mapping = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapping, length, 3, 0) == 0);
    auto* bytes = static_cast<volatile unsigned char*>(mapping);
    PageRuns runs;
    Require(CollectRuns(mapping, 0, length / 2, runs) && runs == PageRuns{{0, length / small / 2}});
    bytes[5 * small] = 1;
    Require(CollectFreshRuns(mapping, 0, length, runs) && runs == PageRuns{{length / small / 2, length / small}});
    Require(CollectFreshRuns(mapping, 0, length, runs) && runs.empty());
    bytes[200 * small] = 1;
    Require(Written(mapping, length, {{5, 6}, {200, 201}}));
    Require(sceKernelMunmap(mapping, length) == 0);
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
    const auto base = static_cast<std::uint64_t>(phys);
    Require(GuestArena::GuestArenaSharedBacking_nid_postfix(address + page, page, &file, &offset));
    Require(offset == base + page * 2);
    unsigned char byte = 0;
    Require(pread(file, &byte, 1, static_cast<off_t>(offset + 5)) == 1 && byte == 0x5a);
    const int seals = fcntl(file, F_GET_SEALS);
    Require(seals >= 0 && (seals & F_SEAL_SHRINK) != 0 && (seals & F_SEAL_GROW) != 0);
    Require((fcntl(file, F_GETFD) & FD_CLOEXEC) != 0);
    close(file);
    Require(GuestArena::GuestArenaSharedBacking_nid_postfix(address, page * 2, &file, &offset) && offset == base + page);
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
    Require(GuestArena::GuestArenaSharedBacking_nid_postfix(flexibleAddress, page * 2, &file, &offset) && offset == base);
    close(file);
    Require(sceKernelMapDirectMemory(&high, page, 3, 0x10, phys + page * 2, 0) == 0);
    Require(!GuestArena::GuestArenaSharedBacking_nid_postfix(flexibleAddress, page * 2, &file, &offset));
    Require(GuestArena::GuestArenaSharedBacking_nid_postfix(flexibleAddress + page, page, &file, &offset) && offset == base + page * 2);
    close(file);
    Require(sceKernelMunmap(high, page) == 0);
    Require(!GuestArena::GuestArenaSharedBacking_nid_postfix(flexibleAddress + page, page, &file, &offset));
    Require(sceKernelMunmap(flexible, page) == 0);
    Require(sceKernelMunmap(first, page * 2) == 0);
    Require(!GuestArena::GuestArenaSharedBacking_nid_postfix(address, page, &file, &offset));
    Require(sceKernelReleaseDirectMemory(phys, page * 3) == 0);

    std::int64_t left = 0;
    std::int64_t right = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page, 0, 0, &left) == 0);
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page, 0, 0, &right) == 0);
    void* leftView = nullptr;
    void* rightView = nullptr;
    Require(sceKernelMapDirectMemory(&leftView, page, 3, 0, left, 0) == 0);
    Require(sceKernelMapDirectMemory(&rightView, page, 3, 0, right, 0) == 0);
    int leftFile = -1;
    int rightFile = -1;
    std::uint64_t leftOffset = 0;
    std::uint64_t rightOffset = 0;
    Require(GuestArena::GuestArenaSharedBacking_nid_postfix(reinterpret_cast<std::uintptr_t>(leftView), page, &leftFile, &leftOffset) && leftOffset == static_cast<std::uint64_t>(left));
    Require(GuestArena::GuestArenaSharedBacking_nid_postfix(reinterpret_cast<std::uintptr_t>(rightView), page, &rightFile, &rightOffset) && rightOffset == static_cast<std::uint64_t>(right));
    struct stat leftInfo {};
    struct stat rightInfo {};
    Require(fstat(leftFile, &leftInfo) == 0 && fstat(rightFile, &rightInfo) == 0 && leftInfo.st_ino == rightInfo.st_ino && leftInfo.st_dev == rightInfo.st_dev);
    close(leftFile);
    close(rightFile);
    std::memset(leftView, 0x77, page);
    Require(sceKernelMunmap(leftView, page) == 0);
    Require(sceKernelReleaseDirectMemory(left, page) == 0);
    std::int64_t again = 0;
    Require(sceKernelAllocateDirectMemory(left, left + static_cast<std::int64_t>(page), page, 0, 0, &again) == 0 && again == left);
    void* againView = nullptr;
    Require(sceKernelMapDirectMemory(&againView, page, 3, 0, again, 0) == 0);
    for (std::size_t i = 0; i < page; ++i) Require(static_cast<const unsigned char*>(againView)[i] == 0);
    Require(sceKernelMunmap(againView, page) == 0);
    Require(sceKernelMunmap(rightView, page) == 0);
    Require(sceKernelReleaseDirectMemory(again, page) == 0);
    Require(sceKernelReleaseDirectMemory(right, page) == 0);
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

static void CheckDirectMemoryWriteWatchFollowsRemovedMappings() {
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::puts("write watch unavailable: removed direct mappings not tested");
        return;
    }
    constexpr std::size_t page = 0x4000;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * 4, 0, 0, &phys) == 0);
    void* whole = nullptr;
    Require(sceKernelMapDirectMemory(&whole, page * 4, 3, 0, phys, 0) == 0);
    const auto address = reinterpret_cast<std::uintptr_t>(whole);
    const auto watched = [](const void* pointer, std::size_t offset) { return GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(pointer) + offset, page); };
    void* pair = nullptr;
    Require(sceKernelMapDirectMemory(&pair, page * 2, 3, 0, phys + page, 0) == 0);
    Require(watched(whole, 0) && !watched(whole, page) && !watched(whole, page * 2) && watched(whole, page * 3));
    Require(!watched(pair, 0) && !watched(pair, page));
    Require(sceKernelMunmap(pair, page) == 0);
    void* single = nullptr;
    Require(sceKernelMapDirectMemory(&single, page, 3, 0, phys + page, 0) == 0);
    Require(!watched(single, 0) && watched(whole, 0) && watched(whole, page * 3));
    Require(sceKernelMunmap(single, page) == 0);
    Require(sceKernelMunmap(static_cast<unsigned char*>(pair) + page, page) == 0);
    void* replaced = whole;
    Require(sceKernelMapDirectMemory(&replaced, page, 3, 0x10, phys + page * 3, 0) == 0 && replaced == whole);
    Require(!watched(whole, 0) && !watched(whole, page * 3));
    std::uintptr_t start = 0;
    std::uintptr_t end = 0;
    std::uint64_t offset = 0;
    int type = 0;
    Require(QueryDirectMapping(address, &start, &end, &offset, &type) && start == address && end == address + page && offset == static_cast<std::uint64_t>(phys) + page * 3);
    void* alone = nullptr;
    Require(sceKernelMapDirectMemory(&alone, page, 3, 0, phys, 0) == 0);
    Require(watched(alone, 0));
    void* again = nullptr;
    Require(sceKernelMapDirectMemory(&again, page, 3, 0, phys, 0) == 0);
    Require(!watched(alone, 0) && !watched(again, 0));
    Require(sceKernelMunmap(again, page) == 0);
    Require(sceKernelMunmap(alone, page) == 0);
    Require(sceKernelMunmap(whole, page * 4) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * 4) == 0);
}

static double MapAcrossBackingsSeconds(std::int64_t phys, std::size_t backings) {
    constexpr std::size_t page = 0x4000;
    const auto threadSeconds = [] {
        timespec now{};
        Require(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now) == 0);
        return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) * 1e-9;
    };
    auto best = std::numeric_limits<double>::max();
    for (int attempt = 0; attempt < 3; ++attempt) {
        void* mapped = nullptr;
        const auto before = threadSeconds();
        Require(sceKernelMapDirectMemory(&mapped, page * backings, 3, 0, phys, 0) == 0);
        best = std::min(best, threadSeconds() - before);
        const auto last = reinterpret_cast<std::uintptr_t>(mapped) + page * (backings - 1);
        std::uintptr_t start = 0;
        std::uintptr_t end = 0;
        std::uint64_t offset = 0;
        int type = 0;
        Require(QueryDirectMapping(last, &start, &end, &offset, &type) && start == last && end == last + page);
        Require(sceKernelMunmap(mapped, page * backings) == 0);
    }
    return best;
}

static void CheckMappingCostFollowsTheMappingCount() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t few = 1024;
    constexpr std::size_t many = few * 16;
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page * many, 0, 0, &phys) == 0);
    Require(sceKernelReleaseDirectMemory(phys, page * many) == 0);
    for (std::size_t index = 0; index < many; ++index) {
        const auto wanted = phys + static_cast<std::int64_t>(page * index);
        std::int64_t piece = 0;
        Require(sceKernelAllocateDirectMemory(wanted, wanted + static_cast<std::int64_t>(page), page, 0, 0, &piece) == 0 && piece == wanted);
    }
    const auto small = MapAcrossBackingsSeconds(phys, few);
    const auto large = MapAcrossBackingsSeconds(phys, many);
    Require(large <= small * 48);
    Require(sceKernelReleaseDirectMemory(phys, page * many) == 0);
}
#endif

#if defined(__linux__) || defined(_WIN32)
static void CheckRegularFileMapping() {
    constexpr std::size_t page = 0x4000;
    const auto path = std::filesystem::path("anyps5-file-mapping-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
    {
        std::ofstream stream(path, std::ios::binary);
        std::vector<char> seed(page * 2, 0x11);
        std::fill(seed.begin() + page, seed.end(), 0x22);
        stream.write(seed.data(), static_cast<std::streamsize>(seed.size()));
    }
    const auto byteAt = [&](int fd, std::int64_t offset) {
        unsigned char byte = 0;
        Require(sceKernelPread(fd, &byte, 1, offset) == 1);
        return byte;
    };
    const int fd = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(fd >= 0);
    auto* shared = static_cast<volatile unsigned char*>(mmap_nid_postfix(nullptr, page * 2, 3, 0x1, fd, 0));
    Require(shared != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    Require(shared[0] == 0x11 && shared[page] == 0x22);
    const unsigned char written = 0x33;
    Require(sceKernelPwrite(fd, &written, 1, page + 7) == 1);
    Require(shared[page + 7] == 0x33);
    shared[5] = 0x77;
    Require(munmap_nid_postfix(const_cast<unsigned char*>(shared), page * 2) == 0);
    Require(byteAt(fd, 5) == 0x77);
    auto* privateCopy = static_cast<volatile unsigned char*>(mmap_nid_postfix(nullptr, page, 3, 0x2, fd, 0));
    Require(privateCopy != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    Require(privateCopy[5] == 0x77);
    privateCopy[6] = 0x66;
    Require(munmap_nid_postfix(const_cast<unsigned char*>(privateCopy), page) == 0);
    Require(byteAt(fd, 6) == 0x11);
    auto* second = static_cast<volatile unsigned char*>(mmap_nid_postfix(nullptr, page, 1, 0x1, fd, page));
    Require(second != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)) && second[0] == 0x22);
    Require(munmap_nid_postfix(const_cast<unsigned char*>(second), page) == 0);
    Require(sceKernelClose(fd) == 0);
    const int readOnly = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(readOnly >= 0);
    *__error_nid_postfix() = 0;
    Require(mmap_nid_postfix(nullptr, page, 3, 0x1, readOnly, 0) == reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)) && *__error_nid_postfix() == 13);
    auto* readable = static_cast<volatile unsigned char*>(mmap_nid_postfix(nullptr, page, 1, 0x1, readOnly, 0));
    Require(readable != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)) && readable[5] == 0x77);
    *__error_nid_postfix() = 0;
    Require(mprotect_nid_postfix(const_cast<unsigned char*>(readable), page, 3) == -1 && *__error_nid_postfix() == 13);
    Require(mprotect_nid_postfix(const_cast<unsigned char*>(readable), page, 1) == 0);
    Require(readable[5] == 0x77);
    Require(munmap_nid_postfix(const_cast<unsigned char*>(readable), page) == 0);
    Require(sceKernelClose(readOnly) == 0);
    constexpr std::size_t tail = 0x1100;
    constexpr std::size_t hostPage = 0x1000;
    std::filesystem::resize_file(path, page + tail);
    const int partial = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(partial >= 0);
    auto* rounded = static_cast<volatile unsigned char*>(mmap_nid_postfix(nullptr, page * 2, 1, 0x1, partial, 0));
    Require(rounded != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    Require(rounded[5] == 0x77 && rounded[page] == 0x22 && rounded[page + 7] == 0x33 && rounded[page + tail - 1] == 0x22);
    for (std::size_t at = page + tail; at < page + 2 * hostPage; ++at) Require(rounded[at] == 0);
#ifdef _WIN32
    for (std::size_t at = page + 2 * hostPage; at < page * 2; ++at) Require(rounded[at] == 0);
    *__error_nid_postfix() = 0;
    Require(mmap_nid_postfix(nullptr, page * 3, 1, 0x1, partial, 0) == reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)) && *__error_nid_postfix() == 45);
#endif
    Require(munmap_nid_postfix(const_cast<unsigned char*>(rounded), page * 2) == 0);
    Require(sceKernelClose(partial) == 0);
    Require(std::filesystem::file_size(path) == page + tail);
    const int partialWritable = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(partialWritable >= 0);
    auto* roundedWritable = static_cast<volatile unsigned char*>(mmap_nid_postfix(nullptr, page * 2, 3, 0x1, partialWritable, 0));
    Require(roundedWritable != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    const unsigned char nearEnd = 0x44;
    Require(sceKernelPwrite(partialWritable, &nearEnd, 1, page + tail - 2) == 1);
    Require(roundedWritable[page + tail - 2] == 0x44);
    roundedWritable[page + tail - 1] = 0x55;
    roundedWritable[page + tail] = 0x66;
#ifdef _WIN32
    roundedWritable[page * 2 - 1] = 0x66;
    Require(roundedWritable[page * 2 - 1] == 0x66);
#endif
    Require(munmap_nid_postfix(const_cast<unsigned char*>(roundedWritable), page * 2) == 0);
    Require(byteAt(partialWritable, page + tail - 1) == 0x55);
    Require(sceKernelClose(partialWritable) == 0);
    Require(std::filesystem::file_size(path) == page + tail);
    int ends[2] = {-1, -1};
#ifdef _WIN32
    Require(_pipe(ends, 4096, _O_BINARY) == 0);
#else
    Require(pipe(ends) == 0);
#endif
    *__error_nid_postfix() = 0;
    Require(mmap_nid_postfix(nullptr, page, 1, 0x1, ends[0], 0) == reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)) && *__error_nid_postfix() == 45);
#ifdef _WIN32
    _close(ends[0]);
    _close(ends[1]);
#else
    close(ends[0]);
    close(ends[1]);
#endif
    Require(std::filesystem::remove(path));
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

static void CheckVirtualQueryWireLayout() {
    constexpr std::size_t page = 0x4000;
    constexpr char name[] = "query wire layout";
    void* mapped = nullptr;
    Require(sceKernelMapNamedFlexibleMemory(&mapped, page, 3, 0, name) == 0);
    struct {
        std::array<unsigned char, 16> before;
        VirtualQueryInfo info;
        std::array<unsigned char, 16> after;
    } result{};
    result.before.fill(0xa5);
    result.after.fill(0x5a);
    Require(sceKernelVirtualQuery(mapped, 0, &result.info, 72) == 0);
    const auto* bytes = reinterpret_cast<const unsigned char*>(&result.info);
    std::uintptr_t start = 0, end = 0;
    std::int32_t protection = 0;
    std::memcpy(&start, bytes, sizeof(start));
    std::memcpy(&end, bytes + 8, sizeof(end));
    std::memcpy(&protection, bytes + 24, sizeof(protection));
    Require(start == reinterpret_cast<std::uintptr_t>(mapped) && end == start + page);
    Require(protection == 3 && bytes[32] == 0x11); // flexible and committed
    Require(std::memcmp(bytes + 33, name, sizeof(name)) == 0);
    Require(bytes[65] == 0 && bytes[66] == 0);
    Require(std::all_of(bytes + 67, bytes + 72, [](auto byte) { return byte == 0; }));
    Require(std::all_of(result.before.begin(), result.before.end(), [](auto byte) { return byte == 0xa5; }));
    Require(std::all_of(result.after.begin(), result.after.end(), [](auto byte) { return byte == 0x5a; }));
    Require(sceKernelClearVirtualRangeName(mapped, page) == 0);
    Require(sceKernelMunmap(mapped, page) == 0);
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

static void CheckPosixFixedAnonymousMapping() {
    constexpr std::size_t page = 0x4000;
    constexpr int fixed = 0x1012;
    constexpr int exclusive = 0x5012;
    const auto failed = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    void* reservation = nullptr;
    Require(sceKernelReserveVirtualRange(&reservation, page * 4, 0, 0) == 0);
    auto* memory = static_cast<unsigned char*>(reservation);
    Require(mmap_nid_postfix(memory, page, 3, fixed, -1, 0) == memory);
    Require(std::all_of(memory, memory + page, [](unsigned char value) { return value == 0; }));
    memory[0] = 0x31;
    memory[page - 1] = 0x32;
    Require(memory[0] == 0x31 && memory[page - 1] == 0x32);
    Require(mmap_nid_postfix(memory, page, 3, fixed, -1, 0) == memory);
    Require(std::all_of(memory, memory + page, [](unsigned char value) { return value == 0; }));
    memory[0] = 0x41;
    memory[page - 1] = 0x42;
    *__error_nid_postfix() = 0;
    Require(mmap_nid_postfix(memory, page, 3, exclusive, -1, 0) == failed);
    Require(*__error_nid_postfix() == 12);
    Require(memory[0] == 0x41 && memory[page - 1] == 0x42);
    VirtualQueryInfo before{};
    VirtualQueryInfo after{};
    Require(sceKernelVirtualQuery(memory + page, 0, &before, sizeof(before)) == 0);
    *__error_nid_postfix() = 0;
    Require(mmap_nid_postfix(memory + page, page, 3, exclusive, -1, 0) == failed);
    Require(*__error_nid_postfix() == 12);
    Require(sceKernelVirtualQuery(memory + page, 0, &after, sizeof(after)) == 0);
    Require(before.start == after.start && before.end == after.end && before.is_committed == after.is_committed);
    Require(mmap_nid_postfix(memory + page, page * 2, 3, fixed, -1, 0) == memory + page);
    memory[page] = 0x51;
    memory[page * 2 - 1] = 0x52;
    memory[page * 2] = 0x61;
    Require(sceKernelMunmap(memory + page * 3, page) == 0);
    Require(mmap_nid_postfix(memory + page * 3, page, 3, exclusive, -1, 0) == memory + page * 3);
    Require(std::all_of(memory + page * 3, memory + page * 4, [](unsigned char value) { return value == 0; }));
    memory[page * 4 - 1] = 0x62;
    Require(memory[page * 4 - 1] == 0x62);
    Require(mmap_nid_postfix(memory + page * 2, page + 1, 3, fixed, -1, 0) == memory + page * 2);
    Require(std::all_of(memory + page * 2, memory + page * 4, [](unsigned char value) { return value == 0; }));
    Require(memory[0] == 0x41 && memory[page - 1] == 0x42);
    Require(memory[page] == 0x51 && memory[page * 2 - 1] == 0x52);
    memory[page * 4 - 1] = 0x71;
    Require(memory[page * 4 - 1] == 0x71);
    const auto invalid = [&](void* address, std::size_t length, int protection, int flags, int descriptor, std::int64_t offset) {
        *__error_nid_postfix() = 0;
        Require(mmap_nid_postfix(address, length, protection, flags, descriptor, offset) == failed);
        Require(*__error_nid_postfix() == 22);
        Require(memory[0] == 0x41 && memory[page * 4 - 1] == 0x71);
    };
    invalid(memory, 0, 3, fixed, -1, 0);
    invalid(memory, std::numeric_limits<std::size_t>::max(), 3, fixed, -1, 0);
    invalid(memory + 1, page, 3, fixed, -1, 0);
    invalid(nullptr, page, 3, fixed, -1, 0);
    invalid(memory, page, 8, fixed, -1, 0);
    invalid(memory, page, 3, fixed, 0, 0);
    invalid(memory, page, 3, fixed, -1, 1);
    invalid(memory, page, 3, fixed, -1, -1);
    invalid(nullptr, page, 3, 0x5002, -1, 0);
    std::uintptr_t arenaBase = 0;
    std::size_t arenaBytes = 0;
    GuestArena::GuestArenaRange_nid_postfix(&arenaBase, &arenaBytes);
    Require(arenaBase >= page && arenaBytes >= page);
    invalid(reinterpret_cast<void*>(arenaBase - page), page, 3, fixed, -1, 0);
    invalid(reinterpret_cast<void*>(arenaBase + arenaBytes), page, 3, fixed, -1, 0);
    invalid(reinterpret_cast<void*>(arenaBase + arenaBytes - page), page * 2, 3, fixed, -1, 0);
    invalid(reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - page + 1), page * 2, 3, fixed, -1, 0);
    Require(munmap_nid_postfix(memory, page * 4) == 0);
}

static void CheckPosixFixedAnonymousReleasesFlexibleAccounting() {
    constexpr std::size_t page = 0x4000;
    constexpr int fixed = 0x1012;
    std::size_t before = 0;
    std::size_t available = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&before) == 0);
    void* base = nullptr;
    Require(sceKernelMapFlexibleMemory(&base, page * 3, 3, 0) == 0 && base != nullptr);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - page * 3);
    auto* bytes = static_cast<unsigned char*>(base);
    bytes[0] = 0x31;
    bytes[page] = 0x41;
    bytes[page * 2] = 0x51;
    Require(mmap_nid_postfix(bytes + page, page, 3, fixed, -1, 0) == bytes + page);
    Require(bytes[0] == 0x31 && bytes[page] == 0 && bytes[page * 2] == 0x51);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - page * 2);
    Require(sceKernelMunmap(base, page * 3) == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before);
}

#if defined(__linux__)
static void CheckPosixFixedFileReleasesFlexibleAccounting() {
    constexpr std::size_t page = 0x4000;
    constexpr int fixedShared = 0x11;
    const auto path = std::filesystem::path("anyps5-fixed-file-accounting-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
    {
        std::ofstream stream(path, std::ios::binary);
        std::vector<char> data(page, 0x6d);
        stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    const int fd = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(fd >= 0);
    std::size_t before = 0;
    std::size_t available = 0;
    Require(sceKernelAvailableFlexibleMemorySize(&before) == 0);
    void* base = nullptr;
    Require(sceKernelMapFlexibleMemory(&base, page * 3, 3, 0) == 0 && base != nullptr);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - page * 3);
    auto* bytes = static_cast<unsigned char*>(base);
    bytes[0] = 0x31;
    bytes[page * 2] = 0x51;
    Require(mmap_nid_postfix(bytes + page, page, 1, fixedShared, fd, 0) == bytes + page);
    Require(bytes[0] == 0x31 && bytes[page] == 0x6d && bytes[page * 2] == 0x51);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before - page * 2);
    Require(sceKernelMunmap(base, page * 3) == 0);
    Require(sceKernelAvailableFlexibleMemorySize(&available) == 0 && available == before);
    Require(sceKernelClose(fd) == 0);
    std::filesystem::remove(path);
}
#endif

static void CheckSceKernelMmap() {
    constexpr std::size_t page = 0x4000;
    void* mapped = nullptr;
    Require(sceKernelMmap(nullptr, page, 3, 0x1002, -1, 0, &mapped) == 0 && mapped != nullptr);
    static_cast<volatile unsigned char*>(mapped)[page - 1] = 7;
    Require(static_cast<volatile unsigned char*>(mapped)[page - 1] == 7);
    Require(sceKernelMunmap(mapped, page) == 0);
    void* untouched = &mapped;
    Require(sceKernelMmap(nullptr, 0, 3, 0x1002, -1, 0, &untouched) == static_cast<int>(0x80020016u));
    Require(untouched == &mapped);
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


static void CheckAvailableDirectMemoryIsLargestFreeBlock() {
    constexpr std::int64_t window = 0x4000000;
    const std::int64_t top = static_cast<std::int64_t>(DIRECT_MEMORY_SIZE) - window;
    std::int64_t first = -1;
    std::int64_t gap = -1;
    std::int64_t last = -1;
    Require(sceKernelAllocateDirectMemory(top, top + window, 0x200000, 0x200000, 0, &first) == 0 && first == top);
    Require(sceKernelAllocateDirectMemory(top, top + window, 0x400000, 0x200000, 0, &gap) == 0 && gap == top + 0x200000);
    Require(sceKernelAllocateDirectMemory(top, top + window, 0x200000, 0x200000, 0, &last) == 0 && last == top + 0x600000);
    Require(sceKernelReleaseDirectMemory(gap, 0x400000) == 0);

    std::int64_t start = -1;
    std::size_t size = 0;
    Require(sceKernelAvailableDirectMemorySize(top, top + window, 0, &start, &size) == 0);
    Require(start == top + 0x800000 && size == static_cast<std::size_t>(window - 0x800000));
    Require(sceKernelAvailableDirectMemorySize(top, top + 0x800000, 0, &start, &size) == 0);
    Require(start == top + 0x200000 && size == 0x400000);
    Require(sceKernelAvailableDirectMemorySize(top, top + window, 0x1000000, &start, &size) == 0);
    Require(start == top + 0x1000000 && size == static_cast<std::size_t>(window - 0x1000000));
    Require(sceKernelAvailableDirectMemorySize(top, top + 0x200000, 0, &start, &size) == SCE_KERNEL_ERROR_ENOMEM);
    Require(start == 0 && size == 0);
    Require(sceKernelAvailableDirectMemorySize(top + window, top, 0, &start, &size) == SCE_KERNEL_ERROR_EAGAIN);
    Require(sceKernelAvailableDirectMemorySize(-1, top + window, 0, &start, &size) == SCE_KERNEL_ERROR_EAGAIN);
    Require(sceKernelAvailableDirectMemorySize(top, top + window, std::numeric_limits<std::size_t>::max(), &start, &size) == SCE_KERNEL_ERROR_EAGAIN);
    Require(sceKernelAvailableDirectMemorySize(top, top + window, 0, nullptr, &size) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelReleaseDirectMemory(first, 0x200000) == 0);
    Require(sceKernelReleaseDirectMemory(last, 0x200000) == 0);
    Require(sceKernelAvailableDirectMemorySize(top, top + window, 0, &start, &size) == 0);
    Require(start == top && size == static_cast<std::size_t>(window));
}

static void CheckMunmapOverUnmappedPages() {
    constexpr std::size_t page = 0x4000;
    const auto failed = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    void* raw = mmap_nid_postfix(nullptr, page * 3, 3, 0x1002, -1, 0);
    Require(raw != failed);
    auto* memory = static_cast<unsigned char*>(raw);
    memory[0] = 1;
    memory[page * 2] = 2;
    Require(munmap_nid_postfix(memory + page, page) == 0);
    Require(munmap_nid_postfix(memory, page * 3) == 0);
    {
        GuestAllocations::Mutation mutation;
        Require(!mutation.Overlaps(memory, page * 3));
    }
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(memory, 0, &info, sizeof(info)) == SCE_KERNEL_ERROR_EACCES);
    Require(sceKernelVirtualQuery(memory + page * 2, 0, &info, sizeof(info)) == SCE_KERNEL_ERROR_EACCES);
    Require(munmap_nid_postfix(memory, page * 3) == 0);
    Require(sceKernelMunmap(memory, page * 3) == 0);
    Require(sceKernelMunmap(memory, 0) == SCE_KERNEL_ERROR_EINVAL);
}

int main(int argc, char** argv) {
#ifdef _WIN32
    _putenv_s("APS5_PIN_WAIT_MS", "1000");
#endif
    if (argc == 2 && std::strcmp(argv[1], "fixed-anonymous") == 0) {
        CheckPosixFixedAnonymousMapping();
        CheckPosixFixedAnonymousReleasesFlexibleAccounting();
#if defined(__linux__)
        CheckPosixFixedFileReleasesFlexibleAccounting();
#endif
        return 0;
    }
    Require(argc == 1);
    CheckAvailableDirectMemoryIsLargestFreeBlock();
    CheckVirtualQuerySplitFlexibleRanges();
    CheckVirtualQueryForNonReadableGuestRange();
    CheckReleaseFlexibleMemory();
    CheckFlexibleMappingIgnoresFlag8000();
    CheckSceKernelMmap();
    CheckInternalMemoryAvailableSize();
    CheckVirtualQueryPartialMunmap();
    CheckMunmapOverUnmappedPages();
    CheckPageTableStats();
    CheckVirtualQueryWireLayout();
    CheckNamedAndHintedMappings();
    CheckInternalNamedFlexibleMapping();
    CheckFlexibleFixedReplacement();
    CheckMunmapScopedFlexibleRelease();
    CheckGuestModuleUsesFlexibleMemory();
    CheckBatchMapStopsAtInvalidEntry();
    CheckCheckedReleaseDirectMemory();
    CheckAudioCoprocessorProtection();
    CheckMprotectIgnoresUndefinedBits();
    CheckStrictMemoryProtectionBits();
    CheckPosixMprotectIgnoresUndefinedBits();
    CheckDirectMemoryFollowsPhysicalPages();
    CheckSparseDirectMemoryBacking();
    CheckSavedDirectMemorySurvivesCachePressure();
    CheckReusedDirectMemorySections();
    CheckReleaseDirectMemoryClearsMappings();
    CheckReleaseDirectMemoryRejectsInvalidRanges();
    CheckFixedMappingReplacesPartialOverlap();
    CheckFixedMappingOverHoleInsideAllocation();
    CheckDirectMemoryGpuProtBits();
    CheckSplitPrivateCollect();
    CheckGpuAccessFollowsProtection();
    CheckGpuMapsAreObserved();
    CheckClearingTheObserverWaitsForItsCalls();
    CheckHintedVirtualReservation();
    CheckReservationHintWithoutRoomAbove();
    CheckFixedVirtualReservation();
    CheckReservedRangeIsNotCommitted();
    CheckHintInsideReservation();
    CheckNoOverwriteRefusesLiveMapping();
    CheckMlock();
    CheckSharedDirectMemoryLifecycle();
    CheckGetDirectMemoryType();
    CheckMtypeprotect();
    CheckHeapAlignment();
    CheckHeapAfterMappingReuse();
    CheckNoOverwriteRejectsHostOccupiedMapping();
    CheckFixedMappingsReachTheApplicationAreaEnd();
#if defined(_WIN32) || defined(__linux__)
    CheckGuestModuleImageProtection();
#endif
#ifdef _WIN32
    CheckAliasAcrossDirectAllocations();
    CheckAliasOfReorderedSharedPages();
    CheckAliasRefusesPagesThatAreNotSharedViews();
    CheckAliasLeavesResidentRunsUnderAHostWrite();
#endif
    CheckUnhintedKernelMappingsLandInTheWindow();
#if defined(__linux__)
    CheckUnmappedArenaRangeStaysReserved();
#endif
    CheckArenaPlacesUnhintedMappingsInsideTheWindow();
    CheckArenaReusesAFreedRange();
    CheckArenaHonoursHintsWithoutGoingBelowThem();
    CheckSharedWriteTracking();
    CheckSharedWritePages();
    CheckSharedWritesDuringHostWrite();
    CheckReadsIntoSharedWriteTracking();
    CheckPinnedSharedPages();
    CheckFailedCollectKeepsWrites();
    CheckSingleDirectMappingTracksWritesWithoutFaults();
    CheckLargeResidentWritesStopFaulting();
    CheckImportAccessDoesNotInventSharedWrites();
    CheckResidentCollectPreservesPartialWrites();
    CheckResidentCollectionDuringHostWrites();
    CheckSecondDirectMappingWaitsForLeases();
    CheckReleaseKeepsPhysicalUntilViewsUnmapped(sceKernelReleaseDirectMemory);
    CheckReleaseKeepsPhysicalUntilViewsUnmapped(sceKernelCheckedReleaseDirectMemory);
    CheckConcurrentCheckedReleasesUnmapViewsOnce();
    CheckBreakingAResidentRunKeepsConcurrentAccesses();
    CheckRemappedDirectMemoryIsChargedOnce();
#if defined(__linux__)
    CheckWriteWatch();
    CheckWriteWatchArmedCollect();
    CheckWriteWatchFreshCollect();
    CheckDirectMemoryWriteWatch();
    CheckDirectMemoryWriteWatchFollowsRemovedMappings();
    CheckMappingCostFollowsTheMappingCount();
    CheckDirectMemoryBackingNeedsNoFilesystem();
    CheckDirectMemorySharedBacking();
    CheckVirtualQueryFindsNeighbours();
    CheckReleaseFindsEveryDirectMapping();
#endif
#if defined(__linux__) || defined(_WIN32)
    CheckRegularFileMapping();
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
    reject(page, 3, 0x1012, -1, 0, 22);
    reject(page, 3, 0x2, 0, 0, 45);    // file-backed
    reject(page, 3, 0x22, -1, 0, 45);  // Linux MAP_ANON is not guest MAP_ANON
    reject(page, 3, 0x1003, -1, 0, 22);
    reject(page, 3, 0x3, 0, 0, 22);
    auto* anonymousOnly = static_cast<volatile unsigned char*>(mmap_nid_postfix(nullptr, page, 3, 0x1000, -1, 0));
    Require(anonymousOnly != failed && (reinterpret_cast<std::uintptr_t>(anonymousOnly) & (page - 1)) == 0);
    anonymousOnly[page - 1] = 7;
    Require(anonymousOnly[page - 1] == 7 && anonymousOnly[0] == 0);
    Require(munmap_nid_postfix(const_cast<unsigned char*>(anonymousOnly), page) == 0);
    reject(page, 3, 0x1000, 0, 0, 22);

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
    for (int advice = 0; advice <= 10; ++advice) Require(madvise_nid_postfix(memory, page * 3, advice) == 0);
    Require(madvise_nid_postfix(reinterpret_cast<void*>(0x800000000000), 1, 10) == 0);
    Require(madvise_nid_postfix(memory, std::numeric_limits<std::size_t>::max(), 10) == 0);
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
    char mincoreVec[4] = {};
    Require(mincore_nid_postfix(memory, page * 3, mincoreVec) == 0);
    Require(mincoreVec[0] == 1 && mincoreVec[1] == 1 && mincoreVec[2] == 1);
    mincoreVec[0] = mincoreVec[1] = 0x55;
    Require(mincore_nid_postfix(memory + 1, page, mincoreVec) == 0 && mincoreVec[0] == 1 && mincoreVec[1] == 0x55);
    Require(mincore_nid_postfix(memory, 0, nullptr) == 0);
    Require(mincore_nid_postfix(reinterpret_cast<void*>(0x800000000000), 0, nullptr) == -1 && *__error_nid_postfix() == 12);
    *__error_nid_postfix() = 0;
    Require(mincore_nid_postfix(memory, page, nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(mincore_nid_postfix(reinterpret_cast<void*>(0x800000000000), page, nullptr) == -1 && *__error_nid_postfix() == 12);
    *__error_nid_postfix() = 0;
    Require(mincore_nid_postfix(reinterpret_cast<void*>(0x800000000000), page, mincoreVec) == -1 && *__error_nid_postfix() == 12);
    *__error_nid_postfix() = 0;
    Require(mincore_nid_postfix(memory, std::numeric_limits<std::size_t>::max(), mincoreVec) == -1 && *__error_nid_postfix() == 12);
    *__error_nid_postfix() = 0;
    void* mincoreReserved = nullptr;
    Require(sceKernelReserveVirtualRange(&mincoreReserved, page, 0, 0) == 0);
    mincoreVec[0] = 0x55;
    Require(mincore_nid_postfix(mincoreReserved, page, mincoreVec) == 0 && mincoreVec[0] == 0);
    Require(sceKernelMunmap(mincoreReserved, page) == 0);
    Require(munmap_nid_postfix(memory + 1, page) == -1 && *__error_nid_postfix() == 22);
    Require(munmap_nid_postfix(memory, 0) == -1 && *__error_nid_postfix() == 22);
    Require(memory[0] == 42);
    Require(munmap_nid_postfix(memory + page, 1) == 0); // round to one guest page
    Require(memory[0] == 42 && memory[page * 2] == 73);
    Require(mincore_nid_postfix(memory, page, mincoreVec) == 0 && mincoreVec[0] == 1);
    Require(mincore_nid_postfix(memory, page * 3, mincoreVec) == -1 && *__error_nid_postfix() == 12);
    *__error_nid_postfix() = 0;
    Require(munmap_nid_postfix(memory, page) == 0);
    Require(memory[page * 2] == 73);
    Require(munmap_nid_postfix(memory + page * 2, page) == 0);
    Require(munmap_nid_postfix(memory, page) == 0);
    Require(madvise_nid_postfix(memory, page, 4) == 0);
    Require(mincore_nid_postfix(memory, page, mincoreVec) == -1 && *__error_nid_postfix() == 12);
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

    reject(page, 3, 0x2001002, -1, 0, 45);
    constexpr std::size_t superpage = std::size_t{1} << 21;
    auto* aligned = static_cast<unsigned char*>(mmap_nid_postfix(nullptr, superpage + page, 3, 0x1001002, -1, 0));
    Require(aligned != failed && (reinterpret_cast<std::uintptr_t>(aligned) & (superpage - 1)) == 0);
    aligned[0] = 1;
    aligned[superpage + page - 1] = 2;
    {
        GuestAllocations::Mutation mutation;
        const auto range = mutation.Find(aligned);
        Require(range.bytes == superpage + page && range.readable && range.writable);
    }
    Require(munmap_nid_postfix(aligned, superpage + page) == 0);
}
