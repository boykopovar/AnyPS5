#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include "prx/libkernel/Pthread/Posix/Common.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace {

constexpr int sceTimedOut = static_cast<int>(0x8002003cu);
std::mutex& InitializationMutex() {
    static std::mutex mutex;
    return mutex;
}

PthreadCond destroyedCond() {
    return reinterpret_cast<PthreadCond>(std::uintptr_t{1});
}

PthreadMutex destroyedMutex() {
    return reinterpret_cast<PthreadMutex>(std::uintptr_t{2});
}

PthreadMutex adaptiveInitializer() {
    return reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
}

PthreadCond resolveLocked(PthreadCond* cond) {
    if (!cond)
        throw std::invalid_argument("Condition variable pointer is null");
    std::atomic_ref<PthreadCond> slot(*cond);
    auto current = slot.load(std::memory_order_acquire);
    if (current == destroyedCond())
        return nullptr;
    if (!current) {
        current = new PthreadCondPrivate();
        slot.store(current, std::memory_order_release);
    }
    return current;
}

std::shared_ptr<TimedWait::Condition> resolveCond(PthreadCond* cond, int* clockId = nullptr) {
    std::lock_guard lock(InitializationMutex());
    const auto current = resolveLocked(cond);
    if (!current)
        return nullptr;
    if (clockId) *clockId = current->_clockid;
    return current->_cv;
}

std::mutex& WaiterMutex() {
    static std::mutex mutex;
    return mutex;
}

std::multimap<const TimedWait::Condition*, Pthread>& Waiters() {
    static std::multimap<const TimedWait::Condition*, Pthread> waiters;
    return waiters;
}

std::multimap<const TimedWait::Condition*, Pthread>::iterator AddWaiter(const TimedWait::Condition* condition) {
    std::lock_guard lock(WaiterMutex());
    return Waiters().emplace(condition, scePthreadSelf());
}

void RemoveWaiter(std::multimap<const TimedWait::Condition*, Pthread>::iterator waiter) {
    std::lock_guard lock(WaiterMutex());
    Waiters().erase(waiter);
}

bool IsWaiting(const TimedWait::Condition* condition, Pthread thread) {
    std::lock_guard lock(WaiterMutex());
    const auto [first, last] = Waiters().equal_range(condition);
    for (auto waiter = first; waiter != last; ++waiter)
        if (waiter->second == thread) return true;
    return false;
}

int mutexOwnership(PthreadMutex* mutex, PthreadMutex* owned) {
    if (!mutex)
        throw std::invalid_argument("Mutex pointer is null");
    const auto current = std::atomic_ref<PthreadMutex>(*mutex).load(std::memory_order_acquire);
    if (current == destroyedMutex())
        return SCE_KERNEL_ERROR_EINVAL;
    if (!current || current == adaptiveInitializer() || current->_owner.load(std::memory_order_acquire) != std::this_thread::get_id())
        return SCE_KERNEL_ERROR_EPERM;
    *owned = current;
    return 0;
}

template <class TNative>
struct WakingLock {
    PthreadMutex mutex;
    TNative& native;
    void lock() { native.lock(); }
    void unlock() {
        native.unlock();
#ifdef _WIN32
        TimedWait::Unpark(mutex);
#endif
    }
};

int finishWait(std::shared_ptr<TimedWait::Condition>& condition, bool timedOut) {
    if (ThreadCancel::Requested()) {
        condition->NotifyOne();
        condition.reset();
        scePthreadExit(GUEST_CANCELED);
    }
    return timedOut ? sceTimedOut : 0;
}

int waitUntil(std::shared_ptr<TimedWait::Condition> condition, PthreadMutex* mutex, std::optional<std::uint64_t> deadlineNanos, const void* caller) {
    if (!condition)
        return SCE_KERNEL_ERROR_EINVAL;
    PthreadMutex m = nullptr;
    if (const int error = mutexOwnership(mutex, &m))
        return error;
    bool timedOut = false;
    const auto waitStart = std::chrono::steady_clock::now();
    struct Trace {
        const void* caller; const bool& timedOut; std::chrono::steady_clock::time_point start;
        ~Trace() { KernelTraceWait_nid_postfix("cond", caller, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()), timedOut); }
    } trace{caller, timedOut, waitStart};
    if (m->_type == MutexType::Recursive) {
        WakingLock<std::recursive_timed_mutex> waking{m, m->_rmtx};
        std::unique_lock lock(waking, std::adopt_lock);
        const auto previousCount = m->_count;
        m->_count = 0;
        m->_owner.store(std::thread::id{}, std::memory_order_release);
        for (int level = 1; level < previousCount; ++level)
            m->_rmtx.unlock();
        const auto waiter = AddWaiter(condition.get());
        timedOut = !ThreadCancel::Wait(*condition, lock, deadlineNanos);
        RemoveWaiter(waiter);
        for (int level = 1; level < previousCount; ++level)
            m->_rmtx.lock();
        m->_owner.store(std::this_thread::get_id(), std::memory_order_release);
        m->_count = previousCount;
        lock.release();
        return finishWait(condition, timedOut);
    }
    WakingLock<std::timed_mutex> waking{m, m->_mtx};
    std::unique_lock lock(waking, std::adopt_lock);
    m->_owner.store(std::thread::id{}, std::memory_order_release);
    const auto waiter = AddWaiter(condition.get());
    timedOut = !ThreadCancel::Wait(*condition, lock, deadlineNanos);
    RemoveWaiter(waiter);
    m->_owner.store(std::this_thread::get_id(), std::memory_order_release);
    lock.release();
    return finishWait(condition, timedOut);
}

}

int CondOperations::AbsoluteTimedwait(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
    KernelUseconds usec = 0;
    int clockId = 0;
    auto condition = resolveCond(cond, &clockId);
    if (!condition)
        return SCE_KERNEL_ERROR_EINVAL;
    if (!PosixThread::RelativeMicroseconds(clockId, abstime, &usec))
        throw std::invalid_argument("Invalid absolute condition variable timeout");
    return waitUntil(std::move(condition), mutex, TimedWait::DeadlineNanos(usec), __builtin_return_address(0));
}

int CondOperations::SetName(PthreadCond* cond, const char* name) {
    if (!name)
        throw std::invalid_argument("Condition variable name is null");
    std::lock_guard lock(InitializationMutex());
    const auto current = resolveLocked(cond);
    if (!current)
        return SCE_KERNEL_ERROR_EINVAL;
    return 0;
}

extern "C" {

int APS5_VABI scePthreadCondattrInit(PthreadCondattr* attr) {
    if (!attr)
        throw std::invalid_argument("Condition attribute pointer is null");
    *attr = new PthreadCondattrPrivate{0};
    return 0;
}

int APS5_VABI scePthreadCondattrDestroy(PthreadCondattr* attr) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    delete *attr;
    *attr = nullptr;
    return 0;
}

int APS5_VABI scePthreadCondattrSetclock(PthreadCondattr* attr, KernelClockid clockId) {
    constexpr KernelClockid realtime = 0, virtualTime = 1, profiling = 2, monotonic = 4;
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    if (clockId != realtime && clockId != virtualTime && clockId != profiling && clockId != monotonic) return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->_clockid = static_cast<int>(clockId);
    return 0;
}

int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char*) {
    if (!cond)
        throw std::invalid_argument("Condition variable pointer is null");
    auto replacement = std::make_unique<PthreadCondPrivate>();
    if (attr && *attr)
        replacement->_clockid = (*attr)->_clockid;
    std::lock_guard lock(InitializationMutex());
    std::atomic_ref<PthreadCond>(*cond).store(replacement.release(), std::memory_order_release);
    return 0;
}

int APS5_VABI scePthreadCondDestroy(PthreadCond* cond) {
    if (!cond)
        throw std::invalid_argument("Condition variable pointer is null");
    std::lock_guard lock(InitializationMutex());
    if (*cond == destroyedCond())
        return SCE_KERNEL_ERROR_EINVAL;
    if (*cond == nullptr)
        return 0;
    delete *cond;
    std::atomic_ref<PthreadCond>(*cond).store(destroyedCond(), std::memory_order_release);
    return 0;
}

int APS5_VABI scePthreadCondSignal(PthreadCond* cond) {
    const auto resolved = resolveCond(cond);
    if (!resolved)
        return SCE_KERNEL_ERROR_EINVAL;
    resolved->NotifyOne();
    return 0;
}

int APS5_VABI scePthreadCondBroadcast(PthreadCond* cond) {
    const auto resolved = resolveCond(cond);
    if (!resolved)
        return SCE_KERNEL_ERROR_EINVAL;
    resolved->NotifyAll();
    return 0;
}

int APS5_VABI scePthreadCondSignalto(PthreadCond* cond, Pthread thread) {
    const auto resolved = resolveCond(cond);
    if (!resolved || !thread)
        return SCE_KERNEL_ERROR_EINVAL;
    if (!IsWaiting(resolved.get(), thread))
        return SCE_KERNEL_ERROR_EPERM;
    resolved->NotifyAll();
    return 0;
}

int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex) {
    return waitUntil(resolveCond(cond), mutex, std::nullopt, __builtin_return_address(0));
}

int APS5_VABI scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, KernelUseconds usec) {
    return waitUntil(resolveCond(cond), mutex, TimedWait::DeadlineNanos(usec), __builtin_return_address(0));
}

}
