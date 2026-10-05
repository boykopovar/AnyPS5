#ifndef CORE_LIBS_PRX_LIBSCEULT_SCEULTTYPES_HPP
#define CORE_LIBS_PRX_LIBSCEULT_SCEULTTYPES_HPP

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>
#include "SceTypes.hpp"

constexpr int ULT_OK = 0;
constexpr int ULT_ERROR_NULL = -2139029503;
constexpr int ULT_ERROR_ALIGNMENT = -2139029502;
constexpr int ULT_ERROR_RANGE = -2139029501;
constexpr int ULT_ERROR_INVALID = -2139029500;
constexpr int ULT_ERROR_STATE = -2139029498;
constexpr int ULT_ERROR_BUSY = -2139029497;
constexpr int ULT_ERROR_AGAIN = -2139029496;

struct UltMutexState {
    std::recursive_mutex _mutex;
    std::uint32_t _attribute = 0;
};

struct UltSemaphoreState {
    std::mutex _mutex;
    std::condition_variable _available;
    std::int32_t _resources = 0;
    std::uint32_t _waiters = 0;
    bool _alive = true;
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
    Pthread _thread = nullptr;
};

#endif
