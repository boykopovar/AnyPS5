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

constexpr std::uint64_t SHORT_SLEEP_NANOS = 50000ULL;
constexpr std::uint64_t SHORT_SLEEP_SPIN_RUN_NANOS = 1000000ULL;
constexpr unsigned SHORT_SLEEP_PAUSES_PER_CLOCK_READ = 8;
constexpr std::uint64_t LEAD_NANOS = 20000ULL;

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
    int timerDepth = 0;
};

struct DeferredDrain {
    void (*run)(void*);
    void* argument;
};

std::mutex drainBoxesLock;
std::unordered_map<unsigned long, std::vector<DeferredDrain>> drainBoxes;
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
    auto* state = WaitState().bound;
    state->fetch_add(1, std::memory_order_seq_cst);
    if (testAlert) testAlert();
    else SleepEx(0, TRUE);
    state->fetch_sub(1, std::memory_order_seq_cst);
    if (testAlert) testAlert();
    else SleepEx(0, TRUE);
    ReapDeliveryDrain();
}

std::uint64_t& LastWakeNanos() {
    thread_local std::uint64_t woke = 0;
    return woke;
}

DWORD AlertableWait(DWORD count, const HANDLE* handles, DWORD milliseconds) {
    WaitState().bound->fetch_add(1, std::memory_order_seq_cst);
    DWORD result;
    do {
        result = WaitForMultipleObjectsEx(count, handles, FALSE, milliseconds, TRUE);
    } while (result == WAIT_IO_COMPLETION);
    LastWakeNanos() = NowNanos();
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

constexpr std::uint64_t CALIBRATION_WAIT_NANOS = 1000000ULL;
constexpr std::size_t CALIBRATION_WAITS = 9;
constexpr std::uint64_t MAX_TIMER_LATE_NANOS = 1000000ULL;
constexpr std::uint64_t LATE_LEAD_SHARE = 8;
constexpr std::uint64_t LATE_AVERAGE_WEIGHT = 16;

std::uint64_t CalibratedTimerLateNanos() {
    const HANDLE timer = CreateHighResolutionTimer();
    if (!timer) return 0;
    std::array<std::uint64_t, CALIBRATION_WAITS> samples{};
    for (auto& sample : samples) {
        LARGE_INTEGER due{};
        due.QuadPart = -static_cast<LONGLONG>(CALIBRATION_WAIT_NANOS / 100ULL);
        const std::uint64_t start = NowNanos();
        if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) || WaitForSingleObject(timer, 100) != WAIT_OBJECT_0) {
            CloseHandle(timer);
            return 0;
        }
        const std::uint64_t elapsed = NowNanos() - start;
        sample = elapsed > CALIBRATION_WAIT_NANOS ? elapsed - CALIBRATION_WAIT_NANOS : 0;
    }
    CloseHandle(timer);
    std::sort(samples.begin(), samples.end());
    return std::min(samples[CALIBRATION_WAITS / 2], MAX_TIMER_LATE_NANOS);
}

std::atomic<std::uint64_t>& TimerLateAverage() {
    static std::atomic<std::uint64_t> average{CalibratedTimerLateNanos()};
    return average;
}

void RecordTimerLate(std::uint64_t dueNanos) {
    const std::uint64_t woke = LastWakeNanos();
    const std::uint64_t sample = std::min(woke > dueNanos ? woke - dueNanos : 0, MAX_TIMER_LATE_NANOS);
    auto& average = TimerLateAverage();
    const std::uint64_t previous = average.load(std::memory_order_relaxed);
    average.store((previous * (LATE_AVERAGE_WEIGHT - 1) + sample) / LATE_AVERAGE_WEIGHT, std::memory_order_relaxed);
}

std::uint64_t LeadNanos(std::uint64_t remainingNanos) {
    return LEAD_NANOS + std::min(TimerLateAverage().load(std::memory_order_relaxed), remainingNanos / LATE_LEAD_SHARE);
}

bool TimerWait(HANDLE timer, std::uint64_t nanos) {
    struct Nesting {
        int& depth;
        HANDLE own;
        explicit Nesting(int& depth) : depth(depth), own(depth != 0 ? CreateHighResolutionTimer() : nullptr) { ++depth; }
        ~Nesting() {
            --depth;
            if (own) CloseHandle(own);
        }
    } nesting(WaitState().timerDepth);
    if (nesting.depth > 1) timer = nesting.own;
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
    std::uint64_t shortSleepRunStartNanos = 0;
    std::uint64_t shortSleepEndNanos = 0;
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

std::atomic<void (*)()>& DrainPostHook() {
    static std::atomic<void (*)()> hook{nullptr};
    return hook;
}

void SetDeliveryDrainPostHook(void (*hook)()) {
    DrainPostHook().store(hook, std::memory_order_release);
}

void QueueForDeliveryDrain(unsigned long thread, const std::atomic<bool>* targetFinished, void (*run)(void*), void* argument) {
    if (const auto hook = DrainPostHook().load(std::memory_order_acquire)) hook();
    bool queued = false;
    {
        const std::lock_guard lock(drainBoxesLock);
        if (targetFinished == nullptr || !targetFinished->load(std::memory_order_acquire)) {
            drainBoxes[thread].push_back({run, argument});
            queued = true;
        }
    }
    if (!queued) run(argument);
}

void ReapDeliveryDrain() {
    std::vector<DeferredDrain> deferred;
    {
        const std::lock_guard lock(drainBoxesLock);
        const auto box = drainBoxes.find(GetCurrentThreadId());
        if (box != drainBoxes.end()) {
            deferred = std::move(box->second);
            drainBoxes.erase(box);
        }
    }
    for (const auto& entry : deferred) entry.run(entry.argument);
}

std::size_t DeliveryDrainPending() {
    const std::lock_guard lock(drainBoxesLock);
    std::size_t pending = 0;
    for (const auto& box : drainBoxes) pending += box.second.size();
    return pending;
}

namespace {

Waiter* ThisThreadWaiter() {
    return &WindowsThreadLocal<Waiter>::Get();
}

HANDLE ThreadTimer(unsigned kind) {
    auto& timer = ThisThreadWaiter()->sleepTimers[kind];
    if (!timer) timer = CreateHighResolutionTimer();
    return timer;
}

void SleepShort(std::uint64_t nanos) {
    Waiter* waiter = ThisThreadWaiter();
    std::uint64_t now = NowNanos();
    const std::uint64_t deadlineNanos = now + nanos;
    if (now - waiter->shortSleepEndNanos >= nanos) waiter->shortSleepRunStartNanos = now;
    if (now - waiter->shortSleepRunStartNanos >= SHORT_SLEEP_SPIN_RUN_NANOS) {
        if (!TimerWait(ThreadTimer(0), nanos)) AlertableSleep(1);
        now = NowNanos();
    }
    {
        DeliveryDeferral deferral;
        while (now < deadlineNanos) {
            for (unsigned pause = 0; pause < SHORT_SLEEP_PAUSES_PER_CLOCK_READ; ++pause) YieldProcessor();
            now = NowNanos();
        }
    }
    DrainApcs();
    waiter->shortSleepEndNanos = NowNanos();
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
    const std::uint64_t start = NowNanos();
    const std::uint64_t lead = LeadNanos(deadlineNanos > start ? deadlineNanos - start : 0);
    for (;;) {
        const std::uint64_t now = NowNanos();
        if (now >= deadlineNanos) return EventSet(waiter->event, 0);
        const std::uint64_t remaining = deadlineNanos - now;
        if (remaining > lead) {
            const std::uint64_t bulk = remaining - lead;
            LARGE_INTEGER due{};
            due.QuadPart = -static_cast<LONGLONG>(bulk / 100ULL);
            if (waiter->timer && SetWaitableTimer(waiter->timer, &due, 0, nullptr, nullptr, FALSE)) {
                HANDLE handles[2] = {waiter->event, waiter->timer};
                const DWORD result = AlertableWait(2, handles, INFINITE);
                if (result == WAIT_OBJECT_0) return true;
                if (result == WAIT_OBJECT_0 + 1) RecordTimerLate(deadlineNanos - lead);
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
        if (coarseWaiters.load(std::memory_order_seq_cst) != 0) coarse.notify_one();
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
        if (coarseWaiters.load(std::memory_order_seq_cst) != 0) coarse.notify_all();
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

}
