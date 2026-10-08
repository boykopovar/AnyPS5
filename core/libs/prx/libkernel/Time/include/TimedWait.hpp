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
void PollSleepUntil(std::uint64_t deadlineNanos);

constexpr std::uint64_t LOCK_BULK_NANOS = 40000000ULL;
constexpr std::uint64_t LOCK_POLL_NANOS = 500000ULL;
constexpr std::uint64_t LOCK_TAIL_NANOS = 1000000ULL;

template <class TTryLock, class TTryLockFor>
bool AcquireUntil(std::uint64_t deadlineNanos, TTryLock tryLock, TTryLockFor tryLockFor) {
    if (Coarse()) return tryLockFor(RemainingMicros(deadlineNanos));
    for (;;) {
        if (tryLock()) return true;
        const std::uint64_t now = NowNanos();
        if (now >= deadlineNanos) return false;
        const std::uint64_t remaining = deadlineNanos - now;
        if (remaining > LOCK_BULK_NANOS) {
            if (tryLockFor((remaining - LOCK_BULK_NANOS) / 1000ULL)) return true;
            continue;
        }
        if (remaining <= LOCK_TAIL_NANOS) SleepUntil(deadlineNanos);
        else PollSleepUntil(now + LOCK_POLL_NANOS);
    }
}

class InterruptibleScope {
public:
    InterruptibleScope();
    ~InterruptibleScope();
    InterruptibleScope(const InterruptibleScope&) = delete;
    InterruptibleScope& operator=(const InterruptibleScope&) = delete;

    void Poll();
};

constexpr std::uint64_t INTERRUPT_SLICE_MICROS = 1000ULL;

template <class TTryLock, class TTryLockFor>
void AcquireInterruptibly(TTryLock tryLock, TTryLockFor tryLockFor) {
    if (tryLock()) return;
    InterruptibleScope scope;
    while (!tryLockFor(INTERRUPT_SLICE_MICROS)) scope.Poll();
}

template <class TTryLock, class TTryLockFor>
bool AcquireInterruptiblyUntil(std::uint64_t deadlineNanos, TTryLock tryLock, TTryLockFor tryLockFor) {
    if (tryLock()) return true;
    if (Coarse()) return tryLockFor(RemainingMicros(deadlineNanos));
    InterruptibleScope scope;
    return AcquireUntil(deadlineNanos, tryLock, [&](std::uint64_t micros) {
        if (tryLockFor(micros < INTERRUPT_SLICE_MICROS ? micros : INTERRUPT_SLICE_MICROS)) return true;
        scope.Poll();
        return false;
    });
}

struct Waiter;

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
