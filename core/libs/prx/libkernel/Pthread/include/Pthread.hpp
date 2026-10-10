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
#include <ctime>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>

#ifndef _WIN32
#include <pthread.h>
#endif

enum class MutexType : std::uint32_t {
    ErrorCheck = 1,
    Recursive = 2,
    Normal = 3,
    Adaptive = 4,
};

struct PthreadMutexattrPrivate {
    MutexType type;
    int protocol = 0;
    int ceiling = 0;
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
    // TODO(technical debt): winpthreads initializes a static rwlock on first use and fails a
    // concurrent first lock with EINVAL, which shared_timed_mutex ignores. Initialize it here.
    PthreadRwlockPrivate() {
        _lock.lock();
        _lock.unlock();
    }
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
static constexpr std::uint32_t CPU_CLOCK_BIT = 0x80000000u;
static constexpr std::uint32_t CPU_CLOCK_PROCESS_BIT = 0x40000000u;
static constexpr std::uint32_t CPU_CLOCK_ID_MASK = ~(CPU_CLOCK_BIT | CPU_CLOCK_PROCESS_BIT);

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

inline std::atomic<std::int64_t> nextThreadId{100000};

struct PthreadPrivate {
    std::int64_t tid = nextThreadId.fetch_add(1, std::memory_order_relaxed);
#ifdef _WIN32
    void* nativeHandle = nullptr;
#else
    pthread_t hostThread{};
#endif
    std::thread::id threadId;
    std::atomic<unsigned> references{2};
    std::atomic<bool> inWait{false};
    std::atomic<int> pendingException{0};
    void* wakeEvent = nullptr;
    void* stackAddress = nullptr;
    std::size_t stackSize = 0;
    std::atomic<int> waitCount{0};
    std::atomic<KernelCpumask> affinity{DEFAULT_THREAD_AFFINITY};
    std::atomic<int> priority{DEFAULT_THREAD_PRIORITY};
    int cpuClockThread = 0;
#ifndef _WIN32
    clockid_t hostCpuClock{};
    bool hostCpuClockBound = false;
#endif
    std::mutex nameLock;
    std::string name;
    std::atomic<bool> _finished;
    void* _retval;
    bool _detached;
    bool _adopted;
    std::mutex _join_mtx;
    TimedWait::Condition _join_cv;
    std::atomic<bool> cancelPending{false};
    std::mutex cancelLock;
    TimedWait::Condition* cancelWait = nullptr;

    PthreadPrivate();
    ~PthreadPrivate();
    PthreadPrivate(const PthreadPrivate&) = delete;
    PthreadPrivate& operator=(const PthreadPrivate&) = delete;
};

bool GuestThreadStack(std::uintptr_t address, std::uintptr_t* start, std::uintptr_t* end);
int GuestThreadCpuClockId(const PthreadPrivate* thread);
bool GuestThreadCpuNanos(int cpuClockThread, std::uint64_t* nanos);

#endif
