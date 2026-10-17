#ifndef CORE_LIBS_PRX_LIBKERNEL_TIME_INCLUDE_TIMEDWAIT_HPP
#define CORE_LIBS_PRX_LIBKERNEL_TIME_INCLUDE_TIMEDWAIT_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace TimedWait {

bool Coarse();
void BindThreadWaitState(std::atomic<int>* state);
std::uint64_t NowNanos();
std::uint64_t DeadlineNanos(std::uint64_t microseconds);
std::uint64_t RemainingMicros(std::uint64_t deadlineNanos);
void SleepNanos(std::uint64_t nanos);
void SleepUntil(std::uint64_t deadlineNanos);
bool SleepNanosUnlessChanged(std::uint64_t nanos, const std::atomic<std::uint64_t>& counter, std::uint64_t observed);
void WakeChangeSleepers();

struct Waiter;

#ifdef _WIN32
constexpr unsigned PARK_SPINS = 1000;

class DeliveryDeferral {
public:
    DeliveryDeferral();
    ~DeliveryDeferral();
    DeliveryDeferral(const DeliveryDeferral&) = delete;
    DeliveryDeferral& operator=(const DeliveryDeferral&) = delete;
};

void DeliverDeferred();
void SpinPause();
Waiter* BeginPark(const void* key);
bool FinishPark(const void* key, Waiter* waiter, const std::uint64_t* deadlineNanos);
void CancelPark(const void* key, Waiter* waiter);
void Unpark(const void* key);

template <class TTryLock>
bool ParkAcquire(const void* key, TTryLock tryLock, const std::uint64_t* deadlineNanos) {
    for (unsigned spin = 0; spin < PARK_SPINS; ++spin) {
        if (tryLock()) return true;
        SpinPause();
    }
    for (;;) {
        Waiter* waiter = BeginPark(key);
        if (tryLock()) {
            CancelPark(key, waiter);
            return true;
        }
        if (!FinishPark(key, waiter, deadlineNanos)) return tryLock();
        if (tryLock()) return true;
    }
}
#endif

template <class TTryLock, class TTryLockFor>
bool AcquireUntil(const void* key, std::uint64_t deadlineNanos, TTryLock tryLock, TTryLockFor tryLockFor) {
#ifdef _WIN32
    if (!Coarse()) return ParkAcquire(key, tryLock, &deadlineNanos);
#endif
    static_cast<void>(key);
    static_cast<void>(tryLock);
    return tryLockFor(RemainingMicros(deadlineNanos));
}

class Condition {
public:
    Condition() = default;
    ~Condition() = default;
    Condition(const Condition&) = delete;
    Condition& operator=(const Condition&) = delete;

    void NotifyOne();
    void NotifyAll();

    template <class TLock>
    void Wait(TLock& lock) {
#ifdef _WIN32
        if (!Coarse()) {
            Waiter* waiter = enqueue();
            lock.unlock();
            waitSignal(waiter);
            lock.lock();
            return;
        }
#endif
        coarse.wait(lock);
    }

    template <class TLock>
    bool WaitUntil(TLock& lock, std::uint64_t deadlineNanos) {
#ifdef _WIN32
        if (!Coarse()) {
            Waiter* waiter = enqueue();
            lock.unlock();
            const bool signaled = waitSignalUntil(waiter, deadlineNanos);
            lock.lock();
            return signaled;
        }
#endif
        return coarse.wait_for(lock, std::chrono::microseconds(RemainingMicros(deadlineNanos))) == std::cv_status::no_timeout;
    }

    template <class TLock, class TPredicate>
    void Wait(TLock& lock, TPredicate predicate) {
        while (!predicate()) Wait(lock);
    }

    template <class TLock, class TPredicate>
    bool WaitUntil(TLock& lock, std::uint64_t deadlineNanos, TPredicate predicate) {
        while (!predicate()) {
            if (!WaitUntil(lock, deadlineNanos)) return predicate();
        }
        return true;
    }

private:
#ifdef _WIN32
    Waiter* enqueue();
    void unlink(Waiter* waiter);
    void waitSignal(Waiter* waiter);
    bool waitSignalUntil(Waiter* waiter, std::uint64_t deadlineNanos);

    std::mutex queueLock;
    Waiter* head = nullptr;
    Waiter* tail = nullptr;
#endif
    std::condition_variable_any coarse;
};

}  // namespace TimedWait

#endif
