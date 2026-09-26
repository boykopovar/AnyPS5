#include "prx/libkernel/Pthread/include/Cond.hpp"
#include <cstdint>
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
    return toPosix(scePthreadCondBroadcast(cond));
}

int APS5_VABI pthread_cond_init_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr) {
    return toPosix(scePthreadCondInit(cond, attr, nullptr));
}

int APS5_VABI pthread_cond_signal_nid_postfix(PthreadCond* cond) {
    return toPosix(scePthreadCondSignal(cond));
}

int APS5_VABI pthread_cond_timedwait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
    return toPosix(CondOperations::AbsoluteTimedwait(cond, mutex, abstime));
}

int APS5_VABI pthread_cond_wait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex) {
    return toPosix(scePthreadCondWait(cond, mutex));
}

int APS5_VABI pthread_condattr_destroy_nid_postfix(PthreadCondattr* attr) {
    return toPosix(scePthreadCondattrDestroy(attr));
}

int APS5_VABI pthread_condattr_init_nid_postfix(PthreadCondattr* attr) {
    return toPosix(scePthreadCondattrInit(attr));
}

int APS5_VABI pthread_condattr_setclock_nid_postfix(PthreadCondattr* attr, KernelClockid clock_id) {
    return toPosix(scePthreadCondattrSetclock(attr, clock_id));
}

}
