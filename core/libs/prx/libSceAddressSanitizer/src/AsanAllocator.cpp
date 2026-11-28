/* Derived from LLVM compiler-rt lib/asan (asan_allocator.cpp, asan_allocator.h, asan_errors.cpp, asan_descriptions.cpp, asan_report.cpp, asan_stats.cpp, asan_debugging.cpp) at llvmorg-16.0.6 (af2ed9f794c422923414d162dd1f48d986e2d6e3) */
/* Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceAddressSanitizer/src/AsanRuntime.hpp"
#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/MallocManagedSize.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace Asan::Runtime {

namespace {

constexpr uptr MaxAllowedMallocSize = uptr{1} << 40;
constexpr uptr DefaultQuarantineBytes = uptr{256} << 20;
constexpr uptr ChunkSearchDistance = 4096;
constexpr int ErrnoEnomem = 12;
constexpr int ErrnoEinval = 22;

enum class ChunkState : u8 {
    Allocated,
    Quarantine,
};

struct Chunk {
    uptr allocBegin = 0;
    uptr allocEnd = 0;
    uptr userBegin = 0;
    uptr size = 0;
    uptr userAlignment = 0;
    ChunkState state = ChunkState::Allocated;
    std::uint32_t allocStack = 0;
    std::uint32_t freeStack = 0;
    int allocTid = -1;
    int freeTid = -1;
    bool freed = false;

    uptr End() const { return userBegin + size; }
};

struct Stats {
    uptr mallocs = 0;
    uptr malloced = 0;
    uptr mallocedRedzones = 0;
    uptr frees = 0;
    uptr freed = 0;
    uptr realFrees = 0;
    uptr reallyFreed = 0;
    uptr reallocs = 0;
    uptr realloced = 0;
    uptr mmaped = 0;
    uptr munmaped = 0;
    uptr mmaps = 0;
    uptr munmaps = 0;
    uptr mallocLarge = 0;
    uptr maxMalloced = 0;
    uptr maxSystem = 0;
    uptr inUse = 0;
    uptr maxInUse = 0;
};

std::recursive_mutex heapMutex;
std::map<uptr, Chunk> chunks;
std::deque<uptr> quarantine;
uptr quarantineBytes = 0;
Stats stats;

uptr QuarantineLimit() {
    const int megabytes = GetFlags().quarantineSizeMb;
    return megabytes < 0 ? DefaultQuarantineBytes : static_cast<uptr>(megabytes) << 20;
}

uptr MallocLimit() {
    const int megabytes = GetFlags().maxAllocationSizeMb;
    return megabytes ? std::min(MaxAllowedMallocSize, static_cast<uptr>(megabytes) << 20) : MaxAllowedMallocSize;
}

bool IsPowerOfTwo(uptr value) {
    return value != 0 && (value & (value - 1)) == 0;
}

uptr Log2(uptr value) {
    uptr log = 0;
    while ((uptr{1} << (log + 1)) <= value) ++log;
    return log;
}

uptr ComputeRzLog(uptr size) {
    const uptr log = size <= 64 - 16 ? 0
        : size <= 128 - 32 ? 1
        : size <= 512 - 64 ? 2
        : size <= 4096 - 128 ? 3
        : size <= (uptr{1} << 14) - 256 ? 4
        : size <= (uptr{1} << 15) - 512 ? 5
        : size <= (uptr{1} << 16) - 1024 ? 6
        : 7;
    const uptr minLog = Log2(static_cast<uptr>(GetFlags().redzone)) - 4;
    const uptr maxLog = Log2(static_cast<uptr>(GetFlags().maxRedzone)) - 4;
    return std::min(std::max(log, minLog), maxLog);
}

uptr UserRequestedAlignment(uptr alignment) {
    if (alignment < 8) return 0;
    return std::min<uptr>(alignment, 512);
}

Chunk* ChunkContaining(uptr address) {
    auto found = chunks.upper_bound(address);
    if (found == chunks.begin()) return nullptr;
    --found;
    if (address >= found->second.allocEnd) return nullptr;
    return &found->second;
}

Chunk* ChooseChunk(uptr address, Chunk* left, Chunk* right) {
    if (left == nullptr) return right;
    if (right == nullptr) return left;
    if (left->state != right->state) {
        if (left->state == ChunkState::Allocated) return left;
        if (right->state == ChunkState::Allocated) return right;
    }
    const sptr leftOffset = static_cast<sptr>(address - left->End());
    const sptr rightOffset = static_cast<sptr>(right->userBegin - address);
    return leftOffset < rightOffset ? left : right;
}

Chunk* FindHeapChunkByAddress(uptr address) {
    Chunk* first = ChunkContaining(address);
    if (first == nullptr || address < first->userBegin) {
        Chunk* second = nullptr;
        const uptr probe = first != nullptr ? first->allocBegin - 1 : address - 1;
        if (address - probe < ChunkSearchDistance) second = ChunkContaining(probe);
        if (second != nullptr && address + 1 > second->End()) first = ChooseChunk(address, second, first);
    }
    return first;
}

uptr AllocationSize(uptr pointer) {
    Chunk* const chunk = ChunkContaining(pointer);
    if (chunk == nullptr || chunk->state != ChunkState::Allocated || chunk->userBegin != pointer) return 0;
    return chunk->size;
}

void HintAllocatorCannotReturnNull(ScopedErrorReport& report) {
    report.Append(ReportLine("HINT: if you don't care about these errors you may set allocator_may_return_null=1\n"));
}

[[noreturn]] void FatalAllocatorReport(const std::string& line, const char* bugType, const CallSite& site) {
    ScopedErrorReport report(true);
    report.Record(CurrentError{true, false, site.here, site.bp, 0, 0, 0, 0, bugType});
    report.Append(ReportLine(line));
    const Trace trace = MallocTrace(site.here, site.caller, site.bp);
    report.Append(RenderStack(trace));
    HintAllocatorCannotReturnNull(report);
    report.Append(SummaryFor(bugType, trace));
    report.Finish();
    Die();
}

void ReportFreeError(uptr pointer, bool doubleFree, const CallSite& site) {
    ScopedErrorReport report(false);
    const char* const bugType = doubleFree ? "double-free" : "bad-free";
    report.Record(CurrentError{true, false, site.here, site.bp, 0, pointer, 0, 0, bugType});
    const std::string thread = ThreadIdAndName(CurrentThreadId());
    if (doubleFree) report.Append(ReportLine(Format("ERROR: AddressSanitizer: attempting double-free on %s in thread %s:\n", Pointer(pointer).c_str(), thread.c_str())));
    else report.Append(ReportLine(Format("ERROR: AddressSanitizer: attempting free on address which was not malloc()-ed: %s in thread %s\n", Pointer(pointer).c_str(), thread.c_str())));
    const Trace trace = UnwindFrom(site.here, site.bp, StackTraceMax);
    report.Append(RenderStack(trace));
    report.Append(DescribeAccessedAddress(pointer, 1));
    report.Append(SummaryFor(bugType, trace));
    report.Finish();
}

void ReportNotOwned(uptr pointer, const char* function, uptr pc, uptr bp) {
    ScopedErrorReport report(false);
    report.Record(CurrentError{true, false, pc, bp, 0, pointer, 0, 0, "bad-malloc_usable_size"});
    report.Append(ReportLine(Format("ERROR: AddressSanitizer: attempting to call %s for pointer which is not owned: %s\n", function, Pointer(pointer).c_str())));
    const Trace trace = UnwindFrom(pc, bp, StackTraceMax);
    report.Append(RenderStack(trace));
    report.Append(DescribeAccessedAddress(pointer, 1));
    report.Append(SummaryFor(std::strcmp(function, "malloc_usable_size()") == 0 ? "bad-malloc_usable_size" : "bad-__sanitizer_get_allocated_size", trace));
    report.Finish();
}

void SetErrno(int value) {
    *__error_nid_postfix() = value;
}

void* SetErrnoOnNull(void* pointer) {
    if (pointer == nullptr) SetErrno(ErrnoEnomem);
    return pointer;
}

void Recycle(Chunk& chunk) {
    stats.realFrees += 1;
    stats.reallyFreed += chunk.size;
    stats.munmaped += chunk.allocEnd - chunk.allocBegin;
    stats.munmaps += 1;
    PoisonShadow(chunk.allocBegin, chunk.allocEnd - chunk.allocBegin, 0);
    void* const block = reinterpret_cast<void*>(chunk.allocBegin);
    chunks.erase(chunk.allocBegin);
    GuestHeap::GuestHeapFree_nid_postfix(block);
}

void DrainQuarantine(uptr limit) {
    while (quarantineBytes > limit && !quarantine.empty()) {
        const uptr begin = quarantine.front();
        quarantine.pop_front();
        auto found = chunks.find(begin);
        if (found == chunks.end()) continue;
        quarantineBytes -= found->second.size;
        Recycle(found->second);
    }
}

void* Allocate(uptr size, uptr alignment, const CallSite& site, bool canFill) {
    EnsureShadow();
    const Flags& flags = GetFlags();
    const uptr requestedAlignment = UserRequestedAlignment(alignment);
    if (alignment < Granularity) alignment = Granularity;
    if (size == 0) size = 1;
    Require(IsPowerOfTwo(alignment), "guest AddressSanitizer: allocation alignment is not a power of two");
    const uptr rzSize = uptr{16} << ComputeRzLog(size);
    const uptr roundedSize = RoundUp(size, alignment);
    uptr neededSize = roundedSize + 2 * rzSize;
    if (alignment > Granularity) neededSize += alignment;
    const uptr limit = MallocLimit();
    if (size > MaxAllowedMallocSize || neededSize > MaxAllowedMallocSize || size > limit) {
        if (flags.allocatorMayReturnNull) {
            Emit(ReportLine(Format("WARNING: AddressSanitizer failed to allocate 0x%zx bytes\n", static_cast<std::size_t>(size))));
            return nullptr;
        }
        FatalAllocatorReport(Format("ERROR: AddressSanitizer: requested allocation size 0x%zx (0x%zx after adjustments for alignment, red zones etc.) exceeds maximum supported size of 0x%zx (thread %s)\n",
                                    static_cast<std::size_t>(size), static_cast<std::size_t>(neededSize), static_cast<std::size_t>(limit), ThreadIdAndName(CurrentThreadId()).c_str()),
                             "allocation-size-too-big", site);
    }
    void* block = nullptr;
    try {
        block = GuestHeap::GuestHeapAlign_nid_postfix(Granularity, neededSize);
    } catch (const std::bad_alloc&) {
        block = nullptr;
    } catch (const std::length_error&) {
        block = nullptr;
    }
    if (block == nullptr) {
        if (flags.allocatorMayReturnNull) return nullptr;
        ScopedErrorReport report(true);
        report.Record(CurrentError{true, false, site.here, site.bp, 0, 0, 0, 0, "out-of-memory"});
        report.Append(ReportLine(Format("ERROR: AddressSanitizer: out of memory: allocator is trying to allocate 0x%zx bytes\n", static_cast<std::size_t>(size))));
        const Trace trace = MallocTrace(site.here, site.caller, site.bp);
        report.Append(RenderStack(trace));
        HintAllocatorCannotReturnNull(report);
        report.Append(SummaryFor("out-of-memory", trace));
        report.Finish();
        Die();
    }
    const uptr allocBegin = reinterpret_cast<uptr>(block);
    const uptr allocEnd = allocBegin + neededSize;
    const uptr userBegin = RoundUp(allocBegin + rzSize, alignment);
    Require(userBegin + size <= allocEnd, "guest AddressSanitizer: allocation layout overflowed its block");
    if (flags.poisonHeap) PoisonShadow(allocBegin, neededSize, HeapLeftRedzoneMagic);
    const uptr roundedDown = RoundDown(size, Granularity);
    if (roundedDown) PoisonShadow(userBegin, roundedDown, 0);
    if (size != roundedDown) *ShadowOf(userBegin + roundedDown) = flags.poisonPartial ? static_cast<u8>(size & (Granularity - 1)) : 0;
    if (!flags.poisonHeap) PoisonShadow(allocBegin, neededSize, 0);
    if (canFill && flags.maxMallocFillSize > 0)
        std::memset(reinterpret_cast<void*>(userBegin), flags.mallocFillByte, std::min<uptr>(size, static_cast<uptr>(flags.maxMallocFillSize)));
    std::lock_guard lock(heapMutex);
    Chunk chunk;
    chunk.allocBegin = allocBegin;
    chunk.allocEnd = allocEnd;
    chunk.userBegin = userBegin;
    chunk.size = size;
    chunk.userAlignment = requestedAlignment;
    chunk.allocStack = StackDepotPut(MallocTrace(site.here, site.caller, site.bp));
    chunk.allocTid = CurrentThreadId();
    chunks[allocBegin] = chunk;
    stats.mallocs += 1;
    stats.malloced += size;
    stats.mallocedRedzones += neededSize - size;
    stats.mmaped += neededSize;
    stats.mmaps += 1;
    if (neededSize > (uptr{1} << 17)) stats.mallocLarge += 1;
    stats.inUse += size;
    stats.maxInUse = std::max(stats.maxInUse, stats.inUse);
    stats.maxSystem = std::max(stats.maxSystem, stats.mmaped - stats.munmaped);
    return reinterpret_cast<void*>(userBegin);
}

void Deallocate(void* pointer, const CallSite& site) {
    const uptr address = reinterpret_cast<uptr>(pointer);
    if (address == 0) return;
    EnsureShadow();
    std::unique_lock lock(heapMutex);
    Chunk* const chunk = ChunkContaining(address);
    if (chunk == nullptr || chunk->userBegin != address) {
        lock.unlock();
        ReportFreeError(address, false, site);
        return;
    }
    if (chunk->state != ChunkState::Allocated) {
        lock.unlock();
        ReportFreeError(address, true, site);
        return;
    }
    chunk->state = ChunkState::Quarantine;
    chunk->freeStack = StackDepotPut(MallocTrace(site.here, site.caller, site.bp));
    chunk->freeTid = CurrentThreadId();
    chunk->freed = true;
    const Flags& flags = GetFlags();
    if (flags.maxFreeFillSize > 0) std::memset(pointer, flags.freeFillByte, std::min<uptr>(chunk->size, static_cast<uptr>(flags.maxFreeFillSize)));
    if (flags.poisonHeap) PoisonShadow(chunk->userBegin, RoundUp(chunk->size, Granularity), HeapFreeMagic);
    stats.frees += 1;
    stats.freed += chunk->size;
    stats.inUse -= chunk->size;
    quarantine.push_back(chunk->allocBegin);
    quarantineBytes += chunk->size;
    DrainQuarantine(QuarantineLimit());
}

void* Reallocate(void* oldPointer, uptr newSize, uptr alignment, const CallSite& site) {
    const uptr address = reinterpret_cast<uptr>(oldPointer);
    {
        std::lock_guard lock(heapMutex);
        stats.reallocs += 1;
        stats.realloced += newSize;
    }
    void* const newPointer = Allocate(newSize, alignment, site, true);
    if (newPointer == nullptr) return nullptr;
    uptr copySize = 0;
    bool valid = false;
    bool quarantined = false;
    {
        std::lock_guard lock(heapMutex);
        Chunk* const chunk = ChunkContaining(address);
        if (chunk != nullptr && chunk->userBegin == address) {
            valid = chunk->state == ChunkState::Allocated;
            quarantined = chunk->state == ChunkState::Quarantine;
            copySize = std::min(newSize, chunk->size);
        }
    }
    if (!valid) ReportFreeError(address, quarantined, site);
    std::memcpy(newPointer, oldPointer, copySize);
    Deallocate(oldPointer, site);
    return newPointer;
}

void* Realloc(void* pointer, uptr size, uptr alignment, const CallSite& site) {
    if (pointer == nullptr) return SetErrnoOnNull(Allocate(size, alignment, site, true));
    if (size == 0) {
        if (GetFlags().allocatorFreesAndReturnsNullOnReallocZero) {
            Deallocate(pointer, site);
            return nullptr;
        }
        size = 1;
    }
    return SetErrnoOnNull(Reallocate(pointer, size, alignment, site));
}

std::string HeapChunkAccess(uptr address, uptr accessSize, const Chunk& chunk) {
    uptr badAddress = address;
    std::string where;
    if (address < chunk.userBegin) {
        where = Format("%s is located %zd bytes before", Pointer(badAddress).c_str(), static_cast<std::ptrdiff_t>(chunk.userBegin - address));
    } else if (address + accessSize > chunk.End()) {
        sptr offset = static_cast<sptr>(address - chunk.End());
        if (offset < 0) {
            badAddress -= static_cast<uptr>(offset);
            offset = 0;
        }
        where = Format("%s is located %zd bytes after", Pointer(badAddress).c_str(), static_cast<std::ptrdiff_t>(offset));
    } else {
        where = Format("%s is located %zd bytes inside of", Pointer(badAddress).c_str(), static_cast<std::ptrdiff_t>(address - chunk.userBegin));
    }
    return where + Format(" %zu-byte region [%s,%s)\n", static_cast<std::size_t>(chunk.size), Pointer(chunk.userBegin).c_str(), Pointer(chunk.End()).c_str());
}

}

bool DescribeHeapAddress(uptr address, uptr accessSize, std::string* text) {
    std::lock_guard lock(heapMutex);
    const Chunk* const chunk = FindHeapChunkByAddress(address);
    if (chunk == nullptr) return false;
    *text = HeapChunkAccess(address, accessSize, *chunk);
    if (chunk->freed) {
        *text += "freed by thread " + ThreadIdAndName(chunk->freeTid) + " here:\n" + RenderStack(StackDepotGet(chunk->freeStack));
        *text += "previously allocated by thread " + ThreadIdAndName(chunk->allocTid) + " here:\n";
    } else {
        *text += "allocated by thread " + ThreadIdAndName(chunk->allocTid) + " here:\n";
    }
    *text += RenderStack(StackDepotGet(chunk->allocStack));
    return true;
}

bool HeapRegionForAddress(uptr address, uptr* begin, uptr* size) {
    std::lock_guard lock(heapMutex);
    const Chunk* const chunk = FindHeapChunkByAddress(address);
    if (chunk == nullptr) return false;
    *begin = chunk->userBegin;
    *size = chunk->size;
    return true;
}

}

using namespace Asan::Runtime;

#define APS5_ASAN_CALL_SITE                                                                  \
    const uptr pc = reinterpret_cast<uptr>(__builtin_return_address(0));                     \
    const CallSite site{CurrentPc(), pc, reinterpret_cast<uptr>(__builtin_frame_address(0))}

namespace {

uptr HeapStack(uptr address, uptr* trace, uptr size, std::uint32_t* threadId, bool allocation) {
    Trace stack;
    int tid = -1;
    {
        std::lock_guard lock(heapMutex);
        const Chunk* const chunk = FindHeapChunkByAddress(address);
        if (chunk == nullptr) return 0;
        if (allocation) {
            if (chunk->allocStack == 0) return 0;
            stack = StackDepotGet(chunk->allocStack);
            tid = chunk->allocTid;
        } else {
            if (!chunk->freed) return 0;
            stack = StackDepotGet(chunk->freeStack);
            tid = chunk->freeTid;
        }
    }
    if (threadId != nullptr) {
        const auto value = static_cast<std::uint32_t>(tid);
        if (!GuestCopyOut_nid_no_patch(threadId, &value, sizeof(value))) throw std::invalid_argument("guest AddressSanitizer: thread id output is not writable");
    }
    if (trace == nullptr || size == 0) return 0;
    const std::size_t count = std::min<std::size_t>({static_cast<std::size_t>(size), stack.size(), StackTraceMax});
    std::vector<uptr> frames(count);
    for (std::size_t index = 0; index < count; ++index) frames[index] = PreviousInstructionPc(stack[index]);
    if (count && !GuestCopyOut_nid_no_patch(trace, frames.data(), count * sizeof(uptr))) throw std::invalid_argument("guest AddressSanitizer: trace output is not writable");
    return count;
}

}

extern "C" {

[[gnu::noinline]] void* APS5_VABI __asan_malloc(std::size_t size) {
    APS5_ASAN_CALL_SITE;
    return SetErrnoOnNull(Allocate(size, 8, site, true));
}

[[gnu::noinline]] void APS5_VABI __asan_free(void* pointer) {
    APS5_ASAN_CALL_SITE;
    Deallocate(pointer, site);
}

[[gnu::noinline]] void* APS5_VABI __asan_calloc(std::size_t count, std::size_t size) {
    APS5_ASAN_CALL_SITE;
    if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) {
        if (GetFlags().allocatorMayReturnNull) {
            SetErrno(ErrnoEnomem);
            return nullptr;
        }
        EnsureShadow();
        FatalAllocatorReport(Format("ERROR: AddressSanitizer: calloc parameters overflow: count * size (%zd * %zd) cannot be represented in type size_t (thread %s)\n",
                                    static_cast<std::ptrdiff_t>(count), static_cast<std::ptrdiff_t>(size), ThreadIdAndName(CurrentThreadId()).c_str()),
                             "calloc-overflow", site);
    }
    void* const pointer = Allocate(count * size, 8, site, false);
    if (pointer != nullptr) std::memset(pointer, 0, count * size);
    return SetErrnoOnNull(pointer);
}

[[gnu::noinline]] void* APS5_VABI __asan_realloc(void* pointer, std::size_t size) {
    APS5_ASAN_CALL_SITE;
    return Realloc(pointer, size, 8, site);
}

[[gnu::noinline]] void* APS5_VABI __asan_memalign(std::size_t alignment, std::size_t size) {
    APS5_ASAN_CALL_SITE;
    if (!IsPowerOfTwo(alignment)) {
        SetErrno(ErrnoEinval);
        if (GetFlags().allocatorMayReturnNull) return nullptr;
        EnsureShadow();
        FatalAllocatorReport(Format("ERROR: AddressSanitizer: invalid allocation alignment: %zd, alignment must be a power of two (thread %s)\n", static_cast<std::ptrdiff_t>(alignment), ThreadIdAndName(CurrentThreadId()).c_str()),
                             "invalid-allocation-alignment", site);
    }
    return SetErrnoOnNull(Allocate(size, alignment, site, true));
}

[[gnu::noinline]] void* APS5_VABI __asan_aligned_alloc(std::size_t alignment, std::size_t size) {
    APS5_ASAN_CALL_SITE;
    if (!IsPowerOfTwo(alignment) || (size & (alignment - 1)) != 0) {
        SetErrno(ErrnoEinval);
        if (GetFlags().allocatorMayReturnNull) return nullptr;
        EnsureShadow();
        FatalAllocatorReport(Format("ERROR: AddressSanitizer: invalid alignment requested in aligned_alloc: %zd, alignment must be a power of two and the requested size 0x%zx must be a multiple of alignment (thread %s)\n",
                                    static_cast<std::ptrdiff_t>(alignment), static_cast<std::size_t>(size), ThreadIdAndName(CurrentThreadId()).c_str()),
                             "invalid-aligned-alloc-alignment", site);
    }
    return SetErrnoOnNull(Allocate(size, alignment, site, true));
}

[[gnu::noinline]] int APS5_VABI __asan_posix_memalign(void** pointer, std::size_t alignment, std::size_t size) {
    APS5_ASAN_CALL_SITE;
    if (!IsPowerOfTwo(alignment) || alignment % sizeof(void*) != 0) {
        if (GetFlags().allocatorMayReturnNull) return ErrnoEinval;
        EnsureShadow();
        FatalAllocatorReport(Format("ERROR: AddressSanitizer: invalid alignment requested in posix_memalign: %zd, alignment must be a power of two and a multiple of sizeof(void*) == %zd (thread %s)\n",
                                    static_cast<std::ptrdiff_t>(alignment), static_cast<std::ptrdiff_t>(sizeof(void*)), ThreadIdAndName(CurrentThreadId()).c_str()),
                             "invalid-posix-memalign-alignment", site);
    }
    void* const allocated = Allocate(size, alignment, site, true);
    if (allocated == nullptr) return ErrnoEnomem;
    if (!GuestCopyOut_nid_no_patch(pointer, &allocated, sizeof(allocated))) throw std::invalid_argument("guest AddressSanitizer: posix_memalign output is not writable");
    return 0;
}

[[gnu::noinline]] void* APS5_VABI __asan_reallocalign(void* pointer, std::size_t size, std::size_t alignment) {
    APS5_ASAN_CALL_SITE;
    if (!IsPowerOfTwo(alignment)) {
        SetErrno(ErrnoEinval);
        if (GetFlags().allocatorMayReturnNull) return nullptr;
        EnsureShadow();
        FatalAllocatorReport(Format("ERROR: AddressSanitizer: invalid allocation alignment: %zd, alignment must be a power of two (thread %s)\n", static_cast<std::ptrdiff_t>(alignment), ThreadIdAndName(CurrentThreadId()).c_str()),
                             "invalid-allocation-alignment", site);
    }
    return Realloc(pointer, size, alignment, site);
}

[[gnu::noinline]] std::size_t APS5_VABI __asan_malloc_usable_size(const void* pointer) {
    APS5_ASAN_CALL_SITE;
    if (pointer == nullptr) return 0;
    EnsureShadow();
    uptr size = 0;
    {
        std::lock_guard lock(heapMutex);
        size = AllocationSize(reinterpret_cast<uptr>(pointer));
    }
    if (GetFlags().checkMallocUsableSize && size == 0) ReportNotOwned(reinterpret_cast<uptr>(pointer), "malloc_usable_size()", pc, site.bp);
    return size;
}

int APS5_VABI __asan_malloc_stats(void* block) {
    Require(block != nullptr, "guest AddressSanitizer: malloc_stats with a null block");
    MallocManagedSize managed{};
    if (!GuestCopyIn_nid_no_patch(&managed, block, sizeof(managed))) throw std::invalid_argument("guest AddressSanitizer: malloc_stats block is not readable");
    if (managed.size != sizeof(MallocManagedSize) || managed.version != 1) return 1;
    {
        std::lock_guard lock(heapMutex);
        managed.maxSystemSize = stats.maxSystem;
        managed.currentSystemSize = stats.mmaped - stats.munmaped;
        managed.maxInuseSize = stats.maxInUse;
        managed.currentInuseSize = stats.inUse;
    }
    if (!GuestCopyOut_nid_no_patch(block, &managed, sizeof(managed))) throw std::invalid_argument("guest AddressSanitizer: malloc_stats block is not writable");
    return 0;
}

void APS5_VABI __asan_print_accumulated_stats() {
    Stats snapshot;
    {
        std::lock_guard lock(heapMutex);
        stats.maxMalloced = std::max(stats.maxMalloced, stats.malloced);
        snapshot = stats;
    }
    std::string text = Format("Stats: %zuM malloced (%zuM for red zones) by %zu calls\n", static_cast<std::size_t>(snapshot.malloced >> 20), static_cast<std::size_t>(snapshot.mallocedRedzones >> 20),
                              static_cast<std::size_t>(snapshot.mallocs));
    text += Format("Stats: %zuM realloced by %zu calls\n", static_cast<std::size_t>(snapshot.realloced >> 20), static_cast<std::size_t>(snapshot.reallocs));
    text += Format("Stats: %zuM freed by %zu calls\n", static_cast<std::size_t>(snapshot.freed >> 20), static_cast<std::size_t>(snapshot.frees));
    text += Format("Stats: %zuM really freed by %zu calls\n", static_cast<std::size_t>(snapshot.reallyFreed >> 20), static_cast<std::size_t>(snapshot.realFrees));
    text += Format("Stats: %zuM (%zuM-%zuM) mmaped; %zu maps, %zu unmaps\n", static_cast<std::size_t>((snapshot.mmaped - snapshot.munmaped) >> 20), static_cast<std::size_t>(snapshot.mmaped >> 20),
                   static_cast<std::size_t>(snapshot.munmaped >> 20), static_cast<std::size_t>(snapshot.mmaps), static_cast<std::size_t>(snapshot.munmaps));
    text += Format("Stats: malloc large: %zu\n", static_cast<std::size_t>(snapshot.mallocLarge));
    Emit(text);
}

std::size_t APS5_VABI __sanitizer_get_estimated_allocated_size(std::size_t size) {
    return size;
}

int APS5_VABI __sanitizer_get_ownership(const void* pointer) {
    std::lock_guard lock(heapMutex);
    return AllocationSize(reinterpret_cast<uptr>(pointer)) > 0 ? 1 : 0;
}

[[gnu::noinline]] std::size_t APS5_VABI __sanitizer_get_allocated_size(const void* pointer) {
    APS5_ASAN_CALL_SITE;
    if (pointer == nullptr) return 0;
    uptr size = 0;
    {
        std::lock_guard lock(heapMutex);
        size = AllocationSize(reinterpret_cast<uptr>(pointer));
    }
    if (size == 0) {
        EnsureShadow();
        ReportNotOwned(reinterpret_cast<uptr>(pointer), "__sanitizer_get_allocated_size()", site.here, site.bp);
    }
    return size;
}

std::size_t APS5_VABI __sanitizer_get_allocated_size_fast(const void* pointer) {
    std::lock_guard lock(heapMutex);
    const uptr size = AllocationSize(reinterpret_cast<uptr>(pointer));
    Require(size != 0, "guest AddressSanitizer: __sanitizer_get_allocated_size_fast of a pointer the allocator does not own");
    return size;
}

const void* APS5_VABI __sanitizer_get_allocated_begin(const void* pointer) {
    std::lock_guard lock(heapMutex);
    const uptr address = reinterpret_cast<uptr>(pointer);
    const Chunk* const chunk = ChunkContaining(address);
    if (chunk == nullptr || chunk->state != ChunkState::Allocated || address < chunk->userBegin || address >= chunk->End()) return nullptr;
    return reinterpret_cast<const void*>(chunk->userBegin);
}

std::size_t APS5_VABI __sanitizer_get_current_allocated_bytes() {
    std::lock_guard lock(heapMutex);
    return stats.malloced > stats.freed ? stats.malloced - stats.freed : 1;
}

std::size_t APS5_VABI __sanitizer_get_heap_size() {
    std::lock_guard lock(heapMutex);
    return stats.mmaped - stats.munmaped;
}

std::size_t APS5_VABI __sanitizer_get_free_bytes() {
    std::lock_guard lock(heapMutex);
    const uptr totalFree = stats.mmaped - stats.munmaped + stats.reallyFreed;
    const uptr totalUsed = stats.malloced + stats.mallocedRedzones;
    return totalFree > totalUsed ? totalFree - totalUsed : 1;
}

std::size_t APS5_VABI __sanitizer_get_unmapped_bytes() {
    return 0;
}

void APS5_VABI __sanitizer_purge_allocator() {
    std::lock_guard lock(heapMutex);
    DrainQuarantine(0);
}

[[gnu::noinline]] int APS5_VABI __asan_update_allocation_context(void* address) {
    APS5_ASAN_CALL_SITE;
    std::lock_guard lock(heapMutex);
    Chunk* const chunk = ChunkContaining(reinterpret_cast<uptr>(address));
    if (chunk == nullptr || chunk->state != ChunkState::Allocated) return 0;
    chunk->allocStack = StackDepotPut(MallocTrace(site.here, site.caller, site.bp));
    chunk->allocTid = CurrentThreadId();
    return 1;
}

std::uintptr_t APS5_VABI __asan_get_alloc_stack(std::uintptr_t address, std::uintptr_t* trace, std::uintptr_t size, std::uint32_t* threadId) {
    return HeapStack(address, trace, size, threadId, true);
}

std::uintptr_t APS5_VABI __asan_get_free_stack(std::uintptr_t address, std::uintptr_t* trace, std::uintptr_t size, std::uint32_t* threadId) {
    return HeapStack(address, trace, size, threadId, false);
}

const char* APS5_VABI __asan_locate_address(std::uintptr_t address, char* name, std::uintptr_t nameSize, std::uintptr_t* regionAddress, std::uintptr_t* regionSize) {
    EnsureShadow();
    if (name != nullptr && nameSize > 0) {
        const char empty = '\0';
        if (!GuestCopyOut_nid_no_patch(name, &empty, 1)) throw std::invalid_argument("guest AddressSanitizer: name buffer is not writable");
    }
    uptr begin = 0;
    uptr size = 0;
    const char* kind = "heap-invalid";
    std::string globalName(nameSize > 0 ? nameSize : 1, '\0');
    if (address >= ShadowBegin && address < ShadowEnd) kind = "high shadow";
    else if (HeapRegionForAddress(address, &begin, &size)) kind = "heap";
    else if (StackRegionForAddress(address, name, nameSize, &begin, &size)) kind = "stack";
    else if (GlobalRegionForAddress(address, globalName.data(), globalName.size(), &begin, &size)) {
        kind = "global";
        if (name != nullptr && nameSize > 0 && !GuestCopyOut_nid_no_patch(name, globalName.data(), std::strlen(globalName.c_str()) + 1))
            throw std::invalid_argument("guest AddressSanitizer: name buffer is not writable");
    }
    if (regionAddress != nullptr && !GuestCopyOut_nid_no_patch(regionAddress, &begin, sizeof(begin))) throw std::invalid_argument("guest AddressSanitizer: region address output is not writable");
    if (regionSize != nullptr && !GuestCopyOut_nid_no_patch(regionSize, &size, sizeof(size))) throw std::invalid_argument("guest AddressSanitizer: region size output is not writable");
    return kind;
}

}
