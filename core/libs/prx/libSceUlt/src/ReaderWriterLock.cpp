#include "prx/libSceUlt/src/ReaderWriterLock.hpp"
#include "prx/libSceUlt/SceUltTypes.hpp"
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace {

struct UltReaderWriterLockState {
    std::mutex _mutex;
    std::condition_variable _released;
    std::uint32_t _readers = 0;
    bool _writing = false;
    std::thread::id _writer{};
    std::uint32_t _waiters = 0;
    bool _alive = true;
    void* _waitingPool = nullptr;
    char _name[32]{};
};

std::mutex gLocksMutex;
std::unordered_map<void*, std::shared_ptr<UltReaderWriterLockState>> gLocks;

int FindLock(void* rwlock, std::shared_ptr<UltReaderWriterLockState>& state) {
    if (rwlock == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::lock_guard<std::mutex> lock(gLocksMutex);
    const auto it = gLocks.find(rwlock);
    if (it == gLocks.end()) {
        return ULT_ERROR_STATE;
    }
    state = it->second;
    return ULT_OK;
}

bool HeldByCaller(const UltReaderWriterLockState& state) {
    return state._writing && state._writer == std::this_thread::get_id();
}

int Wait(UltReaderWriterLockState& state, std::unique_lock<std::mutex>& lock, bool writer) {
    ++state._waiters;
    state._released.wait(lock, [&] { return !state._alive || (!state._writing && (!writer || state._readers == 0)); });
    --state._waiters;
    return state._alive ? ULT_OK : ULT_ERROR_STATE;
}

}

bool UltReaderWriterLocksWaitOn(void* pool) {
    std::lock_guard<std::mutex> lock(gLocksMutex);
    for (const auto& entry : gLocks) {
        if (entry.second->_waitingPool != pool) continue;
        std::lock_guard<std::mutex> stateLock(entry.second->_mutex);
        if (entry.second->_waiters != 0) return true;
    }
    return false;
}

void UltFinalizeReaderWriterLocks() {
    std::lock_guard<std::mutex> lock(gLocksMutex);
    for (const auto& entry : gLocks) {
        std::lock_guard<std::mutex> stateLock(entry.second->_mutex);
        entry.second->_alive = false;
        entry.second->_released.notify_all();
    }
    gLocks.clear();
}

extern "C" {

int APS5_VABI sceUltReaderWriterLockOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    (void)buildVersion;
    return InitializeOptParam(optParam);
}

int APS5_VABI sceUltReaderWriterLockCreate(void* rwlock, const char* name, void* waitingQueueResourcePool, const UltOptParam* optParam, std::uint32_t buildVersion) {
    (void)optParam;
    (void)buildVersion;
    if (rwlock == nullptr) {
        return ULT_ERROR_NULL;
    }
    if (waitingQueueResourcePool != nullptr && !UltWaitingPoolExists(waitingQueueResourcePool)) {
        return ULT_ERROR_INVALID;
    }
    auto state = std::make_shared<UltReaderWriterLockState>();
    state->_waitingPool = waitingQueueResourcePool;
    if (name != nullptr) {
        std::strncpy(state->_name, name, sizeof(state->_name) - 1);
    }
    std::lock_guard<std::mutex> lock(gLocksMutex);
    if (gLocks.contains(rwlock)) {
        return ULT_ERROR_STATE;
    }
    std::memset(rwlock, 0, 256);
    gLocks[rwlock] = std::move(state);
    return ULT_OK;
}

int APS5_VABI sceUltReaderWriterLockDestroy(void* rwlock) {
    if (rwlock == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::lock_guard<std::mutex> lock(gLocksMutex);
    const auto it = gLocks.find(rwlock);
    if (it == gLocks.end()) {
        return ULT_ERROR_STATE;
    }
    const auto owner = it->second;
    auto& state = *owner;
    std::lock_guard<std::mutex> stateLock(state._mutex);
    if (state._readers != 0 || state._writing || state._waiters != 0) {
        return ULT_ERROR_BUSY;
    }
    state._alive = false;
    gLocks.erase(it);
    return ULT_OK;
}

int APS5_VABI sceUltReaderWriterLockLockRead(void* rwlock) {
    std::shared_ptr<UltReaderWriterLockState> state;
    if (const int found = FindLock(rwlock, state); found != ULT_OK) return found;
    std::unique_lock<std::mutex> lock(state->_mutex);
    if (!state->_alive || HeldByCaller(*state)) {
        return ULT_ERROR_STATE;
    }
    if (const int waited = Wait(*state, lock, false); waited != ULT_OK) return waited;
    ++state->_readers;
    return ULT_OK;
}

int APS5_VABI sceUltReaderWriterLockTryLockRead(void* rwlock) {
    std::shared_ptr<UltReaderWriterLockState> state;
    if (const int found = FindLock(rwlock, state); found != ULT_OK) return found;
    std::lock_guard<std::mutex> lock(state->_mutex);
    if (!state->_alive || HeldByCaller(*state)) {
        return ULT_ERROR_STATE;
    }
    if (state->_writing) {
        return ULT_ERROR_BUSY;
    }
    ++state->_readers;
    return ULT_OK;
}

int APS5_VABI sceUltReaderWriterLockUnlockRead(void* rwlock) {
    std::shared_ptr<UltReaderWriterLockState> state;
    if (const int found = FindLock(rwlock, state); found != ULT_OK) return found;
    std::lock_guard<std::mutex> lock(state->_mutex);
    if (!state->_alive || state->_readers == 0) {
        return ULT_ERROR_STATE;
    }
    if (--state->_readers == 0) {
        state->_released.notify_all();
    }
    return ULT_OK;
}

int APS5_VABI sceUltReaderWriterLockLockWrite(void* rwlock) {
    std::shared_ptr<UltReaderWriterLockState> state;
    if (const int found = FindLock(rwlock, state); found != ULT_OK) return found;
    std::unique_lock<std::mutex> lock(state->_mutex);
    if (!state->_alive || HeldByCaller(*state)) {
        return ULT_ERROR_STATE;
    }
    if (const int waited = Wait(*state, lock, true); waited != ULT_OK) return waited;
    state->_writing = true;
    state->_writer = std::this_thread::get_id();
    return ULT_OK;
}

int APS5_VABI sceUltReaderWriterLockTryLockWrite(void* rwlock) {
    std::shared_ptr<UltReaderWriterLockState> state;
    if (const int found = FindLock(rwlock, state); found != ULT_OK) return found;
    std::lock_guard<std::mutex> lock(state->_mutex);
    if (!state->_alive || HeldByCaller(*state)) {
        return ULT_ERROR_STATE;
    }
    if (state->_writing || state->_readers != 0) {
        return ULT_ERROR_BUSY;
    }
    state->_writing = true;
    state->_writer = std::this_thread::get_id();
    return ULT_OK;
}

int APS5_VABI sceUltReaderWriterLockUnlockWrite(void* rwlock) {
    std::shared_ptr<UltReaderWriterLockState> state;
    if (const int found = FindLock(rwlock, state); found != ULT_OK) return found;
    std::lock_guard<std::mutex> lock(state->_mutex);
    if (!state->_alive || !HeldByCaller(*state)) {
        return ULT_ERROR_STATE;
    }
    state->_writing = false;
    state->_writer = {};
    state->_released.notify_all();
    return ULT_OK;
}

int APS5_VABI sceUltGetReaderWriterLockInfo(void* object, UltReaderWriterLockInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity) {
    if (object == nullptr || info == nullptr) return ULT_ERROR_NULL;
    if (waitingThreads == nullptr && capacity != 0) return ULT_ERROR_NULL;
    std::shared_ptr<UltReaderWriterLockState> state;
    if (const int found = FindLock(object, state); found != ULT_OK) return found;
    std::lock_guard<std::mutex> lock(state->_mutex);
    if (!state->_alive) return ULT_ERROR_STATE;
    std::memset(info, 0, sizeof(*info));
    info->size = sizeof(UltReaderWriterLockInfo);
    info->object = object;
    std::memcpy(info->name, state->_name, sizeof(info->name));
    info->waitingQueuePool = state->_waitingPool;
    info->readers = state->_readers;
    info->waitingThreads = state->_waiters;
    info->lockType = state->_writing ? 1 : (state->_readers > 0 ? 2 : 0);
    const auto toCopy = std::min(capacity, info->waitingThreads);
    for (std::uint32_t i = 0; i < toCopy; ++i) {
        std::memset(&waitingThreads[i], 0, sizeof(UltWaitingThreadInfo));
        waitingThreads[i].size = sizeof(UltWaitingThreadInfo);
    }
    return ULT_OK;
}

}
