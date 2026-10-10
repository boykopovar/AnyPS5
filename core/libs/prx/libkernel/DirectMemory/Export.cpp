#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fstream>
#include <sstream>
#include <sys/mman.h>
#include <sys/resource.h>
#endif
#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <iterator>
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

struct NamedRange {
 uintptr_t end;
 std::string name;
};

std::mutex g_rangeNameLock;
std::map<uintptr_t, NamedRange> g_rangeNames;

void EraseRangeNames(uintptr_t start, uintptr_t end) {
 auto it = g_rangeNames.lower_bound(start);
 if (it != g_rangeNames.begin() && std::prev(it)->second.end > start) --it;
 while (it != g_rangeNames.end() && it->first < end) {
  const auto rangeStart = it->first;
  const auto range = it->second;
  it = g_rangeNames.erase(it);
  if (rangeStart < start) g_rangeNames.emplace(rangeStart, NamedRange{start, range.name});
  if (range.end > end) it = g_rangeNames.emplace(end, NamedRange{range.end, range.name}).first;
 }
}

void ApplyRangeName(uintptr_t address, VirtualQueryInfo* info) {
 std::lock_guard lock(g_rangeNameLock);
 auto next = g_rangeNames.upper_bound(address);
 if (next != g_rangeNames.begin()) {
  const auto containing = std::prev(next);
  if (address < containing->second.end) {
   info->start = std::max<uintptr_t>(info->start, containing->first);
   info->end = std::min<uintptr_t>(info->end, containing->second.end);
   std::strncpy(info->name, containing->second.name.c_str(), sizeof(info->name) - 1);
   return;
  }
  info->start = std::max<uintptr_t>(info->start, containing->second.end);
 }
 if (next != g_rangeNames.end()) info->end = std::min<uintptr_t>(info->end, next->first);
}

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
 if (search_start < 0 || search_end <= search_start || len == 0
  || (len & (PS5_PAGE_SIZE - 1)) || !phys_addr_out
  || (alignment != 0 && (alignment & (PS5_PAGE_SIZE - 1))))
  return SCE_KERNEL_ERROR_EINVAL;
 return DirectMemoryAlloc(search_start, search_end, len, alignment, memory_type, phys_addr_out);
}

int APS5_VABI sceKernelAllocateMainDirectMemory(size_t len, size_t alignment, int memory_type, int64_t* phys_addr_out) {
 return sceKernelAllocateDirectMemory(0, static_cast<int64_t>(DIRECT_MEMORY_SIZE), len, alignment, memory_type, phys_addr_out);
}

int APS5_VABI sceKernelAvailableDirectMemorySize(int64_t search_start, int64_t search_end, size_t alignment, int64_t* phys_addr_out, size_t* size_out) {
 if (!phys_addr_out || !size_out) return SCE_KERNEL_ERROR_EINVAL;
 int64_t tmpPhys = 0;
 int ret = DirectMemoryAlloc(search_start, search_end, PS5_PAGE_SIZE, alignment, -1, &tmpPhys);
 if (ret != 0) { *phys_addr_out = 0; *size_out = 0; return ret; }
 DirectMemoryFree(tmpPhys, PS5_PAGE_SIZE);
 *phys_addr_out = tmpPhys;
 *size_out = DirectMemoryFreeRun(static_cast<uint64_t>(tmpPhys), static_cast<uint64_t>(search_end));
 return 0;
}

int APS5_VABI sceKernelDirectMemoryQuery(int64_t offset, int flags, void* info, size_t info_size) {
 constexpr int SCE_KERNEL_DMQ_FIND_NEXT = 1;
 if (!info || offset < 0) return SCE_KERNEL_ERROR_EINVAL;
 struct DirectMemoryQueryInfo { int64_t start; int64_t end; int memory_type; };
 if (info_size < sizeof(DirectMemoryQueryInfo)) return SCE_KERNEL_ERROR_EINVAL;
 auto* q = static_cast<DirectMemoryQueryInfo*>(info);
 if (!DirectMemoryFind(offset, (flags & SCE_KERNEL_DMQ_FIND_NEXT) != 0, &q->start, &q->end, &q->memory_type)) return SCE_KERNEL_ERROR_EACCES;
 return 0;
}

int APS5_VABI sceKernelGetDirectMemoryType(int64_t offset, int* memory_type, int64_t* start, int64_t* end) {
 if (!memory_type || !start || !end) return SCE_KERNEL_ERROR_EINVAL;
 int64_t blockStart = 0;
 int64_t blockEnd = 0;
 int blockType = 0;
 if (!DirectMemoryFind(offset, false, &blockStart, &blockEnd, &blockType)) return SCE_KERNEL_ERROR_ENOENT;
 *memory_type = blockType;
 *start = blockStart;
 *end = blockEnd;
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

int APS5_VABI sceKernelSetVirtualRangeName(const void* addr, uint64_t len, const char* name);

int APS5_VABI sceKernelMapNamedDirectMemory(void** addr, size_t len, int prot, int flags, int64_t direct_memory_start, size_t alignment, const char* name) {
 const int result = DoMapDirect(addr, len, prot, flags, direct_memory_start, alignment);
 if (result == 0 && name) sceKernelSetVirtualRangeName(*addr, len, name);
 return result;
}

int32_t APS5_VABI sceKernelMapNamedFlexibleMemory(void** addr_in_out, size_t len, int prot, int flags, const char* name) {
 const int result = _mapFlexible(addr_in_out, len, prot, flags);
 if (result == 0 && name) sceKernelSetVirtualRangeName(*addr_in_out, len, name);
 return result;
}

int32_t APS5_VABI sceKernelMapNamedFlexibleMemoryInternal(void** addr_in_out, size_t len, int prot, int flags, const char* name) {
 constexpr int IgnoredInternalFlag = 0x8000;
 return sceKernelMapNamedFlexibleMemory(addr_in_out, len, prot, flags & ~IgnoredInternalFlag, name);
}

int APS5_VABI sceKernelMprotect(const void* addr, size_t len, int prot) {
 return DoMprotect(addr, len, prot);
}

int APS5_VABI sceKernelMunmap(uint64_t vaddr, size_t len) {
 const int result = DoMunmap(reinterpret_cast<void*>(vaddr), len);
 if (result == 0) _releaseFlexible(static_cast<uintptr_t>(vaddr), len);
 return result;
}

int APS5_VABI sceKernelReleaseFlexibleMemory(void* addr, size_t len) {
 return sceKernelMunmap(reinterpret_cast<uint64_t>(addr), len);
}

int APS5_VABI sceKernelReleaseDirectMemory(int64_t start, size_t len) {
 if (start < 0 || len == 0) return SCE_KERNEL_ERROR_EINVAL;
 DirectMemoryFree(start, len);
 return 0;
}

int APS5_VABI sceKernelReserveVirtualRange(void** addr, size_t len, int flags, size_t alignment) {
 return DoReserveVirtual(addr, len, flags, alignment);
}

int APS5_VABI sceKernelVirtualQuery(const void* addr, int flags, VirtualQueryInfo* info, uint64_t info_size) {
 if (!info || info_size < sizeof(VirtualQueryInfo)) return SCE_KERNEL_ERROR_EINVAL;
 memset(info, 0, sizeof(VirtualQueryInfo));
 const auto address = reinterpret_cast<uintptr_t>(addr);
 constexpr int findNext = 1;
 std::uintptr_t reservedStart = 0;
 std::uintptr_t reservedEnd = 0;
 if (GuestReservation(address, &reservedStart, &reservedEnd)) {
  info->start = reservedStart;
  info->end = reservedEnd;
  return 0;
 }
 const auto lease = GuestAllocations::GuestAllocationsAcquireAll_nid_postfix();
 assert(std::is_sorted(lease.begin(), lease.end(), [](const auto& left, const auto& right) { return left->address < right->address; }));
 const GuestAllocations::Range* best = nullptr;
 for (const auto& range : lease) {
  const auto begin = range->address;
  const auto end = begin + range->bytes;
  if (address >= begin && address < end) { best = range.get(); break; }
  if ((flags & findNext) != 0 && begin > address && (best == nullptr || begin < best->address)) best = range.get();
 }
 if (best != nullptr) {
  info->start = best->address;
  info->end = best->address + best->bytes;
  info->protection = (best->readable ? 1 : 0) | (best->writable ? 2 : 0) | (!best->releasable ? 4 : 0);
  int recorded = 0;
  if (GuestProtection(std::max<uintptr_t>(address, info->start), &recorded)) info->protection = recorded;
  std::uintptr_t directStart = 0;
  std::uintptr_t directEnd = 0;
  std::uint64_t physicalOffset = 0;
  int memoryType = 0;
  const bool direct = QueryDirectMapping(std::max<uintptr_t>(address, info->start), &directStart, &directEnd, &physicalOffset, &memoryType);
  info->is_direct = direct ? 1u : 0u;
  info->is_flexible = !direct && best->releasable ? 1u : 0u;
  if (direct) {
   info->start = std::max(info->start, directStart);
   info->end = std::min(info->end, directEnd);
   info->memory_type = memoryType;
  }
  info->is_committed = 1;
  ApplyRangeName(std::max<uintptr_t>(address, info->start), info);
  if (direct) info->offset = physicalOffset + info->start - directStart;
  return 0;
 }
 // Memory the registry does not know (the title's own heap blocks, stacks): the host's committed
 // region around the address is the honest extent; unmapped memory is an error, as on the PS5.
 #ifdef _WIN32
 MEMORY_BASIC_INFORMATION host{};
 if (VirtualQuery(addr, &host, sizeof(host)) == 0 || host.State != MEM_COMMIT) return SCE_KERNEL_ERROR_EACCES;
 info->start = reinterpret_cast<uintptr_t>(host.BaseAddress);
 info->end = info->start + host.RegionSize;
 const bool writable = (host.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) != 0;
 const bool executable = (host.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
 #else
 std::ifstream maps("/proc/self/maps");
 std::string line;
 bool found = false, writable = false, executable = false;
 while (std::getline(maps, line)) {
  std::istringstream fields(line);
  uintptr_t begin = 0, end = 0;
  char dash = 0;
  std::string perms;
  if (!(fields >> std::hex >> begin >> dash >> end >> perms) || address < begin || address >= end) continue;
  if (perms.size() < 3 || perms[0] != 'r') return SCE_KERNEL_ERROR_EACCES;
  info->start = begin;
  info->end = end;
  writable = perms[1] == 'w';
  executable = perms[2] == 'x';
  found = true;
  break;
 }
 if (!found) return SCE_KERNEL_ERROR_EACCES;
 #endif
 info->protection = 1 | (writable ? 2 : 0) | (executable ? 4 : 0);
 info->is_flexible = 1;
 info->is_committed = 1;
 ApplyRangeName(std::max<uintptr_t>(address, info->start), info);
 return 0;
}

// ---------------------------------------------------------------------------
// Moved as-is (not yet implemented) from the monolithic libkernel/Export.cpp.
// ---------------------------------------------------------------------------

int APS5_VABI sceKernelCheckedReleaseDirectMemory(int64_t start, size_t len) {
 if (start < 0 || (static_cast<uint64_t>(start) & (PS5_PAGE_SIZE - 1)) != 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
 if (len == 0) return 0;
 return DirectMemoryCheckedFree(start, len) ? 0 : SCE_KERNEL_ERROR_ENOENT;
}

int APS5_VABI sceKernelMtypeprotect(const void* addr, size_t len, int type, int prot) {
 return DoMtypeprotect(addr, len, type, prot);
}

int APS5_VABI sceKernelQueryMemoryProtection(void* addr, void** start, void** end, int* prot) {
 VirtualQueryInfo info{};
 const int result = sceKernelVirtualQuery(addr, 0, &info, sizeof(info));
 if (result != 0) return result;
 if (start) *start = reinterpret_cast<void*>(info.start);
 if (end) *end = reinterpret_cast<void*>(info.end);
 if (prot) *prot = info.protection;
 return 0;
}

int APS5_VABI sceKernelIsStack(void* addr, void** start, void** end) {
 std::uintptr_t stackStart = 0;
 std::uintptr_t stackEnd = 0;
 if (!GuestThreadStack(reinterpret_cast<std::uintptr_t>(addr), &stackStart, &stackEnd)) {
  VirtualQueryInfo info{};
  if (sceKernelVirtualQuery(addr, 0, &info, sizeof(info)) != 0) return SCE_KERNEL_ERROR_EACCES;
 }
 if (start) *start = reinterpret_cast<void*>(stackStart);
 if (end) *end = reinterpret_cast<void*>(stackEnd);
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
 const auto start = reinterpret_cast<uintptr_t>(addr);
 if (!addr || len == 0 || !name || len > UINTPTR_MAX - start) return SCE_KERNEL_ERROR_EINVAL;
 std::lock_guard lock(g_rangeNameLock);
 EraseRangeNames(start, start + len);
 g_rangeNames.emplace(start, NamedRange{start + len, std::string(name, strnlen(name, 31))});
 return 0;
}

int APS5_VABI sceKernelClearVirtualRangeName(const void* addr, uint64_t len) {
 const auto start = reinterpret_cast<uintptr_t>(addr);
 if (!addr || len == 0 || len > UINTPTR_MAX - start) return SCE_KERNEL_ERROR_EINVAL;
 std::lock_guard lock(g_rangeNameLock);
 EraseRangeNames(start, start + len);
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
  if (entry.length == 0 || entry.operation < OpMapDirect || entry.operation > OpTypeProtect) {
   result = SCE_KERNEL_ERROR_EINVAL;
   break;
  }
  switch (entry.operation) {
  case OpMapDirect:
   result = DoMapDirect(&entry.start, entry.length, static_cast<uint8_t>(entry.protection), flags, static_cast<int64_t>(entry.offset), 0);
   break;
  case OpUnmap:
   result = sceKernelMunmap(reinterpret_cast<uint64_t>(entry.start), entry.length);
   break;
  case OpProtect:
   result = DoMprotect(entry.start, entry.length, static_cast<uint8_t>(entry.protection));
   break;
  case OpTypeProtect:
   result = DoMtypeprotect(entry.start, entry.length, static_cast<uint8_t>(entry.type), static_cast<uint8_t>(entry.protection));
   break;
  case OpMapFlexible:
   result = _mapFlexible(&entry.start, entry.length, static_cast<uint8_t>(entry.protection), flags);
   break;
  }
  if (result != 0) break;
 }
 if (num_entries_out) *num_entries_out = processed;
 return result;
}

}

namespace {

bool PageRange(void* address, std::uint64_t length, std::uintptr_t& start, std::uintptr_t& end) {
    constexpr std::uintptr_t pageMask = PS5_PAGE_SIZE - 1;
    const auto first = reinterpret_cast<std::uintptr_t>(address);
    if (length > UINTPTR_MAX - first || first + length > UINTPTR_MAX - pageMask) return false;
    start = first & ~pageMask;
    end = (first + length + pageMask) & ~pageMask;
    return true;
}

#ifdef _WIN32
std::mutex g_workingSetLock;

bool HostRangeAccessible(std::uintptr_t start, std::uintptr_t end) {
    for (auto cursor = start; cursor < end;) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region)) return false;
        if (region.State != MEM_COMMIT || (region.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) return false;
        cursor = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
    }
    return true;
}

int LockHostPages(std::uintptr_t start, std::uintptr_t end) {
    if (!HostRangeAccessible(start, end)) return SCE_KERNEL_ERROR_ENOMEM;
    auto* const address = reinterpret_cast<void*>(start);
    const SIZE_T bytes = end - start;
    std::lock_guard lock(g_workingSetLock);
    if (VirtualLock(address, bytes)) return 0;
    if (GetLastError() != ERROR_WORKING_SET_QUOTA) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualLock failed");
    SIZE_T minimum = 0;
    SIZE_T maximum = 0;
    DWORD flags = 0;
    if (!GetProcessWorkingSetSizeEx(GetCurrentProcess(), &minimum, &maximum, &flags)) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "GetProcessWorkingSetSizeEx failed");
    if (bytes > std::numeric_limits<SIZE_T>::max() - maximum) return SCE_KERNEL_ERROR_EAGAIN;
    if (!SetProcessWorkingSetSizeEx(GetCurrentProcess(), minimum + bytes, maximum + bytes, flags)) return SCE_KERNEL_ERROR_EAGAIN;
    if (VirtualLock(address, bytes)) return 0;
    if (GetLastError() != ERROR_WORKING_SET_QUOTA) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualLock failed");
    return SCE_KERNEL_ERROR_EAGAIN;
}

std::uintptr_t HostPageSize() {
    static const std::uintptr_t size = [] {
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        return static_cast<std::uintptr_t>(info.dwPageSize);
    }();
    return size;
}

int UnlockHostPages(std::uintptr_t start, std::uintptr_t end) {
    for (auto cursor = start; cursor < end;) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region) || region.State == MEM_FREE) return SCE_KERNEL_ERROR_ENOMEM;
        const auto next = std::min(end, reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize);
        if (region.State == MEM_RESERVE) {
            for (auto reserved = cursor; reserved < next;) {
                std::uintptr_t reservationStart = 0;
                std::uintptr_t reservationEnd = 0;
                int recorded = 0;
                if (GuestReservation(reserved, &reservationStart, &reservationEnd)) {
                    reserved = reservationEnd;
                } else if (GuestProtection(reserved, &recorded)) {
                    reserved = (reserved & ~static_cast<std::uintptr_t>(PS5_PAGE_SIZE - 1)) + PS5_PAGE_SIZE;
                } else {
                    return SCE_KERNEL_ERROR_ENOMEM;
                }
            }
        }
        cursor = next;
    }
    for (auto cursor = start; cursor < end;) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region)) return SCE_KERNEL_ERROR_ENOMEM;
        const auto next = std::min(end, reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize);
        if (region.State == MEM_COMMIT && (region.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0 && !VirtualUnlock(reinterpret_cast<void*>(cursor), next - cursor)) {
            if (GetLastError() != ERROR_NOT_LOCKED) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualUnlock failed");
            for (auto page = cursor; page < next; page += HostPageSize()) {
                if (!VirtualUnlock(reinterpret_cast<void*>(page), std::min(HostPageSize(), next - page)) && GetLastError() != ERROR_NOT_LOCKED) {
                    throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualUnlock failed");
                }
            }
        }
        cursor = next;
    }
    return 0;
}
#else
int LockHostPages(std::uintptr_t start, std::uintptr_t end) {
    auto* const address = reinterpret_cast<void*>(start);
    const std::size_t bytes = end - start;
    if (::mlock(address, bytes) == 0) return 0;
    int error = errno;
    rlimit limit{};
    if ((error == ENOMEM || error == EPERM || error == EAGAIN) && getrlimit(RLIMIT_MEMLOCK, &limit) == 0 && limit.rlim_cur < limit.rlim_max) {
        limit.rlim_cur = limit.rlim_max;
        if (setrlimit(RLIMIT_MEMLOCK, &limit) == 0) {
            if (::mlock(address, bytes) == 0) return 0;
            error = errno;
        }
    }
    switch (error) {
    case ENOMEM: return SCE_KERNEL_ERROR_ENOMEM;
    case EPERM: return SCE_KERNEL_ERROR_EPERM;
    case EAGAIN: return SCE_KERNEL_ERROR_EAGAIN;
    default: throw std::system_error(error, std::generic_category(), "mlock failed");
    }
}

int UnlockHostPages(std::uintptr_t start, std::uintptr_t end) {
    if (::munlock(reinterpret_cast<void*>(start), end - start) == 0) return 0;
    const int error = errno;
    if (error == ENOMEM) return SCE_KERNEL_ERROR_ENOMEM;
    throw std::system_error(error, std::generic_category(), "munlock failed");
}
#endif

}

extern "C" {

int APS5_VABI sceKernelMlock_nid_postfix(void* address, std::uint64_t length) {
    std::uintptr_t start = 0;
    std::uintptr_t end = 0;
    if (!PageRange(address, length, start, end)) return SCE_KERNEL_ERROR_EINVAL;
    if (start == end) return 0;
    return LockHostPages(start, end);
}

int APS5_VABI sceKernelMunlock_nid_postfix(void* address, std::uint64_t length) {
    std::uintptr_t start = 0;
    std::uintptr_t end = 0;
    if (!PageRange(address, length, start, end)) return SCE_KERNEL_ERROR_EINVAL;
    if (start == end) return 0;
    return UnlockHostPages(start, end);
}

}
