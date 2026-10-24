#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"
#include "../include/Rwlock.hpp"
#include "Common.hpp"
#include <atomic>
#include <stdexcept>
#include <string>

namespace {

constexpr int GUEST_REALTIME_CLOCK = 0;
constexpr int PROCESS_PRIVATE = 0;

int toPosix(int result) {
    if (result == 0)
        return 0;
    const auto error = static_cast<std::uint32_t>(result);
    if ((error & 0xffff0000u) != 0x80020000u)
        throw std::runtime_error("Unexpected SCE rwlock error");
    return static_cast<int>(error & 0xffffu);
}

void initializeStatic(PthreadRwlock* rwlock, const char* funcName) {
    if (!rwlock) throw std::runtime_error(std::string(funcName) + ": null rwlock");
    std::atomic_ref<PthreadRwlock> slot(*rwlock);
    PthreadRwlock current = slot.load(std::memory_order_acquire);
    if (current != nullptr) return;
    auto* created = new PthreadRwlockPrivate();
    if (!slot.compare_exchange_strong(current, created, std::memory_order_acq_rel, std::memory_order_acquire))
        delete created;
}

}

extern "C" {

int APS5_VABI pthread_rwlock_wrlock_nid_postfix(PthreadRwlock* rwlock);

int APS5_VABI pthread_rwlock_destroy_nid_postfix(PthreadRwlock* rwlock) {
    if (!rwlock) throw std::runtime_error("pthread_rwlock_destroy: null rwlock");
    if (*rwlock == nullptr) return 0;
    return toPosix(scePthreadRwlockDestroy(rwlock));
}

int APS5_VABI pthread_rwlock_init_nid_postfix(PthreadRwlock* rwlock, const PthreadRwlockattr* attr) {
    return toPosix(scePthreadRwlockInit(rwlock, attr, nullptr));
}

int APS5_VABI pthread_rwlockattr_destroy_nid_postfix(PthreadRwlockattr* attr) {
    return toPosix(scePthreadRwlockattrDestroy(attr));
}

int APS5_VABI pthread_rwlockattr_init_nid_postfix(PthreadRwlockattr* attr) {
    return toPosix(scePthreadRwlockattrInit(attr));
}

int APS5_VABI pthread_rwlockattr_getpshared_nid_postfix(const PthreadRwlockattr* attr, int* pshared) {
    if (!attr || !*attr) throw std::runtime_error("pthread_rwlockattr_getpshared: null attr");
    if (!pshared) throw std::runtime_error("pthread_rwlockattr_getpshared: null pshared");
    *pshared = PROCESS_PRIVATE;
    return 0;
}

int APS5_VABI pthread_rwlockattr_setpshared_nid_postfix(PthreadRwlockattr* attr, int pshared) {
    if (pshared != PROCESS_PRIVATE) return PosixThread::GUEST_EINVAL;
    if (!attr || !*attr) throw std::runtime_error("pthread_rwlockattr_setpshared: null attr");
    return 0;
}

int APS5_VABI pthread_rwlock_rdlock_nid_postfix(PthreadRwlock* rwlock) {
    initializeStatic(rwlock, __func__);
    return toPosix(scePthreadRwlockRdlock(rwlock));
}

int APS5_VABI pthread_rwlock_timedrdlock_nid_postfix(PthreadRwlock* rwlock, const KernelTimespec* abstime) {
    if (!abstime) return pthread_rwlock_rdlock_nid_postfix(rwlock);
    initializeStatic(rwlock, __func__);
    if (scePthreadRwlockTryrdlock(rwlock) == 0) return 0;
    KernelUseconds usec = 0;
    if (!PosixThread::RelativeMicroseconds(GUEST_REALTIME_CLOCK, abstime, &usec)) return PosixThread::GUEST_EINVAL;
    return toPosix(scePthreadRwlockTimedrdlock(rwlock, usec));
}

int APS5_VABI pthread_rwlock_timedwrlock_nid_postfix(PthreadRwlock* rwlock, const KernelTimespec* abstime) {
    if (!abstime) return pthread_rwlock_wrlock_nid_postfix(rwlock);
    initializeStatic(rwlock, __func__);
    if (scePthreadRwlockTrywrlock(rwlock) == 0) return 0;
    KernelUseconds usec = 0;
    if (!PosixThread::RelativeMicroseconds(GUEST_REALTIME_CLOCK, abstime, &usec)) return PosixThread::GUEST_EINVAL;
    return toPosix(scePthreadRwlockTimedwrlock(rwlock, usec));
}

int APS5_VABI pthread_rwlock_reltimedrdlock_np_nid_postfix(PthreadRwlock* rwlock, KernelUseconds usec) {
    initializeStatic(rwlock, __func__);
    return toPosix(scePthreadRwlockTimedrdlock(rwlock, usec));
}

int APS5_VABI pthread_rwlock_reltimedwrlock_np_nid_postfix(PthreadRwlock* rwlock, KernelUseconds usec) {
    initializeStatic(rwlock, __func__);
    return toPosix(scePthreadRwlockTimedwrlock(rwlock, usec));
}

int APS5_VABI pthread_rwlock_tryrdlock_nid_postfix(PthreadRwlock* rwlock) {
    initializeStatic(rwlock, __func__);
    return toPosix(scePthreadRwlockTryrdlock(rwlock));
}

int APS5_VABI pthread_rwlock_trywrlock_nid_postfix(PthreadRwlock* rwlock) {
    initializeStatic(rwlock, __func__);
    return toPosix(scePthreadRwlockTrywrlock(rwlock));
}

int APS5_VABI pthread_rwlock_unlock_nid_postfix(PthreadRwlock* rwlock) {
    initializeStatic(rwlock, __func__);
    return toPosix(scePthreadRwlockUnlock(rwlock));
}

int APS5_VABI pthread_rwlock_wrlock_nid_postfix(PthreadRwlock* rwlock) {
    initializeStatic(rwlock, __func__);
    return toPosix(scePthreadRwlockWrlock(rwlock));
}

}
