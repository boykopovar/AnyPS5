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

UltThreadIdentity UltCurrentThreadIdentity_nid_no_patch() {
    if (gCurrentUlthread != nullptr) return {};
    const auto self = scePthreadSelf();
    std::uintptr_t thread;
    std::memcpy(&thread, self, sizeof(thread));
    return {reinterpret_cast<void*>(thread), 1};
}

UltThreadIdentity UltCurrentOwnerIdentity_nid_no_patch() {
    if (gCurrentUlthread == nullptr) return UltCurrentThreadIdentity_nid_no_patch();
    void* name = nullptr;
    std::memcpy(&name, gCurrentUlthread->_name, sizeof(name));
    return {name, 2};
}

bool CurrentWaitRecord(UltWaitRecord& record, std::int32_t waitValue) {
    const UltThreadIdentity identity = UltCurrentThreadIdentity_nid_no_patch();
    if (identity.thread == nullptr) return false;
    record = {identity.thread, identity.type, waitValue};
    return true;
}

void RemoveWaitRecord(std::vector<UltWaitRecord>& records, const UltWaitRecord& record) {
    const auto found = std::find_if(records.begin(), records.end(), [&](const auto& entry) {
        return entry.thread == record.thread && entry.type == record.type;
    });
    if (found != records.end()) records.erase(found);
}

int ValidateWaitRecords(const UltWaitingThreadInfo* records, std::uint32_t capacity) {
    for (std::uint32_t index = 0; index < capacity; ++index) {
        std::uint64_t size = 0;
        if (!GuestCopyIn_nid_no_patch(&size, &records[index].size, sizeof(size))) return ULT_ERROR_NULL;
        if (size != sizeof(UltWaitingThreadInfo)) return ULT_ERROR_INVALID;
    }
    return ULT_OK;
}

int WriteWaitRecords(const std::vector<UltWaitRecord>& records, UltWaitingThreadInfo* destination, std::uint32_t capacity) {
    const std::size_t count = std::min<std::size_t>(capacity, records.size());
    if (count == 0) return ULT_OK;
    std::vector<UltWaitingThreadInfo> infos(count);
    for (std::size_t index = 0; index < count; ++index) {
        infos[index].size = sizeof(UltWaitingThreadInfo);
        infos[index].thread = records[index].thread;
        infos[index].threadType = records[index].type;
        infos[index].waitValue = records[index].waitValue;
    }
    if (!GuestCopyOut_nid_no_patch(destination, infos.data(), count * sizeof(UltWaitingThreadInfo))) return ULT_ERROR_NULL;
    return ULT_OK;
}

bool UltWaitingPoolExists(void* pool) {
    std::lock_guard<std::mutex> lock(gMutex);
    return gResourcePools.contains(pool);
}


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
        mutexes.reserve(gMutexes.size());
        for (auto& entry : gSemaphores) {
            auto& state = entry.second;
            std::lock_guard<std::mutex> stateLock(state->_mutex);
            state->_alive = false;
            semaphores.push_back(state);
        }
        gSemaphores.clear();
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
    UltMutexOptParam local{};
    local.header[0] = 0x6C696275;
    local.header[1] = 0x6C746D61;
    local.header[2] = 0x67696300;
    local.buildVersion = buildVersion;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(optParam, &local, sizeof(local));
    errno = savedErrno;
    if (!copied) return ULT_ERROR_NULL;
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
    (void)buildVersion;
    if (mutex == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::uint32_t attribute = 0;
    if (optParam != nullptr && !GuestCopyIn_nid_no_patch(&attribute, &optParam->attribute, sizeof(attribute))) return ULT_ERROR_NULL;
    auto state = std::make_shared<UltMutexState>();
    state->_attribute = attribute;
    state->_waitingPool = waitingQueueResourcePool;
    if (!CopyUltName(state->_name, name)) return ULT_ERROR_NULL;
    std::lock_guard<std::mutex> lock(gMutex);
    if (waitingQueueResourcePool != nullptr && gResourcePools.find(waitingQueueResourcePool) == gResourcePools.end()) {
        return ULT_ERROR_INVALID;
    }
    if (gMutexes.find(mutex) != gMutexes.end()) {
        return ULT_ERROR_STATE;
    }
    if (!ClearUltObject(mutex, 256)) return ULT_ERROR_NULL;
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
        if ((state->_attribute & 1) == 0) {
            return ULT_ERROR_STATE;
        }
        ++state->_lockCount;
        return ULT_OK;
    }
    UltWaitRecord record;
    const bool tracked = CurrentWaitRecord(record, 0);
    if (tracked) state->_waiterRecords.push_back(record);
    ++state->_waiters;
    try {
        state->_available.wait(lock, [&] {
            return !state->_alive || state->_lockCount == 0;
        });
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
    state->_ownerIdentity = UltCurrentOwnerIdentity_nid_no_patch();
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
        if ((state->_attribute & 1) == 0) {
            return ULT_ERROR_STATE;
        }
        ++state->_lockCount;
        return ULT_OK;
    }
    if (state->_lockCount != 0) {
        return ULT_ERROR_BUSY;
    }
    state->_ownerIdentity = UltCurrentOwnerIdentity_nid_no_patch();
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
        state->_ownerIdentity = {};
        state->_available.notify_one();
    }
    return ULT_OK;
}

int APS5_VABI sceUltConditionVariableCreate(void* condition, const char* name, void* mutex, const void* optParam, std::uint32_t buildVersion) {
    (void)buildVersion;
    if (condition == nullptr || mutex == nullptr) return ULT_ERROR_NULL;
    if (optParam != nullptr) throw std::runtime_error("ULT condition variable options are not implemented");
    std::lock_guard lock(gMutex);
    const auto mutexIt = gMutexes.find(mutex);
    if (mutexIt == gMutexes.end()) return ULT_ERROR_INVALID;
    if (gConditions.contains(condition)) return ULT_ERROR_STATE;
    auto state = std::make_shared<UltConditionState>();
    state->_mutex = mutexIt->second;
    state->_mutexObject = mutex;
    if (!CopyUltName(state->_name, name)) return ULT_ERROR_NULL;
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
    if (condition == nullptr) return ULT_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(condition) & 7u) != 0) return ULT_ERROR_ALIGNMENT;
    std::shared_ptr<UltConditionState> state;
    {
        std::lock_guard lock(gMutex);
        const auto it = gConditions.find(condition);
        if (it == gConditions.end()) return ULT_ERROR_STATE;
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

int APS5_VABI sceUltConditionVariableSignalAll(void* condition) {
    std::shared_ptr<UltConditionState> state;
    {
        std::lock_guard lock(gMutex);
        const auto it = gConditions.find(condition);
        if (it == gConditions.end()) return condition == nullptr ? ULT_ERROR_NULL : ULT_ERROR_STATE;
        state = it->second;
    }
    std::lock_guard stateLock(state->_mutex->_mutex);
    if (!state->_alive || !state->_mutex->_alive) return ULT_ERROR_STATE;
    for (auto* waiter : state->_queue) {
        waiter->_signaled = true;
        waiter->_available.notify_one();
    }
    state->_queue.clear();
    return ULT_OK;
}

}
