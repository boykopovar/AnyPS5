#include <algorithm>
#include <atomic>
#include <bit>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "SceUltTypes.hpp"
#include "prx/libSceUlt/src/ReaderWriterLock.hpp"

extern "C" int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
extern "C" int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
extern "C" void APS5_VABI scePthreadExit(void* retval);
extern "C" void APS5_VABI scePthreadYield();

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
    std::memcpy(&value, optParam, sizeof(value));
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
        ++state->_waiters;
        try {
            (push ? pool._spaceAvailable : state->_dataAvailable).wait(lock, ready);
        } catch (...) {
            --state->_waiters;
            throw;
        }
        --state->_waiters;
        if (!state->_alive) return ULT_ERROR_STATE;
    }
    if (push) {
        const auto slot = pool._free;
        std::memcpy(pool._data.data() + slot * pool._dataSize, source, state->_dataSize);
        pool._free = pool._next[slot];
        pool._next[slot] = UINT32_MAX;
        if (state->_tail == UINT32_MAX) state->_head = slot;
        else pool._next[state->_tail] = slot;
        state->_tail = slot;
    } else {
        const auto slot = state->_head;
        std::memcpy(data, pool._data.data() + slot * pool._dataSize, state->_dataSize);
        state->_head = pool._next[slot];
        if (state->_head == UINT32_MAX) state->_tail = UINT32_MAX;
        pool._next[slot] = pool._free;
        pool._free = slot;
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

}

bool UltWaitingPoolExists(void* pool) {
    std::lock_guard<std::mutex> lock(gMutex);
    return gResourcePools.contains(pool);
}

extern "C" {

int APS5_VABI sceUltInitialize() {
    return ULT_OK;
}

int APS5_VABI sceUltFinalize() {
    std::vector<std::shared_ptr<UltSemaphoreState>> semaphores;
    std::vector<std::shared_ptr<UltMutexState>> mutexes;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        if (!gUlthreads.empty()) {
            return ULT_ERROR_BUSY;
        }
        semaphores.reserve(gSemaphores.size());
        for (auto& entry : gSemaphores) {
            auto& state = entry.second;
            std::lock_guard<std::mutex> stateLock(state->_mutex);
            state->_alive = false;
            semaphores.push_back(state);
        }
        gSemaphores.clear();
        mutexes.reserve(gMutexes.size());
        for (auto& entry : gMutexes) {
            auto& state = entry.second;
            std::lock_guard<std::mutex> stateLock(state->_mutex);
            state->_alive = false;
            mutexes.push_back(state);
        }
        for (const auto& [object, condition] : gConditions) {
            std::lock_guard stateLock(condition->_mutex->_mutex);
            condition->_alive = false;
            --condition->_mutex->_conditions;
            for (auto* waiter : condition->_queue) waiter->_available.notify_one();
        }
        gConditions.clear();
        gMutexes.clear();
        UltFinalizeReaderWriterLocks();
        gResourcePools.clear();
        for (auto& entry : gQueues) {
            auto& state = *entry.second;
            std::lock_guard stateLock(state._pool->_mutex);
            ReleaseQueue(state);
            state._dataAvailable.notify_all();
            state._pool->_spaceAvailable.notify_all();
        }
        gQueues.clear();
        gQueueDataPools.clear();
        gRuntimes.clear();
    }
    for (const auto& state : semaphores) {
        state->_available.notify_all();
    }
    for (const auto& state : mutexes) {
        state->_available.notify_all();
    }
    return ULT_OK;
}

int APS5_VABI sceUltMutexOptParamInitialize(UltMutexOptParam* optParam, std::uint32_t buildVersion) {
    if (optParam == nullptr) {
        return ULT_ERROR_NULL;
    }
    if ((reinterpret_cast<std::uintptr_t>(optParam) & 7u) != 0) {
        return ULT_ERROR_ALIGNMENT;
    }
    std::memset(optParam, 0, sizeof(*optParam));
    optParam->header[0] = 0x6C696275;
    optParam->header[1] = 0x6C746D61;
    optParam->header[2] = 0x67696300;
    optParam->buildVersion = buildVersion;
    return ULT_OK;
}

int APS5_VABI sceUltSemaphoreOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    (void)buildVersion;
    return InitializeOptParam(optParam);
}

int APS5_VABI sceUltQueueOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    (void)buildVersion;
    return InitializeOptParam(optParam);
}

int APS5_VABI sceUltQueueDataResourcePoolOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    (void)buildVersion;
    return InitializeOptParam(optParam);
}

int APS5_VABI sceUltWaitingQueueResourcePoolOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    (void)buildVersion;
    return InitializeOptParam(optParam);
}

int APS5_VABI sceUltUlthreadOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    (void)buildVersion;
    return InitializeOptParam(optParam);
}

int APS5_VABI sceUltConditionVariableOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    (void)buildVersion;
    return InitializeOptParam(optParam);
}

int APS5_VABI _sceUltConditionVariableOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    return sceUltConditionVariableOptParamInitialize(optParam, buildVersion);
}

int APS5_VABI sceUltMutexCreate(void* mutex, const char* name, void* waitingQueueResourcePool, const UltMutexOptParam* optParam, std::uint32_t buildVersion) {
    (void)name;
    (void)buildVersion;
    if (mutex == nullptr) {
        return ULT_ERROR_NULL;
    }
    auto state = std::make_shared<UltMutexState>();
    state->_attribute = optParam != nullptr ? optParam->attribute : 0;
    state->_waitingPool = waitingQueueResourcePool;
    std::lock_guard<std::mutex> lock(gMutex);
    if (waitingQueueResourcePool != nullptr && gResourcePools.find(waitingQueueResourcePool) == gResourcePools.end()) {
        return ULT_ERROR_INVALID;
    }
    if (gMutexes.find(mutex) != gMutexes.end()) {
        return ULT_ERROR_STATE;
    }
    std::memset(mutex, 0, 256);
    gMutexes[mutex] = std::move(state);
    return ULT_OK;
}

int APS5_VABI sceUltMutexDestroy(void* mutex) {
    if (mutex == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gMutexes.find(mutex);
    if (it == gMutexes.end()) {
        return ULT_ERROR_STATE;
    }
    auto state = it->second;
    std::lock_guard<std::mutex> stateLock(state->_mutex);
    if (state->_lockCount != 0 || state->_waiters != 0 || state->_conditions != 0 || state->_conditionWaiters != 0) {
        return ULT_ERROR_STATE;
    }
    state->_alive = false;
    gMutexes.erase(it);
    return ULT_OK;
}

int APS5_VABI sceUltMutexLock(void* mutex) {
    std::shared_ptr<UltMutexState> state;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gMutexes.find(mutex);
        if (it == gMutexes.end()) {
            return mutex == nullptr ? ULT_ERROR_NULL : ULT_ERROR_STATE;
        }
        state = it->second;
    }
    const auto self = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock(state->_mutex);
    if (!state->_alive) {
        return ULT_ERROR_STATE;
    }
    if (state->_owner == self) {
        if (state->_attribute == 0) {
            return ULT_ERROR_STATE;
        }
        ++state->_lockCount;
        return ULT_OK;
    }
    ++state->_waiters;
    try {
        state->_available.wait(lock, [&] {
            return !state->_alive || state->_lockCount == 0;
        });
    } catch (...) {
        --state->_waiters;
        throw;
    }
    --state->_waiters;
    if (!state->_alive) {
        return ULT_ERROR_STATE;
    }
    state->_owner = self;
    state->_lockCount = 1;
    return ULT_OK;
}

int APS5_VABI sceUltMutexTryLock(void* mutex) {
    std::shared_ptr<UltMutexState> state;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gMutexes.find(mutex);
        if (it == gMutexes.end()) {
            return mutex == nullptr ? ULT_ERROR_NULL : ULT_ERROR_STATE;
        }
        state = it->second;
    }
    const auto self = std::this_thread::get_id();
    std::lock_guard<std::mutex> lock(state->_mutex);
    if (!state->_alive) {
        return ULT_ERROR_STATE;
    }
    if (state->_owner == self) {
        ++state->_lockCount;
        return ULT_OK;
    }
    if (state->_lockCount != 0) {
        return ULT_ERROR_BUSY;
    }
    state->_owner = self;
    state->_lockCount = 1;
    return ULT_OK;
}

int APS5_VABI sceUltMutexUnlock(void* mutex) {
    std::shared_ptr<UltMutexState> state;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gMutexes.find(mutex);
        if (it == gMutexes.end()) {
            return mutex == nullptr ? ULT_ERROR_NULL : ULT_ERROR_STATE;
        }
        state = it->second;
    }
    const auto self = std::this_thread::get_id();
    std::lock_guard<std::mutex> lock(state->_mutex);
    if (!state->_alive) {
        return ULT_ERROR_STATE;
    }
    if (state->_owner != self) {
        return ULT_ERROR_PERMISSION;
    }
    --state->_lockCount;
    if (state->_lockCount == 0) {
        state->_owner = {};
        state->_available.notify_one();
    }
    return ULT_OK;
}

int APS5_VABI sceUltConditionVariableCreate(void* condition, const char* name, void* mutex, const void* optParam, std::uint32_t buildVersion) {
    (void)name;
    (void)buildVersion;
    if (condition == nullptr || mutex == nullptr) return ULT_ERROR_NULL;
    if (optParam != nullptr) throw std::runtime_error("ULT condition variable options are not implemented");
    std::lock_guard lock(gMutex);
    const auto mutexIt = gMutexes.find(mutex);
    if (mutexIt == gMutexes.end()) return ULT_ERROR_INVALID;
    if (gConditions.contains(condition)) return ULT_ERROR_STATE;
    auto state = std::make_shared<UltConditionState>();
    state->_mutex = mutexIt->second;
    std::lock_guard stateLock(state->_mutex->_mutex);
    if (!state->_mutex->_alive) return ULT_ERROR_STATE;
    gConditions.emplace(condition, state);
    ++state->_mutex->_conditions;
    return ULT_OK;
}

int APS5_VABI _sceUltConditionVariableCreate(void* condition, const char* name, void* mutex, const void* optParam, std::uint32_t buildVersion) {
    return sceUltConditionVariableCreate(condition, name, mutex, optParam, buildVersion);
}

int APS5_VABI sceUltConditionVariableDestroy(void* condition) {
    std::lock_guard lock(gMutex);
    const auto it = gConditions.find(condition);
    if (it == gConditions.end()) return condition == nullptr ? ULT_ERROR_NULL : ULT_ERROR_STATE;
    auto state = it->second;
    std::lock_guard stateLock(state->_mutex->_mutex);
    if (state->_waiters != 0) return ULT_ERROR_BUSY;
    state->_alive = false;
    --state->_mutex->_conditions;
    gConditions.erase(it);
    return ULT_OK;
}

int APS5_VABI sceUltConditionVariableSignal(void* condition) {
    std::shared_ptr<UltConditionState> state;
    {
        std::lock_guard lock(gMutex);
        const auto it = gConditions.find(condition);
        if (it == gConditions.end()) return condition == nullptr ? ULT_ERROR_NULL : ULT_ERROR_STATE;
        state = it->second;
    }
    std::lock_guard stateLock(state->_mutex->_mutex);
    if (!state->_alive || !state->_mutex->_alive) return ULT_ERROR_STATE;
    if (!state->_queue.empty()) {
        auto* waiter = state->_queue.front();
        state->_queue.pop_front();
        waiter->_signaled = true;
        waiter->_available.notify_one();
    }
    return ULT_OK;
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
    UltConditionWaiter waiter;
    state->_queue.push_back(&waiter);
    ++state->_waiters;
    ++held->_conditionWaiters;
    held->_lockCount = 0;
    held->_owner = {};
    held->_available.notify_one();
    try {
        waiter._available.wait(lock, [&] { return waiter._signaled || !state->_alive || !held->_alive; });
        if (held->_alive && state->_alive) {
            ++held->_waiters;
            try {
                held->_available.wait(lock, [&] { return !held->_alive || held->_lockCount == 0; });
            } catch (...) {
                --held->_waiters;
                throw;
            }
            --held->_waiters;
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
    held->_lockCount = 1;
    return ULT_OK;
}

std::uint64_t APS5_VABI sceUltWaitingQueueResourcePoolGetWorkAreaSize(std::uint32_t numThreads, std::uint32_t numSyncObjects) {
    return 32ull * (numSyncObjects + 2u * numThreads + 2u);
}

int APS5_VABI sceUltWaitingQueueResourcePoolCreate(void* pool, const char* name, std::uint32_t numThreads, std::uint32_t numSyncObjects, void* workArea, const void* optParam, std::uint32_t buildVersion) {
    (void)name;
    (void)optParam;
    (void)buildVersion;
    if (pool == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::memset(pool, 0, 256);
    std::lock_guard<std::mutex> lock(gMutex);
    gResourcePools[pool] = {numThreads, numSyncObjects, workArea};
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
            if (entry.second->_waiters != 0) {
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
    (void)name;
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
    state->_data.resize(numData * dataSize);
    state->_next.resize(numData);
    for (std::uint32_t slot = 0; slot < numData; ++slot) {
        state->_next[slot] = slot + 1u < numData ? slot + 1u : UINT32_MAX;
    }
    state->_free = 0;
    gQueueDataPools.emplace(pool, std::move(state));
    std::memset(pool, 0, 512);
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
    (void)name;
    (void)optParam;
    (void)buildVersion;
    if (queue == nullptr) {
        return ULT_ERROR_NULL;
    }
    if ((reinterpret_cast<std::uintptr_t>(queue) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    if (dataSize == 0) return ULT_ERROR_RANGE;
    auto state = std::make_shared<UltQueueState>();
    state->_dataSize = dataSize;
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
    gQueues.emplace(queue, std::move(state));
    ++pool._queues;
    std::memset(queue, 0, 512);
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
        if (state->_waiters != 0) return ULT_ERROR_STATE;
        ReleaseQueue(*state);
        gQueues.erase(it);
    }
    pool._spaceAvailable.notify_all();
    return ULT_OK;
}

int APS5_VABI sceUltSemaphoreCreate(void* semaphore, const char* name, std::uint32_t numInitialResource, void* waitingQueueResourcePool, const void* optParam, std::uint32_t buildVersion) {
    (void)name;
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
    std::lock_guard<std::mutex> lock(gMutex);
    if (waitingQueueResourcePool != nullptr && gResourcePools.find(waitingQueueResourcePool) == gResourcePools.end()) {
        return ULT_ERROR_INVALID;
    }
    if (gSemaphores.find(semaphore) != gSemaphores.end()) {
        return ULT_ERROR_STATE;
    }
    std::memset(semaphore, 0, 256);
    gSemaphores[semaphore] = std::move(state);
    return ULT_OK;
}

int APS5_VABI sceUltSemaphoreAcquire(void* semaphore, std::int32_t numResource) {
    if (semaphore == nullptr) return ULT_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(semaphore) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    if (numResource <= 0) {
        return ULT_ERROR_RANGE;
    }
    std::shared_ptr<UltSemaphoreState> state;
    if (int ret = semaphoreGetState(semaphore, &state); ret != ULT_OK) {
        return ret;
    }
    std::unique_lock<std::mutex> lock(state->_mutex);
    if (!state->_alive) {
        return ULT_ERROR_STATE;
    }
    state->_waiters++;
    state->_available.wait(lock, [&] { return !state->_alive || state->_resources >= numResource; });
    state->_waiters--;
    if (!state->_alive) {
        return ULT_ERROR_STATE;
    }
    state->_resources -= numResource;
    return ULT_OK;
}

int APS5_VABI sceUltSemaphoreTryAcquire(void* semaphore, std::int32_t numResource) {
    if (semaphore == nullptr) return ULT_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(semaphore) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    if (numResource <= 0) {
        return ULT_ERROR_RANGE;
    }
    std::shared_ptr<UltSemaphoreState> state;
    if (int ret = semaphoreGetState(semaphore, &state); ret != ULT_OK) {
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
    if (semaphore == nullptr) return ULT_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(semaphore) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    if (numResource <= 0) {
        return ULT_ERROR_RANGE;
    }
    std::shared_ptr<UltSemaphoreState> state;
    if (int ret = semaphoreGetState(semaphore, &state); ret != ULT_OK) {
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
    (void)buildVersion;
    if (optParam == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::memset(optParam, 0, sizeof(*optParam));
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
    (void)name;
    (void)optParam;
    (void)buildVersion;
    if (runtime == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::memset(runtime, 0, 4096);
    std::lock_guard<std::mutex> lock(gMutex);
    gRuntimes[runtime] = {maxNumUlthread, numWorkerThread, workArea};
    return ULT_OK;
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
    {
        std::lock_guard<std::mutex> lock(gMutex);
        if (gRuntimes.find(runtime) == gRuntimes.end()) {
            return ULT_ERROR_STATE;
        }
        if (gUlthreads.find(ulthread) != gUlthreads.end()) {
            return ULT_ERROR_STATE;
        }
        std::memset(ulthread, 0, 512);
        gUlthreads[ulthread] = state;
    }
    const int result = scePthreadCreate(&state->_thread, nullptr, ulthreadRunner, state.get(), name != nullptr ? name : "");
    if (result != 0) {
        std::lock_guard<std::mutex> lock(gMutex);
        gUlthreads.erase(ulthread);
        return ULT_ERROR_AGAIN;
    }
    return ULT_OK;
}

int APS5_VABI sceUltUlthreadJoin(void* ulthread, std::int32_t* status) {
    if (ulthread == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::shared_ptr<UltUlthreadState> state;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gUlthreads.find(ulthread);
        if (it == gUlthreads.end()) {
            return ULT_ERROR_STATE;
        }
        state = it->second;
    }
    void* result = nullptr;
    if (scePthreadJoin(state->_thread, &result) != 0) {
        return ULT_ERROR_STATE;
    }
    if (status != nullptr) {
        *status = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(result));
    }
    std::lock_guard<std::mutex> lock(gMutex);
    gUlthreads.erase(ulthread);
    return ULT_OK;
}

int APS5_VABI sceUltUlthreadTryJoin(void* ulthread, std::int32_t* status) {
    if (ulthread == nullptr) {
        return ULT_ERROR_NULL;
    }
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gUlthreads.find(ulthread);
        if (it == gUlthreads.end()) {
            return ULT_ERROR_STATE;
        }
        if (!it->second->_exited.load(std::memory_order_acquire)) {
            return ULT_ERROR_AGAIN;
        }
    }
    return sceUltUlthreadJoin(ulthread, status);
}

int APS5_VABI sceUltUlthreadGetSelf(void** ulthread) {
    if (ulthread == nullptr) {
        return ULT_ERROR_NULL;
    }
    if (gCurrentUlthread == nullptr) {
        return ULT_ERROR_STATE;
    }
    *ulthread = gCurrentUlthread->_object;
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
    if (optParam != nullptr && (optParam->header[0] != 0x6C696275 || optParam->header[1] != 0x6C746D61 || optParam->header[2] != 0x67696300 || optParam->attribute > 1 || std::any_of(std::begin(optParam->reserved), std::end(optParam->reserved), [](std::uint32_t value) { return value != 0; }))) {
        return ULT_ERROR_INVALID;
    }
    return sceUltMutexCreate(mutex, name, waitingQueueResourcePool, optParam, 0);
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

int APS5_VABI _sceUltUlthreadCreate(void* ulthread, const char* name, UltUlthreadEntry entry, std::uint64_t arg, void* context, std::uint64_t sizeContext, void* runtime, const void* optParam) {
    return sceUltUlthreadCreate(ulthread, name, entry, arg, context, sizeContext, runtime, optParam, 0);
}

int APS5_VABI sceUltConditionVariableSignalAll(void* conditionVariable) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltGetConditionVariableInfo(void* object, UltConditionVariableInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltGetMutexInfo(void* object, UltMutexInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltGetQueueDataResourcePoolInfo(void* object, UltQueueDataResourcePoolInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltGetQueueInfo(void* object, UltQueueInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltGetReaderWriterLockInfo(void* object, UltReaderWriterLockInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltGetSemaphoreInfo(void* object, UltSemaphoreInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltGetUlthreadInfo(void* object, UltUlthreadInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltGetUlthreadRuntimeInfo(void* runtime, UltUlthreadRuntimeInfo* info, void** ulthreads, std::uint32_t ulthreadCapacity, Pthread* workerThreads, std::uint32_t workerCapacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltGetWaitingQueueResourcePoolInfo(void* object, UltWaitingQueueResourcePoolInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltAbstractThreadFactoryCreateThread() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltAbstractThreadFactoryGetNumMaxThreads() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltAbstractThreadFactoryJoinThread() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltKernelThreadFactoryFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltKernelThreadFactoryGetFactoryPointer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltKernelThreadFactoryInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltModuleFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltModuleInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltUlthreadFactoryFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltUlthreadFactoryGetFactoryPointer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltUlthreadFactoryGetWorkAreaSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUltUlthreadFactoryInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}


}
