#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::size_t GuestPageSize = 0x4000;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "guest kernel batch map guest pointers check failed at %s:%u\n",
            location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

std::size_t HostPageSize() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
}

void* AllocateGuardPages(std::size_t count) {
    const std::size_t page = HostPageSize();
#ifdef _WIN32
    void* pages = VirtualAlloc(nullptr, count * page, MEM_RESERVE, PAGE_NOACCESS);
    Require(pages != nullptr);
    return pages;
#else
    void* pages = mmap(nullptr, count * page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(pages != MAP_FAILED);
    return pages;
#endif
}

void ProtectPage(void* pages, std::size_t index, bool readable, bool writable) {
    const std::size_t page = HostPageSize();
#ifdef _WIN32
    const DWORD protection = readable ? (writable ? PAGE_READWRITE : PAGE_READONLY) : PAGE_NOACCESS;
    DWORD old = 0;
    Require(VirtualAlloc(pages, (index + 1) * page, MEM_COMMIT, PAGE_READWRITE) != nullptr);
    Require(VirtualProtect(static_cast<char*>(pages) + index * page, page, protection, &old) != 0);
#else
    const int protection = (readable ? PROT_READ : 0) | (writable ? PROT_WRITE : 0);
    Require(mprotect(static_cast<char*>(pages) + index * page, page, protection) == 0);
#endif
}

KernelBatchMapEntry FlexibleMapEntry() {
    KernelBatchMapEntry entry{};
    entry.operation = 3;
    entry.length = GuestPageSize;
    entry.protection = 3;
    entry.start = nullptr;
    return entry;
}

void CheckBatchMapHappyPath() {
    KernelBatchMapEntry entries[2] = {FlexibleMapEntry(), FlexibleMapEntry()};
    int processed = -1;
    Require(sceKernelBatchMap2(entries, 2, &processed, 0) == 0 && processed == 2);
    Require(entries[0].start != nullptr && entries[1].start != nullptr);
    auto* bytes = static_cast<volatile unsigned char*>(entries[0].start);
    bytes[GuestPageSize - 1] = 1;
    Require(bytes[GuestPageSize - 1] == 1);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(entries[0].start), GuestPageSize) == 0);
    KernelBatchMapEntry unmapEntry{};
    unmapEntry.operation = 1;
    unmapEntry.start = entries[1].start;
    unmapEntry.length = GuestPageSize;
    unmapEntry.protection = 3;
    processed = -1;
    Require(sceKernelBatchMap2(&unmapEntry, 1, &processed, 0) == 0 && processed == 1);
    void* flexible = nullptr;
    Require(sceKernelMapFlexibleMemory(&flexible, GuestPageSize, 3, 0) == 0 && flexible != nullptr);
    KernelMemoryPoolBatchEntry poolEntry{};
    poolEntry.op = 3;
    poolEntry.protect.addr = flexible;
    poolEntry.protect.len = GuestPageSize;
    poolEntry.protect.prot = 1;
    processed = -1;
    Require(sceKernelMemoryPoolBatch(&poolEntry, 1, &processed, 0) == 0 && processed == 1);
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(flexible, 0, &info, sizeof(info)) == 0 && info.protection == 1);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(flexible), GuestPageSize) == 0);
}

void CheckPartialProcessing() {
    KernelBatchMapEntry entries[2] = {FlexibleMapEntry(), FlexibleMapEntry()};
    entries[1].length = 0;
    int processed = -1;
    Require(sceKernelBatchMap2(entries, 2, &processed, 0) == SCE_KERNEL_ERROR_EINVAL && processed == 1);
    Require(entries[0].start != nullptr);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(entries[0].start), GuestPageSize) == 0);
}

void CheckUnwritableResultSlot() {
    void* pages = AllocateGuardPages(2);
    const std::size_t page = HostPageSize();
    const KernelBatchMapEntry entries[2] = {FlexibleMapEntry(), FlexibleMapEntry()};
    ProtectPage(pages, 0, true, true);
    ProtectPage(pages, 1, true, true);
    std::memcpy(static_cast<char*>(pages) + page - sizeof(entries[0]), entries, sizeof(entries));
    ProtectPage(pages, 1, true, false);
    auto* array = reinterpret_cast<KernelBatchMapEntry*>(static_cast<char*>(pages) + page - sizeof(entries[0]));
    int processed = -1;
    Require(sceKernelBatchMap2(array, 2, &processed, 0) == SCE_KERNEL_ERROR_EFAULT && processed == 1);
    Require(array[0].start != nullptr);
    void* unwritable = AllocateGuardPages(1);
    Require(sceKernelBatchMap2(array, 1, static_cast<int*>(unwritable), 0) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelMemoryPoolBatch(reinterpret_cast<const KernelMemoryPoolBatchEntry*>(array), 1,
        static_cast<int*>(unwritable), 0) == SCE_KERNEL_ERROR_EFAULT);
}

void CheckUnreadableEntriesArray() {
    auto* pages = AllocateGuardPages(2);
    auto* batchEntries = static_cast<KernelBatchMapEntry*>(pages);
    int processed = -1;
    Require(sceKernelBatchMap2(batchEntries, 1, &processed, 0) == SCE_KERNEL_ERROR_EFAULT && processed == 0);
    processed = -1;
    Require(sceKernelBatchMap2(batchEntries, 2, &processed, 0) == SCE_KERNEL_ERROR_EFAULT && processed == 0);
    processed = -1;
    Require(sceKernelBatchMap(batchEntries, 1, &processed) == SCE_KERNEL_ERROR_EFAULT && processed == 0);
    auto* poolEntries = static_cast<KernelMemoryPoolBatchEntry*>(pages);
    processed = -1;
    Require(sceKernelMemoryPoolBatch(poolEntries, 1, &processed, 0) == SCE_KERNEL_ERROR_EFAULT && processed == 0);
    Require(sceKernelBatchMap2(nullptr, 1, &processed, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelMemoryPoolBatch(nullptr, 0, &processed, 0) == SCE_KERNEL_ERROR_EINVAL);
}

void CheckPoolProtectDropsUndefinedBits() {
    void* flexible = nullptr;
    Require(sceKernelMapFlexibleMemory(&flexible, GuestPageSize, 3, 0) == 0 && flexible != nullptr);
    KernelMemoryPoolBatchEntry poolEntry{};
    poolEntry.op = 3;
    poolEntry.protect.addr = flexible;
    poolEntry.protect.len = GuestPageSize;
    poolEntry.protect.prot = 0x09;
    int processed = -1;
    Require(sceKernelMemoryPoolBatch(&poolEntry, 1, &processed, 0) == 0 && processed == 1);
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(flexible, 0, &info, sizeof(info)) == 0 && info.protection == 1);
    poolEntry.protect.prot = 0x0B;
    processed = -1;
    Require(sceKernelMemoryPoolBatch(&poolEntry, 1, &processed, 0) == 0 && processed == 1);
    Require(sceKernelVirtualQuery(flexible, 0, &info, sizeof(info)) == 0 && info.protection == 3);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(flexible), GuestPageSize) == 0);
}

} // namespace

int main() {
    CheckBatchMapHappyPath();
    CheckPartialProcessing();
    CheckUnwritableResultSlot();
    CheckUnreadableEntriesArray();
    CheckPoolProtectDropsUndefinedBits();
    return 0;
}
