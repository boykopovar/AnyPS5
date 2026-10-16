#include "SceTypes.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <thread>
#include <stdexcept>

extern "C" {
int APS5_VABI sceUltInitialize();
int APS5_VABI sceUltFinalize();
std::uint64_t APS5_VABI sceUltUlthreadRuntimeGetWorkAreaSize(std::uint32_t, std::uint32_t);
int APS5_VABI sceUltUlthreadRuntimeCreate(void*, const char*, std::uint32_t, std::uint32_t, void*, const void*, std::uint32_t);
int APS5_VABI sceUltUlthreadCreate(void*, const char*, UltUlthreadEntry, std::uint64_t, void*, std::uint64_t, void*, const void*, std::uint32_t);
int APS5_VABI sceUltUlthreadJoin(void*, std::int32_t*);
int APS5_VABI sceUltUlthreadTryJoin(void*, std::int32_t*);
int APS5_VABI sceUltUlthreadRuntimeDestroy(void*);
int APS5_VABI _sceUltUlthreadRuntimeOptParamInitialize(void*, std::uint32_t);
int APS5_VABI _sceUltUlthreadRuntimeCreate(void*, const char*, std::uint32_t, std::uint32_t, void*, const void*, std::uint32_t);
int APS5_VABI _sceUltUlthreadCreate(void*, const char*, UltUlthreadEntry, std::uint64_t, void*, std::uint64_t, void*, const void*, std::uint32_t);
int APS5_VABI _sceUltWaitingQueueResourcePoolCreate(void*, const char*, std::uint32_t, std::uint32_t, void*, const void*, std::uint32_t);
int APS5_VABI _sceUltSemaphoreCreate(void*, const char*, std::int32_t, void*, const void*, std::uint32_t);
int APS5_VABI sceUltSemaphoreAcquire(void*, std::int32_t);
int APS5_VABI sceUltSemaphoreRelease(void*, std::int32_t);
int APS5_VABI _sceUltMutexOptParamInitialize(void*, std::uint32_t);
int APS5_VABI _sceUltMutexCreate(void*, const char*, void*, const void*, std::uint32_t);
int APS5_VABI sceUltMutexLock(void*);
int APS5_VABI sceUltMutexUnlock(void*);
int APS5_VABI _sceUltQueueDataResourcePoolCreate(void*, const char*, std::uint32_t, std::uint64_t, std::uint32_t, void*, void*, const void*, std::uint32_t);
int APS5_VABI _sceUltQueueCreate(void*, const char*, std::uint64_t, void*, void*, const void*, std::uint32_t);
int APS5_VABI sceUltQueuePush(void*, const void*);
int APS5_VABI sceUltQueuePop(void*, void*);
int APS5_VABI sceUltMutexDestroy(void*);
int APS5_VABI sceUltWaitingQueueResourcePoolDestroy(void*);
int APS5_VABI sceUltSemaphoreDestroy(void*);
int APS5_VABI sceUltQueueDestroy(void*);
int APS5_VABI sceUltQueueDataResourcePoolDestroy(void*);
int APS5_VABI sceUltConditionVariableDestroy();
int APS5_VABI sceUltReaderWriterLockDestroy();
int APS5_VABI sceUltGetConditionVariableInfo();
}

static constexpr int Ok = 0;
static constexpr int Null = -2139029503;
static constexpr int State = -2139029498;
static constexpr int Busy = -2139029497;

static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ULT thread check failed at line %d\n", line);
        std::abort();
    }
}

#define CHECK(value) Require((value), __LINE__)

static std::atomic<bool> gStarted{false};
static std::atomic<bool> gRelease{false};

static std::int32_t APS5_VABI Parked(std::uint64_t arg) {
    gStarted.store(true, std::memory_order_release);
    while (!gRelease.load(std::memory_order_acquire)) std::this_thread::yield();
    return static_cast<std::int32_t>(arg);
}

static std::int32_t APS5_VABI Immediate(std::uint64_t arg) {
    return static_cast<std::int32_t>(arg);
}

struct alignas(8) Runtime { std::uint8_t bytes[4096]{}; };
struct alignas(8) Thread { std::uint8_t bytes[512]{}; };

static void Outstanding() {
    Runtime runtime;
    Thread thread;
    CHECK(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == Ok);
    CHECK(sceUltUlthreadCreate(&thread, "parked", Parked, 0x5A, nullptr, 0, &runtime, nullptr, 0) == Ok);
    while (!gStarted.load(std::memory_order_acquire)) std::this_thread::yield();

    const int outstanding = sceUltFinalize();
    gRelease.store(true, std::memory_order_release);
    std::int32_t status = 0;
    const int joined = sceUltUlthreadJoin(&thread, &status);

    CHECK(outstanding == Busy);
    CHECK(joined == Ok && status == 0x5A);
    CHECK(sceUltUlthreadJoin(&thread, &status) == State);
    CHECK(sceUltFinalize() == Ok);
}

static void Validation() {
    Runtime runtime;
    Thread thread;
    gStarted.store(false, std::memory_order_release);
    gRelease.store(false, std::memory_order_release);
    CHECK(sceUltUlthreadRuntimeCreate(nullptr, "runtime", 1, 1, nullptr, nullptr, 0) == Null);
    CHECK(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == Ok);
    CHECK(sceUltUlthreadCreate(nullptr, "thread", Immediate, 0, nullptr, 0, &runtime, nullptr, 0) == Null);
    CHECK(sceUltUlthreadCreate(&thread, "thread", nullptr, 0, nullptr, 0, &runtime, nullptr, 0) == Null);
    CHECK(sceUltUlthreadCreate(&thread, "thread", Immediate, 0, nullptr, 0, nullptr, nullptr, 0) == Null);
    CHECK(sceUltUlthreadJoin(nullptr, nullptr) == Null);
    CHECK(sceUltUlthreadJoin(&thread, nullptr) == State);

    CHECK(sceUltUlthreadCreate(&thread, "thread", Immediate, 0x27, nullptr, 0, &runtime, nullptr, 0) == Ok);
    CHECK(sceUltUlthreadCreate(&thread, "thread", Immediate, 0x27, nullptr, 0, &runtime, nullptr, 0) == State);
    CHECK(sceUltFinalize() == Busy);
    std::int32_t status = 0;
    CHECK(sceUltUlthreadJoin(&thread, &status) == Ok && status == 0x27);
    CHECK(sceUltFinalize() == Ok);
    CHECK(sceUltUlthreadRuntimeGetWorkAreaSize(1, 1) == 256u + 16u * 1024u);
    bool threwCv = false;
    try {
        sceUltConditionVariableDestroy();
    } catch (const std::runtime_error&) {
        threwCv = true;
    }
    CHECK(threwCv);
    bool threwRwl = false;
    try {
        sceUltReaderWriterLockDestroy();
    } catch (const std::runtime_error&) {
        threwRwl = true;
    }
    CHECK(threwRwl);
    bool threwInfo = false;
    try {
        sceUltGetConditionVariableInfo();
    } catch (const std::runtime_error&) {
        threwInfo = true;
    }
    CHECK(threwInfo);
}

static void TryJoin() {
    Runtime runtime;
    Thread thread;
    std::uint8_t optParam[128];
    std::memset(optParam, 0xff, sizeof(optParam));
    CHECK(_sceUltUlthreadRuntimeOptParamInitialize(nullptr, 0) == Null);
    CHECK(_sceUltUlthreadRuntimeOptParamInitialize(optParam, 0) == Ok && optParam[0] == 0);
    CHECK(_sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == Ok);
    CHECK(_sceUltUlthreadCreate(&thread, "parked", Parked, 0x3C, nullptr, 0, &runtime, nullptr, 0) == Ok);
    while (!gStarted.load(std::memory_order_acquire)) std::this_thread::yield();

    std::int32_t status = 0;
    const int running = sceUltUlthreadTryJoin(&thread, &status);
    gRelease.store(true, std::memory_order_release);
    int joined;
    while ((joined = sceUltUlthreadTryJoin(&thread, &status)) == Busy) std::this_thread::yield();

    CHECK(running == Busy);
    CHECK(joined == Ok && status == 0x3C);
    CHECK(sceUltUlthreadTryJoin(&thread, &status) == State);
    CHECK(sceUltUlthreadTryJoin(nullptr, &status) == Null);
    CHECK(sceUltUlthreadRuntimeDestroy(&runtime) == Ok);
}

static void Destroy() {
    Runtime runtime;
    Runtime other;
    Thread thread;
    CHECK(sceUltUlthreadRuntimeDestroy(nullptr) == Null);
    CHECK(sceUltUlthreadRuntimeDestroy(&runtime) == State);
    CHECK(_sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == Ok);
    CHECK(_sceUltUlthreadRuntimeCreate(&other, "other", 1, 1, nullptr, nullptr, 0) == Ok);
    CHECK(_sceUltUlthreadCreate(&thread, "thread", Immediate, 0x11, nullptr, 0, &runtime, nullptr, 0) == Ok);
    CHECK(sceUltUlthreadRuntimeDestroy(&other) == Ok);
    CHECK(sceUltUlthreadRuntimeDestroy(&runtime) == Busy);
    std::int32_t status = 0;
    CHECK(sceUltUlthreadJoin(&thread, &status) == Ok && status == 0x11);
    CHECK(sceUltUlthreadRuntimeDestroy(&runtime) == Ok);
    CHECK(sceUltUlthreadRuntimeDestroy(&runtime) == State);
    CHECK(_sceUltUlthreadCreate(&thread, "thread", Immediate, 0, nullptr, 0, &runtime, nullptr, 0) == State);
    CHECK(sceUltFinalize() == Ok);
}

static void UnderscoredCreate() {
    alignas(8) static std::uint8_t pool[256];
    alignas(8) static std::uint8_t semaphore[256];
    alignas(8) static std::uint8_t unknownPool[256];
    CHECK(_sceUltWaitingQueueResourcePoolCreate(nullptr, "pool", 1, 1, nullptr, nullptr, 0) == Null);
    CHECK(_sceUltWaitingQueueResourcePoolCreate(pool, "pool", 1, 1, nullptr, nullptr, 0) == Ok);
    CHECK(_sceUltSemaphoreCreate(nullptr, "semaphore", 1, pool, nullptr, 0) == Null);
    CHECK(_sceUltSemaphoreCreate(semaphore, "semaphore", -1, pool, nullptr, 0) != Ok);
    CHECK(_sceUltSemaphoreCreate(semaphore, "semaphore", 2, unknownPool, nullptr, 0) != Ok);
    CHECK(_sceUltSemaphoreCreate(semaphore, "semaphore", 2, pool, nullptr, 0) == Ok);
    CHECK(_sceUltSemaphoreCreate(semaphore, "semaphore", 2, pool, nullptr, 0) == State);
    CHECK(sceUltSemaphoreAcquire(semaphore, 2) == Ok);
    CHECK(sceUltSemaphoreRelease(semaphore, 2) == Ok);
    alignas(8) static std::uint8_t mutexOptParam[128];
    alignas(8) static std::uint8_t mutex[256];
    std::memset(mutexOptParam, 0xFF, sizeof(mutexOptParam));
    CHECK(_sceUltMutexOptParamInitialize(nullptr, 0) == Null);
    CHECK(_sceUltMutexOptParamInitialize(mutexOptParam, 0) == Ok);
    CHECK(mutexOptParam[8] == 0 && mutexOptParam[15] == 0);
    CHECK(_sceUltMutexCreate(nullptr, "mutex", pool, mutexOptParam, 0) == Null);
    CHECK(_sceUltMutexCreate(mutex, "mutex", pool, mutexOptParam, 0) == Ok);
    CHECK(sceUltMutexLock(mutex) == Ok);
    CHECK(sceUltMutexUnlock(mutex) == Ok);
    alignas(8) static std::uint8_t dataPool[512];
    alignas(8) static std::uint8_t queue[512];
    CHECK(_sceUltQueueDataResourcePoolCreate(nullptr, "data", 1, 8, 1, pool, nullptr, nullptr, 0) == Null);
    CHECK(_sceUltQueueDataResourcePoolCreate(dataPool, "data", 1, 8, 1, pool, nullptr, nullptr, 0) == Ok);
    CHECK(_sceUltQueueCreate(nullptr, "queue", 8, pool, dataPool, nullptr, 0) == Null);
    CHECK(_sceUltQueueCreate(queue, "queue", 8, pool, dataPool, nullptr, 0) == Ok);
    std::uint64_t value = 0x1122334455667788ull, output = 0;
    CHECK(sceUltQueuePush(queue, &value) == Ok);
    CHECK(sceUltQueuePop(queue, &output) == Ok && output == value);
    CHECK(sceUltWaitingQueueResourcePoolDestroy(nullptr) == Null);
    CHECK(sceUltWaitingQueueResourcePoolDestroy(unknownPool) == State);
    CHECK(sceUltMutexDestroy(nullptr) == Null);
    CHECK(sceUltMutexLock(mutex) == Ok);
    CHECK(sceUltMutexDestroy(mutex) == Busy);
    std::atomic<bool> waiterStarted{false};
    std::atomic<bool> waiterAcquired{false};
    std::thread waiter([&] {
        waiterStarted.store(true, std::memory_order_release);
        CHECK(sceUltMutexLock(mutex) == Ok);
        waiterAcquired.store(true, std::memory_order_release);
        CHECK(sceUltMutexUnlock(mutex) == Ok);
    });
    while (!waiterStarted.load(std::memory_order_acquire)) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(!waiterAcquired.load(std::memory_order_acquire));
    CHECK(sceUltMutexDestroy(mutex) == Busy);
    CHECK(sceUltWaitingQueueResourcePoolDestroy(pool) == Busy);
    CHECK(sceUltMutexUnlock(mutex) == Ok);
    waiter.join();
    CHECK(waiterAcquired.load(std::memory_order_acquire));
    CHECK(sceUltWaitingQueueResourcePoolDestroy(pool) == Ok);
    CHECK(sceUltWaitingQueueResourcePoolDestroy(pool) == State);
    CHECK(sceUltMutexDestroy(mutex) == Ok);
    CHECK(sceUltMutexDestroy(mutex) == State);
    CHECK(sceUltSemaphoreDestroy(semaphore) == Ok);
    CHECK(sceUltQueueDestroy(queue) == Ok);
    CHECK(sceUltQueueDataResourcePoolDestroy(dataPool) == Ok);
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    CHECK(sceUltInitialize() == Ok);
    if (std::strcmp(argv[1], "outstanding") == 0) Outstanding();
    else if (std::strcmp(argv[1], "validation") == 0) Validation();
    else if (std::strcmp(argv[1], "tryjoin") == 0) TryJoin();
    else if (std::strcmp(argv[1], "destroy") == 0) Destroy();
    else if (std::strcmp(argv[1], "underscoredcreate") == 0) UnderscoredCreate();
    else CHECK(false);
}
