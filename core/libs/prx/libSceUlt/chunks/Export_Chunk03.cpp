extern "C" {
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceUlt/include/Api.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cerrno>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "SceUltTypes.hpp"
#include "prx/libSceUlt/src/ReaderWriterLock.hpp"

namespace {

std::mutex gMutex;
std::unordered_map<void*, std::shared_ptr<UltMutexState>> gMutexes;
std::unordered_map<void*, std::shared_ptr<UltConditionState>> gConditions;
std::unordered_map<void*, std::shared_ptr<UltSemaphoreState>> gSemaphores;
std::unordered_map<void*, UltResourcePoolState> gResourcePools;
std::unordered_map<void*, std::shared_ptr<UltQueueDataPoolState>> gQueueDataPools;
std::unordered_map<void*, std::shared_ptr<UltQueueState>> gQueues;
std::unordered_map<void*, UltRuntimeState> gRuntimes;
std::unordered_map<void*, std::shared_ptr<UltUlthreadState>> gUlthreads;

int validateCommonOptParam(const void* optParam) {
    if (optParam == nullptr) return ULT_OK;
    if ((reinterpret_cast<std::uintptr_t>(optParam) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    UltCommonOptParam value;
    if (!GuestCopyIn_nid_no_patch(&value, optParam, sizeof(value))) return ULT_ERROR_NULL;
    if (value.header[0] != 0x6C696275 || value.header[1] != 0x6C746D61 || value.header[2] != 0x67696300) return ULT_ERROR_INVALID;
    if (std::any_of(std::begin(value.reserved), std::end(value.reserved), [](std::uint8_t byte) { return byte != 0; })) return ULT_ERROR_INVALID;
    return ULT_OK;
}

int semaphoreGetState(void* semaphore, std::shared_ptr<UltSemaphoreState>* out) {
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gSemaphores.find(semaphore);
    if (it == gSemaphores.end()) {
        return semaphore == nullptr ? ULT_ERROR_NULL : ULT_ERROR_STATE;
    }
    *out = it->second;
    return ULT_OK;
}

int semaphoreRequest(void* semaphore, std::int32_t numResource, std::shared_ptr<UltSemaphoreState>* out) {
    if (semaphore == nullptr) return ULT_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(semaphore) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    if (numResource <= 0) return ULT_ERROR_RANGE;
    return semaphoreGetState(semaphore, out);
}

int queueGetState(void* queue, std::shared_ptr<UltQueueState>* out) {
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gQueues.find(queue);
    if (it == gQueues.end()) {
        return queue == nullptr ? ULT_ERROR_NULL : ULT_ERROR_STATE;
    }
    *out = it->second;
    return ULT_OK;
}

int QueueTransfer(void* queue, void* data, const void* source, bool push, bool blocking) {
    std::shared_ptr<UltQueueState> state;
    if (int result = queueGetState(queue, &state); result != ULT_OK) return result;
    if ((push ? source : data) == nullptr) return ULT_ERROR_NULL;
    auto& pool = *state->_pool;
    std::unique_lock lock(pool._mutex);
    if (!state->_alive) return ULT_ERROR_STATE;
    const auto ready = [&] { return !state->_alive || (push ? pool._free : state->_head) != UINT32_MAX; };
    if (!ready()) {
        if (!blocking) return ULT_ERROR_AGAIN;
        auto& waiters = push ? state->_waitingPushers : state->_waitingPoppers;
        auto& records = push ? state->_pusherRecords : state->_popperRecords;
        UltWaitRecord record;
        const bool tracked = CurrentWaitRecord(record, push ? 2 : 1);
        if (tracked) records.push_back(record);
        ++waiters;
        try {
            (push ? pool._spaceAvailable : state->_dataAvailable).wait(lock, ready);
        } catch (...) {
            --waiters;
            if (tracked) RemoveWaitRecord(records, record);
            throw;
        }
        --waiters;
        if (tracked) RemoveWaitRecord(records, record);
        if (!state->_alive) return ULT_ERROR_STATE;
    }
    if (push) {
        const auto slot = pool._free;
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(pool._data.data() + slot * pool._dataSize, source, state->_dataSize);
        errno = savedErrno;
        if (!copied) return ULT_ERROR_NULL;
        pool._free = pool._next[slot];
        pool._next[slot] = UINT32_MAX;
        if (state->_tail == UINT32_MAX) state->_head = slot;
        else pool._next[state->_tail] = slot;
        state->_tail = slot;
        ++state->_queuedItems;
    } else {
        const auto slot = state->_head;
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(data, pool._data.data() + slot * pool._dataSize, state->_dataSize);
        errno = savedErrno;
        if (!copied) return ULT_ERROR_NULL;
        state->_head = pool._next[slot];
        if (state->_head == UINT32_MAX) state->_tail = UINT32_MAX;
        pool._next[slot] = pool._free;
        pool._free = slot;
        --state->_queuedItems;
    }
    lock.unlock();
    if (push) state->_dataAvailable.notify_one();
    else pool._spaceAvailable.notify_one();
    return ULT_OK;
}

void ReleaseQueue(UltQueueState& state) {
    auto& pool = *state._pool;
    if (state._tail != UINT32_MAX) {
        pool._next[state._tail] = pool._free;
        pool._free = state._head;
    }
    state._head = state._tail = UINT32_MAX;
    state._queuedItems = 0;
    state._alive = false;
    --pool._queues;
}

thread_local UltUlthreadState* gCurrentUlthread = nullptr;

void* APS5_VABI ulthreadRunner(void* arg) {
    auto* state = static_cast<UltUlthreadState*>(arg);
    gCurrentUlthread = state;
    const auto result = state->_entry(state->_arg);
    state->_exited.store(true, std::memory_order_release);
    return reinterpret_cast<void*>(static_cast<std::intptr_t>(result));
}

void APS5_VABI releaseUlthreadJoin(void* argument) {
    auto& state = *static_cast<std::shared_ptr<UltUlthreadState>*>(argument);
    if (!state) return;
    {
        std::lock_guard lock(gMutex);
        state->_joining = false;
    }
    state.reset();
}

int joinUlthread(void* ulthread, std::int32_t* status, bool blocking) {
    if (ulthread == nullptr) return ULT_ERROR_NULL;
    std::shared_ptr<UltUlthreadState> state;
    {
        std::lock_guard lock(gMutex);
        const auto it = gUlthreads.find(ulthread);
        if (it == gUlthreads.end()) return ULT_ERROR_STATE;
        state = it->second;
        if (!blocking && !state->_exited.load(std::memory_order_acquire)) return ULT_ERROR_AGAIN;
        if (state->_joining || state.get() == gCurrentUlthread) return ULT_ERROR_STATE;
        state->_joining = true;
    }
    CleanupHandler cleanup{};
    bool cleanupRegistered = false;
    void* result = nullptr;
    int joined;
    try {
        __pthread_cleanup_push_imp_nid_postfix(releaseUlthreadJoin, &state, &cleanup);
        cleanupRegistered = true;
        joined = scePthreadJoin(state->_thread, &result);
    } catch (...) {
        if (state) {
            if (cleanupRegistered) __pthread_cleanup_pop_imp_nid_postfix(1);
            else releaseUlthreadJoin(&state);
        }
        throw;
    }
    __pthread_cleanup_pop_imp_nid_postfix(0);
    if (joined != 0) {
        releaseUlthreadJoin(&state);
        return ULT_ERROR_STATE;
    }
    {
        std::lock_guard lock(gMutex);
        const auto it = gUlthreads.find(ulthread);
        if (it != gUlthreads.end() && it->second == state) gUlthreads.erase(it);
    }
    if (status == nullptr) return ULT_OK;
    const auto exitStatus = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(result));
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(status, &exitStatus, sizeof(exitStatus));
    errno = savedErrno;
    return copied ? ULT_OK : ULT_ERROR_NULL;
}

}

int APS5_VABI sceUltUlthreadCreate(void* ulthread, const char* name, UltUlthreadEntry entry, std::uint64_t arg, void* context, std::uint64_t sizeContext, void* runtime, const void* optParam, std::uint32_t buildVersion) {
    (void)context;
    (void)sizeContext;
    (void)optParam;
    (void)buildVersion;
    if (ulthread == nullptr || entry == nullptr || runtime == nullptr) {
        return ULT_ERROR_NULL;
    }
    auto state = std::make_shared<UltUlthreadState>();
    state->_entry = entry;
    state->_arg = arg;
    state->_object = ulthread;
    state->_runtime = runtime;
    if (!CopyUltName(state->_name, name)) return ULT_ERROR_NULL;
    std::lock_guard lock(gMutex);
    if (!gRuntimes.contains(runtime) || gUlthreads.contains(ulthread)) return ULT_ERROR_STATE;
    const std::string threadName = state->_name;
    if (!ClearUltObject(ulthread, 512)) return ULT_ERROR_NULL;
    gUlthreads.emplace(ulthread, state);
    try {
        if (scePthreadCreate(&state->_thread, nullptr, ulthreadRunner, state.get(), threadName.c_str()) != 0) {
            gUlthreads.erase(ulthread);
            return ULT_ERROR_AGAIN;
        }
    } catch (...) {
        if (state->_thread == nullptr) gUlthreads.erase(ulthread);
        throw;
    }
    return ULT_OK;
}

int APS5_VABI sceUltUlthreadJoin(void* ulthread, std::int32_t* status) {
    return joinUlthread(ulthread, status, true);
}

int APS5_VABI sceUltUlthreadTryJoin(void* ulthread, std::int32_t* status) {
    return joinUlthread(ulthread, status, false);
}

int APS5_VABI sceUltUlthreadGetSelf(void** ulthread) {
    if (ulthread == nullptr) {
        return ULT_ERROR_NULL;
    }
    if (gCurrentUlthread == nullptr) {
        return ULT_ERROR_STATE;
    }
    if (!GuestCopyOut_nid_no_patch(ulthread, &gCurrentUlthread->_object, sizeof(*ulthread))) return ULT_ERROR_NULL;
    return ULT_OK;
}

int APS5_VABI sceUltUlthreadYield() {
    if (gCurrentUlthread == nullptr) {
        return ULT_ERROR_STATE;
    }
    scePthreadYield();

    return ULT_OK;
}

int APS5_VABI sceUltUlthreadExit(std::int32_t status) {
    if (gCurrentUlthread == nullptr) {
        return ULT_ERROR_STATE;
    }
    gCurrentUlthread->_exited.store(true, std::memory_order_release);
    scePthreadExit(reinterpret_cast<void*>(static_cast<std::intptr_t>(status)));

    return ULT_OK;
}

int APS5_VABI sceUltUlthreadRuntimeDestroy(void* runtime) {
    if (runtime == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gRuntimes.find(runtime);
    if (it == gRuntimes.end()) {
        return ULT_ERROR_STATE;
    }
    for (const auto& entry : gUlthreads) {
        if (entry.second->_runtime == runtime) {
            return ULT_ERROR_STATE;
        }
    }
    gRuntimes.erase(it);
    return ULT_OK;
}

int APS5_VABI _sceUltWaitingQueueResourcePoolOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    return sceUltWaitingQueueResourcePoolOptParamInitialize(optParam, buildVersion);
}

int APS5_VABI _sceUltWaitingQueueResourcePoolCreate(void* pool, const char* name, std::uint32_t numThreads, std::uint32_t numSyncObjects, void* workArea, const void* optParam) {
    if (pool == nullptr) return ULT_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(pool) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    if (name == nullptr) return ULT_ERROR_NULL;
    if (const int result = validateCommonOptParam(optParam); result != ULT_OK) return result;
    if (numThreads == 0 || numSyncObjects == 0) return ULT_ERROR_INVALID;
    if ((reinterpret_cast<std::uintptr_t>(workArea) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    if (workArea == nullptr) return ULT_ERROR_NULL;
    return sceUltWaitingQueueResourcePoolCreate(pool, name, numThreads, numSyncObjects, workArea, optParam, 0);
}

int APS5_VABI _sceUltSemaphoreOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    return sceUltSemaphoreOptParamInitialize(optParam, buildVersion);
}

int APS5_VABI _sceUltSemaphoreCreate(void* semaphore, const char* name, std::uint32_t numInitialResource, void* waitingQueueResourcePool, const void* optParam) {
    if (semaphore == nullptr || name == nullptr) return ULT_ERROR_NULL;
    if (((reinterpret_cast<std::uintptr_t>(semaphore) | reinterpret_cast<std::uintptr_t>(waitingQueueResourcePool)) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    if (const int result = validateCommonOptParam(optParam); result != ULT_OK) return result;
    return sceUltSemaphoreCreate(semaphore, name, numInitialResource, waitingQueueResourcePool, optParam, 0);
}

int APS5_VABI _sceUltMutexOptParamInitialize(UltMutexOptParam* optParam, std::uint32_t buildVersion) {
    return sceUltMutexOptParamInitialize(optParam, buildVersion);
}

int APS5_VABI _sceUltMutexCreate(void* mutex, const char* name, void* waitingQueueResourcePool, const UltMutexOptParam* optParam) {
    if (mutex == nullptr || name == nullptr || waitingQueueResourcePool == nullptr) {
        return ULT_ERROR_NULL;
    }
    if (((reinterpret_cast<std::uintptr_t>(mutex) | reinterpret_cast<std::uintptr_t>(optParam) | reinterpret_cast<std::uintptr_t>(waitingQueueResourcePool)) & 7u) != 0) {
        return ULT_ERROR_ALIGNMENT;
    }
    if (optParam != nullptr) {
        UltMutexOptParam options;
        if (!GuestCopyIn_nid_no_patch(&options, optParam, sizeof(options))) return ULT_ERROR_NULL;
        if (options.header[0] != 0x6C696275 || options.header[1] != 0x6C746D61 || options.header[2] != 0x67696300 || options.attribute > 1 || std::any_of(std::begin(options.reserved), std::end(options.reserved), [](std::uint32_t value) { return value != 0; })) {
            return ULT_ERROR_INVALID;
        }
    }
    return sceUltMutexCreate(mutex, name, waitingQueueResourcePool, optParam, 0);
}

int APS5_VABI _sceUltQueueDataResourcePoolOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    return sceUltQueueDataResourcePoolOptParamInitialize(optParam, buildVersion);
}

int APS5_VABI _sceUltQueueDataResourcePoolCreate(void* pool, const char* name, std::uint32_t numData, std::uint64_t dataSize, std::uint32_t numQueueObject, void* waitingQueueResourcePool, void* workArea, const void* optParam) {
    if (pool == nullptr) return ULT_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(pool) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    if (const int result = validateCommonOptParam(optParam); result != ULT_OK) return result;
    if (numData == 0 || dataSize == 0 || numQueueObject == 0) return ULT_ERROR_INVALID;
    if ((reinterpret_cast<std::uintptr_t>(workArea) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    if (workArea == nullptr || name == nullptr) return ULT_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(waitingQueueResourcePool) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    return sceUltQueueDataResourcePoolCreate(pool, name, numData, dataSize, numQueueObject, waitingQueueResourcePool, workArea, optParam, 0);
}

int APS5_VABI _sceUltQueueOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    return sceUltQueueOptParamInitialize(optParam, buildVersion);
}

int APS5_VABI _sceUltQueueCreate(void* queue, const char* name, std::uint64_t dataSize, void* waitingQueueResourcePool, void* queueDataResourcePool, const void* optParam) {
    if (queue == nullptr || name == nullptr || queueDataResourcePool == nullptr) return ULT_ERROR_NULL;
    if (((reinterpret_cast<std::uintptr_t>(queue) | reinterpret_cast<std::uintptr_t>(waitingQueueResourcePool) | reinterpret_cast<std::uintptr_t>(queueDataResourcePool)) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    {
        std::lock_guard lock(gMutex);
        const auto it = gQueueDataPools.find(queueDataResourcePool);
        if (it == gQueueDataPools.end()) return ULT_ERROR_INVALID;
        if (dataSize == 0 || dataSize > it->second->_dataSize) return ULT_ERROR_INVALID;
    }
    if (const int result = validateCommonOptParam(optParam); result != ULT_OK) return result;
    return sceUltQueueCreate(queue, name, dataSize, waitingQueueResourcePool, queueDataResourcePool, optParam, 0);
}

int APS5_VABI _sceUltUlthreadRuntimeOptParamInitialize(UltUlthreadRuntimeOptParam* optParam, std::uint32_t buildVersion) {
    return sceUltUlthreadRuntimeOptParamInitialize(optParam, buildVersion);
}

int APS5_VABI _sceUltUlthreadRuntimeCreate(void* runtime, const char* name, std::uint32_t maxNumUlthread, std::uint32_t numWorkerThread, void* workArea, const void* optParam) {
    return sceUltUlthreadRuntimeCreate(runtime, name, maxNumUlthread, numWorkerThread, workArea, optParam, 0);
}

int APS5_VABI _sceUltUlthreadOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    return sceUltUlthreadOptParamInitialize(optParam, buildVersion);
}

int APS5_VABI _sceUltUlthreadCreate(void* ulthread, const char* name, UltUlthreadEntry entry, std::uint64_t arg, void* context, std::uint64_t sizeContext, void* runtime, const void* optParam) {
    return sceUltUlthreadCreate(ulthread, name, entry, arg, context, sizeContext, runtime, optParam, 0);
}

int APS5_VABI sceUltGetConditionVariableInfo(void* object, UltConditionVariableInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity) {
    struct RestoreErrno {
        int value = errno;
        ~RestoreErrno() { errno = value; }
    } restore;
    if (object == nullptr || info == nullptr) return ULT_ERROR_NULL;
    if (waitingThreads == nullptr && capacity != 0) return ULT_ERROR_NULL;
    std::shared_ptr<UltConditionState> state;
    {
        std::lock_guard lock(gMutex);
        const auto it = gConditions.find(object);
        if (it == gConditions.end()) return ULT_ERROR_STATE;
        state = it->second;
    }
    std::lock_guard stateLock(state->_mutex->_mutex);
    if (!state->_alive) return ULT_ERROR_STATE;
    if (const int status = ValidateWaitRecords(waitingThreads, capacity); status != ULT_OK) return status;
    UltConditionVariableInfo localInfo{};
    localInfo.size = sizeof(UltConditionVariableInfo);
    localInfo.object = object;
    std::memcpy(localInfo.name, state->_name, sizeof(localInfo.name));
    localInfo.mutex = state->_mutexObject;
    localInfo.waitingThreads = static_cast<std::uint32_t>(state->_queue.size());
    std::vector<UltWaitRecord> records;
    for (const auto* waiter : state->_queue) {
        if (waiter->_tracked) records.push_back(waiter->_record);
    }
    const std::size_t count = std::min<std::size_t>(capacity, records.size());
    UltConditionVariableInfo previousInfo{};
    if (count > 0 && !GuestCopyIn_nid_no_patch(&previousInfo, info, sizeof(previousInfo))) return ULT_ERROR_NULL;
    if (!GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo))) return ULT_ERROR_NULL;
    if (count > 0) {
        const int writeStatus = WriteWaitRecords(records, waitingThreads, capacity);
        if (writeStatus != ULT_OK) {
            GuestCopyOut_nid_no_patch(info, &previousInfo, sizeof(previousInfo));
            return writeStatus;
        }
    }
    return ULT_OK;
}

int APS5_VABI sceUltGetMutexInfo(void* object, UltMutexInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity) {
    struct RestoreErrno {
        int value = errno;
        ~RestoreErrno() { errno = value; }
    } restore;
    if (object == nullptr || info == nullptr) return ULT_ERROR_NULL;
    if (waitingThreads == nullptr && capacity != 0) return ULT_ERROR_NULL;
    std::shared_ptr<UltMutexState> state;
    {
        std::lock_guard lock(gMutex);
        const auto it = gMutexes.find(object);
        if (it == gMutexes.end()) return ULT_ERROR_STATE;
        state = it->second;
    }
    std::lock_guard stateLock(state->_mutex);
    if (!state->_alive) return ULT_ERROR_STATE;
    if (const int status = ValidateWaitRecords(waitingThreads, capacity); status != ULT_OK) return status;
    UltMutexInfo localInfo{};
    localInfo.size = sizeof(UltMutexInfo);
    localInfo.object = object;
    std::memcpy(localInfo.name, state->_name, sizeof(localInfo.name));
    localInfo.waitingQueuePool = state->_waitingPool;
    localInfo.owner = state->_ownerIdentity.thread;
    localInfo.ownerType = state->_ownerIdentity.type;
    localInfo.attributes = state->_attribute;
    localInfo.lockCount = (state->_attribute & 1) ? state->_lockCount : 0;
    localInfo.waitingThreads = state->_waiters;
    const std::size_t count = std::min<std::size_t>(capacity, state->_waiterRecords.size());
    UltMutexInfo previousInfo{};
    if (count > 0 && !GuestCopyIn_nid_no_patch(&previousInfo, info, sizeof(previousInfo))) return ULT_ERROR_NULL;
    if (!GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo))) return ULT_ERROR_NULL;
    if (count > 0) {
        const int writeStatus = WriteWaitRecords(state->_waiterRecords, waitingThreads, capacity);
        if (writeStatus != ULT_OK) {
            GuestCopyOut_nid_no_patch(info, &previousInfo, sizeof(previousInfo));
            return writeStatus;
        }
    }
    return ULT_OK;
}

int APS5_VABI sceUltGetQueueDataResourcePoolInfo(void* object, UltQueueDataResourcePoolInfo* info) {
    if (object == nullptr || info == nullptr) return ULT_ERROR_NULL;
    std::shared_ptr<UltQueueDataPoolState> state;
    {
        std::lock_guard lock(gMutex);
        const auto it = gQueueDataPools.find(object);
        if (it == gQueueDataPools.end()) return ULT_ERROR_STATE;
        state = it->second;
    }
    std::lock_guard poolLock(state->_mutex);
    UltQueueDataResourcePoolInfo localInfo{};
    localInfo.size = sizeof(UltQueueDataResourcePoolInfo);
    localInfo.object = object;
    std::memcpy(localInfo.name, state->_name, sizeof(localInfo.name));
    localInfo.waitingQueuePool = state->_waitingPool;
    localInfo.dataSize = state->_dataSize;
    localInfo.maxItems = state->_numData;
    localInfo.maxQueues = state->_numQueueObject;
    localInfo.availableQueues = state->_numQueueObject > state->_queues ? state->_numQueueObject - state->_queues : 0;
    std::uint32_t freeCount = 0;
    for (auto next = state->_free; next != UINT32_MAX; next = state->_next[next]) {
        ++freeCount;
    }
    localInfo.availableItems = freeCount;
    if (!GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo))) return ULT_ERROR_NULL;
    return ULT_OK;
}

int APS5_VABI sceUltGetQueueInfo(void* object, UltQueueInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity) {
    struct RestoreErrno {
        int value = errno;
        ~RestoreErrno() { errno = value; }
    } restore;
    if (object == nullptr || info == nullptr) return ULT_ERROR_NULL;
    if (waitingThreads == nullptr && capacity != 0) return ULT_ERROR_NULL;
    std::shared_ptr<UltQueueState> state;
    {
        std::lock_guard lock(gMutex);
        const auto it = gQueues.find(object);
        if (it == gQueues.end()) return ULT_ERROR_STATE;
        state = it->second;
    }
    auto& pool = *state->_pool;
    std::lock_guard poolLock(pool._mutex);
    if (!state->_alive) return ULT_ERROR_STATE;
    if (const int status = ValidateWaitRecords(waitingThreads, capacity); status != ULT_OK) return status;
    UltQueueInfo localInfo{};
    localInfo.size = sizeof(UltQueueInfo);
    localInfo.object = object;
    std::memcpy(localInfo.name, state->_name, sizeof(localInfo.name));
    localInfo.waitingQueuePool = state->_waitingPool;
    localInfo.dataPool = state->_dataPool;
    localInfo.dataSize = state->_dataSize;
    localInfo.queuedItems = state->_queuedItems;
    localInfo.waitingPushers = state->_waitingPushers;
    localInfo.waitingPoppers = state->_waitingPoppers;
    std::vector<UltWaitRecord> records = state->_pusherRecords;
    records.insert(records.end(), state->_popperRecords.begin(), state->_popperRecords.end());
    const std::size_t count = std::min<std::size_t>(capacity, records.size());
    UltQueueInfo previousInfo{};
    if (count > 0 && !GuestCopyIn_nid_no_patch(&previousInfo, info, sizeof(previousInfo))) return ULT_ERROR_NULL;
    if (!GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo))) return ULT_ERROR_NULL;
    if (count > 0) {
        const int writeStatus = WriteWaitRecords(records, waitingThreads, capacity);
        if (writeStatus != ULT_OK) {
            GuestCopyOut_nid_no_patch(info, &previousInfo, sizeof(previousInfo));
            return writeStatus;
        }
    }
    return ULT_OK;
}

int APS5_VABI sceUltGetSemaphoreInfo(void* object, UltSemaphoreInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity) {
    struct RestoreErrno {
        int value = errno;
        ~RestoreErrno() { errno = value; }
    } restore;
    if (object == nullptr || info == nullptr) return ULT_ERROR_NULL;
    if (waitingThreads == nullptr && capacity != 0) return ULT_ERROR_NULL;
    std::shared_ptr<UltSemaphoreState> state;
    {
        std::lock_guard lock(gMutex);
        const auto it = gSemaphores.find(object);
        if (it == gSemaphores.end()) return ULT_ERROR_STATE;
        state = it->second;
    }
    std::lock_guard stateLock(state->_mutex);
    if (!state->_alive) return ULT_ERROR_STATE;
    if (const int status = ValidateWaitRecords(waitingThreads, capacity); status != ULT_OK) return status;
    UltSemaphoreInfo localInfo{};
    localInfo.size = sizeof(UltSemaphoreInfo);
    localInfo.object = object;
    std::memcpy(localInfo.name, state->_name, sizeof(localInfo.name));
    localInfo.waitingQueuePool = state->_waitingPool;
    localInfo.resources = static_cast<std::uint32_t>(state->_resources);
    localInfo.waitingThreads = state->_waiters;
    const std::size_t count = std::min<std::size_t>(capacity, state->_waiterRecords.size());
    UltSemaphoreInfo previousInfo{};
    if (count > 0 && !GuestCopyIn_nid_no_patch(&previousInfo, info, sizeof(previousInfo))) return ULT_ERROR_NULL;
    if (!GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo))) return ULT_ERROR_NULL;
    if (count > 0) {
        const int writeStatus = WriteWaitRecords(state->_waiterRecords, waitingThreads, capacity);
        if (writeStatus != ULT_OK) {
            GuestCopyOut_nid_no_patch(info, &previousInfo, sizeof(previousInfo));
            return writeStatus;
        }
    }
    return ULT_OK;
}

int APS5_VABI sceUltGetUlthreadInfo(void* object, UltUlthreadInfo* info) {
    if (object == nullptr || info == nullptr) return ULT_ERROR_NULL;
    std::shared_ptr<UltUlthreadState> state;
    {
        std::lock_guard lock(gMutex);
        const auto it = gUlthreads.find(object);
        if (it == gUlthreads.end()) return ULT_ERROR_STATE;
        state = it->second;
    }
    UltUlthreadInfo localInfo{};
    localInfo.size = sizeof(UltUlthreadInfo);
    localInfo.object = object;
    std::memcpy(localInfo.name, state->_name, sizeof(localInfo.name));
    localInfo.runtime = state->_runtime;
    localInfo.contextAttribute = state->_contextAttribute;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo));
    errno = savedErrno;
    if (!copied) return ULT_ERROR_NULL;
    return ULT_OK;
}

}
