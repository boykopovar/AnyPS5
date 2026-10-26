#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "Common.hpp"
#include <cstdint>
#include <stdexcept>

namespace {

constexpr int POSIX_PROCESS_PRIVATE = 0;

int toPosix(int result) {
    if (result == 0)
        return 0;
    const auto error = static_cast<std::uint32_t>(result);
    if ((error & 0xffff0000u) != 0x80020000u)
        throw std::runtime_error("Unexpected SCE condition variable error");
    return static_cast<int>(error & 0xffffu);
}

}

extern "C" {

int APS5_VABI pthread_cond_destroy_nid_postfix(PthreadCond* cond) {
    if (!cond) return PosixThread::GUEST_EINVAL;
    return toPosix(scePthreadCondDestroy(cond));
}

int APS5_VABI pthread_cond_broadcast_nid_postfix(PthreadCond* cond) {
    return toPosix(scePthreadCondBroadcast(cond));
}

int APS5_VABI pthread_cond_init_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr) {
    return toPosix(scePthreadCondInit(cond, attr, nullptr));
}

int APS5_VABI pthread_cond_signal_nid_postfix(PthreadCond* cond) {
    return toPosix(scePthreadCondSignal(cond));
}

int APS5_VABI pthread_cond_timedwait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
    if (!abstime || abstime->tv_sec < 0 || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000) return PosixThread::GUEST_EINVAL;
    return toPosix(CondOperations::AbsoluteTimedwait(cond, mutex, abstime));
}

int APS5_VABI pthread_cond_reltimedwait_np_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, KernelUseconds usec) {
    return toPosix(scePthreadCondTimedwait(cond, mutex, usec));
}

int APS5_VABI pthread_cond_wait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex) {
    return toPosix(scePthreadCondWait(cond, mutex));
}

int APS5_VABI pthread_cond_setname_np_nid_postfix(PthreadCond* cond, const char* name) {
    return toPosix(CondOperations::SetName(cond, name));
}

int APS5_VABI pthread_condattr_destroy_nid_postfix(PthreadCondattr* attr) {
    if (!attr || !*attr) return PosixThread::GUEST_EINVAL;
    return toPosix(scePthreadCondattrDestroy(attr));
}

int APS5_VABI pthread_condattr_init_nid_postfix(PthreadCondattr* attr) {
    if (!attr) return PosixThread::GUEST_EINVAL;
    return toPosix(scePthreadCondattrInit(attr));
}

int APS5_VABI pthread_condattr_setclock_nid_postfix(PthreadCondattr* attr, KernelClockid clock_id) {
    if (!attr || !*attr) return PosixThread::GUEST_EINVAL;
    return toPosix(scePthreadCondattrSetclock(attr, clock_id));
}

int APS5_VABI pthread_condattr_setpshared_nid_postfix(PthreadCondattr* attr, int pshared) {
    if (!attr || !*attr) return PosixThread::GUEST_EINVAL;
    if (pshared != POSIX_PROCESS_PRIVATE) return PosixThread::GUEST_EINVAL;
    (*attr)->_pshared = pshared;
    return 0;
}

int APS5_VABI pthread_condattr_getpshared_nid_postfix(const PthreadCondattr* attr, int* pshared) {
    if (!attr || !*attr) return PosixThread::GUEST_EINVAL;
    if (!pshared) throw std::runtime_error("pthread_condattr_getpshared: null pshared");
    *pshared = (*attr)->_pshared;
    return 0;
}

int APS5_VABI pthread_cond_signalto_np_nid_postfix(PthreadCond* cond, Pthread thread) {
    return toPosix(scePthreadCondSignalto(cond, thread));
}

int APS5_VABI scePthreadCondattrSetpshared(PthreadCondattr* attr, int pshared) {
    return PosixThread::ToSce(pthread_condattr_setpshared_nid_postfix(attr, pshared));
}

int APS5_VABI scePthreadCondattrGetpshared(const PthreadCondattr* attr, int* pshared) {
    return PosixThread::ToSce(pthread_condattr_getpshared_nid_postfix(attr, pshared));
}

}
