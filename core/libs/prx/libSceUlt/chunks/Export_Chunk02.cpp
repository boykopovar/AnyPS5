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

int APS5_VABI sceUltConditionVariableWait(void* condition) {
    std::shared_ptr<UltConditionState> state;
    {
        std::lock_guard lock(gMutex);
        const auto it = gConditions.find(condition);
        if (it == gConditions.end()) return condition == nullptr ? ULT_ERROR_NULL : ULT_ERROR_STATE;
        state = it->second;
    }
    const auto held = state->_mutex;
    const auto owner = std::this_thread::get_id();
    std::unique_lock lock(held->_mutex);
    if (!state->_alive || !held->_alive || held->_owner != owner || held->_lockCount == 0) return ULT_ERROR_STATE;
    if (held->_lockCount != 1) throw std::runtime_error("ULT condition wait with a recursively held mutex is not implemented");
    const auto ownerIdentity = held->_ownerIdentity;
    UltConditionWaiter waiter;
    waiter._tracked = CurrentWaitRecord(waiter._record, 0);
    state->_queue.push_back(&waiter);
    ++state->_waiters;
    ++held->_conditionWaiters;
    held->_lockCount = 0;
    held->_owner = {};
    held->_ownerIdentity = {};
    held->_available.notify_one();
    try {
        waiter._available.wait(lock, [&] { return waiter._signaled || !state->_alive || !held->_alive; });
        if (held->_alive && state->_alive) {
            UltWaitRecord mutexRecord;
            const bool mutexTracked = CurrentWaitRecord(mutexRecord, 0);
            if (mutexTracked) held->_waiterRecords.push_back(mutexRecord);
            ++held->_waiters;
            try {
                held->_available.wait(lock, [&] { return !held->_alive || held->_lockCount == 0; });
            } catch (...) {
                --held->_waiters;
                if (mutexTracked) RemoveWaitRecord(held->_waiterRecords, mutexRecord);
                throw;
            }
            --held->_waiters;
            if (mutexTracked) RemoveWaitRecord(held->_waiterRecords, mutexRecord);
        }
    } catch (...) {
        state->_queue.remove(&waiter);
        --state->_waiters;
        --held->_conditionWaiters;
        throw;
    }
    state->_queue.remove(&waiter);
    --state->_waiters;
    --held->_conditionWaiters;
    if (!held->_alive || !state->_alive) return ULT_ERROR_STATE;
    held->_owner = owner;
    held->_ownerIdentity = ownerIdentity;
    held->_lockCount = 1;
    return ULT_OK;
}

std::uint64_t APS5_VABI sceUltWaitingQueueResourcePoolGetWorkAreaSize(std::uint32_t numThreads, std::uint32_t numSyncObjects) {
    return 32ull * (numSyncObjects + 2u * numThreads + 2u);
}

int APS5_VABI sceUltWaitingQueueResourcePoolCreate(void* pool, const char* name, std::uint32_t numThreads, std::uint32_t numSyncObjects, void* workArea, const void* optParam, std::uint32_t buildVersion) {
    (void)optParam;
    (void)buildVersion;
    if (pool == nullptr) {
        return ULT_ERROR_NULL;
    }
    UltResourcePoolState state{numThreads, numSyncObjects, workArea};
    if (!CopyUltName(state._name, name)) return ULT_ERROR_NULL;
    std::lock_guard<std::mutex> lock(gMutex);
    if (gResourcePools.contains(pool)) return ULT_ERROR_STATE;
    if (!ClearUltObject(pool, 256)) return ULT_ERROR_NULL;
    gResourcePools[pool] = state;
    return ULT_OK;
}

int APS5_VABI sceUltWaitingQueueResourcePoolDestroy(void* pool) {
    if (pool == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gResourcePools.find(pool);
    if (it == gResourcePools.end()) {
        return ULT_ERROR_STATE;
    }
    for (const auto& entry : gMutexes) {
        if (entry.second->_waitingPool == pool) {
            std::lock_guard<std::mutex> stateLock(entry.second->_mutex);
            if (entry.second->_waiters != 0) {
                return ULT_ERROR_STATE;
            }
        }
    }
    for (const auto& entry : gSemaphores) {
        if (entry.second->_waitingPool == pool) {
            std::lock_guard<std::mutex> stateLock(entry.second->_mutex);
            if (entry.second->_waiters != 0) {
                return ULT_ERROR_STATE;
            }
        }
    }
    for (const auto& entry : gQueues) {
        if (entry.second->_pool && entry.second->_pool->_waitingPool == pool) {
            std::lock_guard<std::mutex> poolLock(entry.second->_pool->_mutex);
            if (entry.second->_waitingPushers != 0 || entry.second->_waitingPoppers != 0) {
                return ULT_ERROR_STATE;
            }
        }
    }
    if (UltReaderWriterLocksWaitOn(pool)) {
        return ULT_ERROR_BUSY;
    }
    gResourcePools.erase(it);
    return ULT_OK;
}

std::uint64_t APS5_VABI sceUltQueueDataResourcePoolGetWorkAreaSize(std::uint32_t numData, std::uint64_t dataSize, std::uint32_t numQueueObject) {
    const auto nodeArea = 32ull * (numQueueObject + 2u * numData + 3u);
    if (dataSize != 0 && numData > (UINT64_MAX - nodeArea) / dataSize) {
        throw std::out_of_range("ULT queue work area size overflow");
    }
    return static_cast<std::uint64_t>(numData) * dataSize + nodeArea;
}

int APS5_VABI sceUltQueueDataResourcePoolCreate(void* pool, const char* name, std::uint32_t numData, std::uint64_t dataSize, std::uint32_t numQueueObject, void* waitingQueueResourcePool, void* workArea, const void* optParam, std::uint32_t buildVersion) {
    (void)optParam;
    (void)buildVersion;
    if (pool == nullptr) {
        return ULT_ERROR_NULL;
    }
    if ((reinterpret_cast<std::uintptr_t>(pool) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    if (numData == 0 || numQueueObject == 0 || dataSize == 0 ||
        dataSize > std::numeric_limits<std::size_t>::max() / numData) return ULT_ERROR_RANGE;
    std::lock_guard<std::mutex> lock(gMutex);
    if (waitingQueueResourcePool != nullptr && gResourcePools.find(waitingQueueResourcePool) == gResourcePools.end()) {
        return ULT_ERROR_INVALID;
    }
    if (gQueueDataPools.contains(pool)) return ULT_ERROR_STATE;
    auto state = std::make_shared<UltQueueDataPoolState>();
    state->_numData = numData;
    state->_dataSize = dataSize;
    state->_numQueueObject = numQueueObject;
    state->_waitingPool = waitingQueueResourcePool;
    state->_workArea = workArea;
    if (!CopyUltName(state->_name, name)) return ULT_ERROR_NULL;
    state->_data.resize(numData * dataSize);
    state->_next.resize(numData);
    for (std::uint32_t slot = 0; slot < numData; ++slot) {
        state->_next[slot] = slot + 1u < numData ? slot + 1u : UINT32_MAX;
    }
    state->_free = 0;
    if (!ClearUltObject(pool, 512)) return ULT_ERROR_NULL;
    gQueueDataPools.emplace(pool, std::move(state));
    return ULT_OK;
}

int APS5_VABI sceUltQueueDataResourcePoolDestroy(void* pool) {
    if (pool == nullptr) return ULT_ERROR_NULL;
    std::lock_guard lock(gMutex);
    const auto it = gQueueDataPools.find(pool);
    if (it == gQueueDataPools.end()) return ULT_ERROR_STATE;
    auto state = it->second;
    std::lock_guard stateLock(state->_mutex);
    if (state->_queues != 0) return ULT_ERROR_BUSY;
    gQueueDataPools.erase(it);
    return ULT_OK;
}

int APS5_VABI sceUltQueueCreate(void* queue, const char* name, std::uint64_t dataSize, void* waitingQueueResourcePool, void* queueDataResourcePool, const void* optParam, std::uint32_t buildVersion) {
    (void)optParam;
    (void)buildVersion;
    if (queue == nullptr) {
        return ULT_ERROR_NULL;
    }
    if ((reinterpret_cast<std::uintptr_t>(queue) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    if (dataSize == 0) return ULT_ERROR_RANGE;
    auto state = std::make_shared<UltQueueState>();
    state->_dataSize = dataSize;
    state->_waitingPool = waitingQueueResourcePool;
    state->_dataPool = queueDataResourcePool;
    if (!CopyUltName(state->_name, name)) return ULT_ERROR_NULL;
    std::lock_guard<std::mutex> lock(gMutex);
    auto dataPoolIt = gQueueDataPools.find(queueDataResourcePool);
    if (dataPoolIt == gQueueDataPools.end()) {
        return ULT_ERROR_INVALID;
    }
    if (waitingQueueResourcePool != nullptr && gResourcePools.find(waitingQueueResourcePool) == gResourcePools.end()) {
        return ULT_ERROR_INVALID;
    }
    if (gQueues.contains(queue)) return ULT_ERROR_STATE;
    state->_pool = dataPoolIt->second;
    auto& pool = *state->_pool;
    std::lock_guard stateLock(pool._mutex);
    if (dataSize > pool._dataSize || waitingQueueResourcePool != pool._waitingPool) return ULT_ERROR_INVALID;
    if (pool._queues == pool._numQueueObject) return ULT_ERROR_BUSY;
    if (!ClearUltObject(queue, 512)) return ULT_ERROR_NULL;
    gQueues.emplace(queue, std::move(state));
    ++pool._queues;
    return ULT_OK;
}

int APS5_VABI sceUltQueuePush(void* queue, const void* data) {
    return QueueTransfer(queue, nullptr, data, true, true);
}

int APS5_VABI sceUltQueueTryPush(void* queue, const void* data) {
    return QueueTransfer(queue, nullptr, data, true, false);
}

int APS5_VABI sceUltQueuePop(void* queue, void* data) {
    return QueueTransfer(queue, data, nullptr, false, true);
}

int APS5_VABI sceUltQueueTryPop(void* queue, void* data) {
    return QueueTransfer(queue, data, nullptr, false, false);
}

int APS5_VABI sceUltQueueDestroy(void* queue) {
    if (queue == nullptr) return ULT_ERROR_NULL;
    std::lock_guard lock(gMutex);
    const auto it = gQueues.find(queue);
    if (it == gQueues.end()) return ULT_ERROR_STATE;
    auto state = it->second;
    auto& pool = *state->_pool;
    {
        std::lock_guard stateLock(pool._mutex);
        if (state->_waitingPushers != 0 || state->_waitingPoppers != 0) return ULT_ERROR_STATE;
        ReleaseQueue(*state);
        gQueues.erase(it);
    }
    pool._spaceAvailable.notify_all();
    return ULT_OK;
}

int APS5_VABI sceUltSemaphoreCreate(void* semaphore, const char* name, std::uint32_t numInitialResource, void* waitingQueueResourcePool, const void* optParam, std::uint32_t buildVersion) {
    (void)optParam;
    (void)buildVersion;
    if (semaphore == nullptr) {
        return ULT_ERROR_NULL;
    }
    if ((reinterpret_cast<std::uintptr_t>(semaphore) & 7u) != 0) {
        return ULT_ERROR_ALIGNMENT;
    }
    auto state = std::make_shared<UltSemaphoreState>();
    state->_resources = std::bit_cast<std::int32_t>(numInitialResource);
    state->_waitingPool = waitingQueueResourcePool;
    if (!CopyUltName(state->_name, name)) return ULT_ERROR_NULL;
    std::lock_guard<std::mutex> lock(gMutex);
    if (waitingQueueResourcePool != nullptr && gResourcePools.find(waitingQueueResourcePool) == gResourcePools.end()) {
        return ULT_ERROR_INVALID;
    }
    if (gSemaphores.find(semaphore) != gSemaphores.end()) {
        return ULT_ERROR_STATE;
    }
    if (!ClearUltObject(semaphore, 256)) return ULT_ERROR_NULL;
    gSemaphores[semaphore] = std::move(state);
    return ULT_OK;
}

int APS5_VABI sceUltSemaphoreAcquire(void* semaphore, std::int32_t numResource) {
    std::shared_ptr<UltSemaphoreState> state;
    if (int ret = semaphoreRequest(semaphore, numResource, &state); ret != ULT_OK) {
        return ret;
    }
    std::unique_lock<std::mutex> lock(state->_mutex);
    if (!state->_alive) {
        return ULT_ERROR_STATE;
    }
    UltWaitRecord record;
    const bool tracked = CurrentWaitRecord(record, 0);
    if (tracked) state->_waiterRecords.push_back(record);
    ++state->_waiters;
    try {
        state->_available.wait(lock, [&] { return !state->_alive || state->_resources >= numResource; });
    } catch (...) {
        --state->_waiters;
        if (tracked) RemoveWaitRecord(state->_waiterRecords, record);
        throw;
    }
    --state->_waiters;
    if (tracked) RemoveWaitRecord(state->_waiterRecords, record);
    if (!state->_alive) {
        return ULT_ERROR_STATE;
    }
    state->_resources -= numResource;
    return ULT_OK;
}

int APS5_VABI sceUltSemaphoreTryAcquire(void* semaphore, std::int32_t numResource) {
    std::shared_ptr<UltSemaphoreState> state;
    if (int ret = semaphoreRequest(semaphore, numResource, &state); ret != ULT_OK) {
        return ret;
    }
    std::lock_guard<std::mutex> lock(state->_mutex);
    if (!state->_alive) {
        return ULT_ERROR_STATE;
    }
    if (state->_resources < numResource) {
        return ULT_ERROR_AGAIN;
    }
    state->_resources -= numResource;
    return ULT_OK;
}

int APS5_VABI sceUltSemaphoreRelease(void* semaphore, std::int32_t numResource) {
    std::shared_ptr<UltSemaphoreState> state;
    if (int ret = semaphoreRequest(semaphore, numResource, &state); ret != ULT_OK) {
        return ret;
    }
    {
        std::lock_guard<std::mutex> lock(state->_mutex);
        if (!state->_alive) {
            return ULT_ERROR_STATE;
        }
        if (state->_resources > INT32_MAX - numResource) {
            return ULT_ERROR_RANGE;
        }
        state->_resources += numResource;
    }
    state->_available.notify_all();
    return ULT_OK;
}

int APS5_VABI sceUltSemaphoreDestroy(void* semaphore) {
    if (semaphore == nullptr) return ULT_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(semaphore) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    std::lock_guard<std::mutex> globalLock(gMutex);
    auto it = gSemaphores.find(semaphore);
    if (it == gSemaphores.end()) {
        return ULT_ERROR_STATE;
    }
    auto state = it->second;
    std::lock_guard<std::mutex> stateLock(state->_mutex);
    if (state->_waiters != 0) {
        return ULT_ERROR_STATE;
    }
    state->_alive = false;
    gSemaphores.erase(it);
    return ULT_OK;
}

int APS5_VABI sceUltUlthreadRuntimeOptParamInitialize(UltUlthreadRuntimeOptParam* optParam, std::uint32_t buildVersion) {
    if (optParam == nullptr) {
        return ULT_ERROR_NULL;
    }
    const std::uint32_t defaults[] = {0x6C696275, 0x6C746D61, 0x67696300, buildVersion, 0x4000, 0, 0x1FFF, 0, 0x2BC, 4};
    UltUlthreadRuntimeOptParam local{};
    std::memcpy(local.bytes, defaults, sizeof(defaults));
    if (!GuestCopyOut_nid_no_patch(optParam, &local, sizeof(local))) return ULT_ERROR_NULL;
    return ULT_OK;
}

std::uint64_t APS5_VABI sceUltUlthreadRuntimeGetWorkAreaSize(std::uint32_t maxNumUlthread, std::uint32_t numWorkerThread) {
    const std::uint32_t totalThreads = maxNumUlthread + numWorkerThread;
    const auto threadNodes = 32ull * (2u * maxNumUlthread + 4u);
    const auto threadArea = (threadNodes + 4ull * maxNumUlthread + 7u) & 0x3FFFFFFFF8ull;
    const auto schedulerArea = 32ull * (2u * numWorkerThread + 4u) + 32ull * (2u * totalThreads + 4u);
    return threadArea + schedulerArea + 512ull * numWorkerThread + 8ull * (static_cast<std::uint64_t>(maxNumUlthread) + totalThreads);
}

int APS5_VABI sceUltUlthreadRuntimeCreate(void* runtime, const char* name, std::uint32_t maxNumUlthread, std::uint32_t numWorkerThread, void* workArea, const void* optParam, std::uint32_t buildVersion) {
    (void)buildVersion;
    if (runtime == nullptr) {
        return ULT_ERROR_NULL;
    }
    if (optParam != nullptr) {
        std::uint32_t header[3];
        if (!GuestCopyIn_nid_no_patch(header, optParam, sizeof(header))) return ULT_ERROR_NULL;
        if (header[0] != 0x6C696275 || header[1] != 0x6C746D61 || header[2] != 0x67696300) return ULT_ERROR_INVALID;
    }
    UltRuntimeState state{maxNumUlthread, numWorkerThread, workArea};
    if (!CopyUltName(state._name, name)) return ULT_ERROR_NULL;
    std::lock_guard<std::mutex> lock(gMutex);
    if (gRuntimes.contains(runtime)) return ULT_ERROR_STATE;
    if (!ClearUltObject(runtime, 4096)) return ULT_ERROR_NULL;
    gRuntimes[runtime] = state;
    return ULT_OK;
}

}
