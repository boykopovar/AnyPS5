#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <map>
#include <mutex>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace GuestArena {
namespace {

constexpr std::uintptr_t PreferredBase = 0x0000001000000000ull;
constexpr std::size_t MaximumSize = 0x0000007000000000ull;
constexpr std::size_t MinimumSize = 0x0000001000000000ull;
constexpr std::uintptr_t MapAreaEnd = 0x000000FC00000000ull;

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
        if (_base == 0) throw std::runtime_error("guest address space arena is unavailable");
        if (alignment == 0 || (alignment & (alignment - 1)) != 0) throw std::invalid_argument("invalid guest arena alignment");
        std::lock_guard lock(_lock);
        std::uintptr_t candidate = alignUp(_base, alignment);
        for (const auto& [start, end] : _used) {
            if (candidate + bytes <= start) break;
            candidate = std::max(candidate, alignUp(end, alignment));
        }
        if (bytes > _end - candidate) throw std::runtime_error("guest address space arena exhausted");
        _used.emplace(candidate, candidate + bytes);
        return reinterpret_cast<void*>(candidate);
    }

    void MarkUsed(const void* pointer, std::size_t bytes) {
        std::lock_guard lock(_lock);
        const auto start = reinterpret_cast<std::uintptr_t>(pointer);
        const auto next = _used.lower_bound(start);
        if ((next != _used.end() && next->first < start + bytes) || (next != _used.begin() && std::prev(next)->second > start))
            throw std::runtime_error("fixed guest mapping overlaps a guest arena range");
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
    }

private:
    Arena() {
#ifdef _WIN32
        // Windows places other reservations randomly, so take the lowest base and largest size that fit.
        for (std::uintptr_t base = PreferredBase; base + MinimumSize <= MapAreaEnd; base += MinimumSize) {
            for (std::size_t size = MaximumSize; size >= MinimumSize; size /= 2) {
                if (base + size > MapAreaEnd) continue;
                // Write watching lets the GPU driver learn which pages the CPU wrote instead of comparing
                // whole resources; the plain reservation is the fallback. Debug aid: APS5_NO_WRITE_WATCH=1
                // skips it, to measure what the write faults it re-arms cost the game's threads.
                static const bool noWriteWatch = std::getenv("APS5_NO_WRITE_WATCH") != nullptr;
                void* reserved = noWriteWatch ? nullptr : VirtualAlloc(reinterpret_cast<void*>(base), size, MEM_RESERVE | MEM_WRITE_WATCH, PAGE_NOACCESS);
                _writeWatched = reserved != nullptr;
                if (!reserved) reserved = VirtualAlloc(reinterpret_cast<void*>(base), size, MEM_RESERVE, PAGE_NOACCESS);
                if (!reserved) continue;
                _base = reinterpret_cast<std::uintptr_t>(reserved);
                _end = _base + size;
                return;
            }
        }
        std::fprintf(stderr, "[memory] guest arena unavailable below 0x%llx\n", static_cast<unsigned long long>(MapAreaEnd));
#endif
    }

    std::mutex _lock;
    std::map<std::uintptr_t, std::uintptr_t> _used;
    std::uintptr_t _base = 0;
    std::uintptr_t _end = 0;
    bool _writeWatched = false;

public:
    std::uintptr_t Base() const { return _base; }
    std::size_t Size() const { return _end - _base; }
    bool WriteWatched() const { return _writeWatched; }
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

bool GuestArenaWriteWatched_nid_postfix() {
    return Arena::Get().WriteWatched();
}

}
