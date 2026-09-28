#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <chrono>
#include <stdexcept>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = 0x80020062;
static constexpr int SCE_KERNEL_ERROR_EBUSY = 0x80020010;
static constexpr int SCE_KERNEL_ERROR_EPERM = 0x80020001;

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

int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr*, const char*) {
    if (!cond) throw std::runtime_error("scePthreadCondInit: null cond");
    auto* p = new (std::nothrow) PthreadCondPrivate{};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    *cond = p;
    return SCE_OK;
}

int APS5_VABI scePthreadCondDestroy(PthreadCond* cond) {
    if (!cond || !*cond) throw std::runtime_error("scePthreadCondDestroy: null cond");
    if ((*cond)->_waiters.load(std::memory_order_acquire)) return SCE_KERNEL_ERROR_EBUSY;
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
    auto* m = *mutex;
    auto* c = *cond;
    const auto thread = std::this_thread::get_id();
    if (m->_owner.load(std::memory_order_acquire) != thread ||
        (m->_type == MutexType::Recursive && m->_count != 1)) return SCE_KERNEL_ERROR_EPERM;
    c->_waiters.fetch_add(1, std::memory_order_acq_rel);
    if (m->_type == MutexType::Recursive) {
        std::unique_lock<std::recursive_timed_mutex> lk(m->_rmtx, std::adopt_lock);
        auto res = c->_cv.wait_for(lk, std::chrono::microseconds(usec));
        lk.release();
        m->_owner.store(thread, std::memory_order_release);
        c->_waiters.fetch_sub(1, std::memory_order_acq_rel);
        return res == std::cv_status::timeout ? SCE_KERNEL_ERROR_ETIMEDOUT : SCE_OK;
    }
    std::unique_lock<std::timed_mutex> lk(m->_mtx, std::adopt_lock);
    auto res = c->_cv.wait_for(lk, std::chrono::microseconds(usec));
    lk.release();
    m->_owner.store(thread, std::memory_order_release);
    c->_waiters.fetch_sub(1, std::memory_order_acq_rel);
    return res == std::cv_status::timeout ? SCE_KERNEL_ERROR_ETIMEDOUT : SCE_OK;
}

int APS5_VABI scePthreadCondSignalto(PthreadCond* cond, Pthread thread) {
 (void)cond;
 (void)thread;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex) {
 extern int APS5_VABI pthread_cond_wait_nid_postfix(PthreadCond*, PthreadMutex*);
 const int error = pthread_cond_wait_nid_postfix(cond, mutex);
 return error ? static_cast<int>(0x80020000u | static_cast<unsigned>(error)) : 0;
}

}
