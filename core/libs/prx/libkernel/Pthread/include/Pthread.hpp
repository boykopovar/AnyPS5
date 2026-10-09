#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_PTHREAD_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_PTHREAD_HPP

#include <sched.h>
#include "SceTypes.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <atomic>
#include <bit>
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
    Adaptive = 4,
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

    PthreadMutexPrivate() : _type(MutexType::ErrorCheck), _owner(std::thread::id{}), _count(0) {}
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
    TimedWait::Condition _cv;
    int _clockid = 0;
};

struct PthreadSemPrivate {
    std::mutex _mutex;
    TimedWait::Condition _cv;
    int _count = 0;

    explicit PthreadSemPrivate(unsigned int value) : _count(static_cast<int>(value)) {}
};

static constexpr KernelCpumask DEFAULT_THREAD_AFFINITY = 0x1FFF;
static constexpr int DEFAULT_THREAD_PRIORITY = 700;

inline int GuestCpuFromHost(unsigned hostCpu, KernelCpumask threadAffinity) {
    KernelCpumask mask = threadAffinity & DEFAULT_THREAD_AFFINITY;
    if (mask == 0) mask = DEFAULT_THREAD_AFFINITY;
    for (auto skip = hostCpu % static_cast<unsigned>(std::popcount(mask)); skip != 0; --skip) mask &= mask - 1;
    return std::countr_zero(mask);
}

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
#else
    std::thread _thr;
#endif
    std::thread::id threadId;
    std::atomic<unsigned> references{2};
    void* stackAddress = nullptr;
    std::size_t stackSize = 0;
    std::atomic<int> waitCount{0};
    std::atomic<KernelCpumask> affinity{DEFAULT_THREAD_AFFINITY};
    std::atomic<int> priority{DEFAULT_THREAD_PRIORITY};
    std::mutex nameLock;
    std::string name;
    std::atomic<bool> _finished;
    void* _retval;
    bool _detached;
    bool _adopted;
    std::mutex _join_mtx;
    std::condition_variable _join_cv;

    PthreadPrivate() : _finished(false), _retval(nullptr), _detached(false), _adopted(false) {}
};

bool GuestThreadStack(std::uintptr_t address, std::uintptr_t* start, std::uintptr_t* end);

#endif
