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

int APS5_VABI sceUltGetUlthreadRuntimeInfo(void* runtime, UltUlthreadRuntimeInfo* info, void** ulthreads, std::uint32_t ulthreadCapacity, Pthread* workerThreads, std::uint32_t workerCapacity) {
    if (runtime == nullptr || info == nullptr) return ULT_ERROR_NULL;
    if (ulthreads == nullptr && ulthreadCapacity != 0) return ULT_ERROR_NULL;
    if (workerThreads == nullptr && workerCapacity != 0) return ULT_ERROR_NULL;
    if (workerThreads != nullptr && workerCapacity != 0) throw std::runtime_error("ULT ulthread runtime worker thread handles are not implemented");
    std::lock_guard lock(gMutex);
    const auto it = gRuntimes.find(runtime);
    if (it == gRuntimes.end()) return ULT_ERROR_STATE;
    const auto& state = it->second;
    UltUlthreadRuntimeInfo localInfo{};
    localInfo.size = sizeof(UltUlthreadRuntimeInfo);
    localInfo.object = runtime;
    std::memcpy(localInfo.name, state._name, sizeof(localInfo.name));
    localInfo.maxUlthreads = state._maxNumUlthread;
    localInfo.workerThreads = state._numWorkerThread;
    std::uint32_t count = 0;
    for (const auto& [obj, ulthread] : gUlthreads) {
        if (ulthread->_runtime == runtime && !ulthread->_exited.load(std::memory_order_acquire)) {
            if (ulthreads != nullptr && count < ulthreadCapacity && !GuestCopyOut_nid_no_patch(&ulthreads[count], &obj, sizeof(obj))) return ULT_ERROR_NULL;
            ++count;
        }
    }
    localInfo.activeUlthreads = count;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo));
    errno = savedErrno;
    if (!copied) return ULT_ERROR_NULL;
    return ULT_OK;
}

int APS5_VABI sceUltGetWaitingQueueResourcePoolInfo(void* object, UltWaitingQueueResourcePoolInfo* info) {
    if (object == nullptr || info == nullptr) return ULT_ERROR_NULL;
    std::lock_guard lock(gMutex);
    const auto it = gResourcePools.find(object);
    if (it == gResourcePools.end()) return ULT_ERROR_STATE;
    const auto& state = it->second;
    UltWaitingQueueResourcePoolInfo localInfo{};
    localInfo.size = sizeof(UltWaitingQueueResourcePoolInfo);
    localInfo.object = object;
    std::memcpy(localInfo.name, state._name, sizeof(localInfo.name));
    localInfo.maxThreads = state._numThreads;
    localInfo.availableThreads = state._numThreads;
    localInfo.maxSyncObjects = state._numSyncObjects;
    localInfo.availableSyncObjects = state._numSyncObjects;
    if (!GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo))) return ULT_ERROR_NULL;
    return ULT_OK;
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
