#ifdef _WIN32

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <intrin.h>
#include <malloc.h>
#include <new>
#include <windows.h>
#include <xmmintrin.h>

namespace {

constexpr std::size_t Granule = 16;
constexpr std::size_t LargestCached = 1024;
constexpr std::size_t Classes = LargestCached / Granule;
constexpr std::size_t SmallestKept = sizeof(void*);
constexpr std::uintptr_t HeldBits = Granule - 1;

constexpr std::uint16_t Depth(std::size_t sizeClass) {
    return sizeClass < 256 / Granule ? 64 : 16;
}

struct FreeBlock {
    std::uintptr_t link;
};

FreeBlock* Next(const FreeBlock* block) noexcept {
    return reinterpret_cast<FreeBlock*>(block->link & ~HeldBits);
}

std::size_t Held(const FreeBlock* block, std::size_t sizeClass) noexcept {
    return sizeClass * Granule + (block->link & HeldBits) + 1;
}

struct Cache {
    FreeBlock* heads[Classes];
    std::uint16_t counts[Classes];
};

constexpr std::uint16_t KeptLists = 8;

struct Depot {
    SRWLOCK lock = SRWLOCK_INIT;
    FreeBlock* lists[KeptLists] = {};
    std::uint16_t count = 0;
};

Depot depots[Classes];

bool KeepList(std::size_t sizeClass, FreeBlock* list) noexcept {
    auto& depot = depots[sizeClass];
    AcquireSRWLockExclusive(&depot.lock);
    const bool kept = depot.count < KeptLists;
    if (kept) depot.lists[depot.count++] = list;
    ReleaseSRWLockExclusive(&depot.lock);
    return kept;
}

FreeBlock* TakeList(std::size_t sizeClass) noexcept {
    auto& depot = depots[sizeClass];
    AcquireSRWLockExclusive(&depot.lock);
    FreeBlock* list = depot.count != 0 ? depot.lists[--depot.count] : nullptr;
    ReleaseSRWLockExclusive(&depot.lock);
    return list;
}

Cache* const Closed = reinterpret_cast<Cache*>(std::uintptr_t{1});

constexpr unsigned long TlsSlotsOffset = 0x1480;
constexpr DWORD DirectSlots = 64;

constexpr DWORD Unset = TLS_OUT_OF_INDEXES;
constexpr DWORD Failed = TLS_OUT_OF_INDEXES - 1;
std::atomic<DWORD> tlsSlot{Unset};
std::atomic<DWORD> flsSlot{FLS_OUT_OF_INDEXES};

void Drain(Cache* cache) noexcept {
    for (std::size_t sizeClass = 0; sizeClass < Classes; ++sizeClass) {
        while (FreeBlock* block = cache->heads[sizeClass]) {
            cache->heads[sizeClass] = Next(block);
            std::free(block);
        }
        cache->counts[sizeClass] = 0;
    }
}

void WINAPI ThreadEnded(void* data) {
    auto* cache = static_cast<Cache*>(data);
    if (cache == nullptr || cache == Closed) return;
    const auto slot = tlsSlot.load(std::memory_order_acquire);
    if (slot < Failed) TlsSetValue(slot, Closed);
    Drain(cache);
    std::free(cache);
}

DWORD Slot() noexcept {
    auto slot = tlsSlot.load(std::memory_order_acquire);
    if (slot != Unset) return slot;
    const DWORD error = GetLastError();
    const DWORD fls = FlsAlloc(ThreadEnded);
    DWORD tls = fls == FLS_OUT_OF_INDEXES ? TLS_OUT_OF_INDEXES : TlsAlloc();
    if (tls == TLS_OUT_OF_INDEXES || tls >= Failed) {
        if (tls != TLS_OUT_OF_INDEXES) TlsFree(tls);
        if (fls != FLS_OUT_OF_INDEXES) FlsFree(fls);
        tls = Failed;
    }
    DWORD expected = Unset;
    if (tlsSlot.compare_exchange_strong(expected, tls, std::memory_order_acq_rel)) {
        if (tls != Failed) flsSlot.store(fls, std::memory_order_release);
        SetLastError(error);
        return tls;
    }
    if (tls != Failed) {
        TlsFree(tls);
        FlsFree(fls);
    }
    SetLastError(error);
    return expected;
}

Cache* SlotValue(DWORD slot) noexcept {
    if (slot < DirectSlots) return reinterpret_cast<Cache*>(__readgsqword(TlsSlotsOffset + slot * sizeof(void*)));
    const DWORD error = GetLastError();
    auto* value = static_cast<Cache*>(TlsGetValue(slot));
    SetLastError(error);
    return value;
}

Cache* CreateCache(DWORD slot) noexcept {
    const auto fls = flsSlot.load(std::memory_order_acquire);
    if (fls == FLS_OUT_OF_INDEXES) return nullptr;
    auto* cache = static_cast<Cache*>(std::calloc(1, sizeof(Cache)));
    if (cache == nullptr) return nullptr;
    const DWORD error = GetLastError();
    if (!FlsSetValue(fls, cache) || !TlsSetValue(slot, cache)) {
        FlsSetValue(fls, nullptr);
        std::free(cache);
        cache = nullptr;
    }
    SetLastError(error);
    return cache;
}

Cache* CurrentCache() noexcept {
    const auto slot = Slot();
    if (slot == Failed) return nullptr;
    auto* cache = SlotValue(slot);
    if (cache == Closed) return nullptr;
    return cache != nullptr ? cache : CreateCache(slot);
}

bool Keep(Cache* cache, FreeBlock* block, std::size_t held) noexcept {
    const auto sizeClass = (held - 1) / Granule;
    if (cache->counts[sizeClass] == Depth(sizeClass) && KeepList(sizeClass, cache->heads[sizeClass])) {
        cache->heads[sizeClass] = nullptr;
        cache->counts[sizeClass] = 0;
    }
    if (cache->counts[sizeClass] == Depth(sizeClass)) return false;
    block->link = reinterpret_cast<std::uintptr_t>(cache->heads[sizeClass]) | ((held - 1) & HeldBits);
    cache->heads[sizeClass] = block;
    ++cache->counts[sizeClass];
    return true;
}

void* TryAllocate(std::size_t bytes) noexcept {
    if (bytes == 0) bytes = 1;
    if (bytes <= LargestCached) {
        const auto sizeClass = (bytes - 1) / Granule;
        if (auto* cache = CurrentCache()) {
            if (cache->heads[sizeClass] == nullptr) {
                if (FreeBlock* list = TakeList(sizeClass)) {
                    cache->heads[sizeClass] = list;
                    cache->counts[sizeClass] = Depth(sizeClass);
                }
            }
            if (FreeBlock* block = cache->heads[sizeClass]) {
                FreeBlock* next = Next(block);
                cache->heads[sizeClass] = next;
                --cache->counts[sizeClass];
                if (next != nullptr) _mm_prefetch(reinterpret_cast<const char*>(next), _MM_HINT_T0);
                if (Held(block, sizeClass) >= bytes || _msize(block) >= bytes) return block;
                if (sizeClass == 0 || !Keep(cache, block, sizeClass * Granule)) std::free(block);
            }
        }
        return std::malloc((sizeClass + 1) * Granule);
    }
    return std::malloc(bytes);
}

void* Allocate(std::size_t bytes) {
    for (;;) {
        if (void* block = TryAllocate(bytes)) return block;
        const auto handler = std::get_new_handler();
        if (handler == nullptr) throw std::bad_alloc();
        handler();
    }
}

void Release(void* block, std::size_t bytes = 0) noexcept {
    if (block == nullptr) return;
    if ((reinterpret_cast<std::uintptr_t>(block) & HeldBits) == 0) {
        if (auto* cache = CurrentCache()) {
            const auto held = bytes >= SmallestKept ? bytes : _msize(block);
            if (held >= SmallestKept && held < LargestCached + Granule
                && Keep(cache, static_cast<FreeBlock*>(block), std::min(held, LargestCached)))
                return;
        }
    }
    std::free(block);
}

}

void* operator new(std::size_t bytes) { return Allocate(bytes); }
void* operator new[](std::size_t bytes) { return Allocate(bytes); }
void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
    try {
        return Allocate(bytes);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t bytes, const std::nothrow_t&) noexcept {
    try {
        return Allocate(bytes);
    } catch (...) {
        return nullptr;
    }
}
void operator delete(void* block) noexcept { Release(block); }
void operator delete[](void* block) noexcept { Release(block); }
void operator delete(void* block, std::size_t bytes) noexcept { Release(block, bytes); }
void operator delete[](void* block, std::size_t bytes) noexcept { Release(block, bytes); }
void operator delete(void* block, const std::nothrow_t&) noexcept { Release(block); }
void operator delete[](void* block, const std::nothrow_t&) noexcept { Release(block); }

#endif
