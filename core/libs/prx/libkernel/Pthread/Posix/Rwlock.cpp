#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"
#include <atomic>
#include <stdexcept>

extern "C" {
int APS5_VABI scePthreadRwlockInit(PthreadRwlock* rwlock, const PthreadRwlockattr* attr, const char* name);
int APS5_VABI scePthreadRwlockDestroy(PthreadRwlock* rwlock);
int APS5_VABI scePthreadRwlockWrlock(PthreadRwlock* rwlock);
}

namespace {

int _toErrno(int sceResult) {
    return sceResult == 0 ? 0 : static_cast<int>(static_cast<std::uint32_t>(sceResult) & 0xFFFFu);
}

void _initializeStatic(PthreadRwlock* rwlock, const char* funcName) {
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

int APS5_VABI pthread_rwlock_destroy_nid_postfix(PthreadRwlock* rwlock) {
    if (!rwlock) throw std::runtime_error("pthread_rwlock_destroy: null rwlock");
    if (*rwlock == nullptr) return 0;
    return _toErrno(scePthreadRwlockDestroy(rwlock));
}

int APS5_VABI pthread_rwlock_init_nid_postfix(PthreadRwlock* rwlock, const PthreadRwlockattr* attr) {
    return _toErrno(scePthreadRwlockInit(rwlock, attr, nullptr));
}

int APS5_VABI pthread_rwlock_wrlock_nid_postfix(PthreadRwlock* rwlock) {
    _initializeStatic(rwlock, __func__);
    return _toErrno(scePthreadRwlockWrlock(rwlock));
}

}
