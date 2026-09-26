#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include <chrono>
#include <memory>
#include <stdexcept>

namespace {

constexpr int sceTimedOut = static_cast<int>(0x8002003cu);
std::mutex condInitializationMutex;

PthreadCond destroyedCond() {
    return reinterpret_cast<PthreadCond>(std::uintptr_t{2});
}

PthreadCond resolveCond(PthreadCond* cond) {
    if (!cond)
        throw std::invalid_argument("Condition variable pointer is null");
    std::lock_guard lock(condInitializationMutex);
    if (*cond == destroyedCond())
        throw std::runtime_error("Condition variable has been destroyed");
    if (!*cond)
        *cond = new PthreadCondPrivate();
    return *cond;
}

PthreadMutex lockedMutex(PthreadMutex* mutex) {
    if (!mutex || !*mutex)
        throw std::invalid_argument("Mutex pointer is null");
    auto* current = *mutex;
    if (current->_owner.load(std::memory_order_acquire) != std::this_thread::get_id())
        throw std::runtime_error("Condition wait mutex is not owned by the current thread");
    return current;
}

int waitUntil(PthreadCond* cond, PthreadMutex* mutex, const std::chrono::system_clock::time_point* deadline) {
    auto* c = resolveCond(cond);
    auto* m = lockedMutex(mutex);
    if (m->_type == MutexType::Recursive) {
        std::unique_lock<std::recursive_timed_mutex> lock(m->_rmtx, std::adopt_lock);
        const auto previousCount = m->_count;
        m->_count = 0;
        m->_owner.store(std::thread::id{}, std::memory_order_release);
        const bool timedOut = deadline && c->_cv.wait_until(lock, *deadline) == std::cv_status::timeout;
        if (!deadline)
            c->_cv.wait(lock);
        m->_owner.store(std::this_thread::get_id(), std::memory_order_release);
        m->_count = previousCount;
        lock.release();
        return timedOut ? sceTimedOut : 0;
    }
    std::unique_lock<std::timed_mutex> lock(m->_mtx, std::adopt_lock);
    m->_owner.store(std::thread::id{}, std::memory_order_release);
    const bool timedOut = deadline && c->_cv.wait_until(lock, *deadline) == std::cv_status::timeout;
    if (!deadline)
        c->_cv.wait(lock);
    m->_owner.store(std::this_thread::get_id(), std::memory_order_release);
    lock.release();
    return timedOut ? sceTimedOut : 0;
}

std::chrono::system_clock::time_point toDeadline(const KernelTimespec* abstime) {
    if (!abstime || abstime->tv_sec < 0 || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000)
        throw std::invalid_argument("Invalid absolute condition variable timeout");
    const auto maximum = std::chrono::nanoseconds::max().count();
    if (abstime->tv_sec > (maximum - abstime->tv_nsec) / 1000000000)
        throw std::overflow_error("Absolute condition variable timeout exceeds the host clock range");
    const auto duration = std::chrono::nanoseconds(abstime->tv_sec * 1000000000 + abstime->tv_nsec);
    if (std::chrono::duration<long double>(duration) >= std::chrono::duration<long double>(std::chrono::system_clock::duration::max()))
        throw std::overflow_error("Absolute condition variable timeout exceeds the host clock range");
    return std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(duration));
}

}

int CondOperations::AbsoluteTimedwait(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
    const auto deadline = toDeadline(abstime);
    return waitUntil(cond, mutex, &deadline);
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
    std::lock_guard lock(condInitializationMutex);
    *cond = replacement.release();
    return 0;
}

int APS5_VABI scePthreadCondDestroy(PthreadCond* cond) {
    if (!cond)
        throw std::invalid_argument("Condition variable pointer is null");
    std::lock_guard lock(condInitializationMutex);
    if (*cond == destroyedCond())
        throw std::runtime_error("Condition variable has already been destroyed");
    delete *cond;
    *cond = destroyedCond();
    return 0;
}

int APS5_VABI scePthreadCondSignal(PthreadCond* cond) {
    resolveCond(cond)->_cv.notify_one();
    return 0;
}

int APS5_VABI scePthreadCondBroadcast(PthreadCond* cond) {
    resolveCond(cond)->_cv.notify_all();
    return 0;
}

int APS5_VABI scePthreadCondSignalto(PthreadCond* cond, Pthread thread) {
    (void)thread;
    resolveCond(cond)->_cv.notify_all();
    return 0;
}

int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex) {
    return waitUntil(cond, mutex, nullptr);
}

int APS5_VABI scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, KernelUseconds usec) {
    const auto deadline = std::chrono::system_clock::now() + std::chrono::microseconds(usec);
    return waitUntil(cond, mutex, &deadline);
}

}
