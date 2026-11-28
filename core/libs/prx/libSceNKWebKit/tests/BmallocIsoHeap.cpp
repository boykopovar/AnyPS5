#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>

namespace {

using NKWebKit::BmallocAbi::AllIsoHeaps;
using NKWebKit::BmallocAbi::IsoHeapImplBase;
using NKWebKit::BmallocAbi::IsoTLSEntry;
using NKWebKit::BmallocAbi::IsoTLSLayout;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "bmalloc iso heap check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename Exception, typename Action> bool Throws(Action action) {
    try {
        action();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

namespace IsoHeapDriver {

struct Hash {
    std::uint64_t value = 1469598103934665603ull;
    void Mix(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            value ^= (v >> (8 * i)) & 0xff;
            value *= 1099511628211ull;
        }
    }
};

struct Random {
    std::uint32_t state;
    std::uint32_t operator()(std::uint32_t bound) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state % bound;
    }
    std::uint64_t Wide() {
        std::uint64_t high = (*this)(0x10000);
        std::uint64_t middle = (*this)(0x10000);
        std::uint64_t low = (*this)(0x10000);
        return high << 48 | middle << 32 | low << 16 | (*this)(0x10000);
    }
};

constexpr int HeapCount = 4;
constexpr int EntryCount = 96;

inline std::uint64_t Bytes(Random& random) {
    switch (random(5)) {
    case 0: return random(4096);
    case 1: return static_cast<std::uint64_t>(random(64)) * 16384;
    case 2: return random.Wide();
    case 3: return ~static_cast<std::uint64_t>(random(100));
    default: return 0;
    }
}

inline std::uint64_t EntrySize(Random& random, bool small) {
    if (small) return random(3) ? random(4096) : random(1 << 20);
    switch (random(4)) {
    case 0: return random(4096);
    case 1: return 0xFFFFFFFFull - random(16);
    case 2: return random.Wide();
    default: return (static_cast<std::uint64_t>(random(16)) << 32) | random(4096);
    }
}

template<typename Adapter> std::uint64_t Run(Adapter& adapter, std::uint32_t seed, int count) {
    Hash hash;
    Random random{seed ? seed : 1};
    adapter.Reset();
    for (int heap = 0; heap < HeapCount; ++heap) {
        const unsigned char* bytes = adapter.HeapBytes(heap);
        for (int offset = 16; offset < 106; ++offset) hash.Mix(static_cast<std::uint64_t>(offset) << 8 | bytes[offset]);
    }
    bool heapAdded[HeapCount] = {};
    int entries = 0;
    for (int i = 0; i < count; ++i) {
        const std::uint32_t operation = random(8);
        hash.Mix(operation);
        if (operation < 4) {
            const int heap = static_cast<int>(random(HeapCount));
            adapter.Counter(static_cast<int>(operation), heap, Bytes(random));
            hash.Mix(adapter.Footprint(heap));
            hash.Mix(adapter.Freeable(heap));
        } else if (operation == 4) {
            const int heap = static_cast<int>(random(HeapCount));
            if (!heapAdded[heap]) {
                heapAdded[heap] = true;
                adapter.AddHeap(heap);
            }
            for (int index = adapter.HeadHeap(), steps = 0; index >= 0 && steps < HeapCount; index = adapter.NextHeap(index), ++steps) hash.Mix(static_cast<std::uint64_t>(index));
        } else if ((operation == 5 || operation == 6) && entries < EntryCount) {
            const bool layout = operation == 6;
            const int entry = entries++;
            adapter.NewEntry(entry, EntrySize(random, layout));
            const unsigned char* bytes = adapter.EntryBytes(entry);
            for (int offset = 8; offset < 24; ++offset) hash.Mix(static_cast<std::uint64_t>(offset) << 8 | bytes[offset]);
            if (layout) {
                adapter.AddEntry(entry);
                hash.Mix(adapter.EntryOffset(entry));
                hash.Mix(static_cast<std::uint64_t>(adapter.LayoutHead()) << 32 | static_cast<std::uint32_t>(adapter.LayoutTail()));
            }
            hash.Mix(adapter.Extent(entry));
        } else if (entries) {
            const int entry = static_cast<int>(random(static_cast<std::uint32_t>(entries)));
            hash.Mix(adapter.Extent(entry));
            hash.Mix(adapter.EntryOffset(entry));
        }
    }
    for (int entry = adapter.LayoutHead(); entry >= 0; entry = adapter.NextEntry(entry)) hash.Mix(static_cast<std::uint64_t>(entry));
    return hash.value;
}

}  // namespace IsoHeapDriver

struct Adapter {
    unsigned char lock[64]{};
    alignas(16) unsigned char heapStorage[IsoHeapDriver::HeapCount][sizeof(IsoHeapImplBase)];
    alignas(16) unsigned char entryStorage[IsoHeapDriver::EntryCount][sizeof(IsoTLSEntry)];
    AllIsoHeaps heaps{};
    IsoTLSLayout layout{};
    IsoHeapImplBase* Heap(int i) { return reinterpret_cast<IsoHeapImplBase*>(heapStorage[i]); }
    IsoTLSEntry* Entry(int i) { return reinterpret_cast<IsoTLSEntry*>(entryStorage[i]); }
    void Reset() {
        std::memset(heapStorage, 0xCD, sizeof(heapStorage));
        std::memset(entryStorage, 0xCD, sizeof(entryStorage));
        for (int i = 0; i < IsoHeapDriver::HeapCount; ++i) _ZN7bmalloc15IsoHeapImplBaseC2ERNS_5MutexE(Heap(i), lock);
        heaps = {};
        layout = {};
    }
    const unsigned char* HeapBytes(int i) { return heapStorage[i]; }
    void Counter(int op, int heap, std::uint64_t bytes) {
        switch (op) {
        case 0:
            _ZN7bmalloc15IsoHeapImplBase9didCommitEPvm(Heap(heap), nullptr, bytes);
            break;
        case 1:
            _ZN7bmalloc15IsoHeapImplBase11didDecommitEPvm(Heap(heap), nullptr, bytes);
            break;
        case 2:
            _ZN7bmalloc15IsoHeapImplBase13isNowFreeableEPvm(Heap(heap), nullptr, bytes);
            break;
        default:
            _ZN7bmalloc15IsoHeapImplBase18isNoLongerFreeableEPvm(Heap(heap), nullptr, bytes);
            break;
        }
    }
    std::uint64_t Footprint(int heap) { return _ZN7bmalloc15IsoHeapImplBase9footprintEv(Heap(heap)); }
    std::uint64_t Freeable(int heap) { return _ZN7bmalloc15IsoHeapImplBase14freeableMemoryEv(Heap(heap)); }
    int IndexOf(const IsoHeapImplBase* heap) {
        for (int i = 0; i < IsoHeapDriver::HeapCount; ++i)
            if (heap == Heap(i)) return i;
        return -1;
    }
    int IndexOf(const IsoTLSEntry* entry) {
        for (int i = 0; i < IsoHeapDriver::EntryCount; ++i)
            if (entry == Entry(i)) return i;
        return -1;
    }
    void AddHeap(int heap) { _ZN7bmalloc11AllIsoHeaps3addEPNS_15IsoHeapImplBaseE(&heaps, Heap(heap)); }
    int HeadHeap() { return IndexOf(_ZN7bmalloc11AllIsoHeaps4headEv(&heaps)); }
    int NextHeap(int heap) { return IndexOf(Heap(heap)->next); }
    void NewEntry(int entry, std::uint64_t size) { _ZN7bmalloc11IsoTLSEntryC2Em(Entry(entry), size); }
    const unsigned char* EntryBytes(int entry) { return entryStorage[entry]; }
    void AddEntry(int entry) { _ZN7bmalloc12IsoTLSLayout3addEPNS_11IsoTLSEntryE(&layout, Entry(entry)); }
    std::uint64_t EntryOffset(int entry) { return _ZNK7bmalloc11IsoTLSEntry6offsetEv(Entry(entry)); }
    std::uint64_t Extent(int entry) { return _ZNK7bmalloc11IsoTLSEntry6extentEv(Entry(entry)); }
    int LayoutHead() { return IndexOf(layout.head); }
    int LayoutTail() { return IndexOf(layout.tail); }
    int NextEntry(int entry) { return IndexOf(Entry(entry)->next); }
};

const std::uint64_t ExpectedHashes[] = {
    0xd45724f78d644d2bull,
    0x28f8c572825b4b96ull,
    0xe7185fe17d346d81ull,
    0x160492b12f710242ull,
    0x01793d0dc1dd1457ull,
    0x34c75d47725c0b48ull,
    0x1f63a62f4bd55652ull,
    0x56bf289fecee89cdull,
    0x6e8696390e8f522full,
    0x9d85b6dd55f553b7ull,
    0x72a0e4808ec4d709ull,
    0xc5e1c3f50fd43f22ull,
    0x52596c628722735full,
    0x16681cc6d7bc33f6ull,
    0x6deb50e8c431553full,
    0x7342cef560ad47caull,
};

void TestOracleTranscripts() {
    for (std::size_t seed = 0; seed < std::size(ExpectedHashes); ++seed) {
        static Adapter adapter;
        Require(IsoHeapDriver::Run(adapter, static_cast<std::uint32_t>(seed + 1), 1000) == ExpectedHashes[seed]);
    }
}

using Slot = void (APS5_VABI*)(void*);
using ScavengeSlot = void (APS5_VABI*)(void*, void*);

void TestVtablesAndDeletingDestructors() {
    unsigned char lock[8]{};
    void* heapMemory = _ZN7bmalloc15IsoHeapImplBasenwEm(sizeof(IsoHeapImplBase));
    auto* heap = static_cast<IsoHeapImplBase*>(heapMemory);
    _ZN7bmalloc15IsoHeapImplBaseC2ERNS_5MutexE(heap, lock);
    Require(heap->lock == lock && heap->availableShared == 0xFF && heap->isInlineDirectoryEligibleOrDecommitted);
    Require(Throws<std::logic_error>([&] { reinterpret_cast<ScavengeSlot>(const_cast<void*>(heap->vptr[2]))(heap, nullptr); }));
    reinterpret_cast<Slot>(const_cast<void*>(heap->vptr[0]))(heap);
    reinterpret_cast<Slot>(const_cast<void*>(heap->vptr[1]))(heap);

    void* entryMemory = _ZN7bmalloc11IsoTLSEntrynwEm(sizeof(IsoTLSEntry));
    auto* entry = static_cast<IsoTLSEntry*>(entryMemory);
    _ZN7bmalloc11IsoTLSEntryC2Em(entry, 0x1'0000'0010ull);
    Require(!entry->next && entry->offset == UINT_MAX && entry->size == 0x10);
    Require(_ZNK7bmalloc11IsoTLSEntry4sizeEv(entry) == 0x10);
    Require(_ZNK7bmalloc11IsoTLSEntry6extentEv(entry) == 0xF);
    for (int slot = 2; slot < 6; ++slot) Require(Throws<std::logic_error>([&] { reinterpret_cast<Slot>(const_cast<void*>(entry->vptr[slot]))(entry); }));
    _ZN7bmalloc11IsoTLSEntryD0Ev(entry);
}

void TestLayoutAssertions() {
    alignas(16) unsigned char storage[3][sizeof(IsoTLSEntry)];
    auto* first = reinterpret_cast<IsoTLSEntry*>(storage[0]);
    auto* second = reinterpret_cast<IsoTLSEntry*>(storage[1]);
    auto* third = reinterpret_cast<IsoTLSEntry*>(storage[2]);
    _ZN7bmalloc11IsoTLSEntryC2Em(first, 12);
    _ZN7bmalloc11IsoTLSEntryC2Em(second, 0xFFFFFFF0u);
    _ZN7bmalloc11IsoTLSEntryC2Em(third, 4);
    IsoTLSLayout layout{};
    _ZN7bmalloc12IsoTLSLayout3addEPNS_11IsoTLSEntryE(&layout, first);
    _ZN7bmalloc12IsoTLSLayout3addEPNS_11IsoTLSEntryE(&layout, second);
    Require(first->offset == 0 && second->offset == 16 && first->next == second && layout.head == first && layout.tail == second);
    Require(Throws<std::logic_error>([&] { _ZN7bmalloc12IsoTLSLayout3addEPNS_11IsoTLSEntryE(&layout, first); }));
    second->size = UINT_MAX - 16;
    second->offset = 16;
    Require(_ZNK7bmalloc11IsoTLSEntry6extentEv(second) == UINT_MAX);
    Require(Throws<std::logic_error>([&] { _ZN7bmalloc12IsoTLSLayout3addEPNS_11IsoTLSEntryE(&layout, third); }));
    Require(third->offset == UINT_MAX && layout.tail == second);
    IsoTLSLayout broken{first, nullptr};
    first->next = nullptr;
    Require(Throws<std::logic_error>([&] { _ZN7bmalloc12IsoTLSLayout3addEPNS_11IsoTLSEntryE(&broken, third); }));
    broken = {nullptr, first};
    Require(Throws<std::logic_error>([&] { _ZN7bmalloc12IsoTLSLayout3addEPNS_11IsoTLSEntryE(&broken, third); }));
}

void TestAddToAllIsoHeaps() {
    unsigned char lock[8]{};
    alignas(16) static unsigned char storage[2][sizeof(IsoHeapImplBase)];
    auto* first = reinterpret_cast<IsoHeapImplBase*>(storage[0]);
    auto* second = reinterpret_cast<IsoHeapImplBase*>(storage[1]);
    _ZN7bmalloc15IsoHeapImplBaseC2ERNS_5MutexE(first, lock);
    _ZN7bmalloc15IsoHeapImplBaseC2ERNS_5MutexE(second, lock);
    AllIsoHeaps* host = BmallocHostAllIsoHeaps_nid_no_patch();
    IsoHeapImplBase* before = _ZN7bmalloc11AllIsoHeaps4headEv(host);
    _ZN7bmalloc15IsoHeapImplBase16addToAllIsoHeapsEv(first);
    _ZN7bmalloc15IsoHeapImplBase16addToAllIsoHeapsEv(second);
    Require(_ZN7bmalloc11AllIsoHeaps4headEv(host) == second && second->next == first && first->next == before);
}

void TestNullArguments() {
    unsigned char lock[8]{};
    alignas(16) unsigned char storage[sizeof(IsoHeapImplBase)];
    std::memset(storage, 0xCD, sizeof(storage));
    auto* heap = reinterpret_cast<IsoHeapImplBase*>(storage);
    IsoTLSLayout layout{};
    AllIsoHeaps heaps{};
    Require(Throws<std::invalid_argument>([&] { _ZN7bmalloc15IsoHeapImplBaseC2ERNS_5MutexE(heap, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7bmalloc15IsoHeapImplBaseC2ERNS_5MutexE(nullptr, lock); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7bmalloc15IsoHeapImplBase9didCommitEPvm(nullptr, nullptr, 1); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7bmalloc15IsoHeapImplBase16addToAllIsoHeapsEv(nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7bmalloc11AllIsoHeaps3addEPNS_15IsoHeapImplBaseE(&heaps, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7bmalloc11AllIsoHeaps4headEv(nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7bmalloc11IsoTLSEntryC2Em(nullptr, 1); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7bmalloc11IsoTLSEntry6extentEv(nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7bmalloc12IsoTLSLayout3addEPNS_11IsoTLSEntryE(&layout, nullptr); }));
    for (unsigned char byte : storage) Require(byte == 0xCD);
    Require(!heaps.head && !layout.head);
}

}  // namespace

int main() {
    TestOracleTranscripts();
    TestVtablesAndDeletingDestructors();
    TestLayoutAssertions();
    TestAddToAllIsoHeaps();
    TestNullArguments();
    std::puts("bmalloc iso heap tests passed");
    return 0;
}
