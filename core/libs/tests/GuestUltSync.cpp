#include "SceTypes.hpp"
#include "SceUltTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <thread>
#include <vector>

extern "C" {
int APS5_VABI sceUltInitialize();
int APS5_VABI sceUltFinalize();
int APS5_VABI sceUltMutexCreate(void*, const char*, void*, const UltMutexOptParam*, std::uint32_t);
int APS5_VABI sceUltMutexLock(void*);
int APS5_VABI sceUltMutexTryLock(void*);
int APS5_VABI sceUltMutexUnlock(void*);
int APS5_VABI sceUltMutexDestroy(void*);
int APS5_VABI sceUltSemaphoreOptParamInitialize(void*, std::uint32_t);
int APS5_VABI sceUltQueueOptParamInitialize(void*, std::uint32_t);
int APS5_VABI sceUltQueueDataResourcePoolOptParamInitialize(void*, std::uint32_t);
int APS5_VABI sceUltWaitingQueueResourcePoolOptParamInitialize(void*, std::uint32_t);
int APS5_VABI sceUltUlthreadOptParamInitialize(void*, std::uint32_t);
int APS5_VABI _sceUltWaitingQueueResourcePoolCreate(void*, const char*, std::uint32_t, std::uint32_t, void*, const void*);
int APS5_VABI sceUltWaitingQueueResourcePoolDestroy(void*);
int APS5_VABI sceUltReaderWriterLockOptParamInitialize(void*, std::uint32_t);
int APS5_VABI sceUltConditionVariableOptParamInitialize(void*, std::uint32_t);
int APS5_VABI _sceUltConditionVariableOptParamInitialize(void*, std::uint32_t);
int APS5_VABI sceUltReaderWriterLockCreate(void*, const char*, void*, const void*, std::uint32_t);
int APS5_VABI sceUltReaderWriterLockDestroy(void*);
int APS5_VABI sceUltReaderWriterLockLockRead(void*);
int APS5_VABI sceUltReaderWriterLockTryLockRead(void*);
int APS5_VABI sceUltReaderWriterLockUnlockRead(void*);
int APS5_VABI sceUltReaderWriterLockLockWrite(void*);
int APS5_VABI sceUltReaderWriterLockTryLockWrite(void*);
int APS5_VABI sceUltReaderWriterLockUnlockWrite(void*);
std::uint64_t APS5_VABI sceUltUlthreadRuntimeGetWorkAreaSize(std::uint32_t, std::uint32_t);
int APS5_VABI sceUltUlthreadRuntimeCreate(void*, const char*, std::uint32_t, std::uint32_t, void*, const void*, std::uint32_t);
int APS5_VABI sceUltUlthreadRuntimeDestroy(void*);
int APS5_VABI sceUltUlthreadCreate(void*, const char*, UltUlthreadEntry, std::uint64_t, void*, std::uint64_t, void*, const void*, std::uint32_t);
int APS5_VABI sceUltUlthreadJoin(void*, std::int32_t*);
int APS5_VABI sceUltUlthreadTryJoin(void*, std::int32_t*);
int APS5_VABI sceUltUlthreadGetSelf(void**);
int APS5_VABI sceUltUlthreadYield();
int APS5_VABI sceUltUlthreadExit(std::int32_t);
int APS5_VABI sceUltConditionVariableCreate(void*, const char*, void*, const void*, std::uint32_t);
int APS5_VABI sceUltConditionVariableDestroy(void*);
int APS5_VABI sceUltSemaphoreCreate(void*, const char*, std::uint32_t, void*, const void*, std::uint32_t);
int APS5_VABI sceUltSemaphoreDestroy(void*);
int APS5_VABI sceUltQueueDataResourcePoolCreate(void*, const char*, std::uint32_t, std::uint64_t, std::uint32_t, void*, void*, const void*, std::uint32_t);
int APS5_VABI sceUltQueueDataResourcePoolDestroy(void*);
int APS5_VABI sceUltQueueCreate(void*, const char*, std::uint64_t, void*, void*, const void*, std::uint32_t);
int APS5_VABI sceUltQueuePush(void*, const void*);
int APS5_VABI sceUltQueuePop(void*, void*);
int APS5_VABI sceUltQueueDestroy(void*);
int APS5_VABI sceUltWaitingQueueResourcePoolCreate(void*, const char*, std::uint32_t, std::uint32_t, void*, const void*, std::uint32_t);
int APS5_VABI sceUltGetConditionVariableInfo(void*, UltConditionVariableInfo*, UltWaitingThreadInfo*, std::uint32_t);
int APS5_VABI sceUltGetMutexInfo(void*, UltMutexInfo*, UltWaitingThreadInfo*, std::uint32_t);
int APS5_VABI sceUltGetSemaphoreInfo(void*, UltSemaphoreInfo*, UltWaitingThreadInfo*, std::uint32_t);
int APS5_VABI sceUltGetReaderWriterLockInfo(void*, UltReaderWriterLockInfo*, UltWaitingThreadInfo*, std::uint32_t);
int APS5_VABI sceUltGetQueueInfo(void*, UltQueueInfo*, UltWaitingThreadInfo*, std::uint32_t);
int APS5_VABI sceUltGetWaitingQueueResourcePoolInfo(void*, UltWaitingQueueResourcePoolInfo*);
int APS5_VABI sceUltGetQueueDataResourcePoolInfo(void*, UltQueueDataResourcePoolInfo*);
int APS5_VABI sceUltGetUlthreadInfo(void*, UltUlthreadInfo*);
int APS5_VABI sceUltGetUlthreadRuntimeInfo(void*, UltUlthreadRuntimeInfo*, void**, std::uint32_t, Pthread*, std::uint32_t);
}

namespace {

constexpr int Ok = 0;
constexpr int Null = -2139029503;
constexpr int Invalid = -2139029500;
constexpr int Permission = -2139029499;
constexpr int Again = -2139029496;
constexpr int State = -2139029498;
constexpr int Busy = -2139029497;

void Require(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "ULT sync check failed at line %d\n", line);
    std::abort();
}

#define CHECK(value) Require((value), __LINE__)

struct alignas(8) Object {
    std::uint8_t bytes[256];
};

void MutexTryLock() {
    Object mutex{};
    CHECK(sceUltMutexTryLock(nullptr) == Null);
    CHECK(sceUltMutexTryLock(&mutex) == State);
    UltMutexOptParam options{};
    options.attribute = 1;
    CHECK(sceUltMutexCreate(&mutex, "trylock", nullptr, &options, 0) == Ok);
    CHECK(sceUltMutexTryLock(&mutex) == Ok);
    CHECK(sceUltMutexTryLock(&mutex) == Ok);
    std::atomic<int> other{0};
    std::thread([&] { other = sceUltMutexTryLock(&mutex); }).join();
    CHECK(other == Busy);
    CHECK(sceUltMutexUnlock(&mutex) == Ok);
    std::thread([&] { other = sceUltMutexTryLock(&mutex); }).join();
    CHECK(other == Busy);
    CHECK(sceUltMutexUnlock(&mutex) == Ok);
    CHECK(sceUltMutexUnlock(&mutex) == Permission);
    std::thread([&] {
        other = sceUltMutexTryLock(&mutex);
        if (other == Ok) other = sceUltMutexUnlock(&mutex);
    }).join();
    CHECK(other == Ok);
    CHECK(sceUltMutexLock(&mutex) == Ok);
    CHECK(sceUltMutexDestroy(&mutex) == State);
    CHECK(sceUltMutexUnlock(&mutex) == Ok);
    CHECK(sceUltMutexDestroy(&mutex) == Ok);
    CHECK(sceUltMutexTryLock(&mutex) == State);
}

void OptParams() {
    using Initializer = int (APS5_VABI *)(void*, std::uint32_t);
    for (const Initializer initialize : {sceUltSemaphoreOptParamInitialize, sceUltQueueOptParamInitialize, sceUltQueueDataResourcePoolOptParamInitialize,
                                         sceUltWaitingQueueResourcePoolOptParamInitialize, sceUltUlthreadOptParamInitialize, sceUltReaderWriterLockOptParamInitialize,
                                         sceUltConditionVariableOptParamInitialize, _sceUltConditionVariableOptParamInitialize}) {
        std::uint8_t bytes[136];
        std::memset(bytes, 0xa5, sizeof(bytes));
        CHECK(initialize(nullptr, 0) == Null);
        CHECK(initialize(bytes, 0) == Ok);
        for (std::size_t index = 0; index < 128; ++index) CHECK(bytes[index] == 0);
        for (std::size_t index = 128; index < sizeof(bytes); ++index) CHECK(bytes[index] == 0xa5);
    }
}


void WaitUntil(const std::atomic<bool>& flag) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!flag.load() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK(flag.load());
}

void Settle() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void ReaderWriterValidation() {
    Object rwlock{};
    Object pool{};
    std::memset(&rwlock, 0x5a, sizeof(rwlock));
    CHECK(sceUltReaderWriterLockCreate(nullptr, "rwlock", nullptr, nullptr, 0) == Null);
    CHECK(sceUltReaderWriterLockCreate(&rwlock, "rwlock", &pool, nullptr, 0) == Invalid);
    for (const auto call : {sceUltReaderWriterLockDestroy, sceUltReaderWriterLockLockRead, sceUltReaderWriterLockTryLockRead, sceUltReaderWriterLockUnlockRead,
                            sceUltReaderWriterLockLockWrite, sceUltReaderWriterLockTryLockWrite, sceUltReaderWriterLockUnlockWrite}) {
        CHECK(call(nullptr) == Null);
        CHECK(call(&rwlock) == State);
    }
    CHECK(sceUltReaderWriterLockCreate(&rwlock, "rwlock", nullptr, nullptr, 0) == Ok);
    for (const auto byte : rwlock.bytes) CHECK(byte == 0);
    CHECK(sceUltReaderWriterLockCreate(&rwlock, "rwlock", nullptr, nullptr, 0) == State);
    CHECK(sceUltReaderWriterLockUnlockRead(&rwlock) == State);
    CHECK(sceUltReaderWriterLockUnlockWrite(&rwlock) == State);
    CHECK(sceUltReaderWriterLockLockRead(&rwlock) == Ok);
    CHECK(sceUltReaderWriterLockDestroy(&rwlock) == Busy);
    CHECK(sceUltReaderWriterLockUnlockWrite(&rwlock) == State);
    CHECK(sceUltReaderWriterLockUnlockRead(&rwlock) == Ok);
    CHECK(sceUltReaderWriterLockLockWrite(&rwlock) == Ok);
    CHECK(sceUltReaderWriterLockLockWrite(&rwlock) == State && sceUltReaderWriterLockTryLockWrite(&rwlock) == State);
    CHECK(sceUltReaderWriterLockLockRead(&rwlock) == State && sceUltReaderWriterLockTryLockRead(&rwlock) == State);
    CHECK(sceUltReaderWriterLockDestroy(&rwlock) == Busy);
    std::atomic<int> other{0};
    std::thread([&] { other = sceUltReaderWriterLockUnlockWrite(&rwlock); }).join();
    CHECK(other == State);
    CHECK(sceUltReaderWriterLockUnlockWrite(&rwlock) == Ok);
    CHECK(sceUltReaderWriterLockDestroy(&rwlock) == Ok);
    CHECK(sceUltReaderWriterLockLockRead(&rwlock) == State && sceUltReaderWriterLockDestroy(&rwlock) == State);
}

void ReaderWriterReaders() {
    Object rwlock{};
    CHECK(sceUltReaderWriterLockCreate(&rwlock, "readers", nullptr, nullptr, 0) == Ok);
    CHECK(sceUltReaderWriterLockLockRead(&rwlock) == Ok);
    std::atomic<int> other{0};
    std::thread([&] {
        other = sceUltReaderWriterLockTryLockRead(&rwlock);
        if (other == Ok) other = sceUltReaderWriterLockLockRead(&rwlock);
    }).join();
    CHECK(other == Ok);
    CHECK(sceUltReaderWriterLockTryLockWrite(&rwlock) == Busy);
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
        CHECK(!written.load());
        CHECK(sceUltReaderWriterLockUnlockRead(&rwlock) == Ok);
    }
    writer.join();
    CHECK(written.load() && other == Ok);
    CHECK(sceUltReaderWriterLockUnlockRead(&rwlock) == State);
    CHECK(sceUltReaderWriterLockDestroy(&rwlock) == Ok);
}

void ReaderWriterWriter() {
    Object rwlock{};
    CHECK(sceUltReaderWriterLockCreate(&rwlock, "writer", nullptr, nullptr, 0) == Ok);
    CHECK(sceUltReaderWriterLockTryLockWrite(&rwlock) == Ok);
    std::atomic<int> other{0};
    std::thread([&] { other = sceUltReaderWriterLockTryLockRead(&rwlock); }).join();
    CHECK(other == Busy);
    std::thread([&] { other = sceUltReaderWriterLockTryLockWrite(&rwlock); }).join();
    CHECK(other == Busy);
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
    CHECK(!read.load());
    CHECK(sceUltReaderWriterLockDestroy(&rwlock) == Busy);
    CHECK(sceUltReaderWriterLockUnlockWrite(&rwlock) == Ok);
    reader.join();
    CHECK(read.load() && other == Ok);
    CHECK(sceUltReaderWriterLockTryLockWrite(&rwlock) == Ok && sceUltReaderWriterLockUnlockWrite(&rwlock) == Ok);
    CHECK(sceUltReaderWriterLockDestroy(&rwlock) == Ok);
}

void ReaderWriterPool() {
    Object pool{};
    Object rwlock{};
    std::uint8_t workArea[2048];
    CHECK(_sceUltWaitingQueueResourcePoolCreate(&pool, "pool", 2, 2, workArea, nullptr) == Ok);
    CHECK(sceUltReaderWriterLockCreate(&rwlock, "pooled", &pool, nullptr, 0) == Ok);
    CHECK(sceUltReaderWriterLockLockWrite(&rwlock) == Ok);
    std::atomic<bool> started{false};
    std::atomic<int> other{0};
    std::thread reader([&] {
        started = true;
        other = sceUltReaderWriterLockLockRead(&rwlock);
        if (other == Ok) other = sceUltReaderWriterLockUnlockRead(&rwlock);
    });
    WaitUntil(started);
    Settle();
    CHECK(sceUltWaitingQueueResourcePoolDestroy(&pool) == Busy);
    CHECK(sceUltReaderWriterLockUnlockWrite(&rwlock) == Ok);
    reader.join();
    CHECK(other == Ok);
    CHECK(sceUltReaderWriterLockDestroy(&rwlock) == Ok);
    CHECK(sceUltWaitingQueueResourcePoolDestroy(&pool) == Ok);
}

void ReaderWriterFinalize() {
    Object rwlock{};
    CHECK(sceUltReaderWriterLockCreate(&rwlock, "finalize", nullptr, nullptr, 0) == Ok);
    CHECK(sceUltReaderWriterLockLockWrite(&rwlock) == Ok);
    std::atomic<bool> started{false};
    std::atomic<int> other{0};
    std::thread waiter([&] {
        started = true;
        other = sceUltReaderWriterLockLockWrite(&rwlock);
    });
    WaitUntil(started);
    Settle();
    CHECK(sceUltFinalize() == Ok);
    waiter.join();
    CHECK(other == State);
    CHECK(sceUltReaderWriterLockUnlockWrite(&rwlock) == State);
    CHECK(sceUltInitialize() == Ok);
}

struct alignas(8) UlthreadObject {
    std::uint8_t bytes[512];
};

struct alignas(8) UlthreadRuntimeObject {
    std::uint8_t bytes[4096];
};

std::atomic<void*> gSelf{nullptr};
std::atomic<int> gYield{-1};
std::atomic<bool> gPastExit{false};

std::int32_t APS5_VABI ExitingEntry(std::uint64_t status) {
    void* self = nullptr;
    if (sceUltUlthreadGetSelf(&self) == Ok) gSelf = self;
    gYield = sceUltUlthreadYield();
    sceUltUlthreadExit(static_cast<std::int32_t>(status));
    gPastExit = true;
    return -1;
}

void Ulthreads() {
    void* self = &gSelf;
    CHECK(sceUltUlthreadGetSelf(nullptr) == Null);
    CHECK(sceUltUlthreadGetSelf(&self) == State && self == &gSelf);
    CHECK(sceUltUlthreadYield() == State);
    CHECK(sceUltUlthreadExit(1) == State);
    std::vector<std::uint8_t> workArea(sceUltUlthreadRuntimeGetWorkAreaSize(2, 1));
    UlthreadRuntimeObject runtime{};
    CHECK(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 2, 1, workArea.data(), nullptr, 0) == Ok);
    UlthreadObject first{};
    std::uint8_t context[4096];
    CHECK(sceUltUlthreadCreate(&first, "exits", ExitingEntry, 42, context, sizeof(context), &runtime, nullptr, 0) == Ok);
    std::int32_t status = 0;
    CHECK(sceUltUlthreadJoin(&first, &status) == Ok);
    CHECK(status == 42 && gSelf.load() == &first && gYield.load() == Ok && !gPastExit.load());
    UlthreadObject second{};
    gSelf = nullptr;
    CHECK(sceUltUlthreadCreate(&second, "tryjoin", ExitingEntry, 7, context, sizeof(context), &runtime, nullptr, 0) == Ok);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    int joined = Again;
    while (joined == Again && std::chrono::steady_clock::now() < deadline) {
        joined = sceUltUlthreadTryJoin(&second, &status);
        if (joined == Again) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(joined == Ok && status == 7 && gSelf.load() == &second);
    CHECK(sceUltUlthreadRuntimeDestroy(&runtime) == Ok);
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

    CHECK(sceUltGetMutexInfo(nullptr, nullptr, nullptr, 0) == Null);
    CHECK(sceUltGetMutexInfo(mutexObj, nullptr, nullptr, 0) == Null);
    CHECK(sceUltGetMutexInfo(nullptr, &mutexInfo, nullptr, 0) == Null);
    CHECK(sceUltGetMutexInfo(mutexObj, &mutexInfo, nullptr, 1) == Null);
    CHECK(sceUltGetConditionVariableInfo(nullptr, nullptr, nullptr, 0) == Null);
    CHECK(sceUltGetSemaphoreInfo(nullptr, nullptr, nullptr, 0) == Null);
    CHECK(sceUltGetReaderWriterLockInfo(nullptr, nullptr, nullptr, 0) == Null);
    CHECK(sceUltGetQueueInfo(nullptr, nullptr, nullptr, 0) == Null);
    CHECK(sceUltGetWaitingQueueResourcePoolInfo(nullptr, nullptr) == Null);
    CHECK(sceUltGetQueueDataResourcePoolInfo(nullptr, nullptr) == Null);
    CHECK(sceUltGetUlthreadInfo(nullptr, nullptr) == Null);
    CHECK(sceUltGetUlthreadRuntimeInfo(nullptr, nullptr, nullptr, 0, nullptr, 0) == Null);
    CHECK(sceUltGetUlthreadRuntimeInfo(nullptr, &rtInfo, nullptr, 1, nullptr, 0) == Null);
    CHECK(sceUltGetUlthreadRuntimeInfo(nullptr, &rtInfo, nullptr, 0, nullptr, 1) == Null);

    CHECK(sceUltGetMutexInfo(dummy, &mutexInfo, nullptr, 0) == State);
    CHECK(sceUltGetConditionVariableInfo(dummy, &condInfo, nullptr, 0) == State);
    CHECK(sceUltGetSemaphoreInfo(dummy, &semInfo, nullptr, 0) == State);
    CHECK(sceUltGetReaderWriterLockInfo(dummy, &rwInfo, nullptr, 0) == State);
    CHECK(sceUltGetWaitingQueueResourcePoolInfo(dummy, &poolInfo) == State);
    CHECK(sceUltGetQueueDataResourcePoolInfo(dummy, &dataPoolInfo) == State);
    CHECK(sceUltGetQueueInfo(dummy, &queueInfo, nullptr, 0) == State);
    CHECK(sceUltGetUlthreadInfo(dummy, &threadInfo) == State);
    CHECK(sceUltGetUlthreadRuntimeInfo(dummy, &rtInfo, nullptr, 0, nullptr, 0) == State);

    CHECK(sceUltWaitingQueueResourcePoolCreate(poolObj, "test-pool", 4, 4, nullptr, nullptr, 0) == Ok);
    CHECK(sceUltGetWaitingQueueResourcePoolInfo(poolObj, &poolInfo) == Ok);
    CHECK(poolInfo.size == sizeof(UltWaitingQueueResourcePoolInfo));
    CHECK(poolInfo.object == poolObj);
    CHECK(std::strcmp(poolInfo.name, "test-pool") == 0);
    CHECK(poolInfo.maxThreads == 4);
    CHECK(poolInfo.maxSyncObjects == 4);

    CHECK(sceUltMutexCreate(mutexObj, "test-mutex", poolObj, nullptr, 0) == Ok);
    CHECK(sceUltGetMutexInfo(mutexObj, &mutexInfo, waitThreads, 4) == Ok);
    CHECK(mutexInfo.size == sizeof(UltMutexInfo));
    CHECK(mutexInfo.object == mutexObj);
    CHECK(std::strcmp(mutexInfo.name, "test-mutex") == 0);
    CHECK(mutexInfo.waitingQueuePool == poolObj);
    CHECK(mutexInfo.lockCount == 0);
    CHECK(mutexInfo.waitingThreads == 0);

    CHECK(sceUltMutexLock(mutexObj) == Ok);
    CHECK(sceUltGetMutexInfo(mutexObj, &mutexInfo, nullptr, 0) == Ok);
    CHECK(mutexInfo.lockCount == 1);
    CHECK(sceUltMutexUnlock(mutexObj) == Ok);

    CHECK(sceUltConditionVariableCreate(condObj, "test-cnd", mutexObj, nullptr, 0) == Ok);
    CHECK(sceUltGetConditionVariableInfo(condObj, &condInfo, waitThreads, 4) == Ok);
    CHECK(condInfo.size == sizeof(UltConditionVariableInfo));
    CHECK(condInfo.object == condObj);
    CHECK(condInfo.mutex == mutexObj);
    CHECK(std::strcmp(condInfo.name, "test-cnd") == 0);
    CHECK(condInfo.waitingThreads == 0);

    CHECK(sceUltSemaphoreCreate(semObj, "test-sem", 3, poolObj, nullptr, 0) == Ok);
    CHECK(sceUltGetSemaphoreInfo(semObj, &semInfo, waitThreads, 4) == Ok);
    CHECK(semInfo.size == sizeof(UltSemaphoreInfo));
    CHECK(semInfo.object == semObj);
    CHECK(std::strcmp(semInfo.name, "test-sem") == 0);
    CHECK(semInfo.waitingQueuePool == poolObj);
    CHECK(semInfo.resources == 3);

    CHECK(sceUltReaderWriterLockCreate(rwObj, "test-rw", poolObj, nullptr, 0) == Ok);
    CHECK(sceUltGetReaderWriterLockInfo(rwObj, &rwInfo, waitThreads, 4) == Ok);
    CHECK(rwInfo.size == sizeof(UltReaderWriterLockInfo));
    CHECK(rwInfo.object == rwObj);
    CHECK(std::strcmp(rwInfo.name, "test-rw") == 0);
    CHECK(rwInfo.waitingQueuePool == poolObj);
    CHECK(rwInfo.readers == 0);
    CHECK(rwInfo.lockType == 0);

    CHECK(sceUltReaderWriterLockLockRead(rwObj) == Ok);
    CHECK(sceUltGetReaderWriterLockInfo(rwObj, &rwInfo, nullptr, 0) == Ok);
    CHECK(rwInfo.readers == 1);
    CHECK(rwInfo.lockType == 2);
    CHECK(sceUltReaderWriterLockUnlockRead(rwObj) == Ok);

    CHECK(sceUltQueueDataResourcePoolCreate(dataPoolObj, "test-datapool", 8, 64, 2, poolObj, nullptr, nullptr, 0) == Ok);
    CHECK(sceUltGetQueueDataResourcePoolInfo(dataPoolObj, &dataPoolInfo) == Ok);
    CHECK(dataPoolInfo.size == sizeof(UltQueueDataResourcePoolInfo));
    CHECK(dataPoolInfo.object == dataPoolObj);
    CHECK(std::strcmp(dataPoolInfo.name, "test-datapool") == 0);
    CHECK(dataPoolInfo.waitingQueuePool == poolObj);
    CHECK(dataPoolInfo.dataSize == 64);
    CHECK(dataPoolInfo.maxItems == 8);
    CHECK(dataPoolInfo.availableItems == 8);
    CHECK(dataPoolInfo.maxQueues == 2);

    CHECK(sceUltQueueCreate(queueObj, "test-queue", 64, poolObj, dataPoolObj, nullptr, 0) == Ok);
    CHECK(sceUltGetQueueInfo(queueObj, &queueInfo, waitThreads, 4) == Ok);
    CHECK(queueInfo.size == sizeof(UltQueueInfo));
    CHECK(queueInfo.object == queueObj);
    CHECK(std::strcmp(queueInfo.name, "test-queue") == 0);
    CHECK(queueInfo.waitingQueuePool == poolObj);
    CHECK(queueInfo.dataPool == dataPoolObj);
    CHECK(queueInfo.dataSize == 64);
    CHECK(queueInfo.queuedItems == 0);

    std::uint8_t itemData[64]{0x42};
    CHECK(sceUltQueuePush(queueObj, itemData) == Ok);
    CHECK(sceUltGetQueueInfo(queueObj, &queueInfo, nullptr, 0) == Ok);
    CHECK(queueInfo.queuedItems == 1);
    CHECK(sceUltQueuePop(queueObj, itemData) == Ok);
    CHECK(sceUltGetQueueInfo(queueObj, &queueInfo, nullptr, 0) == Ok);
    CHECK(queueInfo.queuedItems == 0);

    CHECK(sceUltUlthreadRuntimeCreate(rtObj, "test-rt", 2, 1, nullptr, nullptr, 0) == Ok);
    void* activeThreads[4]{};
    CHECK(sceUltGetUlthreadRuntimeInfo(rtObj, &rtInfo, activeThreads, 4, nullptr, 0) == Ok);
    CHECK(rtInfo.size == sizeof(UltUlthreadRuntimeInfo));
    CHECK(rtInfo.object == rtObj);
    CHECK(std::strcmp(rtInfo.name, "test-rt") == 0);
    CHECK(rtInfo.maxUlthreads == 2);
    CHECK(rtInfo.workerThreads == 1);
    CHECK(rtInfo.activeUlthreads == 0);

    Pthread workerHandles[2]{};
    bool workerQueryThrew = false;
    try {
        sceUltGetUlthreadRuntimeInfo(rtObj, &rtInfo, nullptr, 0, workerHandles, 2);
    } catch (const std::runtime_error&) {
        workerQueryThrew = true;
    }
    CHECK(workerQueryThrew);

    UltUlthreadRuntimeInfo untouchedInfo{};
    std::memset(&untouchedInfo, 0xEE, sizeof(untouchedInfo));
    CHECK(sceUltGetUlthreadRuntimeInfo(rtObj, &untouchedInfo, nullptr, 0, nullptr, 1) == Null);
    const auto* infoBytes = reinterpret_cast<const std::uint8_t*>(&untouchedInfo);
    for (std::size_t i = 0; i < sizeof(untouchedInfo); ++i) {
        CHECK(infoBytes[i] == 0xEE);
    }

    CHECK(sceUltConditionVariableDestroy(condObj) == Ok);
    CHECK(sceUltMutexDestroy(mutexObj) == Ok);
    CHECK(sceUltSemaphoreDestroy(semObj) == Ok);
    CHECK(sceUltReaderWriterLockDestroy(rwObj) == Ok);
    CHECK(sceUltQueueDestroy(queueObj) == Ok);
    CHECK(sceUltQueueDataResourcePoolDestroy(dataPoolObj) == Ok);
    CHECK(sceUltWaitingQueueResourcePoolDestroy(poolObj) == Ok);
    CHECK(sceUltUlthreadRuntimeDestroy(rtObj) == Ok);
}

void WaitForQueueWaiters(std::uint8_t* queue, UltQueueInfo* info, std::uint32_t pushers, std::uint32_t poppers) {
    for (int attempt = 0; attempt < 4000; ++attempt) {
        CHECK(sceUltGetQueueInfo(queue, info, nullptr, 0) == Ok);
        if (info->waitingPushers == pushers && info->waitingPoppers == poppers) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(false);
}

void QueueWaiters() {
    alignas(8) std::uint8_t poolObj[256]{};
    alignas(8) std::uint8_t dataPoolObj[512]{};
    alignas(8) std::uint8_t queueObj[512]{};
    UltQueueInfo info{};
    UltWaitingThreadInfo waiters[4]{};

    CHECK(sceUltWaitingQueueResourcePoolCreate(poolObj, "waiter-pool", 4, 4, nullptr, nullptr, 0) == Ok);
    CHECK(sceUltQueueDataResourcePoolCreate(dataPoolObj, "waiter-data", 2, 64, 1, poolObj, nullptr, nullptr, 0) == Ok);
    CHECK(sceUltQueueCreate(queueObj, "waiter-queue", 64, poolObj, dataPoolObj, nullptr, 0) == Ok);

    std::uint8_t item[64]{0x11};
    std::uint8_t popped[64]{};

    std::atomic<int> popResult{1};
    std::thread popper([&] { popResult.store(sceUltQueuePop(queueObj, popped)); });
    WaitForQueueWaiters(queueObj, &info, 0, 1);
    CHECK(info.queuedItems == 0);
    std::memset(waiters, 0xAA, sizeof(waiters));
    CHECK(sceUltGetQueueInfo(queueObj, &info, waiters, 4) == Ok);
    CHECK(info.waitingPushers == 0);
    CHECK(info.waitingPoppers == 1);
    CHECK(waiters[0].size == sizeof(UltWaitingThreadInfo));
    for (std::size_t i = 1; i < sizeof(waiters) / sizeof(waiters[0]); ++i) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&waiters[i]);
        for (std::size_t j = 0; j < sizeof(UltWaitingThreadInfo); ++j) CHECK(bytes[j] == 0xAA);
    }
    CHECK(sceUltQueueDestroy(queueObj) == State);
    CHECK(sceUltQueuePush(queueObj, item) == Ok);
    popper.join();
    CHECK(popResult.load() == Ok);
    CHECK(popped[0] == 0x11);
    WaitForQueueWaiters(queueObj, &info, 0, 0);

    CHECK(sceUltQueuePush(queueObj, item) == Ok);
    CHECK(sceUltQueuePush(queueObj, item) == Ok);

    std::atomic<int> pushResult{1};
    std::thread pusher([&] { pushResult.store(sceUltQueuePush(queueObj, item)); });
    WaitForQueueWaiters(queueObj, &info, 1, 0);
    CHECK(info.queuedItems == 2);
    CHECK(sceUltGetQueueInfo(queueObj, &info, waiters, 0) == Ok);
    CHECK(info.waitingPushers == 1);
    CHECK(info.waitingPoppers == 0);
    CHECK(sceUltQueuePop(queueObj, popped) == Ok);
    pusher.join();
    CHECK(pushResult.load() == Ok);
    WaitForQueueWaiters(queueObj, &info, 0, 0);
    CHECK(info.queuedItems == 2);

    CHECK(sceUltQueuePop(queueObj, popped) == Ok);
    CHECK(sceUltQueuePop(queueObj, popped) == Ok);
    CHECK(sceUltQueueDestroy(queueObj) == Ok);
    CHECK(sceUltQueueDataResourcePoolDestroy(dataPoolObj) == Ok);
    CHECK(sceUltWaitingQueueResourcePoolDestroy(poolObj) == Ok);
}

}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    CHECK(sceUltInitialize() == Ok);
    if (std::strcmp(argv[1], "mutex-trylock") == 0) MutexTryLock();
    else if (std::strcmp(argv[1], "opt-params") == 0) OptParams();
    else if (std::strcmp(argv[1], "rwlock-validation") == 0) ReaderWriterValidation();
    else if (std::strcmp(argv[1], "rwlock-readers") == 0) ReaderWriterReaders();
    else if (std::strcmp(argv[1], "rwlock-writer") == 0) ReaderWriterWriter();
    else if (std::strcmp(argv[1], "rwlock-pool") == 0) ReaderWriterPool();
    else if (std::strcmp(argv[1], "rwlock-finalize") == 0) ReaderWriterFinalize();
    else if (std::strcmp(argv[1], "ulthreads") == 0) Ulthreads();
    else if (std::strcmp(argv[1], "get-info") == 0) GetInfoTests();
    else if (std::strcmp(argv[1], "queue-waiters") == 0) QueueWaiters();
    else return 2;
    CHECK(sceUltFinalize() == Ok);
    std::puts("ULT sync tests passed");
    return 0;
}
