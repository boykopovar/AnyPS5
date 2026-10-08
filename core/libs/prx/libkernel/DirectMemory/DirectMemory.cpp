#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <algorithm>
#include <cerrno>
#include <iterator>
#include <map>
#include <mutex>
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
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
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
    GuestArena::GuestArenaCommit_nid_postfix(addr, len, winProt, PS5_PAGE_SIZE);
}

static void* mmap_aligned(size_t len, int prot, size_t alignment, std::uintptr_t hint = 0) {
    auto& arena = KernelArena::Get();
    void* result = GuestArena::GuestArenaAllocateAtOrAbove_nid_postfix(hint, len, alignment);
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
    auto& arena = KernelArena::Get();
    if (arena.Contains(addr, len)) GuestArena::GuestArenaReset_nid_postfix(addr, len);
    else if (!VirtualFree(addr, len, MEM_DECOMMIT)) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualFree decommit failed");
    if (arena.Contains(addr, len)) arena.Release(addr, len);
    return 0;
}

static int munmap_release(void* addr) {
    if (!VirtualFree(addr, 0, MEM_RELEASE))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualFree release failed");
    return 0;
}

static int mprotect(void* addr, size_t len, int prot) {
    GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(addr), len, WinProtFromPosix(prot));
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
    if ((prot & ~0x3F7) != 0) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Unsupported memory protection bits: " + std::to_string(prot));
    }
    int result = PROT_NONE;
    if (prot & 0x113) result |= PROT_READ;
    if (prot & 0x222) result |= PROT_READ | PROT_WRITE;
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

class PhysicalBacking {
public:
    explicit PhysicalBacking(std::size_t bytes, int memoryType) : memoryType(memoryType) {
#ifdef _WIN32
        const auto size = static_cast<std::uint64_t>(bytes);
        section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, static_cast<DWORD>(size >> 32), static_cast<DWORD>(size), nullptr);
        if (!section) {
            const auto error = static_cast<int>(GetLastError());
            char message[96];
            std::snprintf(message, sizeof(message), "create direct memory backing of 0x%llx bytes (%llu MiB)", static_cast<unsigned long long>(size), static_cast<unsigned long long>((size + 0xFFFFF) >> 20));
            throw std::system_error(error, std::system_category(), message);
        }
#else
        file = memfd_create("direct memory", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        if (file < 0) throw std::system_error(errno, std::generic_category(), "create direct memory backing");
        if (ftruncate(file, static_cast<off_t>(bytes)) != 0) {
            const int error = errno;
            ::close(file);
            throw std::system_error(error, std::generic_category(), "size direct memory backing");
        }
        if (fcntl(file, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW) != 0) {
            const int error = errno;
            ::close(file);
            throw std::system_error(error, std::generic_category(), "seal direct memory backing");
        }
#endif
    }

    int MemoryType() const { return memoryType; }

#if defined(__linux__)
    int File() const { return file; }
#endif

    ~PhysicalBacking() {
#ifdef _WIN32
        CloseHandle(section);
#else
        ::close(file);
#endif
    }

    PhysicalBacking(const PhysicalBacking&) = delete;
    PhysicalBacking& operator=(const PhysicalBacking&) = delete;

    void Map(std::uintptr_t address, std::size_t bytes, std::uint64_t offset, int protection) const {
#ifdef _WIN32
        GuestArena::GuestArenaMap_nid_postfix(reinterpret_cast<void*>(address), bytes, section, offset, WinProtFromPosix(protection));
#else
        if (::mmap(reinterpret_cast<void*>(address), bytes, protection, MAP_SHARED | MAP_FIXED, file, static_cast<off_t>(offset)) == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "map direct memory backing");
#endif
    }

private:
    int memoryType;
#ifdef _WIN32
    HANDLE section = nullptr;
#else
    int file = -1;
#endif
};

struct PhysicalPage {
    std::shared_ptr<PhysicalBacking> backing;
    std::uint64_t offset;
};

std::mutex g_directLock;
std::map<std::uint64_t, PhysicalPage> g_physPages;

struct DirectMapping {
    std::uintptr_t end;
    std::uint64_t phys;
    int memoryType;
    std::shared_ptr<PhysicalBacking> backing;
};

std::map<std::uintptr_t, DirectMapping> g_directMappings;

void EraseMappings(std::uintptr_t start, std::uintptr_t end) {
    auto it = g_directMappings.lower_bound(start);
    if (it != g_directMappings.begin() && std::prev(it)->second.end > start) --it;
    while (it != g_directMappings.end() && it->first < end) {
        const auto base = it->first;
        const auto mapping = it->second;
        it = g_directMappings.erase(it);
        if (base < start) g_directMappings.emplace(base, DirectMapping{start, mapping.phys, mapping.memoryType, mapping.backing});
        if (mapping.end > end) it = g_directMappings.emplace(end, DirectMapping{mapping.end, mapping.phys + end - base, mapping.memoryType, mapping.backing}).first;
    }
}

void ErasePhysMappings(std::uint64_t first, std::uint64_t last) {
    for (auto it = g_directMappings.begin(); it != g_directMappings.end();) {
        const auto base = it->first;
        const auto mapping = it->second;
        const auto mappingLast = mapping.phys + (mapping.end - base);
        if (mapping.phys >= last || mappingLast <= first) {
            ++it;
            continue;
        }
        it = g_directMappings.erase(it);
        if (mapping.phys < first) {
            const auto keep = first - mapping.phys;
            g_directMappings.emplace(base, DirectMapping{base + keep, mapping.phys, mapping.memoryType, mapping.backing});
        }
        if (mappingLast > last) {
            const auto skip = last - mapping.phys;
            it = g_directMappings.emplace(base + skip, DirectMapping{mapping.end, last, mapping.memoryType, mapping.backing}).first;
        }
    }
}


void ValidatePhysicalRange(std::uint64_t phys, std::size_t len) {
    for (std::size_t offset = 0; offset < len; offset += PS5_PAGE_SIZE) {
        if (!g_physPages.contains(phys + offset)) {
            char message[160];
            std::snprintf(message, sizeof(message), "direct memory range 0x%llx+0x%zx references unallocated physical page 0x%llx", static_cast<unsigned long long>(phys), len, static_cast<unsigned long long>(phys + offset));
            throw std::invalid_argument(message);
        }
    }
}

#if defined(__linux__)
void WatchMapping(std::uintptr_t address, std::size_t len) {
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<const void*>(address), len);
    const auto end = address + len;
    for (auto added = g_directMappings.lower_bound(address); added != g_directMappings.end() && added->first < end; ++added) {
        const auto addedPhys = added->second.phys;
        const auto addedPhysEnd = addedPhys + (added->second.end - added->first);
        for (const auto& [base, other] : g_directMappings) {
            if ((base >= address && base < end) || other.backing != added->second.backing) continue;
            const auto first = std::max(addedPhys, other.phys);
            const auto last = std::min(addedPhysEnd, other.phys + (other.end - base));
            if (first >= last) continue;
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<const void*>(added->first + (first - addedPhys)), last - first);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<const void*>(base + (first - other.phys)), last - first);
        }
    }
}
#endif

#if defined(__linux__)
bool SharedBacking(std::uintptr_t address, std::size_t bytes, int* file, std::uint64_t* offset) {
    std::lock_guard lock(g_directLock);
    const auto next = g_directMappings.upper_bound(address);
    if (bytes == 0 || bytes > std::numeric_limits<std::uintptr_t>::max() - address || next == g_directMappings.begin()) return false;
    auto it = std::prev(next);
    if (address >= it->second.end) return false;
    const auto backing = it->second.backing;
    const auto phys = it->second.phys + (address - it->first);
    const auto page = g_physPages.find(phys - phys % PS5_PAGE_SIZE);
    if (page == g_physPages.end() || page->second.backing != backing) return false;
    const auto end = address + bytes;
    for (auto covered = it->second.end; covered < end;) {
        const auto following = std::next(it);
        if (following == g_directMappings.end() || following->first != covered || following->second.backing != backing || following->second.phys != it->second.phys + (it->second.end - it->first)) return false;
        it = following;
        covered = it->second.end;
    }
    const int duplicate = fcntl(backing->File(), F_DUPFD_CLOEXEC, 0);
    if (duplicate < 0) throw std::system_error(errno, std::generic_category(), "duplicate direct memory backing");
    *file = duplicate;
    *offset = page->second.offset + phys % PS5_PAGE_SIZE;
    return true;
}
#endif

void AddMapping(std::uintptr_t address, std::size_t len, std::uint64_t phys, int nativeProt) {
    ValidatePhysicalRange(phys, len);
    EraseMappings(address, address + len);
    for (std::size_t offset = 0; offset < len;) {
        const auto& page = g_physPages.at(phys + offset);
        std::size_t bytes = PS5_PAGE_SIZE;
        while (offset + bytes < len) {
            const auto& next = g_physPages.at(phys + offset + bytes);
            if (next.backing != page.backing || next.offset != page.offset + bytes) break;
            bytes += PS5_PAGE_SIZE;
        }
        page.backing->Map(address + offset, bytes, page.offset, nativeProt);
        g_directMappings.emplace(address + offset, DirectMapping{address + offset + bytes, phys + offset, page.backing->MemoryType(), page.backing});
        offset += bytes;
    }
#if defined(__linux__)
    WatchMapping(address, len);
#endif
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
        if (physStart >= 0) {
            AddMapping(address, len, static_cast<std::uint64_t>(physStart), nativeProtection);
        } else {
            EraseMappings(address, address + len);
#ifdef _WIN32
            GuestArena::GuestArenaReset_nid_postfix(addr, len);
            if (nativeProtection != PROT_NONE) CommitArenaRange(addr, len, WinProtFromPosix(nativeProtection));
#else
            if (::mmap(addr, len, nativeProtection, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "remap anonymous memory");
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(addr, len);
#endif
        }
    });
    return true;
}

void Unmap(void* addr, size_t len) {
#if defined(__linux__)
    if (munmap(addr, len) != 0) throw std::system_error(errno, std::generic_category(), "munmap failed");
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(addr, len);
#else
    if (KernelArena::Get().Contains(addr, len)) munmap(addr, len);
    else munmap_release(addr);
#endif
}

void* MapPlaced(void* addr, size_t len, int prot, int flags, size_t alignment) {
    ValidateLength(len);
    alignment = ValidateAlignment(alignment);
    constexpr int GuestMapFixed = 0x10;
    constexpr int GuestMapNoOverwrite = 0x80;
    constexpr int GuestMapNoCoalesce = 0x400000;
    constexpr int SupportedFlags = GuestMapFixed | GuestMapNoOverwrite | GuestMapNoCoalesce;
    if ((flags & ~SupportedFlags) != 0) {
        char message[64];
        std::snprintf(message, sizeof(message), "Unsupported memory mapping flags 0x%x", flags);
        throw std::invalid_argument(message);
    }
    if ((flags & GuestMapFixed) != 0) {
        ValidateRange(addr, len, alignment);
#ifdef _WIN32
        if ((flags & GuestMapNoOverwrite) != 0) {
            const auto start = reinterpret_cast<std::uintptr_t>(addr);
            auto cursor = start;
            while (cursor - start < len) {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<LPCVOID>(cursor), &info, sizeof(info)) != sizeof(info))
                    throw std::system_error(EINVAL, std::generic_category(), "No-overwrite range query failed");
                if (info.State == MEM_FREE) {
                    cursor = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
                    continue;
                }
                const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
                if (regionEnd <= cursor) throw std::system_error(EINVAL, std::generic_category(), "No-overwrite range query failed");
                const auto usedEnd = std::min(regionEnd, start + len);
                const auto usedBegin = std::max(reinterpret_cast<std::uintptr_t>(info.BaseAddress), start);
                if (info.State != MEM_RESERVE || !GuestArena::GuestArenaContains_nid_postfix(reinterpret_cast<void*>(usedBegin), usedEnd - usedBegin)) {
                    char busy[160];
                    std::snprintf(busy, sizeof(busy), "No-overwrite range %p+0x%zx is occupied (state=0x%lx prot=0x%lx)", addr, len,
                        (unsigned long)info.State, (unsigned long)info.Protect);
                    throw std::system_error(EEXIST, std::generic_category(), busy);
                }
                cursor = usedEnd;
            }
        }
#endif
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
        return mmap_aligned(len, prot, alignment, reinterpret_cast<std::uintptr_t>(addr));
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

std::mutex g_reservationLock;
std::map<std::uintptr_t, std::uintptr_t> g_reservations;

void EraseReservations(const void* addr, size_t len) {
    const auto start = reinterpret_cast<std::uintptr_t>(addr);
    const auto end = start + len;
    std::lock_guard lock(g_reservationLock);
    auto it = g_reservations.lower_bound(start);
    if (it != g_reservations.begin() && std::prev(it)->second > start) --it;
    while (it != g_reservations.end() && it->first < end) {
        const auto rangeStart = it->first;
        const auto rangeEnd = it->second;
        it = g_reservations.erase(it);
        if (rangeStart < start) g_reservations.emplace(rangeStart, start);
        if (rangeEnd > end) it = g_reservations.emplace(end, rangeEnd).first;
    }
}

void RecordReservation(const void* addr, size_t len) {
    EraseReservations(addr, len);
    const auto start = reinterpret_cast<std::uintptr_t>(addr);
    std::lock_guard lock(g_reservationLock);
    g_reservations.emplace(start, start + len);
}

void RecordProtection(const void* addr, size_t len, int prot) {
    const auto start = reinterpret_cast<std::uintptr_t>(addr);
    std::lock_guard lock(g_protectionLock);
    EraseProtections(start, start + len);
    if (prot >= 0) g_protections.emplace(start, ProtectedRange{start + len, prot});
}

void* MapAligned(void* addr, size_t len, int prot, int flags, size_t alignment) {
    void* mapped = MapPlaced(addr, len, prot, flags, alignment);
#if defined(__linux__)
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(mapped, len);
#endif
    return mapped;
}

void ValidateOutput(void** addr) {
    if (!addr) {
        // return SCE_KERNEL_ERROR_EINVAL;
        throw std::invalid_argument("Null memory mapping output");
    }
}

}

bool Reserved(const void* addr, size_t len) {
    const auto start = reinterpret_cast<std::uintptr_t>(addr);
    const auto end = start + len;
    std::lock_guard lock(g_reservationLock);
    auto it = g_reservations.upper_bound(start);
    if (it == g_reservations.begin()) return false;
    auto cursor = start;
    for (--it; it != g_reservations.end() && it->first <= cursor; ++it) {
        cursor = std::max(cursor, it->second);
        if (cursor >= end) return true;
    }
    return false;
}

void UnmapRegistered(GuestAllocations::Mutation& mutation, void* addr, size_t len) {
    mutation.Unmap(addr, len, [&](const void* piece, std::size_t pieceBytes, const void* allocation, bool last) {
        auto* pieceAddress = const_cast<void*>(piece);
        std::lock_guard lock(g_directLock);
        EraseMappings(reinterpret_cast<std::uintptr_t>(piece), reinterpret_cast<std::uintptr_t>(piece) + pieceBytes);
#if defined(__linux__)
        Unmap(pieceAddress, pieceBytes);
#else
        if (KernelArena::Get().Contains(pieceAddress, pieceBytes)) munmap(pieceAddress, pieceBytes);
        else if (last) munmap_release(const_cast<void*>(allocation));
        else munmap(pieceAddress, pieceBytes);
#endif
    });
    EraseReservations(addr, len);
    RecordProtection(addr, len, -1);
}

void ReplaceFixedOverlap(GuestAllocations::Mutation& mutation, void* addr, size_t len, int flags) {
    constexpr int GuestMapNoOverwrite = 0x80;
    if (addr == nullptr || (flags & GuestMapFixedFlag) == 0) return;
    if ((flags & GuestMapNoOverwrite) == 0 && mutation.Overlaps(addr, len)) UnmapRegistered(mutation, addr, len);
    mutation.RequireAvailable(addr, len);
}

bool FixedNoOverwriteConflict(const GuestAllocations::Mutation& mutation, void* addr, size_t len, int flags) {
    constexpr int GuestMapNoOverwrite = 0x80;
    if (addr == nullptr || (flags & GuestMapFixedFlag) == 0 || (flags & GuestMapNoOverwrite) == 0) return false;
    return mutation.Overlaps(addr, len) && !Reserved(addr, len);
}

int DoMapDirect(void** addr, size_t len, int prot, int flags, int64_t physStart, size_t alignment) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    if (physStart < 0 || (static_cast<std::uint64_t>(physStart) & (PS5_PAGE_SIZE - 1)) != 0 || static_cast<std::uint64_t>(physStart) >= DIRECT_MEMORY_SIZE || len > DIRECT_MEMORY_SIZE - static_cast<std::uint64_t>(physStart)) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    GuestAllocations::Mutation mutation;
    if (FixedNoOverwriteConflict(mutation, *addr, len, flags)) return SCE_KERNEL_ERROR_ENOMEM;
    if (RemapFixedIntoRegistered(mutation, *addr, len, prot, flags, physStart)) {
        EraseReservations(*addr, len);
        RecordProtection(*addr, len, prot);
        return 0;
    }
    ReplaceFixedOverlap(mutation, *addr, len, flags);
    std::lock_guard lock(g_directLock);
    ValidatePhysicalRange(static_cast<std::uint64_t>(physStart), len);
    void* mapped = MapAligned(*addr, len, PROT_NONE, flags, alignment);
    try {
        AddMapping(reinterpret_cast<std::uintptr_t>(mapped), len, static_cast<std::uint64_t>(physStart), LinuxProtFromSce(prot));
        mutation.Add(mapped, len, (prot & 3) != 0, (prot & 2) != 0);
    } catch (...) {
        EraseMappings(reinterpret_cast<std::uintptr_t>(mapped), reinterpret_cast<std::uintptr_t>(mapped) + len);
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    EraseReservations(mapped, len);
    RecordProtection(mapped, len, prot);
    Trace("map direct %p+0x%zx phys=0x%llx prot=0x%x flags=0x%x align=0x%zx", mapped, len, static_cast<unsigned long long>(physStart), prot, flags, alignment);
    return 0;
}

int DoMapAnon(void** addr, size_t len, int prot, int flags) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    if (FixedNoOverwriteConflict(mutation, *addr, len, flags)) return SCE_KERNEL_ERROR_ENOMEM;
    if (RemapFixedIntoRegistered(mutation, *addr, len, prot, flags)) {
        EraseReservations(*addr, len);
        RecordProtection(*addr, len, prot);
        return 0;
    }
    ReplaceFixedOverlap(mutation, *addr, len, flags);
    void* mapped = MapAligned(*addr, len, LinuxProtFromSce(prot), flags, PS5_PAGE_SIZE);
    try {
        mutation.Add(mapped, len, (prot & 3) != 0, (prot & 2) != 0);
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    EraseReservations(mapped, len);
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
    });
    RecordProtection(pointer, bytes, prot);
    return 0;
}

int DoMtypeprotect(const void* addr, size_t len, int type, int prot) {
    const int result = DoMprotect(addr, len, prot);
    const auto address = reinterpret_cast<std::uintptr_t>(addr);
    constexpr auto pageMask = static_cast<std::uintptr_t>(PS5_PAGE_SIZE - 1);
    const auto first = address & ~pageMask;
    const auto end = (address + len + pageMask) & ~pageMask;
    std::vector<std::pair<std::uint64_t, std::size_t>> physical;
    {
        std::lock_guard lock(g_directLock);
        auto it = g_directMappings.lower_bound(first);
        if (it != g_directMappings.begin() && std::prev(it)->second.end > first) --it;
        while (it != g_directMappings.end() && it->first < end) {
            const auto base = it->first;
            const auto mapping = it->second;
            it = g_directMappings.erase(it);
            if (base < first) g_directMappings.emplace(base, DirectMapping{first, mapping.phys, mapping.memoryType, mapping.backing});
            const auto low = std::max(base, first);
            const auto high = std::min(mapping.end, end);
            g_directMappings.emplace(low, DirectMapping{high, mapping.phys + low - base, type, mapping.backing});
            physical.emplace_back(mapping.phys + low - base, high - low);
            if (mapping.end > end) it = g_directMappings.emplace(end, DirectMapping{mapping.end, mapping.phys + end - base, mapping.memoryType, mapping.backing}).first;
        }
    }
    for (const auto& [phys, bytes] : physical) DirectMemoryRetype(static_cast<int64_t>(phys), bytes, type);
    return result;
}

int DoMunmap(void* addr, size_t len) {
    Trace("unmap %p+0x%zx", addr, len);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0 || !addr) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    UnmapRegistered(mutation, addr, len);
    return 0;
}

int DoReserveVirtual(void** addr, size_t len, int flags, size_t alignment) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    GuestAllocations::Mutation mutation;
    const bool fixed = *addr != nullptr && (flags & GuestMapFixedFlag) != 0;
    if (fixed && mutation.Covers(*addr, len)) {
        constexpr int GuestMapNoOverwrite = 0x80;
        if ((flags & GuestMapNoOverwrite) == 0 && RemapFixedIntoRegistered(mutation, *addr, len, 0, GuestMapFixedFlag)) {
            RecordProtection(*addr, len, -1);
            RecordReservation(*addr, len);
            return 0;
        }
        mutation.RequireAvailable(*addr, len);
    }
    if (fixed) mutation.RequireAvailable(*addr, len);
    constexpr int GuestMapNoCoalesce = 0x400000;
    void* mapped = MapAligned(fixed ? *addr : nullptr, len, PROT_NONE, fixed ? GuestMapFixedFlag | (flags & GuestMapNoCoalesce) : 0, alignment);
    try {
        mutation.Add(mapped, len, false, false);
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    RecordReservation(mapped, len);
    Trace("reserve %p+0x%zx flags=0x%x align=0x%zx", mapped, len, flags, alignment);
    return 0;
}

void CreateDirectMemoryBacking(int64_t start, size_t len, int memoryType) {
    std::lock_guard lock(g_directLock);
    const auto first = static_cast<std::uint64_t>(start);
    for (std::size_t offset = 0; offset < len; offset += PS5_PAGE_SIZE) {
        if (g_physPages.contains(first + offset)) throw std::runtime_error("physical allocation overlaps live direct memory");
    }
#if defined(__linux__)
    static std::once_flag registered;
    std::call_once(registered, [] { GuestArena::GuestArenaSetSharedBacking_nid_postfix(&SharedBacking); });
#endif
    const auto backing = std::make_shared<PhysicalBacking>(len, memoryType);
    std::map<std::uint64_t, PhysicalPage> pages;
    for (std::size_t offset = 0; offset < len; offset += PS5_PAGE_SIZE) pages.emplace(first + offset, PhysicalPage{backing, offset});
    g_physPages.merge(pages);
    Trace("allocate physical 0x%llx+0x%zx", static_cast<unsigned long long>(first), len);
}

void ForgetDirectMemory(int64_t start, size_t len) {
    const auto first = static_cast<std::uint64_t>(start);
    std::lock_guard lock(g_directLock);
    ValidatePhysicalRange(first, len);
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

bool QueryDirectMapping(std::uintptr_t address, std::uintptr_t* start, std::uintptr_t* end, std::uint64_t* offset, int* memoryType) {
    std::lock_guard lock(g_directLock);
    const auto next = g_directMappings.upper_bound(address);
    if (next == g_directMappings.begin()) return false;
    const auto it = std::prev(next);
    if (address >= it->second.end) return false;
    *start = it->first;
    *end = it->second.end;
    *offset = it->second.phys;
    *memoryType = it->second.memoryType;
    return true;
}
