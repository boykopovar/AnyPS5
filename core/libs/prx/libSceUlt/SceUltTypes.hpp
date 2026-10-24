#ifndef CORE_LIBS_PRX_LIBSCEULT_SCEULTTYPES_HPP
#define CORE_LIBS_PRX_LIBSCEULT_SCEULTTYPES_HPP

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstring>

#include <cstddef>
#include <memory>
#include <list>
#include <mutex>
#include <thread>
#include <vector>
#include "SceTypes.hpp"

constexpr int ULT_OK = 0;
constexpr int ULT_ERROR_NULL = -2139029503;
constexpr int ULT_ERROR_ALIGNMENT = -2139029502;
constexpr int ULT_ERROR_RANGE = -2139029501;
constexpr int ULT_ERROR_INVALID = -2139029500;
constexpr int ULT_ERROR_PERMISSION = -2139029499;
constexpr int ULT_ERROR_STATE = -2139029498;
constexpr int ULT_ERROR_BUSY = -2139029497;
constexpr int ULT_ERROR_AGAIN = -2139029496;

struct UltOptParam {
    std::uint8_t bytes[128];
};

inline int InitializeOptParam(UltOptParam* optParam) {
    if (optParam == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::memset(optParam, 0, sizeof(*optParam));
    return ULT_OK;
}

struct alignas(8) UltCommonOptParam {
    std::uint32_t header[3];
    std::uint32_t buildVersion;
    std::uint8_t reserved[112];
};

static_assert(sizeof(UltCommonOptParam) == 128);

struct UltWaitingThreadInfo {
    std::uint64_t size;
    void* thread;
    std::uint32_t threadType;
    std::int32_t waitValue;
    std::byte reserved[40];
};

static_assert(sizeof(UltWaitingThreadInfo) == 64);
static_assert(offsetof(UltWaitingThreadInfo, size) == 0);
static_assert(offsetof(UltWaitingThreadInfo, thread) == 8);
static_assert(offsetof(UltWaitingThreadInfo, threadType) == 16);
static_assert(offsetof(UltWaitingThreadInfo, waitValue) == 20);
static_assert(offsetof(UltWaitingThreadInfo, reserved) == 24);

struct UltConditionVariableInfo {
    std::uint64_t size;
    void* object;
    char name[32];
    void* mutex;
    std::uint32_t waitingThreads;
    std::byte reserved[68];
};

static_assert(sizeof(UltConditionVariableInfo) == 128);
static_assert(offsetof(UltConditionVariableInfo, size) == 0);
static_assert(offsetof(UltConditionVariableInfo, object) == 8);
static_assert(offsetof(UltConditionVariableInfo, name) == 16);
static_assert(offsetof(UltConditionVariableInfo, mutex) == 48);
static_assert(offsetof(UltConditionVariableInfo, waitingThreads) == 56);
static_assert(offsetof(UltConditionVariableInfo, reserved) == 60);

struct UltMutexInfo {
    std::uint64_t size;
    void* object;
    char name[32];
    void* waitingQueuePool;
    void* owner;
    std::uint32_t ownerType;
    std::uint32_t attributes;
    std::uint32_t lockCount;
    std::uint32_t waitingThreads;
    std::byte reserved[48];
};

static_assert(sizeof(UltMutexInfo) == 128);
static_assert(offsetof(UltMutexInfo, size) == 0);
static_assert(offsetof(UltMutexInfo, object) == 8);
static_assert(offsetof(UltMutexInfo, name) == 16);
static_assert(offsetof(UltMutexInfo, waitingQueuePool) == 48);
static_assert(offsetof(UltMutexInfo, owner) == 56);
static_assert(offsetof(UltMutexInfo, ownerType) == 64);
static_assert(offsetof(UltMutexInfo, attributes) == 68);
static_assert(offsetof(UltMutexInfo, lockCount) == 72);
static_assert(offsetof(UltMutexInfo, waitingThreads) == 76);
static_assert(offsetof(UltMutexInfo, reserved) == 80);

struct UltSemaphoreInfo {
    std::uint64_t size;
    void* object;
    char name[32];
    void* waitingQueuePool;
    std::uint32_t resources;
    std::uint32_t waitingThreads;
    std::byte reserved[64];
};

static_assert(sizeof(UltSemaphoreInfo) == 128);
static_assert(offsetof(UltSemaphoreInfo, size) == 0);
static_assert(offsetof(UltSemaphoreInfo, object) == 8);
static_assert(offsetof(UltSemaphoreInfo, name) == 16);
static_assert(offsetof(UltSemaphoreInfo, waitingQueuePool) == 48);
static_assert(offsetof(UltSemaphoreInfo, resources) == 56);
static_assert(offsetof(UltSemaphoreInfo, waitingThreads) == 60);
static_assert(offsetof(UltSemaphoreInfo, reserved) == 64);

struct UltReaderWriterLockInfo {
    std::uint64_t size;
    void* object;
    char name[32];
    void* waitingQueuePool;
    std::uint32_t lockType;
    std::uint32_t readers;
    void* owner;
    std::uint32_t ownerType;
    std::uint32_t waitingThreads;
    std::byte reserved[48];
};

static_assert(sizeof(UltReaderWriterLockInfo) == 128);
static_assert(offsetof(UltReaderWriterLockInfo, size) == 0);
static_assert(offsetof(UltReaderWriterLockInfo, object) == 8);
static_assert(offsetof(UltReaderWriterLockInfo, name) == 16);
static_assert(offsetof(UltReaderWriterLockInfo, waitingQueuePool) == 48);
static_assert(offsetof(UltReaderWriterLockInfo, lockType) == 56);
static_assert(offsetof(UltReaderWriterLockInfo, readers) == 60);
static_assert(offsetof(UltReaderWriterLockInfo, owner) == 64);
static_assert(offsetof(UltReaderWriterLockInfo, ownerType) == 72);
static_assert(offsetof(UltReaderWriterLockInfo, waitingThreads) == 76);
static_assert(offsetof(UltReaderWriterLockInfo, reserved) == 80);

struct UltQueueInfo {
    std::uint64_t size;
    void* object;
    char name[32];
    void* waitingQueuePool;
    void* dataPool;
    std::uint64_t dataSize;
    std::uint32_t queuedItems;
    std::uint32_t waitingPushers;
    std::uint32_t waitingPoppers;
    std::byte reserved[172];
};

static_assert(sizeof(UltQueueInfo) == 256);
static_assert(offsetof(UltQueueInfo, size) == 0);
static_assert(offsetof(UltQueueInfo, object) == 8);
static_assert(offsetof(UltQueueInfo, name) == 16);
static_assert(offsetof(UltQueueInfo, waitingQueuePool) == 48);
static_assert(offsetof(UltQueueInfo, dataPool) == 56);
static_assert(offsetof(UltQueueInfo, dataSize) == 64);
static_assert(offsetof(UltQueueInfo, queuedItems) == 72);
static_assert(offsetof(UltQueueInfo, waitingPushers) == 76);
static_assert(offsetof(UltQueueInfo, waitingPoppers) == 80);
static_assert(offsetof(UltQueueInfo, reserved) == 84);

struct UltWaitingQueueResourcePoolInfo {
    std::uint64_t size;
    void* object;
    char name[32];
    std::uint32_t availableThreads;
    std::uint32_t maxThreads;
    std::uint32_t availableSyncObjects;
    std::uint32_t maxSyncObjects;
    std::byte reserved[64];
};

static_assert(sizeof(UltWaitingQueueResourcePoolInfo) == 128);
static_assert(offsetof(UltWaitingQueueResourcePoolInfo, size) == 0);
static_assert(offsetof(UltWaitingQueueResourcePoolInfo, object) == 8);
static_assert(offsetof(UltWaitingQueueResourcePoolInfo, name) == 16);
static_assert(offsetof(UltWaitingQueueResourcePoolInfo, availableThreads) == 48);
static_assert(offsetof(UltWaitingQueueResourcePoolInfo, maxThreads) == 52);
static_assert(offsetof(UltWaitingQueueResourcePoolInfo, availableSyncObjects) == 56);
static_assert(offsetof(UltWaitingQueueResourcePoolInfo, maxSyncObjects) == 60);
static_assert(offsetof(UltWaitingQueueResourcePoolInfo, reserved) == 64);

struct UltQueueDataResourcePoolInfo {
    std::uint64_t size;
    void* object;
    char name[32];
    void* waitingQueuePool;
    std::uint64_t dataSize;
    std::uint32_t availableItems;
    std::uint32_t maxItems;
    std::uint32_t availableQueues;
    std::uint32_t maxQueues;
    std::byte reserved[48];
};

static_assert(sizeof(UltQueueDataResourcePoolInfo) == 128);
static_assert(offsetof(UltQueueDataResourcePoolInfo, size) == 0);
static_assert(offsetof(UltQueueDataResourcePoolInfo, object) == 8);
static_assert(offsetof(UltQueueDataResourcePoolInfo, name) == 16);
static_assert(offsetof(UltQueueDataResourcePoolInfo, waitingQueuePool) == 48);
static_assert(offsetof(UltQueueDataResourcePoolInfo, dataSize) == 56);
static_assert(offsetof(UltQueueDataResourcePoolInfo, availableItems) == 64);
static_assert(offsetof(UltQueueDataResourcePoolInfo, maxItems) == 68);
static_assert(offsetof(UltQueueDataResourcePoolInfo, availableQueues) == 72);
static_assert(offsetof(UltQueueDataResourcePoolInfo, maxQueues) == 76);
static_assert(offsetof(UltQueueDataResourcePoolInfo, reserved) == 80);

struct UltUlthreadRuntimeInfo {
    std::uint64_t size;
    void* object;
    char name[32];
    std::uint32_t activeUlthreads;
    std::uint32_t maxUlthreads;
    std::uint32_t workerThreads;
    std::uint32_t reserved60;
    std::uint64_t workerStackSize;
    std::byte reserved[56];
};

static_assert(sizeof(UltUlthreadRuntimeInfo) == 128);
static_assert(offsetof(UltUlthreadRuntimeInfo, size) == 0);
static_assert(offsetof(UltUlthreadRuntimeInfo, object) == 8);
static_assert(offsetof(UltUlthreadRuntimeInfo, name) == 16);
static_assert(offsetof(UltUlthreadRuntimeInfo, maxUlthreads) == 52);
static_assert(offsetof(UltUlthreadRuntimeInfo, activeUlthreads) == 48);
static_assert(offsetof(UltUlthreadRuntimeInfo, workerThreads) == 56);
static_assert(offsetof(UltUlthreadRuntimeInfo, reserved60) == 60);
static_assert(offsetof(UltUlthreadRuntimeInfo, workerStackSize) == 64);
static_assert(offsetof(UltUlthreadRuntimeInfo, reserved) == 72);

struct UltUlthreadInfo {
    std::uint64_t size;
    void* object;
    char name[32];
    std::uint32_t contextAttribute;
    std::uint32_t reserved52;
    void* runtime;
    FiberInfo fiber;
    std::byte reserved[64];
};

static_assert(sizeof(FiberInfo) == 128);
static_assert(offsetof(FiberInfo, entry) == 8);
static_assert(offsetof(FiberInfo, arg_on_initialize) == 16);
static_assert(offsetof(FiberInfo, addr_context) == 24);
static_assert(offsetof(FiberInfo, size_context) == 32);
static_assert(offsetof(FiberInfo, name) == 40);
static_assert(offsetof(FiberInfo, size_context_margin) == 72);
static_assert(sizeof(UltUlthreadInfo) == 256);
static_assert(offsetof(UltUlthreadInfo, object) == 8);
static_assert(offsetof(UltUlthreadInfo, name) == 16);
static_assert(offsetof(UltUlthreadInfo, contextAttribute) == 48);
static_assert(offsetof(UltUlthreadInfo, runtime) == 56);
static_assert(offsetof(UltUlthreadInfo, fiber) == 64);
static_assert(offsetof(UltUlthreadInfo, reserved) == 192);

struct UltMutexState {
    std::mutex _mutex;
    std::condition_variable _available;
    std::thread::id _owner{};
    std::uint32_t _lockCount = 0;
    std::uint32_t _waiters = 0;
    std::uint32_t _conditions = 0;
    std::uint32_t _conditionWaiters = 0;
    bool _alive = true;
    std::uint32_t _attribute = 0;
    void* _waitingPool = nullptr;
};

struct UltConditionWaiter {
    std::condition_variable _available;
    bool _signaled = false;
};

struct UltConditionState {
    std::shared_ptr<UltMutexState> _mutex;
    std::list<UltConditionWaiter*> _queue;
    std::uint32_t _waiters = 0;
    bool _alive = true;
};

struct UltSemaphoreState {
    std::mutex _mutex;
    std::condition_variable _available;
    std::int32_t _resources = 0;
    std::uint32_t _waiters = 0;
    bool _alive = true;
    void* _waitingPool = nullptr;
};

struct UltResourcePoolState {
    std::uint32_t _numThreads = 0;
    std::uint32_t _numSyncObjects = 0;
    void* _workArea = nullptr;
};

struct UltQueueDataPoolState {
    std::mutex _mutex;
    std::condition_variable _spaceAvailable;
    std::vector<std::uint8_t> _data;
    std::vector<std::uint32_t> _next;
    std::uint32_t _free = UINT32_MAX;
    std::uint32_t _queues = 0;
    std::uint32_t _numData = 0;
    std::uint64_t _dataSize = 0;
    std::uint32_t _numQueueObject = 0;
    void* _waitingPool = nullptr;
    void* _workArea = nullptr;
};

struct UltQueueState {
    std::shared_ptr<UltQueueDataPoolState> _pool;
    std::condition_variable _dataAvailable;
    std::uint32_t _head = UINT32_MAX;
    std::uint32_t _tail = UINT32_MAX;
    std::uint32_t _waiters = 0;
    bool _alive = true;
    std::uint64_t _dataSize = 0;
};

struct UltRuntimeState {
    std::uint32_t _maxNumUlthread = 0;
    std::uint32_t _numWorkerThread = 0;
    void* _workArea = nullptr;
};

struct UltUlthreadState {
    UltUlthreadEntry _entry = nullptr;
    std::uint64_t _arg = 0;
    void* _object = nullptr;
    Pthread _thread = nullptr;
    void* _runtime = nullptr;
    std::atomic<bool> _exited{false};
};

#endif
