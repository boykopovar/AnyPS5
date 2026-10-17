#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/MallocManagedSize.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <list>
#include <mutex>
#include <new>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace GuestHeap {
namespace {

constexpr std::uint32_t ReadWriteProtection = 0x04;

// Each block is preceded by a header holding the raw block address and its size in bytes.
constexpr std::size_t HeaderBytes = 2 * sizeof(void*);
constexpr std::size_t RawAlignment = 16;
constexpr std::size_t MinimumAlignment = 32;
constexpr std::size_t PageBytes = 0x4000;
constexpr std::size_t SpanBytes = 1u << 20;
constexpr std::size_t MaximumSmallBytes = 64u * 1024u;
// Large blocks are rounded up to this so that freed blocks are reused by later requests of similar size.
constexpr std::size_t LargeGranuleBytes = 64u * 1024u;

std::size_t alignUp(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

struct Usage {
    std::atomic<std::size_t> system{0};
    std::atomic<std::size_t> maxSystem{0};
    std::atomic<std::size_t> inUse{0};
    std::atomic<std::size_t> maxInUse{0};
};

Usage& usage() {
    static Usage value;
    return value;
}

void grow(std::atomic<std::size_t>& current, std::atomic<std::size_t>& peak, std::size_t bytes) {
    const auto now = current.fetch_add(bytes, std::memory_order_relaxed) + bytes;
    auto seen = peak.load(std::memory_order_relaxed);
    while (now > seen && !peak.compare_exchange_weak(seen, now, std::memory_order_relaxed)) {}
}

void shrink(std::atomic<std::size_t>& current, std::size_t bytes) {
    current.fetch_sub(bytes, std::memory_order_relaxed);
}

class ArenaHeap {
public:
    static ArenaHeap& Get() {
        static ArenaHeap heap;
        return heap;
    }

    void* Allocate(std::size_t total, std::size_t& blockBytes) {
        if (total > MaximumSmallBytes &&
            total > std::numeric_limits<std::size_t>::max() - (LargeGranuleBytes - 1))
            throw std::length_error("guest heap allocation overflow");
        if (total > MaximumSmallBytes) {
            blockBytes = alignUp(total, LargeGranuleBytes);
            if (void* cached = _large.Take(blockBytes)) return cached;
            void* raw = arenaAllocate(blockBytes, PageBytes);
            commit(raw, blockBytes);
            grow(usage().system, usage().maxSystem, blockBytes);
            return raw;
        }
        auto& sizeClass = _classes[classIndex(total)];
        blockBytes = sizeClass.bytes;
        std::lock_guard lock(sizeClass.lock);
        if (sizeClass.freeList) {
            void* block = sizeClass.freeList;
            sizeClass.freeList = *static_cast<void**>(block);
            return block;
        }
        if (sizeClass.bump == nullptr || sizeClass.bytes > static_cast<std::size_t>(sizeClass.bumpEnd - sizeClass.bump)) {
            auto* span = static_cast<std::uint8_t*>(arenaAllocate(SpanBytes, PageBytes));
            commit(span, SpanBytes);
            grow(usage().system, usage().maxSystem, SpanBytes);
            sizeClass.bump = span;
            sizeClass.bumpEnd = span + SpanBytes;
        }
        void* block = sizeClass.bump;
        sizeClass.bump += sizeClass.bytes;
        return block;
    }

    void Free(void* raw, std::size_t blockBytes) {
        if (blockBytes > MaximumSmallBytes) {
            _large.Put(raw, blockBytes);
            return;
        }
        auto& sizeClass = _classes[classIndex(blockBytes)];
        std::lock_guard lock(sizeClass.lock);
        *static_cast<void**>(raw) = sizeClass.freeList;
        sizeClass.freeList = raw;
    }

private:
    struct SizeClass {
        std::mutex lock;
        std::size_t bytes = 0;
        void* freeList = nullptr;
        std::uint8_t* bump = nullptr;
        std::uint8_t* bumpEnd = nullptr;
    };

    static constexpr std::size_t ClassCount = 64;

    ArenaHeap() {
        std::size_t bytes = 32;
        for (std::size_t index = 0; index < ClassCount; ++index) {
            _classes[index].bytes = std::min(bytes, MaximumSmallBytes);
            bytes = bytes < 256 ? bytes + 32 : alignUp(bytes + bytes / 4, 64);
        }
        _classes[ClassCount - 1].bytes = MaximumSmallBytes;
    }

    std::size_t classIndex(std::size_t bytes) const {
        const auto found = std::lower_bound(_classes.begin(), _classes.end(), bytes, [](const SizeClass& sizeClass, std::size_t value) { return sizeClass.bytes < value; });
        return static_cast<std::size_t>(found - _classes.begin());
    }

    static void* arenaAllocate(std::size_t bytes, std::size_t alignment) {
        try {
            return GuestArena::GuestArenaAllocate_nid_postfix(bytes, alignment);
        } catch (const std::runtime_error&) {
            throw std::bad_alloc();
        }
    }

    static void commit(void* pointer, std::size_t bytes) {
        try {
            GuestArena::GuestArenaCommit_nid_postfix(pointer, bytes, ReadWriteProtection, bytes);
        } catch (const std::system_error& error) {
            abandon(pointer, bytes);
            if (outOfMemory(error)) throw std::bad_alloc();
            throw;
        } catch (...) {
            abandon(pointer, bytes);
            throw;
        }
    }

    static void abandon(void* pointer, std::size_t bytes) {
        GuestArena::GuestArenaReset_nid_postfix(pointer, bytes);
        GuestArena::GuestArenaRelease_nid_postfix(pointer, bytes);
    }

    static bool outOfMemory(const std::system_error& error) {
#ifdef _WIN32
        constexpr int NotEnoughMemory = 8;
        constexpr int OutOfMemory = 14;
        constexpr int CommitmentLimit = 1455;
        return error.code().category() == std::system_category() &&
            (error.code().value() == NotEnoughMemory || error.code().value() == OutOfMemory || error.code().value() == CommitmentLimit);
#else
        return error.code() == std::errc::not_enough_memory;
#endif
    }

    // Freed large blocks stay committed and are handed out again for the same block size: the game
    // frees and re-allocates hundreds of MB of temporary buffers every frame, and decommitting each
    // one made every re-touch a demand-zero page fault plus a TLB shootdown across every thread
    // (over 100K faults per second, a third of the main thread's time in the kernel). The cache is
    // bounded (APS5_HEAP_CACHE_MIB, default 4096); past the budget the oldest blocks are decommitted
    // and released, which is also the only time the GPU driver's page cache needs invalidating.
    class LargeCache {
    public:
        void* Take(std::size_t bytes) {
            std::lock_guard lock(_lock);
            const auto found = _bySize.find(bytes);
            if (found == _bySize.end() || found->second.empty()) return nullptr;
            const auto entry = found->second.back();
            found->second.pop_back();
            void* raw = entry->raw;
            _cachedBytes -= entry->bytes;
            _order.erase(entry);
            return raw;
        }

        void Put(void* raw, std::size_t bytes) {
            std::lock_guard lock(_lock);
            _order.push_back({raw, bytes});
            _bySize[bytes].push_back(std::prev(_order.end()));
            _cachedBytes += bytes;
            while (_cachedBytes > budget() && !_order.empty()) {
                const auto oldest = _order.front();
                auto& same = _bySize[oldest.bytes];
                same.erase(std::find(same.begin(), same.end(), _order.begin()));
                _order.pop_front();
                _cachedBytes -= oldest.bytes;
                release(oldest.raw, oldest.bytes);
            }
        }

    private:
        struct Entry {
            void* raw;
            std::size_t bytes;
        };

        static std::size_t budget() {
            static const std::size_t value = [] {
                const char* text = std::getenv("APS5_HEAP_CACHE_MIB");
                return (text ? static_cast<std::size_t>(std::strtoull(text, nullptr, 10)) : std::size_t{4096}) << 20;
            }();
            return value;
        }

        static void release(void* raw, std::size_t bytes) {
            GuestArena::GuestArenaReset_nid_postfix(raw, bytes);
            GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(raw), bytes);
            GuestArena::GuestArenaRelease_nid_postfix(raw, bytes);
            shrink(usage().system, bytes);
        }

        std::mutex _lock;
        std::list<Entry> _order;
        std::unordered_map<std::size_t, std::vector<std::list<Entry>::iterator>> _bySize;
        std::size_t _cachedBytes = 0;
    };

    std::array<SizeClass, ClassCount> _classes;
    LargeCache _large;
};

void* rawAllocate(std::size_t total, std::size_t& blockBytes) {
    if (GuestArena::GuestArenaAvailable_nid_postfix()) return ArenaHeap::Get().Allocate(total, blockBytes);
    blockBytes = total;
    void* raw = std::malloc(total);
    if (raw == nullptr) throw std::bad_alloc();
    grow(usage().system, usage().maxSystem, total);
    return raw;
}

void rawFree(void* raw, std::size_t blockBytes) {
    if (GuestArena::GuestArenaAvailable_nid_postfix()) {
        ArenaHeap::Get().Free(raw, blockBytes);
    } else {
        std::free(raw);
        shrink(usage().system, blockBytes);
    }
}

void* allocate(GuestAllocations::Mutation& mutation, std::size_t alignment, std::size_t bytes, bool gpu) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) throw std::invalid_argument("invalid guest heap alignment");
    alignment = std::max(alignment, MinimumAlignment);
    const std::size_t padding = alignment > RawAlignment ? alignment : 0;
    if (bytes > std::numeric_limits<std::size_t>::max() - HeaderBytes - padding) throw std::length_error("guest heap allocation overflow");
    std::size_t blockBytes = 0;
    void* raw = rawAllocate(bytes + HeaderBytes + padding, blockBytes);
    const auto address = alignUp(reinterpret_cast<std::uintptr_t>(raw) + HeaderBytes, alignment);
    auto* pointer = reinterpret_cast<void*>(address);
    reinterpret_cast<void**>(pointer)[-2] = raw;
    reinterpret_cast<std::size_t*>(pointer)[-1] = blockBytes;
    try {
        mutation.Add(pointer, bytes, true, true, gpu);
    } catch (...) {
        rawFree(raw, blockBytes);
        throw;
    }
    grow(usage().inUse, usage().maxInUse, blockBytes);
    return pointer;
}

void free(GuestAllocations::Mutation& mutation, void* pointer) {
    mutation.Remove(pointer);
    const auto blockBytes = reinterpret_cast<std::size_t*>(pointer)[-1];
    shrink(usage().inUse, blockBytes);
    rawFree(reinterpret_cast<void**>(pointer)[-2], blockBytes);
}

}

void* GuestHeapAllocate_nid_postfix(std::size_t bytes) {
    GuestAllocations::Mutation mutation;
    return allocate(mutation, MinimumAlignment, bytes, false);
}

void GuestHeapFree_nid_postfix(void* pointer) {
    if (pointer == nullptr) return;
    GuestAllocations::Mutation mutation;
    const auto range = mutation.Find(pointer);
    mutation.RequireUnpinned(pointer, range.bytes);
    free(mutation, pointer);
}

void* GuestHeapReallocate_nid_postfix(void* pointer, std::size_t bytes) {
    if (pointer == nullptr) return GuestHeapAllocate_nid_postfix(bytes);
    GuestAllocations::Mutation mutation;
    const auto range = mutation.Find(pointer);
    mutation.RequireUnpinned(pointer, range.bytes);
    if (bytes == 0) {
        free(mutation, pointer);
        return nullptr;
    }
    void* result = allocate(mutation, MinimumAlignment, bytes, range.gpu);
    std::memcpy(result, pointer, std::min(bytes, range.bytes));
    free(mutation, pointer);
    return result;
}

void* GuestHeapRealign_nid_postfix(void* pointer, std::size_t bytes, std::size_t alignment) {
    if (pointer == nullptr) return GuestHeapAlign_nid_postfix(alignment, bytes);
    GuestAllocations::Mutation mutation;
    const auto range = mutation.Find(pointer);
    mutation.RequireUnpinned(pointer, range.bytes);
    if (bytes == 0) {
        free(mutation, pointer);
        return nullptr;
    }
    void* result = allocate(mutation, alignment, bytes, range.gpu);
    std::memcpy(result, pointer, std::min(bytes, range.bytes));
    free(mutation, pointer);
    return result;
}

std::size_t GuestHeapUsableSize_nid_postfix(const void* pointer) {
    if (pointer == nullptr) return 0;
    GuestAllocations::Mutation mutation;
    return mutation.Find(pointer).bytes;
}

void* GuestHeapAlign_nid_postfix(std::size_t alignment, std::size_t bytes) {
    GuestAllocations::Mutation mutation;
    return allocate(mutation, alignment, bytes, false);
}

void* GuestHeapAlignForGpu_nid_postfix(std::size_t alignment, std::size_t bytes) {
    GuestAllocations::Mutation mutation;
    return allocate(mutation, alignment, bytes, true);
}

int GuestHeapStatsFast_nid_postfix(void* stats) {
    auto* managed = static_cast<MallocManagedSize*>(stats);
    if (managed == nullptr || managed->size < sizeof(MallocManagedSize)) return 22;
    auto& value = usage();
    managed->maxSystemSize = value.maxSystem.load(std::memory_order_relaxed);
    managed->currentSystemSize = value.system.load(std::memory_order_relaxed);
    managed->maxInuseSize = value.maxInUse.load(std::memory_order_relaxed);
    managed->currentInuseSize = value.inUse.load(std::memory_order_relaxed);
    return 0;
}

}
