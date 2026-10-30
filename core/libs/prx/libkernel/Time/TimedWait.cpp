#include "prx/libkernel/Time/include/TimedWait.hpp"
#include "prx/libkernel/Pthread/include/WindowsThreadLocal.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
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

constexpr std::uint64_t YIELD_BELOW_NANOS = 50000ULL;
constexpr std::uint64_t LEAD_NANOS = 500000ULL;

std::mutex& ChangeSleepersMutex() {
    static std::mutex mutex;
    return mutex;
}

std::atomic<std::uint64_t>& ChangeSleeperCount() {
    static std::atomic<std::uint64_t> count{0};
    return count;
}

#ifdef _WIN32
std::vector<HANDLE>& ChangeSleepers() {
    static std::vector<HANDLE> sleepers;
    return sleepers;
}
#else
std::condition_variable& ChangeCondition() {
    static std::condition_variable condition;
    return condition;
}
#endif

struct ThreadWaitState {
    std::atomic<int> own{0};
    std::atomic<int>* bound = &own;
};
ThreadWaitState& WaitState() {
#ifdef _WIN32
    return WindowsThreadLocal<ThreadWaitState>::Get();
#else
    static thread_local ThreadWaitState state;
    return state;
#endif
}

#ifdef _WIN32

using NtTestAlertFunction = LONG(NTAPI*)();

void DrainApcs() {
    static const auto testAlert = reinterpret_cast<NtTestAlertFunction>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtTestAlert")));
    if (testAlert) testAlert();
    else SleepEx(0, TRUE);
}

DWORD AlertableWait(DWORD count, const HANDLE* handles, DWORD milliseconds) {
    WaitState().bound->fetch_add(1, std::memory_order_seq_cst);
    DWORD result;
    do {
        result = WaitForMultipleObjectsEx(count, handles, FALSE, milliseconds, TRUE);
    } while (result == WAIT_IO_COMPLETION);
    WaitState().bound->fetch_sub(1, std::memory_order_seq_cst);
    DrainApcs();
    return result;
}

void AlertableSleep(DWORD milliseconds) {
    WaitState().bound->fetch_add(1, std::memory_order_seq_cst);
    SleepEx(milliseconds, TRUE);
    WaitState().bound->fetch_sub(1, std::memory_order_seq_cst);
    DrainApcs();
}

HANDLE ThreadTimer(unsigned kind);

HANDLE CreateHighResolutionTimer() {
    return CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
}

bool TimerWait(HANDLE timer, std::uint64_t nanos) {
    thread_local int depth = 0;
    struct Nesting {
        HANDLE own;
        explicit Nesting(bool nested) : own(nested ? CreateHighResolutionTimer() : nullptr) { ++depth; }
        ~Nesting() {
            --depth;
            if (own) CloseHandle(own);
        }
    } nesting(depth != 0);
    if (depth > 1) timer = nesting.own;
    if (!timer) return false;
    LARGE_INTEGER due{};
    due.QuadPart = -static_cast<LONGLONG>(nanos / 100ULL);
    if (due.QuadPart == 0) due.QuadPart = -1;
    if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) return false;
    return AlertableWait(1, &timer, INFINITE) == WAIT_OBJECT_0;
}

DWORD WholeMilliseconds(std::uint64_t nanos) {
    const std::uint64_t millis = nanos / 1000000ULL;
    return millis == 0 ? 1u : static_cast<DWORD>(millis > 0xFFFFFFFEULL ? 0xFFFFFFFEULL : millis);
}

void SleepNanosCoarse(std::uint64_t nanos) {
    if (nanos < YIELD_BELOW_NANOS) {
        SwitchToThread();
        return;
    }
    const HANDLE timer = ThreadTimer(0);
    if (TimerWait(timer, nanos)) return;
    AlertableSleep(static_cast<DWORD>((nanos + 999999ULL) / 1000000ULL));
}

#endif

}  // namespace

#ifdef _WIN32

struct Waiter {
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HANDLE timer = CreateHighResolutionTimer();
    HANDLE sleepTimers[2]{};
    Waiter* previous = nullptr;
    Waiter* next = nullptr;
    Waiter* nested = nullptr;
    const void* key = nullptr;
    HANDLE wake = nullptr;
    bool queued = false;
    bool busy = false;

    ~Waiter() {
        delete nested;
        if (event) CloseHandle(event);
        if (wake) CloseHandle(wake);
        if (timer) CloseHandle(timer);
        for (const auto sleepTimer : sleepTimers) if (sleepTimer) CloseHandle(sleepTimer);
    }
};

namespace {

Waiter* ThisThreadWaiter() {
    return &WindowsThreadLocal<Waiter>::Get();
}

HANDLE ThreadTimer(unsigned kind) {
    auto& timer = ThisThreadWaiter()->sleepTimers[kind];
    if (!timer) timer = CreateHighResolutionTimer();
    return timer;
}

Waiter* AcquireWaiter() {
    Waiter* waiter = ThisThreadWaiter();
    while (waiter->busy) {
        if (!waiter->nested) waiter->nested = new Waiter();
        waiter = waiter->nested;
    }
    waiter->busy = true;
    return waiter;
}

bool EventSet(HANDLE event, DWORD milliseconds) {
    return AlertableWait(1, &event, milliseconds) == WAIT_OBJECT_0;
}

bool WaitEventUntil(Waiter* waiter, std::uint64_t deadlineNanos) {
    for (;;) {
        const std::uint64_t now = NowNanos();
        if (now >= deadlineNanos) return EventSet(waiter->event, 0);
        const std::uint64_t remaining = deadlineNanos - now;
        if (remaining > LEAD_NANOS) {
            const std::uint64_t bulk = remaining - LEAD_NANOS;
            LARGE_INTEGER due{};
            due.QuadPart = -static_cast<LONGLONG>(bulk / 100ULL);
            if (waiter->timer && SetWaitableTimer(waiter->timer, &due, 0, nullptr, nullptr, FALSE)) {
                HANDLE handles[2] = {waiter->event, waiter->timer};
                if (AlertableWait(2, handles, INFINITE) == WAIT_OBJECT_0) return true;
                continue;
            }
            if (EventSet(waiter->event, WholeMilliseconds(bulk))) return true;
            continue;
        }
        if (EventSet(waiter->event, 0)) return true;
        YieldProcessor();
    }
}

}  // namespace

Waiter* Condition::enqueue() {
    Waiter* waiter = AcquireWaiter();
    std::lock_guard lock(queueLock);
    waiter->queued = true;
    waiter->next = nullptr;
    waiter->previous = tail;
    if (tail) tail->next = waiter;
    else head = waiter;
    tail = waiter;
    return waiter;
}

void Condition::unlink(Waiter* waiter) {
    if (waiter->previous) waiter->previous->next = waiter->next;
    else head = waiter->next;
    if (waiter->next) waiter->next->previous = waiter->previous;
    else tail = waiter->previous;
    waiter->previous = nullptr;
    waiter->next = nullptr;
    waiter->queued = false;
}

void Condition::waitSignal(Waiter* waiter) {
    AlertableWait(1, &waiter->event, INFINITE);
    waiter->busy = false;
}

bool Condition::waitSignalUntil(Waiter* waiter, std::uint64_t deadlineNanos) {
    bool signaled = WaitEventUntil(waiter, deadlineNanos);
    if (!signaled) {
        std::lock_guard lock(queueLock);
        if (waiter->queued) {
            unlink(waiter);
        } else {
            WaitForSingleObject(waiter->event, 0);
            signaled = true;
        }
    }
    waiter->busy = false;
    return signaled;
}

void Condition::NotifyOne() {
    if (Coarse()) {
        coarse.notify_one();
        return;
    }
    std::lock_guard lock(queueLock);
    Waiter* waiter = head;
    if (!waiter) return;
    unlink(waiter);
    SetEvent(waiter->event);
}

void Condition::NotifyAll() {
    if (Coarse()) {
        coarse.notify_all();
        return;
    }
    std::lock_guard lock(queueLock);
    while (Waiter* waiter = head) {
        unlink(waiter);
        SetEvent(waiter->event);
    }
}

DeliveryDeferral::DeliveryDeferral() {
    WaitState().bound->fetch_add(1, std::memory_order_seq_cst);
}

DeliveryDeferral::~DeliveryDeferral() {
    WaitState().bound->fetch_sub(1, std::memory_order_seq_cst);
}

void DeliverDeferred() {
    DrainApcs();
}

namespace {

struct alignas(64) ParkBucket {
    std::mutex lock;
    Waiter* head = nullptr;
    Waiter* tail = nullptr;
    std::atomic<int> parked{0};
};

ParkBucket parkBuckets[256];

ParkBucket& ParkBucketFor(const void* key) {
    return parkBuckets[(reinterpret_cast<std::uintptr_t>(key) * 0x9E3779B97F4A7C15ULL) >> 56];
}

void UnlinkParked(ParkBucket& bucket, Waiter* waiter) {
    if (waiter->previous) waiter->previous->next = waiter->next;
    else bucket.head = waiter->next;
    if (waiter->next) waiter->next->previous = waiter->previous;
    else bucket.tail = waiter->previous;
    waiter->previous = nullptr;
    waiter->next = nullptr;
    waiter->queued = false;
}

bool LeavePark(ParkBucket& bucket, Waiter* waiter) {
    bool signaled;
    {
        DeliveryDeferral deferral;
        std::lock_guard lock(bucket.lock);
        signaled = !waiter->queued;
        if (!signaled) UnlinkParked(bucket, waiter);
    }
    if (signaled) WaitForSingleObject(waiter->event, 0);
    return signaled;
}

void ReleaseParked(ParkBucket& bucket, Waiter* waiter) {
    bucket.parked.fetch_sub(1, std::memory_order_seq_cst);
    waiter->busy = false;
    DrainApcs();
}

}  // namespace

void SpinPause() {
    YieldProcessor();
}

Waiter* BeginPark(const void* key) {
    auto& bucket = ParkBucketFor(key);
    Waiter* waiter;
    {
        DeliveryDeferral deferral;
        waiter = AcquireWaiter();
        waiter->key = key;
        bucket.parked.fetch_add(1, std::memory_order_seq_cst);
        std::lock_guard lock(bucket.lock);
        waiter->queued = true;
        waiter->next = nullptr;
        waiter->previous = bucket.tail;
        if (bucket.tail) bucket.tail->next = waiter;
        else bucket.head = waiter;
        bucket.tail = waiter;
    }
    DrainApcs();
    return waiter;
}

bool FinishPark(const void* key, Waiter* waiter, const std::uint64_t* deadlineNanos) {
    auto& bucket = ParkBucketFor(key);
    bool signaled = deadlineNanos ? WaitEventUntil(waiter, *deadlineNanos) : AlertableWait(1, &waiter->event, INFINITE) == WAIT_OBJECT_0;
    if (!signaled) signaled = LeavePark(bucket, waiter);
    ReleaseParked(bucket, waiter);
    if (!signaled && !deadlineNanos) throw std::runtime_error("TimedWait: an untimed parked wait ended without a wake");
    return signaled;
}

void CancelPark(const void* key, Waiter* waiter) {
    auto& bucket = ParkBucketFor(key);
    LeavePark(bucket, waiter);
    ReleaseParked(bucket, waiter);
}

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
    coarse.notify_one();
}

void Condition::NotifyAll() {
    coarse.notify_all();
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
    if (deadlineNanos > now + LEAD_NANOS) {
        const HANDLE timer = ThreadTimer(1);
        if (!TimerWait(timer, deadlineNanos - now - LEAD_NANOS)) AlertableSleep(WholeMilliseconds(deadlineNanos - now - LEAD_NANOS));
        now = NowNanos();
    }
    while (now < deadlineNanos) {
        YieldProcessor();
        now = NowNanos();
    }
#else
    const std::uint64_t now = NowNanos();
    if (deadlineNanos > now) SleepNanos(deadlineNanos - now);
#endif
}

bool SleepNanosUnlessChanged(std::uint64_t nanos, const std::atomic<std::uint64_t>& counter, std::uint64_t observed) {
    const std::uint64_t deadlineNanos = NowNanos() + nanos;
#ifdef _WIN32
    if (Coarse() || nanos < YIELD_BELOW_NANOS) {
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
    if (deadlineNanos > now + LEAD_NANOS) {
        const HANDLE handles[2] = {ThreadTimer(1), waiter->wake};
        LARGE_INTEGER due{};
        due.QuadPart = -static_cast<LONGLONG>((deadlineNanos - now - LEAD_NANOS) / 100ULL);
        if (due.QuadPart == 0) due.QuadPart = -1;
        if (handles[0] && SetWaitableTimer(handles[0], &due, 0, nullptr, nullptr, FALSE)) {
            if (AlertableWait(2, handles, INFINITE) == WAIT_OBJECT_0 + 1) CancelWaitableTimer(handles[0]);
        } else {
            AlertableWait(1, &handles[1], WholeMilliseconds(deadlineNanos - now - LEAD_NANOS));
        }
        if (counter.load(std::memory_order_seq_cst) != observed) return true;
        now = NowNanos();
    }
    while (now < deadlineNanos) {
        if (counter.load(std::memory_order_seq_cst) != observed) return true;
        YieldProcessor();
        now = NowNanos();
    }
    return false;
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
    if (Coarse()) {
        SleepNanosCoarse(nanos);
        return;
    }
    if (nanos < YIELD_BELOW_NANOS) {
        SwitchToThread();
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
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {}
#endif
}

}  // namespace TimedWait
