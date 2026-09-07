#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_PTHREAD_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_PTHREAD_HPP

#include <sched.h>
#include "SceTypes.hpp"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
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

struct PthreadCondattrPrivate {
    int _clockid;
};

struct PthreadCondPrivate {
    std::condition_variable_any _cv;
};

struct PthreadAttrPrivate {
    std::size_t _stacksize;
    int _detachstate;
    int _schedpriority;
    int _schedpolicy;
    int _inheritsched;
};

struct PthreadPrivate {
    std::thread _thr;
    std::atomic<bool> _finished;
    void* _retval;
    bool _detached;
    std::mutex _join_mtx;
    std::condition_variable _join_cv;

    PthreadPrivate() : _finished(false), _retval(nullptr), _detached(false) {}
};

#endif
