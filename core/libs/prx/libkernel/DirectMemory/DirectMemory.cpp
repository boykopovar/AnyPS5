#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <algorithm>
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
    bool Owns(const void* pointer, size_t len) const { return Contains(pointer, len) && !GuestArena::GuestArenaHostRegionOverlaps_nid_postfix(reinterpret_cast<std::uintptr_t>(pointer), len); }
    void* Allocate(size_t len, size_t alignment) { return GuestArena::GuestArenaAllocate_nid_postfix(len, alignment); }
    void MarkUsed(const void* pointer, size_t len) { GuestArena::GuestArenaMarkUsed_nid_postfix(pointer, len); }
    void Release(const void* pointer, size_t len) { GuestArena::GuestArenaRelease_nid_postfix(pointer, len); }
};

static void CommitArenaRange(void* addr, size_t len, DWORD winProt) {
    GuestArena::GuestArenaCommit_nid_postfix(addr, len, winProt, PS5_PAGE_SIZE);
}

static void* mmap_aligned(size_t len, int prot, size_t alignment, std::uintptr_t hint = 0) {
    auto& arena = KernelArena::Get();
    void* result = GuestArena::GuestArenaAllocateFromHint_nid_postfix(hint, len, alignment);
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

static void CommitReservedPages(void* addr, size_t len) {
    auto cursor = reinterpret_cast<std::uintptr_t>(addr);
    const auto end = cursor + len;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<void*>(cursor), &memory, sizeof(memory)) != sizeof(memory))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualQuery failed");
        const auto regionEnd = std::min(end, reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize);
        if (memory.State == MEM_RESERVE && !VirtualAlloc(reinterpret_cast<void*>(cursor), regionEnd - cursor, MEM_COMMIT, PAGE_READWRITE))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualAlloc stack commit failed");
        cursor = regionEnd;
    }
}

static std::uintptr_t AboveStackGuard(std::uintptr_t start, std::uintptr_t end) {
    auto cursor = start;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<void*>(cursor), &memory, sizeof(memory)) != sizeof(memory))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualQuery failed");
        if (memory.State != MEM_RESERVE && (memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) == 0)) break;
        cursor = std::min(end, reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize);
    }
    return cursor;
}

static bool AtStackReserveBottom(std::uintptr_t start, std::uintptr_t end) {
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(reinterpret_cast<void*>(start), &memory, sizeof(memory)) != sizeof(memory))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "VirtualQuery failed");
    return memory.State == MEM_RESERVE && reinterpret_cast<std::uintptr_t>(memory.AllocationBase) == start && reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize > end;
}

static int mprotect(void* addr, size_t len, int prot) {
    GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(addr), len, WinProtFromPosix(prot));
    if (prot != PROT_NONE && KernelArena::Get().Owns(addr, len)) CommitArenaRange(addr, len, WinProtFromPosix(prot));
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

#if defined(__linux__)
static void MapUnmappedPages(void* addr, size_t len) {
    constexpr std::uintptr_t hostPage = 0x1000;
    auto cursor = reinterpret_cast<std::uintptr_t>(addr);
    const auto end = cursor + len;
    const auto half = [&](std::size_t bytes) { return std::max<std::size_t>(hostPage, (bytes / 2) & ~(hostPage - 1)); };
    while (cursor < end) {
        auto* run = reinterpret_cast<void*>(cursor);
        auto bytes = static_cast<std::size_t>(end - cursor);
        if (::msync(run, hostPage, MS_ASYNC) == 0) {
            while (bytes > hostPage && ::msync(run, bytes, MS_ASYNC) != 0) bytes = half(bytes);
            cursor += bytes;
            continue;
        }
        void* mapped = MAP_FAILED;
        while ((mapped = ::mmap(run, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE | MAP_NORESERVE, -1, 0)) == MAP_FAILED && errno == EEXIST && bytes > hostPage) bytes = half(bytes);
        if (mapped != run) {
            const int error = mapped == MAP_FAILED ? errno : EEXIST;
            if (mapped != MAP_FAILED) ::munmap(mapped, bytes);
            throw std::system_error(error, std::generic_category(), "cannot map the unmapped pages of a host stack");
        }
        cursor += bytes;
    }
}
#endif

namespace {

constexpr int GuestMapFixedFlag = 0x10;
constexpr int GuestProtGpuReadWrite = 0x30;
constexpr int GuestProtCpuExecute = 0x4;

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

#if defined(__linux__)
int PhysicalFile() {
    static const int file = [] {
        const int created = memfd_create("direct memory", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        if (created < 0) throw std::system_error(errno, std::generic_category(), "create direct memory backing");
        if (ftruncate(created, static_cast<off_t>(DIRECT_MEMORY_SIZE)) != 0 || fcntl(created, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW) != 0) {
            const int error = errno;
            ::close(created);
            throw std::system_error(error, std::generic_category(), "size direct memory backing");
        }
        return created;
    }();
    return file;
}

std::vector<bool>& PhysicalUsed();

void ZeroReusedPhysical(std::uint64_t start, std::size_t bytes) {
    const auto first = start / PS5_PAGE_SIZE;
    const auto count = bytes / PS5_PAGE_SIZE;
    for (std::size_t page = 0; page < count;) {
        if (!PhysicalUsed()[first + page]) {
            PhysicalUsed()[first + page] = true;
            ++page;
            continue;
        }
        std::size_t run = 0;
        while (page + run < count && PhysicalUsed()[first + page + run]) ++run;
        const auto offset = (first + page) * PS5_PAGE_SIZE;
        void* view = ::mmap(nullptr, run * PS5_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, PhysicalFile(), static_cast<off_t>(offset));
        if (view == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "map reused direct memory");
        std::memset(view, 0, run * PS5_PAGE_SIZE);
        ::munmap(view, run * PS5_PAGE_SIZE);
        page += run;
    }
}
#endif

#ifdef _WIN32
bool SectionPoolEnabled() {
    static const bool enabled = std::getenv("APS5_NO_DIRECT_MEMORY_SECTION_POOL") == nullptr;
    return enabled;
}

class SectionPool {
public:
    static SectionPool& Get() {
        static auto* pool = new SectionPool;
        return *pool;
    }

    HANDLE Take(std::size_t bytes) {
        std::lock_guard lock(mutex);
        const auto found = sections.find(bytes);
        if (found == sections.end() || found->second.empty()) return nullptr;
        const auto section = found->second.back();
        found->second.pop_back();
        --count;
        pooledBytes -= bytes;
        return section;
    }

    bool Keep(HANDLE section, std::size_t bytes) noexcept {
        if (bytes > MaxSectionBytes) return false;
        std::lock_guard lock(mutex);
        if (count == MaxSections || bytes > MaxPooledBytes - pooledBytes) return false;
        try {
            sections[bytes].push_back(section);
        } catch (const std::bad_alloc&) {
            return false;
        }
        ++count;
        pooledBytes += bytes;
        return true;
    }

private:
    static constexpr std::size_t MaxSections = 256;
    static constexpr std::size_t MaxSectionBytes = 16u << 20;
    static constexpr std::size_t MaxPooledBytes = 256u << 20;
    std::mutex mutex;
    std::map<std::size_t, std::vector<HANDLE>> sections;
    std::size_t count = 0;
    std::size_t pooledBytes = 0;
};
#endif

class PhysicalBacking {
public:
    PhysicalBacking(std::size_t bytes, int memoryType, std::uint64_t start) : memoryType(memoryType), bytes(bytes) {
#if defined(__EMSCRIPTEN__)
        static_cast<void>(start);
#elif defined(_WIN32)
        static_cast<void>(start);
        const auto size = static_cast<std::uint64_t>(bytes);
        if (SectionPoolEnabled()) section = SectionPool::Get().Take(bytes);
        if (!section) section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, static_cast<DWORD>(size >> 32), static_cast<DWORD>(size), nullptr);
        if (!section) {
            const auto error = static_cast<int>(GetLastError());
            char message[96];
            std::snprintf(message, sizeof(message), "create direct memory backing of 0x%llx bytes (%llu MiB)", static_cast<unsigned long long>(size), static_cast<unsigned long long>((size + 0xFFFFF) >> 20));
            throw std::system_error(error, std::system_category(), message);
        }
#else
        file = PhysicalFile();
        fileOffset = start;
        ZeroReusedPhysical(start, bytes);
#endif
    }

    int MemoryType() const { return memoryType; }

#if defined(__linux__)
    int File() const { return file; }
    void* HostWriteData(std::uint64_t offset, std::size_t length) {
        if (offset > bytes || length > bytes - offset) return nullptr;
        if (hostWriteView == nullptr) {
            auto* mapped = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file, static_cast<off_t>(fileOffset));
            if (mapped == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "Direct memory write view mmap failed");
            hostWriteView = mapped;
        }
        return static_cast<std::byte*>(hostWriteView) + offset;
    }
    std::uint64_t FileOffset() const { return fileOffset; }
#endif

    ~PhysicalBacking() {
#if defined(__EMSCRIPTEN__)
#elif defined(_WIN32)
        if (!SectionPoolEnabled()) {
            GuestArena::GuestArenaForgetSection_nid_postfix(section);
            CloseHandle(section);
        } else if (!GuestArena::GuestArenaForgetReusableSection_nid_no_patch(section) || !SectionPool::Get().Keep(section, bytes)) {
            CloseHandle(section);
        }
#else
        if (hostWriteView != nullptr) ::munmap(hostWriteView, bytes);
#endif
    }

    PhysicalBacking(const PhysicalBacking&) = delete;
    PhysicalBacking& operator=(const PhysicalBacking&) = delete;

    void Map(std::uintptr_t address, std::size_t bytes, std::uint64_t offset, int protection) const {
#if defined(__EMSCRIPTEN__)
        static_cast<void>(offset);
        static_cast<void>(protection);
        GuestArena::GuestArenaCommit_nid_postfix(reinterpret_cast<void*>(address), bytes, 0x04, PS5_PAGE_SIZE);
#elif defined(_WIN32)
        GuestArena::GuestArenaMapDirect_nid_postfix(reinterpret_cast<void*>(address), bytes, section, offset, WinProtFromPosix(protection));
#else
        if (::mmap(reinterpret_cast<void*>(address), bytes, protection, MAP_SHARED | MAP_FIXED, file, static_cast<off_t>(fileOffset + offset)) == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "map direct memory backing");
#endif
    }

private:
    int memoryType;
    std::size_t bytes;
#if defined(__EMSCRIPTEN__)
#elif defined(_WIN32)
    HANDLE section = nullptr;
#else
    int file = -1;
    void* hostWriteView = nullptr;
    std::uint64_t fileOffset = 0;
#endif
};

struct PhysicalPage {
    std::shared_ptr<PhysicalBacking> backing;
    std::uint64_t offset;
};

struct PhysicalBlock {
    std::uint64_t end;
    std::uint64_t base;
    std::shared_ptr<PhysicalBacking> backing;
};

struct DirectMapping {
    std::uintptr_t end;
    std::uint64_t phys;
    int memoryType;
    int nativeProtection;
    std::shared_ptr<PhysicalBacking> backing;
};

struct PhysicalSpan {
    std::uint64_t end;
    std::vector<std::uintptr_t> bases;
};

struct ProtectedRange {
    std::uintptr_t end;
    int prot;
};

struct DirectMemoryState {
#if defined(__linux__)
    std::vector<bool> physicalUsed = std::vector<bool>(DIRECT_MEMORY_SIZE / PS5_PAGE_SIZE);
#endif
    std::map<std::uint64_t, PhysicalSpan> physicalSpans;
    std::mutex directLock;
    std::map<std::uint64_t, PhysicalBlock> physBlocks;
    std::map<std::uint64_t, PhysicalPage> physPages;
    std::map<std::uintptr_t, DirectMapping> directMappings;
    std::mutex protectionLock;
    std::map<std::uintptr_t, ProtectedRange> protections;
    std::mutex reservationLock;
    std::map<std::uintptr_t, std::uintptr_t> reservations;
};

DirectMemoryState& MemoryState() {
    static auto* state = new DirectMemoryState;
    return *state;
}

#if defined(__linux__)
std::vector<bool>& PhysicalUsed() { return MemoryState().physicalUsed; }
#endif

auto& g_directLock = MemoryState().directLock;
auto& g_physBlocks = MemoryState().physBlocks;
auto& g_physPages = MemoryState().physPages;
auto& g_directMappings = MemoryState().directMappings;
auto& g_physicalSpans = MemoryState().physicalSpans;
auto& g_protectionLock = MemoryState().protectionLock;
auto& g_protections = MemoryState().protections;
auto& g_reservationLock = MemoryState().reservationLock;
auto& g_reservations = MemoryState().reservations;

void SplitPhysicalSpan(std::uint64_t at) {
    auto span = g_physicalSpans.upper_bound(at);
    if (span == g_physicalSpans.begin()) return;
    --span;
    if (span->first == at || span->second.end <= at) return;
    g_physicalSpans.emplace_hint(std::next(span), at, PhysicalSpan{span->second.end, span->second.bases});
    span->second.end = at;
}

std::map<std::uintptr_t, DirectMapping>::iterator InsertMapping(std::uintptr_t base, DirectMapping mapping) {
    const auto [inserted, added] = g_directMappings.emplace(base, std::move(mapping));
    if (!added) throw std::runtime_error("direct mapping inserted over a live one");
    const auto first = inserted->second.phys;
    const auto last = first + (inserted->second.end - base);
    SplitPhysicalSpan(first);
    SplitPhysicalSpan(last);
    auto span = g_physicalSpans.lower_bound(first);
    for (auto cursor = first; cursor < last; ++span) {
        if (span == g_physicalSpans.end() || span->first > cursor) span = g_physicalSpans.emplace_hint(span, cursor, PhysicalSpan{span == g_physicalSpans.end() ? last : std::min(last, span->first), {}});
        auto& bases = span->second.bases;
        bases.insert(std::upper_bound(bases.begin(), bases.end(), base), base);
        cursor = span->second.end;
    }
    return inserted;
}

std::map<std::uintptr_t, DirectMapping>::iterator RemoveMapping(std::map<std::uintptr_t, DirectMapping>::iterator mapping) {
    const auto base = mapping->first;
    const auto first = mapping->second.phys;
    const auto last = first + (mapping->second.end - base);
    for (auto span = g_physicalSpans.lower_bound(first); span != g_physicalSpans.end() && span->first < last;) {
        auto& bases = span->second.bases;
        bases.erase(std::lower_bound(bases.begin(), bases.end(), base));
        span = bases.empty() ? g_physicalSpans.erase(span) : std::next(span);
    }
    for (auto span = g_physicalSpans.lower_bound(first); span != g_physicalSpans.end() && span->first <= last;) {
        if (span != g_physicalSpans.begin()) {
            const auto before = std::prev(span);
            if (before->second.end == span->first && before->second.bases == span->second.bases) {
                before->second.end = span->second.end;
                span = g_physicalSpans.erase(span);
                continue;
            }
        }
        ++span;
    }
    return g_directMappings.erase(mapping);
}

std::vector<std::uintptr_t> MappingsSharingPhys(std::uint64_t first, std::uint64_t last) {
    std::vector<std::uintptr_t> bases;
    for (auto span = g_physicalSpans.lower_bound(first); span != g_physicalSpans.end() && span->first < last; ++span) bases.insert(bases.end(), span->second.bases.begin(), span->second.bases.end());
    if (auto span = g_physicalSpans.lower_bound(first); span != g_physicalSpans.begin() && (--span)->second.end > first) bases.insert(bases.end(), span->second.bases.begin(), span->second.bases.end());
    std::sort(bases.begin(), bases.end());
    bases.erase(std::unique(bases.begin(), bases.end()), bases.end());
    return bases;
}

void EraseMappings(std::uintptr_t start, std::uintptr_t end) {
    auto it = g_directMappings.lower_bound(start);
    if (it != g_directMappings.begin() && std::prev(it)->second.end > start) --it;
    while (it != g_directMappings.end() && it->first < end) {
        const auto base = it->first;
        const auto mapping = it->second;
        it = RemoveMapping(it);
        if (base < start) InsertMapping(base, DirectMapping{start, mapping.phys, mapping.memoryType, mapping.nativeProtection, mapping.backing});
        if (mapping.end > end) it = InsertMapping(end, DirectMapping{mapping.end, mapping.phys + end - base, mapping.memoryType, mapping.nativeProtection, mapping.backing});
    }
}

void SplitPhysicalBlock(std::uint64_t at) {
    auto block = g_physBlocks.upper_bound(at);
    if (block == g_physBlocks.begin()) return;
    --block;
    if (block->first == at || block->second.end <= at) return;
    g_physBlocks.emplace_hint(std::next(block), at, PhysicalBlock{block->second.end, block->second.base, block->second.backing});
    block->second.end = at;
}

void ErasePhysMappings(std::uint64_t first, std::uint64_t last) {
    for (const auto base : MappingsSharingPhys(first, last)) {
        const auto it = g_directMappings.find(base);
        if (it == g_directMappings.end()) continue;
        const auto mapping = it->second;
        const auto mappingLast = mapping.phys + (mapping.end - base);
        if (mapping.phys >= last || mappingLast <= first) continue;
        RemoveMapping(it);
        if (mapping.phys < first) {
            const auto keep = first - mapping.phys;
            InsertMapping(base, DirectMapping{base + keep, mapping.phys, mapping.memoryType, mapping.nativeProtection, mapping.backing});
        }
        if (mappingLast > last) {
            const auto skip = last - mapping.phys;
            InsertMapping(base + skip, DirectMapping{mapping.end, last, mapping.memoryType, mapping.nativeProtection, mapping.backing});
        }
    }
}

std::optional<std::uint64_t> FirstUnallocatedPage(std::uint64_t phys, std::size_t len) {
    const auto last = phys + len;
    auto it = g_physBlocks.upper_bound(phys);
    if (it != g_physBlocks.begin()) --it;
    for (auto cursor = phys; cursor < last;) {
        if (it == g_physBlocks.end() || it->first > cursor || it->second.end <= cursor) return cursor;
        cursor = std::min(last, it->second.end);
        ++it;
    }
    return std::nullopt;
}

void ValidatePhysicalRange(std::uint64_t phys, std::size_t len) {
    const auto missing = FirstUnallocatedPage(phys, len);
    if (!missing) return;
    char message[160];
    std::snprintf(message, sizeof(message), "direct memory range 0x%llx+0x%zx references unallocated physical page 0x%llx", static_cast<unsigned long long>(phys), len, static_cast<unsigned long long>(*missing));
    throw std::invalid_argument(message);
}

#if defined(__linux__)
void WatchMapping(std::uintptr_t address, std::size_t len) {
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<const void*>(address), len);
    const auto end = address + len;
    for (auto added = g_directMappings.lower_bound(address); added != g_directMappings.end() && added->first < end; ++added) {
        const auto addedPhys = added->second.phys;
        const auto addedPhysEnd = addedPhys + (added->second.end - added->first);
        for (const auto base : MappingsSharingPhys(addedPhys, addedPhysEnd)) {
            const auto& other = g_directMappings.at(base);
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
    *offset = backing->FileOffset() + page->second.offset + phys % PS5_PAGE_SIZE;
    return true;
}

bool WriteSharedBacking(std::uintptr_t address, const void* source, std::size_t bytes) {
    if (bytes == 0 || bytes > std::numeric_limits<std::uintptr_t>::max() - address) return false;
    std::lock_guard lock(g_directLock);
    const auto next = g_directMappings.upper_bound(address);
    if (next == g_directMappings.begin()) return false;
    struct WriteSpan {
        void* destination;
        std::size_t bytes;
    };
    std::vector<WriteSpan> spans;
    const auto end = address + bytes;
    auto it = std::prev(next);
    for (auto cursor = address; cursor < end; ++it) {
        if (it == g_directMappings.end() || it->first > cursor || cursor >= it->second.end) return false;
        const auto physical = it->second.phys + (cursor - it->first);
        const auto page = g_physPages.find(physical - physical % PS5_PAGE_SIZE);
        if (page == g_physPages.end() || page->second.backing != it->second.backing) return false;
        const auto offset = page->second.offset + physical % PS5_PAGE_SIZE;
        const auto length = std::min(end, it->second.end) - cursor;
        auto* destination = it->second.backing->HostWriteData(offset, length);
        if (destination == nullptr) return false;
        spans.push_back({destination, length});
        cursor += length;
    }
    auto* input = static_cast<const std::byte*>(source);
    for (const auto& span : spans) {
        std::memcpy(span.destination, input, span.bytes);
        input += span.bytes;
    }
    return true;
}
#endif

void AddMapping(std::uintptr_t address, std::size_t len, std::uint64_t phys, int nativeProt, bool records = true) {
    ValidatePhysicalRange(phys, len);
    if (records) EraseMappings(address, address + len);
#ifdef __EMSCRIPTEN__
    if (records && !MappingsSharingPhys(phys, phys + len).empty()) throw std::system_error(ENOTSUP, std::generic_category(), "WebAssembly memory cannot map one direct memory range at two addresses");
#endif
    const auto last = phys + len;
    auto it = g_physBlocks.upper_bound(phys);
    --it;
    for (auto cursor = phys; cursor < last; ++it) {
        const auto runEnd = std::min(last, it->second.end);
        const auto bytes = static_cast<std::size_t>(runEnd - cursor);
        it->second.backing->Map(address + (cursor - phys), bytes, cursor - it->second.base, nativeProt);
        if (records) InsertMapping(address + (cursor - phys), DirectMapping{address + (cursor - phys) + bytes, cursor, it->second.backing->MemoryType(), nativeProt, it->second.backing});
        cursor = runEnd;
    }
#if defined(__linux__)
    WatchMapping(address, len);
#endif
}

void RequireResidentUnpinned(GuestAllocations::Mutation& mutation, std::uintptr_t address, std::size_t len) {
#ifdef _WIN32
    std::uintptr_t start = 0;
    std::uintptr_t end = 0;
    if (GuestArena::GuestArenaResidentSpan_nid_postfix(address, len, &start, &end)) mutation.RequireUnpinned(reinterpret_cast<const void*>(start), end - start);
#else
    static_cast<void>(mutation);
    static_cast<void>(address);
    static_cast<void>(len);
#endif
}

void RequireAliasesUnpinned(GuestAllocations::Mutation& mutation, std::uint64_t phys, std::size_t len) {
#ifdef _WIN32
    std::vector<std::pair<std::uintptr_t, std::size_t>> aliases;
    {
        std::lock_guard lock(g_directLock);
        for (const auto base : MappingsSharingPhys(phys, phys + len)) {
            const auto& mapping = g_directMappings.at(base);
            const auto first = std::max(phys, mapping.phys);
            const auto last = std::min(phys + len, mapping.phys + (mapping.end - base));
            if (first < last) aliases.emplace_back(base + (first - mapping.phys), last - first);
        }
    }
    for (const auto& [address, bytes] : aliases) RequireResidentUnpinned(mutation, address, bytes);
#else
    static_cast<void>(mutation);
    static_cast<void>(phys);
    static_cast<void>(len);
#endif
}

bool RemapFixedIntoRegistered(GuestAllocations::Mutation& mutation, void* addr, size_t len, int prot, int flags, int64_t physStart = -1) {
    constexpr int GuestMapFixed = 0x10;
    constexpr int GuestMapNoOverwrite = 0x80;
    if (addr == nullptr || (flags & GuestMapFixed) == 0 || (flags & GuestMapNoOverwrite) != 0 || !mutation.Covers(addr, len)) return false;
    ValidateRange(addr, len, PS5_PAGE_SIZE);
    RequireResidentUnpinned(mutation, reinterpret_cast<std::uintptr_t>(addr), len);
    if (physStart >= 0) RequireAliasesUnpinned(mutation, static_cast<std::uint64_t>(physStart), len);
    Trace("remap fixed %p+0x%zx prot=0x%x phys=0x%llx", addr, len, prot, static_cast<long long>(physStart));
    const auto nativeProtection = LinuxProtFromSce(prot);
    mutation.Protect(addr, len, (prot & 3) != 0, (prot & 2) != 0, (prot & GuestProtGpuReadWrite) != 0, [&] {
        const auto address = reinterpret_cast<std::uintptr_t>(addr);
        std::lock_guard lock(g_directLock);
        if (physStart >= 0) {
            const auto phys = static_cast<std::uint64_t>(physStart);
            auto existing = g_directMappings.upper_bound(address);
            if (existing != g_directMappings.begin() && (--existing)->first <= address && existing->second.end >= address + len && existing->second.phys + (address - existing->first) == phys && existing->second.nativeProtection == nativeProtection) {
                AddMapping(address, len, phys, nativeProtection, false);
                return;
            }
            AddMapping(address, len, phys, nativeProtection);
        } else {
            EraseMappings(address, address + len);
#ifdef _WIN32
            GuestArena::GuestArenaReset_nid_postfix(addr, len);
            if (nativeProtection != PROT_NONE) CommitArenaRange(addr, len, WinProtFromPosix(nativeProtection));
#elif defined(__EMSCRIPTEN__)
            GuestArena::GuestArenaReset_nid_postfix(addr, len);
            GuestArena::GuestArenaCommit_nid_postfix(addr, len, 0x04, PS5_PAGE_SIZE);
#else
            if (::mmap(addr, len, nativeProtection, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "remap anonymous memory");
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(addr, len);
#endif
        }
    });
    if ((prot & GuestProtGpuReadWrite) != 0) mutation.NoteGpuMapping(addr, len);
    return true;
}

#if defined(__linux__) || defined(__EMSCRIPTEN__)
bool ArenaAvailable() { return GuestArena::GuestArenaAvailable_nid_postfix(); }

void* ArenaCommit(void* addr, size_t len, int prot) {
#ifdef __EMSCRIPTEN__
    static_cast<void>(prot);
    GuestArena::GuestArenaCommit_nid_postfix(addr, len, 0x04, PS5_PAGE_SIZE);
    return addr;
#else
    return mmap(addr, len, prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
#endif
}

void ArenaForget(void* addr, size_t len) {
    if (GuestArena::GuestArenaContains_nid_postfix(addr, len)) GuestArena::GuestArenaRelease_nid_postfix(addr, len);
}
#endif

void Unmap(void* addr, size_t len) {
#if defined(__linux__) || defined(__EMSCRIPTEN__)
    if (ArenaAvailable() && GuestArena::GuestArenaContains_nid_postfix(addr, len))
        GuestArena::GuestArenaReset_nid_postfix(addr, len);
    else if (munmap(addr, len) != 0) throw std::system_error(errno, std::generic_category(), "munmap failed");
    ArenaForget(addr, len);
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
#if defined(__linux__) || defined(__EMSCRIPTEN__)
        if (ArenaAvailable() && !GuestArena::GuestArenaContains_nid_postfix(addr, len))
            throw std::invalid_argument("fixed mapping is outside the guest address space arena");
        if (ArenaAvailable()) {
            GuestArena::GuestArenaMarkUsed_nid_postfix(addr, len);
            if (void* placed = ArenaCommit(addr, len, prot); placed != MAP_FAILED) return placed;
            GuestArena::GuestArenaRelease_nid_postfix(addr, len);
            throw std::system_error(errno, std::generic_category(), "Arena mapping failed");
        }
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
#if defined(__linux__) || defined(__EMSCRIPTEN__)
        if (ArenaAvailable()) {
            void* placed = GuestArena::GuestArenaAllocateFromHint_nid_postfix(
                reinterpret_cast<std::uintptr_t>(addr), len, alignment);
            if (void* committed = ArenaCommit(placed, len, prot); committed != MAP_FAILED) return committed;
            ArenaForget(placed, len);
            throw std::system_error(errno, std::generic_category(), "Arena mapping failed");
        }
#ifdef __EMSCRIPTEN__
        throw std::runtime_error("the guest address space arena is unavailable");
#else
        return MapAtOrAbove(reinterpret_cast<std::uintptr_t>(addr), len, prot, alignment);
#endif
#else
        return mmap_aligned(len, prot, alignment, reinterpret_cast<std::uintptr_t>(addr));
#endif
    }
#ifdef _WIN32
    return mmap_aligned(len, prot, alignment);
#endif
#if defined(__linux__) || defined(__EMSCRIPTEN__)
    if (ArenaAvailable()) {
        void* placed = GuestArena::GuestArenaAllocate_nid_postfix(len, alignment);
        if (void* committed = ArenaCommit(placed, len, prot); committed != MAP_FAILED) return committed;
        ArenaForget(placed, len);
        throw std::system_error(errno, std::generic_category(), "Arena mapping failed");
    }
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

void EraseProtections(std::uintptr_t start, std::uintptr_t end, std::map<std::uintptr_t, ProtectedRange>& protections = g_protections) {
    auto it = protections.lower_bound(start);
    if (it != protections.begin() && std::prev(it)->second.end > start) --it;
    while (it != protections.end() && it->first < end) {
        const auto rangeStart = it->first;
        const auto range = it->second;
        it = protections.erase(it);
        if (rangeStart < start) protections.emplace(rangeStart, ProtectedRange{start, range.prot});
        if (range.end > end) it = protections.emplace(end, ProtectedRange{range.end, range.prot}).first;
    }
}

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

int ReportedProtection(int prot) {
    if ((prot & 0x2) != 0) prot |= 0x1;
    if ((prot & 0x80) != 0) prot |= 0x40;
    if ((prot & 0x200) != 0) prot |= 0x100;
    return prot;
}

void RecordProtection(const void* addr, size_t len, int prot) {
    const auto start = reinterpret_cast<std::uintptr_t>(addr);
    std::lock_guard lock(g_protectionLock);
    EraseProtections(start, start + len);
    if (prot >= 0) g_protections.emplace(start, ProtectedRange{start + len, ReportedProtection(prot)});
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
    RequireResidentUnpinned(mutation, reinterpret_cast<std::uintptr_t>(addr), len);
    mutation.Unmap(addr, len, [&](const void* piece, std::size_t pieceBytes, const void* allocation, bool last) {
        auto* pieceAddress = const_cast<void*>(piece);
        std::lock_guard lock(g_directLock);
        EraseMappings(reinterpret_cast<std::uintptr_t>(piece), reinterpret_cast<std::uintptr_t>(piece) + pieceBytes);
#if defined(__linux__) || defined(__EMSCRIPTEN__)
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

int PlaceHintInReservation(void* addr, size_t len, int flags) {
    if (addr == nullptr || (flags & GuestMapFixedFlag) != 0 || !Reserved(addr, len)) return flags;
    return flags | GuestMapFixedFlag;
}

int DoMapDirect(void** addr, size_t len, int prot, int flags, int64_t physStart, size_t alignment) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
    if (physStart < 0 || (static_cast<std::uint64_t>(physStart) & (PS5_PAGE_SIZE - 1)) != 0 || static_cast<std::uint64_t>(physStart) >= DIRECT_MEMORY_SIZE || len > DIRECT_MEMORY_SIZE - static_cast<std::uint64_t>(physStart)) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    if ((prot & ~0x3F7) == 0 && (prot & GuestProtCpuExecute) != 0) return SCE_KERNEL_ERROR_EINVAL;
    {
        std::lock_guard lock(g_directLock);
        if (FirstUnallocatedPage(static_cast<std::uint64_t>(physStart), len)) return SCE_KERNEL_ERROR_ENOMEM;
    }
    GuestAllocations::Mutation mutation;
    flags = PlaceHintInReservation(*addr, len, flags);
    if (FixedNoOverwriteConflict(mutation, *addr, len, flags)) return SCE_KERNEL_ERROR_ENOMEM;
    if (RemapFixedIntoRegistered(mutation, *addr, len, prot, flags, physStart)) {
        EraseReservations(*addr, len);
        RecordProtection(*addr, len, prot);
        return 0;
    }
    ReplaceFixedOverlap(mutation, *addr, len, flags);
    RequireAliasesUnpinned(mutation, static_cast<std::uint64_t>(physStart), len);
    std::lock_guard lock(g_directLock);
    ValidatePhysicalRange(static_cast<std::uint64_t>(physStart), len);
    void* mapped = MapAligned(*addr, len, PROT_NONE, flags, alignment);
    try {
        AddMapping(reinterpret_cast<std::uintptr_t>(mapped), len, static_cast<std::uint64_t>(physStart), LinuxProtFromSce(prot));
        mutation.Add(mapped, len, (prot & 3) != 0, (prot & 2) != 0, (prot & GuestProtGpuReadWrite) != 0);
        if ((prot & GuestProtGpuReadWrite) != 0) mutation.NoteGpuMapping(mapped, len);
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

int DoMapAnon(void** addr, size_t len, int prot, int flags, size_t alignment, bool exclusive, [[maybe_unused]] bool allowStackReplacement) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0) return SCE_KERNEL_ERROR_EINVAL;
#ifdef _WIN32
    if (allowStackReplacement && (flags & GuestMapFixedFlag) != 0 && !KernelArena::Get().Owns(*addr, len)) {
        constexpr int GuestMapNoOverwrite = 0x80;
        if ((prot & ~7) != 0 || (flags & ~(GuestMapFixedFlag | GuestMapNoOverwrite)) != 0)
            return SCE_KERNEL_ERROR_EINVAL;
        ValidateRange(*addr, len, ValidateAlignment(alignment));
        const auto start = reinterpret_cast<std::uintptr_t>(*addr);
        int result = SCE_KERNEL_ERROR_EINVAL;
        ReplaceGuestThreadStack(start, len, (prot & 3) == 3, [&] {
            if (exclusive || (flags & GuestMapNoOverwrite) != 0) {
                result = SCE_KERNEL_ERROR_ENOMEM;
                return false;
            }
            void* allocation = nullptr;
            for (auto cursor = start; cursor < start + len;) {
                MEMORY_BASIC_INFORMATION memory{};
                if (VirtualQuery(reinterpret_cast<void*>(cursor), &memory, sizeof(memory)) != sizeof(memory) ||
                    (memory.State != MEM_COMMIT && memory.State != MEM_RESERVE) || memory.Type != MEM_PRIVATE ||
                    (memory.Protect & PAGE_GUARD) != 0 ||
                    memory.RegionSize == 0 || reinterpret_cast<std::uintptr_t>(memory.BaseAddress) > cursor ||
                    memory.RegionSize > std::numeric_limits<std::uintptr_t>::max() - reinterpret_cast<std::uintptr_t>(memory.BaseAddress))
                    return false;
                if (allocation == nullptr) allocation = memory.AllocationBase;
                if (memory.AllocationBase != allocation) return false;
                const auto next = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
                if (next <= cursor) return false;
                cursor = std::min(start + len, next);
            }
            std::lock_guard lock(g_protectionLock);
            auto replacement = g_protections;
            EraseProtections(start, start + len, replacement);
            const auto recorded = replacement.emplace(start, ProtectedRange{start + len, ReportedProtection(prot)}).first;
            result = SCE_KERNEL_ERROR_ENOMEM;
            if (!VirtualFree(*addr, len, MEM_DECOMMIT)) return false;
            if (VirtualAlloc(*addr, len, MEM_COMMIT, WinProtFromPosix(LinuxProtFromSce(prot))) != *addr) {
                const auto error = GetLastError();
                recorded->second.prot = 0;
                g_protections.swap(replacement);
                throw std::system_error(static_cast<int>(error), std::system_category(), "Guest stack replacement commit failed");
            }
            g_protections.swap(replacement);
            result = 0;
            return true;
        });
        return result;
    }
#endif
    GuestAllocations::Mutation mutation;
    if (exclusive && mutation.Overlaps(*addr, len)) return SCE_KERNEL_ERROR_ENOMEM;
    flags = PlaceHintInReservation(*addr, len, flags);
    if (FixedNoOverwriteConflict(mutation, *addr, len, flags)) return SCE_KERNEL_ERROR_ENOMEM;
    if (RemapFixedIntoRegistered(mutation, *addr, len, prot, flags)) {
        EraseReservations(*addr, len);
        RecordProtection(*addr, len, prot);
        return 0;
    }
    ReplaceFixedOverlap(mutation, *addr, len, flags);
    void* mapped = MapAligned(*addr, len, LinuxProtFromSce(prot), flags, alignment);
    try {
        mutation.Add(mapped, len, (prot & 3) != 0, (prot & 2) != 0, (prot & GuestProtGpuReadWrite) != 0);
        if ((prot & GuestProtGpuReadWrite) != 0) mutation.NoteGpuMapping(mapped, len);
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

int DoMapFileBacked_nid_no_patch(void** addr, size_t len, int prot, int flags, int descriptor, std::uint64_t offset, size_t alignment) {
    ValidateOutput(addr);
    if (len == 0 || (len & (PS5_PAGE_SIZE - 1)) != 0 || (offset & (PS5_PAGE_SIZE - 1)) != 0 || len > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) || offset > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) - len) return SCE_KERNEL_ERROR_EINVAL;
#if defined(__linux__) || defined(__APPLE__)
    const bool shared = (flags & 3) == 1;
    GuestAllocations::Mutation mutation;
    const int placementFlags = flags & (GuestMapFixedFlag | 0x80 | 0x400000);
    flags = PlaceHintInReservation(*addr, len, placementFlags);
    if (FixedNoOverwriteConflict(mutation, *addr, len, flags)) return SCE_KERNEL_ERROR_ENOMEM;
    ReplaceFixedOverlap(mutation, *addr, len, flags);
    void* mapped = MapPlaced(*addr, len, PROT_NONE, flags, alignment);
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(mapped, len);
    const int nativeProtection = LinuxProtFromSce(prot);
    const int nativeFlags = (shared ? MAP_SHARED : MAP_PRIVATE) | MAP_FIXED;
#ifdef __APPLE__
    const int mappedProtection = nativeProtection & ~PROT_EXEC;
#else
    const int mappedProtection = nativeProtection;
#endif
    if (::mmap(mapped, len, mappedProtection, nativeFlags, descriptor, static_cast<off_t>(offset)) == MAP_FAILED) {
        const int error = errno;
        Unmap(mapped, len);
        throw std::system_error(error, std::generic_category(), "map guest shared memory");
    }
    if (mappedProtection != nativeProtection && ::mprotect(mapped, len, nativeProtection) != 0) {
        const int error = errno;
        Unmap(mapped, len);
        throw std::system_error(error, std::generic_category(), "make a guest file mapping executable");
    }
    try {
        mutation.Add(mapped, len, (prot & 3) != 0, (prot & 2) != 0, false);
    } catch (...) {
        Unmap(mapped, len);
        throw;
    }
    *addr = mapped;
    EraseReservations(mapped, len);
    RecordProtection(mapped, len, prot);
    Trace("map file %p+0x%zx offset=0x%llx prot=0x%x flags=0x%x", mapped, len, static_cast<unsigned long long>(offset), prot, flags);
    return 0;
#elif defined(__EMSCRIPTEN__)
    static_cast<void>(prot);
    static_cast<void>(flags);
    static_cast<void>(descriptor);
    static_cast<void>(offset);
    static_cast<void>(alignment);
    throw std::system_error(ENOTSUP, std::generic_category(), "file-backed guest mmap is not supported in WebAssembly");
#else
    if ((flags & 3) != 1) throw std::system_error(ENOTSUP, std::generic_category(), "map file: a private file mapping is not mapped from the file on Windows");
    const HANDLE file = WindowsDescriptorHandle_nid_no_patch(descriptor);
    LARGE_INTEGER fileBytes{};
    if (!GetFileSizeEx(file, &fileBytes)) throw std::system_error(EBADF, std::generic_category(), "map file: read the file size");
    const auto fileEnd = static_cast<std::uint64_t>(fileBytes.QuadPart);
    if (offset + len >= fileEnd + PS5_PAGE_SIZE) throw std::system_error(ENOTSUP, std::generic_category(), "map file: a shared mapping of a whole page past the end of the file is not implemented on Windows");
    const int access = WindowsDescriptorAccess_nid_no_patch(file, descriptor, "mmap");
    if (access == 1) throw std::system_error(ENOTSUP, std::generic_category(), "map file: a shared mapping of a write-only descriptor is not implemented on Windows");
    if (access == 0 && (prot & PROT_WRITE) != 0) throw std::system_error(EACCES, std::generic_category(), "map file: a writable shared mapping of a read-only descriptor");
    const bool execute = (prot & PROT_EXEC) != 0;
    const DWORD viewProtection = execute ? (access == 2 ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ) : (access == 2 ? PAGE_READWRITE : PAGE_READONLY);
    const HANDLE source = execute ? ReOpenFile(file, GENERIC_READ | GENERIC_EXECUTE | (access == 2 ? GENERIC_WRITE : 0), FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0) : file;
    if (source == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        throw std::system_error(error == ERROR_ACCESS_DENIED ? EACCES : EIO, std::generic_category(), "map file: reopen the file with execute access");
    }
    const HANDLE section = CreateFileMappingW(source, nullptr, viewProtection, 0, 0, nullptr);
    const DWORD sectionError = GetLastError();
    if (source != file) CloseHandle(source);
    if (section == nullptr) throw std::system_error(sectionError == ERROR_ACCESS_DENIED ? EACCES : EIO, std::generic_category(), "map file: create the file section");
    GuestAllocations::Mutation mutation;
    const int placementFlags = flags & (GuestMapFixedFlag | 0x80 | 0x400000);
    flags = PlaceHintInReservation(*addr, len, placementFlags);
    void* mapped = nullptr;
    try {
        if (FixedNoOverwriteConflict(mutation, *addr, len, flags)) {
            CloseHandle(section);
            return SCE_KERNEL_ERROR_ENOMEM;
        }
        ReplaceFixedOverlap(mutation, *addr, len, flags);
        mapped = MapPlaced(*addr, len, PROT_NONE, flags, alignment);
        GuestArena::GuestArenaMapFile_nid_postfix(mapped, len, section, offset, WinProtFromPosix(LinuxProtFromSce(prot)), viewProtection, static_cast<std::size_t>(std::min<std::uint64_t>(len, fileEnd - offset)));
        mutation.Add(mapped, len, (prot & 3) != 0, (prot & 2) != 0, false);
    } catch (...) {
        if (mapped != nullptr) Unmap(mapped, len);
        GuestArena::GuestArenaForgetSection_nid_postfix(section);
        CloseHandle(section);
        throw;
    }
    GuestArena::GuestArenaForgetSection_nid_postfix(section);
    CloseHandle(section);
    *addr = mapped;
    EraseReservations(mapped, len);
    RecordProtection(mapped, len, prot);
    Trace("map file %p+0x%zx offset=0x%llx prot=0x%x flags=0x%x", mapped, len, static_cast<unsigned long long>(offset), prot, flags);
    return 0;
#endif
}

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
struct GuestImageSearch {
    std::uintptr_t address = 0;
    std::uintptr_t base = 0;
    std::uintptr_t pageSize = 0;
    bool guestModule = false;
    bool first = true;
    bool main = false;
    bool found = false;
};

static int FindImageContaining(dl_phdr_info* image, std::size_t, void* data) {
    auto& search = *static_cast<GuestImageSearch*>(data);
    const bool main = search.first;
    search.first = false;
    for (int index = 0; index < image->dlpi_phnum; ++index) {
        const auto& header = image->dlpi_phdr[index];
        if (header.p_type != PT_LOAD) continue;
        const auto start = (image->dlpi_addr + header.p_vaddr) & ~(search.pageSize - 1);
        const auto end = (image->dlpi_addr + header.p_vaddr + header.p_memsz + search.pageSize - 1) & ~(search.pageSize - 1);
        if (search.address < start || search.address >= end) continue;
        search.base = image->dlpi_addr;
        search.guestModule = image->dlpi_name != nullptr && std::string_view(image->dlpi_name).ends_with(".guest.prx");
        search.main = main;
        search.found = true;
        return 1;
    }
    return 0;
}
#endif

int DoMprotect(const void* addr, size_t len, int prot) {
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
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(pointer, &memory, sizeof(memory)) != sizeof(memory)) throw std::runtime_error("Cannot query guest memory protection range");
    if (memory.Type == MEM_IMAGE) {
        if (memory.AllocationBase == GetModuleHandleW(nullptr)) mutation.RegisterMainImage();
        else if (IsGuestModuleImage(memory.AllocationBase)) mutation.RegisterImage(memory.AllocationBase);
        else throw std::invalid_argument("Memory protection of a foreign image is not supported");
    }
    for (auto cursor = first; cursor < end;) {
        MEMORY_BASIC_INFORMATION view{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &view, sizeof(view)) != sizeof(view)) throw std::runtime_error("Cannot query guest memory protection range");
        if (view.Type == MEM_MAPPED && (nativeProtection & ~PosixRightsFromWin(view.AllocationProtect)) != 0)
            throw std::system_error(EACCES, std::generic_category(), "mprotect above the maximum protection of a mapped view");
        cursor = reinterpret_cast<std::uintptr_t>(view.BaseAddress) + view.RegionSize;
    }
#else
    mutation.RegisterMainImage();
    GuestImageSearch image{.address = address, .pageSize = static_cast<std::uintptr_t>(::sysconf(_SC_PAGESIZE))};
    dl_iterate_phdr(FindImageContaining, &image);
    if (image.found && !image.main) {
        if (!image.guestModule) throw std::invalid_argument("Memory protection of a foreign image is not supported");
        mutation.RegisterImage(reinterpret_cast<const void*>(image.base));
    }
    mutation.TrimToRegistered(PS5_PAGE_SIZE, pointer, bytes);
#endif
    std::vector<std::pair<const void*, std::size_t>> spans;
    mutation.ForRegistered(pointer, bytes, [&](const void* span, std::size_t spanBytes) { spans.emplace_back(span, spanBytes); });
    if (spans.empty() || spans.front().first != pointer) return SCE_KERNEL_ERROR_EINVAL;
    for (const auto& [span, spanBytes] : spans) mutation.RequireUnpinned(span, spanBytes);
    for (const auto& [span, spanBytes] : spans) {
        mutation.Protect(span, spanBytes, (prot & 3) != 0, (prot & 2) != 0, (prot & GuestProtGpuReadWrite) != 0, [&] {
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

int DoMtypeprotect(const void* addr, size_t len, int type, int prot) {
    const int result = DoMprotect(addr, len, prot);
    if (result != 0) return result;
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
            it = RemoveMapping(it);
            if (base < first) InsertMapping(base, DirectMapping{first, mapping.phys, mapping.memoryType, mapping.nativeProtection, mapping.backing});
            const auto low = std::max(base, first);
            const auto high = std::min(mapping.end, end);
            InsertMapping(low, DirectMapping{high, mapping.phys + low - base, type, mapping.nativeProtection, mapping.backing});
            physical.emplace_back(mapping.phys + low - base, high - low);
            if (mapping.end > end) it = InsertMapping(end, DirectMapping{mapping.end, mapping.phys + end - base, mapping.memoryType, mapping.nativeProtection, mapping.backing});
        }
    }
    for (const auto& [phys, bytes] : physical) DirectMemoryRetype(static_cast<int64_t>(phys), bytes, type);
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
    void* mapped = MapAligned(*addr, len, PROT_NONE, fixed ? GuestMapFixedFlag | (flags & GuestMapNoCoalesce) : 0, alignment);
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

void CreateDirectMemoryBacking(int64_t start, size_t len, int memoryType) {
    std::lock_guard lock(g_directLock);
    const auto first = static_cast<std::uint64_t>(start);
    const auto last = first + len;
    if (auto block = g_physBlocks.upper_bound(first); (block != g_physBlocks.end() && block->first < last) || (block != g_physBlocks.begin() && std::prev(block)->second.end > first)) throw std::runtime_error("physical allocation overlaps live direct memory");
#if defined(__linux__)
    static std::once_flag registered;
    std::call_once(registered, [] {
        GuestArena::GuestArenaSetSharedBacking_nid_postfix(&SharedBacking);
        GuestArena::GuestArenaSetSharedBackingWriter_nid_no_patch(&WriteSharedBacking);
    });
#endif
    const auto backing = std::make_shared<PhysicalBacking>(len, memoryType, first);
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

std::vector<DirectMemoryView> DirectMemoryViews(int64_t start, size_t len) {
    const auto first = static_cast<std::uint64_t>(start);
    const auto last = first + len;
    std::vector<DirectMemoryView> views;
    std::lock_guard lock(g_directLock);
    for (const auto base : MappingsSharingPhys(first, last)) {
        const auto& mapping = g_directMappings.at(base);
        const auto mappingLast = mapping.phys + (mapping.end - base);
        if (mapping.phys >= last || mappingLast <= first) continue;
        const auto viewStart = base + (std::max(first, mapping.phys) - mapping.phys);
        const auto viewEnd = base + (std::min(last, mappingLast) - mapping.phys);
        if (!views.empty() && views.back().address + views.back().bytes == viewStart) views.back().bytes += viewEnd - viewStart;
        else views.push_back({viewStart, viewEnd - viewStart});
    }
    return views;
}

void UnmapDirectMemoryViews(const std::vector<DirectMemoryView>& views) {
    if (views.empty()) return;
    GuestAllocations::Mutation mutation;
    for (const auto& view : views) {
        Trace("unmap released view %p+0x%zx", reinterpret_cast<void*>(view.address), view.bytes);
        UnmapRegistered(mutation, reinterpret_cast<void*>(view.address), view.bytes);
    }
}
