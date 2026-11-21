extern "C" {
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/AppMetadata/include/ParamJsonParser.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fstream>
#include <sstream>
#include <sys/mman.h>
#include <sys/resource.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
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
#include <cstdlib>
#include <mutex>
#include <utility>
#include <vector>

extern "C" std::uint64_t GuestImageSegmentBytes_nid_no_patch();

namespace {

size_t _configuredFlexibleSize() {
    static const size_t size = [] {
        constexpr std::uint64_t defaultSize = 448ULL * 1024 * 1024;
        const auto path = ResolvePath_nid_no_patch("/app0/sce_sys/param.json");
        const auto bytes = std::filesystem::exists(path) ? parseParamJson(path).flexibleMemorySizeBytes.value_or(defaultSize) : defaultSize;
        if (bytes > std::numeric_limits<size_t>::max())
            throw std::runtime_error("param.json kernel.flexibleMemorySize exceeds host size range");
        std::fprintf(stderr, "[memory] configured flexible memory: 0x%llx bytes (%llu MiB)\n", static_cast<unsigned long long>(bytes), static_cast<unsigned long long>(bytes >> 20));
        return static_cast<size_t>(bytes);
    }();
    return size;
}

bool UndefinedProtection(int prot) {
    return (prot & ~0x3F7) != 0;
}

bool RefusedFlexibleFlag(int flags) {
    switch (flags) {
        case 0x1:
        case 0x2:
        case 0x4:
        case 0x8:
        case 0x20:
        case 0x40:
        case 0x100:
        case 0x200:
        case 0x400:
        case 0x800:
        case 0x1000:
        case 0x2000:
        case 0x4000:
        case 0x10000:
        case 0x20000:
        case 0x40000:
        case 0x80000:
        case 0x100000:
            return true;
        default:
            return false;
    }
}

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

int CopyRangeName(const char* name, std::size_t maximum, std::string& copy) {
 const int savedErrno = errno;
 const int error = CopyGuestString_nid_no_patch(name, maximum, copy);
 errno = savedErrno;
 return error;
}

bool CopyMappedName(const char* name, std::string& copy) {
 constexpr std::size_t Longest = 31;
 const int error = CopyRangeName(name, Longest, copy);
 if (error != 63) return error == 0;
 char bytes[Longest];
 const int savedErrno = errno;
 const bool read = GuestCopyIn_nid_no_patch(bytes, name, sizeof(bytes));
 errno = savedErrno;
 if (read) copy.assign(bytes, sizeof(bytes));
 return read;
}

int NameVirtualRange(const void* addr, uint64_t len, std::string name) {
 const auto start = reinterpret_cast<uintptr_t>(addr);
 if (!addr || len == 0 || len > UINTPTR_MAX - start) return SCE_KERNEL_ERROR_EINVAL;
 std::lock_guard lock(g_rangeNameLock);
 EraseRangeNames(start, start + len);
 g_rangeNames.emplace(start, NamedRange{start + len, std::move(name)});
 return 0;
}

void NameMappedRange(const void* addr, uint64_t len, const char* name) {
 if (std::string copy; name && CopyMappedName(name, copy)) NameVirtualRange(addr, len, std::move(copy));
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

void ReleaseClaimedDirectMemory(int64_t start, size_t len) {
 try {
  UnmapDirectMemoryRange(start, len);
 } catch (...) {
  DirectMemoryAbandonRelease(start, len);
  throw;
 }
 DirectMemoryFinishRelease(start, len);
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

void _releaseFlexibleLocked(uintptr_t start, size_t len) {
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

int _mapFlexible(void** addr, size_t len, int prot, int flags) {
    const size_t configured = _configuredFlexibleSize();
    const size_t images = GuestImageSegmentBytes_nid_no_patch();
    std::lock_guard lock(g_flexibleLock);
    size_t replaced = 0;
    constexpr int MapFixed = 0x10;
    constexpr int MapNoOverwrite = 0x80;
    if (addr != nullptr && *addr != nullptr && (flags & (MapFixed | MapNoOverwrite)) == MapFixed) {
        const auto start = reinterpret_cast<uintptr_t>(*addr);
        if (len <= std::numeric_limits<uintptr_t>::max() - start) {
            const auto end = start + len;
            for (const auto& [rangeStart, rangeLength] : g_flexibleRanges) {
                const auto rangeEnd = rangeStart + rangeLength;
                if (rangeStart < end && rangeEnd > start) replaced += std::min(end, rangeEnd) - std::max(start, rangeStart);
            }
        }
    }
    const size_t used = images + _flexibleUsedLocked() - replaced;
    if (len > configured - std::min(configured, used)) {
        std::fprintf(stderr, "[memory] flexible memory exhausted: request 0x%zx with 0x%zx of 0x%zx in use\n", len, used, configured);
        return SCE_KERNEL_ERROR_ENOMEM;
    }
    const int result = DoMapFlexibleAnon(addr, len, prot, flags);
    if (result == 0) {
        const auto start = reinterpret_cast<uintptr_t>(*addr);
        _releaseFlexibleLocked(start, len);
        g_flexibleRanges[start] = len;
    }
    return result;
}

void _releaseFlexible(uintptr_t start, size_t len) {
    std::lock_guard lock(g_flexibleLock);
    _releaseFlexibleLocked(start, len);
}

std::mutex g_lockedRangesLock;
std::map<uintptr_t, size_t> g_lockedRanges;

void RecordLockedRangeLocked(uintptr_t start, uintptr_t end) {
    auto it = g_lockedRanges.begin();
    while (it != g_lockedRanges.end()) {
        if (it->first <= end && it->first + it->second >= start) {
            start = std::min(start, it->first);
            end = std::max(end, it->first + it->second);
            it = g_lockedRanges.erase(it);
        } else {
            ++it;
        }
    }
    g_lockedRanges[start] = end - start;
}

void RecordLockedRange(uintptr_t start, uintptr_t end) {
    std::lock_guard lock(g_lockedRangesLock);
    RecordLockedRangeLocked(start, end);
}

void ForgetLockedRangeLocked(uintptr_t start, uintptr_t end) {
    auto it = g_lockedRanges.upper_bound(start);
    if (it != g_lockedRanges.begin()) --it;
    while (it != g_lockedRanges.end() && it->first < end) {
        const uintptr_t rangeStart = it->first;
        const uintptr_t rangeEnd = rangeStart + it->second;
        if (rangeEnd <= start) { ++it; continue; }
        it = g_lockedRanges.erase(it);
        if (rangeStart < start) g_lockedRanges[rangeStart] = start - rangeStart;
        if (rangeEnd > end) g_lockedRanges[end] = rangeEnd - end;
    }
}

void ForgetLockedRange(uintptr_t start, uintptr_t end) {
    std::lock_guard lock(g_lockedRangesLock);
    ForgetLockedRangeLocked(start, end);
}

}

int APS5_VABI sceKernelGetTraceMemoryStats(std::uint64_t* first, std::uint64_t* second) {
 constexpr std::uint64_t zero = 0;
 if (first != nullptr && !GuestCopyOut_nid_no_patch(first, &zero, sizeof(zero))) return SCE_KERNEL_ERROR_EFAULT;
 if (second != nullptr && !GuestCopyOut_nid_no_patch(second, &zero, sizeof(zero))) return SCE_KERNEL_ERROR_EFAULT;
 return 0;
}

int APS5_VABI sceKernelGetPrtAperture(int index, void** addr, size_t* len) {
 if (index < 0 || index >= PRT_APERTURE_COUNT || !addr || !len) return SCE_KERNEL_ERROR_EINVAL;
 std::lock_guard lock(g_prtLock);
 if (!GuestCopyOut_nid_no_patch(addr, &g_prtApertures[index].address, sizeof(void*)) ||
     !GuestCopyOut_nid_no_patch(len, &g_prtApertures[index].length, sizeof(size_t)))
  return SCE_KERNEL_ERROR_EFAULT;
 return 0;
}

int APS5_VABI sceKernelSetPrtAperture(int index, void* addr, size_t len) {
 if (index < 0 || index >= PRT_APERTURE_COUNT) return SCE_KERNEL_ERROR_EINVAL;
 std::lock_guard lock(g_prtLock);
 g_prtApertures[index] = {addr, len};
 return 0;
}

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
  KernelBatchMapEntry entry;
  {
   const int savedErrno = errno;
   const bool read = GuestCopyIn_nid_no_patch(&entry, entries + processed, sizeof(entry));
   errno = savedErrno;
   if (!read) {
    result = SCE_KERNEL_ERROR_EFAULT;
    break;
   }
  }
  if (entry.length == 0 || entry.operation < OpMapDirect || entry.operation > OpTypeProtect) {
   result = SCE_KERNEL_ERROR_EINVAL;
   break;
  }
  switch (entry.operation) {
  case OpMapDirect:
   result = DoMapDirect(&entry.start, entry.length, static_cast<uint8_t>(entry.protection), flags, static_cast<int64_t>(entry.offset), 0);
   if (result == 0) _releaseFlexible(reinterpret_cast<uintptr_t>(entry.start), entry.length);
   break;
  case OpUnmap:
   result = sceKernelMunmap(reinterpret_cast<uint64_t>(entry.start), entry.length);
   break;
  case OpProtect:
   result = DoMprotect(entry.start, entry.length, static_cast<uint8_t>(entry.protection));
   break;
  case OpTypeProtect:
   if (UndefinedProtection(static_cast<uint8_t>(entry.protection))) {
    result = SCE_KERNEL_ERROR_EINVAL;
    break;
   }
   result = DoMtypeprotect(entry.start, entry.length, static_cast<uint8_t>(entry.type), static_cast<uint8_t>(entry.protection));
   break;
  case OpMapFlexible:
   result = _mapFlexible(&entry.start, entry.length, static_cast<uint8_t>(entry.protection), flags);
   break;
  }
  if (result != 0) break;
  const int savedErrno = errno;
  const bool copied = GuestCopyOut_nid_no_patch(&entries[processed].start, &entry.start, sizeof(entry.start));
  errno = savedErrno;
  if (!copied) {
   result = SCE_KERNEL_ERROR_EFAULT;
   break;
  }
 }
 if (num_entries_out != nullptr) {
  const int savedErrno = errno;
  const bool copied = GuestCopyOut_nid_no_patch(num_entries_out, &processed, sizeof(processed));
  errno = savedErrno;
  if (!copied) return SCE_KERNEL_ERROR_EFAULT;
 }
 return result;
}

int32_t APS5_VABI sceKernelMapNamedSystemFlexibleMemory(void** addr_in_out, size_t len, int prot, int flags, const char* name) {
    constexpr size_t MaximumNameLength = 32;
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    std::string copy;
    const int error = name == nullptr ? 0 : CopyRangeName(name, MaximumNameLength, copy);
    if (name != nullptr && error != 0 && error != 63) throw std::runtime_error("sceKernelMapNamedSystemFlexibleMemory: unreadable name");
    const int result = DoMapAnon(addr_in_out, len, prot, flags);
    if (result != 0) return result;
    _releaseFlexible(reinterpret_cast<uintptr_t>(*addr_in_out), len);
    if (name == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    if (error == 63) return SCE_KERNEL_ERROR_ENAMETOOLONG;
    NameVirtualRange(*addr_in_out, len, std::move(copy));
    return 0;
}

}

bool GuestRangeMapped(std::uintptr_t start, std::uintptr_t end) {
    for (auto cursor = start; cursor < end;) {
        VirtualQueryInfo info{};
        if (sceKernelVirtualQuery(reinterpret_cast<const void*>(cursor), 0, &info, sizeof(info)) != 0) return false;
        if (info.end <= cursor) return false;
        cursor = info.end;
    }
    return true;
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
    if (VirtualLock(address, bytes)) {
        RecordLockedRange(start, end);
        return 0;
    }
    if (GetLastError() != ERROR_WORKING_SET_QUOTA) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualLock failed");
    SIZE_T minimum = 0;
    SIZE_T maximum = 0;
    DWORD flags = 0;
    if (!GetProcessWorkingSetSizeEx(GetCurrentProcess(), &minimum, &maximum, &flags)) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "GetProcessWorkingSetSizeEx failed");
    if (bytes > std::numeric_limits<SIZE_T>::max() - maximum) return SCE_KERNEL_ERROR_EAGAIN;
    if (!SetProcessWorkingSetSizeEx(GetCurrentProcess(), minimum + bytes, maximum + bytes, flags)) return SCE_KERNEL_ERROR_EAGAIN;
    if (VirtualLock(address, bytes)) {
        RecordLockedRange(start, end);
        return 0;
    }
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
    ForgetLockedRange(start, end);
    return 0;
}
#else
#ifdef __APPLE__
void AddUnwiredHostPages(std::uintptr_t start, std::uintptr_t end, std::vector<std::pair<std::uintptr_t, std::uintptr_t>>& unwired) {
    for (auto cursor = start; cursor < end;) {
        mach_vm_address_t region = cursor;
        mach_vm_size_t size = 0;
        vm_region_basic_info_data_64_t info{};
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;
        if (mach_vm_region(mach_task_self(), &region, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) != KERN_SUCCESS || region >= end) {
            unwired.emplace_back(cursor, end);
            return;
        }
        if (region > cursor) unwired.emplace_back(cursor, region);
        const auto last = std::min<std::uintptr_t>(end, region + size);
        if (info.user_wired_count == 0) unwired.emplace_back(std::max<std::uintptr_t>(cursor, region), last);
        cursor = last;
    }
}

int WireHostPages(std::uintptr_t start, std::uintptr_t end) {
    std::lock_guard lock(g_lockedRangesLock);
    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> unwired;
    auto cursor = start;
    auto it = g_lockedRanges.upper_bound(start);
    if (it != g_lockedRanges.begin()) --it;
    for (; it != g_lockedRanges.end() && it->first < end; ++it) {
        const auto lockedEnd = it->first + it->second;
        if (lockedEnd <= cursor) continue;
        if (it->first > cursor) unwired.emplace_back(cursor, it->first);
        AddUnwiredHostPages(std::max(cursor, it->first), std::min(end, lockedEnd), unwired);
        cursor = std::max(cursor, lockedEnd);
    }
    if (cursor < end) unwired.emplace_back(cursor, end);
    for (std::size_t index = 0; index < unwired.size(); ++index) {
        if (::mlock(reinterpret_cast<void*>(unwired[index].first), unwired[index].second - unwired[index].first) != 0) {
            const int error = errno;
            for (std::size_t wired = 0; wired < index; ++wired) ::munlock(reinterpret_cast<void*>(unwired[wired].first), unwired[wired].second - unwired[wired].first);
            return error;
        }
    }
    RecordLockedRangeLocked(start, end);
    return 0;
}
#else
int WireHostPages(std::uintptr_t start, std::uintptr_t end) {
    if (::mlock(reinterpret_cast<void*>(start), end - start) != 0) return errno;
    RecordLockedRange(start, end);
    return 0;
}
#endif

int LockHostPages(std::uintptr_t start, std::uintptr_t end) {
#ifdef __APPLE__
    for (mach_vm_address_t cursor = start; cursor < end;) {
        mach_vm_address_t region = cursor;
        mach_vm_size_t size = 0;
        vm_region_basic_info_data_64_t info{};
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;
        const kern_return_t status = mach_vm_region(mach_task_self(), &region, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object);
        if (status == KERN_INVALID_ADDRESS) return SCE_KERNEL_ERROR_ENOMEM;
        if (status != KERN_SUCCESS) throw std::runtime_error("mach_vm_region failed");
        if (region > cursor || (info.protection & (VM_PROT_READ | VM_PROT_EXECUTE)) == 0) return SCE_KERNEL_ERROR_ENOMEM;
        cursor = region + size;
    }
#endif
    int error = WireHostPages(start, end);
    if (error == 0) return 0;
    rlimit limit{};
    if ((error == ENOMEM || error == EPERM || error == EAGAIN) && getrlimit(RLIMIT_MEMLOCK, &limit) == 0 && limit.rlim_cur < limit.rlim_max) {
        limit.rlim_cur = limit.rlim_max;
        if (setrlimit(RLIMIT_MEMLOCK, &limit) == 0) {
            error = WireHostPages(start, end);
            if (error == 0) return 0;
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
    if (!GuestRangeMapped(start, end)) return SCE_KERNEL_ERROR_ENOMEM;
#ifdef __APPLE__
    std::lock_guard lock(g_lockedRangesLock);
    if (::munlock(reinterpret_cast<void*>(start), end - start) == 0) {
        ForgetLockedRangeLocked(start, end);
        return 0;
    }
#else
    if (::munlock(reinterpret_cast<void*>(start), end - start) == 0) {
        ForgetLockedRange(start, end);
        return 0;
    }
#endif
    const int error = errno;
    if (error == ENOMEM) return SCE_KERNEL_ERROR_ENOMEM;
    throw std::system_error(error, std::generic_category(), "munlock failed");
}
#endif

}

extern "C" {

bool APS5_VABI DirectMemoryAliased_nid_postfix(std::uintptr_t address, std::size_t bytes) {
    return DirectMemoryAliased(address, bytes);
}

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

int APS5_VABI sceKernelMunlockall_nid_postfix() {
#if !defined(_WIN32) && !defined(__APPLE__)
    ::munlockall();
#endif
    std::map<uintptr_t, size_t> ranges;
    {
        std::lock_guard lock(g_lockedRangesLock);
        ranges.swap(g_lockedRanges);
#ifdef __APPLE__
        const int saved = errno;
        for (const auto& [start, len] : ranges) ::munlock(reinterpret_cast<void*>(start), len);
        errno = saved;
#endif
    }
#ifdef _WIN32
    for (const auto& [start, len] : ranges) {
        const auto end = start + len;
        for (auto cursor = start; cursor < end;) {
            MEMORY_BASIC_INFORMATION region{};
            if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region)) break;
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
    }
#endif
    return 0;
}

}
