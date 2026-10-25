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
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#ifndef _WIN32
#include <pthread.h>
#endif

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
    int pshared = 0;
};

constexpr int ADAPTIVE_MUTEX_SPINS = 2000;

struct PthreadMutexPrivate {
    std::recursive_timed_mutex _rmtx;
    std::timed_mutex _mtx;
    MutexType _type;
    std::atomic<std::thread::id> _owner;
    int _count;
    int _spinloops = 0;
    int _yieldloops = 0;

    PthreadMutexPrivate() : _type(MutexType::ErrorCheck), _owner(std::thread::id{}), _count(0) {}

    void SetType(MutexType type) {
        _type = type;
        _spinloops = type == MutexType::Adaptive ? ADAPTIVE_MUTEX_SPINS : 0;
    }
};

struct PthreadRwlockattrPrivate {
    int type;
};

struct PthreadRwlockPrivate {
    std::mutex _state;
    TimedWait::Condition _changed;
    std::atomic<std::thread::id> _writer;
    std::atomic<int> _readers{0};
    int _waitingWriters = 0;
};

struct PthreadCondattrPrivate {
    int _clockid;
    int _pshared = 0;
};

struct PthreadCondPrivate {
    std::shared_ptr<TimedWait::Condition> _cv = std::make_shared<TimedWait::Condition>();
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
static constexpr int GUEST_EXPLICIT_SCHED = 0;
static constexpr int GUEST_INHERIT_SCHED = 4;
static constexpr int GUEST_SCOPE_PROCESS = 0;
static constexpr int GUEST_SCOPE_SYSTEM = 2;

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
    bool _affinityExplicit = false;
    std::size_t _guardsize = 0x1000;
    int _solosched = 0;
    int _scope = GUEST_SCOPE_SYSTEM;
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
    std::atomic<int> schedPolicy{1};
    void* wakeEvent = nullptr;
    void* stackAddress = nullptr;
    std::size_t stackSize = 0;
    std::atomic<int> waitCount{0};
    std::atomic<KernelCpumask> affinity{DEFAULT_THREAD_AFFINITY};
    std::atomic<int> priority{DEFAULT_THREAD_PRIORITY};
    int scope = GUEST_SCOPE_SYSTEM;
    int cpuClockThread = 0;
#ifndef _WIN32
    clockid_t hostCpuClock{};
    bool hostCpuClockBound = false;
    void* guestStack = nullptr;
    std::size_t guestStackBytes = 0;
    int hostTid = 0;
#endif
    std::mutex nameLock;
    std::string name;
    std::atomic<bool> _finished;
    void* _retval;
    bool _detached;
    bool _adopted;
    Pthread* _handleStorage;
    std::atomic<bool> _joined;
        bool _joining = false;
    std::mutex _join_mtx;
    TimedWait::Condition _join_cv;
    std::atomic<bool> cancelPending{false};
    std::mutex cancelLock;
    TimedWait::Condition* cancelWait = nullptr;

    explicit PthreadPrivate(const PthreadAttrPrivate* attr = nullptr);
    ~PthreadPrivate();
    PthreadPrivate(const PthreadPrivate&) = delete;
    PthreadPrivate& operator=(const PthreadPrivate&) = delete;
};

bool GuestThreadStack(std::uintptr_t address, std::uintptr_t* start, std::uintptr_t* end);
bool GuestThreadStackHostRangeOverlaps(std::uintptr_t start, std::uintptr_t end);
void NoteGuestStackProtection(std::uintptr_t start, std::size_t bytes, bool readWrite);
#ifdef _WIN32
bool ReplaceGuestThreadStack(std::uintptr_t start, std::size_t bytes, bool readWrite, const std::function<bool()>& replace);
#endif
int GuestThreadCpuClockId(const PthreadPrivate* thread);
bool GuestThreadCpuNanos(int cpuClockThread, std::uint64_t* nanos);
void SetLiveGuestThreadsAffinity(KernelCpumask mask);
KernelCpumask LiveGuestThreadsAffinity();
bool GuestThreadIsLive(const PthreadPrivate* thread);
std::unique_lock<std::mutex> LockLiveGuestThread(const PthreadPrivate* thread);
bool LiveGuestThreadExists(std::int64_t tid);
bool GetLiveGuestThreadAffinity_nid_no_patch(std::int64_t tid, KernelCpumask& mask);
bool GetLiveGuestThreadSched_nid_no_patch(std::int64_t tid, int& policy, int& priority);
bool SetLiveGuestThreadAffinity_nid_no_patch(std::int64_t tid, KernelCpumask mask);
extern "C" std::size_t GetDefaultStackSize_nid_no_patch();
extern "C" bool SetDefaultStackSize_nid_no_patch(std::size_t size);
extern "C" [[noreturn]] void APS5_VABI EnterGuestMainStack_nid_no_patch(void* arguments, void (APS5_VABI* entry)(void*));

#endif
