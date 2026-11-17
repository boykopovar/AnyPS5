#include "prx/libSceUlt/include/Api.hpp"
#include <cerrno>
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

constexpr int Ok = 0;
constexpr int Null = -2139029503;
constexpr int Alignment = -2139029502;
constexpr int Range = -2139029501;
constexpr int Invalid = -2139029500;
constexpr int Permission = -2139029499;
constexpr int Again = -2139029496;
constexpr int State = -2139029498;
constexpr int Busy = -2139029497;

struct alignas(8) Object {
    std::uint8_t bytes[256];
};

void MutexTryLock() {
    Object mutex{};
    Require(sceUltMutexTryLock(nullptr) == Null);
    Require(sceUltMutexTryLock(&mutex) == State);
    UltMutexOptParam options{};
    options.attribute = 1;
    Require(sceUltMutexCreate(&mutex, "trylock", nullptr, &options, 0) == Ok);
    Require(sceUltMutexTryLock(&mutex) == Ok);
    Require(sceUltMutexTryLock(&mutex) == Ok);
    std::atomic<int> other{0};
    std::thread([&] { other = sceUltMutexTryLock(&mutex); }).join();
    Require(other == Busy);
    Require(sceUltMutexUnlock(&mutex) == Ok);
    std::thread([&] { other = sceUltMutexTryLock(&mutex); }).join();
    Require(other == Busy);
    Require(sceUltMutexUnlock(&mutex) == Ok);
    Require(sceUltMutexUnlock(&mutex) == Permission);
    std::thread([&] {
        other = sceUltMutexTryLock(&mutex);
        if (other == Ok) other = sceUltMutexUnlock(&mutex);
    }).join();
    Require(other == Ok);
    Require(sceUltMutexLock(&mutex) == Ok);
    Require(sceUltMutexDestroy(&mutex) == State);
    Require(sceUltMutexUnlock(&mutex) == Ok);
    Require(sceUltMutexDestroy(&mutex) == Ok);
    Require(sceUltMutexTryLock(&mutex) == State);
    Require(sceUltMutexCreate(&mutex, "plain", nullptr, nullptr, 0) == Ok);
    Require(sceUltMutexTryLock(&mutex) == Ok);
    Require(sceUltMutexTryLock(&mutex) == State);
    Require(sceUltMutexUnlock(&mutex) == Ok);
    std::thread([&] {
        other = sceUltMutexTryLock(&mutex);
        if (other == Ok) other = sceUltMutexUnlock(&mutex);
    }).join();
    Require(other == Ok);
    Require(sceUltMutexDestroy(&mutex) == Ok);
}

void MutexAndRuntimeOptParams() {
    alignas(8) std::uint8_t misaligned[sizeof(UltMutexOptParam) + 8];
    Require(sceUltMutexOptParamInitialize(nullptr, 0) == Null);
    errno = 71;
    Require(sceUltMutexOptParamInitialize(reinterpret_cast<UltMutexOptParam*>(0x10), 0) == Null);
    Require(errno == 71);
    Require(sceUltMutexOptParamInitialize(reinterpret_cast<UltMutexOptParam*>(misaligned + 4), 0) == -2139029502);
    UltMutexOptParam mutexOptions;
    std::memset(&mutexOptions, 0xa5, sizeof(mutexOptions));
    Require(sceUltMutexOptParamInitialize(&mutexOptions, 0x1234) == Ok);
    Require(mutexOptions.header[0] == 0x6C696275 && mutexOptions.header[1] == 0x6C746D61 && mutexOptions.header[2] == 0x67696300);
    Require(mutexOptions.buildVersion == 0x1234 && mutexOptions.attribute == 0);
    for (const std::uint32_t word : mutexOptions.reserved) Require(word == 0);
    UltUlthreadRuntimeOptParam runtimeOptions;
    std::memset(&runtimeOptions, 0xa5, sizeof(runtimeOptions));
    Require(sceUltUlthreadRuntimeOptParamInitialize(nullptr, 0) == Null);
    Require(sceUltUlthreadRuntimeOptParamInitialize(&runtimeOptions, 0x12400000) == Ok);
    const std::uint8_t console[40] = {0x75, 0x62, 0x69, 0x6c, 0x61, 0x6d, 0x74, 0x6c, 0x00, 0x63, 0x69, 0x67, 0x00, 0x00, 0x40, 0x12, 0x00, 0x40, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xbc, 0x02, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00};
    Require(std::memcmp(runtimeOptions.bytes, console, sizeof(console)) == 0);
    for (std::size_t i = sizeof(console); i < sizeof(runtimeOptions.bytes); i++) Require(runtimeOptions.bytes[i] == 0);
}

void UnreadableOptParams() {
    const auto* unreadable = reinterpret_cast<const UltMutexOptParam*>(0x10);
    Object mutex{};
    Object pool{};
    Object object{};
    alignas(8) std::uint8_t runtime[4096]{};
    std::uint8_t workArea[2048];
    errno = 71;
    Require(sceUltUlthreadRuntimeOptParamInitialize(reinterpret_cast<UltUlthreadRuntimeOptParam*>(0x10), 0) == Null);
    Require(sceUltMutexCreate(&mutex, "unreadable", nullptr, unreadable, 0) == Null);
    Require(_sceUltMutexCreate(&mutex, "unreadable", &pool, unreadable) == Null);
    Require(sceUltUlthreadRuntimeCreate(runtime, "unreadable", 2, 1, nullptr, unreadable, 0) == Null);
    Require(_sceUltWaitingQueueResourcePoolCreate(&pool, "unreadable", 2, 2, workArea, unreadable) == Null);
    Require(_sceUltSemaphoreCreate(&object, "unreadable", 1, nullptr, unreadable) == Null);
    Require(_sceUltQueueDataResourcePoolCreate(&object, "unreadable", 8, 64, 2, nullptr, workArea, unreadable) == Null);
    Require(errno == 71);
    Require(sceUltMutexCreate(&mutex, "plain", nullptr, nullptr, 0) == Ok);
    Require(sceUltMutexDestroy(&mutex) == Ok);
}

void OptParams() {
    using Initializer = decltype(&sceUltSemaphoreOptParamInitialize);
    for (const Initializer initialize : {sceUltSemaphoreOptParamInitialize, sceUltQueueOptParamInitialize, sceUltQueueDataResourcePoolOptParamInitialize,
                                         sceUltWaitingQueueResourcePoolOptParamInitialize, sceUltUlthreadOptParamInitialize, sceUltReaderWriterLockOptParamInitialize,
                                         _sceUltSemaphoreOptParamInitialize, _sceUltQueueOptParamInitialize, _sceUltQueueDataResourcePoolOptParamInitialize,
                                         _sceUltWaitingQueueResourcePoolOptParamInitialize, _sceUltUlthreadOptParamInitialize, _sceUltReaderWriterLockOptParamInitialize,
                                         sceUltConditionVariableOptParamInitialize, _sceUltConditionVariableOptParamInitialize}) {
        alignas(UltOptParam) std::uint8_t bytes[136];
        std::memset(bytes, 0xa5, sizeof(bytes));
        Require(initialize(nullptr, 0) == Null);
        errno = 71;
        Require(initialize(reinterpret_cast<UltOptParam*>(0x10), 0) == Null);
        Require(errno == 71);
        Require(initialize(reinterpret_cast<UltOptParam*>(bytes), 0) == Ok);
        for (std::size_t index = 0; index < 128; ++index) Require(bytes[index] == 0);
        for (std::size_t index = 128; index < sizeof(bytes); ++index) Require(bytes[index] == 0xa5);
    }
}

void WaitUntil(const std::atomic<bool>& flag) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!flag.load() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(flag.load());
}

void Settle() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void ReaderWriterValidation() {
    Object rwlock{};
    Object pool{};
    std::memset(&rwlock, 0x5a, sizeof(rwlock));
    Require(sceUltReaderWriterLockCreate(nullptr, "rwlock", nullptr, nullptr, 0) == Null);
    Require(sceUltReaderWriterLockCreate(&rwlock, "rwlock", &pool, nullptr, 0) == Invalid);
    for (const auto call : {sceUltReaderWriterLockDestroy, sceUltReaderWriterLockLockRead, sceUltReaderWriterLockTryLockRead, sceUltReaderWriterLockUnlockRead,
                            sceUltReaderWriterLockLockWrite, sceUltReaderWriterLockTryLockWrite, sceUltReaderWriterLockUnlockWrite}) {
        Require(call(nullptr) == Null);
        Require(call(&rwlock) == State);
    }
    Require(sceUltReaderWriterLockCreate(&rwlock, "rwlock", nullptr, nullptr, 0) == Ok);
    for (const auto byte : rwlock.bytes) Require(byte == 0);
    Require(sceUltReaderWriterLockCreate(&rwlock, "rwlock", nullptr, nullptr, 0) == State);
    Require(sceUltReaderWriterLockUnlockRead(&rwlock) == State);
    Require(sceUltReaderWriterLockUnlockWrite(&rwlock) == State);
    Require(sceUltReaderWriterLockLockRead(&rwlock) == Ok);
    Require(sceUltReaderWriterLockDestroy(&rwlock) == Busy);
    Require(sceUltReaderWriterLockUnlockWrite(&rwlock) == State);
    Require(sceUltReaderWriterLockUnlockRead(&rwlock) == Ok);
    Require(sceUltReaderWriterLockLockWrite(&rwlock) == Ok);
    Require(sceUltReaderWriterLockLockWrite(&rwlock) == State && sceUltReaderWriterLockTryLockWrite(&rwlock) == State);
    Require(sceUltReaderWriterLockLockRead(&rwlock) == State && sceUltReaderWriterLockTryLockRead(&rwlock) == State);
    Require(sceUltReaderWriterLockDestroy(&rwlock) == Busy);
    std::atomic<int> other{0};
    std::thread([&] { other = sceUltReaderWriterLockUnlockWrite(&rwlock); }).join();
    Require(other == State);
    Require(sceUltReaderWriterLockUnlockWrite(&rwlock) == Ok);
    Require(sceUltReaderWriterLockDestroy(&rwlock) == Ok);
    Require(sceUltReaderWriterLockLockRead(&rwlock) == State && sceUltReaderWriterLockDestroy(&rwlock) == State);
}

void ReaderWriterReaders() {
    Object rwlock{};
    Require(sceUltReaderWriterLockCreate(&rwlock, "readers", nullptr, nullptr, 0) == Ok);
    Require(sceUltReaderWriterLockLockRead(&rwlock) == Ok);
    std::atomic<int> other{0};
    std::thread([&] {
        other = sceUltReaderWriterLockTryLockRead(&rwlock);
        if (other == Ok) other = sceUltReaderWriterLockLockRead(&rwlock);
    }).join();
    Require(other == Ok);
    Require(sceUltReaderWriterLockTryLockWrite(&rwlock) == Busy);
    std::atomic<bool> started{false};
    std::atomic<bool> written{false};
    std::thread writer([&] {
        started = true;
        other = sceUltReaderWriterLockLockWrite(&rwlock);
        written = true;
        if (other == Ok) other = sceUltReaderWriterLockUnlockWrite(&rwlock);
    });
    WaitUntil(started);
    for (int held = 3; held > 0; --held) {
        Settle();
        Require(!written.load());
        Require(sceUltReaderWriterLockUnlockRead(&rwlock) == Ok);
    }
    writer.join();
    Require(written.load() && other == Ok);
    Require(sceUltReaderWriterLockUnlockRead(&rwlock) == State);
    Require(sceUltReaderWriterLockDestroy(&rwlock) == Ok);
}

void ReaderWriterWriter() {
    Object rwlock{};
    Require(sceUltReaderWriterLockCreate(&rwlock, "writer", nullptr, nullptr, 0) == Ok);
    Require(sceUltReaderWriterLockTryLockWrite(&rwlock) == Ok);
    std::atomic<int> other{0};
    std::thread([&] { other = sceUltReaderWriterLockTryLockRead(&rwlock); }).join();
    Require(other == Busy);
    std::thread([&] { other = sceUltReaderWriterLockTryLockWrite(&rwlock); }).join();
    Require(other == Busy);
    std::atomic<bool> started{false};
    std::atomic<bool> read{false};
    std::thread reader([&] {
        started = true;
        other = sceUltReaderWriterLockLockRead(&rwlock);
        read = true;
        if (other == Ok) other = sceUltReaderWriterLockUnlockRead(&rwlock);
    });
    WaitUntil(started);
    Settle();
    Require(!read.load());
    Require(sceUltReaderWriterLockDestroy(&rwlock) == Busy);
    Require(sceUltReaderWriterLockUnlockWrite(&rwlock) == Ok);
    reader.join();
    Require(read.load() && other == Ok);
    Require(sceUltReaderWriterLockTryLockWrite(&rwlock) == Ok && sceUltReaderWriterLockUnlockWrite(&rwlock) == Ok);
    Require(sceUltReaderWriterLockDestroy(&rwlock) == Ok);
}

void ReaderWriterPool() {
    Object pool{};
    Object rwlock{};
    std::uint8_t workArea[2048];
    Require(_sceUltWaitingQueueResourcePoolCreate(&pool, "pool", 2, 2, workArea, nullptr) == Ok);
    Require(sceUltReaderWriterLockCreate(&rwlock, "pooled", &pool, nullptr, 0) == Ok);
    Require(sceUltReaderWriterLockLockWrite(&rwlock) == Ok);
    std::atomic<bool> started{false};
    std::atomic<int> other{0};
    std::thread reader([&] {
        started = true;
        other = sceUltReaderWriterLockLockRead(&rwlock);
        if (other == Ok) other = sceUltReaderWriterLockUnlockRead(&rwlock);
    });
    WaitUntil(started);
    Settle();
    Require(sceUltWaitingQueueResourcePoolDestroy(&pool) == Busy);
    Require(sceUltReaderWriterLockUnlockWrite(&rwlock) == Ok);
    reader.join();
    Require(other == Ok);
    Require(sceUltReaderWriterLockDestroy(&rwlock) == Ok);
    Require(sceUltWaitingQueueResourcePoolDestroy(&pool) == Ok);
}

void ReaderWriterFinalize() {
    Object rwlock{};
    Require(sceUltReaderWriterLockCreate(&rwlock, "finalize", nullptr, nullptr, 0) == Ok);
    Require(sceUltReaderWriterLockLockWrite(&rwlock) == Ok);
    std::atomic<bool> started{false};
    std::atomic<int> other{0};
    std::thread waiter([&] {
        started = true;
        other = sceUltReaderWriterLockLockWrite(&rwlock);
    });
    WaitUntil(started);
    Settle();
    Require(sceUltFinalize() == Ok);
    waiter.join();
    Require(other == State);
    Require(sceUltReaderWriterLockUnlockWrite(&rwlock) == State);
    Require(sceUltInitialize() == Ok);
}

void ReaderWriterUnderscored() {
    Object pool{};
    Object rwlock{};
    std::memset(&rwlock, 0x5a, sizeof(rwlock));
    alignas(UltOptParam) std::uint8_t optBytes[128];
    auto* optParam = reinterpret_cast<UltOptParam*>(optBytes);
    std::uint8_t workArea[2048];
    Require(_sceUltReaderWriterLockOptParamInitialize(optParam, 0) == Ok);
    Require(_sceUltReaderWriterLockCreate(nullptr, "underscored", nullptr, optParam, 0) == Null);
    Require(_sceUltReaderWriterLockCreate(&rwlock, "underscored", &pool, optParam, 0) == Invalid);
    for (const auto byte : rwlock.bytes) Require(byte == 0x5a);
    Require(_sceUltWaitingQueueResourcePoolCreate(&pool, "pool", 2, 2, workArea, nullptr) == Ok);
    Require(_sceUltReaderWriterLockCreate(&rwlock, "underscored", &pool, optParam, 0) == Ok);
    for (const auto byte : rwlock.bytes) Require(byte == 0);
    Require(_sceUltReaderWriterLockCreate(&rwlock, "underscored", &pool, optParam, 0) == State);
    Require(sceUltReaderWriterLockLockRead(&rwlock) == Ok);
    Require(sceUltReaderWriterLockTryLockWrite(&rwlock) == Busy);
    Require(sceUltReaderWriterLockDestroy(&rwlock) == Busy);
    Require(sceUltReaderWriterLockUnlockRead(&rwlock) == Ok);
    Require(sceUltReaderWriterLockLockWrite(&rwlock) == Ok);
    Require(sceUltReaderWriterLockUnlockWrite(&rwlock) == Ok);
    Require(sceUltReaderWriterLockDestroy(&rwlock) == Ok);
    Require(sceUltWaitingQueueResourcePoolDestroy(&pool) == Ok);
}

struct alignas(8) UlthreadObject {
    std::uint8_t bytes[512];
};

struct alignas(8) UlthreadRuntimeObject {
    std::uint8_t bytes[4096];
};

std::atomic<int> gUnwritableSelf{0};
std::atomic<int> gUnwritableSelfErrno{0};
std::atomic<void*> gSelf{nullptr};
std::atomic<int> gYield{-1};
std::atomic<bool> gPastExit{false};
std::atomic<bool> gRelease{false};

std::int32_t APS5_VABI ExitingEntry(std::uint64_t status) {
    void* self = nullptr;
    errno = 71;
    gUnwritableSelf = sceUltUlthreadGetSelf(reinterpret_cast<void**>(0x10));
    gUnwritableSelfErrno = errno;
    if (sceUltUlthreadGetSelf(&self) == Ok) gSelf = self;
    gYield = sceUltUlthreadYield();
    sceUltUlthreadExit(static_cast<std::int32_t>(status));

    gPastExit = true;
    return -1;
}

std::int32_t APS5_VABI WaitingEntry(std::uint64_t) {
    WaitUntil(gRelease);
    return 0;
}

void Ulthreads() {
    void* self = &gSelf;
    Require(sceUltUlthreadGetSelf(nullptr) == Null);
    Require(sceUltUlthreadGetSelf(&self) == State && self == &gSelf);
    Require(sceUltUlthreadYield() == State);
    Require(sceUltUlthreadExit(1) == State);
    std::vector<std::uint8_t> workArea(sceUltUlthreadRuntimeGetWorkAreaSize(2, 1));
    UlthreadRuntimeObject runtime{};
    Require(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 2, 1, workArea.data(), nullptr, 0) == Ok);
    UlthreadObject first{};
    std::uint8_t context[4096];
    Require(sceUltUlthreadCreate(&first, "exits", ExitingEntry, 42, context, sizeof(context), &runtime, nullptr, 0) == Ok);
    UltUlthreadInfo threadQueryInfo{};
    Require(sceUltGetUlthreadInfo(&first, &threadQueryInfo) == Ok);
    Require(std::strcmp(threadQueryInfo.name, "exits") == 0);
    errno = 71;
    Require(sceUltGetUlthreadInfo(&first, reinterpret_cast<UltUlthreadInfo*>(0x10)) == Null);
    Require(errno == 71);
    std::int32_t status = 0;
    Require(sceUltUlthreadJoin(&first, &status) == Ok);
    Require(status == 42 && gSelf.load() == &first && gYield.load() == Ok && !gPastExit.load());
    Require(gUnwritableSelf.load() == Null && gUnwritableSelfErrno.load() == 71);
    UlthreadObject second{};
    gSelf = nullptr;
    Require(sceUltUlthreadCreate(&second, "tryjoin", ExitingEntry, 7, context, sizeof(context), &runtime, nullptr, 0) == Ok);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    int joined = Again;
    while (joined == Again && std::chrono::steady_clock::now() < deadline) {
        joined = sceUltUlthreadTryJoin(&second, &status);
        if (joined == Again) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(joined == Ok && status == 7 && gSelf.load() == &second);
    UlthreadObject waiting{};
    Require(sceUltUlthreadCreate(&waiting, "waits", WaitingEntry, 0, context, sizeof(context), &runtime, nullptr, 0) == Ok);
    UltUlthreadRuntimeInfo runtimeInfo{};
    errno = 71;
    Require(sceUltGetUlthreadRuntimeInfo(&runtime, &runtimeInfo, reinterpret_cast<void**>(0x10), 1, nullptr, 0) == Null);
    Require(errno == 71);
    void* active[1]{};
    Require(sceUltGetUlthreadRuntimeInfo(&runtime, &runtimeInfo, active, 1, nullptr, 0) == Ok);
    Require(runtimeInfo.activeUlthreads == 1 && active[0] == &waiting);
    gRelease = true;
    Require(sceUltUlthreadJoin(&waiting, &status) == Ok && status == 0);
    Require(sceUltUlthreadRuntimeDestroy(&runtime) == Ok);
}

void* CurrentThreadId() {
    std::uintptr_t id;
    const auto self = scePthreadSelf();
    std::memcpy(&id, self, sizeof(id));
    return reinterpret_cast<void*>(id);
}

template<typename TInfo, typename TQuery>
void CheckUnsupportedInfo(TQuery query, void* object, std::uint32_t capacity = 4) {
    TInfo info;
    std::memset(&info, 0xEE, sizeof(info));
    UltWaitingThreadInfo waiters[4];
    std::memset(waiters, 0xAA, sizeof(waiters));
    for (auto& waiter : waiters) waiter.size = sizeof(UltWaitingThreadInfo);
    std::uint8_t savedWaiters[sizeof(waiters)];
    std::memcpy(savedWaiters, waiters, sizeof(waiters));
    bool threw = false;
    try {
        query(object, &info, waiters, capacity);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&info);
    for (std::size_t index = 0; index < sizeof(info); ++index) Require(bytes[index] == 0xEE);
    Require(std::memcmp(waiters, savedWaiters, sizeof(waiters)) == 0);
}

template<typename TInfo, typename TQuery>
void CheckWaiterRecord(TQuery query, void* object, std::int32_t waitValue) {
    TInfo info;
    std::memset(&info, 0xEE, sizeof(info));
    UltWaitingThreadInfo waiter;
    std::memset(&waiter, 0xAA, sizeof(waiter));
    waiter.size = sizeof(UltWaitingThreadInfo);
    Require(query(object, &info, &waiter, 1) == Ok);
    Require(info.size == sizeof(TInfo));
    Require(waiter.size == sizeof(UltWaitingThreadInfo));
    Require(waiter.thread != nullptr && waiter.threadType == 1 && waiter.waitValue == waitValue);
}

template<typename TInfo, typename TQuery>
void CheckWaitingRecordSizes(TQuery query, void* object) {
    TInfo info;
    UltWaitingThreadInfo waiters[4];
    for (std::size_t invalid = 0; invalid < 4; ++invalid) {
        for (const std::uint64_t size : {std::uint64_t{0}, std::uint64_t{63}, std::uint64_t{65}, UINT64_MAX}) {
            std::memset(&info, 0xEE, sizeof(info));
            std::memset(waiters, 0xAA, sizeof(waiters));
            for (auto& waiter : waiters) waiter.size = sizeof(UltWaitingThreadInfo);
            waiters[invalid].size = size;
            std::uint8_t savedWaiters[sizeof(waiters)];
            std::memcpy(savedWaiters, waiters, sizeof(waiters));
            Require(query(object, &info, waiters, 4) == Invalid);
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(&info);
            for (std::size_t index = 0; index < sizeof(info); ++index) Require(bytes[index] == 0xEE);
            Require(std::memcmp(waiters, savedWaiters, sizeof(waiters)) == 0);
        }
    }
    Require(query(object, &info, waiters, 0) == Ok);
    for (auto& waiter : waiters) waiter.size = sizeof(UltWaitingThreadInfo);
    std::uint8_t savedWaiters[sizeof(waiters)];
    std::memcpy(savedWaiters, waiters, sizeof(waiters));
    Require(query(object, &info, waiters, 4) == Ok);
    Require(std::memcmp(waiters, savedWaiters, sizeof(waiters)) == 0);
}

void GetInfoTests() {
    alignas(8) std::uint8_t dummy[512]{};
    alignas(8) std::uint8_t poolObj[256]{};
    alignas(8) std::uint8_t mutexObj[256]{};
    alignas(8) std::uint8_t condObj[256]{};
    alignas(8) std::uint8_t semObj[256]{};
    alignas(8) std::uint8_t rwObj[256]{};
    alignas(8) std::uint8_t dataPoolObj[512]{};
    alignas(8) std::uint8_t queueObj[512]{};
    alignas(8) std::uint8_t rtObj[4096]{};

    UltMutexInfo mutexInfo{};
    UltConditionVariableInfo condInfo{};
    UltSemaphoreInfo semInfo{};
    UltReaderWriterLockInfo rwInfo{};
    UltWaitingQueueResourcePoolInfo poolInfo{};
    UltQueueDataResourcePoolInfo dataPoolInfo{};
    UltQueueInfo queueInfo{};
    UltUlthreadRuntimeInfo rtInfo{};
    UltUlthreadInfo threadInfo{};
    UltWaitingThreadInfo waitThreads[4]{};
    for (auto& waiter : waitThreads) waiter.size = sizeof(UltWaitingThreadInfo);

    Require(sceUltGetMutexInfo(nullptr, nullptr, nullptr, 0) == Null);
    Require(sceUltGetMutexInfo(mutexObj, nullptr, nullptr, 0) == Null);
    Require(sceUltGetMutexInfo(nullptr, &mutexInfo, nullptr, 0) == Null);
    Require(sceUltGetMutexInfo(mutexObj, &mutexInfo, nullptr, 1) == Null);
    Require(sceUltGetConditionVariableInfo(nullptr, nullptr, nullptr, 0) == Null);
    Require(sceUltGetSemaphoreInfo(nullptr, nullptr, nullptr, 0) == Null);
    Require(sceUltGetReaderWriterLockInfo(nullptr, nullptr, nullptr, 0) == Null);
    Require(sceUltGetQueueInfo(nullptr, nullptr, nullptr, 0) == Null);
    Require(sceUltGetWaitingQueueResourcePoolInfo(nullptr, nullptr) == Null);
    Require(sceUltGetQueueDataResourcePoolInfo(nullptr, nullptr) == Null);
    Require(sceUltGetUlthreadInfo(nullptr, nullptr) == Null);
    Require(sceUltGetUlthreadRuntimeInfo(nullptr, nullptr, nullptr, 0, nullptr, 0) == Null);
    Require(sceUltGetUlthreadRuntimeInfo(nullptr, &rtInfo, nullptr, 1, nullptr, 0) == Null);
    Require(sceUltGetUlthreadRuntimeInfo(nullptr, &rtInfo, nullptr, 0, nullptr, 1) == Null);

    Require(sceUltGetMutexInfo(dummy, &mutexInfo, nullptr, 0) == State);
    Require(sceUltGetConditionVariableInfo(dummy, &condInfo, nullptr, 0) == State);
    Require(sceUltGetSemaphoreInfo(dummy, &semInfo, nullptr, 0) == State);
    Require(sceUltGetReaderWriterLockInfo(dummy, &rwInfo, nullptr, 0) == State);
    Require(sceUltGetWaitingQueueResourcePoolInfo(dummy, &poolInfo) == State);
    Require(sceUltGetQueueDataResourcePoolInfo(dummy, &dataPoolInfo) == State);
    Require(sceUltGetQueueInfo(dummy, &queueInfo, nullptr, 0) == State);
    Require(sceUltGetUlthreadInfo(dummy, &threadInfo) == State);
    Require(sceUltGetUlthreadRuntimeInfo(dummy, &rtInfo, nullptr, 0, nullptr, 0) == State);

    Require(sceUltWaitingQueueResourcePoolCreate(poolObj, "test-pool", 4, 4, nullptr, nullptr, 0) == Ok);
    Require(sceUltGetWaitingQueueResourcePoolInfo(poolObj, &poolInfo) == Ok);
    errno = 71;
    Require(sceUltGetWaitingQueueResourcePoolInfo(poolObj, reinterpret_cast<UltWaitingQueueResourcePoolInfo*>(0x10)) == Null);
    Require(errno == 71);
    Require(poolInfo.size == sizeof(UltWaitingQueueResourcePoolInfo));
    Require(poolInfo.object == poolObj);
    Require(std::strcmp(poolInfo.name, "test-pool") == 0);
    Require(poolInfo.maxThreads == 4);
    Require(poolInfo.maxSyncObjects == 4);

    Require(sceUltMutexCreate(mutexObj, "test-mutex", poolObj, nullptr, 0) == Ok);
    Require(sceUltGetMutexInfo(mutexObj, &mutexInfo, waitThreads, 4) == Ok);
    errno = 71;
    Require(sceUltGetMutexInfo(mutexObj, reinterpret_cast<UltMutexInfo*>(0x10), waitThreads, 4) == Null);
    Require(errno == 71);
    Require(sceUltGetMutexInfo(mutexObj, &mutexInfo, reinterpret_cast<UltWaitingThreadInfo*>(0x10), 4) == Null);
    Require(errno == 71);
    Require(mutexInfo.size == sizeof(UltMutexInfo));
    Require(mutexInfo.object == mutexObj);
    Require(std::strcmp(mutexInfo.name, "test-mutex") == 0);
    Require(mutexInfo.waitingQueuePool == poolObj);
    Require(mutexInfo.lockCount == 0);
    Require(mutexInfo.waitingThreads == 0);
    Require(mutexInfo.owner == nullptr && mutexInfo.ownerType == 0);
    CheckWaitingRecordSizes<UltMutexInfo>(sceUltGetMutexInfo, mutexObj);

    Require(sceUltMutexLock(mutexObj) == Ok);
    Require(sceUltGetMutexInfo(mutexObj, &mutexInfo, nullptr, 0) == Ok);
    Require(mutexInfo.lockCount == 0);
    const auto owner = CurrentThreadId();
    Require(owner != nullptr && mutexInfo.owner == owner && mutexInfo.ownerType == 1);
    std::thread([&] {
        Require(CurrentThreadId() != owner);
        UltMutexInfo held{};
        Require(sceUltGetMutexInfo(mutexObj, &held, nullptr, 0) == Ok);
        Require(held.owner == owner && held.ownerType == 1 && held.lockCount == 0);
    }).join();
    Require(sceUltMutexUnlock(mutexObj) == Ok);
    Require(sceUltGetMutexInfo(mutexObj, &mutexInfo, nullptr, 0) == Ok);
    Require(mutexInfo.owner == nullptr && mutexInfo.ownerType == 0 && mutexInfo.lockCount == 0);

    Object recursiveMutex{};
    UltMutexOptParam recursiveOptions{};
    recursiveOptions.attribute = 1;
    Require(sceUltMutexCreate(&recursiveMutex, "recursive-info", poolObj, &recursiveOptions, 0) == Ok);
    Require(sceUltMutexLock(&recursiveMutex) == Ok);
    CheckUnsupportedInfo<UltMutexInfo>(sceUltGetMutexInfo, &recursiveMutex, 0);
    Require(sceUltMutexUnlock(&recursiveMutex) == Ok);
    Require(sceUltMutexDestroy(&recursiveMutex) == Ok);

    Require(sceUltConditionVariableCreate(condObj, "test-cnd", mutexObj, nullptr, 0) == Ok);
    Require(sceUltGetConditionVariableInfo(condObj, &condInfo, waitThreads, 4) == Ok);
    errno = 71;
    Require(sceUltGetConditionVariableInfo(condObj, reinterpret_cast<UltConditionVariableInfo*>(0x10), waitThreads, 4) == Null);
    Require(errno == 71);
    Require(condInfo.size == sizeof(UltConditionVariableInfo));
    Require(condInfo.object == condObj);
    Require(condInfo.mutex == mutexObj);
    Require(std::strcmp(condInfo.name, "test-cnd") == 0);
    Require(condInfo.waitingThreads == 0);

    Require(sceUltSemaphoreCreate(semObj, "test-sem", 3, poolObj, nullptr, 0) == Ok);
    Require(sceUltGetSemaphoreInfo(semObj, &semInfo, waitThreads, 4) == Ok);
    Require(semInfo.size == sizeof(UltSemaphoreInfo));
    Require(semInfo.object == semObj);
    Require(std::strcmp(semInfo.name, "test-sem") == 0);
    Require(semInfo.waitingQueuePool == poolObj);
    Require(semInfo.resources == 3);
    errno = 71;
    Require(sceUltGetSemaphoreInfo(semObj, reinterpret_cast<UltSemaphoreInfo*>(0x10), waitThreads, 4) == Null);
    Require(errno == 71);

    Require(sceUltReaderWriterLockCreate(rwObj, "test-rw", poolObj, nullptr, 0) == Ok);
    Require(sceUltGetReaderWriterLockInfo(rwObj, &rwInfo, waitThreads, 4) == Ok);
    errno = 71;
    Require(sceUltGetReaderWriterLockInfo(rwObj, reinterpret_cast<UltReaderWriterLockInfo*>(0x10), waitThreads, 4) == Null);
    Require(errno == 71);
    Require(sceUltGetReaderWriterLockInfo(rwObj, &rwInfo, reinterpret_cast<UltWaitingThreadInfo*>(0x10), 4) == Null);
    Require(errno == 71);
    Require(rwInfo.size == sizeof(UltReaderWriterLockInfo));
    Require(rwInfo.object == rwObj);
    Require(std::strcmp(rwInfo.name, "test-rw") == 0);
    Require(rwInfo.waitingQueuePool == poolObj);
    Require(rwInfo.readers == 0);
    Require(rwInfo.lockType == 0);
    Require(rwInfo.owner == nullptr && rwInfo.ownerType == 0);
    CheckWaitingRecordSizes<UltReaderWriterLockInfo>(sceUltGetReaderWriterLockInfo, rwObj);

    Require(sceUltReaderWriterLockLockRead(rwObj) == Ok);
    Require(sceUltGetReaderWriterLockInfo(rwObj, &rwInfo, nullptr, 0) == Ok);
    Require(rwInfo.readers == 1);
    Require(rwInfo.lockType == 1);
    Require(rwInfo.owner == nullptr && rwInfo.ownerType == 0);
    Require(sceUltReaderWriterLockLockRead(rwObj) == Ok);
    Require(sceUltGetReaderWriterLockInfo(rwObj, &rwInfo, nullptr, 0) == Ok);
    Require(rwInfo.lockType == 1 && rwInfo.readers == 2 && rwInfo.owner == nullptr && rwInfo.ownerType == 0);
    Require(sceUltReaderWriterLockUnlockRead(rwObj) == Ok);
    Require(sceUltReaderWriterLockUnlockRead(rwObj) == Ok);
    Require(sceUltGetReaderWriterLockInfo(rwObj, &rwInfo, nullptr, 0) == Ok);
    Require(rwInfo.lockType == 0 && rwInfo.readers == 0 && rwInfo.owner == nullptr && rwInfo.ownerType == 0);
    for (const auto writeLock : {sceUltReaderWriterLockLockWrite, sceUltReaderWriterLockTryLockWrite}) {
        Require(writeLock(rwObj) == Ok);
        std::thread([&] {
            UltReaderWriterLockInfo held{};
            Require(sceUltGetReaderWriterLockInfo(rwObj, &held, nullptr, 0) == Ok);
            Require(held.lockType == 2 && held.readers == 0 && held.owner == owner && held.ownerType == 1);
        }).join();
        Require(sceUltReaderWriterLockUnlockWrite(rwObj) == Ok);
        Require(sceUltGetReaderWriterLockInfo(rwObj, &rwInfo, nullptr, 0) == Ok);
        Require(rwInfo.lockType == 0 && rwInfo.owner == nullptr && rwInfo.ownerType == 0);
    }

    Require(sceUltQueueDataResourcePoolCreate(dataPoolObj, "test-datapool", 8, 64, 2, poolObj, nullptr, nullptr, 0) == Ok);
    Require(sceUltGetQueueDataResourcePoolInfo(dataPoolObj, &dataPoolInfo) == Ok);
    errno = 71;
    Require(sceUltGetQueueDataResourcePoolInfo(dataPoolObj, reinterpret_cast<UltQueueDataResourcePoolInfo*>(0x10)) == Null);
    Require(errno == 71);
    Require(dataPoolInfo.size == sizeof(UltQueueDataResourcePoolInfo));
    Require(dataPoolInfo.object == dataPoolObj);
    Require(std::strcmp(dataPoolInfo.name, "test-datapool") == 0);
    Require(dataPoolInfo.waitingQueuePool == poolObj);
    Require(dataPoolInfo.dataSize == 64);
    Require(dataPoolInfo.maxItems == 8);
    Require(dataPoolInfo.availableItems == 8);
    Require(dataPoolInfo.maxQueues == 2);

    Require(sceUltQueueCreate(queueObj, "test-queue", 64, poolObj, dataPoolObj, nullptr, 0) == Ok);
    Require(sceUltGetQueueInfo(queueObj, &queueInfo, waitThreads, 4) == Ok);
    errno = 71;
    Require(sceUltGetQueueInfo(queueObj, reinterpret_cast<UltQueueInfo*>(0x10), waitThreads, 4) == Null);
    Require(errno == 71);
    Require(queueInfo.size == sizeof(UltQueueInfo));
    Require(queueInfo.object == queueObj);
    Require(std::strcmp(queueInfo.name, "test-queue") == 0);
    Require(queueInfo.waitingQueuePool == poolObj);
    Require(queueInfo.dataPool == dataPoolObj);
    Require(queueInfo.dataSize == 64);
    Require(queueInfo.queuedItems == 0);

    std::uint8_t itemData[64]{0x42};
    Require(sceUltQueuePush(queueObj, itemData) == Ok);
    Require(sceUltGetQueueInfo(queueObj, &queueInfo, nullptr, 0) == Ok);
    Require(queueInfo.queuedItems == 1);
    Require(sceUltQueuePop(queueObj, itemData) == Ok);
    Require(sceUltGetQueueInfo(queueObj, &queueInfo, nullptr, 0) == Ok);
    Require(queueInfo.queuedItems == 0);

    Require(sceUltUlthreadRuntimeCreate(rtObj, "test-rt", 2, 1, nullptr, nullptr, 0) == Ok);
    void* activeThreads[4]{};
    Require(sceUltGetUlthreadRuntimeInfo(rtObj, &rtInfo, activeThreads, 4, nullptr, 0) == Ok);
    errno = 71;
    Require(sceUltGetUlthreadRuntimeInfo(rtObj, reinterpret_cast<UltUlthreadRuntimeInfo*>(0x10), nullptr, 0, nullptr, 0) == Null);
    Require(errno == 71);
    Require(rtInfo.size == sizeof(UltUlthreadRuntimeInfo));
    Require(rtInfo.object == rtObj);
    Require(std::strcmp(rtInfo.name, "test-rt") == 0);
    Require(rtInfo.maxUlthreads == 2);
    Require(rtInfo.workerThreads == 1);
    Require(rtInfo.activeUlthreads == 0);

    Pthread workerHandles[2]{};
    bool workerQueryThrew = false;
    try {
        sceUltGetUlthreadRuntimeInfo(rtObj, &rtInfo, nullptr, 0, workerHandles, 2);
    } catch (const std::runtime_error&) {
        workerQueryThrew = true;
    }
    Require(workerQueryThrew);

    UltUlthreadRuntimeInfo untouchedInfo{};
    std::memset(&untouchedInfo, 0xEE, sizeof(untouchedInfo));
    Require(sceUltGetUlthreadRuntimeInfo(rtObj, &untouchedInfo, nullptr, 0, nullptr, 1) == Null);
    const auto* infoBytes = reinterpret_cast<const std::uint8_t*>(&untouchedInfo);
    for (std::size_t i = 0; i < sizeof(untouchedInfo); ++i) {
        Require(infoBytes[i] == 0xEE);
    }

    Require(sceUltConditionVariableDestroy(condObj) == Ok);
    Require(sceUltMutexDestroy(mutexObj) == Ok);
    Require(sceUltSemaphoreDestroy(semObj) == Ok);
    Require(sceUltReaderWriterLockDestroy(rwObj) == Ok);
    Require(sceUltQueueDestroy(queueObj) == Ok);
    Require(sceUltQueueDataResourcePoolDestroy(dataPoolObj) == Ok);
    Require(sceUltWaitingQueueResourcePoolDestroy(poolObj) == Ok);
    Require(sceUltUlthreadRuntimeDestroy(rtObj) == Ok);
}

void WaitForQueueWaiters(std::uint8_t* queue, UltQueueInfo* info, std::uint32_t pushers, std::uint32_t poppers) {
    for (int attempt = 0; attempt < 4000; ++attempt) {
        Require(sceUltGetQueueInfo(queue, info, nullptr, 0) == Ok);
        if (info->waitingPushers == pushers && info->waitingPoppers == poppers) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(false);
}

void QueueWaiters() {
    alignas(8) std::uint8_t poolObj[256]{};
    alignas(8) std::uint8_t dataPoolObj[512]{};
    alignas(8) std::uint8_t queueObj[512]{};
    UltQueueInfo info{};
    UltWaitingThreadInfo waiters[4]{};

    Require(sceUltWaitingQueueResourcePoolCreate(poolObj, "waiter-pool", 4, 4, nullptr, nullptr, 0) == Ok);
    Require(sceUltQueueDataResourcePoolCreate(dataPoolObj, "waiter-data", 2, 64, 1, poolObj, nullptr, nullptr, 0) == Ok);
    Require(sceUltQueueCreate(queueObj, "waiter-queue", 64, poolObj, dataPoolObj, nullptr, 0) == Ok);

    std::uint8_t item[64]{0x11};
    std::uint8_t popped[64]{};

    std::atomic<int> popResult{1};
    std::thread popper([&] { popResult.store(sceUltQueuePop(queueObj, popped)); });
    WaitForQueueWaiters(queueObj, &info, 0, 1);
    Require(info.queuedItems == 0);
    std::memset(waiters, 0xAA, sizeof(waiters));
    CheckWaiterRecord<UltQueueInfo>(sceUltGetQueueInfo, queueObj, 1);
    Require(sceUltGetQueueInfo(queueObj, &info, waiters, 0) == Ok);
    const auto* waiterBytes = reinterpret_cast<const std::uint8_t*>(waiters);
    for (std::size_t index = 0; index < sizeof(waiters); ++index) Require(waiterBytes[index] == 0xAA);
    Require(sceUltQueueDestroy(queueObj) == State);
    Require(sceUltQueuePush(queueObj, item) == Ok);
    popper.join();
    Require(popResult.load() == Ok);
    Require(popped[0] == 0x11);
    WaitForQueueWaiters(queueObj, &info, 0, 0);

    Require(sceUltQueuePush(queueObj, item) == Ok);
    Require(sceUltQueuePush(queueObj, item) == Ok);

    std::atomic<int> pushResult{1};
    std::thread pusher([&] { pushResult.store(sceUltQueuePush(queueObj, item)); });
    WaitForQueueWaiters(queueObj, &info, 1, 0);
    Require(info.queuedItems == 2);
    CheckWaiterRecord<UltQueueInfo>(sceUltGetQueueInfo, queueObj, 2);
    Require(sceUltGetQueueInfo(queueObj, &info, waiters, 0) == Ok);
    Require(info.waitingPushers == 1);
    Require(info.waitingPoppers == 0);
    Require(sceUltQueuePop(queueObj, popped) == Ok);
    pusher.join();
    Require(pushResult.load() == Ok);
    WaitForQueueWaiters(queueObj, &info, 0, 0);
    Require(info.queuedItems == 2);

    Require(sceUltQueuePop(queueObj, popped) == Ok);
    Require(sceUltQueuePop(queueObj, popped) == Ok);
    Require(sceUltQueueDestroy(queueObj) == Ok);
    Require(sceUltQueueDataResourcePoolDestroy(dataPoolObj) == Ok);
    Require(sceUltWaitingQueueResourcePoolDestroy(poolObj) == Ok);
}

template<typename TInfo, typename TQuery>
void WaitForInfoWaiters(TQuery query, void* object) {
    for (int attempt = 0; attempt < 4000; ++attempt) {
        TInfo info{};
        Require(query(object, &info, nullptr, 0) == Ok);
        if (info.waitingThreads == 1) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(false);
}

void SemaphoreValidation() {
    Object pool{};
    Object semaphore{};
    Object unknown{};
    Require(sceUltWaitingQueueResourcePoolCreate(&pool, "validation-pool", 4, 4, nullptr, nullptr, 0) == Ok);
    Require(sceUltSemaphoreCreate(&semaphore, "validation-semaphore", 1, &pool, nullptr, 0) == Ok);
    using Call = decltype(&sceUltSemaphoreAcquire);
    for (const Call call : {sceUltSemaphoreAcquire, sceUltSemaphoreTryAcquire, sceUltSemaphoreRelease}) {
        Require(call(nullptr, 0) == Null);
        Require(call(semaphore.bytes + 1, 0) == Alignment);
        Require(call(&semaphore, 0) == Range);
        Require(call(&semaphore, -1) == Range);
        Require(call(&unknown, 1) == State);
    }
    Require(sceUltSemaphoreTryAcquire(&semaphore, 1) == Ok);
    Require(sceUltSemaphoreRelease(&semaphore, 1) == Ok);
    Require(sceUltSemaphoreDestroy(&semaphore) == Ok);
    Require(sceUltWaitingQueueResourcePoolDestroy(&pool) == Ok);
}

void InfoWaiters() {
    Object pool{};
    Object mutex{};
    Object condition{};
    Object semaphore{};
    Object rwlock{};
    Require(sceUltWaitingQueueResourcePoolCreate(&pool, "info-pool", 4, 4, nullptr, nullptr, 0) == Ok);
    Require(sceUltMutexCreate(&mutex, "info-mutex", &pool, nullptr, 0) == Ok);
    Require(sceUltConditionVariableCreate(&condition, "info-condition", &mutex, nullptr, 0) == Ok);
    Require(sceUltSemaphoreCreate(&semaphore, "info-semaphore", 0, &pool, nullptr, 0) == Ok);
    Require(sceUltReaderWriterLockCreate(&rwlock, "info-rwlock", &pool, nullptr, 0) == Ok);

    Require(sceUltMutexLock(&mutex) == Ok);
    std::thread mutexWaiter([&] {
        Require(sceUltMutexLock(&mutex) == Ok);
        Require(sceUltMutexUnlock(&mutex) == Ok);
    });
    WaitForInfoWaiters<UltMutexInfo>(sceUltGetMutexInfo, &mutex);
    CheckWaiterRecord<UltMutexInfo>(sceUltGetMutexInfo, &mutex, 0);
    Require(sceUltMutexUnlock(&mutex) == Ok);
    mutexWaiter.join();
    UltMutexInfo mutexAfter{};
    Require(sceUltGetMutexInfo(&mutex, &mutexAfter, nullptr, 0) == Ok && mutexAfter.waitingThreads == 0);

    std::thread conditionWaiter([&] {
        Require(sceUltMutexLock(&mutex) == Ok);
        Require(sceUltConditionVariableWait(&condition) == Ok);
        UltMutexInfo info{};
        Require(sceUltGetMutexInfo(&mutex, &info, nullptr, 0) == Ok);
        Require(info.owner == CurrentThreadId() && info.ownerType == 1);
        Require(sceUltMutexUnlock(&mutex) == Ok);
    });
    WaitForInfoWaiters<UltConditionVariableInfo>(sceUltGetConditionVariableInfo, &condition);
    CheckWaiterRecord<UltConditionVariableInfo>(sceUltGetConditionVariableInfo, &condition, 0);
    Require(sceUltMutexLock(&mutex) == Ok);
    Require(sceUltConditionVariableSignal(&condition) == Ok);
    UltConditionVariableInfo signaled{};
    UltWaitingThreadInfo signaledRecord{};
    std::memset(&signaledRecord, 0xAA, sizeof(signaledRecord));
    signaledRecord.size = sizeof(UltWaitingThreadInfo);
    const UltWaitingThreadInfo poisoned = signaledRecord;
    Require(sceUltGetConditionVariableInfo(&condition, &signaled, &signaledRecord, 1) == Ok);
    Require(signaled.waitingThreads == 0);
    Require(std::memcmp(&poisoned, &signaledRecord, sizeof(poisoned)) == 0);
    Require(sceUltMutexUnlock(&mutex) == Ok);
    conditionWaiter.join();

    std::thread semaphoreWaiter([&] { Require(sceUltSemaphoreAcquire(&semaphore, 1) == Ok); });
    WaitForInfoWaiters<UltSemaphoreInfo>(sceUltGetSemaphoreInfo, &semaphore);
    CheckWaiterRecord<UltSemaphoreInfo>(sceUltGetSemaphoreInfo, &semaphore, 0);
    Require(sceUltSemaphoreRelease(&semaphore, 1) == Ok);
    semaphoreWaiter.join();

    Require(sceUltReaderWriterLockLockWrite(&rwlock) == Ok);
    std::thread reader([&] {
        Require(sceUltReaderWriterLockLockRead(&rwlock) == Ok);
        Require(sceUltReaderWriterLockUnlockRead(&rwlock) == Ok);
    });
    WaitForInfoWaiters<UltReaderWriterLockInfo>(sceUltGetReaderWriterLockInfo, &rwlock);
    CheckWaiterRecord<UltReaderWriterLockInfo>(sceUltGetReaderWriterLockInfo, &rwlock, 1);
    Require(sceUltReaderWriterLockUnlockWrite(&rwlock) == Ok);
    reader.join();

    Require(sceUltReaderWriterLockLockRead(&rwlock) == Ok);
    std::thread writer([&] {
        Require(sceUltReaderWriterLockLockWrite(&rwlock) == Ok);
        Require(sceUltReaderWriterLockUnlockWrite(&rwlock) == Ok);
    });
    WaitForInfoWaiters<UltReaderWriterLockInfo>(sceUltGetReaderWriterLockInfo, &rwlock);
    CheckWaiterRecord<UltReaderWriterLockInfo>(sceUltGetReaderWriterLockInfo, &rwlock, 2);
    Require(sceUltReaderWriterLockUnlockRead(&rwlock) == Ok);
    writer.join();

    Require(sceUltReaderWriterLockDestroy(&rwlock) == Ok);
    Require(sceUltSemaphoreDestroy(&semaphore) == Ok);
    Require(sceUltConditionVariableDestroy(&condition) == Ok);
    Require(sceUltMutexDestroy(&mutex) == Ok);
    Require(sceUltWaitingQueueResourcePoolDestroy(&pool) == Ok);
}

void UnreadableNamesAndObjects() {
    const auto* unreadable = reinterpret_cast<const char*>(0x10);
    void* const unwritable = reinterpret_cast<void*>(0x10);
    Object pool{};
    Object mutex{};
    Object named{};
    Object condition{};
    Object semaphore{};
    Object rwlock{};
    alignas(8) std::uint8_t dataPool[512]{};
    alignas(8) std::uint8_t queue[512]{};
    alignas(8) std::uint8_t runtime[4096]{};
    alignas(8) std::uint8_t ulthread[512]{};
    UltMutexInfo mutexInfo{};
    UltWaitingQueueResourcePoolInfo poolInfo{};
    UltQueueDataResourcePoolInfo dataPoolInfo{};
    UltUlthreadRuntimeInfo runtimeInfo{};
    UltUlthreadInfo threadInfo{};

    errno = 71;
    Require(sceUltWaitingQueueResourcePoolCreate(&pool, unreadable, 4, 4, nullptr, nullptr, 0) == Null);
    Require(sceUltGetWaitingQueueResourcePoolInfo(&pool, &poolInfo) == State);
    Require(sceUltWaitingQueueResourcePoolCreate(unwritable, "pool", 4, 4, nullptr, nullptr, 0) == Null);
    Require(sceUltGetWaitingQueueResourcePoolInfo(unwritable, &poolInfo) == State);
    Require(sceUltWaitingQueueResourcePoolCreate(&pool, "pool", 4, 4, nullptr, nullptr, 0) == Ok);

    Require(sceUltMutexCreate(&mutex, unreadable, &pool, nullptr, 0) == Null);
    Require(sceUltMutexCreate(unwritable, "mutex", &pool, nullptr, 0) == Null);
    Require(sceUltGetMutexInfo(unwritable, &mutexInfo, nullptr, 0) == State);
    Require(sceUltMutexCreate(&mutex, "mutex", &pool, nullptr, 0) == Ok);
    Require(sceUltMutexCreate(&named, "a-mutex-name-longer-than-thirty-one-bytes", &pool, nullptr, 0) == Ok);
    Require(sceUltGetMutexInfo(&named, &mutexInfo, nullptr, 0) == Ok);
    Require(std::strcmp(mutexInfo.name, "a-mutex-name-longer-than-thirty") == 0);

    Require(sceUltConditionVariableCreate(&condition, unreadable, &mutex, nullptr, 0) == Null);
    Require(sceUltConditionVariableCreate(&condition, "condition", &mutex, nullptr, 0) == Ok);

    Require(sceUltSemaphoreCreate(&semaphore, unreadable, 1, &pool, nullptr, 0) == Null);
    Require(sceUltSemaphoreCreate(unwritable, "semaphore", 1, &pool, nullptr, 0) == Null);
    Require(sceUltSemaphoreCreate(&semaphore, "semaphore", 1, &pool, nullptr, 0) == Ok);

    Require(sceUltReaderWriterLockCreate(&rwlock, unreadable, &pool, nullptr, 0) == Null);
    Require(sceUltReaderWriterLockCreate(unwritable, "rwlock", &pool, nullptr, 0) == Null);
    Require(sceUltReaderWriterLockCreate(&rwlock, "rwlock", &pool, nullptr, 0) == Ok);

    Require(sceUltQueueDataResourcePoolCreate(dataPool, unreadable, 8, 64, 1, &pool, nullptr, nullptr, 0) == Null);
    Require(sceUltQueueDataResourcePoolCreate(unwritable, "data", 8, 64, 1, &pool, nullptr, nullptr, 0) == Null);
    Require(sceUltGetQueueDataResourcePoolInfo(unwritable, &dataPoolInfo) == State);
    Require(sceUltQueueDataResourcePoolCreate(dataPool, "data", 8, 64, 1, &pool, nullptr, nullptr, 0) == Ok);

    Require(sceUltQueueCreate(queue, unreadable, 64, &pool, dataPool, nullptr, 0) == Null);
    Require(sceUltQueueCreate(unwritable, "queue", 64, &pool, dataPool, nullptr, 0) == Null);
    Require(sceUltQueueCreate(queue, "queue", 64, &pool, dataPool, nullptr, 0) == Ok);

    Require(sceUltUlthreadRuntimeCreate(runtime, unreadable, 2, 1, nullptr, nullptr, 0) == Null);
    Require(sceUltUlthreadRuntimeCreate(unwritable, "runtime", 2, 1, nullptr, nullptr, 0) == Null);
    Require(sceUltGetUlthreadRuntimeInfo(unwritable, &runtimeInfo, nullptr, 0, nullptr, 0) == State);
    Require(sceUltUlthreadRuntimeCreate(runtime, "runtime", 2, 1, nullptr, nullptr, 0) == Ok);

    Require(sceUltUlthreadCreate(ulthread, unreadable, ExitingEntry, 0, nullptr, 0, runtime, nullptr, 0) == Null);
    Require(sceUltGetUlthreadInfo(ulthread, &threadInfo) == State);
    Require(sceUltUlthreadCreate(unwritable, "ulthread", ExitingEntry, 0, nullptr, 0, runtime, nullptr, 0) == Null);
    Require(sceUltGetUlthreadInfo(unwritable, &threadInfo) == State);
    Require(errno == 71);

    Require(sceUltUlthreadRuntimeDestroy(runtime) == Ok);
    Require(sceUltQueueDestroy(queue) == Ok);
    Require(sceUltQueueDataResourcePoolDestroy(dataPool) == Ok);
    Require(sceUltReaderWriterLockDestroy(&rwlock) == Ok);
    Require(sceUltSemaphoreDestroy(&semaphore) == Ok);
    Require(sceUltConditionVariableDestroy(&condition) == Ok);
    Require(sceUltMutexDestroy(&named) == Ok);
    Require(sceUltMutexDestroy(&mutex) == Ok);
    Require(sceUltWaitingQueueResourcePoolDestroy(&pool) == Ok);
}

}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    Require(sceUltInitialize() == Ok);
    if (std::strcmp(argv[1], "mutex-trylock") == 0) MutexTryLock();
    else if (std::strcmp(argv[1], "opt-params") == 0) {
        OptParams();
        MutexAndRuntimeOptParams();
        UnreadableOptParams();
    }
    else if (std::strcmp(argv[1], "rwlock-validation") == 0) ReaderWriterValidation();
    else if (std::strcmp(argv[1], "rwlock-readers") == 0) ReaderWriterReaders();
    else if (std::strcmp(argv[1], "rwlock-writer") == 0) ReaderWriterWriter();
    else if (std::strcmp(argv[1], "rwlock-pool") == 0) ReaderWriterPool();
    else if (std::strcmp(argv[1], "rwlock-finalize") == 0) ReaderWriterFinalize();
    else if (std::strcmp(argv[1], "rwlock-underscored") == 0) ReaderWriterUnderscored();
    else if (std::strcmp(argv[1], "ulthreads") == 0) Ulthreads();
    else if (std::strcmp(argv[1], "get-info") == 0) GetInfoTests();
    else if (std::strcmp(argv[1], "queue-waiters") == 0) QueueWaiters();
    else if (std::strcmp(argv[1], "info-waiters") == 0) InfoWaiters();
    else if (std::strcmp(argv[1], "semaphore-validation") == 0) SemaphoreValidation();
    else if (std::strcmp(argv[1], "guest-pointers") == 0) UnreadableNamesAndObjects();
    else return 2;
    Require(sceUltFinalize() == Ok);
    std::puts("ULT sync tests passed");
    return 0;
}
