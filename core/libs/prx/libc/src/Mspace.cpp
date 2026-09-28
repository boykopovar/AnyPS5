#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {
struct alignas(16) Block {
    std::size_t capacity;
    Block* next;
    void* pointer;
    std::size_t used;
};
struct alignas(16) Arena {
    Arena* next;
    Block* first;
    std::uintptr_t end;
    std::size_t inUse;
    std::size_t peakInUse;
};
constexpr unsigned MspaceThreadUnsafe = 1;
std::mutex arenaMutex;
Arena* arenas = nullptr;

void Error(int value) { *__error_nid_postfix() = value; }
Arena* Find(void* handle) {
    for (auto* arena = arenas; arena; arena = arena->next)
        if (arena == handle) return arena;
    Error(22);
    return nullptr;
}
Block* FindBlock(Arena* arena, const void* pointer) {
    if (arena && pointer)
        for (auto* block = arena->first; block; block = block->next)
            if (block->pointer == pointer) return block;
    Error(22);
    return nullptr;
}
void* Allocate(Arena* arena, std::size_t size, std::size_t alignment) {
    if (!arena) return nullptr;
    size = std::max<std::size_t>(size, 1);
    for (auto* block = arena->first; block; block = block->next) {
        if (block->pointer) continue;
        const auto start = reinterpret_cast<std::uintptr_t>(block + 1);
        if (start > std::numeric_limits<std::uintptr_t>::max() - (alignment - 1)) continue;
        const auto aligned = (start + alignment - 1) & ~(alignment - 1);
        const auto padding = aligned - start;
        if (padding > block->capacity || size > block->capacity - padding) continue;
        auto consumed = padding + size;
        // Splitting preserves header alignment and avoids small unusable fragments.
        if (consumed <= std::numeric_limits<std::size_t>::max() - 15) {
            const auto rounded = (consumed + 15) & ~std::size_t{15};
            if (rounded <= block->capacity && block->capacity - rounded >= sizeof(Block) + 16) {
                auto* tail = reinterpret_cast<Block*>(start + rounded);
                *tail = {block->capacity - rounded - sizeof(Block), block->next, nullptr, 0};
                block->capacity = rounded;
                block->next = tail;
            }
        }
        block->pointer = reinterpret_cast<void*>(aligned);
        block->used = size;
        arena->inUse += sizeof(Block) + block->capacity;
        arena->peakInUse = std::max(arena->peakInUse, arena->inUse);
        return block->pointer;
    }
    Error(12);
    return nullptr;
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
    const auto system = arena->end - reinterpret_cast<std::uintptr_t>(arena);
    stats->maxSystemSize = system;
    stats->currentSystemSize = system;
    stats->maxInuseSize = arena->peakInUse;
    stats->currentInuseSize = arena->inUse;
    return 0;
}

bool InsideAllocation(Arena* arena, std::uintptr_t start, std::size_t size) {
    for (auto* block = arena->first; block; block = block->next) {
        const auto pointer = reinterpret_cast<std::uintptr_t>(block->pointer);
        if (pointer && start >= pointer && size <= block->used && start - pointer <= block->used - size) return true;
    }
    return false;
}

void Release(Arena* arena, Block* released) {
    arena->inUse -= sizeof(Block) + released->capacity;
    released->pointer = nullptr;
    released->used = 0;
    for (auto* block = arena->first; block && block->next;) {
        if (!block->pointer && !block->next->pointer) {
            block->capacity += sizeof(Block) + block->next->capacity;
            block->next = block->next->next;
        } else block = block->next;
    }
}
}

extern "C" {
void* APS5_VABI sceLibcMspaceCreate_nid_postfix(const char* name, void* base,
                                              std::size_t size, unsigned flags) {
    (void)name;
    const auto start = reinterpret_cast<std::uintptr_t>(base);
    if (!base || (start & 15) || size < sizeof(Arena) + sizeof(Block) + 16 ||
        size > std::numeric_limits<std::uintptr_t>::max() - start || (flags & ~MspaceThreadUnsafe) != 0) {
        Error(22);
        return nullptr;
    }
    std::lock_guard lock(arenaMutex);
    for (auto* arena = arenas; arena; arena = arena->next) {
        if (start < arena->end && reinterpret_cast<std::uintptr_t>(arena) < start + size && !InsideAllocation(arena, start, size)) {
            Error(22);
            return nullptr;
        }
    }
    auto* arena = static_cast<Arena*>(base);
    auto* block = reinterpret_cast<Block*>(arena + 1);
    *block = {size - sizeof(Arena) - sizeof(Block), nullptr, nullptr, 0};
    *arena = {arenas, block, start + size, 0, 0};
    arenas = arena;
    return arena;
}

int APS5_VABI sceLibcMspaceDestroy_nid_postfix(void* handle) {
    std::lock_guard lock(arenaMutex);
    for (auto** entry = &arenas; *entry; entry = &(*entry)->next) {
        if (*entry == handle) {
            *entry = (*entry)->next;
            return 0;
        }
    }
    Error(22);
    return -1;
}

void* APS5_VABI sceLibcMspaceMalloc_nid_postfix(void* handle, std::size_t size) {
    std::lock_guard lock(arenaMutex);
    return Allocate(Find(handle), size, 16);
}

void APS5_VABI sceLibcMspaceFree_nid_postfix(void* handle, void* pointer) {
    if (!pointer) return;
    std::lock_guard lock(arenaMutex);
    auto* arena = Find(handle);
    if (auto* block = FindBlock(arena, pointer)) Release(arena, block);
}

void* APS5_VABI sceLibcMspaceCalloc_nid_postfix(void* handle, std::size_t count, std::size_t size) {
    if (size && count > std::numeric_limits<std::size_t>::max() / size) {
        Error(12);
        return nullptr;
    }
    std::lock_guard lock(arenaMutex);
    void* result = Allocate(Find(handle), count * size, 16);
    if (result) std::memset(result, 0, count * size);
    return result;
}

void* APS5_VABI sceLibcMspaceRealloc_nid_postfix(void* handle, void* pointer, std::size_t size) {
    std::lock_guard lock(arenaMutex);
    auto* arena = Find(handle);
    if (!pointer) return Allocate(arena, size, 16);
    auto* block = FindBlock(arena, pointer);
    if (!block) return nullptr;
    if (!size) { Release(arena, block); return nullptr; }
    const auto padding = reinterpret_cast<std::uintptr_t>(pointer) - reinterpret_cast<std::uintptr_t>(block + 1);
    if (size <= block->capacity - padding) { block->used = size; return pointer; }
    void* result = Allocate(arena, size, 16);
    if (result) {
        std::memcpy(result, pointer, block->used);
        Release(arena, block);
    }
    return result;
}

int APS5_VABI sceLibcMspacePosixMemalign_nid_postfix(void* handle, void** result,
                                                   std::size_t alignment, std::size_t size) {
    if (!result || alignment < sizeof(void*) || (alignment & (alignment - 1))) return 22;
    std::lock_guard lock(arenaMutex);
    auto* arena = Find(handle);
    if (!arena) return 22;
    void* pointer = Allocate(arena, size, std::max<std::size_t>(alignment, 16));
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
    return Allocate(Find(handle), size, std::max<std::size_t>(alignment, 16));
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
    for (auto* arena = arenas; arena; arena = arena->next)
        for (auto* block = arena->first; block; block = block->next)
            if (block->pointer == pointer) return block->used;
    Error(22);
    return 0;
}
}
