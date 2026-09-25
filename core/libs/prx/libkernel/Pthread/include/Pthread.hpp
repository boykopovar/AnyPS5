#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_PTHREAD_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_PTHREAD_HPP

#include <sched.h>
#include "SceTypes.hpp"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>

enum class MutexType : std::uint32_t {
    ErrorCheck = 1,
    Recursive = 2,
    Normal = 3,
};

struct PthreadMutexattrPrivate {
    MutexType type;
};

struct PthreadMutexPrivate {
    std::recursive_timed_mutex _rmtx;
    std::timed_mutex _mtx;
    MutexType _type;
    std::atomic<std::thread::id> _owner;
    int _count;

    PthreadMutexPrivate() : _type(MutexType::Normal), _count(0) {}
};

struct PthreadRwlockattrPrivate {
    int type;
};

struct PthreadRwlockPrivate {
    std::shared_timed_mutex _lock;
    std::atomic<std::thread::id> _writer;
};

struct PthreadCondattrPrivate {
    int _clockid;
};

struct PthreadCondPrivate {
    std::condition_variable_any _cv;
    int _clockid = 0;
};

static constexpr KernelCpumask DEFAULT_THREAD_AFFINITY = 0x1FFF;
static constexpr int DEFAULT_THREAD_PRIORITY = 700;

struct PthreadAttrPrivate {
    void* stackAddress = nullptr;
    std::size_t _stacksize;
    int _detachstate;
    int _schedpriority;
    int _schedpolicy;
    int _inheritsched;
    KernelCpumask _affinity = DEFAULT_THREAD_AFFINITY;
    std::size_t _guardsize = 0x1000;
    int _solosched = 0;
};

struct PthreadPrivate {
#ifdef _WIN32
    void* nativeHandle = nullptr;
    std::thread::id threadId;
    std::atomic<unsigned> references{2};
#else
    std::thread _thr;
#endif
    void* stackAddress = nullptr;
    std::size_t stackSize = 0;
    std::atomic<KernelCpumask> affinity{DEFAULT_THREAD_AFFINITY};
    std::atomic<int> priority{DEFAULT_THREAD_PRIORITY};
    std::mutex nameLock;
    std::string name;
    std::atomic<bool> _finished;
    void* _retval;
    bool _detached;
    std::mutex _join_mtx;
    std::condition_variable _join_cv;

    PthreadPrivate() : _finished(false), _retval(nullptr), _detached(false) {}
};

#endif
