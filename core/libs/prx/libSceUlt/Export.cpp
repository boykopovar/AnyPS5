#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "SceUltTypes.hpp"

extern "C" int scePthreadCreate(Pthread* thread, const PthreadAttr* attr, void* (*entry)(void*), void* arg, const char* name);
extern "C" int scePthreadJoin(Pthread thread, void** retval);

namespace {


std::mutex gMutex;
std::unordered_map<void*, std::shared_ptr<UltMutexState>> gMutexes;
std::unordered_map<void*, std::shared_ptr<UltSemaphoreState>> gSemaphores;
std::unordered_map<void*, UltResourcePoolState> gResourcePools;
std::unordered_map<void*, UltQueueDataPoolState> gQueueDataPools;
std::unordered_map<void*, std::shared_ptr<UltQueueState>> gQueues;
std::unordered_map<void*, UltRuntimeState> gRuntimes;
std::unordered_map<void*, std::shared_ptr<UltUlthreadState>> gUlthreads;

std::uint64_t alignUp(std::uint64_t value, std::uint64_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
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

void* ulthreadRunner(void* arg) {
    auto* state = static_cast<UltUlthreadState*>(arg);
    return reinterpret_cast<void*>(static_cast<std::intptr_t>(state->_entry(state->_arg)));
}

}

extern "C" {

int APS5_VABI sceUltInitialize() {
    return ULT_OK;
}

int APS5_VABI sceUltFinalize() {
    std::vector<std::shared_ptr<UltSemaphoreState>> semaphores;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        semaphores.reserve(gSemaphores.size());
        for (auto& entry : gSemaphores) {
            auto& state = entry.second;
            std::lock_guard<std::mutex> stateLock(state->_mutex);
            state->_alive = false;
            semaphores.push_back(state);
        }
        gSemaphores.clear();
        gMutexes.clear();
        gResourcePools.clear();
        gQueueDataPools.clear();
        gQueues.clear();
        gRuntimes.clear();
        gUlthreads.clear();
    }
    for (const auto& state : semaphores) {
        state->_available.notify_all();
    }
    return ULT_OK;
}

int APS5_VABI sceUltMutexOptParamInitialize(UltMutexOptParam* optParam, std::uint32_t buildVersion) {
    (void)buildVersion;
    if (optParam == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::memset(optParam, 0, sizeof(*optParam));
    return ULT_OK;
}

int APS5_VABI sceUltMutexCreate(void* mutex, const char* name, void* waitingQueueResourcePool, const UltMutexOptParam* optParam, std::uint32_t buildVersion) {
    (void)name;
    (void)buildVersion;
    if (mutex == nullptr) {
        return ULT_ERROR_NULL;
    }
    auto state = std::make_shared<UltMutexState>();
    state->_attribute = optParam != nullptr ? optParam->attribute : 0;
    std::lock_guard<std::mutex> lock(gMutex);
    if (waitingQueueResourcePool != nullptr && gResourcePools.find(waitingQueueResourcePool) == gResourcePools.end()) {
        return ULT_ERROR_INVALID;
    }
    std::memset(mutex, 0, 256);
    gMutexes[mutex] = std::move(state);
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
    state->_mutex.lock();
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
    state->_mutex.unlock();
    return ULT_OK;
}

int APS5_VABI sceUltWaitingQueueResourcePoolGetWorkAreaSize(std::uint32_t numThreads, std::uint32_t numSyncObjects) {
    return static_cast<int>(alignUp(static_cast<std::uint64_t>(numThreads + numSyncObjects) * 256u, 8u));
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

std::uint64_t APS5_VABI sceUltQueueDataResourcePoolGetWorkAreaSize(std::uint32_t numData, std::uint64_t dataSize, std::uint32_t numQueueObject) {
    const std::uint64_t dataArea = static_cast<std::uint64_t>(numData) * alignUp(dataSize, 8u);
    const std::uint64_t queueArea = static_cast<std::uint64_t>(numQueueObject) * 512u;
    return alignUp(dataArea + queueArea, 8u);
}

int APS5_VABI sceUltQueueDataResourcePoolCreate(void* pool, const char* name, std::uint32_t numData, std::uint64_t dataSize, std::uint32_t numQueueObject, void* waitingQueueResourcePool, void* workArea, const void* optParam, std::uint32_t buildVersion) {
    (void)name;
    (void)optParam;
    (void)buildVersion;
    if (pool == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::lock_guard<std::mutex> lock(gMutex);
    if (waitingQueueResourcePool != nullptr && gResourcePools.find(waitingQueueResourcePool) == gResourcePools.end()) {
        return ULT_ERROR_INVALID;
    }
    std::memset(pool, 0, 512);
    gQueueDataPools[pool] = {numData, dataSize, numQueueObject, waitingQueueResourcePool, workArea};
    return ULT_OK;
}

int APS5_VABI sceUltQueueCreate(void* queue, const char* name, std::uint64_t dataSize, void* waitingQueueResourcePool, void* queueDataResourcePool, const void* optParam, std::uint32_t buildVersion) {
    (void)name;
    (void)optParam;
    (void)buildVersion;
    if (queue == nullptr) {
        return ULT_ERROR_NULL;
    }
    auto state = std::make_shared<UltQueueState>();
    state->_dataSize = dataSize;
    state->_waitingPool = waitingQueueResourcePool;
    state->_dataPool = queueDataResourcePool;
    std::lock_guard<std::mutex> lock(gMutex);
    auto dataPoolIt = gQueueDataPools.find(queueDataResourcePool);
    if (dataPoolIt == gQueueDataPools.end()) {
        return ULT_ERROR_INVALID;
    }
    if (waitingQueueResourcePool != nullptr && gResourcePools.find(waitingQueueResourcePool) == gResourcePools.end()) {
        return ULT_ERROR_INVALID;
    }
    state->_capacity = dataPoolIt->second._numData;
    std::memset(queue, 0, 512);
    gQueues[queue] = std::move(state);
    return ULT_OK;
}

int APS5_VABI sceUltQueuePush(void* queue, const void* data) {
    std::shared_ptr<UltQueueState> state;
    if (int ret = queueGetState(queue, &state); ret != ULT_OK) {
        return ret;
    }
    if (data == nullptr && state->_dataSize != 0) {
        return ULT_ERROR_NULL;
    }
    std::lock_guard<std::mutex> lock(state->_mutex);
    if (state->_capacity != 0 && state->_items.size() >= state->_capacity) {
        return ULT_OK;
    }
    auto& item = state->_items.emplace_back(static_cast<std::size_t>(state->_dataSize));
    if (!item.empty()) {
        std::memcpy(item.data(), data, item.size());
    }
    return ULT_OK;
}

int APS5_VABI sceUltQueueTryPop(void* queue, void* data) {
    std::shared_ptr<UltQueueState> state;
    if (int ret = queueGetState(queue, &state); ret != ULT_OK) {
        return ret;
    }
    if (data == nullptr && state->_dataSize != 0) {
        return ULT_ERROR_NULL;
    }
    std::lock_guard<std::mutex> lock(state->_mutex);
    if (state->_items.empty()) {
        return ULT_ERROR_AGAIN;
    }
    auto item = std::move(state->_items.front());
    state->_items.pop_front();
    if (!item.empty()) {
        std::memcpy(data, item.data(), item.size());
    }
    return ULT_OK;
}

int APS5_VABI sceUltSemaphoreCreate(void* semaphore, const char* name, std::int32_t numInitialResource, void* waitingQueueResourcePool, const void* optParam, std::uint32_t buildVersion) {
    (void)name;
    (void)optParam;
    (void)buildVersion;
    if (semaphore == nullptr) {
        return ULT_ERROR_NULL;
    }
    if ((reinterpret_cast<std::uintptr_t>(semaphore) & 7u) != 0) {
        return ULT_ERROR_ALIGNMENT;
    }
    if (numInitialResource < 0) {
        return ULT_ERROR_RANGE;
    }
    auto state = std::make_shared<UltSemaphoreState>();
    state->_resources = numInitialResource;
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
    if (semaphore == nullptr) {
        return ULT_ERROR_NULL;
    }
    std::lock_guard<std::mutex> globalLock(gMutex);
    auto it = gSemaphores.find(semaphore);
    if (it == gSemaphores.end()) {
        return ULT_ERROR_STATE;
    }
    auto state = it->second;
    std::lock_guard<std::mutex> stateLock(state->_mutex);
    if (state->_waiters != 0) {
        return ULT_ERROR_BUSY;
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
    return alignUp(static_cast<std::uint64_t>(maxNumUlthread) * 256u + static_cast<std::uint64_t>(numWorkerThread) * 16u * 1024u, 8u);
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

}
