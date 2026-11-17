#include "prx/libSceUlt/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <semaphore>

struct UltConditionVariableInfo;
struct UltWaitingThreadInfo;

static constexpr int Ok = 0;
static constexpr int Null = -2139029503;
static constexpr int Alignment = -2139029502;
static constexpr int Invalid = -2139029500;
static constexpr int Permission = -2139029499;
static constexpr int State = -2139029498;
static constexpr int Busy = -2139029497;
static constexpr int Again = -2139029496;

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

static std::binary_semaphore gTargetStarted{0};
static std::binary_semaphore gTargetRelease{0};
static std::binary_semaphore gJoinStart{0};

static std::int32_t APS5_VABI GatedTarget(std::uint64_t argument) {
    gTargetStarted.release();
    Require(gTargetRelease.try_acquire_for(std::chrono::seconds(4)));
    return static_cast<std::int32_t>(argument);
}

static void* APS5_VABI JoinTarget(void* argument) {
    Require(gJoinStart.try_acquire_for(std::chrono::seconds(4)));
    std::int32_t status = -1;
    Require(sceUltUlthreadJoin(argument, &status) == Ok && status == 91);
    return nullptr;
}

static void AwaitJoinWait(Pthread caller) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    for (;;) {
        {
            std::lock_guard lock(caller->cancelLock);
            if (caller->cancelWait != nullptr) return;
        }
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}

static void CanceledJoin(bool pending) {
    Runtime runtime;
    Thread thread;
    Require(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == Ok);
    Require(sceUltUlthreadCreate(&thread, "target", GatedTarget, 91, nullptr, 0, &runtime, nullptr, 0) == Ok);
    Require(gTargetStarted.try_acquire_for(std::chrono::seconds(2)));
    Pthread caller = nullptr;
    Require(scePthreadCreate(&caller, nullptr, JoinTarget, &thread, "joiner") == 0);
    if (!pending) {
        gJoinStart.release();
        AwaitJoinWait(caller);
    }
    Require(scePthreadCancel(caller) == 0);
    if (pending) gJoinStart.release();
    void* canceled = nullptr;
    Require(scePthreadJoin(caller, &canceled) == 0);
    Require(canceled == reinterpret_cast<void*>(std::uintptr_t{1}));
    Pthread replacement = nullptr;
    Require(scePthreadCreate(&replacement, nullptr, JoinTarget, &thread, "replacement") == 0);
    gJoinStart.release();
    AwaitJoinWait(replacement);
    Require(sceUltUlthreadRuntimeDestroy(&runtime) == State);
    Require(sceUltFinalize() == Busy);
    gTargetRelease.release();
    Require(scePthreadJoin(replacement, nullptr) == 0);
    Require(sceUltUlthreadJoin(&thread, nullptr) == State);
    Require(sceUltUlthreadRuntimeDestroy(&runtime) == Ok);
    Require(sceUltFinalize() == Ok);
}

static std::int32_t APS5_VABI SelfJoin(std::uint64_t) {
    void* self = nullptr;
    Require(sceUltUlthreadGetSelf(&self) == Ok);
    std::int32_t status = -1;
    Require(sceUltUlthreadJoin(self, &status) == State && status == -1);
    Require(sceUltUlthreadTryJoin(self, &status) == Again && status == -1);
    return 91;
}

static void SelfJoin() {
    Runtime runtime;
    Thread thread;
    Require(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == Ok);
    Require(sceUltUlthreadCreate(&thread, "self", SelfJoin, 0, nullptr, 0, &runtime, nullptr, 0) == Ok);
    std::int32_t status = -1;
    Require(sceUltUlthreadJoin(&thread, &status) == Ok && status == 91);
    Require(sceUltUlthreadRuntimeDestroy(&runtime) == Ok);
    Require(sceUltFinalize() == Ok);
}

static void Outstanding() {
    Runtime runtime;
    Thread thread;
    Require(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == Ok);
    Require(sceUltUlthreadCreate(&thread, "parked", Parked, 0x5A, nullptr, 0, &runtime, nullptr, 0) == Ok);
    while (!gStarted.load(std::memory_order_acquire)) std::this_thread::yield();

    const int outstanding = sceUltFinalize();
    gRelease.store(true, std::memory_order_release);
    std::int32_t status = 0;
    const int joined = sceUltUlthreadJoin(&thread, &status);

    Require(outstanding == Busy);
    Require(joined == Ok && status == 0x5A);
    Require(sceUltUlthreadJoin(&thread, &status) == State);
    Require(sceUltFinalize() == Ok);
}

static void Validation() {
    Runtime runtime;
    Thread thread;
    gStarted.store(false, std::memory_order_release);
    gRelease.store(false, std::memory_order_release);
    Require(sceUltUlthreadRuntimeCreate(nullptr, "runtime", 1, 1, nullptr, nullptr, 0) == Null);
    Require(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == Ok);
    Require(sceUltUlthreadCreate(nullptr, "thread", Immediate, 0, nullptr, 0, &runtime, nullptr, 0) == Null);
    Require(sceUltUlthreadCreate(&thread, "thread", nullptr, 0, nullptr, 0, &runtime, nullptr, 0) == Null);
    Require(sceUltUlthreadCreate(&thread, "thread", Immediate, 0, nullptr, 0, nullptr, nullptr, 0) == Null);
    Require(sceUltUlthreadJoin(nullptr, nullptr) == Null);
    Require(sceUltUlthreadJoin(&thread, nullptr) == State);

    Require(sceUltUlthreadCreate(&thread, "thread", Immediate, 0x27, nullptr, 0, &runtime, nullptr, 0) == Ok);
    Require(sceUltUlthreadCreate(&thread, "thread", Immediate, 0x27, nullptr, 0, &runtime, nullptr, 0) == State);
    Require(sceUltFinalize() == Busy);
    std::int32_t status = 0;
    Require(sceUltUlthreadJoin(&thread, &status) == Ok && status == 0x27);
    Require(sceUltFinalize() == Ok);
    Require(sceUltUlthreadRuntimeGetWorkAreaSize(1, 1) == 1184);
    Require(sceUltConditionVariableDestroy(nullptr) == -2139029503);
    Require(sceUltReaderWriterLockDestroy(nullptr) == Null);
    Require(sceUltGetConditionVariableInfo(nullptr, nullptr, nullptr, 0) == Null);
}

static void TryJoin() {
    Require(sceUltUlthreadRuntimeGetWorkAreaSize(0, 0) == 384);
    Require(sceUltUlthreadRuntimeGetWorkAreaSize(1, 1) == 1184);
    Require(sceUltUlthreadRuntimeGetWorkAreaSize(3, 2) == 2128);
    Require(sceUltUlthreadRuntimeGetWorkAreaSize(0x80000000u, 0) == 0xA00000180ull);
    Runtime runtime;
    Thread thread;
    std::uint8_t optParam[128];
    std::memset(optParam, 0xff, sizeof(optParam));
    Require(_sceUltUlthreadRuntimeOptParamInitialize(nullptr, 0) == Null);
    std::uint8_t zeroOptParam[128] = {};
    Require(_sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, zeroOptParam) == Invalid);
    Require(_sceUltUlthreadRuntimeOptParamInitialize(reinterpret_cast<UltUlthreadRuntimeOptParam*>(optParam), 0) == Ok && optParam[0] == 0x75);
    Require(_sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, optParam) == Ok);
    Require(_sceUltUlthreadCreate(&thread, "parked", Parked, 0x3C, nullptr, 0, &runtime, nullptr) == Ok);
    while (!gStarted.load(std::memory_order_acquire)) std::this_thread::yield();

    std::int32_t status = 0;
    const int running = sceUltUlthreadTryJoin(&thread, &status);
    gRelease.store(true, std::memory_order_release);
    int joined;
    while ((joined = sceUltUlthreadTryJoin(&thread, &status)) == Again) std::this_thread::yield();

    Require(running == Again);
    Require(joined == Ok && status == 0x3C);
    Require(sceUltUlthreadTryJoin(&thread, &status) == State);
    Require(sceUltUlthreadTryJoin(nullptr, &status) == Null);
    Require(sceUltUlthreadRuntimeDestroy(&runtime) == Ok);
}

static void Destroy() {
    Runtime runtime;
    Runtime other;
    Thread thread;
    Require(sceUltUlthreadRuntimeDestroy(nullptr) == Null);
    Require(sceUltUlthreadRuntimeDestroy(&runtime) == State);
    Require(_sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr) == Ok);
    runtime.bytes[0] = 0x5A;
    Require(_sceUltUlthreadRuntimeCreate(&runtime, "again", 2, 2, nullptr, nullptr) == State);
    Require(runtime.bytes[0] == 0x5A);
    UltUlthreadRuntimeInfo runtimeInfo{};
    Require(sceUltGetUlthreadRuntimeInfo(&runtime, &runtimeInfo, nullptr, 0, nullptr, 0) == Ok);
    Require(std::strcmp(runtimeInfo.name, "runtime") == 0 && runtimeInfo.maxUlthreads == 1 && runtimeInfo.workerThreads == 1);
    Require(_sceUltUlthreadRuntimeCreate(&other, "other", 1, 1, nullptr, nullptr) == Ok);
    Require(_sceUltUlthreadCreate(&thread, "thread", Immediate, 0x11, nullptr, 0, &runtime, nullptr) == Ok);
    Require(sceUltUlthreadRuntimeDestroy(&other) == Ok);
    Require(sceUltUlthreadRuntimeDestroy(&runtime) == State);
    std::int32_t status = 0;
    Require(sceUltUlthreadJoin(&thread, &status) == Ok && status == 0x11);
    Require(sceUltUlthreadRuntimeDestroy(&runtime) == Ok);
    Require(sceUltUlthreadRuntimeDestroy(&runtime) == State);
    Require(_sceUltUlthreadCreate(&thread, "thread", Immediate, 0, nullptr, 0, &runtime, nullptr) == State);
    Require(sceUltFinalize() == Ok);
}

static void UnderscoredCreate() {
    alignas(8) static std::uint8_t pool[256];
    alignas(8) static std::uint8_t semaphore[256];
    alignas(8) static std::uint8_t unknownPool[256];
    Require(_sceUltWaitingQueueResourcePoolCreate(nullptr, "pool", 1, 1, nullptr, nullptr) == Null);
    alignas(8) std::uint8_t workArea[1024]{};
    alignas(8) std::uint32_t commonOptions[32]{};
    commonOptions[0] = 0x6C696275;
    commonOptions[1] = 0x6C746D61;
    commonOptions[2] = 0x67696300;
    Require(_sceUltWaitingQueueResourcePoolCreate(pool, "pool", 1, 1, nullptr, nullptr) == Null);
    Require(_sceUltWaitingQueueResourcePoolCreate(pool, "pool", 1, 1, workArea + 1, nullptr) == Alignment);
    commonOptions[31] = 1;
    Require(_sceUltWaitingQueueResourcePoolCreate(pool, "pool", 1, 1, workArea, commonOptions) == Invalid);
    commonOptions[31] = 0;
    Require(_sceUltWaitingQueueResourcePoolCreate(pool, "pool", 1, 1, workArea, commonOptions) == Ok);
    pool[0] = 0x5A;
    Require(_sceUltWaitingQueueResourcePoolCreate(pool, "again", 2, 2, workArea, nullptr) == State);
    Require(pool[0] == 0x5A);
    UltWaitingQueueResourcePoolInfo poolInfo{};
    Require(sceUltGetWaitingQueueResourcePoolInfo(pool, &poolInfo) == Ok);
    Require(std::strcmp(poolInfo.name, "pool") == 0 && poolInfo.maxThreads == 1 && poolInfo.maxSyncObjects == 1);
    Require(_sceUltSemaphoreCreate(nullptr, "semaphore", 1, pool, nullptr) == Null);
    Require(_sceUltSemaphoreCreate(semaphore, "semaphore", UINT32_MAX, pool, nullptr) == Ok);
    Require(sceUltSemaphoreTryAcquire(semaphore, 1) == Again);
    Require(sceUltSemaphoreRelease(semaphore, 1) == Ok);
    Require(sceUltSemaphoreTryAcquire(semaphore, 1) == Again);
    Require(sceUltSemaphoreDestroy(semaphore) == Ok);
    Require(sceUltSemaphoreAcquire(nullptr, 0) == Null);
    Require(sceUltSemaphoreTryAcquire(nullptr, 0) == Null);
    Require(sceUltSemaphoreRelease(nullptr, 0) == Null);
    Require(sceUltSemaphoreAcquire(semaphore + 1, 0) == Alignment);
    Require(sceUltSemaphoreTryAcquire(semaphore + 1, 0) == Alignment);
    Require(sceUltSemaphoreRelease(semaphore + 1, 0) == Alignment);
    Require(sceUltSemaphoreDestroy(semaphore + 1) == Alignment);
    Require(_sceUltSemaphoreCreate(semaphore, "semaphore", 2, unknownPool, nullptr) != Ok);
    commonOptions[0] = 0;
    Require(_sceUltSemaphoreCreate(semaphore, "semaphore", 2, pool, commonOptions) == Invalid);
    commonOptions[0] = 0x6C696275;
    Require(_sceUltSemaphoreCreate(semaphore, "semaphore", 2, pool, commonOptions) == Ok);
    Require(_sceUltSemaphoreCreate(semaphore, "semaphore", 2, pool, nullptr) == State);
    Require(sceUltSemaphoreAcquire(semaphore, 2) == Ok);
    Require(sceUltSemaphoreRelease(semaphore, 2) == Ok);
    alignas(8) static std::uint8_t mutexOptParam[136];
    alignas(8) static std::uint8_t mutex[256];
    std::memset(mutexOptParam, 0xFF, sizeof(mutexOptParam));
    Require(_sceUltMutexOptParamInitialize(nullptr, 0) == Null);
    Require(_sceUltMutexOptParamInitialize(reinterpret_cast<UltMutexOptParam*>(mutexOptParam + 1), 0) == Alignment);
    Require(mutexOptParam[0] == 0xFF && mutexOptParam[127] == 0xFF);
    Require(_sceUltMutexOptParamInitialize(reinterpret_cast<UltMutexOptParam*>(mutexOptParam), 0x12345678) == Ok);
    UltMutexOptParam options{};
    std::memcpy(&options, mutexOptParam, sizeof(options));
    Require(options.header[0] == 0x6C696275 && options.header[1] == 0x6C746D61 && options.header[2] == 0x67696300);
    Require(options.buildVersion == 0x12345678 && options.attribute == 0);
    for (std::size_t index = 16; index < 128; ++index) Require(mutexOptParam[index] == 0);
    for (std::size_t index = 128; index < sizeof(mutexOptParam); ++index) Require(mutexOptParam[index] == 0xFF);
    Require(_sceUltMutexCreate(mutex, nullptr, pool, reinterpret_cast<const UltMutexOptParam*>(mutexOptParam)) == Null);
    Require(_sceUltMutexCreate(mutex, "mutex", nullptr, reinterpret_cast<const UltMutexOptParam*>(mutexOptParam)) == Null);
    Require(_sceUltMutexCreate(mutex, "mutex", pool, reinterpret_cast<const UltMutexOptParam*>(mutexOptParam + 1)) == Alignment);
    mutexOptParam[127] = 1;
    Require(_sceUltMutexCreate(mutex, "mutex", pool, reinterpret_cast<const UltMutexOptParam*>(mutexOptParam)) == Invalid);
    mutexOptParam[127] = 0;
    mutexOptParam[16] = 2;
    Require(_sceUltMutexCreate(mutex, "mutex", pool, reinterpret_cast<const UltMutexOptParam*>(mutexOptParam)) == Invalid);
    mutexOptParam[16] = 0;
    mutexOptParam[0] = 0;
    Require(_sceUltMutexCreate(mutex, "mutex", pool, reinterpret_cast<const UltMutexOptParam*>(mutexOptParam)) == Invalid);
    Require(_sceUltMutexOptParamInitialize(reinterpret_cast<UltMutexOptParam*>(mutexOptParam), 0) == Ok);
    Require(_sceUltMutexCreate(nullptr, "mutex", pool, reinterpret_cast<const UltMutexOptParam*>(mutexOptParam)) == Null);
    Require(_sceUltMutexCreate(mutex, "mutex", pool, reinterpret_cast<const UltMutexOptParam*>(mutexOptParam)) == Ok);
    Require(sceUltMutexLock(mutex) == Ok);
    Require(sceUltMutexLock(mutex) == State);
    std::thread foreignUnlock([&] { Require(sceUltMutexUnlock(mutex) == Permission); });
    foreignUnlock.join();
    Require(sceUltMutexUnlock(mutex) == Ok);
    Require(sceUltMutexUnlock(mutex) == Permission);
    Require(sceUltMutexDestroy(mutex) == Ok);
    mutexOptParam[16] = 1;
    Require(_sceUltMutexCreate(mutex, "mutex", pool, reinterpret_cast<const UltMutexOptParam*>(mutexOptParam)) == Ok);
    Require(sceUltMutexLock(mutex) == Ok);
    Require(sceUltMutexLock(mutex) == Ok);
    Require(sceUltMutexUnlock(mutex) == Ok);
    Require(sceUltMutexDestroy(mutex) == State);
    Require(sceUltMutexUnlock(mutex) == Ok);
    alignas(8) static std::uint8_t dataPool[512];
    alignas(8) static std::uint8_t queue[512];
    Require(_sceUltQueueDataResourcePoolCreate(nullptr, "data", 1, 8, 1, pool, nullptr, nullptr) == Null);
    Require(_sceUltQueueDataResourcePoolCreate(dataPool, "data", 1, 8, 1, pool, nullptr, nullptr) == Null);
    commonOptions[4] = 1;
    Require(_sceUltQueueDataResourcePoolCreate(dataPool, "data", 1, 8, 1, pool, workArea, commonOptions) == Invalid);
    commonOptions[4] = 0;
    Require(_sceUltQueueDataResourcePoolCreate(dataPool, "data", 1, 8, 1, pool, workArea, commonOptions) == Ok);
    Require(_sceUltQueueCreate(nullptr, "queue", 8, pool, dataPool, nullptr) == Null);
    commonOptions[31] = 1;
    Require(_sceUltQueueCreate(queue, "queue", 8, pool, dataPool, commonOptions) == Invalid);
    commonOptions[31] = 0;
    Require(_sceUltQueueCreate(queue, "queue", 8, pool, dataPool, reinterpret_cast<std::uint8_t*>(commonOptions) + 1) == Alignment);
    Require(_sceUltQueueCreate(queue, "queue", 8, pool, dataPool, commonOptions) == Ok);
    std::uint64_t value = 0x1122334455667788ull, output = 0;
    Require(sceUltQueuePush(queue, &value) == Ok);
    Require(sceUltQueuePop(queue, &output) == Ok && output == value);
    Require(sceUltWaitingQueueResourcePoolDestroy(nullptr) == Null);
    Require(sceUltWaitingQueueResourcePoolDestroy(unknownPool) == State);
    Require(sceUltMutexDestroy(nullptr) == Null);
    Require(sceUltMutexLock(mutex) == Ok);
    Require(sceUltMutexDestroy(mutex) == State);
    std::atomic<bool> waiterStarted{false};
    std::atomic<bool> waiterAcquired{false};
    std::thread waiter([&] {
        waiterStarted.store(true, std::memory_order_release);
        Require(sceUltMutexLock(mutex) == Ok);
        waiterAcquired.store(true, std::memory_order_release);
        Require(sceUltMutexUnlock(mutex) == Ok);
    });
    while (!waiterStarted.load(std::memory_order_acquire)) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(!waiterAcquired.load(std::memory_order_acquire));
    Require(sceUltMutexDestroy(mutex) == State);
    Require(sceUltWaitingQueueResourcePoolDestroy(pool) == State);
    Require(sceUltMutexUnlock(mutex) == Ok);
    waiter.join();
    Require(waiterAcquired.load(std::memory_order_acquire));
    Require(sceUltWaitingQueueResourcePoolDestroy(pool) == Ok);
    Require(sceUltWaitingQueueResourcePoolDestroy(pool) == State);
    Require(sceUltMutexDestroy(mutex) == Ok);
    Require(sceUltMutexDestroy(mutex) == State);
    Require(sceUltSemaphoreDestroy(semaphore) == Ok);
    Require(sceUltQueueDestroy(queue) == Ok);
    Require(sceUltQueueDataResourcePoolDestroy(dataPool) == Ok);
}

static void UnwritableStatus() {
    Runtime runtime;
    Thread thread;
    auto* unwritable = reinterpret_cast<std::int32_t*>(0x10);
    std::int32_t status = 0;
    Require(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == Ok);
    Require(sceUltUlthreadCreate(&thread, "thread", Immediate, 0x27, nullptr, 0, &runtime, nullptr, 0) == Ok);
    errno = 71;
    Require(sceUltUlthreadJoin(&thread, unwritable) == Null);
    Require(errno == 71);
    Require(sceUltUlthreadJoin(&thread, &status) == State);
    Require(sceUltUlthreadRuntimeDestroy(&runtime) == Ok);

    Require(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == Ok);
    Require(sceUltUlthreadCreate(&thread, "thread", Immediate, 0x27, nullptr, 0, &runtime, nullptr, 0) == Ok);
    int joined;
    do {
        errno = 71;
        joined = sceUltUlthreadTryJoin(&thread, unwritable);
        if (joined == Again) std::this_thread::yield();
    } while (joined == Again);
    Require(joined == Null);
    Require(errno == 71);
    Require(sceUltUlthreadTryJoin(&thread, &status) == State);
    Require(sceUltUlthreadRuntimeDestroy(&runtime) == Ok);
    Require(sceUltFinalize() == Ok);
}

int main(int argc, char** argv) {
    Require(argc == 2);
    Require(sceUltInitialize() == Ok);
    if (std::strcmp(argv[1], "outstanding") == 0) Outstanding();
    else if (std::strcmp(argv[1], "validation") == 0) Validation();
    else if (std::strcmp(argv[1], "tryjoin") == 0) TryJoin();
    else if (std::strcmp(argv[1], "destroy") == 0) Destroy();
    else if (std::strcmp(argv[1], "underscoredcreate") == 0) UnderscoredCreate();
    else if (std::strcmp(argv[1], "cancel-blocked") == 0) CanceledJoin(false);
    else if (std::strcmp(argv[1], "cancel-pending") == 0) CanceledJoin(true);
    else if (std::strcmp(argv[1], "self-join") == 0) SelfJoin();
    else if (std::strcmp(argv[1], "unwritable-status") == 0) UnwritableStatus();
    else Require(false);
}
