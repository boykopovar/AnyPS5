#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <array>
#include "SceTypes.hpp"
#include <cstring>
#include <exception>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <thread>
#include <utility>
#include <vector>
#if defined(__linux__)
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
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sceKernelMapNamedFlexibleMemory(void**, std::size_t, int, int, const char*);
int APS5_VABI sceKernelMapFlexibleMemory(void**, std::size_t, int, int);
int APS5_VABI sceKernelMunmap(void*, std::size_t);
int APS5_VABI sceKernelMprotect(const void*, std::size_t, int);
int APS5_VABI sceKernelVirtualQuery(const void*, int, VirtualQueryInfo*, std::uint64_t);
int APS5_VABI sceKernelSetVirtualRangeName(const void*, std::uint64_t, const char*);
int APS5_VABI sceKernelClearVirtualRangeName(const void*, std::uint64_t);
int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
int APS5_VABI sceKernelMapDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t);
int APS5_VABI sceKernelReleaseDirectMemory(std::int64_t, std::size_t);
int APS5_VABI sceKernelReserveVirtualRange(void**, std::size_t, int, std::size_t);
int APS5_VABI sceKernelMemoryPoolReserve(void*, std::size_t, std::size_t, int, void**);
int APS5_VABI sceKernelMlock_nid_postfix(void*, std::uint64_t);
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
#if defined(__linux__)
    void* hinted = first;
    Require(sceKernelMapFlexibleMemory(&hinted, length, 3, 0) == 0);
    Require(hinted > first && (reinterpret_cast<std::uintptr_t>(hinted) & 0x3fff) == 0);
    bool rejected = false;
    void* overwrite = first;
    try { sceKernelMapFlexibleMemory(&overwrite, 0x4000, 3, 0x90); } catch (const std::exception&) { rejected = true; }
    Require(rejected && overwrite == first);
    Require(sceKernelMunmap(hinted, length) == 0);
#endif
    Require(sceKernelMunmap(first, length) == 0);
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

static void CheckFixedVirtualReservation() {
    constexpr std::size_t page = 0x4000;
    void* probe = nullptr;
    Require(sceKernelReserveVirtualRange(&probe, page * 4, 0, 0) == 0);
    Require(sceKernelMunmap(probe, page * 4) == 0);
    void* const requested = static_cast<unsigned char*>(probe) + page;
    void* fixed = requested;
    Require(sceKernelReserveVirtualRange(&fixed, page * 2, 0x400010, 0) == 0);
    Require(fixed == requested);
    Require(sceKernelMunmap(fixed, page * 2) == 0);
    void* pooled = nullptr;
    Require(sceKernelMemoryPoolReserve(requested, page * 2, 0, 0x10, &pooled) == 0);
    Require(pooled == requested);
    Require(sceKernelMunmap(pooled, page * 2) == 0);
}

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

int main() {
    CheckNamedAndHintedMappings();
    CheckDirectMemoryFollowsPhysicalPages();
    CheckFixedVirtualReservation();
    CheckMlock();
    CheckSharedDirectMemoryLifecycle();
    CheckHeapAfterMappingReuse();
    CheckSharedWriteTracking();
#if defined(__linux__)
    CheckWriteWatch();
    CheckDirectMemoryWriteWatch();
    CheckDirectMemoryBackingNeedsNoFilesystem();
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
    Require(munmap_nid_postfix(memory + 1, page) == -1 && *__error_nid_postfix() == 22);
    Require(munmap_nid_postfix(memory, 0) == -1 && *__error_nid_postfix() == 22);
    Require(memory[0] == 42);
    Require(munmap_nid_postfix(memory + page, 1) == 0); // round to one guest page
    Require(memory[0] == 42 && memory[page * 2] == 73);
    Require(munmap_nid_postfix(memory, page) == 0);
    Require(memory[page * 2] == 73);
    Require(munmap_nid_postfix(memory + page * 2, page) == 0);
    Require(munmap_nid_postfix(memory, page) == -1);
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
