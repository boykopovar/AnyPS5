#ifndef CORE_LIBS_PRX_LIBKERNEL_TIME_INCLUDE_TIMEDWAIT_HPP
#define CORE_LIBS_PRX_LIBKERNEL_TIME_INCLUDE_TIMEDWAIT_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#elif !defined(_WIN32)
#include <semaphore.h>
#endif

namespace TimedWait {

class TickConverter {
public:
    explicit TickConverter(std::uint64_t frequency) : frequency(frequency) {
        if (frequency == 0) return;
        if (NANOS_PER_SECOND % frequency == 0) {
            mode = Mode::Direct;
            nanosPerTick = NANOS_PER_SECOND / frequency;
        } else if (frequency <= MAX_RECIPROCAL_FREQUENCY) {
            mode = Mode::Reciprocal;
            divider = Reciprocal(frequency);
        }
    }

    std::uint64_t Nanos(std::uint64_t ticks) const {
        switch (mode) {
        case Mode::Direct:
            return ticks * nanosPerTick;
        case Mode::Reciprocal: {
            const std::uint64_t whole = divider.Divide(ticks);
            const std::uint64_t remainder = ticks - whole * frequency;
            return whole * NANOS_PER_SECOND + divider.Divide(remainder * NANOS_PER_SECOND);
        }
        default:
            return (ticks / frequency) * NANOS_PER_SECOND + (ticks % frequency) * NANOS_PER_SECOND / frequency;
        }
    }

private:
    static constexpr std::uint64_t NANOS_PER_SECOND = 1000000000ULL;
    static constexpr std::uint64_t MAX_RECIPROCAL_FREQUENCY = 1ULL << 34;

    enum class Mode { Division, Direct, Reciprocal };

    class Reciprocal {
    public:
        Reciprocal() = default;

        explicit Reciprocal(std::uint64_t divisor) {
            while (divisor > (std::uint64_t{1} << shift)) ++shift;
            const unsigned __int128 span = static_cast<unsigned __int128>((std::uint64_t{1} << shift) - divisor) << 64;
            multiplier = static_cast<std::uint64_t>(span / divisor) + 1;
        }

        std::uint64_t Divide(std::uint64_t value) const {
            const auto high = static_cast<std::uint64_t>((static_cast<unsigned __int128>(value) * multiplier) >> 64);
            return (high + ((value - high) >> 1)) >> (shift - 1);
        }

    private:
        std::uint64_t multiplier = 0;
        unsigned shift = 0;
    };

    std::uint64_t frequency;
    std::uint64_t nanosPerTick = 0;
    Mode mode = Mode::Division;
    Reciprocal divider;
};

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

class DeliveryDeferral {
public:
    DeliveryDeferral();
    ~DeliveryDeferral();
    DeliveryDeferral(const DeliveryDeferral&) = delete;
    DeliveryDeferral& operator=(const DeliveryDeferral&) = delete;
};

void DeliverDeferred();

#ifdef _WIN32
void QueueForDeliveryDrain(unsigned long thread, const std::atomic<bool>* targetFinished, void (*run)(void*), void* argument);
void ReapDeliveryDrain();
std::size_t DeliveryDrainPending();
void SetDeliveryDrainPostHook(void (*hook)());

constexpr unsigned PARK_SPINS = 1000;

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

#ifdef _WIN32
using SingleWaiterCondition = Condition;
#else
bool DeferDelivery(int signum);

class SingleWaiterCondition {
public:
    SingleWaiterCondition();
    ~SingleWaiterCondition();
    SingleWaiterCondition(const SingleWaiterCondition&) = delete;
    SingleWaiterCondition& operator=(const SingleWaiterCondition&) = delete;

    void NotifyOne();

    template <class TLock, class TPredicate>
    void Wait(TLock& lock, TPredicate predicate) {
        while (!predicate()) {
            lock.unlock();
            waitSignal(nullptr);
            lock.lock();
        }
    }

    template <class TLock, class TPredicate>
    bool WaitUntil(TLock& lock, std::uint64_t deadlineNanos, TPredicate predicate) {
        while (!predicate()) {
            lock.unlock();
            const bool signaled = waitSignal(&deadlineNanos);
            lock.lock();
            if (!signaled) return predicate();
        }
        return true;
    }

private:
    bool waitSignal(const std::uint64_t* deadlineNanos);

#ifdef __APPLE__
    dispatch_semaphore_t semaphore;
#else
    sem_t semaphore;
#endif
};
#endif

}  // namespace TimedWait

#endif
