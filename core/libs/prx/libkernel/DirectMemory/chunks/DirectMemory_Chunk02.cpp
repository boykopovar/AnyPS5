#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <memory>
#include <new>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(__linux__)
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#elif defined(__EMSCRIPTEN__)
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#else
#include <windows.h>
#include "prx/libc/include/WindowsDescriptor.hpp"

static constexpr int PROT_NONE = 0;
static constexpr int PROT_READ = 1;
static constexpr int PROT_WRITE = 2;
static constexpr int PROT_EXEC = 4;
static constexpr int MAP_PRIVATE = 0x02;
static constexpr int MAP_ANONYMOUS = 0x20;
static constexpr int MAP_FIXED = 0x10;
static void* const MAP_FAILED = reinterpret_cast<void*>(-1);

int DoMapShared(void** addr, size_t len, int prot, HostSharedMemory object) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    const auto nativeProtection = LinuxProtFromSce(prot);
    GuestAllocations::Mutation mutation;
    void* mapped = MapPlaced(*addr, len, PROT_NONE, 0, PS5_PAGE_SIZE);
    try {
#if defined(_WIN32)
        GuestArena::GuestArenaMap_nid_postfix(mapped, len, object, 0, WinProtFromPosix(nativeProtection));
#elif defined(__APPLE__)
        if (::mmap(mapped, len, nativeProtection & ~PROT_EXEC, MAP_SHARED | MAP_FIXED, object, 0) == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "map shared memory");
        if ((nativeProtection & PROT_EXEC) != 0 && ::mprotect(mapped, len, nativeProtection) != 0) throw std::system_error(errno, std::generic_category(), "make shared memory executable");
#elif defined(__EMSCRIPTEN__)
        static_cast<void>(object);
        throw std::system_error(ENOTSUP, std::generic_category(), "shared memory mappings are not supported in WebAssembly");
#else
        if (::mmap(mapped, len, nativeProtection, MAP_SHARED | MAP_FIXED, object, 0) == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "map shared memory");
#endif
        mutation.Add(mapped, len, (prot & 3) != 0, (prot & 2) != 0, false);
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    RecordProtection(mapped, len, prot);
    Trace("map shared %p+0x%zx prot=0x%x", mapped, len, prot);
    return 0;
}

#ifdef _WIN32
static int PosixRightsFromWin(DWORD protection) {
    switch (protection & 0xff) {
    case PAGE_NOACCESS: return PROT_NONE;
    case PAGE_READONLY: return PROT_READ;
    case PAGE_READWRITE:
    case PAGE_WRITECOPY: return PROT_READ | PROT_WRITE;
    case PAGE_EXECUTE: return PROT_EXEC;
    case PAGE_EXECUTE_READ: return PROT_READ | PROT_EXEC;
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY: return PROT_READ | PROT_WRITE | PROT_EXEC;
    default: throw std::runtime_error("Unknown Windows page protection of a mapped view");
    }
}

static bool IsGuestModuleImage(const void* base) {
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(address);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(address + static_cast<std::uintptr_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    const auto sectionCount = nt->FileHeader.NumberOfSections;
    for (unsigned i = 0; i < sectionCount; ++i) {
        if (std::strncmp(reinterpret_cast<const char*>(sections[i].Name), ".elf0", IMAGE_SIZEOF_SHORT_NAME) == 0) return true;
    }
    return false;
}
#else
struct GuestImageOverlap {
    std::uintptr_t base = 0;
    bool guestModule = false;
    bool containsStart = false;
};

struct GuestImageSearch {
    std::uintptr_t start = 0;
    std::uintptr_t end = 0;
    std::uintptr_t requested = 0;
    std::uintptr_t pageSize = 0;
    bool first = true;
    std::vector<GuestImageOverlap> images;
};

static int FindImagesOverlapping(dl_phdr_info* image, std::size_t, void* data) {
    auto& search = *static_cast<GuestImageSearch*>(data);
    const bool main = search.first;
    search.first = false;
    GuestImageOverlap overlap{.base = image->dlpi_addr, .guestModule = image->dlpi_name != nullptr && std::string_view(image->dlpi_name).ends_with(".guest.prx")};
    bool overlaps = false;
    for (int index = 0; index < image->dlpi_phnum; ++index) {
        const auto& header = image->dlpi_phdr[index];
        if (header.p_type != PT_LOAD) continue;
        const auto start = (image->dlpi_addr + header.p_vaddr) & ~(search.pageSize - 1);
        const auto end = (image->dlpi_addr + header.p_vaddr + header.p_memsz + search.pageSize - 1) & ~(search.pageSize - 1);
        if (search.end <= start || search.start >= end) continue;
        overlaps = true;
        if (search.requested >= start && search.requested < end) overlap.containsStart = true;
    }
    if (overlaps && !main) search.images.push_back(overlap);
    return 0;
}
#endif

int DoMprotect(const void* addr, size_t len, int prot, bool requireMappedStart) {
    Trace("protect %p+0x%zx prot=0x%x", addr, len, prot);
    const auto address = reinterpret_cast<std::uintptr_t>(addr);
    constexpr auto pageMask = static_cast<std::uintptr_t>(PS5_PAGE_SIZE - 1);
    const auto limit = std::numeric_limits<std::uintptr_t>::max();
    if (address == 0 || len == 0 || len > limit - address || address + len > limit - pageMask) throw std::invalid_argument("Invalid guest memory protection range");
    auto first = address & ~pageMask;
    auto end = (address + len + pageMask) & ~pageMask;
#ifdef __APPLE__
    GuestAllocations::GuestAllocationsGuestPages_nid_postfix(address, len, PS5_PAGE_SIZE, &first, &end);
#endif
    auto bytes = static_cast<std::size_t>(end - first);
    const void* pointer = reinterpret_cast<const void*>(first);
    const auto nativeProtection = LinuxProtFromSce(prot);
    if (GuestThreadStackHostRangeOverlaps(address, address + len))
        throw std::system_error(EACCES, std::generic_category(), "mprotect overlaps host-only guest thread stack pages");
    std::uintptr_t stackStart = 0;
    std::uintptr_t stackEnd = 0;
    if ((prot & GuestProtGpuReadWrite) == 0 && GuestThreadStack(address, &stackStart, &stackEnd) && address >= stackStart && address + len <= stackEnd) {
        constexpr std::uintptr_t hostPageMask = 0xfff;
        const auto requested = std::max(first, (stackStart + hostPageMask) & ~hostPageMask);
        const auto insideEnd = std::min(end, stackEnd & ~hostPageMask);
        if (requested >= insideEnd) return 0;
        auto* requestedPointer = reinterpret_cast<void*>(requested);
        const auto requestedBytes = static_cast<std::size_t>(insideEnd - requested);
#ifdef _WIN32
        const bool hostStack = !KernelArena::Get().Owns(requestedPointer, requestedBytes);
        const bool reserveBottom = hostStack && nativeProtection != PROT_NONE && AtStackReserveBottom(requested, insideEnd);
        const auto inside = hostStack && !reserveBottom ? AboveStackGuard(requested, insideEnd) : requested;
        if (inside == insideEnd) {
            RecordProtection(requestedPointer, requestedBytes, prot);
            return 0;
        }
#else
        const auto inside = requested;
#endif
        auto* insidePointer = reinterpret_cast<void*>(inside);
        const auto insideBytes = static_cast<std::size_t>(insideEnd - inside);
#ifdef _WIN32
        if (nativeProtection != PROT_NONE && hostStack) CommitReservedPages(insidePointer, insideBytes);
        const bool protectedPages = mprotect(insidePointer, insideBytes, nativeProtection) == 0;
#elif defined(__APPLE__)
        const int error = GuestWriteWatch::GuestWriteWatchProtect_nid_postfix(insidePointer, insideBytes, nativeProtection);
        if (error != 0) errno = error;
        const bool protectedPages = error == 0;
#else
        bool protectedPages = mprotect(insidePointer, insideBytes, nativeProtection) == 0;
#if defined(__linux__)
        if (!protectedPages && errno == ENOMEM) {
            MapUnmappedPages(insidePointer, static_cast<std::size_t>((stackEnd & ~hostPageMask) - inside));
            protectedPages = mprotect(insidePointer, insideBytes, nativeProtection) == 0;
        }
#endif
#endif
        if (!protectedPages) throw std::system_error(errno, std::generic_category(), "mprotect failed");
        if (inside != requested) RecordProtection(requestedPointer, static_cast<std::size_t>(inside - requested), prot);
        NoteGuestStackProtection(inside, insideBytes, (prot & 3) == 3);
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(inside, insideBytes);
        RecordProtection(insidePointer, insideBytes, prot);
        return 0;
    }
    GuestAllocations::Mutation mutation;
#ifdef _WIN32
    for (auto cursor = first; cursor < end;) {
        MEMORY_BASIC_INFORMATION view{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &view, sizeof(view)) != sizeof(view)) throw std::runtime_error("Cannot query guest memory protection range");
        if (view.Type == MEM_IMAGE) {
            if (view.AllocationBase == GetModuleHandleW(nullptr)) mutation.RegisterMainImage();
            else if (IsGuestModuleImage(view.AllocationBase)) mutation.RegisterImage(view.AllocationBase);
            else if (cursor == first) throw std::invalid_argument("Memory protection of a foreign image is not supported");
        }
        if (view.Type == MEM_MAPPED && (nativeProtection & ~PosixRightsFromWin(view.AllocationProtect)) != 0)
            throw std::system_error(EACCES, std::generic_category(), "mprotect above the maximum protection of a mapped view");
        cursor = reinterpret_cast<std::uintptr_t>(view.BaseAddress) + view.RegionSize;
    }
#else
    mutation.RegisterMainImage();
    GuestImageSearch search{.start = first, .end = end, .requested = address, .pageSize = static_cast<std::uintptr_t>(::sysconf(_SC_PAGESIZE))};
    dl_iterate_phdr(FindImagesOverlapping, &search);
    for (const auto& image : search.images) {
        if (image.guestModule) mutation.RegisterImage(reinterpret_cast<const void*>(image.base));
        else if (image.containsStart) throw std::invalid_argument("Memory protection of a foreign image is not supported");
    }
    mutation.TrimToRegistered(PS5_PAGE_SIZE, pointer, bytes);
#endif
    std::vector<std::pair<const void*, std::size_t>> spans;
    mutation.ForRegistered(pointer, bytes, [&](const void* span, std::size_t spanBytes) { spans.emplace_back(span, spanBytes); });
    if (requireMappedStart && (spans.empty() || spans.front().first != pointer)) return SCE_KERNEL_ERROR_EINVAL;
    for (const auto& [span, spanBytes] : spans) mutation.RequireUnpinned(span, spanBytes);
    for (const auto& [span, spanBytes] : spans) {
        mutation.Protect(span, spanBytes, (prot & 3) != 0, (prot & 2) != 0, (prot & GuestProtGpuReadWrite) != 0, (prot & GuestProtGpuWrite) != 0, [&] {
#ifdef __APPLE__
            if (const int error = GuestWriteWatch::GuestWriteWatchProtect_nid_postfix(const_cast<void*>(span), spanBytes, nativeProtection); error != 0)
                throw std::system_error(error, std::generic_category(), "mprotect failed");
#else
            if (mprotect(const_cast<void*>(span), spanBytes, nativeProtection) != 0) throw std::system_error(errno, std::generic_category(), "mprotect failed");
#endif
        });
        RecordProtection(span, spanBytes, prot);
    }
    return 0;
}

std::atomic<void (*)()> g_retypePause{nullptr};

int DoMtypeprotect(const void* addr, size_t len, int type, int prot, bool requireMappedStart) {
    const int result = DoMprotect(addr, len, prot, requireMappedStart);
    if (result != 0) return result;
    const auto address = reinterpret_cast<std::uintptr_t>(addr);
    constexpr auto pageMask = static_cast<std::uintptr_t>(PS5_PAGE_SIZE - 1);
    const auto first = address & ~pageMask;
    const auto end = (address + len + pageMask) & ~pageMask;
    const auto pool = DirectMemoryPoolLock();
    std::lock_guard lock(g_directLock);
    std::vector<std::pair<std::uint64_t, std::size_t>> physical;
    auto it = g_directMappings.lower_bound(first);
    if (it != g_directMappings.begin() && std::prev(it)->second.end > first) --it;
    for (; it != g_directMappings.end() && it->first < end; ++it) {
        const auto low = std::max(it->first, first);
        const auto high = std::min(it->second.end, end);
        physical.emplace_back(it->second.phys + low - it->first, high - low);
    }
    if (const auto pause = g_retypePause.load(std::memory_order_acquire)) pause();
    for (const auto& [phys, bytes] : physical) DirectMemoryRetypeLocked(pool, static_cast<int64_t>(phys), bytes, type);
    return result;
}

int DoMunmap(void* addr, size_t len) {
    Trace("unmap %p+0x%zx", addr, len);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0 || !addr) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    if (!mutation.Overlaps(addr, len)) return 0;
    UnmapRegistered(mutation, addr, len);
    return 0;
}

int DoReserveVirtual(void** addr, size_t len, int flags, size_t alignment) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    const bool fixed = *addr != nullptr && (flags & GuestMapFixedFlag) != 0;
    if (fixed) {
        if (GuestArena::GuestArenaAvailable_nid_postfix() && !GuestArena::GuestArenaContains_nid_postfix(*addr, len)) return SCE_KERNEL_ERROR_EINVAL;
        if (FixedNoOverwriteConflict(mutation, *addr, len, flags)) return SCE_KERNEL_ERROR_ENOMEM;
        if (mutation.Covers(*addr, len)) {
            constexpr int GuestMapNoOverwrite = 0x80;
            if ((flags & GuestMapNoOverwrite) == 0 && RemapFixedIntoRegistered(mutation, *addr, len, 0, GuestMapFixedFlag)) {
                RecordProtection(*addr, len, -1);
                RecordReservation(*addr, len);
                return 0;
            }
        }
        mutation.RequireAvailable(*addr, len);
        std::uintptr_t usedStart = 0, usedEnd = 0;
        if (GuestArena::GuestArenaAvailable_nid_postfix() &&
            GuestArena::GuestArenaNextUsed_nid_no_patch(reinterpret_cast<std::uintptr_t>(*addr), &usedStart, &usedEnd) &&
            usedStart < reinterpret_cast<std::uintptr_t>(*addr) + len && reinterpret_cast<std::uintptr_t>(*addr) < usedEnd) {
            return SCE_KERNEL_ERROR_ENOMEM;
        }
    }
    constexpr int GuestMapNoCoalesce = 0x400000;
    void* mapped = nullptr;
    if (fixed) {
        try {
            mapped = MapAligned(*addr, len, PROT_NONE, GuestMapFixedFlag | (flags & GuestMapNoCoalesce), alignment);
        } catch (const std::invalid_argument&) {
            return SCE_KERNEL_ERROR_EINVAL;
        }
    } else {
        mapped = MapAligned(*addr, len, PROT_NONE, 0, alignment);
    }
    try {
        mutation.Add(mapped, len, false, false, false);
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    RecordReservation(mapped, len);
    Trace("reserve %p+0x%zx flags=0x%x align=0x%zx", mapped, len, flags, alignment);
    return 0;
}

void CreateDirectMemoryBacking(int64_t start, size_t len) {
    std::lock_guard lock(g_directLock);
    const auto first = static_cast<std::uint64_t>(start);
    const auto last = first + len;
    if (auto block = g_physBlocks.upper_bound(first); (block != g_physBlocks.end() && block->first < last) || (block != g_physBlocks.begin() && std::prev(block)->second.end > first)) throw std::runtime_error("physical allocation overlaps live direct memory");
#if defined(__linux__)
    RegisterSharedBacking();
#endif
    const auto backing = std::make_shared<PhysicalBacking>(len, first);
    std::map<std::uint64_t, PhysicalPage> pages;
    for (std::size_t offset = 0; offset < len; offset += PS5_PAGE_SIZE) pages.emplace(first + offset, PhysicalPage{backing, offset});
    g_physBlocks.emplace(first, PhysicalBlock{last, first, backing});
    g_physPages.merge(pages);
    Trace("allocate physical 0x%llx+0x%zx", static_cast<unsigned long long>(first), len);
}

void ForgetDirectMemory(int64_t start, size_t len) {
    const auto first = static_cast<std::uint64_t>(start);
    std::lock_guard lock(g_directLock);
    ValidatePhysicalRange(first, len);
    SplitPhysicalBlock(first);
    SplitPhysicalBlock(first + len);
    g_physBlocks.erase(g_physBlocks.lower_bound(first), g_physBlocks.lower_bound(first + len));
    g_physPages.erase(g_physPages.lower_bound(first), g_physPages.lower_bound(first + len));
    ErasePhysMappings(first, first + len);
    Trace("release physical 0x%llx+0x%zx", static_cast<unsigned long long>(first), len);
}

bool GuestReservation(std::uintptr_t addr, std::uintptr_t* start, std::uintptr_t* end) {
    std::lock_guard lock(g_reservationLock);
    const auto next = g_reservations.upper_bound(addr);
    if (next == g_reservations.begin()) return false;
    const auto containing = std::prev(next);
    if (addr >= containing->second) return false;
    *start = containing->first;
    *end = containing->second;
    return true;
}

bool GuestProtection(uintptr_t addr, int* prot) {
    std::lock_guard lock(g_protectionLock);
    auto next = g_protections.upper_bound(addr);
    if (next == g_protections.begin()) return false;
    const auto containing = std::prev(next);
    if (addr >= containing->second.end) return false;
    *prot = containing->second.prot;
    return true;
}

void GuestPageTableUsage(std::uint64_t granularity, std::uint64_t* cpuEntries, std::uint64_t* gpuEntries) {
    constexpr std::uint64_t GpuDirectorySpan = 1ULL << 30;
    struct DirectSpan {
        std::uintptr_t start;
        std::uintptr_t end;
        std::uint64_t phys;
    };
    std::vector<DirectSpan> direct;
    {
        std::lock_guard lock(g_directLock);
        direct.reserve(g_directMappings.size());
        for (const auto& [start, mapping] : g_directMappings) direct.push_back({start, mapping.end, mapping.phys});
    }
    const auto largePage = [&](std::uintptr_t granule) {
        const auto next = std::upper_bound(direct.begin(), direct.end(), granule, [](std::uintptr_t address, const DirectSpan& span) { return address < span.start; });
        if (next == direct.begin()) return false;
        const auto& span = *std::prev(next);
        return granule + granularity <= span.end && (span.phys + (granule - span.start)) % granularity == 0;
    };
    std::uint64_t cpu = 0;
    std::uint64_t gpu = 0;
    std::uint64_t cpuNext = 0;
    std::uint64_t gpuNext = 0;
    std::uint64_t directoryNext = 0;
    std::lock_guard lock(g_protectionLock);
    for (const auto& [start, range] : g_protections) {
        if (range.end <= start) continue;
        const auto first = start / granularity;
        const auto last = (range.end - 1) / granularity;
        if (last + 1 > std::max(first, cpuNext)) cpu += last + 1 - std::max(first, cpuNext);
        cpuNext = std::max(cpuNext, last + 1);
        if ((range.prot & GuestProtGpuReadWrite) == 0) continue;
        const auto firstDirectory = start / GpuDirectorySpan;
        const auto lastDirectory = (range.end - 1) / GpuDirectorySpan;
        if (lastDirectory + 1 > std::max(firstDirectory, directoryNext)) gpu += lastDirectory + 1 - std::max(firstDirectory, directoryNext);
        directoryNext = std::max(directoryNext, lastDirectory + 1);
        for (auto granule = std::max(first, gpuNext); granule <= last; ++granule) {
            const auto address = static_cast<std::uintptr_t>(granule * granularity);
            if (address < start || address + granularity > range.end || !largePage(address)) ++gpu;
        }
        gpuNext = std::max(gpuNext, last + 1);
    }
    *cpuEntries = cpu;
    *gpuEntries = gpu;
}

bool QueryDirectMapping(std::uintptr_t address, std::uintptr_t* start, std::uintptr_t* end, std::uint64_t* offset, int* memoryType) {
    std::uint64_t phys;
    {
        std::lock_guard lock(g_directLock);
        const auto next = g_directMappings.upper_bound(address);
        if (next == g_directMappings.begin()) return false;
        const auto it = std::prev(next);
        if (address >= it->second.end) return false;
        *start = it->first;
        *end = it->second.end;
        *offset = it->second.phys;
        phys = it->second.phys + (address - it->first);
    }
    std::int64_t blockStart = 0;
    std::int64_t blockEnd = 0;
    if (!DirectMemoryFind(static_cast<std::int64_t>(phys), false, &blockStart, &blockEnd, memoryType)) return false;
    const auto behind = phys - static_cast<std::uint64_t>(blockStart);
    const auto ahead = static_cast<std::uint64_t>(blockEnd) - phys;
    if (address - *start > behind) {
        *offset += address - *start - behind;
        *start = address - behind;
    }
    if (*end - address > ahead) *end = address + ahead;
    return true;
}

