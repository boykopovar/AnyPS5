#include "prx/libkernel/Time/include/TimedWait.hpp"
#include "prx/libkernel/Pthread/include/WindowsThreadLocal.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <pthread.h>
#include <system_error>
#include <time.h>
#endif

namespace TimedWait {
namespace {
namespace {
namespace {
void Unpark(const void* key) {
    auto& bucket = ParkBucketFor(key);
    if (bucket.parked.load(std::memory_order_seq_cst) == 0) return;
    {
        DeliveryDeferral deferral;
        std::lock_guard lock(bucket.lock);
        for (Waiter* waiter = bucket.head; waiter; waiter = waiter->next) {
            if (waiter->key != key) continue;
            UnlinkParked(bucket, waiter);
            SetEvent(waiter->event);
            break;
        }
    }
    DrainApcs();
}

#else

void Condition::NotifyOne() {
    if (coarseWaiters.load(std::memory_order_seq_cst) != 0) coarse.notify_one();
}

void Condition::NotifyAll() {
    if (coarseWaiters.load(std::memory_order_seq_cst) != 0) coarse.notify_all();
}

namespace {

constinit thread_local std::atomic<int> deferralDepth{0};
constinit thread_local std::atomic<int> deferredSignal{0};

class DeliveryAllowance {
public:
    DeliveryAllowance() : depth(deferralDepth.exchange(0)) { DeliverDeferred(); }
    ~DeliveryAllowance() { deferralDepth.store(depth); }
    DeliveryAllowance(const DeliveryAllowance&) = delete;
    DeliveryAllowance& operator=(const DeliveryAllowance&) = delete;

private:
    int depth;
};

}

DeliveryDeferral::DeliveryDeferral() {
    deferralDepth.fetch_add(1);
}

DeliveryDeferral::~DeliveryDeferral() {
    deferralDepth.fetch_sub(1);
}

void DeliverDeferred() {
    if (deferralDepth.load() != 0) return;
    const int signum = deferredSignal.exchange(0);
    if (signum != 0) pthread_kill(pthread_self(), signum);
}

bool DeferDelivery(int signum) {
    if (deferralDepth.load() == 0) return false;
    deferredSignal.store(signum);
    return true;
}

#ifdef __APPLE__
SingleWaiterCondition::SingleWaiterCondition() : semaphore(dispatch_semaphore_create(0)) {
    if (semaphore == nullptr) throw std::runtime_error("TimedWait: cannot create a wait semaphore");
}

SingleWaiterCondition::~SingleWaiterCondition() {
    dispatch_release(semaphore);
}

void SingleWaiterCondition::NotifyOne() {
    dispatch_semaphore_signal(semaphore);
}

bool SingleWaiterCondition::waitSignal(const std::uint64_t* deadlineNanos) {
    DeliveryAllowance allowance;
    if (deadlineNanos == nullptr) return dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER) == 0;
    const std::uint64_t now = NowNanos();
    const std::uint64_t remaining = *deadlineNanos > now ? *deadlineNanos - now : 0;
    const dispatch_time_t deadline = remaining >= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ? DISPATCH_TIME_FOREVER : dispatch_time(DISPATCH_TIME_NOW, static_cast<std::int64_t>(remaining));
    return dispatch_semaphore_wait(semaphore, deadline) == 0;
}
#else
SingleWaiterCondition::SingleWaiterCondition() {
    if (sem_init(&semaphore, 0, 0) != 0) throw std::system_error(errno, std::generic_category(), "TimedWait: cannot create a wait semaphore");
}

SingleWaiterCondition::~SingleWaiterCondition() {
    sem_destroy(&semaphore);
}

void SingleWaiterCondition::NotifyOne() {
    sem_post(&semaphore);
}

bool SingleWaiterCondition::waitSignal(const std::uint64_t* deadlineNanos) {
    DeliveryAllowance allowance;
    timespec deadline{};
    if (deadlineNanos != nullptr) deadline = {static_cast<time_t>(*deadlineNanos / 1000000000ULL), static_cast<long>(*deadlineNanos % 1000000000ULL)};
    for (;;) {
        const int result = deadlineNanos == nullptr ? sem_wait(&semaphore) : sem_clockwait(&semaphore, CLOCK_MONOTONIC, &deadline);
        if (result == 0) return true;
        if (errno == ETIMEDOUT) return false;
        if (errno != EINTR) throw std::system_error(errno, std::generic_category(), "TimedWait: cannot wait for a wait semaphore");
    }
}
#endif

#endif

void BindThreadWaitState(std::atomic<int>* state) {
    auto& local = WaitState();
    local.bound = state != nullptr ? state : &local.own;
}

bool Coarse() {
#ifdef _WIN32
    static const bool coarse = std::getenv("APS5_COARSE_TIMED_WAITS") != nullptr;
    return coarse;
#else
    return true;
#endif
}

std::uint64_t TimerLateNanos() {
#ifdef _WIN32
    return TimerLateAverage().load(std::memory_order_relaxed);
#else
    return 0;
#endif
}

std::uint64_t NowNanos() {
#ifdef _WIN32
    static const std::uint64_t frequency = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return static_cast<std::uint64_t>(f.QuadPart);
    }();
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    const std::uint64_t ticks = static_cast<std::uint64_t>(counter.QuadPart);
    return (ticks / frequency) * 1000000000ULL + (ticks % frequency) * 1000000000ULL / frequency;
#else
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts.tv_nsec);
#endif
}

std::uint64_t DeadlineNanos(std::uint64_t microseconds) {
    const std::uint64_t now = NowNanos();
    if (microseconds > (std::numeric_limits<std::uint64_t>::max() - now) / 1000ULL) return std::numeric_limits<std::uint64_t>::max();
    return now + microseconds * 1000ULL;
}

std::uint64_t RemainingMicros(std::uint64_t deadlineNanos) {
    const std::uint64_t now = NowNanos();
    return deadlineNanos > now ? (deadlineNanos - now) / 1000ULL : 0;
}

void SleepUntil(std::uint64_t deadlineNanos) {
#ifdef _WIN32
    std::uint64_t now = NowNanos();
    const std::uint64_t lead = LeadNanos(deadlineNanos > now ? deadlineNanos - now : 0);
    if (deadlineNanos > now + lead) {
        const HANDLE timer = ThreadTimer(1);
        if (TimerWait(timer, deadlineNanos - now - lead)) RecordTimerLate(deadlineNanos - lead);
        else AlertableSleep(WholeMilliseconds(deadlineNanos - now - lead));
        now = NowNanos();
    }
    {
        DeliveryDeferral deferral;
        while (now < deadlineNanos) {
            YieldProcessor();
            now = NowNanos();
        }
    }
    DrainApcs();
#else
    const std::uint64_t now = NowNanos();
    if (deadlineNanos > now) SleepNanos(deadlineNanos - now);
#endif
}

bool SleepNanosUnlessChanged(std::uint64_t nanos, const std::atomic<std::uint64_t>& counter, std::uint64_t observed) {
    const std::uint64_t deadlineNanos = NowNanos() + nanos;
#ifdef _WIN32
    if (Coarse() || nanos < SHORT_SLEEP_NANOS) {
        SleepNanos(nanos);
        return false;
    }
    Waiter* waiter = ThisThreadWaiter();
    if (!waiter->wake) waiter->wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!waiter->wake) throw std::runtime_error("TimedWait: CreateEventW failed for a wakeable sleep");
    {
        std::lock_guard lock(ChangeSleepersMutex());
        ChangeSleepers().push_back(waiter->wake);
    }
    ChangeSleeperCount().fetch_add(1, std::memory_order_seq_cst);
    struct Unregister {
        HANDLE wake;
        ~Unregister() {
            ChangeSleeperCount().fetch_sub(1, std::memory_order_seq_cst);
            std::lock_guard lock(ChangeSleepersMutex());
            auto& sleepers = ChangeSleepers();
            sleepers.erase(std::find(sleepers.begin(), sleepers.end(), wake));
        }
    } unregister{waiter->wake};
    ResetEvent(waiter->wake);
    std::uint64_t now = NowNanos();
    if (counter.load(std::memory_order_seq_cst) != observed) return true;
    const std::uint64_t lead = LeadNanos(deadlineNanos > now ? deadlineNanos - now : 0);
    if (deadlineNanos > now + lead) {
        const HANDLE handles[2] = {ThreadTimer(1), waiter->wake};
        LARGE_INTEGER due{};
        due.QuadPart = -static_cast<LONGLONG>((deadlineNanos - now - lead) / 100ULL);
        if (due.QuadPart == 0) due.QuadPart = -1;
        if (handles[0] && SetWaitableTimer(handles[0], &due, 0, nullptr, nullptr, FALSE)) {
            const DWORD result = AlertableWait(2, handles, INFINITE);
            if (result == WAIT_OBJECT_0) RecordTimerLate(deadlineNanos - lead);
            else if (result == WAIT_OBJECT_0 + 1) CancelWaitableTimer(handles[0]);
        } else {
            AlertableWait(1, &handles[1], WholeMilliseconds(deadlineNanos - now - lead));
        }
        if (counter.load(std::memory_order_seq_cst) != observed) return true;
        now = NowNanos();
    }
    bool changed = false;
    {
        DeliveryDeferral deferral;
        while (now < deadlineNanos) {
            if (counter.load(std::memory_order_seq_cst) != observed) {
                changed = true;
                break;
            }
            YieldProcessor();
            now = NowNanos();
        }
    }
    DrainApcs();
    return changed;
#else
    std::unique_lock lock(ChangeSleepersMutex());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::nanoseconds(nanos);
    ChangeSleeperCount().fetch_add(1, std::memory_order_seq_cst);
    const bool changed = ChangeCondition().wait_until(lock, deadline, [&] { return counter.load(std::memory_order_seq_cst) != observed; });
    ChangeSleeperCount().fetch_sub(1, std::memory_order_seq_cst);
    return changed;
#endif
}

void WakeChangeSleepers() {
    if (ChangeSleeperCount().load(std::memory_order_seq_cst) == 0) return;
    std::lock_guard lock(ChangeSleepersMutex());
#ifdef _WIN32
    for (const HANDLE sleeper : ChangeSleepers()) SetEvent(sleeper);
#else
    ChangeCondition().notify_all();
#endif
}

void SleepNanos(std::uint64_t nanos) {
    if (nanos == 0) return;
#ifdef _WIN32
    if (nanos < SHORT_SLEEP_NANOS) {
        SleepShort(nanos);
        return;
    }
    if (Coarse()) {
        SleepNanosCoarse(nanos);
        return;
    }
    SleepUntil(NowNanos() + nanos);
#else
    struct SleepRegistration {
        std::atomic<int>* state = WaitState().bound;
        SleepRegistration() { state->fetch_add(1, std::memory_order_seq_cst); }
        ~SleepRegistration() { state->fetch_sub(1, std::memory_order_seq_cst); }
    } registration;
    struct timespec req{};
    req.tv_sec = static_cast<time_t>(nanos / 1000000000ULL);
    req.tv_nsec = static_cast<long>(nanos % 1000000000ULL);
    const int savedErrno = errno;
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {}
    errno = savedErrno;
#endif
}

}  // namespace TimedWait
}
}
}
