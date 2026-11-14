#include "prx/libSceUlt/include/Api.hpp"
#include "prx/libSceUlt/src/ReaderWriterLock.hpp"
#include "prx/libSceUlt/SceUltTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

struct WaitRecord {
    void* thread = nullptr;
    std::uint32_t type = 0;
    std::int32_t waitValue = 0;
};

struct UltReaderWriterLockState {
    std::mutex _mutex;
    std::condition_variable _released;
    std::uint32_t _readers = 0;
    bool _writing = false;
    std::thread::id _writer{};
    UltThreadIdentity _ownerIdentity;
    std::uint32_t _waiters = 0;
    bool _alive = true;
    void* _waitingPool = nullptr;
    char _name[32]{};
    std::vector<WaitRecord> _waiterRecords;
};

std::mutex gLocksMutex;
std::unordered_map<void*, std::shared_ptr<UltReaderWriterLockState>> gLocks;

bool TrackWaitRecord(WaitRecord& record, std::int32_t waitValue) {
    const UltThreadIdentity identity = UltCurrentThreadIdentity_nid_no_patch();
    if (identity.thread == nullptr) return false;
    record = {identity.thread, identity.type, waitValue};
    return true;
}

void RemoveWaitRecord(std::vector<WaitRecord>& records, const WaitRecord& record) {
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

int WriteWaitRecords(const std::vector<WaitRecord>& records, UltWaitingThreadInfo* destination, std::uint32_t capacity) {
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

int Wait(UltReaderWriterLockState& state, std::unique_lock<std::mutex>& lock, bool writer, std::int32_t waitValue) {
    WaitRecord record;
    const bool tracked = TrackWaitRecord(record, waitValue);
    if (tracked) state._waiterRecords.push_back(record);
    ++state._waiters;
    try {
        state._released.wait(lock, [&] { return !state._alive || (!state._writing && (!writer || state._readers == 0)); });
    } catch (...) {
        --state._waiters;
        if (tracked) RemoveWaitRecord(state._waiterRecords, record);
        throw;
    }
    --state._waiters;
    if (tracked) RemoveWaitRecord(state._waiterRecords, record);
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
    if (!CopyUltName(state->_name, name)) return ULT_ERROR_NULL;
    std::lock_guard<std::mutex> lock(gLocksMutex);
    if (gLocks.contains(rwlock)) {
        return ULT_ERROR_STATE;
    }
    if (!ClearUltObject(rwlock, 256)) return ULT_ERROR_NULL;
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
    if (const int waited = Wait(*state, lock, false, 1); waited != ULT_OK) return waited;
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
    if (const int waited = Wait(*state, lock, true, 2); waited != ULT_OK) return waited;
    state->_ownerIdentity = UltCurrentThreadIdentity_nid_no_patch();
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
    state->_ownerIdentity = UltCurrentThreadIdentity_nid_no_patch();
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
    state->_ownerIdentity = {};
    state->_released.notify_all();
    return ULT_OK;
}

int APS5_VABI _sceUltReaderWriterLockOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion) {
    return sceUltReaderWriterLockOptParamInitialize(optParam, buildVersion);
}

int APS5_VABI _sceUltReaderWriterLockCreate(void* rwlock, const char* name, void* waitingQueueResourcePool, const UltOptParam* optParam, std::uint32_t buildVersion) {
    return sceUltReaderWriterLockCreate(rwlock, name, waitingQueueResourcePool, optParam, buildVersion);
}

int APS5_VABI sceUltGetReaderWriterLockInfo(void* object, UltReaderWriterLockInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity) {
    if (object == nullptr || info == nullptr) return ULT_ERROR_NULL;
    if (waitingThreads == nullptr && capacity != 0) return ULT_ERROR_NULL;
    std::shared_ptr<UltReaderWriterLockState> state;
    if (const int found = FindLock(object, state); found != ULT_OK) return found;
    std::lock_guard<std::mutex> lock(state->_mutex);
    if (!state->_alive) return ULT_ERROR_STATE;
    if (const int status = ValidateWaitRecords(waitingThreads, capacity); status != ULT_OK) return status;
    if (state->_writing && state->_ownerIdentity.type != 1) NotImplemented_nid_no_patch(__func__);
    UltReaderWriterLockInfo localInfo{};
    localInfo.size = sizeof(UltReaderWriterLockInfo);
    localInfo.object = object;
    std::memcpy(localInfo.name, state->_name, sizeof(localInfo.name));
    localInfo.waitingQueuePool = state->_waitingPool;
    localInfo.readers = state->_readers;
    localInfo.waitingThreads = state->_waiters;
    localInfo.lockType = state->_writing ? 2 : (state->_readers > 0 ? 1 : 0);
    localInfo.owner = state->_ownerIdentity.thread;
    localInfo.ownerType = state->_ownerIdentity.type;
    if (!GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo))) return ULT_ERROR_NULL;
    return WriteWaitRecords(state->_waiterRecords, waitingThreads, capacity);
}

}
