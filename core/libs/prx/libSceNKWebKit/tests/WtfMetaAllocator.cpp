#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using NKWebKit::WtfAbi::MetaAllocator;
using NKWebKit::WtfAbi::MetaAllocatorHandle;
using NKWebKit::WtfAbi::MetaAllocatorHandleRef;
using NKWebKit::WtfAbi::MetaAllocatorStatistics;
using NKWebKit::WtfAbi::MetaAllocatorTracker;
using NKWebKit::WtfAbi::MetaAllocatorVtable;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WtfMetaAllocator check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static int guestAllocations;

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations++;
    return pointer;
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) guestAllocations--;
    std::free(pointer);
}

static void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
static void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
static void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
static void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
static int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

struct TranscriptRandom {
    std::uint64_t state;
    std::uint64_t Next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
    std::uint64_t Below(std::uint64_t bound) {
        return Next() % bound;
    }
};

struct Transcript {
    std::string text;
    TranscriptRandom random;
    std::size_t pageSize = 4096;
    std::uintptr_t bump = 0x100000000ull;

    void Line(const char* format, ...) {
        char buffer[160];
        va_list args;
        va_start(args, format);
        std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        text += buffer;
        text += '\n';
    }

    std::uintptr_t NewSpace(std::size_t& numPages) {
        Line("new %zu", numPages);
        if (!random.Below(8)) return 0;
        if (!random.Below(4)) numPages += random.Below(4);
        std::uintptr_t result = bump;
        bump += numPages * pageSize;
        if (!random.Below(3)) bump += pageSize * (1 + random.Below(2));
        return result;
    }

    void NeedPage(void* page, std::size_t count) {
        Line("need %llx %zu", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(page)), count);
    }

    void PageIsFree(void* page, std::size_t count) {
        Line("free %llx %zu", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(page)), count);
    }
};

template<typename Adapter>
static std::string RunTranscript(std::uint64_t seed, std::size_t granule, std::size_t pageSize, bool tracked, unsigned steps) {
    Transcript transcript;
    transcript.random.state = seed * 0x9E3779B97F4A7C15ull + 1;
    transcript.pageSize = pageSize;
    Adapter adapter(transcript, granule, pageSize, tracked);
    std::vector<typename Adapter::Handle> live;
    for (unsigned step = 0; step < steps; ++step) {
        std::uint64_t op = transcript.random.Below(100);
        if (op < 45) {
            std::size_t size = transcript.random.Below(4) ? 1 + transcript.random.Below(pageSize * 2) : transcript.random.Below(pageSize * 6);
            auto handle = adapter.Allocate(size);
            if (adapter.Valid(handle)) {
                transcript.Line("alloc %zu -> %llx %zu", size, static_cast<unsigned long long>(adapter.Start(handle)), adapter.Size(handle));
                live.push_back(std::move(handle));
            } else
                transcript.Line("alloc %zu -> null", size);
        } else if (op < 75) {
            if (live.empty()) continue;
            std::size_t index = transcript.random.Below(live.size());
            transcript.Line("release %llx", static_cast<unsigned long long>(adapter.Start(live[index])));
            adapter.Release(live[index]);
            live.erase(live.begin() + index);
        } else if (op < 85) {
            if (live.empty()) continue;
            std::size_t index = transcript.random.Below(live.size());
            std::size_t newSize = transcript.random.Below(adapter.Size(live[index]) + 1);
            adapter.Shrink(live[index], newSize);
            transcript.Line("shrink %llx %zu -> %zu", static_cast<unsigned long long>(adapter.Start(live[index])), newSize, adapter.Size(live[index]));
        } else if (op < 90) {
            std::size_t pages = 1 + transcript.random.Below(4);
            std::uintptr_t start = transcript.bump + (transcript.random.Below(2) ? 0 : granule * transcript.random.Below(4));
            transcript.bump = start + pages * pageSize;
            adapter.AddFresh(start, pages * pageSize - (start % pageSize));
            transcript.Line("fresh %llx", static_cast<unsigned long long>(start));
        } else if (op < 95) {
            std::uintptr_t address = 0x100000000ull + transcript.random.Below(transcript.bump - 0x100000000ull + pageSize);
            transcript.Line("in %llx %d %llx", static_cast<unsigned long long>(address), adapter.IsIn(address) ? 1 : 0, static_cast<unsigned long long>(adapter.TrackerFind(address)));
        } else {
            std::size_t allocated = 0, reserved = 0, committed = 0;
            adapter.Stats(allocated, reserved, committed);
            transcript.Line("stats %zu %zu %zu", allocated, reserved, committed);
        }
    }
    while (!live.empty()) {
        transcript.Line("release %llx", static_cast<unsigned long long>(adapter.Start(live.back())));
        adapter.Release(live.back());
        live.pop_back();
    }
    std::size_t allocated = 0, reserved = 0, committed = 0;
    adapter.Stats(allocated, reserved, committed);
    transcript.Line("final %zu %zu %zu", allocated, reserved, committed);
    return std::move(transcript.text);
}

static std::uint64_t TranscriptHash(const std::string& text) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (unsigned char c : text) {
        hash ^= c;
        hash *= 0x100000001b3ull;
    }
    return hash;
}

struct GuestAllocator {
    MetaAllocator base;
    Transcript* transcript;
    int destroyed;
};

static GuestAllocator& Owner(MetaAllocator* self) {
    return *reinterpret_cast<GuestAllocator*>(self);
}

static void APS5_VABI GuestD1(MetaAllocator* self) {
    Owner(self).destroyed++;
    _ZN3WTF13MetaAllocatorD2Ev(self);
}

static void APS5_VABI GuestD0(MetaAllocator* self) {
    GuestD1(self);
}

static std::uintptr_t APS5_VABI GuestAllocateNewSpace(MetaAllocator* self, std::size_t& numPages) {
    return Owner(self).transcript->NewSpace(numPages);
}

static void APS5_VABI GuestNotifyNeedPage(MetaAllocator* self, void* page, std::size_t count) {
    Owner(self).transcript->NeedPage(page, count);
}

static void APS5_VABI GuestNotifyPageIsFree(MetaAllocator* self, void* page, std::size_t count) {
    Owner(self).transcript->PageIsFree(page, count);
}

static const MetaAllocatorVtable GuestVtable{
    GuestD1,
    GuestD0,
    GuestAllocateNewSpace,
    GuestNotifyNeedPage,
    GuestNotifyPageIsFree,
    _ZN3WTF13MetaAllocator7releaseERKNS_6LockerINS_4LockEEERNS_19MetaAllocatorHandleE,
};

static std::uint8_t lockByte;
static const void* const lockHolder = &lockByte;

struct GuestAdapter {
    using Handle = MetaAllocatorHandleRef;
    MetaAllocatorTracker tracker{};
    GuestAllocator allocator{};

    GuestAdapter(Transcript& transcript, std::size_t granule, std::size_t pageSize, bool tracked) {
        _ZN3WTF13MetaAllocatorC2ERNS_4LockEmm(&allocator.base, &lockByte, granule, pageSize);
        allocator.base.vptr = &GuestVtable;
        allocator.transcript = &transcript;
        if (tracked) allocator.base.tracker = &tracker;
    }

    ~GuestAdapter() {
        allocator.base.vptr->destructor(&allocator.base);
    }

    Handle Allocate(std::size_t size) {
        lockByte = 1;
        Handle handle = _ZN3WTF13MetaAllocator8allocateERKNS_6LockerINS_4LockEEEm(&allocator.base, lockHolder, size);
        lockByte = 0;
        return handle;
    }

    bool Valid(const Handle& handle) {
        return handle.ptr != nullptr;
    }

    std::uintptr_t Start(const Handle& handle) {
        return handle.ptr->start;
    }

    std::size_t Size(const Handle& handle) {
        return handle.ptr->sizeInBytes();
    }

    void Release(Handle& handle) {
        Handle released = std::move(handle);
    }

    void Shrink(Handle& handle, std::size_t newSize) {
        _ZN3WTF19MetaAllocatorHandle6shrinkEm(handle.ptr, newSize);
    }

    void AddFresh(std::uintptr_t start, std::size_t size) {
        _ZN3WTF13MetaAllocator17addFreshFreeSpaceEPvm(&allocator.base, reinterpret_cast<void*>(start), size);
    }

    bool IsIn(std::uintptr_t address) {
        return _ZN3WTF13MetaAllocator19isInAllocatedMemoryERKNS_14AbstractLockerEPv(&allocator.base, lockHolder, reinterpret_cast<void*>(address));
    }

    std::uintptr_t TrackerFind(std::uintptr_t address) {
        MetaAllocatorHandle* handle = tracker.allocations.findGreatestLessThanOrEqual(reinterpret_cast<void*>(address));
        if (handle && handle->start <= address && address < handle->end) return handle->start;
        return 0;
    }

    void Stats(std::size_t& allocated, std::size_t& reserved, std::size_t& committed) {
        MetaAllocatorStatistics statistics = _ZN3WTF13MetaAllocator17currentStatisticsERKNS_6LockerINS_4LockEEE(&allocator.base, lockHolder);
        allocated = statistics.bytesAllocated;
        reserved = statistics.bytesReserved;
        committed = statistics.bytesCommitted;
    }
};

struct OracleCase {
    std::uint64_t seed;
    std::size_t granule;
    std::size_t pageSize;
    bool tracked;
    std::uint64_t hash;
};

static const OracleCase oracleCases[] = {
    {1, 32, 4096, true, 0xb07aadd49ba3ffccull},
    {2, 256, 4096, false, 0x5a8d38883ef71d62ull},
    {3, 16, 16384, true, 0x218f9e0bfa283035ull},
    {4, 32, 16384, false, 0xcd403ad85adcbc9eull},
    {5, 256, 16384, true, 0xa9efe91b0a341bd7ull},
    {6, 16, 4096, false, 0xc3fedce4ead3f809ull},
    {7, 32, 4096, true, 0x635bd517c2abfc1bull},
    {8, 256, 4096, false, 0xbc2bb1db51c66504ull},
    {9, 16, 16384, true, 0xdfbca1570864937full},
    {10, 32, 16384, false, 0x8f7a5b8690d9d7ccull},
    {11, 256, 16384, true, 0xbdcc9069c32071a2ull},
    {12, 16, 4096, false, 0x32ec6f0ba4a7e5f1ull},
    {13, 32, 4096, true, 0x54c76e33f14b0e9dull},
    {14, 256, 4096, false, 0x5ff5556d5ae02d14ull},
    {15, 16, 16384, true, 0x96592c3982702ce6ull},
    {16, 32, 16384, false, 0x8c60dc322da757f6ull},
    {17, 256, 16384, true, 0x0126999523adc03eull},
    {18, 16, 4096, false, 0x6825d15d00e45971ull},
    {19, 32, 4096, true, 0xb0981bec8183c4c4ull},
    {20, 256, 4096, false, 0xa9afc0f14c5e51c9ull},
    {21, 16, 16384, true, 0x9d409abef111be1aull},
    {22, 32, 16384, false, 0x2d7d03b4adb4bc7eull},
    {23, 256, 16384, true, 0xfd1ba3cc3f958f13ull},
    {24, 16, 4096, false, 0x565c280a21a2edd3ull},
};

static void TestLayout() {
    MetaAllocatorHandle handle{};
    auto* base = reinterpret_cast<char*>(&handle);
    Require(reinterpret_cast<char*>(&handle.refCount) - base == 0);
    Require(reinterpret_cast<char*>(&handle.m_left) - base == 8);
    Require(reinterpret_cast<char*>(&handle.m_parentAndRed) - base == 24);
    Require(reinterpret_cast<char*>(&handle.allocator) - base == 32);
    Require(reinterpret_cast<char*>(&handle.start) - base == 40);
    Require(reinterpret_cast<char*>(&handle.end) - base == 48);
}

static void TestOracleTranscripts() {
    for (const OracleCase& oracleCase : oracleCases) {
        std::string text = RunTranscript<GuestAdapter>(oracleCase.seed, oracleCase.granule, oracleCase.pageSize, oracleCase.tracked, 600);
        if (TranscriptHash(text) != oracleCase.hash) {
            std::fprintf(stderr, "seed %llu transcript differs:\n%s", static_cast<unsigned long long>(oracleCase.seed), text.c_str());
            Require(false);
        }
    }
}

static void TestCoalescingAndPlacement() {
    Transcript transcript;
    transcript.random.state = 3;
    GuestAdapter adapter(transcript, 16, 4096, false);
    adapter.AddFresh(0x200000000ull, 0x3000);
    Require(_ZN3WTF13MetaAllocator18debugFreeSpaceSizeEv(&adapter.allocator.base) == 0x3000);

    auto first = adapter.Allocate(100);
    Require(first.ptr->start == 0x200000000ull && first.ptr->sizeInBytes() == 112);
    Require(first.ptr->refCount == 1 && first.ptr->allocator == &adapter.allocator.base);
    auto second = adapter.Allocate(0x1000);
    Require(second.ptr->start == 0x200002000ull && second.ptr->sizeInBytes() == 0x1000);
    Require(transcript.text == "need 200000000 1\nneed 200002000 1\n");
    Require(_ZN3WTF13MetaAllocator18debugFreeSpaceSizeEv(&adapter.allocator.base) == 0x3000 - 112 - 0x1000);

    std::size_t allocated = 0, reserved = 0, committed = 0;
    adapter.Stats(allocated, reserved, committed);
    Require(allocated == 112 + 0x1000 && reserved == 0x3000 && committed == 0x2000);
    Require(adapter.IsIn(0x200000fffull) && !adapter.IsIn(0x200001000ull) && adapter.IsIn(0x200002000ull));

    _ZN3WTF19MetaAllocatorHandle6shrinkEm(second.ptr, 1);
    Require(second.ptr->sizeInBytes() == 16);
    adapter.Release(first);
    adapter.Release(second);
    Require(transcript.text == "need 200000000 1\nneed 200002000 1\nfree 200000000 1\nfree 200002000 1\n");
    Require(adapter.allocator.base.freeSpaceSizeMap.m_root->sizeInBytes() == 0x3000);
    Require(!adapter.allocator.base.freeSpaceSizeMap.m_root->m_left && !adapter.allocator.base.freeSpaceSizeMap.m_root->m_right);
    adapter.Stats(allocated, reserved, committed);
    Require(allocated == 0 && reserved == 0x3000 && committed == 0);
}

static void TestNewSpaceFailureAndZeroSize() {
    Transcript transcript;
    transcript.random.state = 1;
    GuestAdapter adapter(transcript, 32, 4096, false);
    Require(!adapter.Allocate(0).ptr);
    Require(transcript.text.empty());
    std::vector<MetaAllocatorHandleRef> handles;
    for (;;) {
        transcript.text.clear();
        handles.push_back(adapter.Allocate(4096));
        if (!handles.back().ptr) break;
    }
    Require(transcript.text == "new 1\n");
}

static void TestDestructorFreesNodesAndRestoresVtable() {
    Transcript transcript;
    transcript.random.state = 5;
    alignas(GuestAdapter) unsigned char storage[sizeof(GuestAdapter)];
    auto* adapter = new (storage) GuestAdapter(transcript, 16, 4096, true);
    adapter->AddFresh(0x300000000ull, 0x1000);
    adapter->AddFresh(0x300004000ull, 0x2000);
    {
        auto handle = adapter->Allocate(32);
        Require(adapter->TrackerFind(handle.ptr->start + 31) == handle.ptr->start);
        Require(adapter->TrackerFind(handle.ptr->start + 32) == 0);
    }
    Require(!adapter->tracker.allocations.m_root);
    MetaAllocator* base = &adapter->allocator.base;
    adapter->~GuestAdapter();
    Require(adapter->allocator.destroyed == 1);
    Require(base->freeSpaceStartAddressMap == nullptr && base->pageOccupancyMap == nullptr);
    Require(base->vptr != &GuestVtable && base->vptr->release == GuestVtable.release);
}

static void TestDeletingDestructor() {
    void* storage = _Znwm_nid_postfix(sizeof(MetaAllocator));
    auto* allocator = static_cast<MetaAllocator*>(storage);
    _ZN3WTF13MetaAllocatorC2ERNS_4LockEmm(allocator, &lockByte, 16, 4096);
    Require(allocator->logAllocationGranule == 4 && allocator->logPageSize == 12);
    _ZN3WTF13MetaAllocator17addFreshFreeSpaceEPvm(allocator, reinterpret_cast<void*>(0x400000000ull), 0x1000);
    Require(guestAllocations == 1);
    allocator->vptr->deletingDestructor(allocator);
    Require(guestAllocations == 0);
}

static void TestNullObjectsThrow() {
    Require(ThrowsInvalidArgument([] { _ZN3WTF13MetaAllocatorC2ERNS_4LockEmm(nullptr, &lockByte, 16, 4096); }));
    Require(ThrowsInvalidArgument([] { _ZN3WTF13MetaAllocator8allocateERKNS_6LockerINS_4LockEEEm(nullptr, lockHolder, 16); }));
    Require(ThrowsInvalidArgument([] { _ZN3WTF19MetaAllocatorHandle6shrinkEm(nullptr, 0); }));
    Require(ThrowsInvalidArgument([] { _ZN3WTF19MetaAllocatorHandleD1Ev(nullptr); }));
}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestLayout();
    TestOracleTranscripts();
    TestCoalescingAndPlacement();
    TestNewSpaceFailureAndZeroSize();
    TestDestructorFreesNodesAndRestoresVtable();
    TestDeletingDestructor();
    TestNullObjectsThrow();
    Require(lockByte == 0);
    return 0;
}
