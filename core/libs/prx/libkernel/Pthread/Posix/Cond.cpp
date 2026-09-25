#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"
#include "Common.hpp"
#include <atomic>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI scePthreadCondattrInit(PthreadCondattr* attr);
int APS5_VABI scePthreadCondattrDestroy(PthreadCondattr* attr);
int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name);
int APS5_VABI scePthreadCondSignal(PthreadCond* cond);
int APS5_VABI scePthreadCondBroadcast(PthreadCond* cond);
int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex);
int APS5_VABI scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, unsigned int usec);
}

static void InitializeStatic(PthreadCond* cond, const char* funcName) {
    if (!cond) throw std::runtime_error(std::string(funcName) + ": null cond");
    std::atomic_ref<PthreadCond> slot(*cond);
    PthreadCond current = slot.load(std::memory_order_acquire);
    if (current != nullptr) return;
    auto* created = new PthreadCondPrivate();
    if (!slot.compare_exchange_strong(current, created, std::memory_order_acq_rel, std::memory_order_acquire))
        delete created;
}

extern "C" {

int APS5_VABI pthread_cond_broadcast_nid_postfix(PthreadCond* cond) {
    InitializeStatic(cond, __func__);
    return PosixThread::ToErrno(scePthreadCondBroadcast(cond));
}

int APS5_VABI pthread_cond_init_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr) {
    return PosixThread::ToErrno(scePthreadCondInit(cond, attr, nullptr));
}

int APS5_VABI pthread_cond_signal_nid_postfix(PthreadCond* cond) {
    InitializeStatic(cond, __func__);
    return PosixThread::ToErrno(scePthreadCondSignal(cond));
}

int APS5_VABI pthread_cond_timedwait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
    InitializeStatic(cond, __func__);
    KernelUseconds usec = 0;
    if (!PosixThread::RelativeMicroseconds((*cond)->_clockid, abstime, &usec)) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadCondTimedwait(cond, mutex, usec));
}

int APS5_VABI pthread_cond_wait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex) {
    InitializeStatic(cond, __func__);
    return PosixThread::ToErrno(scePthreadCondWait(cond, mutex));
}

int APS5_VABI pthread_condattr_destroy_nid_postfix(PthreadCondattr* attr) {
    if (!attr || !*attr) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadCondattrDestroy(attr));
}

int APS5_VABI pthread_condattr_init_nid_postfix(PthreadCondattr* attr) {
    if (!attr) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadCondattrInit(attr));
}

int APS5_VABI pthread_condattr_setclock_nid_postfix(PthreadCondattr* attr, KernelClockid clock_id) {
    if (!attr || !*attr) return PosixThread::GUEST_EINVAL;
    (*attr)->_clockid = clock_id;
    return 0;
}

}
