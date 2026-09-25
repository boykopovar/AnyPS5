#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "DirectMemory.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <map>
#include <cstdio>
#include <mutex>

namespace {

constexpr size_t FLEXIBLE_MEMORY_SIZE = 448ULL * 1024 * 1024;

constexpr int PRT_APERTURE_COUNT = 3;

struct PrtAperture {
    void* address;
    size_t length;
};

std::mutex g_prtLock;
PrtAperture g_prtApertures[PRT_APERTURE_COUNT] = {};

std::mutex g_flexibleLock;
std::map<uintptr_t, size_t> g_flexibleRanges;

size_t _flexibleUsedLocked() {
    size_t used = 0;
    for (const auto& [start, len] : g_flexibleRanges) used += len;
    return used;
}

int _mapFlexible(void** addr, size_t len, int prot, int flags) {
    std::lock_guard lock(g_flexibleLock);
    if (len > FLEXIBLE_MEMORY_SIZE - std::min(FLEXIBLE_MEMORY_SIZE, _flexibleUsedLocked())) {
        std::fprintf(stderr, "[memory] flexible memory exhausted: request 0x%zx with 0x%zx of 0x%zx in use\n", len, _flexibleUsedLocked(), static_cast<size_t>(FLEXIBLE_MEMORY_SIZE));
        return SCE_KERNEL_ERROR_ENOMEM;
    }
    const int result = DoMapAnon(addr, len, prot, flags);
    if (result == 0) g_flexibleRanges[reinterpret_cast<uintptr_t>(*addr)] = len;
    return result;
}

void _releaseFlexible(uintptr_t start, size_t len) {
    std::lock_guard lock(g_flexibleLock);
    const uintptr_t end = start + len;
    auto it = g_flexibleRanges.upper_bound(start);
    if (it != g_flexibleRanges.begin()) --it;
    while (it != g_flexibleRanges.end() && it->first < end) {
        const uintptr_t rangeStart = it->first;
        const uintptr_t rangeEnd = rangeStart + it->second;
        if (rangeEnd <= start) { ++it; continue; }
        it = g_flexibleRanges.erase(it);
        if (rangeStart < start) g_flexibleRanges[rangeStart] = start - rangeStart;
        if (rangeEnd > end) g_flexibleRanges[end] = rangeEnd - end;
    }
}

}

extern "C" {

int APS5_VABI sceKernelAllocateDirectMemory(int64_t search_start, int64_t search_end, size_t len, size_t alignment, int memory_type, int64_t* phys_addr_out) {
 (void)memory_type;
 if (search_start < 0 || search_end <= search_start || len == 0
  || (len & (PS5_PAGE_SIZE - 1)) || !phys_addr_out
  || (alignment != 0 && (alignment & (PS5_PAGE_SIZE - 1))))
  return SCE_KERNEL_ERROR_EINVAL;
 return DirectMemoryAlloc(search_start, search_end, len, alignment, phys_addr_out);
}

int APS5_VABI sceKernelAllocateMainDirectMemory(size_t len, size_t alignment, int memory_type, int64_t* phys_addr_out) {
 return sceKernelAllocateDirectMemory(0, static_cast<int64_t>(DIRECT_MEMORY_SIZE), len, alignment, memory_type, phys_addr_out);
}

int APS5_VABI sceKernelAvailableDirectMemorySize(int64_t search_start, int64_t search_end, size_t alignment, int64_t* phys_addr_out, size_t* size_out) {
 if (!phys_addr_out || !size_out) return SCE_KERNEL_ERROR_EINVAL;
 int64_t tmpPhys = 0;
 int ret = DirectMemoryAlloc(search_start, search_end, PS5_PAGE_SIZE, alignment, &tmpPhys);
 if (ret != 0) { *phys_addr_out = 0; *size_out = 0; return ret; }
 DirectMemoryFree(tmpPhys, PS5_PAGE_SIZE);
 *phys_addr_out = tmpPhys;
 *size_out = static_cast<size_t>(search_end) - static_cast<size_t>(tmpPhys);
 return 0;
}

int APS5_VABI sceKernelDirectMemoryQuery(int64_t offset, int flags, void* info, size_t info_size) {
 (void)flags;
 if (!info || offset < 0) return SCE_KERNEL_ERROR_EINVAL;
 struct DirectMemoryQueryInfo { int64_t start; int64_t end; int memory_type; };
 if (info_size < sizeof(DirectMemoryQueryInfo)) return SCE_KERNEL_ERROR_EINVAL;
 auto* q = static_cast<DirectMemoryQueryInfo*>(info);
 q->start = offset & ~static_cast<int64_t>(PS5_PAGE_SIZE - 1);
 q->end = q->start + PS5_PAGE_SIZE;
 q->memory_type = 3;
 return 0;
}

size_t APS5_VABI sceKernelGetDirectMemorySize(void) {
 return DIRECT_MEMORY_SIZE;
}

int APS5_VABI sceKernelMapDirectMemory(void** addr, size_t len, int prot, int flags, int64_t direct_memory_start, size_t alignment) {
 (void)alignment;
 return DoMapDirect(addr, len, prot, flags, direct_memory_start, alignment);
}

int APS5_VABI sceKernelMapDirectMemory2(void** addr, size_t len, int type, int prot, int flags, int64_t direct_memory_start, size_t alignment) {
 (void)type;
 return DoMapDirect(addr, len, prot, flags, direct_memory_start, alignment);
}

int APS5_VABI sceKernelMapFlexibleMemory(void** addr_in_out, size_t len, int prot, int flags) {
 return _mapFlexible(addr_in_out, len, prot, flags);
}

int APS5_VABI sceKernelMapNamedDirectMemory(void** addr, size_t len, int prot, int flags, int64_t direct_memory_start, size_t alignment, const char* name) {
 (void)name;
 return DoMapDirect(addr, len, prot, flags, direct_memory_start, alignment);
}

int32_t APS5_VABI sceKernelMapNamedFlexibleMemory(void** addr_in_out, size_t len, int prot, int flags, const char* name) {
 (void)name;
 return _mapFlexible(addr_in_out, len, prot, flags);
}

int APS5_VABI sceKernelMprotect(const void* addr, size_t len, int prot) {
 return DoMprotect(addr, len, prot);
}

int APS5_VABI sceKernelMunmap(uint64_t vaddr, size_t len) {
 const int result = DoMunmap(reinterpret_cast<void*>(vaddr), len);
 if (result == 0) _releaseFlexible(static_cast<uintptr_t>(vaddr), len);
 return result;
}

int APS5_VABI sceKernelReleaseDirectMemory(int64_t start, size_t len) {
 if (start < 0 || len == 0) return SCE_KERNEL_ERROR_EINVAL;
 DirectMemoryFree(start, len);
 return 0;
}

int APS5_VABI sceKernelReserveVirtualRange(void** addr, size_t len, int flags, size_t alignment) {
 (void)flags;
 return DoReserveVirtual(addr, len, alignment);
}

int APS5_VABI sceKernelVirtualQuery(const void* addr, int flags, VirtualQueryInfo* info, uint64_t info_size) {
 if (!info || info_size < sizeof(VirtualQueryInfo)) return SCE_KERNEL_ERROR_EINVAL;
 memset(info, 0, sizeof(VirtualQueryInfo));
 const auto address = reinterpret_cast<uintptr_t>(addr);
 // The mapping that contains the address, or with SCE_KERNEL_VQ_FIND_NEXT (flags bit 0) the first
 // mapping at or above it: titles walk their mappings and check that a mapping covers a whole
 // allocation, so the answer must be the registered allocation, not a page.
 constexpr int findNext = 1;
 const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
 const GuestAllocations::Range* best = nullptr;
 for (const auto& range : lease) {
  const auto begin = range->allocationAddress;
  const auto end = begin + range->allocationBytes;
  if (address >= begin && address < end) { best = range.get(); break; }
  if ((flags & findNext) != 0 && begin > address && (best == nullptr || begin < best->allocationAddress)) best = range.get();
 }
 if (best != nullptr) {
  info->start = best->allocationAddress;
  info->end = best->allocationAddress + best->allocationBytes;
  info->protection = (best->readable ? 1 : 0) | (best->writable ? 2 : 0) | (!best->releasable ? 4 : 0);
  info->is_direct = best->releasable ? 1u : 0u;
  info->is_committed = 1;
  return 0;
 }
 // Memory the registry does not know (the title's own heap blocks, stacks): the host's committed
 // region around the address is the honest extent; unmapped memory is an error, as on the PS5.
 MEMORY_BASIC_INFORMATION host{};
 if (VirtualQuery(addr, &host, sizeof(host)) == 0 || host.State != MEM_COMMIT) return SCE_KERNEL_ERROR_EACCES;
 info->start = reinterpret_cast<uintptr_t>(host.BaseAddress);
 info->end = info->start + host.RegionSize;
 const bool writable = (host.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) != 0;
 const bool executable = (host.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
 info->protection = 1 | (writable ? 2 : 0) | (executable ? 4 : 0);
 info->is_flexible = 1;
 info->is_committed = 1;
 return 0;
}

// ---------------------------------------------------------------------------
// Moved as-is (not yet implemented) from the monolithic libkernel/Export.cpp.
// ---------------------------------------------------------------------------

int APS5_VABI sceKernelCheckedReleaseDirectMemory(int64_t start, size_t len) {
 (void)start;
 (void)len;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelMtypeprotect(const void* addr, size_t len, int type, int prot) {
 (void)addr;
 (void)len;
 (void)type;
 (void)prot;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelQueryMemoryProtection(void* addr, void** start, void** end, int* prot) {
 (void)addr;
 (void)start;
 (void)end;
 (void)prot;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelIsStack(void* addr, void** start, void** end) {
 (void)addr;
 (void)start;
 (void)end;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelAvailableFlexibleMemorySize(size_t* size) {
 if (!size) return SCE_KERNEL_ERROR_EINVAL;
 std::lock_guard lock(g_flexibleLock);
 *size = FLEXIBLE_MEMORY_SIZE - std::min(FLEXIBLE_MEMORY_SIZE, _flexibleUsedLocked());
 return 0;
}

int APS5_VABI sceKernelConfiguredFlexibleMemorySize(size_t* size) {
 if (!size) return SCE_KERNEL_ERROR_EINVAL;
 *size = FLEXIBLE_MEMORY_SIZE;
 return 0;
}

int APS5_VABI sceKernelSetVirtualRangeName(const void* addr, uint64_t len, const char* name) {
 (void)addr;
 (void)len;
 (void)name;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelGetPageTableStats(int* cpu_total, int* cpu_available, int* gpu_total, int* gpu_available) {
 (void)cpu_total;
 (void)cpu_available;
 (void)gpu_total;
 (void)gpu_available;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelGetPrtAperture(int index, void** addr, size_t* len) {
 if (index < 0 || index >= PRT_APERTURE_COUNT || !addr || !len) return SCE_KERNEL_ERROR_EINVAL;
 std::lock_guard lock(g_prtLock);
 *addr = g_prtApertures[index].address;
 *len = g_prtApertures[index].length;
 return 0;
}

int APS5_VABI sceKernelSetPrtAperture(int index, void* addr, size_t len) {
 if (index < 0 || index >= PRT_APERTURE_COUNT) return SCE_KERNEL_ERROR_EINVAL;
 std::lock_guard lock(g_prtLock);
 g_prtApertures[index] = {addr, len};
 return 0;
}

int APS5_VABI sceKernelBatchMap2(KernelBatchMapEntry* entries, int num_entries, int* num_entries_out, int flags);

int APS5_VABI sceKernelBatchMap(KernelBatchMapEntry* entries, int num_entries, int* num_entries_out) {
 constexpr int MapFixed = 0x10;
 return sceKernelBatchMap2(entries, num_entries, num_entries_out, MapFixed);
}

int APS5_VABI sceKernelBatchMap2(KernelBatchMapEntry* entries, int num_entries, int* num_entries_out, int flags) {
 if (!entries || num_entries < 0) return SCE_KERNEL_ERROR_EINVAL;
 constexpr int OpMapDirect = 0;
 constexpr int OpUnmap = 1;
 constexpr int OpProtect = 2;
 constexpr int OpMapFlexible = 3;
 constexpr int OpTypeProtect = 4;
 int processed = 0;
 int result = 0;
 for (; processed < num_entries; ++processed) {
  auto& entry = entries[processed];
  switch (entry.operation) {
  case OpMapDirect:
   result = DoMapDirect(&entry.start, entry.length, static_cast<uint8_t>(entry.protection), flags, static_cast<int64_t>(entry.offset), 0);
   break;
  case OpUnmap:
   result = sceKernelMunmap(reinterpret_cast<uint64_t>(entry.start), entry.length);
   break;
  case OpProtect:
  case OpTypeProtect:
   result = DoMprotect(entry.start, entry.length, static_cast<uint8_t>(entry.protection));
   break;
  case OpMapFlexible:
   result = _mapFlexible(&entry.start, entry.length, static_cast<uint8_t>(entry.protection), flags);
   break;
  default:
   throw std::invalid_argument("sceKernelBatchMap2: unsupported operation " + std::to_string(entry.operation));
  }
  if (result != 0) break;
 }
 if (num_entries_out) *num_entries_out = processed;
 return result;
}

}
