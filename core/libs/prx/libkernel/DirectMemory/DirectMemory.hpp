#ifndef CORE_LIBS_PRX_LIBKERNEL_DIRECTMEMORY_DIRECTMEMORY_HPP
#define CORE_LIBS_PRX_LIBKERNEL_DIRECTMEMORY_DIRECTMEMORY_HPP

#include <cstdint>
#include <cstddef>
#include <vector>

#include "prx/libkernel/KernelErrors.hpp"

static constexpr size_t DIRECT_MEMORY_SIZE = 13824ULL * 1024 * 1024;
static constexpr size_t PS5_PAGE_SIZE = 0x4000;

#ifdef _WIN32
using HostSharedMemory = void*;
#else
using HostSharedMemory = int;
#endif

int DirectMemoryAlloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physOut);
void DirectMemoryFree(int64_t start, size_t len);
bool DirectMemoryCheckedFree(int64_t start, size_t len);
bool DirectMemoryClaimRelease(int64_t start, size_t len, bool requireAllocated);
void DirectMemoryFinishRelease(int64_t start, size_t len);
void DirectMemoryAbandonRelease(int64_t start, size_t len) noexcept;
void CreateDirectMemoryBacking(int64_t start, size_t len, int memoryType);
bool QueryDirectMapping(std::uintptr_t address, std::uintptr_t* start, std::uintptr_t* end, std::uint64_t* offset, int* memoryType);
bool DirectMemoryAliased(std::uintptr_t address, std::size_t bytes);
void ForgetDirectMemory(int64_t start, size_t len);

struct DirectMemoryView {
    std::uintptr_t address;
    std::size_t bytes;
};

std::vector<DirectMemoryView> DirectMemoryViews(int64_t start, size_t len);
void UnmapDirectMemoryViews(const std::vector<DirectMemoryView>& views);
bool DirectMemoryFind(int64_t offset, bool findNext, int64_t* start, int64_t* end, int* memoryType);
void DirectMemoryRetype(int64_t start, size_t len, int memoryType);
int DirectMemoryLargestFree(int64_t searchStart, int64_t searchEnd, size_t alignment, int64_t* startOut, size_t* sizeOut);
size_t DirectMemoryFreeRun(uint64_t offset, uint64_t limit);
int DoMapDirect(void** addr, size_t len, int prot, int flags, int64_t physStart, size_t alignment);
int DoMapAnon(void** addr, size_t len, int prot, int flags, size_t alignment = PS5_PAGE_SIZE, bool exclusive = false, bool allowStackReplacement = false);
int DoMapFileBacked_nid_no_patch(void** addr, size_t len, int prot, int flags, int descriptor, std::uint64_t offset, size_t alignment);
void ReleaseFlexibleMappingRange(std::uintptr_t address, size_t len);
int DoMapShared(void** addr, size_t len, int prot, HostSharedMemory object);
int DoMprotect(const void* addr, size_t len, int prot);
int DoMtypeprotect(const void* addr, size_t len, int type, int prot);
int DoMunmap(void* addr, size_t len);
int DoReserveVirtual(void** addr, size_t len, int flags, size_t alignment);
bool GuestProtection(uintptr_t addr, int* prot);
bool GuestReservation(std::uintptr_t addr, std::uintptr_t* start, std::uintptr_t* end);
bool GuestRangeMapped(std::uintptr_t start, std::uintptr_t end);
void ForgetJitSharedMemory(int descriptor);
void DuplicateJitSharedMemory(int from, int to);
void GuestPageTableUsage(std::uint64_t granularity, std::uint64_t* cpu_entries, std::uint64_t* gpu_entries);

#endif
