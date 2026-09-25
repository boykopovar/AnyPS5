#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <chrono>
#include <optional>
#include <thread>
#include <stdexcept>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = 0x8002003C;

static int WaitOn(PthreadCondPrivate* cond, PthreadMutexPrivate* mutex, std::optional<std::chrono::microseconds> timeout, const void* caller) {
    const auto self = std::this_thread::get_id();
    bool timedOut = false;
    const auto waitStart = std::chrono::steady_clock::now();
    struct Trace {
        const void* caller; const bool& timedOut; std::chrono::steady_clock::time_point start;
        ~Trace() { KernelTraceWait_nid_postfix("cond", caller, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()), timedOut); }
    } trace{caller, timedOut, waitStart};
    mutex->_owner.store(std::thread::id{}, std::memory_order_relaxed);
    if (mutex->_type == MutexType::Recursive) {
        std::unique_lock<std::recursive_timed_mutex> lock(mutex->_rmtx, std::adopt_lock);
        if (timeout) timedOut = cond->_cv.wait_for(lock, *timeout) == std::cv_status::timeout;
        else cond->_cv.wait(lock);
        lock.release();
    } else {
        std::unique_lock<std::timed_mutex> lock(mutex->_mtx, std::adopt_lock);
        if (timeout) timedOut = cond->_cv.wait_for(lock, *timeout) == std::cv_status::timeout;
        else cond->_cv.wait(lock);
        lock.release();
    }
    mutex->_owner.store(self, std::memory_order_relaxed);
    return timedOut ? SCE_KERNEL_ERROR_ETIMEDOUT : SCE_OK;
}

extern "C" {

int APS5_VABI scePthreadCondattrInit(PthreadCondattr* attr) {
    if (!attr) throw std::runtime_error("scePthreadCondattrInit: null attr");
    auto* p = new (std::nothrow) PthreadCondattrPrivate{0};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    *attr = p;
    return SCE_OK;
}

int APS5_VABI scePthreadCondattrDestroy(PthreadCondattr* attr) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadCondattrDestroy: null attr");
    delete *attr;
    *attr = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char*) {
    if (!cond) throw std::runtime_error("scePthreadCondInit: null cond");
    auto* p = new (std::nothrow) PthreadCondPrivate{};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    if (attr && *attr) p->_clockid = (*attr)->_clockid;
    *cond = p;
    return SCE_OK;
}

int APS5_VABI scePthreadCondDestroy(PthreadCond* cond) {
    if (!cond || !*cond) throw std::runtime_error("scePthreadCondDestroy: null cond");
    delete *cond;
    *cond = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadCondSignal(PthreadCond* cond) {
    if (!cond || !*cond) throw std::runtime_error("scePthreadCondSignal: null cond");
    (*cond)->_cv.notify_one();
    return SCE_OK;
}

int APS5_VABI scePthreadCondBroadcast(PthreadCond* cond) {
    if (!cond || !*cond) throw std::runtime_error("scePthreadCondBroadcast: null cond");
    (*cond)->_cv.notify_all();
    return SCE_OK;
}

int APS5_VABI scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, unsigned int usec) {
    if (!cond || !*cond || !mutex || !*mutex)
        throw std::runtime_error("scePthreadCondTimedwait: null arg");
    return WaitOn(*cond, *mutex, std::chrono::microseconds(usec), __builtin_return_address(0));
}

int APS5_VABI scePthreadCondSignalto(PthreadCond* cond, Pthread thread) {
    (void)thread;
    if (!cond || !*cond) throw std::runtime_error("scePthreadCondSignalto: null cond");
    (*cond)->_cv.notify_all();
    return SCE_OK;
}

int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex) {
    if (!cond || !*cond || !mutex || !*mutex)
        throw std::runtime_error("scePthreadCondWait: null arg");
    return WaitOn(*cond, *mutex, std::nullopt, __builtin_return_address(0));
}

}
