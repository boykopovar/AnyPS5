#include "prx/libSceNpCommon/include/Api.hpp"
#include "SceTypes.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int NpIpcAlreadyInitialized = static_cast<int>(0x80559D00);
constexpr int NpIpcNotInitialized = static_cast<int>(0x80559D01);
constexpr int NpOutOfMemory = static_cast<int>(0x80550005);
constexpr int NpNullObject = static_cast<int>(0x80558005);
constexpr int NpNotInitialized = static_cast<int>(0x80558001);
constexpr int NpAlreadyInitialized = static_cast<int>(0x80558002);
constexpr int NpAlreadyQueued = static_cast<int>(0x80558006);
constexpr int NpUtilInvalid = static_cast<int>(0x80550601);
constexpr int NpUtilBufferTooSmall = static_cast<int>(0x80550602);
constexpr int NpUtilOutOfRange = static_cast<int>(0x8055060A);
constexpr int NpUtilInvalidCharacter = static_cast<int>(0x80550608);
constexpr int NpInvalidFormat = static_cast<int>(0x80558003);
constexpr int NpInvalidCharacter = static_cast<int>(0x80558004);
constexpr int NpBufferTooSmall = static_cast<int>(0x80558008);
constexpr int NpOutOfRange = static_cast<int>(0x8055800E);
constexpr int NpInvalidArgument = static_cast<int>(0x80558009);
constexpr int NpEventFlagBusy = static_cast<int>(0x8055800C);
constexpr int NpSemaBusy = static_cast<int>(0x8055800D);
constexpr int NpStreamBufferTooSmall = static_cast<int>(0x80559E0D);
constexpr int NpTitleIdInvalid = static_cast<int>(0x80559E11);
constexpr int NpUtilParseOutOfRange = static_cast<int>(0x8055060A);
constexpr int NpTitleIdBufferTooSmall = static_cast<int>(0x80559E0D);
constexpr int NpStreamShort = static_cast<int>(0x80559E10);
constexpr int KernelNotPermitted = static_cast<int>(0x80020001);
constexpr int KernelNoSuchObject = static_cast<int>(0x80020003);
constexpr int NpJsonTooDeep = static_cast<int>(0x80550603);
constexpr int NpServerErrorBadCode = static_cast<int>(0x80559C02);
constexpr std::uint32_t EventFlagWaitAnd = 1;
constexpr std::uint32_t EventFlagShared = 0x100;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void* APS5_VABI ReturnArgument(void* arg) {
    Pthread self {};
    Require(sceNpThreadGetId(&self) == 0 && self != nullptr, "a created thread can read its id");
    return arg;
}

void TestAtomics() {
    std::int32_t value = 5;
    Require(sceNpAtomicInc32(&value) == 5 && value == 6, "increment returns the previous value");
    Require(sceNpAtomicDec32(&value) == 6 && value == 5, "decrement returns the previous value");
    Require(sceNpAtomicCas32(&value, 4, 9) == 5 && value == 5, "a failed compare-and-swap returns the current value");
    Require(sceNpAtomicCas32(&value, 5, 9) == 5 && value == 9, "a successful compare-and-swap returns the previous value");
}

void TestSemaphoresAndFlags() {
    KernelSema sema {};
    Require(sceNpCreateSema(&sema, 0, 0, 2, "NpSema") == 0, "a semaphore is created");
    Require(sceNpPollSema(sema) == NpSemaBusy, "an empty semaphore reports the busy error");
    Require(sceNpSignalSema(sema) == 0 && sceNpPollSema(sema) == 0, "a signalled semaphore can be taken once");
    Require(sceNpDeleteSema(sema) == 0, "the semaphore is deleted");

    KernelEventFlag flag {};
    Require(sceNpCreateEventFlag(&flag, "NpFlag", 0, 0) == 0, "an event flag is created");
    std::uint64_t result = 0;
    Require(sceNpPollEventFlag(flag, 1, EventFlagWaitAnd, &result) == NpEventFlagBusy, "a clear event flag reports the busy error");
    Require(sceNpSetEventFlag(flag, 1) == 0 && sceNpPollEventFlag(flag, 1, EventFlagWaitAnd, &result) == 0 && result == 1, "a set bit is polled");
    Require(sceNpDeleteEventFlag(flag) == 0, "the event flag is deleted");
}

void TestEventFlagClearAndCancel() {
    KernelEventFlag flag {};
    Require(sceNpCreateEventFlag(&flag, "NpClearFlag", 0, 0) == 0, "an event flag is created");
    std::uint64_t result = 0;
    Require(sceNpSetEventFlag(flag, 0b111) == 0 && sceNpClearEventFlag(flag, 0b101) == 0, "the pattern is set and masked");
    Require(sceNpPollEventFlag(flag, 0b010, EventFlagWaitAnd, &result) == NpEventFlagBusy, "a clear keeps only the bits of its pattern");
    Require(sceNpPollEventFlag(flag, 0b101, EventFlagWaitAnd, &result) == 0 && result == 0b101, "a clear leaves the masked bits set");
    int waiting = -1;
    Require(sceNpCancelEventFlag(flag, 0b1000, &waiting) == 0 && waiting == 0, "a cancel reports no waiting thread");
    Require(sceNpPollEventFlag(flag, 0b101, EventFlagWaitAnd, &result) == NpEventFlagBusy, "a cancel replaces the old pattern");
    Require(sceNpPollEventFlag(flag, 0b1000, EventFlagWaitAnd, &result) == 0 && result == 0b1000, "a cancel sets its pattern");
    Require(sceNpDeleteEventFlag(flag) == 0, "the event flag is deleted");
}

struct CondWaiter {
    PthreadMutex* mutex;
    PthreadCond* cond;
    int (APS5_VABI* wait)(PthreadCond*, PthreadMutex*);
    bool ready = false;
    bool released = false;
};

void* APS5_VABI WaitOnCondition(void* arg) {
    auto* waiter = static_cast<CondWaiter*>(arg);
    Require(sceNpMutexLock(waiter->mutex) == 0, "the waiter locks the mutex");
    waiter->ready = true;
    while (!waiter->released) Require(waiter->wait(waiter->cond, waiter->mutex) == 0, "the wait ends without an error");
    Require(sceNpMutexUnlock(waiter->mutex) == 0, "the waiter holds the mutex after the wait");
    return nullptr;
}

template <typename TWake>
void WakeWaiter(PthreadCond* cond, int (APS5_VABI* wait)(PthreadCond*, PthreadMutex*), TWake wake) {
    PthreadMutex mutex {};
    Require(sceNpMutexInit(&mutex, "NpCondMutex", 0) == 0, "a mutex is created");
    CondWaiter waiter {&mutex, cond, wait};
    Pthread thread {};
    Require(sceNpCreateThread(&thread, WaitOnCondition, &waiter, 0, 0x10000, 0, "NpCondWaiter") == 0, "the waiter is created");
    for (bool ready = false; !ready;) {
        Require(sceNpMutexLock(&mutex) == 0, "the main thread locks the mutex");
        ready = waiter.ready;
        if (!ready) {
            Require(sceNpMutexUnlock(&mutex) == 0, "the main thread unlocks the mutex");
            std::this_thread::yield();
        }
    }
    waiter.released = true;
    Require(wake(thread) == 0, "the waiter is signalled");
    Require(sceNpMutexUnlock(&mutex) == 0, "the main thread unlocks the mutex");
    Require(sceNpJoinThread(thread, nullptr) == 0, "the woken waiter is joined");
    Require(sceNpMutexDestroy(&mutex) == 0, "the mutex is destroyed");
}

void TestConditionWakeups() {
    PthreadCond cond {};
    Require(sceNpCondInit(&cond, "NpCond", 0) == 0, "a condition is created");
    WakeWaiter(&cond, sceNpCondWait, [&](Pthread) { return sceNpCondSignalAll(&cond); });
    WakeWaiter(&cond, sceNpCondWait, [&](Pthread thread) { return sceNpCondSignalTo(&cond, thread); });
    Require(sceNpCondDestroy(&cond) == 0, "the condition is destroyed");
    PthreadCond lightweight {};
    Require(sceNpLwCondInit(&lightweight, "NpLwCond") == 0, "a lightweight condition is created");
    WakeWaiter(&lightweight, sceNpLwCondWait, [&](Pthread) { return sceNpLwCondSignal(&lightweight); });
    WakeWaiter(&lightweight, sceNpLwCondWait, [&](Pthread) { return sceNpLwCondSignalAll(&lightweight); });
    WakeWaiter(&lightweight, sceNpLwCondWait, [&](Pthread thread) { return sceNpLwCondSignalTo(&lightweight, thread); });
    Require(sceNpLwCondDestroy(&lightweight) == 0, "the lightweight condition is destroyed");
}

void TestSharedEventFlags() {
    KernelEventFlag creator {};
    Require(sceNpCreateEventFlag(&creator, "NpSharedFlag", EventFlagShared, 0) == 0, "a shared event flag is created");
    KernelEventFlag opened {};
    Require(sceNpOpenEventFlag(&opened, "NpSharedFlag") == 0 && opened != creator, "the shared event flag is opened by name");
    std::uint64_t result = 0;
    Require(sceNpSetEventFlag(opened, 2) == 0 && sceNpPollEventFlag(creator, 2, EventFlagWaitAnd, &result) == 0 && result == 2,
        "a bit set through the opened handle is polled through the creator");
    Require(sceNpCloseEventFlag(creator) == KernelNotPermitted, "the creator handle is not closed");
    Require(sceNpCloseEventFlag(opened) == 0, "the opened handle is closed");
    Require(sceNpCloseEventFlag(opened) == KernelNoSuchObject, "a closed handle is gone");
    Require(sceNpDeleteEventFlag(creator) == 0, "the creator deletes the shared event flag");
}

void TestThreads() {
    Require(sceNpThreadGetId(nullptr) == NpInvalidArgument, "a null thread id output is refused");
    Pthread thread {};
    int marker = 0;
    Require(sceNpCreateThread(&thread, ReturnArgument, &marker, 0, 0x10000, 0, "NpThread") == 0, "a thread is created");
    void* value = nullptr;
    Require(sceNpJoinThread(thread, &value) == 0 && value == &marker, "the thread is joined with its return value");
}

void UseAllocator(const NpAllocator& allocator) {
    auto* block = static_cast<std::uint8_t*>(allocator.allocate(64, allocator.context));
    Require(block != nullptr, "the allocator hands out memory");
    block[63] = 0x5A;
    block = static_cast<std::uint8_t*>(allocator.reallocate(block, 256, allocator.context));
    Require(block != nullptr && block[63] == 0x5A, "reallocation keeps the contents");
    allocator.release(block, allocator.context);
    allocator.release(nullptr, allocator.context);
}

void TestHeaps() {
    NpMemoryHeap memoryHeap {};
    Require(sceNpMemoryHeapInit(nullptr, 0x1000, "np-test") == NpNullObject, "a null memory heap is refused");
    Require(sceNpMemoryHeapInit(&memoryHeap, 0x1001, "np-test") == 0, "a memory heap is created");
    Require(memoryHeap.memory.size == 0x4000 && memoryHeap.memory.address != nullptr && memoryHeap.heap.mspace != nullptr,
        "the memory heap size is rounded up to 16 KiB");
    NpAllocatorEx allocatorEx {};
    sceNpMemoryHeapGetAllocatorEx(&memoryHeap, 0x1234, &allocatorEx);

    Require(allocatorEx.allocator.context == &memoryHeap && allocatorEx.outOfMemoryError == 0x1234,
        "the extended allocator carries the caller's error code");
    UseAllocator(allocatorEx.allocator);
    sceNpMemoryHeapDestroy(&memoryHeap);

    Require(memoryHeap.memory.address == nullptr && memoryHeap.heap.mspace == nullptr, "destroying clears the memory heap");

    NpKernelMemory memory {};
    void* address = nullptr;
    Require(sceNpAllocateKernelMemoryWithAlignment(0x10, "np-test", &address, nullptr) == NpNullObject,
        "kernel memory needs an object");
    Require(sceNpAllocateKernelMemoryWithAlignment(0x10, "np-test", &address, &memory) == 0
            && address == memory.address && memory.size == 0x4000,
        "kernel memory is mapped in 16 KiB units");
    NpHeap heap {};
    Require(sceNpHeapInit(&heap, memory.address, memory.size, "np-test") == 0 && heap.mspace != nullptr, "a heap is created");
    NpAllocator allocator {};
    sceNpHeapGetAllocator(&heap, &allocator);

    Require(allocator.context == &heap, "the heap allocator uses the heap as its context");
    UseAllocator(allocator);
    NpHeapStat stat {};
    Require(sceNpHeapGetStat(&heap, &stat) == 0 && stat.maxInuseSize >= stat.currentInuseSize, "heap statistics are reported");
    sceNpHeapDestroy(&heap);

    Require(heap.mspace == nullptr, "destroying clears the heap");
    Require(sceNpFreeKernelMemory(&memory) == 0 && memory.address == nullptr, "kernel memory is released");
    Require(sceNpFreeKernelMemory(nullptr) == NpNullObject, "freeing needs an object");

    Require(sceNpArchInit() == 0 && sceNpArchInit() == 0, "the library initialises more than once");
    NpAllocatorEx* global = sceNpGlobalHeapGetAllocatorExPtr();
    Require(global->outOfMemoryError == NpOutOfMemory, "the global allocator reports the library's out-of-memory error");
    UseAllocator(global->allocator);
    sceNpArchTerm();

    UseAllocator(sceNpGlobalHeapGetAllocatorExPtr()->allocator);
    sceNpArchTerm();

    Require(sceNpIpc2ClientTerm() == NpIpcNotInitialized, "the ipc client must be started before it stops");
    Require(sceNpIpc2ClientInit() == 0 && sceNpIpc2ClientInit() == NpIpcAlreadyInitialized, "the ipc client starts once");
    Require(sceNpIpc2ClientTerm() == 0 && sceNpIpc2ClientInit() == 0 && sceNpIpc2ClientTerm() == 0, "the ipc client restarts");
}

struct AllocatorRecord {
    std::array<std::uint8_t, 256> block {};
    std::size_t size = 0;
    void* released = nullptr;
    bool fail = false;
};

void* APS5_VABI RecordAllocate(std::size_t size, void* context) {
    auto* record = static_cast<AllocatorRecord*>(context);
    record->size = size;
    return record->fail ? nullptr : record->block.data();
}

void APS5_VABI RecordRelease(void* pointer, void* context) {
    static_cast<AllocatorRecord*>(context)->released = pointer;
}

void TestObjects() {
    AllocatorRecord record {};
    NpAllocator allocator {RecordAllocate, nullptr, RecordRelease, &record};
    void* object = _ZN3sce2np6ObjectnwEmR14SceNpAllocator(40, &allocator);
    NpAllocator* stored = nullptr;
    std::memcpy(&stored, record.block.data(), sizeof(stored));

    Require(object == record.block.data() + 16 && record.size == 56 && stored == &allocator,
        "an object is allocated behind a 16-byte header that keeps its allocator");
    AllocatorRecord other {};
    NpAllocator otherAllocator {RecordAllocate, nullptr, RecordRelease, &other};
    _ZN3sce2np6ObjectdlEPvR14SceNpAllocator(object, &otherAllocator);

    Require(record.released == record.block.data() && other.released == nullptr, "an object is freed through the allocator it was allocated with");
    record.released = nullptr;
    _ZN3sce2np6ObjectdaEPv(_ZN3sce2np6ObjectnaEmR14SceNpAllocator(0, &allocator));

    Require(record.size == 16 && record.released == record.block.data(), "an array is allocated and freed the same way");
    NpAllocatorEx allocatorEx {allocator, 0x1234};
    record.released = nullptr;
    object = _ZN3sce2np6ObjectnwEmR16SceNpAllocatorEx(8, &allocatorEx);
    std::memcpy(&stored, record.block.data(), sizeof(stored));

    Require(object == record.block.data() + 16 && stored == &allocatorEx.allocator, "an extended allocator keeps its own address in the header");
    _ZN3sce2np6ObjectdaEPvR16SceNpAllocatorEx(object, &allocatorEx);
    Require(record.released == record.block.data(), "an object from an extended allocator is freed through it");
    record.released = nullptr;
    _ZN3sce2np6ObjectdlEPv(_ZN3sce2np6ObjectnaEmR16SceNpAllocatorEx(8, &allocatorEx));
    _ZN3sce2np6ObjectdlEPvR16SceNpAllocatorEx(nullptr, &allocatorEx);
    _ZN3sce2np6ObjectdaEPvR14SceNpAllocator(nullptr, &allocator);

    Require(record.released == record.block.data(), "freeing a null object does nothing");
    record.fail = true;
    Require(_ZN3sce2np6ObjectnwEmR14SceNpAllocator(8, &allocator) == nullptr && _ZN3sce2np6ObjectnwEmR16SceNpAllocatorEx(8, &allocatorEx) == nullptr,
        "a failed allocation returns null");
}

void TestObjectsThroughStoredAllocator() {
    AllocatorRecord record {};
    NpAllocator allocator {RecordAllocate, nullptr, RecordRelease, &record};
    NpAllocatorEx allocatorEx {allocator, 0x1234};
    AllocatorRecord other {};
    NpAllocator otherAllocator {RecordAllocate, nullptr, RecordRelease, &other};
    NpAllocatorEx otherEx {otherAllocator, 0};

    record.block.fill(0xAA);
    void* object = _ZN3sce2np6ObjectnaEmR16SceNpAllocatorEx(24, &allocatorEx);
    Require(object == record.block.data() + 16 && record.size == 40, "an extended array is allocated behind the header");
    Require(std::all_of(record.block.begin() + 8, record.block.begin() + 16, [](std::uint8_t b) { return b == 0xAA; }),
        "the second half of the header is left as the allocator returned it");
    _ZN3sce2np6ObjectdlEPvR16SceNpAllocatorEx(object, &otherEx);
    Require(record.released == record.block.data() && other.released == nullptr, "delete with an extended allocator frees through the stored one");

    record.released = nullptr;
    _ZN3sce2np6ObjectdaEPvR14SceNpAllocator(_ZN3sce2np6ObjectnwEmR14SceNpAllocator(8, &allocator), &otherAllocator);
    Require(record.released == record.block.data() && other.released == nullptr, "delete[] with an allocator frees through the stored one");

    record.released = nullptr;
    _ZN3sce2np6ObjectdaEPvR16SceNpAllocatorEx(_ZN3sce2np6ObjectnwEmR16SceNpAllocatorEx(8, &allocatorEx), &otherEx);
    Require(record.released == record.block.data() && other.released == nullptr, "delete[] with an extended allocator frees through the stored one");

    Require(_ZN3sce2np6ObjectnwEmR14SceNpAllocator(SIZE_MAX - 15, &allocator) == record.block.data() + 16 && record.size == 0,
        "the size plus the header wraps unchecked, so a succeeding allocator is asked for 0 bytes");

    record.fail = true;
    Require(_ZN3sce2np6ObjectnaEmR14SceNpAllocator(8, &allocator) == nullptr && _ZN3sce2np6ObjectnaEmR16SceNpAllocatorEx(8, &allocatorEx) == nullptr,
        "a failed array allocation returns null");
}

void TestTitleIdObjects() {
    NpTitleIdObject parsed {};
    std::memset(&parsed, 0x77, sizeof(parsed));
    Require(_ZN3sce2np9NpTitleId5ParseEPS1_PKc(&parsed, "WXYZ54321_07") == 0 && std::memcmp(parsed.titleId.data, "WXYZ54321_00\0\0\0", 16) == 0,
        "parsing keeps the nine character code and resets the suffix to _00");
    Require(_ZN3sce2np9NpTitleId5ParseEPS1_PKcm(&parsed, "WXYZ54321", 9) == NpTitleIdInvalid, "parsing needs the twelve character form");
    Require(_ZN3sce2np9NpTitleId5ParseEPS1_PKc(&parsed, "WXYZ5432a_07") == NpTitleIdInvalid
        && _ZN3sce2np9NpTitleId5ParseEPS1_PKc(&parsed, "WXY154321_07") == NpTitleIdInvalid
        && _ZN3sce2np9NpTitleId5ParseEPS1_PKc(&parsed, "WXYZ54321-07") == NpTitleIdInvalid,
        "parsing needs four letters, five digits, an underscore and two digits");
    Require(std::memcmp(parsed.titleId.data, "WXYZ54321_00", 13) == 0, "a rejected title id leaves the object as it was");

    NpTitleIdObject built {};
    _ZN3sce2np9NpTitleIdC1Ev(&built);
    Require(_ZNK3sce2np9NpTitleId7IsEmptyEv(&built) && built.vtable != nullptr, "a default title id is empty");
    Require(_ZN3sce2np9NpTitleId20ConstructFromTitleIdEPS1_PKcm(&built, "wxyz54321", 9) == 0
        && _ZN3sce2np9NpTitleId20ConstructFromTitleIdEPS1_PKc(&built, "WXYZ54321_00") == NpTitleIdInvalid,
        "constructing takes only the nine character code");
    Require(_ZN3sce2np9NpTitleId20ConstructFromTitleIdEPS1_PKc(&built, "WXYZ54321") == 0 && !_ZNK3sce2np9NpTitleId7IsEmptyEv(&built),
        "a constructed title id is not empty");

    char text[16];
    std::memset(text, 'x', sizeof(text));
    Require(_ZN3sce2np9NpTitleId5BuildERKS1_Pcm(&built, text, 12) == NpTitleIdBufferTooSmall && text[0] == 'x', "building needs 13 bytes");
    Require(_ZN3sce2np9NpTitleId5BuildERKS1_Pcm(&built, text, 14) == 0 && std::strcmp(text, "WXYZ54321_00") == 0 && text[13] == '\0' && text[14] == 'x',
        "building writes the title id and clears the rest of the buffer");

    NpTitleIdObject copy {};
    _ZN3sce2np9NpTitleIdC1ERKS1_(&copy, &parsed);
    NpTitleId plain = parsed.titleId;
    plain.data[11] = '9';
    Require(_ZNK3sce2np9NpTitleId6EqualsERKS1_(&copy, &built) && _ZN3sce2npeqERKNS0_9NpTitleIdES3_(&copy, &built)
        && _ZNK3sce2np9NpTitleId6EqualsEPKc(&copy, "WXYZ54321_00"), "equal title ids compare equal");
    Require(_ZN3sce2npneERKNS0_9NpTitleIdERK12SceNpTitleId(&copy, &plain) && _ZN3sce2npneERK12SceNpTitleIdRKNS0_9NpTitleIdE(&plain, &copy),
        "the suffix takes part in the comparison");
    plain.data[12] = 'q';
    plain.data[11] = '0';
    Require(_ZNK3sce2np9NpTitleId6EqualsERK12SceNpTitleId(&copy, &plain), "only the first twelve characters are compared");

    _ZN3sce2np9NpTitleIdC1ERK12SceNpTitleId(&copy, &plain);
    _ZN3sce2np9NpTitleIdaSERKS1_(&built, &copy);
    Require(built.titleId.data[12] == 'q', "assignment copies all sixteen bytes");
    _ZN3sce2np9NpTitleId5ClearEv(&built);
    Require(_ZNK3sce2np9NpTitleId7IsEmptyEv(&built) && built.titleId.data[12] == '\0', "clearing empties the title id");

    AllocatorRecord record {};
    NpAllocator allocator {RecordAllocate, nullptr, RecordRelease, &record};
    auto* owned = static_cast<NpTitleIdObject*>(_ZN3sce2np6ObjectnwEmR14SceNpAllocator(sizeof(NpTitleIdObject), &allocator));
    _ZN3sce2np9NpTitleIdC1Ev(owned);
    using Destructor = void APS5_VABI (*)(NpTitleIdObject*);
    reinterpret_cast<Destructor>(const_cast<void*>(owned->vtable[1]))(owned);
    Require(record.released == record.block.data(), "the deleting destructor frees through the stored allocator");
}

struct CalloutRecord {
    std::atomic<int> order {0};
    std::atomic<int> first {0};
    std::atomic<int> second {0};
};

CalloutRecord g_callouts;

void APS5_VABI RecordFirst(void*) {
    g_callouts.first = ++g_callouts.order;
}

void APS5_VABI RecordSecond(void*) {
    g_callouts.second = ++g_callouts.order;
}

void APS5_VABI RecordUnexpected(void*) {
    Require(false, "a stopped callout does not run");
}

void TestCallouts() {
    NpCalloutContext context {};
    NpCallout callout {};
    Require(sceNpCalloutStartOnCtx(&context, &callout, 0, RecordFirst, nullptr) == NpNotInitialized, "a context must be initialised first");
    Require(sceNpCalloutInitCtx(&context, "np-callout", 0x4000, 0, 0) == 0, "a callout context starts");
    Require(sceNpCalloutInitCtx(&context, "np-callout", 0x4000, 0, 0) == NpAlreadyInitialized, "a context starts once");

    NpCallout stopped {};
    int wasPending = 0;
    Require(sceNpCalloutStartOnCtx(&context, &stopped, 60000000, RecordUnexpected, nullptr) == 0, "a distant callout is queued");
    Require(sceNpCalloutStartOnCtx(&context, &stopped, 60000000, RecordUnexpected, nullptr) == NpAlreadyQueued,
        "a queued callout cannot be queued again");
    Require(sceNpCalloutStopOnCtx(&context, &stopped, &wasPending) == 0 && wasPending == 1, "a queued callout is removed");
    Require(sceNpCalloutStopOnCtx(&context, &stopped, &wasPending) == 0 && wasPending == 0, "removing twice reports nothing pending");

    NpCallout queuedFirst {};
    NpCallout queuedSecond {};
    Require(sceNpCalloutStartOnCtx(&context, &queuedFirst, 30000000, RecordUnexpected, nullptr) == 0, "an earlier distant callout is queued");
    Require(sceNpCalloutStartOnCtx(&context, &queuedSecond, 60000000, RecordUnexpected, nullptr) == 0, "a later distant callout is queued");
    const std::uint64_t queuedDeadline = queuedFirst.deadline;
    Require(sceNpCalloutStartOnCtx(&context, &queuedFirst, 0, RecordFirst, &context) == NpAlreadyQueued,
        "the earlier callout cannot be queued again");
    Require(queuedFirst.next == &queuedSecond && queuedFirst.deadline == queuedDeadline && queuedFirst.handler == RecordUnexpected &&
        queuedFirst.arg == nullptr, "a rejected start leaves the queued callout unchanged");
    Require(sceNpCalloutStopOnCtx(&context, &queuedSecond, &wasPending) == 0 && wasPending == 1,
        "the later callout is still pending after the rejected start");
    Require(sceNpCalloutStopOnCtx(&context, &queuedFirst, &wasPending) == 0 && wasPending == 1, "the earlier callout is still pending");

    NpCallout later {};
    Require(sceNpCalloutStartOnCtx(&context, &later, 20000, RecordSecond, nullptr) == 0, "a later callout is queued");
    Require(sceNpCalloutStartOnCtx(&context, &callout, 1000, RecordFirst, nullptr) == 0, "an earlier callout is queued");
    for (int i = 0; i < 2000 && g_callouts.second == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(g_callouts.first == 1 && g_callouts.second == 2, "callouts run in deadline order");
    sceNpCalloutTermCtx(&context);

    Require(context.initialized == 0, "terminating clears the context");
    sceNpCalloutTermCtx(&context);

}

bool Equals(const char* actual, const char* expected) {
    return std::strcmp(actual, expected) == 0;
}

void TestIds() {
    Require(sceNpUtilCmpAccountId(7, 7) == 0 && sceNpUtilCmpAccountId(7, 8) == 1, "account ids compare for equality");

    constexpr int NullArgument = static_cast<int>(0x80550003);
    constexpr int InvalidNpId = static_cast<int>(0x80550605);
    constexpr int NotMatch = static_cast<int>(0x80550609);
    auto makeNpId = [](const char* name, std::uint32_t opt, std::uint32_t platform) {
        std::array<std::uint8_t, 0x24> id{};
        std::memcpy(id.data(), name, std::strlen(name));
        std::memcpy(id.data() + 0x14, &opt, sizeof(opt));
        std::memcpy(id.data() + 0x18, &platform, sizeof(platform));
        id[0x1C] = 1;
        return id;
    };
    const auto alice = makeNpId("Alice_01", 5, 0x11223344);
    auto other = alice;
    Require(sceNpCmpNpId(nullptr, alice.data()) == NullArgument && sceNpCmpNpId(alice.data(), nullptr) == NullArgument, "null NpIds are refused");
    Require(sceNpCmpNpId(alice.data(), other.data()) == 0, "identical NpIds match");
    other[0x1C] = 0;
    Require(sceNpCmpNpId(alice.data(), other.data()) == InvalidNpId && sceNpCmpNpId(other.data(), alice.data()) == InvalidNpId, "the flag byte at 0x1C must be 1");
    Require(sceNpCmpNpId(makeNpId("Bob-2", 5, 0x11223344).data(), alice.data()) == NotMatch, "different online ids do not match");
    Require(sceNpCmpNpId(makeNpId("Alice_01", 6, 0x11223344).data(), alice.data()) == NotMatch, "different words at 0x14 do not match");
    Require(sceNpCmpNpId(makeNpId("Alice_01", 5, 0).data(), alice.data()) == 0, "an empty first byte at 0x18 on the first id matches");
    Require(sceNpCmpNpId(alice.data(), makeNpId("Alice_01", 5, 0).data()) == 0, "an empty first byte at 0x18 on the second id matches");
    Require(sceNpCmpNpId(alice.data(), makeNpId("Alice_01", 5, 0x11223355).data()) == NotMatch, "different words at 0x18 do not match");

    Require(sceNpIntIsOnlineIdString("Alice_01") && sceNpIntIsOnlineIdString("Bob-2"), "letters, digits, '_' and '-' are allowed");
    Require(!sceNpIntIsOnlineIdString("ab") && !sceNpIntIsOnlineIdString("abcdefghijklmnopq"), "online ids are 3 to 16 characters");
    Require(!sceNpIntIsOnlineIdString("1abc") && !sceNpIntIsOnlineIdString("ab.c") && !sceNpIntIsOnlineIdString(nullptr),
        "online ids start with a letter and use no other symbols");
}

void TestJsonText() {
    char escaped[64] = {};
    Require(sceNpUtilJsonEscape(escaped, sizeof(escaped), "a\"b/\n\x01", 7) == 0 && Equals(escaped, "a\\\"b\\/\\n\\u0001"),
        "json escaping covers quotes, solidus, newlines and controls");
    std::strcpy(escaped, "x");
    Require(sceNpUtilJsonEscape(escaped, sizeof(escaped), "yz", 2) == 0 && Equals(escaped, "xyz"), "escaping appends to the buffer");
    Require(sceNpUtilJsonEscape(escaped, 6, "a", 1) == NpUtilBufferTooSmall, "escaping needs room for one escape");
    Require(sceNpUtilJsonEscape(escaped, sizeof(escaped), nullptr, 1) == NpUtilInvalid, "escaping needs a source");
    struct Guarded {
        char buffer[16];
        char canary[16];
    } guarded{};
    std::strcpy(guarded.buffer, "0123456789");
    Require(sceNpUtilJsonEscape(guarded.buffer, sizeof(guarded.buffer), "\n\n\n", 3) == NpUtilBufferTooSmall && guarded.canary[0] == 0,
        "escaping into a partly filled buffer counts the text already in it");
    std::memset(guarded.buffer, 'x', sizeof(guarded.buffer));
    Require(sceNpUtilJsonEscape(guarded.buffer, sizeof(guarded.buffer), "a", 1) == NpUtilBufferTooSmall && guarded.canary[0] == 0,
        "escaping into a buffer without a terminator is refused");

    char text[32] = {};
    std::size_t length = 0;
    Require(sceNpUtilJsonUnescape(text, sizeof(text), "a\\nb\\u0041", 10, &length) == 0 && Equals(text, "a\nbA") && length == 5,
        "json unescaping decodes escapes and counts the terminator");
    Require(sceNpUtilJsonUnescape(text, sizeof(text), "\\uD83D\\uDE00", 12, &length) == 0 && Equals(text, "\xF0\x9F\x98\x80")
            && length == 5,
        "surrogate pairs become one UTF-8 character");
    Require(sceNpUtilJsonUnescape(text, sizeof(text), "\\u00G1", 6, &length) == NpUtilBufferTooSmall && Equals(text, "\\u00G1"),
        "a bad unicode escape is copied and reported");
    Require(sceNpUtilJsonUnescape(nullptr, sizeof(text), "abc", 3, &length) == 0 && length == 4, "a null buffer reports the size");
    Require(sceNpUtilJsonUnescape(text, 3, "abc", 3, &length) == NpUtilBufferTooSmall, "unescaping needs room for the terminator");
    length = 0;
    Require(sceNpUtilJsonUnescape(nullptr, 0, "abc", 3, &length) == 0 && length == 4, "a size query with no buffer and no size reports the size");

    char one[4] = {};
    std::size_t consumed = 0;
    Require(sceNpUtilJsonGetOneChar(one, sizeof(one), "\\u00e9x", 7, &length, &consumed) == 0 && length == 2 && consumed == 6
            && std::memcmp(one, "\xC3\xA9", 2) == 0,
        "one escaped character is decoded");
    Require(sceNpUtilJsonGetOneChar(one, sizeof(one), "\xE6\x97\xA5", 3, &length, &consumed) == 0 && length == 3 && consumed == 3,
        "one UTF-8 character is copied");
    Require(sceNpUtilJsonGetOneChar(one, sizeof(one), "\xE6\x97", 2, &length, &consumed) == NpUtilBufferTooSmall && length == 0
            && consumed == 3,
        "a cut UTF-8 character reports its full length");
    Require(sceNpUtilJsonGetOneChar(one, 1, "\xC3\xA9", 2, &length, &consumed) == NpUtilBufferTooSmall && length == 2,
        "a short buffer still reports the decoded size");
    Require(sceNpUtilJsonGetOneChar(one, sizeof(one), "\\ud83dXXde00", 12, &length, &consumed) == NpUtilBufferTooSmall,
        "a high surrogate without a second unicode escape is not combined");

    const char* character = nullptr;
    const char* japanese = "\xE6\x97\xA5" "a";
    Require(sceNpUtilGetNthChar(1, japanese, &character) == 0 && character == japanese + 3, "characters are counted, not bytes");
    Require(sceNpUtilGetNthChar(10, "abc", &character) == 0 && *character == '\0', "counting stops at the end");
    Require(sceNpUtilGetNthChar(2, "a\x01b", &character) == NpUtilInvalidCharacter, "control characters are refused");

    char url[32] = {};
    Require(sceNpUtilHttpUrlEncode(url, sizeof(url), "a b=c*", 6, &length) == 0 && Equals(url, "a+b%3Dc*") && length == 9,
        "form encoding keeps the safe set and escapes the rest");
    Require(sceNpUtilHttpUrlEncode(nullptr, 0, "@", 1, &length) == 0 && length == 4, "a null buffer reports the encoded size");
    Require(sceNpUtilHttpUrlEncode(url, 3, "@", 1, &length) == NpUtilBufferTooSmall, "encoding needs room for the terminator");
}

int g_jsonVariant = 0;
int g_jsonFailAt = 0;
std::vector<std::string> g_jsonEvents;

int RecordJsonEvent(std::int32_t event, std::uint32_t depth, std::int32_t element, const char* key, const void* value,
    std::int32_t arrayIndex, const std::string& offset) {
    std::string text = std::to_string(event) + " " + std::to_string(depth) + " " + std::to_string(element) + " ";
    text += key != nullptr ? key : "-";
    text += " ";
    if (value == nullptr) {
        text += "-";
    } else {
        std::uint32_t type = 0;
        std::memcpy(&type, value, sizeof(type));
        const char* valueText = static_cast<const char*>(value) + sizeof(type);
        if (g_jsonVariant != 0) std::memcpy(&valueText, static_cast<const char*>(value) + 8, sizeof(valueText));
        text += std::to_string(type) + ":" + valueText;
        if (g_jsonVariant == 2) {
            std::uint32_t last = 0;
            std::memcpy(&last, static_cast<const char*>(value) + 16, sizeof(last));
            text += ":" + std::to_string(last);
        }
    }
    text += " " + std::to_string(arrayIndex) + offset;
    g_jsonEvents.push_back(text);
    return static_cast<int>(g_jsonEvents.size()) == g_jsonFailAt ? static_cast<int>(0x80001234) : 0;
}

int APS5_VABI OnJsonEvent(void*, std::int32_t event, std::uint32_t depth, std::int32_t element, const char* key,
    const void* value, std::int32_t arrayIndex, void*) {
    return RecordJsonEvent(event, depth, element, key, value, arrayIndex, "");
}

int APS5_VABI OnJsonExEvent(void*, std::int32_t event, std::uint32_t depth, std::int32_t element, const char* key,
    const void* value, std::int32_t arrayIndex, std::uint32_t offset, void*) {
    return RecordJsonEvent(event, depth, element, key, value, arrayIndex, " @" + std::to_string(offset));
}

bool SameEvents(const std::vector<std::string>& expected) {
    if (g_jsonEvents == expected) return true;
    for (const std::string& event : g_jsonEvents) std::fprintf(stderr, "  got %s\n", event.c_str());
    return false;
}

int ParseJson(const NpJsonRuleSet* rules, const std::string& text, const std::vector<std::size_t>& chunks = {}) {
    alignas(16) static unsigned char context[0x760];
    g_jsonVariant = 0;
    g_jsonEvents.clear();
    if (sceNpJsonParseInit(context, rules, OnJsonEvent, nullptr) != 0) return -1;
    if (chunks.empty()) return sceNpJsonParse(context, text.data(), static_cast<int>(text.size()));
    std::size_t position = 0;
    for (const std::size_t size : chunks) {
        const int result = sceNpJsonParse(context, text.data() + position, static_cast<int>(size));
        if (result != 0) return result;
        position += size;
    }
    return 0;
}

void TestJsonParser() {
    const NpJsonRule topA[] = {{1, "a", 1}, {0, "", 0}};
    const NpJsonRule nestedB[] = {{1, "b", 0}, {0, "", 0}};
    const NpJsonRule* nestedLevels[] = {topA, nestedB};
    const NpJsonRuleSet nested{nestedLevels, 2};
    const std::string nestedText = R"({"a":{"b":"hello","c":1},"x":2})";
    const std::vector<std::string> nestedEvents = {"5 0 0 a - -1", "1 0 0 - - -1", "5 1 0 b - -1", "6 1 0 b 4:hello -1",
        "2 0 0 - - -1"};
    Require(ParseJson(&nested, nestedText) == 0 && SameEvents(nestedEvents), "matched keys report their object and values");
    Require(ParseJson(&nested, nestedText, std::vector<std::size_t>(nestedText.size(), 1)) == 0 && SameEvents(nestedEvents),
        "one byte at a time gives the same events");

    const NpJsonRule flatA[] = {{1, "a", 0}, {0, "", 0}};
    const NpJsonRule* flatLevels[] = {flatA, nestedB};
    const NpJsonRuleSet flat{flatLevels, 2};
    Require(ParseJson(&flat, R"({"a":{"b":"hello"},"b":1})") == 0 && SameEvents({"5 0 0 a - -1", "1 0 0 - - -1"}),
        "an object matched without descend is skipped, close included");

    const NpJsonRule element[] = {{2, nullptr, 0}, {0, "", 0}};
    const NpJsonRule* arrayLevels[] = {flatA, element};
    const NpJsonRuleSet array{arrayLevels, 2};
    Require(ParseJson(&array, R"({"a":[1,"x",true,false,null,-1.5e+3]})") == 0
            && SameEvents({"5 0 0 a - -1", "3 0 0 - - -1", "6 1 0 - 3:1 0", "6 1 0 - 4:x 1", "6 1 0 - 1: 2", "6 1 0 - 2: 3",
                "6 1 0 - 0: 4", "6 1 0 - 3:-1.5e+3 5", "4 0 0 - - -1"}),
        "array elements report their index and value type");

    const NpJsonRule topS[] = {{1, "s", 0}, {0, "", 0}};
    const NpJsonRule* stringLevels[] = {topS};
    const NpJsonRuleSet strings{stringLevels, 1};
    const std::string escaped = "{\"s\":\"\\u00e9\\ud83d\\ude00\\n\xC3\xA9\"}";
    const std::string decoded = "5 0 0 s - -1|6 0 0 s 4:\xC3\xA9\xF0\x9F\x98\x80\n\xC3\xA9 -1";
    const auto joined = [] {
        std::string out;
        for (const std::string& event : g_jsonEvents) out += (out.empty() ? "" : "|") + event;
        return out;
    };
    Require(ParseJson(&strings, escaped) == 0 && joined() == decoded, "escapes and surrogate pairs decode to UTF-8");
    const std::size_t cut = escaped.find("\\n") + 1;
    Require(ParseJson(&strings, escaped, {cut, 2, escaped.size() - cut - 2}) == 0 && joined() == decoded,
        "an escape and a UTF-8 sequence split between calls still decode");

    const NpJsonRule anyElement[] = {{2, nullptr, 1}, {0, "", 0}};
    const NpJsonRule* deepLevels[] = {anyElement};
    const NpJsonRuleSet deep{deepLevels, 1};
    Require(ParseJson(&deep, std::string(20, '[')) == NpJsonTooDeep && SameEvents({"3 0 0 - - -1"}),
        "nesting past 16 levels is refused");
    const NpJsonRule anyMember[] = {{1, nullptr, 1}, {0, "", 0}};
    const NpJsonRule* memberLevels[] = {anyMember};
    const NpJsonRuleSet members{memberLevels, 1};
    const auto objects = [](int depth) {
        std::string text;
        for (int i = 1; i < depth; ++i) text += "{\"k\":";
        text += "{\"k\":1,\"j\":2}";
        return text + std::string(depth - 1, '}');
    };
    const std::string sixteen = objects(16);
    Require(ParseJson(&members, sixteen) == 0 && SameEvents({"1 0 0 - - -1", "2 0 0 - - -1"}), "16 nested objects in one call close");
    Require(ParseJson(&members, sixteen, {80, sixteen.size() - 80}) == 0 && SameEvents({"1 0 0 - - -1"}),
        "a call that ends inside the 16th object loses the close, as in firmware 13.42");
    const std::string fifteen = objects(15);
    Require(ParseJson(&members, fifteen, {75, fifteen.size() - 75}) == 0 && SameEvents({"1 0 0 - - -1", "2 0 0 - - -1"}),
        "a call that ends inside the 15th object keeps the close");
    Require(ParseJson(&strings, "{\"s\":\"" + std::string(0x5FF, 'y') + "\"}") == 0 && g_jsonEvents.size() == 2,
        "a 0x5ff byte string fits");
    Require(ParseJson(&strings, "{\"s\":\"" + std::string(0x600, 'y') + "\"}") == NpUtilBufferTooSmall
            && SameEvents({"5 0 0 s - -1"}),
        "a 0x600 byte string is refused");
    Require(ParseJson(&strings, "{\"s\":" + std::string(0x600, '1') + "}") == NpUtilBufferTooSmall, "a 0x600 digit number is refused");
    const std::string longKey(0x7F, 'k');
    const NpJsonRule topLong[] = {{1, longKey.c_str(), 0}, {0, "", 0}};
    const NpJsonRule* longLevels[] = {topLong};
    const NpJsonRuleSet longRules{longLevels, 1};
    Require(ParseJson(&longRules, "{\"" + std::string(0x90, 'k') + "\":1}") == 0
            && SameEvents({"5 0 0 " + longKey + " - -1", "6 0 0 " + longKey + " 3:1 -1"}),
        "keys are cut to 0x7f bytes");
    g_jsonFailAt = 2;
    Require(ParseJson(&nested, R"({"a":{"b":1}})") == static_cast<int>(0x80001234) && SameEvents({"5 0 0 a - -1", "1 0 0 - - -1"}),
        "a negative callback result stops the parse and is returned");
    g_jsonFailAt = 0;
    Require(sceNpJsonParseInit(nullptr, &nested, OnJsonEvent, nullptr) == NpUtilInvalid, "a null context is refused");

    alignas(16) unsigned char bufContext[0x108] = {};
    char key[0x100] = {};
    char value[0x100] = {};
    const NpJsonRule bufTop[] = {{1, "abc", 0}, {1, "t", 0}, {0, "", 0}};
    const NpJsonRule* bufLevels[] = {bufTop};
    const NpJsonRuleSet bufRules{bufLevels, 1};
    Require(sceNpJsonParseBufInit(bufContext, &bufRules, nullptr, 4, value, 0x20, OnJsonEvent, nullptr) == NpUtilInvalid,
        "the buffered parser needs a key buffer");
    Require(sceNpJsonParseBufInit(bufContext, &bufRules, key, 0, value, 0x20, OnJsonEvent, nullptr) == NpUtilBufferTooSmall,
        "the key buffer needs a size");
    Require(sceNpJsonParseBufInit(bufContext, &bufRules, key, 4, value, 0x1F, OnJsonEvent, nullptr) == NpUtilBufferTooSmall,
        "the value buffer needs 0x20 bytes");
    Require(sceNpJsonParseBufInit(bufContext, &bufRules, key, 4, value, 0x20, OnJsonEvent, nullptr) == 0, "buffered init");
    g_jsonVariant = 1;
    g_jsonEvents.clear();
    const std::string bufText = R"({"abcdef":1,"t":true,"abc":"0123456789012345678901234567890"})";
    Require(sceNpJsonParseBuf(bufContext, bufText.data(), static_cast<int>(bufText.size())) == 0
            && SameEvents({"5 0 0 abc - -1", "6 0 0 abc 3:1 -1", "5 0 1 t - -1", "6 0 1 t 1: -1", "5 0 0 abc - -1",
                "6 0 0 abc 4:0123456789012345678901234567890 -1"}),
        "the caller buffers cut keys and hold values, literals have empty text");
    Require(sceNpJsonParseBufInit(bufContext, &strings, key, 4, value, 0x20, OnJsonEvent, nullptr) == 0, "buffered init again");
    g_jsonEvents.clear();
    const std::string tooLong = "{\"s\":\"" + std::string(0x20, 'y') + "\"}";
    Require(sceNpJsonParseBuf(bufContext, tooLong.data(), static_cast<int>(tooLong.size())) == NpUtilBufferTooSmall,
        "a string longer than the value buffer is refused");

    alignas(16) unsigned char exContext[0x110] = {};
    Require(sceNpJsonParseExInit(exContext, nullptr, key, 0x10, value, 0x20, OnJsonExEvent, nullptr) == 0, "ex init");
    g_jsonVariant = 2;
    g_jsonEvents.clear();
    const std::string everything = R"({"a":[1,{"b":"xy"}],"c":true})";
    Require(sceNpJsonParseEx(exContext, everything.data(), static_cast<int>(everything.size())) == 0
            && SameEvents({"1 0 -1 - - -1 @0", "5 1 -1 a - -1 @1", "3 1 -1 - - -1 @5", "6 2 -1 - 3:1:1 0 @6",
                "1 2 -1 - - 1 @8", "5 3 -1 b - -1 @9", "6 3 -1 b 4:xy:1 -1 @13", "2 2 -1 - - 1 @17", "4 1 -1 - - -1 @18",
                "5 1 -1 c - -1 @20", "6 1 -1 c 1::1 -1 @24", "2 0 -1 - - -1 @28"}),
        "without rules the extended parser reports everything with stream offsets");
    Require(sceNpJsonParseExInit(exContext, &strings, key, 0x10, value, 0x20, OnJsonExEvent, nullptr) == 0, "ex init again");
    g_jsonEvents.clear();
    const std::string pieces = "{\"s\":\"" + std::string("0123456789") + "0123456789" + "0123456789" + "0123456789\",\"n\":1}";
    Require(sceNpJsonParseEx(exContext, pieces.data(), static_cast<int>(pieces.size())) == 0
            && SameEvents({"5 0 0 s - -1 @1", "6 0 0 s 4:0123456789012345678901234567890:0 -1 @5",
                "6 0 0 s 4:123456789:1 -1 @5"}),
        "a long string comes in pieces with the last one marked");
    Require(sceNpJsonParseExInit(exContext, &strings, key, 0x10, value, 0x20, OnJsonExEvent, nullptr) == 0, "ex init for numbers");
    g_jsonEvents.clear();
    const std::string digits = "{\"s\":" + std::string(40, '1') + "}";
    Require(sceNpJsonParseEx(exContext, digits.data(), static_cast<int>(digits.size())) == 0
            && SameEvents({"5 0 0 s - -1 @1", "6 0 0 s 3:" + std::string(31, '1') + ":0 -1 @5", "6 0 0 s 3:111111111:1 -1 @5"}),
        "a long number comes in pieces too");

    const auto code = [](bool multi, std::int32_t http, const std::string& body) {
        return multi ? sceNpServerErrorJsonMultiGetErrorCode(http, body.data(), body.size())
                     : sceNpServerErrorJsonGetErrorCode(http, body.data(), body.size());
    };
    Require(code(false, 404, R"({"error":{"code":2097152}})") == static_cast<int>(0x82200000), "a server error code is returned");
    Require(code(false, 404, R"({"error":{"code":12582912}})") == static_cast<int>(0x82FFFFFF), "codes from 0xc00000 are unknown");
    Require(code(false, 404, R"({"error":{"code":"5"}})") == static_cast<int>(0x82F00194), "a string code falls back to the status");
    Require(code(false, 600, R"({"error":{"code":"5"}})") == static_cast<int>(0x82FFFFFF), "a status past 599 is unknown");
    Require(code(false, 404, R"({"code":7})") == static_cast<int>(0x82F00194), "the single form needs error.code");
    Require(code(true, 404, R"({"code":7})") == static_cast<int>(0x82000007), "the multi form also takes a top level code");
    Require(code(true, 404, R"({"error":{"code":8}})") == static_cast<int>(0x82000008), "the multi form takes error.code");
    Require(code(false, 503, R"({"error":{"code":-1}})") == static_cast<int>(0x82F001F7), "a negative code falls back to the status");
    Require(sceNpServerErrorJsonGetErrorCode(404, nullptr, 5) == static_cast<int>(0x82F00194), "no body falls back to the status");
    alignas(16) unsigned char serverContext[0x760] = {};
    NpServerErrorJsonResult result{};
    Require(sceNpServerErrorJsonParseInit(serverContext, nullptr) == NpUtilInvalid, "the server error parser needs a result");
    std::memset(&result, 0xEE, sizeof(result));
    Require(sceNpServerErrorJsonParseInit(serverContext, &result) == 0 && result.found == 0 && result.code == -1,
        "server error init clears the result");
    const std::string body = R"({"error":{"code":42}})";
    Require(sceNpServerErrorJsonParse(serverContext, body.data(), static_cast<int>(body.size())) == 0 && result.found == 1
            && result.code == 42,
        "the server error parser stores the code");
    const std::string badBody = R"({"error":{"code":"x"}})";
    Require(sceNpServerErrorJsonParseMultiInit(serverContext, &result) == 0, "multi server error init");
    Require(sceNpServerErrorJsonParse(serverContext, badBody.data(), static_cast<int>(badBody.size())) == NpServerErrorBadCode
            && result.found == 0,
        "a code that is not a number stops the parse");
}

void TestTitleIds() {
    NpTitleIdParts parts {{'A', 'B', 'C', 'D', '1', '2', '3', '4', '5'}, 0, 7};
    char titleId[13] = {};
    Require(sceNpUtilBuildTitleId(&parts, titleId, sizeof(titleId)) == 0 && Equals(titleId, "ABCD12345_07"), "the index is two digits");
    Require(sceNpUtilBuildTitleId(&parts, titleId, 12) == NpUtilBufferTooSmall, "a title id needs 13 bytes");
    parts.index = 100;
    Require(sceNpUtilBuildTitleId(&parts, titleId, sizeof(titleId)) == NpUtilOutOfRange, "the index is at most 99");
}

void TestLanguagesAndDebugIds() {
    char code[6] = {};
    char tag[36] = {};
    Require(sceNpUtilGetNpLanguageCode(1, code) == 0 && Equals(code, "en"), "language 1 is English");
    Require(sceNpUtilGetNpLanguageCode2(20, tag) == 0 && Equals(tag, "es-419"), "language 20 is Latin American Spanish");
    Require(Equals(sceNpUtilGetNpLanguageCodeStr(11), "zh-CN") && Equals(sceNpUtilGetNpLanguageCode2Str(30), "uk-UA"),
        "language strings are returned directly");
    Require(sceNpUtilGetNpLanguageCode(31, code) == NpUtilInvalid && sceNpUtilGetNpLanguageCodeStr(31) == nullptr,
        "unknown languages are refused");
    Require(sceNpUtilGetNpLanguageCode2(0, nullptr) == NpUtilInvalid, "a language code needs a buffer");
    const std::uint32_t first = sceNpDbgAssignDebugId();
    Require(first != 0 && sceNpDbgAssignDebugId() == first + 1, "debug ids count up");
}

void TestBase64() {
    const std::uint8_t payload[] = {1, 2, 3, 4, 5};
    char text[16] = {};
    Require(sceNpBase64Encoder(payload, text, 5) == 8 && Equals(text, "AQIDBAU="), "base64 pads the last group");
    Require(sceNpBase64Encoder(payload, text, 0) == 0, "an empty payload encodes to nothing");
    Require(sceNpBase64GetDecodeSize("AQIDBAU=", 8) == 5 && sceNpBase64GetDecodeSize("AQ==", 4) == 1,
        "the decoded size follows the padding");
    Require(sceNpBase64GetDecodeSize("", 0) == NpInvalidFormat && sceNpBase64GetDecodeSize("AQI", 3) == NpInvalidFormat,
        "base64 needs whole groups");
    Require(sceNpBase64GetDecodeSize("AQ=A", 4) == NpInvalidCharacter && sceNpBase64GetDecodeSize("A=AAAQID", 8) == NpInvalidCharacter,
        "padding is only allowed at the end");
    std::uint8_t bytes[8] = {};
    Require(sceNpBase64Decoder("AQIDBAU=", 8, bytes, sizeof(bytes)) == 5 && std::memcmp(bytes, payload, 5) == 0,
        "base64 decodes back to the payload");
    Require(sceNpBase64Decoder("AQID", 4, bytes, 2) == NpInvalidFormat, "decoding refuses a short buffer");
    Require(sceNpBase64Decoder("AQ?D", 4, bytes, 3) == NpInvalidCharacter, "decoding refuses characters outside the alphabet");

    const std::uint8_t url[] = {0xFB, 0x00};
    std::memset(text, 0, sizeof(text));
    Require(sceNpBase64UrlEncoder(url, text, 1) == 2 && Equals(text, "-w"), "base64url leaves out the padding");
    Require(sceNpBase64UrlEncoder(url, text, 2) == 3 && Equals(text, "-wA"), "base64url writes three characters for two bytes");
    Require(sceNpBase64UrlGetDecodeSize("-wA", 3) == 2 && sceNpBase64UrlGetDecodeSize("-w==", 4) == 1,
        "base64url accepts padded and unpadded tails");
    Require(sceNpBase64UrlGetDecodeSize("", 0) == NpInvalidCharacter && sceNpBase64UrlGetDecodeSize("AAAAA", 5) == NpInvalidCharacter,
        "a single trailing character cannot be decoded");
    Require(sceNpBase64UrlGetDecodeSize("+w==", 4) == NpInvalidCharacter, "base64url refuses the standard alphabet");
    Require(sceNpBase64UrlDecoder("-wA", 3, bytes, 2) == 2 && bytes[0] == 0xFB && bytes[1] == 0, "base64url decodes a short tail");
    Require(sceNpBase64UrlDecoder("-w", 2, bytes, 0) == NpInvalidFormat, "base64url decoding refuses a short buffer");
}

void TestNumbers() {
    Require(sceNpHexToInt('a') == 10 && sceNpHexToInt('F') == 15 && sceNpHexToInt('g') == -1, "hex digits are read");
    Require(sceNpIntToHex(10, false) == 'a' && sceNpIntToHex(15, true) == 'F' && sceNpIntToHex(9, true) == '9', "hex digits are written");

    char text[32] = {};
    std::size_t length = 0;
    Require(sceNpUInt32ToStr(text, sizeof(text), 255, 16, 3, &length) == 0 && Equals(text, "0XFF") && length == 4,
        "the hex prefix follows the case flag");
    Require(sceNpUInt32ToStr(text, sizeof(text), 8, 8, 2, &length) == 0 && Equals(text, "010") && length == 3, "octal gets a zero prefix");
    Require(sceNpUInt32ToStr(text, sizeof(text), 0, 10, 4, &length) == 0 && Equals(text, "0") && length == 1, "zero has no sign");
    Require(sceNpUInt32ToStr(text, 4, 1234, 10, 0, &length) == NpBufferTooSmall, "the terminator must fit");
    Require(sceNpUInt32ToStr(text, sizeof(text), 1, 37, 0, &length) == NpInvalidArgument, "the radix is limited to 36");
    Require(sceNpUInt32ToStr(nullptr, 0, 42, 10, 4, &length) == 0 && length == 3, "a null buffer reports the length");
    Require(sceNpInt32ToStr(text, sizeof(text), -2147483647 - 1, 10, 0, &length) == 0 && Equals(text, "-2147483648"),
        "a negative 32-bit value always has a sign");
    Require(sceNpInt32ToStr(text, 3, -42, 10, 0, &length) == NpBufferTooSmall, "the sign takes buffer space");
    Require(sceNpInt64ToStr(text, sizeof(text), -42, 10, 0, &length) == 0 && Equals(text, "42") && length == 2,
        "a 64-bit value only gets a sign with the sign flag");
    Require(sceNpInt64ToStr(text, sizeof(text), -42, 10, 4, &length) == 0 && Equals(text, "-42"), "the sign flag writes the minus");
    Require(sceNpUInt64ToStr(text, sizeof(text), UINT64_MAX, 16, 3, &length) == 0 && Equals(text, "0XFFFFFFFFFFFFFFFF") && length == 18,
        "the full 64-bit range is formatted");

    std::int32_t value32 = 0;
    std::size_t parsed = 0;
    Require(sceNpStrToInt32("-567abc", &value32, 10, &parsed) == 0 && value32 == -567 && parsed == 4, "parsing stops at the first non-digit");
    Require(sceNpStrToInt32("0x1A", &value32, 0, &parsed) == 0 && value32 == 26, "base zero follows the prefix");
    Require(sceNpStrToInt32("invalid", &value32, 10, &parsed) == NpOutOfRange, "no digits is an error");
    parsed = 0;
    Require(sceNpStrToInt32("3000000000", &value32, 10, &parsed) == NpOutOfRange && parsed == 10,
        "the length is reported before the 32-bit range check");
    std::int64_t value64 = 0;
    Require(sceNpStrToInt64("-9223372036854775808", &value64, 10, nullptr) == 0 && value64 == INT64_MIN, "the 64-bit minimum parses");
    Require(sceNpStrToInt64("99999999999999999999", &value64, 10, nullptr) == NpOutOfRange, "64-bit overflow is an error");
    std::uint32_t unsigned32 = 0;
    Require(sceNpStrToUInt32("4294967295", &unsigned32, 10, nullptr) == 0 && unsigned32 == UINT32_MAX, "the 32-bit maximum parses");
    Require(sceNpStrToUInt32("4294967296", &unsigned32, 10, nullptr) == NpOutOfRange, "32-bit unsigned overflow is an error");
    std::uint64_t unsigned64 = 0;
    Require(sceNpStrToUInt64("0xFFFFFFFFFFFFFFFF", &unsigned64, 0, nullptr) == 0 && unsigned64 == UINT64_MAX, "the 64-bit maximum parses");
    Require(sceNpStrToUInt64("18446744073709551616", &unsigned64, 10, nullptr) == NpOutOfRange, "64-bit unsigned overflow is an error");
}

void TestHexAndCopies() {
    const std::uint8_t data[] = {0x0A, 0xFE};
    char text[8] = {};
    Require(sceNpStrBuildHex(text, 5, data, 2, false) == 0 && Equals(text, "0afe"), "bytes are written as lowercase hex");
    Require(sceNpStrBuildHex(text, 5, data, 2, true) == 0 && Equals(text, "0AFE"), "bytes are written as uppercase hex");
    Require(sceNpStrBuildHex(text, 4, data, 2, false) == NpBufferTooSmall, "hex output needs room for the terminator");
    std::uint8_t bytes[4] = {};
    Require(sceNpStrParseHex("0aFE", bytes, 2) == 0 && bytes[0] == 0x0A && bytes[1] == 0xFE, "hex is read in either case");
    Require(sceNpStrParseHex("0AF", bytes, 2) == NpInvalidArgument, "odd hex is refused");
    Require(sceNpStrParseHex("0AFE", bytes, 1) == NpBufferTooSmall, "hex parsing checks the output size");
    Require(sceNpStrParseHex("0AG1", bytes, 2) == NpInvalidArgument, "non-hex characters are refused");
    bytes[1] = 0xFF;
    Require(sceNpStrParseHex("0A5G", bytes, 2) == NpInvalidArgument && bytes[0] == 0x0A && bytes[1] == 0x50,
        "a valid high digit is stored before the low digit is refused");
    bytes[1] = 0xFF;
    Require(sceNpStrnParseHex("0AG5", 4, bytes, 2) == NpInvalidArgument && bytes[1] == 0xFF, "a refused high digit stores nothing");
    Require(sceNpStrnParseHex("DEADBEEF1234", 8, bytes, 4) == 0 && bytes[0] == 0xDE && bytes[3] == 0xEF, "a hex prefix is read");

    char copy[8] = {};
    Require(sceNpStrcpyToBuf(copy, 6, "Hello") == 0 && Equals(copy, "Hello"), "a string that fits is copied");
    Require(sceNpStrcpyToBuf(copy, 5, "Hello") == NpBufferTooSmall, "a string without room for its terminator is refused");
    Require(sceNpStrncpyToBuf(copy, 6, "Hello World", 5) == 0 && Equals(copy, "Hello"), "the length limit sizes the copy");
    Require(sceNpStrncpyToBuf(copy, 5, "Hello World", 5) == NpBufferTooSmall, "the limited string still needs a terminator");
}

}

void TestMemoryStreams() {
    using Read = int(APS5_VABI*)(NpMemoryStreamReader*, void*, void*, std::size_t, std::size_t*);
    using Write = int(APS5_VABI*)(NpMemoryStreamWriter*, void*, const void*, std::size_t, std::size_t*);
    using Destroy = void(APS5_VABI*)(void*);

    const char text[] = "abcdefgh";
    NpMemoryStreamReader reader;
    std::memset(&reader, 0x77, sizeof(reader));
    _ZN3sce2np18MemoryStreamReaderC1EPKvm(&reader, text, 8);
    Require(reader.data == text && reader.size == 8 && reader.position == 0, "a reader starts at the beginning of its buffer");
    std::array<char, 8> out {};
    std::size_t count = 99;
    Require(reinterpret_cast<Read>(const_cast<void*>(reader.vtable[2]))(&reader, nullptr, out.data(), 5, &count) == 0 && count == 5 &&
                std::memcmp(out.data(), "abcde", 5) == 0 && reader.position == 5,
        "a read through the vtable copies the requested bytes");
    out.fill(0);
    Require(_ZN3sce2np18MemoryStreamReader4ReadEPNS0_6HandleEPvmPm(&reader, nullptr, out.data(), 8, &count) == 0 && count == 3 &&
                std::memcmp(out.data(), "fgh", 3) == 0 && out[3] == 0 && reader.position == 8,
        "a read past the end is cut to what remains");
    count = 99;
    Require(_ZN3sce2np18MemoryStreamReader4ReadEPNS0_6HandleEPvmPm(&reader, nullptr, out.data(), 8, &count) == 0 && count == 0,
        "an exhausted reader reports 0 bytes");
    reader.position = 12;
    Require(_ZN3sce2np18MemoryStreamReader4ReadEPNS0_6HandleEPvmPm(&reader, nullptr, out.data(), 8, &count) == 0 && count == 0 &&
                reader.position == 12,
        "a position beyond the size reads nothing");

    std::array<char, 6> buffer {};
    NpMemoryStreamWriter writer;
    _ZN3sce2np18MemoryStreamWriterC2EPvm(&writer, buffer.data(), 6);
    Require(reinterpret_cast<Write>(const_cast<void*>(writer.vtable[2]))(&writer, nullptr, "abcd", 4, &count) == 0 && count == 4 && writer.position == 4,
        "a write through the vtable stores the bytes");
    count = 99;
    Require(_ZN3sce2np18MemoryStreamWriter5WriteEPNS0_6HandleEPKvmPm(&writer, nullptr, "xyz", 3, &count) == NpStreamBufferTooSmall &&
                count == 99 && writer.position == 4,
        "a write that does not fit is refused whole");
    Require(_ZN3sce2np18MemoryStreamWriter5WriteEPNS0_6HandleEPKvmPm(&writer, nullptr, "xy", 2, nullptr) == 0 && writer.position == 6 &&
                std::memcmp(buffer.data(), "abcdxy", 6) == 0,
        "a write may fill the buffer exactly without a count pointer");
    _ZN3sce2np18MemoryStreamWriterC1EPvm(&writer, buffer.data(), 0);
    writer.position = 3;
    Require(_ZN3sce2np18MemoryStreamWriter5WriteEPNS0_6HandleEPKvmPm(&writer, nullptr, "a", 1, &count) == NpStreamBufferTooSmall &&
                _ZN3sce2np18MemoryStreamWriter5WriteEPNS0_6HandleEPKvmPm(&writer, nullptr, "", 0, &count) == 0 && writer.position == 3,
        "a writer of size 0 has no room whatever its position");
    _ZN3sce2np18MemoryStreamWriterC1EPvm(&writer, nullptr, 2);
    Require(_ZN3sce2np18MemoryStreamWriter5WriteEPNS0_6HandleEPKvmPm(&writer, nullptr, "abcdef", 6, &count) == 0 && count == 6 &&
                _ZN3sce2np18MemoryStreamWriter5WriteEPNS0_6HandleEPKvmPm(&writer, nullptr, "ab", 2, &count) == 0 && writer.position == 8,
        "a writer without a buffer only counts");

    reinterpret_cast<Destroy>(const_cast<void*>(reader.vtable[0]))(&reader);
    _ZN3sce2np18MemoryStreamReaderD2Ev(&reader);
    Require(reader.data == text && reader.size == 8 && reader.position == 12, "the reader destructors leave the object untouched");
    reinterpret_cast<Destroy>(const_cast<void*>(writer.vtable[0]))(&writer);
    _ZN3sce2np18MemoryStreamWriterD2Ev(&writer);
    Require(writer.data == nullptr && writer.size == 2 && writer.position == 8, "the writer destructors leave the object untouched");
}

struct ChunkReader {
    NpStreamReader base;
    const std::uint8_t* data;
    std::size_t size;
    std::size_t position;
    std::size_t chunk;
    int failAt;
    int calls;
};

int APS5_VABI ChunkRead(NpStreamReader* self, void*, void* out, std::size_t size, std::size_t* read) {
    auto* reader = reinterpret_cast<ChunkReader*>(self);
    if (++reader->calls == reader->failAt) {
        *read = 0;
        return -1;
    }
    const std::size_t count = std::min({size, reader->chunk, reader->size - reader->position});
    std::memcpy(out, reader->data + reader->position, count);
    reader->position += count;
    *read = count;
    return 0;
}

struct CollectWriter {
    NpStreamWriter base;
    std::array<std::uint8_t, 1024> data;
    std::size_t size;
    std::size_t chunk;
};

int APS5_VABI CollectWrite(NpStreamWriter* self, void*, const void* data, std::size_t size, std::size_t* written) {
    auto* writer = reinterpret_cast<CollectWriter*>(self);
    const std::size_t count = std::min(size, writer->chunk);
    std::memcpy(writer->data.data() + writer->size, data, count);
    writer->size += count;
    *written = count;
    return 0;
}

struct SumContext {
    NpStreamCtx base;
    std::size_t bytes;
    std::uint32_t sum;
    int result;
};

int APS5_VABI SumNotify(NpStreamCtx* self, const void* data, std::size_t size) {
    auto* context = reinterpret_cast<SumContext*>(self);
    for (std::size_t i = 0; i < size; ++i) context->sum += static_cast<const std::uint8_t*>(data)[i];
    context->bytes += size;
    return context->result;
}

const void* const ChunkReaderVtable[] = {nullptr, nullptr, reinterpret_cast<const void*>(&ChunkRead)};
const void* const CollectWriterVtable[] = {nullptr, nullptr, reinterpret_cast<const void*>(&CollectWrite)};
const void* const SumContextVtable[] = {nullptr, nullptr, reinterpret_cast<const void*>(&SumNotify)};

void TestStreamHelpers() {
    std::array<std::uint8_t, 600> source {};
    std::uint32_t total = 0;
    for (std::size_t i = 0; i < source.size(); ++i) total += source[i] = static_cast<std::uint8_t>(i * 7 + 3);
    auto reader = [&](std::size_t size, std::size_t chunk) { return ChunkReader {{ChunkReaderVtable}, source.data(), size, 0, chunk, 0, 0}; };
    std::array<std::uint8_t, 600> out {};
    std::size_t count = 0;

    ChunkReader stream = reader(600, 70);
    SumContext context {{SumContextVtable}, 0, 0, 0};
    Require(_ZN3sce2np12StreamReader8ReadDataEPNS0_6HandleEPNS0_9StreamCtxEPvmPm(&stream.base, nullptr, &context.base, out.data(), 600, &count) == 0 &&
                count == 600 && out == source && context.bytes == 600 && context.sum == total,
        "ReadData gathers short reads and shows every byte to the context");
    stream = reader(100, 70);
    Require(_ZN3sce2np12StreamReader8ReadDataEPNS0_6HandleEPvmPm(&stream.base, nullptr, out.data(), 150, &count) == NpStreamShort && count == 100,
        "ReadData reports a stream that ends early");
    stream = reader(100, 30);
    Require(_ZN3sce2np12StreamReader7ReadAllEPNS0_6HandleEPvmPm(&stream.base, nullptr, out.data(), 150, &count) == 0 && count == 100,
        "ReadAll accepts a stream shorter than the buffer");
    stream = reader(100, 30);
    count = 7;
    Require(_ZN3sce2np12StreamReader7ReadAllEPNS0_6HandleEPvmPm(&stream.base, nullptr, out.data(), 99, &count) == NpStreamBufferTooSmall && count == 7,
        "ReadAll refuses a stream longer than the buffer");
    stream = reader(100, 30);
    Require(_ZN3sce2np12StreamReader7ReadAllEPNS0_6HandleEPvmPm(&stream.base, nullptr, out.data(), 100, &count) == 0 && count == 100,
        "ReadAll accepts a stream that fills the buffer exactly");

    stream = reader(600, 100);
    std::int64_t skipped = 5;
    Require(_ZN3sce2np12StreamReader8SkipDataEPNS0_6HandleElPl(&stream.base, nullptr, 550, &skipped) == 0 && skipped == 555 && stream.position == 550,
        "SkipData adds what it skipped");
    skipped = 0;
    context = {{SumContextVtable}, 0, 0, 0};
    Require(_ZN3sce2np12StreamReader7SkipAllEPNS0_6HandleEPNS0_9StreamCtxEPl(&stream.base, nullptr, &context.base, &skipped) == 0 && skipped == 50 &&
                context.bytes == 50,
        "SkipAll skips to the end");
    stream = reader(600, 100);
    stream.failAt = 3;
    skipped = 0;
    Require(_ZN3sce2np12StreamReader7SkipAllEPNS0_6HandleEPl(&stream.base, nullptr, &skipped) == -1 && skipped == 200,
        "SkipAll stops at a failed read");
    stream = reader(600, 100);
    context = {{SumContextVtable}, 0, 0, -5};
    Require(_ZN3sce2np12StreamReader4ReadEPNS0_6HandleEPNS0_9StreamCtxEPvmPm(&stream.base, nullptr, &context.base, out.data(), 10, &count) == -5 &&
                count == 10,
        "a failing context fails the read");
    context.result = 3;
    Require(_ZN3sce2np12StreamReader4ReadEPNS0_6HandleEPNS0_9StreamCtxEPvmPm(&stream.base, nullptr, &context.base, out.data(), 10, &count) == 0,
        "a positive context result counts as success");

    CollectWriter writer {{CollectWriterVtable}, {}, 0, 90};
    context = {{SumContextVtable}, 0, 0, 0};
    Require(_ZN3sce2np12StreamWriter9WriteDataEPNS0_6HandleEPNS0_9StreamCtxEPKvmPm(&writer.base, nullptr, &context.base, source.data(), 600, &count) == 0 &&
                count == 600 && writer.size == 600 && std::memcmp(writer.data.data(), source.data(), 600) == 0 && context.sum == total,
        "WriteData pushes everything through short writes");
    writer = {{CollectWriterVtable}, {}, 0, 1000};
    Require(_ZN3sce2np12StreamWriter15WriteFilledDataEPNS0_6HandleEcl(&writer.base, nullptr, 'x', 600) == 0 && writer.size == 600 &&
                std::all_of(writer.data.begin(), writer.data.begin() + 600, [](std::uint8_t b) { return b == 'x'; }),
        "WriteFilledData writes the fill byte in chunks");
    writer = {{CollectWriterVtable}, {}, 0, 1000};
    Require(_ZN3sce2np12StreamWriter15WriteFilledDataEPNS0_6HandleEPNS0_9StreamCtxEcl(&writer.base, nullptr, nullptr, 'x', -4) == 0 && writer.size == 0,
        "WriteFilledData of a negative size writes nothing");
    writer = {{CollectWriterVtable}, {}, 0, 4};
    Require(_ZN3sce2np12StreamWriter5WriteEPNS0_6HandleEPNS0_9StreamCtxEPKvmPm(&writer.base, nullptr, nullptr, source.data(), 10, &count) == 0 && count == 4,
        "Write makes a single stream call");
}

void TestRefObjects() {
    NpRefObject object {};
    std::memset(&object, 0x77, sizeof(object));
    _ZN3sce2np9RefObjectC1Ev(&object);
    Require(object.vtable != nullptr && object.refCount == 0, "a new reference-counted object has no references");
    Require(_ZN3sce2np9RefObject6AddRefEv(&object) == 1 && _ZN3sce2np9RefObject6AddRefEv(&object) == 2 && object.refCount == 2,
        "AddRef returns the new count");
    Require(_ZN3sce2np9RefObject7ReleaseEv(&object) == 1 && object.refCount == 1, "Release returns the new count");
    _ZN3sce2np9RefObjectD2Ev(&object);
    Require(object.refCount == 1, "the destructor leaves the count alone");

    AllocatorRecord record {};
    NpAllocator allocator {RecordAllocate, nullptr, RecordRelease, &record};
    auto* owned = static_cast<NpRefObject*>(_ZN3sce2np6ObjectnwEmR14SceNpAllocator(sizeof(NpRefObject), &allocator));
    _ZN3sce2np9RefObjectC2Ev(owned);
    Require(_ZN3sce2np9RefObject6AddRefEv(owned) == 1 && _ZN3sce2np9RefObject6AddRefEv(owned) == 2, "an allocated object takes references");
    Require(_ZN3sce2np9RefObject7ReleaseEv(owned) == 1 && record.released == nullptr, "releasing a reference that is not the last keeps the object");
    Require(_ZN3sce2np9RefObject7ReleaseEv(owned) == 0 && record.released == record.block.data(),
        "releasing the last reference deletes the object through the stored allocator");
}

void TestCommIdObjects() {
    NpCommunicationId parts {};
    std::memset(&parts, 0x77, sizeof(parts));
    Require(sceNpUtilParseTitleId(&parts, "WXYZ54321_07") == 0 && std::memcmp(parts.data, "WXYZ54321", 9) == 0 && parts.term == '\0'
            && parts.num == 7 && parts.dummy == '\0',
        "sceNpUtilParseTitleId splits the id and its number");
    Require(sceNpUtilParseTitleId(&parts, "WXYZ54321-07") == NpUtilParseOutOfRange && sceNpUtilParseTitleId(&parts, "WXYZ54321_007") == NpUtilParseOutOfRange
            && sceNpUtilParseTitleId(&parts, "WXYZ") == NpUtilParseOutOfRange && sceNpUtilParseTitleId(&parts, nullptr) == NpUtilInvalid,
        "sceNpUtilParseTitleId needs an underscore at index 9 and at most 12 characters");

    NpCommIdObject commId {};
    std::memset(&commId, 0x77, sizeof(commId));
    _ZN3sce2np8NpCommIdC1Ev(&commId);
    Require(commId.vtable != nullptr && _ZNK3sce2np8NpCommId7IsEmptyEv(&commId), "a default communication id is empty");
    Require(_ZN3sce2np8NpCommId5ParseEPS1_PKc(&commId, "WXYZ54321_42") == 0 && commId.commId.num == 42 && !_ZNK3sce2np8NpCommId7IsEmptyEv(&commId),
        "parsing keeps the number");
    Require(_ZN3sce2np8NpCommId5ParseEPS1_PKc(&commId, "WXY154321_00") == NpTitleIdInvalid
            && _ZN3sce2np8NpCommId5ParseEPS1_PKcm(&commId, "WXYZ54321_0", 11) == NpTitleIdInvalid && commId.commId.num == 42,
        "a rejected communication id leaves the object as it was");
    Require(_ZN3sce2np8NpCommId5ParseEPS1_PKcm(&commId, "ABCD12345_09xyz", 12) == 0 && commId.commId.num == 9
            && std::memcmp(commId.commId.data, "ABCD12345", 9) == 0,
        "the sized parse reads only the given length");

    char text[16];
    std::memset(text, 'x', sizeof(text));
    Require(_ZN3sce2np8NpCommId5BuildERKS1_Pcm(&commId, text, 14) == 0 && std::strcmp(text, "ABCD12345_09") == 0 && text[13] == '\0'
            && text[14] == 'x',
        "building writes the id and clears the rest of the buffer");
    Require(_ZN3sce2np8NpCommId5BuildERKS1_Pcm(&commId, text, 12) == NpUtilBufferTooSmall && text[0] == '\0', "building needs 13 bytes");

    NpCommIdObject copy {};
    _ZN3sce2np8NpCommIdC1ERKS1_(&copy, &commId);
    NpCommunicationId plain = commId.commId;
    plain.dummy = 'q';
    Require(_ZNK3sce2np8NpCommId6EqualsERKS1_(&copy, &commId) && _ZN3sce2npeqERKNS0_8NpCommIdERK20SceNpCommunicationId(&copy, &plain)
            && _ZNK3sce2np8NpCommId6EqualsEPKc(&copy, "ABCD12345_09") && !_ZNK3sce2np8NpCommId6EqualsEPKc(&copy, "ABCD12345_9"),
        "equal communication ids compare equal, ignoring the spare byte");
    plain.num = 10;
    Require(_ZN3sce2npneERK20SceNpCommunicationIdRKNS0_8NpCommIdE(&plain, &copy) && !_ZNK3sce2np8NpCommId6EqualsERK20SceNpCommunicationId(&copy, &plain),
        "the number takes part in the comparison");
    _ZN3sce2np8NpCommIdC2ERK20SceNpCommunicationId(&copy, &plain);
    Require(copy.commId.dummy == 'q', "constructing from a plain id copies all twelve bytes");
    _ZN3sce2np8NpCommId5ClearEv(&copy);
    Require(_ZNK3sce2np8NpCommId7IsEmptyEv(&copy) && copy.commId.dummy == '\0', "clearing empties the communication id");

    AllocatorRecord record {};
    NpAllocator allocator {RecordAllocate, nullptr, RecordRelease, &record};
    auto* owned = static_cast<NpCommIdObject*>(_ZN3sce2np6ObjectnwEmR14SceNpAllocator(sizeof(NpCommIdObject), &allocator));
    _ZN3sce2np8NpCommIdC1Ev(owned);
    using Destructor = void APS5_VABI (*)(NpCommIdObject*);
    reinterpret_cast<Destructor>(const_cast<void*>(owned->vtable[1]))(owned);
    Require(record.released == record.block.data(), "the deleting destructor frees through the stored allocator");
}

int main() {
    TestAtomics();
    TestSemaphoresAndFlags();
    TestEventFlagClearAndCancel();
    TestSharedEventFlags();
    TestThreads();
    TestConditionWakeups();
    TestHeaps();
    TestObjects();
    TestObjectsThroughStoredAllocator();
    TestMemoryStreams();
    TestTitleIdObjects();
    TestStreamHelpers();
    TestRefObjects();
    TestCommIdObjects();
    TestBase64();
    TestNumbers();
    TestHexAndCopies();
    TestCallouts();
    TestIds();
    TestLanguagesAndDebugIds();
    TestJsonText();
    TestJsonParser();
    TestTitleIds();
    std::puts("guest np common runtime: PASS");
}
