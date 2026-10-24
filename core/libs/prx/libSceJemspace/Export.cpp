#define JEMALLOC_NO_DEMANGLE
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <jemalloc/jemalloc.h>
#include <mutex>

#include "prx/libc/include/general/VabiMacros.hpp"
extern "C" {
void* APS5_VABI malloc_nid_postfix(std::size_t);
void APS5_VABI free_nid_postfix(void*);
int APS5_VABI sceKernelMapNamedFlexibleMemory(void**, std::size_t, int, int, const char*);
int APS5_VABI sceKernelReserveVirtualRange(void**, std::size_t, int, std::size_t);
int APS5_VABI sceKernelMunmap(std::uint64_t, std::size_t);
}
namespace {
struct Space {
    extent_hooks_t hooks;
    std::array<char, 32> name;
    std::uint64_t limit;
    std::uint64_t committed;
    std::uint64_t reserved;
    std::uint64_t peakCommitted;
    std::uint64_t peakReserved;
    unsigned arena;
    unsigned flags;
};
static_assert(sizeof(Space) == 152);
static_assert(offsetof(Space, limit) == 104);
using StatsWriter = void(APS5_VABI*)(void*, const char*);
struct StatsOutput {
    StatsWriter write;
    void* opaque;
};
void WriteStats(void* opaque, const char* text) {
    auto& output = *static_cast<StatsOutput*>(opaque);
    output.write(output.opaque, text);
}
std::recursive_mutex apiMutex;
void Peaks(Space& s) {
    s.peakCommitted = std::max(s.peakCommitted, s.committed);
    s.peakReserved = std::max(s.peakReserved, s.reserved);
}
void* Map(Space& s, void* address, std::size_t bytes, std::size_t alignment) {
    void* p = address;
    if (sceKernelMapNamedFlexibleMemory(&p, bytes, 2, address ? 16 : 0, s.name.data()) != 0) {
        return nullptr;
    }
    if ((address && p == address) ||
        (!address && (alignment == 0 || (reinterpret_cast<std::uintptr_t>(p) & (alignment - 1)) == 0))) {
        return p;
    }
    sceKernelMunmap(reinterpret_cast<std::uintptr_t>(p), bytes);
    if (address) {
        return nullptr;
    }
    void* reserved = nullptr;
    if (sceKernelReserveVirtualRange(&reserved, bytes, 0, alignment) != 0 || reserved == nullptr) {
        return nullptr;
    }
    void* mapped = reserved;
    if (sceKernelMapNamedFlexibleMemory(&mapped, bytes, 2, 16, s.name.data()) != 0) {
        mapped = reinterpret_cast<void*>(-1);
    }
    if (mapped == reinterpret_cast<void*>(-1)) {
        sceKernelMunmap(reinterpret_cast<std::uintptr_t>(reserved), bytes);
        return nullptr;
    }
    return mapped;
}
void* AllocateExtent(extent_hooks_t* h, void* address, std::size_t bytes, std::size_t alignment, bool* zero,
                     bool* commit, unsigned) {
    auto& s = *reinterpret_cast<Space*>(h);
    bytes = (bytes + 16383) & ~std::size_t(16383);
    if (*commit) {
        if (s.limit && bytes + s.committed > s.limit) {
            return nullptr;
        }
        void* p = Map(s, address, bytes, alignment);
        if (!p) {
            return nullptr;
        }
        *zero = true;
        s.committed += bytes;
        s.reserved += bytes;
        Peaks(s);
        return p;
    }
    void* p = address;
    if (sceKernelReserveVirtualRange(&p, bytes, address ? 16 : 0, address ? 0 : alignment) != 0) {
        return nullptr;
    }
    if (address && p != address) {
        sceKernelMunmap(reinterpret_cast<std::uintptr_t>(p), bytes);
        return nullptr;
    }
    if (p) {
        s.reserved += bytes;
        Peaks(s);
    }
    return p;
}
bool DeallocateExtent(extent_hooks_t* h, void* address, std::size_t bytes, bool committed, unsigned) {
    auto& s = *reinterpret_cast<Space*>(h);
    if (sceKernelMunmap(reinterpret_cast<std::uintptr_t>(address), bytes) == 0) {
        s.reserved -= bytes;
        if (committed) {
            s.committed -= bytes;
        }
        Peaks(s);
    }
    return false;
}
void DestroyExtent(extent_hooks_t* h, void* address, std::size_t bytes, bool committed, unsigned arena) {
    DeallocateExtent(h, address, bytes, committed, arena);
}
bool CommitExtent(extent_hooks_t* h, void* address, std::size_t, std::size_t offset, std::size_t bytes, unsigned) {
    auto& s = *reinterpret_cast<Space*>(h);
    if (!Map(s, static_cast<std::byte*>(address) + offset, bytes, 0)) {
        return true;
    }
    s.committed += bytes;
    Peaks(s);
    return false;
}
bool DecommitExtent(extent_hooks_t* h, void* address, std::size_t, std::size_t offset, std::size_t bytes, unsigned) {
    auto& s = *reinterpret_cast<Space*>(h);
    void* expected = static_cast<std::byte*>(address) + offset;
    void* p = expected;
    if (sceKernelReserveVirtualRange(&p, bytes, expected ? 16 : 0, 0) != 0) {
        return true;
    }
    if (p != expected) {
        if (p) {
            sceKernelMunmap(reinterpret_cast<std::uintptr_t>(p), bytes);
        }
        return true;
    }
    s.committed -= bytes;
    Peaks(s);
    return false;
}
bool PurgeExtent(extent_hooks_t* h, void* address, std::size_t, std::size_t offset, std::size_t bytes, unsigned) {
    auto& s = *reinterpret_cast<Space*>(h);
    if (sceKernelMunmap(reinterpret_cast<std::uintptr_t>(static_cast<std::byte*>(address) + offset), bytes) != 0) {
        return true;
    }
    s.committed -= bytes;
    s.reserved -= bytes;
    Peaks(s);
    return false;
}
bool RejectPurge(extent_hooks_t*, void*, std::size_t, std::size_t, std::size_t, unsigned) {
    return true;
}
extent_hooks_t Hooks() {
    return {AllocateExtent, DeallocateExtent, DestroyExtent, CommitExtent, DecommitExtent,
            PurgeExtent,    RejectPurge,      nullptr,       nullptr};
}
int Flags(void* space) {
    return space ? static_cast<Space*>(space)->flags : 0;
}
int Alignment(std::size_t a) {
    const auto m = a - 1;
    return m ? std::countr_zero(m) + 1 : 0;
}
}
extern "C" {
void* APS5_VABI sceJemspaceCreate(const char* name, std::size_t limit, unsigned flags) {
    std::lock_guard lock(apiMutex);
    if (flags) {
        std::fwrite("sceJemspaceCreate(): flags must be 0\n", 37, 1, stderr);
        return nullptr;
    }
    auto* s = static_cast<Space*>(malloc_nid_postfix(sizeof(Space)));
    if (!s) {
        std::fputs("sceJemspaceCreate(): Failed to allocate memory\n", stderr);
        return nullptr;
    }
    s->hooks = Hooks();
    s->name.fill(0);
    if (name) {
        const auto n = std::min<std::size_t>(std::strlen(name), 31);
        std::memcpy(s->name.data(), name, n);
    }
    s->limit = limit;
    s->committed = 0;
    s->reserved = 0;
    s->peakCommitted = 0;
    s->peakReserved = 0;
    auto* hooks = &s->hooks;
    std::size_t size = sizeof(s->arena);
    const int error = anyps5_jemspace_mallctl("arenas.create", &s->arena, &size, &hooks, sizeof(hooks));
    if (error) {
        std::fprintf(stderr, "jemallctl() with arenas.create failed. error = %s\n", std::strerror(error));
        free_nid_postfix(s);
        return nullptr;
    }
    s->flags = (s->arena + 1U) << 20;
    return s;
}
int APS5_VABI sceJemspaceDestroy(void* p) {
    std::lock_guard lock(apiMutex);
    if (!p) {
        return 22;
    }
    auto* s = static_cast<Space*>(p);
    char command[64];
    std::snprintf(command, sizeof(command), "arena.%u.destroy", s->arena);
    const int error = anyps5_jemspace_mallctl(command, nullptr, nullptr, nullptr, 0);
    if (error) {
        std::fprintf(stderr, "jemallctl() with arenas.destroy failed. error = %s\n", std::strerror(error));
    }
    free_nid_postfix(s);
    return 0;
}
void* APS5_VABI sceJemspaceMalloc(void* p, std::size_t size) {
    std::lock_guard lock(apiMutex);
    return anyps5_jemspace_mallocx(size, Flags(p));
}
int APS5_VABI sceJemspaceFree(void*, void* allocation) {
    std::lock_guard lock(apiMutex);
    anyps5_jemspace_free(allocation);
    return 0;
}
void* APS5_VABI sceJemspaceCalloc(void* p, std::size_t count, std::size_t size) {
    std::lock_guard lock(apiMutex);
    return anyps5_jemspace_mallocx(count * size, Flags(p) | MALLOCX_ZERO);
}
void* APS5_VABI sceJemspaceMemalign(void* p, std::size_t alignment, std::size_t size) {
    std::lock_guard lock(apiMutex);
    return anyps5_jemspace_mallocx(size, Flags(p) | Alignment(alignment));
}
void* APS5_VABI sceJemspaceRealloc(void* p, void* allocation, std::size_t size) {
    std::lock_guard lock(apiMutex);
    const auto flags = Flags(p);
    return allocation ? anyps5_jemspace_rallocx(allocation, size, flags) : anyps5_jemspace_mallocx(size, flags);
}
void* APS5_VABI sceJemspaceReallocalign(void* p, void* allocation, std::size_t alignment, std::size_t size) {
    std::lock_guard lock(apiMutex);
    const auto flags = Flags(p) | Alignment(alignment);
    return allocation ? anyps5_jemspace_rallocx(allocation, size, flags) : anyps5_jemspace_mallocx(size, flags);
}
int APS5_VABI sceJemspacePosixMemalign(void* p, void** allocation, std::size_t alignment, std::size_t size) {
    std::lock_guard lock(apiMutex);
    if (!std::has_single_bit(alignment)) {
        return 22;
    }
    void* value = anyps5_jemspace_mallocx(size, Flags(p) | Alignment(alignment));
    if (!value) {
        return 12;
    }
    *allocation = value;
    return 0;
}
std::size_t APS5_VABI sceJemspaceMallocUsableSize(const void* allocation) {
    std::lock_guard lock(apiMutex);
    return allocation ? anyps5_jemspace_sallocx(allocation, 0) : 0;
}
int APS5_VABI sceJemspacePurge(void* p) {
    std::lock_guard lock(apiMutex);
    char command[64];
    std::snprintf(command, sizeof(command), "arena.%u.purge", p ? static_cast<Space*>(p)->arena : 4096U);
    const int error = anyps5_jemspace_mallctl(command, nullptr, nullptr, nullptr, 0);
    if (error) {
        std::fprintf(stderr, "jemallctl() with arenas.purge failed. error = %s\n", std::strerror(error));
    }
    return 0;
}
void APS5_VABI sceJemspacePrintStats(StatsWriter write, void* opaque, const char* options) {
    std::lock_guard lock(apiMutex);
    if (!write) {
        return;
    }
    StatsOutput output{write, opaque};
    anyps5_jemspace_malloc_stats_print(WriteStats, &output, options);
}
void APS5_VABI sceJemspaceGetStats(const void* p, void* stats) {
    std::lock_guard lock(apiMutex);
    if (!p || !stats) {
        return;
    }
    const auto* source = static_cast<const std::byte*>(p) + 104;
    auto* dest = static_cast<std::byte*>(stats);
    std::memmove(dest, source, 32);
    std::memmove(dest + 32, source + 32, 8);
}
}
