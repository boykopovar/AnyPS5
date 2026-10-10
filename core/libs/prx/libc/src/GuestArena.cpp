#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/WindowsMappings.hpp"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iterator>
#include <map>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace GuestArena {
namespace {

#ifdef _WIN32
std::atomic<std::uint64_t> commitGeneration{1};
std::atomic<void (*)(std::uintptr_t, std::size_t, std::uint64_t)> privateMappingObserver{nullptr};
#endif

constexpr std::uintptr_t ArenaStart = 0x0000000200000000ull;
constexpr std::uintptr_t SystemReservedStart = 0x00000007FFFFC000ull;
constexpr std::uintptr_t SystemReservedEnd = 0x0000001000000000ull;
constexpr std::uintptr_t ApplicationAreaEnd = 0x000000FC00000000ull;

std::uintptr_t alignUp(std::uintptr_t value, std::size_t alignment) {
    return (value + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
}

class Arena {
public:
    static Arena& Get() {
        static Arena arena;
        return arena;
    }

    bool Available() const {
        return _base != 0;
    }

    bool Contains(const void* pointer, std::size_t bytes) const {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        return _base != 0 && address >= _base && bytes <= _end - _base && address - _base <= (_end - _base) - bytes;
    }

    void* Allocate(std::size_t bytes, std::size_t alignment) {
        return AllocateAtOrAbove(0, bytes, alignment);
    }

    void* AllocateAtOrAbove(std::uintptr_t hint, std::size_t bytes, std::size_t alignment) {
        if (_base == 0) throw std::runtime_error("guest address space arena is unavailable");
        if (alignment == 0 || (alignment & (alignment - 1)) != 0) throw std::invalid_argument("invalid guest arena alignment");
        std::lock_guard lock(_lock);
        if (hint >= _end) throw std::runtime_error("mapping address hint is above the guest address space arena");
        std::uintptr_t candidate = alignUp(std::max(_base, hint), alignment);
        auto it = _used.upper_bound(candidate);
        if (it != _used.begin() && std::prev(it)->second > candidate) --it;
        for (; it != _used.end(); ++it) {
            const auto& [start, end] = *it;
            if (candidate <= _end && bytes <= _end - candidate && candidate + bytes <= start) break;
            candidate = std::max(candidate, alignUp(end, alignment));
        }
        if (candidate > _end || bytes > _end - candidate) {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "guest address space arena exhausted: requested 0x%zx bytes (align 0x%zx, hint 0x%llx), candidate 0x%llx, end 0x%llx, used_count %zu",
                          bytes, alignment, static_cast<unsigned long long>(hint), static_cast<unsigned long long>(candidate), static_cast<unsigned long long>(_end), _used.size());
            throw std::runtime_error(buf);
        }
        _used.emplace(candidate, candidate + bytes);
        return reinterpret_cast<void*>(candidate);
    }

    void MarkUsed(const void* pointer, std::size_t bytes) {
        std::lock_guard lock(_lock);
        const auto start = reinterpret_cast<std::uintptr_t>(pointer);
        const auto next = _used.lower_bound(start);
        if ((next != _used.end() && next->first < start + bytes) || (next != _used.begin() && std::prev(next)->second > start)) {
            char message[160];
            std::snprintf(message, sizeof(message), "fixed guest mapping 0x%llx+0x%zx overlaps %s", static_cast<unsigned long long>(start), bytes, OverlapsHostRegion(start, bytes) ? "a host region in the guest arena" : "a guest arena range");
            throw std::runtime_error(message);
        }
        _used.emplace(start, start + bytes);
    }

    void Release(const void* pointer, std::size_t bytes) {
        std::lock_guard lock(_lock);
        const auto start = reinterpret_cast<std::uintptr_t>(pointer);
        const auto end = start + bytes;
        auto it = _used.upper_bound(start);
        if (it != _used.begin()) --it;
        while (it != _used.end() && it->first < end) {
            const auto rangeStart = it->first;
            const auto rangeEnd = it->second;
            if (rangeEnd <= start) {
                ++it;
                continue;
            }
            it = _used.erase(it);
            if (rangeStart < start) _used.emplace(rangeStart, start);
            if (rangeEnd > end) _used.emplace(end, rangeEnd);
        }
        for (const auto& [holeStart, holeEnd] : _holes) {
            if (holeStart < end && start < holeEnd) _used.emplace(holeStart, holeEnd);
        }
    }

private:
    Arena() {
#ifdef _WIN32
        _writeWatched = std::getenv("APS5_NO_WRITE_WATCH") == nullptr;
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        const std::uintptr_t granularity = system.dwAllocationGranularity;
        const std::uintptr_t end = ApplicationAreaEnd;
        std::uint32_t retries = 0;
        for (std::uintptr_t cursor = ArenaStart; cursor < end;) {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "query the guest arena range");
            const auto regionEnd = std::min(end, reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize);
            const auto first = info.State == MEM_FREE ? std::min(regionEnd, alignUp(cursor, granularity)) : regionEnd;
            const auto last = std::max(first, regionEnd & ~(granularity - 1));
            if (first < last && WindowsMappings::Get().Reserve(reinterpret_cast<void*>(first), last - first) == nullptr) {
                const auto error = GetLastError();
                if (error == ERROR_INVALID_ADDRESS && ++retries < 64u) continue;
                throw std::system_error(static_cast<int>(error), std::system_category(), "reserve the guest arena range");
            }
            retries = 0;
            if (cursor < first) _holes.emplace_back(cursor, first);
            if (last < regionEnd) _holes.emplace_back(last, regionEnd);
            cursor = regionEnd;
        }
        _hostRegions = _holes;
        _holes.emplace_back(SystemReservedStart, SystemReservedEnd);
        for (const auto& [holeStart, holeEnd] : _holes) _used.emplace(holeStart, holeEnd);
        _base = ArenaStart;
        _end = end;
#endif
    }

    std::mutex _lock;
    std::map<std::uintptr_t, std::uintptr_t> _used;
    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> _holes;
    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> _hostRegions;
    std::uintptr_t _base = 0;
    std::uintptr_t _end = 0;
    bool _writeWatched = false;

public:
    std::uintptr_t Base() const { return _base; }
    std::size_t Size() const { return _end - _base; }
    bool WriteWatched() const { return _writeWatched; }
    bool OverlapsHostRegion(std::uintptr_t start, std::size_t bytes) const {
        for (const auto& [regionStart, regionEnd] : _hostRegions) {
            if (regionStart < start + bytes && start < regionEnd) return true;
        }
        return false;
    }
};

const bool g_reserved = (Arena::Get(), true);

}

bool GuestArenaAvailable_nid_postfix() {
    return Arena::Get().Available();
}

bool GuestArenaContains_nid_postfix(const void* pointer, std::size_t bytes) {
    return Arena::Get().Contains(pointer, bytes);
}

void* GuestArenaAllocate_nid_postfix(std::size_t bytes, std::size_t alignment) {
    return Arena::Get().Allocate(bytes, alignment);
}

void* GuestArenaAllocateAtOrAbove_nid_postfix(std::uintptr_t hint, std::size_t bytes, std::size_t alignment) {
    return Arena::Get().AllocateAtOrAbove(hint, bytes, alignment);
}

void GuestArenaMarkUsed_nid_postfix(const void* pointer, std::size_t bytes) {
    Arena::Get().MarkUsed(pointer, bytes);
}

void GuestArenaRelease_nid_postfix(const void* pointer, std::size_t bytes) {
    Arena::Get().Release(pointer, bytes);
}

void GuestArenaRange_nid_postfix(std::uintptr_t* base, std::size_t* bytes) {
    *base = Arena::Get().Base();
    *bytes = Arena::Get().Size();
}

#ifdef _WIN32
void GuestArenaSetProtection_nid_postfix(std::uintptr_t address, std::size_t bytes, std::uint32_t protection) {
    WindowsMappings::Get().SetProtection(address, bytes, protection);
}

bool GuestArenaHandleWrite_nid_postfix(std::uintptr_t address) {
    return WindowsMappings::Get().HandleWrite(address);
}

void GuestArenaPinWritable_nid_postfix(const void* pointer, std::size_t bytes) {
    if (!Arena::Get().Contains(pointer, bytes)) return;
    WindowsMappings::Get().Pin(reinterpret_cast<std::uintptr_t>(pointer), bytes);
}

void GuestArenaUnpinWritable_nid_postfix(const void* pointer, std::size_t bytes) {
    if (!Arena::Get().Contains(pointer, bytes)) return;
    WindowsMappings::Get().Unpin(reinterpret_cast<std::uintptr_t>(pointer), bytes);
}

bool GuestArenaProtection_nid_postfix(std::uintptr_t address, std::uint32_t* protection) {
    return WindowsMappings::Get().Protection(address, protection);
}

bool GuestArenaCollectWrites_nid_postfix(std::uintptr_t address, std::size_t bytes, void** pages, std::size_t* count, bool clear) {
    return WindowsMappings::Get().Collect(address, bytes, pages, count, clear);
}

bool GuestArenaHostRegionOverlaps_nid_postfix(std::uintptr_t address, std::size_t bytes) {
    return Arena::Get().OverlapsHostRegion(address, bytes);
}

namespace {

std::invalid_argument OutsideArena(const char* operation, const void* pointer, std::size_t bytes) {
    char message[128];
    std::snprintf(message, sizeof(message), "%s 0x%llx+0x%zx outside the guest arena", operation, static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(pointer)), bytes);
    return std::invalid_argument(message);
}

}

std::uint64_t GuestArenaCommitGeneration_nid_postfix() {
    return commitGeneration.load(std::memory_order_acquire);
}

void GuestArenaSetPrivateMappingObserver_nid_postfix(void (*callback)(std::uintptr_t, std::size_t, std::uint64_t)) {
    privateMappingObserver.store(callback, std::memory_order_release);
}

void GuestArenaCommit_nid_postfix(void* pointer, std::size_t bytes, std::uint32_t protection, std::size_t granule) {
    const auto inArena = Arena::Get().Contains(pointer, bytes);
    const auto generation = inArena ? commitGeneration.fetch_add(1, std::memory_order_acq_rel) + 1 : 0;
    const auto created = WindowsMappings::Get().Commit(pointer, bytes, protection, granule, inArena && Arena::Get().WriteWatched());
    if (inArena) {
        if (const auto callback = privateMappingObserver.load(std::memory_order_acquire)) {
            for (const auto& [address, size] : created) callback(address, size, generation);
        }
    }
}

void GuestArenaReset_nid_postfix(void* pointer, std::size_t bytes) {
    WindowsMappings::Get().Reset(pointer, bytes);
}

void GuestArenaMap_nid_postfix(void* pointer, std::size_t bytes, void* section, std::uint64_t offset, std::uint32_t protection) {
    WindowsMappings::Get().Map(pointer, bytes, section, offset, protection);
}

void* GuestArenaMapAlias_nid_postfix(std::uintptr_t address, std::size_t bytes) {
    return WindowsMappings::Get().MapAlias(address, bytes);
}

void GuestArenaUnmapAlias_nid_postfix(void* alias) {
    WindowsMappings::Get().UnmapAlias(alias);
}
#endif

bool GuestArenaWriteWatched_nid_postfix() {
    return Arena::Get().WriteWatched();
}

bool GuestArenaBeginHostWrite_nid_postfix(void* pointer, std::size_t bytes) {
#ifdef _WIN32
    return WindowsMappings::Get().BeginHostWrite(reinterpret_cast<std::uintptr_t>(pointer), bytes);
#else
    (void)pointer;
    (void)bytes;
    return true;
#endif
}

void GuestArenaEndHostWrite_nid_postfix(void* pointer, std::size_t bytes) {
#ifdef _WIN32
    WindowsMappings::Get().EndHostWrite(reinterpret_cast<std::uintptr_t>(pointer), bytes);
#else
    (void)pointer;
    (void)bytes;
#endif
}

#ifndef _WIN32
namespace {

std::atomic<SharedBackingResolver> sharedBackingResolver{nullptr};
std::atomic<SharedBackingWriter> sharedBackingWriter{nullptr};

}

void GuestArenaSetSharedBacking_nid_postfix(SharedBackingResolver resolver) {
    sharedBackingResolver.store(resolver, std::memory_order_release);
}

void GuestArenaSetSharedBackingWriter_nid_no_patch(SharedBackingWriter writer) {
    sharedBackingWriter.store(writer, std::memory_order_release);
}

bool GuestArenaWriteSharedBacking_nid_no_patch(std::uintptr_t address, const void* source, std::size_t bytes) {
    const auto writer = sharedBackingWriter.load(std::memory_order_acquire);
    return writer != nullptr && writer(address, source, bytes);
}

bool GuestArenaSharedBacking_nid_postfix(std::uintptr_t address, std::size_t bytes, int* file, std::uint64_t* offset) {
    const auto resolver = sharedBackingResolver.load(std::memory_order_acquire);
    return resolver != nullptr && resolver(address, bytes, file, offset);
}
#endif

}
