#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/Posix/Common.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

namespace {

constexpr int sceTimedOut = static_cast<int>(0x8002003cu);
std::mutex condInitializationMutex;

PthreadCond destroyedCond() {
    return reinterpret_cast<PthreadCond>(std::uintptr_t{2});
}

PthreadCond resolveCond(PthreadCond* cond) {
    if (!cond)
        throw std::invalid_argument("Condition variable pointer is null");
    std::atomic_ref<PthreadCond> slot(*cond);
    if (const auto current = slot.load(std::memory_order_acquire); current && current != destroyedCond())
        return current;
    std::lock_guard lock(condInitializationMutex);
    const auto current = slot.load(std::memory_order_acquire);
    if (current == destroyedCond())
        throw std::runtime_error("Condition variable has been destroyed");
    if (current)
        return current;
    auto* created = new PthreadCondPrivate();
    slot.store(created, std::memory_order_release);
    return created;
}

PthreadMutex lockedMutex(PthreadMutex* mutex) {
    if (!mutex || !*mutex)
        throw std::invalid_argument("Mutex pointer is null");
    auto* current = *mutex;
    if (current->_owner.load(std::memory_order_acquire) != std::this_thread::get_id())
        throw std::runtime_error("Condition wait mutex is not owned by the current thread");
    return current;
}

int waitUntil(PthreadCond* cond, PthreadMutex* mutex, std::optional<std::uint64_t> deadlineNanos, const void* caller) {
    auto* c = resolveCond(cond);
    auto* m = lockedMutex(mutex);
    bool timedOut = false;
    const auto waitStart = std::chrono::steady_clock::now();
    struct Trace {
        const void* caller; const bool& timedOut; std::chrono::steady_clock::time_point start;
        ~Trace() { KernelTraceWait_nid_postfix("cond", caller, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()), timedOut); }
    } trace{caller, timedOut, waitStart};
    if (m->_type == MutexType::Recursive) {
        std::unique_lock<std::recursive_timed_mutex> lock(m->_rmtx, std::adopt_lock);
        const auto previousCount = m->_count;
        m->_count = 0;
        m->_owner.store(std::thread::id{}, std::memory_order_release);
        for (int level = 1; level < previousCount; ++level)
            m->_rmtx.unlock();
        timedOut = !ThreadCancel::Wait(c->_cv, lock, deadlineNanos);
        for (int level = 1; level < previousCount; ++level)
            m->_rmtx.lock();
        m->_owner.store(std::this_thread::get_id(), std::memory_order_release);
        m->_count = previousCount;
        lock.release();
        ThreadCancel::Check();
        return timedOut ? sceTimedOut : 0;
    }
    std::unique_lock<std::timed_mutex> lock(m->_mtx, std::adopt_lock);
    m->_owner.store(std::thread::id{}, std::memory_order_release);
    timedOut = !ThreadCancel::Wait(c->_cv, lock, deadlineNanos);
    m->_owner.store(std::this_thread::get_id(), std::memory_order_release);
    lock.release();
    ThreadCancel::Check();
    return timedOut ? sceTimedOut : 0;
}

}

int CondOperations::AbsoluteTimedwait(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
    KernelUseconds usec = 0;
    if (!PosixThread::RelativeMicroseconds(resolveCond(cond)->_clockid, abstime, &usec))
        throw std::invalid_argument("Invalid absolute condition variable timeout");
    return waitUntil(cond, mutex, TimedWait::DeadlineNanos(usec), __builtin_return_address(0));
}

extern "C" {

int APS5_VABI scePthreadCondattrInit(PthreadCondattr* attr) {
    if (!attr)
        throw std::invalid_argument("Condition attribute pointer is null");
    *attr = new PthreadCondattrPrivate{0};
    return 0;
}

int APS5_VABI scePthreadCondattrDestroy(PthreadCondattr* attr) {
    if (!attr || !*attr)
        throw std::invalid_argument("Condition attributes are not initialized");
    delete *attr;
    *attr = nullptr;
    return 0;
}

int APS5_VABI scePthreadCondattrSetclock(PthreadCondattr* attr, KernelClockid clockId) {
    if (!attr || !*attr)
        throw std::invalid_argument("Condition attributes are not initialized");
    (*attr)->_clockid = static_cast<int>(clockId);
    return 0;
}

int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char*) {
    if (!cond)
        throw std::invalid_argument("Condition variable pointer is null");
    if (attr && !*attr)
        throw std::invalid_argument("Condition attributes are not initialized");
    auto replacement = std::make_unique<PthreadCondPrivate>();
    if (attr)
        replacement->_clockid = (*attr)->_clockid;
    std::lock_guard lock(condInitializationMutex);
    std::atomic_ref<PthreadCond>(*cond).store(replacement.release(), std::memory_order_release);
    return 0;
}

int APS5_VABI scePthreadCondDestroy(PthreadCond* cond) {
    if (!cond)
        throw std::invalid_argument("Condition variable pointer is null");
    std::lock_guard lock(condInitializationMutex);
    if (*cond == destroyedCond())
        throw std::runtime_error("Condition variable has already been destroyed");
    delete *cond;
    std::atomic_ref<PthreadCond>(*cond).store(destroyedCond(), std::memory_order_release);
    return 0;
}

int APS5_VABI scePthreadCondSignal(PthreadCond* cond) {
    resolveCond(cond)->_cv.NotifyOne();
    return 0;
}

int APS5_VABI scePthreadCondBroadcast(PthreadCond* cond) {
    resolveCond(cond)->_cv.NotifyAll();
    return 0;
}

int APS5_VABI scePthreadCondSignalto(PthreadCond* cond, Pthread thread) {
    (void)thread;
    resolveCond(cond)->_cv.NotifyAll();
    return 0;
}

int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex) {
    return waitUntil(cond, mutex, std::nullopt, __builtin_return_address(0));
}

int APS5_VABI scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, KernelUseconds usec) {
    return waitUntil(cond, mutex, TimedWait::DeadlineNanos(usec), __builtin_return_address(0));
}

}
