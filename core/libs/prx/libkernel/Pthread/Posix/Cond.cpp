#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "Common.hpp"
#include <cstdint>
#include <cerrno>
#include <new>
#include <stdexcept>

namespace {

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

int APS5_VABI pthread_cond_broadcast_nid_postfix(PthreadCond* cond) {
    if (!cond || !*cond) return PosixThread::GUEST_EINVAL;
    return toPosix(scePthreadCondBroadcast(cond));
}

int APS5_VABI pthread_cond_init_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr) {
    if (!cond || (attr && !*attr)) return PosixThread::GUEST_EINVAL;
    try {
        return toPosix(scePthreadCondInit(cond, attr, nullptr));
    } catch (const std::bad_alloc&) {
        return ENOMEM;
    }
}

int APS5_VABI pthread_cond_destroy_nid_postfix(PthreadCond* cond) {
    if (!cond || !*cond) return PosixThread::GUEST_EINVAL;
    const int result = scePthreadCondDestroy(cond);
    if (result == 0) *cond = nullptr;
    return toPosix(result);
}

int APS5_VABI pthread_cond_signal_nid_postfix(PthreadCond* cond) {
    if (!cond || !*cond) return PosixThread::GUEST_EINVAL;
    return toPosix(scePthreadCondSignal(cond));
}

int APS5_VABI pthread_cond_timedwait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
    if (!abstime || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000) return PosixThread::GUEST_EINVAL;
    if (!cond || !*cond || !mutex || !*mutex) return PosixThread::GUEST_EINVAL;
    return toPosix(CondOperations::AbsoluteTimedwait(cond, mutex, abstime));
}

int APS5_VABI pthread_cond_wait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex) {
    if (!cond || !*cond || !mutex || !*mutex) return PosixThread::GUEST_EINVAL;
    return toPosix(scePthreadCondWait(cond, mutex));
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

}
