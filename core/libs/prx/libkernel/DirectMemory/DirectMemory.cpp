#include "DirectMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <cerrno>
#include <iterator>
#include <map>
#include <mutex>
#include <set>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <vector>

#if defined(__linux__)
#include <sys/mman.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#else
#include <windows.h>

static constexpr int PROT_NONE = 0;
static constexpr int PROT_READ = 1;
static constexpr int PROT_WRITE = 2;
static constexpr int PROT_EXEC = 4;
static constexpr int MAP_PRIVATE = 0x02;
static constexpr int MAP_ANONYMOUS = 0x20;
static constexpr int MAP_FIXED = 0x10;
static void* const MAP_FAILED = reinterpret_cast<void*>(-1);

static DWORD WinProtFromPosix(int prot) {
    if (prot == PROT_NONE) return PAGE_NOACCESS;
    if ((prot & PROT_EXEC) && (prot & PROT_WRITE)) return PAGE_EXECUTE_READWRITE;
    if ((prot & PROT_EXEC) && (prot & PROT_READ)) return PAGE_EXECUTE_READ;
    if (prot & PROT_EXEC) return PAGE_EXECUTE;
    if (prot & PROT_WRITE) return PAGE_READWRITE;
    return PAGE_READONLY;
}

// Guest mappings share libc's guest address space arena so they stay inside the PS5 map area.
struct KernelArena {
    static KernelArena& Get() {
        static KernelArena arena;
        return arena;
    }
    bool Contains(const void* pointer, size_t len) const { return GuestArena::GuestArenaContains_nid_postfix(pointer, len); }
    void* Allocate(size_t len, size_t alignment) { return GuestArena::GuestArenaAllocate_nid_postfix(len, alignment); }
    void MarkUsed(const void* pointer, size_t len) { GuestArena::GuestArenaMarkUsed_nid_postfix(pointer, len); }
    void Release(const void* pointer, size_t len) { GuestArena::GuestArenaRelease_nid_postfix(pointer, len); }
};

static void CommitArenaRange(void* addr, size_t len, DWORD winProt) {
    if (!VirtualAlloc(addr, len, MEM_COMMIT, winProt))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualAlloc commit failed");
}

static void* mmap_aligned(size_t len, int prot, size_t alignment) {
    auto& arena = KernelArena::Get();
    void* result = arena.Allocate(len, alignment);
    if (prot != PROT_NONE) {
        try {
            CommitArenaRange(result, len, WinProtFromPosix(prot));
        } catch (...) {
            arena.Release(result, len);
            throw;
        }
    }
    return result;
}

static void* mmap(void* addr, size_t len, int prot, int flags, int, int) {
    DWORD winProt = WinProtFromPosix(prot);
    if (flags & MAP_FIXED) {
        auto& arena = KernelArena::Get();
        if (arena.Contains(addr, len)) {
            arena.MarkUsed(addr, len);
            if (prot != PROT_NONE) {
                try {
                    CommitArenaRange(addr, len, winProt);
                } catch (...) {
                    arena.Release(addr, len);
                    throw;
                }
            }
            return addr;
        }
        void* result = VirtualAlloc(addr, len, MEM_RESERVE | MEM_COMMIT, winProt);
        if (!result) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualAlloc fixed failed");
        return result;
    }
    return mmap_aligned(len, prot, PS5_PAGE_SIZE);
}

static int munmap(void* addr, size_t len) {
    if (!VirtualFree(addr, len, MEM_DECOMMIT))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualFree decommit failed");
    auto& arena = KernelArena::Get();
    if (arena.Contains(addr, len)) arena.Release(addr, len);
    return 0;
}

static int munmap_release(void* addr) {
    if (!VirtualFree(addr, 0, MEM_RELEASE))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualFree release failed");
    return 0;
}

static int mprotect(void* addr, size_t len, int prot) {
    if (prot != PROT_NONE && KernelArena::Get().Contains(addr, len)) CommitArenaRange(addr, len, WinProtFromPosix(prot));
    auto cursor = reinterpret_cast<std::uintptr_t>(addr);
    const auto end = cursor + len;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<void*>(cursor), &memory, sizeof(memory)) != sizeof(memory))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualQuery failed");
        const auto regionEnd = std::min(end, reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize);
        if (memory.State == MEM_COMMIT) {
            DWORD old;
            if (!VirtualProtect(reinterpret_cast<void*>(cursor), regionEnd - cursor, WinProtFromPosix(prot), &old))
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualProtect failed");
        } else if (prot != PROT_NONE) {
            throw std::runtime_error("mprotect of uncommitted memory outside the guest arena");
        }
        cursor = regionEnd;
    }
    return 0;
}
#endif

namespace {

constexpr int GuestMapFixedFlag = 0x10;

#if defined(__linux__)
void* MapAtOrAbove(std::uintptr_t start, size_t len, int prot, size_t alignment) {
    constexpr std::uintptr_t UserLimit = 0x7fff00000000ull;
    for (int attempt = 0; attempt < 8; ++attempt) {
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> used;
        std::ifstream maps("/proc/self/maps");
        std::string line;
        while (std::getline(maps, line)) {
            std::istringstream fields(line);
            std::uintptr_t begin = 0, end = 0;
            char dash = 0;
            if (fields >> std::hex >> begin >> dash >> end) used.emplace_back(begin, end);
        }
        std::sort(used.begin(), used.end());
        auto candidate = (start + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
        for (const auto& [begin, end] : used) {
            if (end <= candidate) continue;
            if (begin >= candidate + len) break;
            candidate = (end + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
        }
        if (candidate + len > UserLimit || candidate + len < candidate) throw std::runtime_error("No free range above the mapping address hint");
        void* result = mmap(reinterpret_cast<void*>(candidate), len, prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (result != MAP_FAILED) return result;
        if (errno != EEXIST) throw std::system_error(errno, std::generic_category(), "Hinted mmap failed");
    }
    throw std::runtime_error("Hinted mmap kept racing with other mappings");
}
#endif

void ValidateLength(size_t len) {
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Memory length must be a positive multiple of the guest page size");
    }
}

size_t ValidateAlignment(size_t alignment) {
    if (alignment == 0) return PS5_PAGE_SIZE;
    if (alignment < PS5_PAGE_SIZE || (alignment & (alignment - 1)) != 0) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Memory alignment must be a power of two no smaller than the guest page size");
    }
    return alignment;
}

void ValidateRange(const void* addr, size_t len, size_t alignment) {
    ValidateLength(len);
    const auto start = reinterpret_cast<std::uintptr_t>(addr);
    if (!addr || (start & (alignment - 1)) != 0 || len > std::numeric_limits<std::uintptr_t>::max() - start) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Invalid memory address, alignment or range");
    }
}

int LinuxProtFromSce(int prot) {
    if ((prot & ~0xF7) != 0) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Unsupported memory protection bits: " + std::to_string(prot));
    }
    int result = PROT_NONE;
    if (prot & 0x13) result |= PROT_READ;
    if (prot & 0x22) result |= PROT_READ | PROT_WRITE;
    if (prot & 4) result |= PROT_READ | PROT_EXEC;
    return result;
}

bool TraceEnabled() {
    static const bool enabled = std::getenv("APS5_TRACE_MEMORY") != nullptr;
    return enabled;
}

void Trace(const char* format, ...) {
    if (!TraceEnabled()) return;
    va_list args;
    va_start(args, format);
    std::fprintf(stderr, "[memory] ");
    std::vfprintf(stderr, format, args);
    std::fputc('\n', stderr);
    va_end(args);
}

// Direct memory keeps its contents across mappings: titles unmap physical pages and map them again at
// another address (titles move their pools that way) and read back what they wrote through the old one.
// The contents move by copying at unmap and map; a shared section with views (as in Kyty) would alias
// them for real but loses the write watching of the guest arena.
struct DirectMapping {
    std::uintptr_t end;
    std::uint64_t phys;
};
std::mutex g_directLock;
std::map<std::uintptr_t, DirectMapping> g_directMappings;
// Contents of unmapped direct memory by physical page; all-zero pages are left out.
std::map<std::uint64_t, std::unique_ptr<unsigned char[]>> g_physPages;

void ProtectOrThrow(std::uintptr_t address, std::size_t len, int nativeProt) {
    if (mprotect(reinterpret_cast<void*>(address), len, nativeProt) != 0) throw std::system_error(errno, std::generic_category(), "mprotect failed");
}

void SaveContents(std::uintptr_t address, std::size_t len, std::uint64_t phys) {
    static const unsigned char zero[PS5_PAGE_SIZE] = {};
    ProtectOrThrow(address, len, PROT_READ);
    for (std::size_t offset = 0; offset < len; offset += PS5_PAGE_SIZE) {
        const auto* page = reinterpret_cast<const unsigned char*>(address + offset);
        if (std::memcmp(page, zero, PS5_PAGE_SIZE) == 0) continue;
        auto& copy = g_physPages[phys + offset];
        copy.reset(new unsigned char[PS5_PAGE_SIZE]);
        std::memcpy(copy.get(), page, PS5_PAGE_SIZE);
    }
}

// A physical page mapped at more than one address has the same contents in every view, and every
// view lacks write access while it is shared (g_shared).
// ponytail: a write to a shared page faults; handing the page to the writing view on the fault, with
// the GPU driver's imports of the other views, is the upgrade if a title writes through an alias.
std::set<std::uintptr_t> g_shared;

// The views of physPage in g_directMappings.
std::vector<std::uintptr_t> Views(std::uint64_t physPage) {
    std::vector<std::uintptr_t> views;
    for (const auto& [base, mapping] : g_directMappings) {
        if (physPage >= mapping.phys && physPage < mapping.phys + (mapping.end - base)) views.push_back(base + (physPage - mapping.phys));
    }
    return views;
}

int NativeProtection(std::uintptr_t view) {
    int prot = 0;
    if (!GuestProtection(view, &prot)) throw std::runtime_error("direct memory view has no recorded protection");
    return LinuxProtFromSce(prot);
}

int SharedProtection(int nativeProt) {
    return nativeProt & ~PROT_WRITE;
}

// Takes the shared page out of its sharing; the views left keep sharing it, or the last one gets its
// own protection back. The views must still hold the same bytes: a write that reached one of them
// (the GPU writes through its imports of guest memory) would have been lost to the others.
void Unshare(std::uintptr_t page, std::uint64_t physPage) {
    const auto others = Views(physPage);
    if (others.empty()) throw std::runtime_error("shared direct memory page has no other view");
    ProtectOrThrow(page, PS5_PAGE_SIZE, PROT_READ);
    ProtectOrThrow(others.front(), PS5_PAGE_SIZE, PROT_READ);
    if (std::memcmp(reinterpret_cast<const void*>(page), reinterpret_cast<const void*>(others.front()), PS5_PAGE_SIZE) != 0) {
        char message[160];
        std::snprintf(message, sizeof(message), "direct memory 0x%llx mapped at 0x%llx and 0x%llx was written through one view; aliased writes are not implemented", static_cast<unsigned long long>(physPage),
                      static_cast<unsigned long long>(page), static_cast<unsigned long long>(others.front()));
        throw std::runtime_error(message);
    }
    const bool last = others.size() == 1;
    if (last) g_shared.erase(others.front());
    const auto prot = NativeProtection(others.front());
    ProtectOrThrow(others.front(), PS5_PAGE_SIZE, last ? prot : SharedProtection(prot));
}

// Takes [start, end) out of the direct mappings; with save its bytes stay as the contents of its pages.
void EraseMappings(std::uintptr_t start, std::uintptr_t end, bool save) {
    auto it = g_directMappings.lower_bound(start);
    if (it != g_directMappings.begin() && std::prev(it)->second.end > start) --it;
    while (it != g_directMappings.end() && it->first < end) {
        const auto base = it->first;
        const auto mapping = it->second;
        it = g_directMappings.erase(it);
        const auto cutStart = std::max(base, start);
        const auto cutEnd = std::min(mapping.end, end);
        if (base < start) g_directMappings.emplace(base, DirectMapping{start, mapping.phys});
        if (mapping.end > end) it = g_directMappings.emplace(end, DirectMapping{mapping.end, mapping.phys + (end - base)}).first;
        if (g_shared.empty()) {
            if (save) SaveContents(cutStart, cutEnd - cutStart, mapping.phys + (cutStart - base));
            continue;
        }
        for (auto page = cutStart; page < cutEnd; page += PS5_PAGE_SIZE) {
            const auto physPage = mapping.phys + (page - base);
            if (g_shared.erase(page) != 0) Unshare(page, physPage);
            else if (save) SaveContents(page, PS5_PAGE_SIZE, physPage);
        }
    }
}

// Records a fresh mapping of phys at address (zero-filled, already protected nativeProt) and brings its contents back.
void AddMapping(std::uintptr_t address, std::size_t len, std::uint64_t phys, int nativeProt) {
    std::vector<std::pair<std::size_t, std::uintptr_t>> shared;
    for (const auto& [base, mapping] : g_directMappings) {
        const auto physEnd = std::min(phys + len, mapping.phys + (mapping.end - base));
        for (auto page = std::max(phys, mapping.phys); page < physEnd; page += PS5_PAGE_SIZE) shared.emplace_back(page - phys, base + (page - mapping.phys));
    }
    g_directMappings.emplace(address, DirectMapping{address + len, phys});
    auto it = g_physPages.lower_bound(phys);
    const auto last = g_physPages.lower_bound(phys + len);
    if (it == last && shared.empty()) return;
    ProtectOrThrow(address, len, PROT_READ | PROT_WRITE);
    for (; it != last; it = g_physPages.erase(it)) std::memcpy(reinterpret_cast<void*>(address + (it->first - phys)), it->second.get(), PS5_PAGE_SIZE);
    for (const auto& [offset, view] : shared) {
        ProtectOrThrow(view, PS5_PAGE_SIZE, PROT_READ);
        std::memcpy(reinterpret_cast<void*>(address + offset), reinterpret_cast<const void*>(view), PS5_PAGE_SIZE);
        ProtectOrThrow(view, PS5_PAGE_SIZE, SharedProtection(NativeProtection(view)));
        g_shared.insert(view);
    }
    ProtectOrThrow(address, len, nativeProt);
    for (const auto& [offset, view] : shared) {
        ProtectOrThrow(address + offset, PS5_PAGE_SIZE, SharedProtection(nativeProt));
        g_shared.insert(address + offset);
    }
    if (!shared.empty()) Trace("alias %p+0x%zx shares %zu physical pages with older views", reinterpret_cast<void*>(address), len, shared.size());
}

bool RemapFixedIntoRegistered(GuestAllocations::Mutation& mutation, void* addr, size_t len, int prot, int flags, int64_t physStart = -1) {
    constexpr int GuestMapFixed = 0x10;
    constexpr int GuestMapNoOverwrite = 0x80;
    if (addr == nullptr || (flags & GuestMapFixed) == 0 || (flags & GuestMapNoOverwrite) != 0 || !mutation.Covers(addr, len)) return false;
    ValidateRange(addr, len, PS5_PAGE_SIZE);
    Trace("remap fixed %p+0x%zx prot=0x%x phys=0x%llx", addr, len, prot, static_cast<long long>(physStart));
    const auto nativeProtection = LinuxProtFromSce(prot);
    mutation.Protect(addr, len, (prot & 3) != 0, (prot & 2) != 0, [&] {
        const auto address = reinterpret_cast<std::uintptr_t>(addr);
        std::lock_guard lock(g_directLock);
        EraseMappings(address, address + len, true);
        // The range shows the physical pages' contents now, not what was mapped there before.
#ifdef _WIN32
        if (physStart >= 0 && !VirtualFree(addr, len, MEM_DECOMMIT)) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualFree decommit failed");
        if (!VirtualAlloc(addr, len, MEM_COMMIT, WinProtFromPosix(nativeProtection))) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualAlloc commit failed");
#else
        if (physStart >= 0 && madvise(addr, len, MADV_DONTNEED) != 0) throw std::system_error(errno, std::generic_category(), "madvise failed");
#endif
        if (mprotect(addr, len, nativeProtection) != 0) throw std::system_error(errno, std::generic_category(), "mprotect failed");
        if (physStart >= 0) AddMapping(address, len, static_cast<std::uint64_t>(physStart), nativeProtection);
    });
    return true;
}

void Unmap(void* addr, size_t len) {
#if defined(__linux__)
    if (munmap(addr, len) != 0) throw std::system_error(errno, std::generic_category(), "munmap failed");
#else
    if (KernelArena::Get().Contains(addr, len)) munmap(addr, len);
    else munmap_release(addr);
#endif
}

void* MapAligned(void* addr, size_t len, int prot, int flags, size_t alignment) {
    ValidateLength(len);
    alignment = ValidateAlignment(alignment);
    constexpr int GuestMapFixed = 0x10;
    constexpr int GuestMapNoOverwrite = 0x80;
    constexpr int GuestMapNoCoalesce = 0x400000;
#if defined(__linux__)
    constexpr int SupportedFlags = GuestMapFixed | GuestMapNoOverwrite | GuestMapNoCoalesce;
#else
    constexpr int SupportedFlags = GuestMapFixed | GuestMapNoCoalesce;
#endif
    if ((flags & ~SupportedFlags) != 0) {
        char message[64];
        std::snprintf(message, sizeof(message), "Unsupported memory mapping flags 0x%x", flags);
        throw std::invalid_argument(message);
    }
    if ((flags & GuestMapFixed) != 0) {
        ValidateRange(addr, len, alignment);
#if defined(__linux__)
        const int placement = (flags & GuestMapNoOverwrite) != 0 ? MAP_FIXED_NOREPLACE : MAP_FIXED;
#else
        const int placement = MAP_FIXED;
#endif
        void* result = mmap(addr, len, prot, MAP_PRIVATE | MAP_ANONYMOUS | placement, -1, 0);
        if (result == MAP_FAILED) {
            // return SCE_KERNEL_ERROR_ENOMEM;
            throw std::system_error(errno, std::generic_category(), "Fixed mmap failed");
        }
        return result;
    }
    if (addr) {
#if defined(__linux__)
        return MapAtOrAbove(reinterpret_cast<std::uintptr_t>(addr), len, prot, alignment);
#else
        throw std::invalid_argument("Non-fixed mapping address hints are not implemented");
#endif
    }
#ifdef _WIN32
    return mmap_aligned(len, prot, alignment);
#endif
    if (len > std::numeric_limits<size_t>::max() - alignment) {
        throw std::overflow_error("Aligned mapping size overflow");
    }
    const size_t allocLen = len + alignment;
    void* result = mmap(nullptr, allocLen, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result == MAP_FAILED) {
        // return SCE_KERNEL_ERROR_ENOMEM;
        throw std::system_error(errno, std::generic_category(), "Aligned mmap failed");
    }
    const auto raw = reinterpret_cast<std::uintptr_t>(result);
    const size_t prefix = (alignment - (raw & (alignment - 1))) & (alignment - 1);
    void* aligned = reinterpret_cast<void*>(raw + prefix);
    const size_t suffix = allocLen - prefix - len;
    if (prefix != 0 && munmap(result, prefix) != 0) {
        const int error = errno;
        Unmap(result, allocLen);
        throw std::system_error(error, std::generic_category(), "Mapping prefix munmap failed");
    }
    if (suffix != 0 && munmap(reinterpret_cast<void*>(raw + prefix + len), suffix) != 0) {
        const int error = errno;
        Unmap(aligned, allocLen - prefix);
        throw std::system_error(error, std::generic_category(), "Mapping suffix munmap failed");
    }
    return aligned;
}

// SCE protections as the title set them: the host page protection keeps only the CPU bits, but titles
// read the GPU and AMPR bits back (a streamer reads straight into memory the APR may write).
struct ProtectedRange {
    std::uintptr_t end;
    int prot;
};
std::mutex g_protectionLock;
std::map<std::uintptr_t, ProtectedRange> g_protections;

void EraseProtections(std::uintptr_t start, std::uintptr_t end) {
    auto it = g_protections.lower_bound(start);
    if (it != g_protections.begin() && std::prev(it)->second.end > start) --it;
    while (it != g_protections.end() && it->first < end) {
        const auto rangeStart = it->first;
        const auto range = it->second;
        it = g_protections.erase(it);
        if (rangeStart < start) g_protections.emplace(rangeStart, ProtectedRange{start, range.prot});
        if (range.end > end) it = g_protections.emplace(end, ProtectedRange{range.end, range.prot}).first;
    }
}

void RecordProtection(const void* addr, size_t len, int prot) {
    const auto start = reinterpret_cast<std::uintptr_t>(addr);
    std::lock_guard lock(g_protectionLock);
    EraseProtections(start, start + len);
    if (prot >= 0) g_protections.emplace(start, ProtectedRange{start + len, prot});
}

void ValidateOutput(void** addr) {
    if (!addr) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Null memory mapping output");
    }
}

}

int DoMapDirect(void** addr, size_t len, int prot, int flags, int64_t physStart, size_t alignment) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    if (physStart < 0 || (static_cast<std::uint64_t>(physStart) & (PS5_PAGE_SIZE - 1)) != 0 || static_cast<std::uint64_t>(physStart) >= DIRECT_MEMORY_SIZE || len > DIRECT_MEMORY_SIZE - static_cast<std::uint64_t>(physStart)) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    GuestAllocations::Mutation mutation;
    if (RemapFixedIntoRegistered(mutation, *addr, len, prot, flags, physStart)) {
        RecordProtection(*addr, len, prot);
        return 0;
    }
    if (*addr != nullptr && (flags & GuestMapFixedFlag) != 0) mutation.RequireAvailable(*addr, len);
    std::lock_guard lock(g_directLock);
    void* mapped = MapAligned(*addr, len, LinuxProtFromSce(prot), flags, alignment);
    try {
        mutation.Add(mapped, len, (prot & 3) != 0, (prot & 2) != 0);
        AddMapping(reinterpret_cast<std::uintptr_t>(mapped), len, static_cast<std::uint64_t>(physStart), LinuxProtFromSce(prot));
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    RecordProtection(mapped, len, prot);
    Trace("map direct %p+0x%zx phys=0x%llx prot=0x%x flags=0x%x align=0x%zx", mapped, len, static_cast<unsigned long long>(physStart), prot, flags, alignment);
    return 0;
}

int DoMapAnon(void** addr, size_t len, int prot, int flags) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    if (RemapFixedIntoRegistered(mutation, *addr, len, prot, flags)) {
        RecordProtection(*addr, len, prot);
        return 0;
    }
    if (*addr != nullptr && (flags & GuestMapFixedFlag) != 0) mutation.RequireAvailable(*addr, len);
    void* mapped = MapAligned(*addr, len, LinuxProtFromSce(prot), flags, PS5_PAGE_SIZE);
    try {
        mutation.Add(mapped, len, (prot & 3) != 0, (prot & 2) != 0);
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    RecordProtection(mapped, len, prot);
    Trace("map anon %p+0x%zx prot=0x%x flags=0x%x", mapped, len, prot, flags);
    return 0;
}

int DoMprotect(const void* addr, size_t len, int prot) {
    Trace("protect %p+0x%zx prot=0x%x", addr, len, prot);
    const auto address = reinterpret_cast<std::uintptr_t>(addr);
    constexpr auto pageMask = static_cast<std::uintptr_t>(PS5_PAGE_SIZE - 1);
    const auto limit = std::numeric_limits<std::uintptr_t>::max();
    if (address == 0 || len == 0 || len > limit - address || address + len > limit - pageMask) throw std::invalid_argument("Invalid guest memory protection range");
    const auto first = address & ~pageMask;
    const auto end = (address + len + pageMask) & ~pageMask;
    const auto bytes = static_cast<std::size_t>(end - first);
    const auto* pointer = reinterpret_cast<const void*>(first);
    const auto nativeProtection = LinuxProtFromSce(prot);
    GuestAllocations::Mutation mutation;
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(pointer, &memory, sizeof(memory)) != sizeof(memory)) throw std::runtime_error("Cannot query guest memory protection range");
    if (memory.Type == MEM_IMAGE) {
        if (memory.AllocationBase != GetModuleHandleW(nullptr)) throw std::invalid_argument("Memory protection of a foreign image is not supported");
        mutation.RegisterMainImage();
    }
#else
    mutation.RegisterMainImage();
#endif
    mutation.Protect(pointer, bytes, (prot & 3) != 0, (prot & 2) != 0, [&] {
        if (mprotect(const_cast<void*>(pointer), bytes, nativeProtection) != 0) throw std::system_error(errno, std::generic_category(), "mprotect failed");
        std::lock_guard lock(g_directLock);
        for (auto it = g_shared.lower_bound(first); it != g_shared.end() && *it < end; ++it) ProtectOrThrow(*it, PS5_PAGE_SIZE, SharedProtection(nativeProtection));
    });
    RecordProtection(pointer, bytes, prot);
    return 0;
}

int DoMunmap(void* addr, size_t len) {
    Trace("unmap %p+0x%zx", addr, len);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0 || !addr) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    mutation.Unmap(addr, len, [&](const void* allocation, bool last) {
        {
            const auto address = reinterpret_cast<std::uintptr_t>(addr);
            std::lock_guard lock(g_directLock);
            EraseMappings(address, address + len, true);
        }
#if defined(__linux__)
        Unmap(addr, len);
#else
        if (KernelArena::Get().Contains(addr, len)) munmap(addr, len);
        else if (last) munmap_release(const_cast<void*>(allocation));
        else munmap(addr, len);
#endif
    });
    RecordProtection(addr, len, -1);
    return 0;
}

int DoReserveVirtual(void** addr, size_t len, size_t alignment) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    void* mapped = MapAligned(nullptr, len, PROT_NONE, 0, alignment);
    try {
        mutation.Add(mapped, len, false, false);
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    Trace("reserve %p+0x%zx align=0x%zx", mapped, len, alignment);
    return 0;
}

void ForgetDirectMemory(int64_t start, size_t len) {
    const auto first = static_cast<std::uint64_t>(start);
    const auto end = first + len;
    std::lock_guard lock(g_directLock);
    g_physPages.erase(g_physPages.lower_bound(first), g_physPages.lower_bound(end));
    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> released;
    for (const auto& [base, mapping] : g_directMappings) {
        const auto mappedEnd = mapping.phys + (mapping.end - base);
        if (mapping.phys < end && first < mappedEnd) released.emplace_back(base + (std::max(first, mapping.phys) - mapping.phys), base + (std::min(end, mappedEnd) - mapping.phys));
    }
    for (const auto& [from, to] : released) EraseMappings(from, to, false);
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
