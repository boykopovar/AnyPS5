#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"
#include "../include/Rwlock.hpp"
#include <atomic>
#include <stdexcept>
#include <string>

namespace {

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

int APS5_VABI pthread_rwlock_destroy_nid_postfix(PthreadRwlock* rwlock) {
    if (!rwlock) throw std::runtime_error("pthread_rwlock_destroy: null rwlock");
    if (*rwlock == nullptr) return 0;
    return toPosix(scePthreadRwlockDestroy(rwlock));
}

int APS5_VABI pthread_rwlock_init_nid_postfix(PthreadRwlock* rwlock, const PthreadRwlockattr* attr) {
    return toPosix(scePthreadRwlockInit(rwlock, attr, nullptr));
}

int APS5_VABI pthread_rwlock_rdlock_nid_postfix(PthreadRwlock* rwlock) {
    initializeStatic(rwlock, __func__);
    return toPosix(scePthreadRwlockRdlock(rwlock));
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
