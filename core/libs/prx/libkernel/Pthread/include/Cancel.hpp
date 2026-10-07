#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_CANCEL_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_CANCEL_HPP

#include "SceTypes.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <cstdint>
#include <optional>

inline constexpr int GUEST_CANCEL_ENABLE = 0;
inline constexpr int GUEST_CANCEL_DISABLE = 1;
inline constexpr int GUEST_CANCEL_DEFERRED = 0;
inline constexpr int GUEST_CANCEL_ASYNCHRONOUS = 2;
inline void* const GUEST_CANCELED = reinterpret_cast<void*>(std::uintptr_t{1});

extern "C" {
void APS5_VABI __pthread_cleanup_push_imp_nid_postfix(void (APS5_VABI* routine)(void*), void* argument, void* info);
void APS5_VABI __pthread_cleanup_pop_imp_nid_postfix(int execute);
}

namespace ThreadCancel {

bool Requested();
void Check();
void BeginWait(TimedWait::Condition* condition);
void EndWait();

template <class TLock>
struct WaitLock {
    TLock& inner;
    void lock() {
        EndWait();
        inner.lock();
    }
    void unlock() { inner.unlock(); }
};

template <class TLock>
bool Wait(TimedWait::Condition& condition, TLock& lock, std::optional<std::uint64_t> deadlineNanos) {
    BeginWait(&condition);
    if (Requested()) {
        EndWait();
        return true;
    }
    WaitLock<TLock> waitLock{lock};
    if (deadlineNanos) return condition.WaitUntil(waitLock, *deadlineNanos);
    condition.Wait(waitLock);
    return true;
}

template <class TLock, class TPredicate>
bool WaitUntil(TimedWait::Condition& condition, TLock& lock, std::optional<std::uint64_t> deadlineNanos, TPredicate predicate) {
    while (!predicate()) {
        const bool signaled = Wait(condition, lock, deadlineNanos);
        if (Requested()) {
            lock.unlock();
            Check();
        }
        if (!signaled) return predicate();
    }
    return true;
}

}

#endif
