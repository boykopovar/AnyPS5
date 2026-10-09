#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <utility>

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {

constexpr unsigned MspaceThreadUnsafe = 1;
constexpr std::uintptr_t Granule = 16;
constexpr std::uintptr_t ArenaHeaderBytes = 64;

struct Chunk {
    std::uintptr_t end;
    bool used;
    std::size_t requested;
};

struct Arena {
    std::uintptr_t base;
    std::uintptr_t end;
    std::map<std::uintptr_t, Chunk> chunks;
    std::set<std::pair<std::size_t, std::uintptr_t>> free;
    std::size_t inUse = 0;
    std::size_t peakInUse = 0;
};

std::mutex arenaMutex;
std::map<std::uintptr_t, std::unique_ptr<Arena>> arenas;

void Error(int value) { *__error_nid_postfix() = value; }

std::uintptr_t AlignUp(std::uintptr_t value, std::uintptr_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

Arena* Find(void* handle) {
    const auto found = arenas.find(reinterpret_cast<std::uintptr_t>(handle));
    if (found != arenas.end()) return found->second.get();
    Error(22);
    return nullptr;
}

void AddFree(Arena& arena, std::uintptr_t start, std::uintptr_t end) {
    arena.chunks[start] = {end, false, 0};
    arena.free.emplace(end - start, start);
}

void RemoveFree(Arena& arena, std::map<std::uintptr_t, Chunk>::iterator chunk) {
    arena.free.erase({chunk->second.end - chunk->first, chunk->first});
}

void* Allocate(Arena* arena, std::size_t size, std::size_t alignment) {
    if (!arena) return nullptr;
    alignment = std::max<std::size_t>(alignment, Granule);
    const auto needed = AlignUp(std::max<std::size_t>(size, 1), Granule);
    if (needed < size || needed > std::numeric_limits<std::uintptr_t>::max() - alignment) {
        Error(12);
        return nullptr;
    }
    for (auto candidate = arena->free.lower_bound({needed, 0}); candidate != arena->free.end(); ++candidate) {
        const auto start = candidate->second;
        const auto chunk = arena->chunks.find(start);
        const auto end = chunk->second.end;
        const auto aligned = AlignUp(start, alignment);
        if (aligned < start || aligned > end || end - aligned < needed) continue;
        RemoveFree(*arena, chunk);
        arena->chunks.erase(chunk);
        if (aligned > start) AddFree(*arena, start, aligned);
        const auto finish = aligned + needed;
        if (finish < end) AddFree(*arena, finish, end);
        arena->chunks[aligned] = {finish, true, size};
        arena->inUse += needed;
        arena->peakInUse = std::max(arena->peakInUse, arena->inUse);
        return reinterpret_cast<void*>(aligned);
    }
    Error(12);
    return nullptr;
}

bool FindUsed(Arena* arena, const void* pointer, std::map<std::uintptr_t, Chunk>::iterator& chunk) {
    if (arena && pointer) {
        chunk = arena->chunks.find(reinterpret_cast<std::uintptr_t>(pointer));
        if (chunk != arena->chunks.end() && chunk->second.used) return true;
    }
    Error(22);
    return false;
}

void Release(Arena& arena, std::map<std::uintptr_t, Chunk>::iterator chunk) {
    auto start = chunk->first;
    auto end = chunk->second.end;
    arena.inUse -= end - start;
    if (chunk != arena.chunks.begin()) {
        const auto previous = std::prev(chunk);
        if (!previous->second.used) {
            start = previous->first;
            RemoveFree(arena, previous);
            arena.chunks.erase(previous);
        }
    }
    const auto next = std::next(chunk);
    if (next != arena.chunks.end() && !next->second.used) {
        end = next->second.end;
        RemoveFree(arena, next);
        arena.chunks.erase(next);
    }
    arena.chunks.erase(chunk);
    AddFree(arena, start, end);
}

bool InsideAllocation(const Arena& arena, std::uintptr_t start, std::size_t size) {
    auto found = arena.chunks.upper_bound(start);
    if (found == arena.chunks.begin()) return false;
    --found;
    return found->second.used && size <= found->second.end - start && start - found->first <= found->second.end - found->first - size;
}

struct MallocManagedSize {
    std::uint16_t size;
    std::uint16_t version;
    std::uint32_t reserved;
    std::size_t maxSystemSize;
    std::size_t currentSystemSize;
    std::size_t maxInuseSize;
    std::size_t currentInuseSize;
};
static_assert(sizeof(MallocManagedSize) == 0x28);

int FillStats(void* handle, MallocManagedSize* stats) {
    if (!stats || stats->size < sizeof(MallocManagedSize)) return 22;
    std::lock_guard lock(arenaMutex);
    auto* arena = Find(handle);
    if (!arena) return 22;
    const auto system = arena->end - arena->base;
    stats->maxSystemSize = system;
    stats->currentSystemSize = system;
    stats->maxInuseSize = arena->peakInUse;
    stats->currentInuseSize = arena->inUse;
    return 0;
}

}

extern "C" {
void* APS5_VABI sceLibcMspaceCreate_nid_postfix(const char* name, void* base,
                                              std::size_t size, unsigned flags) {
    (void)name;
    const auto start = reinterpret_cast<std::uintptr_t>(base);
    if (!base || (start & (Granule - 1)) || size < ArenaHeaderBytes + 2 * Granule ||
        size > std::numeric_limits<std::uintptr_t>::max() - start || (flags & ~MspaceThreadUnsafe) != 0) {
        Error(22);
        return nullptr;
    }
    std::lock_guard lock(arenaMutex);
    for (const auto& [otherBase, other] : arenas) {
        if (start < other->end && otherBase < start + size && !InsideAllocation(*other, start, size)) {
            Error(22);
            return nullptr;
        }
    }
    auto arena = std::make_unique<Arena>();
    arena->base = start;
    arena->end = start + size;
    const auto first = start + ArenaHeaderBytes;
    const auto last = start + (size & ~(Granule - 1));
    AddFree(*arena, first, last);
    arenas.emplace(start, std::move(arena));
    return base;
}

int APS5_VABI sceLibcMspaceDestroy_nid_postfix(void* handle) {
    std::lock_guard lock(arenaMutex);
    if (arenas.erase(reinterpret_cast<std::uintptr_t>(handle)) != 0) return 0;
    Error(22);
    return -1;
}

void* APS5_VABI sceLibcMspaceMalloc_nid_postfix(void* handle, std::size_t size) {
    std::lock_guard lock(arenaMutex);
    return Allocate(Find(handle), size, Granule);
}

void APS5_VABI sceLibcMspaceFree_nid_postfix(void* handle, void* pointer) {
    if (!pointer) return;
    std::lock_guard lock(arenaMutex);
    auto* arena = Find(handle);
    std::map<std::uintptr_t, Chunk>::iterator chunk;
    if (FindUsed(arena, pointer, chunk)) Release(*arena, chunk);
}

void* APS5_VABI sceLibcMspaceCalloc_nid_postfix(void* handle, std::size_t count, std::size_t size) {
    if (size && count > std::numeric_limits<std::size_t>::max() / size) {
        Error(12);
        return nullptr;
    }
    std::lock_guard lock(arenaMutex);
    void* result = Allocate(Find(handle), count * size, Granule);
    if (result) std::memset(result, 0, count * size);
    return result;
}

void* APS5_VABI sceLibcMspaceRealloc_nid_postfix(void* handle, void* pointer, std::size_t size) {
    std::lock_guard lock(arenaMutex);
    auto* arena = Find(handle);
    if (!pointer) return Allocate(arena, size, Granule);
    std::map<std::uintptr_t, Chunk>::iterator chunk;
    if (!FindUsed(arena, pointer, chunk)) return nullptr;
    if (!size) { Release(*arena, chunk); return nullptr; }
    const auto needed = AlignUp(size, Granule);
    if (needed < size) { Error(12); return nullptr; }
    const auto start = chunk->first;
    const auto capacity = chunk->second.end - start;
    if (needed <= capacity) {
        chunk->second.requested = size;
        return pointer;
    }
    const auto next = std::next(chunk);
    if (next != arena->chunks.end() && !next->second.used && next->first == chunk->second.end && next->second.end - start >= needed) {
        const auto nextEnd = next->second.end;
        RemoveFree(*arena, next);
        arena->chunks.erase(next);
        const auto finish = start + needed;
        if (finish < nextEnd) AddFree(*arena, finish, nextEnd);
        arena->inUse += needed - capacity;
        arena->peakInUse = std::max(arena->peakInUse, arena->inUse);
        arena->chunks[start] = {finish, true, size};
        return pointer;
    }
    const auto previousSize = chunk->second.requested;
    void* result = Allocate(arena, size, Granule);
    if (result) {
        std::memcpy(result, pointer, previousSize);
        Release(*arena, arena->chunks.find(start));
    }
    return result;
}

int APS5_VABI sceLibcMspacePosixMemalign_nid_postfix(void* handle, void** result,
                                                   std::size_t alignment, std::size_t size) {
    if (!result || alignment < sizeof(void*) || (alignment & (alignment - 1))) return 22;
    std::lock_guard lock(arenaMutex);
    auto* arena = Find(handle);
    if (!arena) return 22;
    void* pointer = Allocate(arena, size, alignment);
    if (!pointer) return 12;
    *result = pointer;
    return 0;
}

void* APS5_VABI sceLibcMspaceMemalign_nid_postfix(void* handle, std::size_t alignment, std::size_t size) {
    if (alignment == 0 || (alignment & (alignment - 1))) {
        Error(22);
        return nullptr;
    }
    std::lock_guard lock(arenaMutex);
    return Allocate(Find(handle), size, alignment);
}

void* APS5_VABI sceLibcMspaceReallocalign_nid_postfix(void* handle, void* pointer, std::size_t size, std::size_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1))) {
        Error(22);
        return nullptr;
    }
    const auto effective = std::max<std::size_t>(alignment, 16);
    std::lock_guard lock(arenaMutex);
    auto* arena = Find(handle);
    if (!pointer) return Allocate(arena, size, effective);
    std::map<std::uintptr_t, Chunk>::iterator chunk;
    if (!FindUsed(arena, pointer, chunk)) return nullptr;
    if (!size) { Release(*arena, chunk); return nullptr; }
    const auto needed = AlignUp(size, Granule);
    if (needed < size) { Error(12); return nullptr; }
    const auto start = chunk->first;
    if (needed <= chunk->second.end - start && (start & (effective - 1)) == 0) {
        chunk->second.requested = size;
        return pointer;
    }
    const auto previousSize = chunk->second.requested;
    void* result = Allocate(arena, size, effective);
    if (result) {
        std::memcpy(result, pointer, std::min(previousSize, size));
        Release(*arena, arena->chunks.find(start));
    }
    return result;
}

int APS5_VABI sceLibcMspaceMallocStats_nid_postfix(void* handle, MallocManagedSize* stats) {
    return FillStats(handle, stats);
}

int APS5_VABI sceLibcMspaceMallocStatsFast_nid_postfix(void* handle, MallocManagedSize* stats) {
    return FillStats(handle, stats);
}

std::size_t APS5_VABI sceLibcMspaceMallocUsableSize_nid_postfix(const void* pointer) {
    if (!pointer) return 0;
    std::lock_guard lock(arenaMutex);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    for (auto arena = arenas.upper_bound(address); arena != arenas.begin();) {
        --arena;
        if (address >= arena->second->end) continue;
        const auto found = arena->second->chunks.find(address);
        if (found != arena->second->chunks.end() && found->second.used) return found->second.end - found->first;
    }
    Error(22);
    return 0;
}
}
