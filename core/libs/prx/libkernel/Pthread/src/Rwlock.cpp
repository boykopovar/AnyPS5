#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <chrono>
#include <stdexcept>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;
static constexpr int SCE_KERNEL_ERROR_EDEADLK = 0x8002000B;
static constexpr int SCE_KERNEL_ERROR_EBUSY = 0x80020010;
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = 0x8002003C;

static PthreadRwlockPrivate* RequireRwlock(PthreadRwlock* rwlock, const char* funcName) {
    if (!rwlock || !*rwlock) throw std::runtime_error(std::string(funcName) + ": null rwlock");
    return *rwlock;
}

static bool OwnsWrite(const PthreadRwlockPrivate* lock) {
    return lock->_writer.load(std::memory_order_acquire) == std::this_thread::get_id();
}

extern "C" {

int APS5_VABI scePthreadRwlockDestroy(PthreadRwlock* rwlock) {
    delete RequireRwlock(rwlock, __func__);
    *rwlock = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockInit(PthreadRwlock* rwlock, const PthreadRwlockattr* attr, const char* name) {
    (void)attr;
    (void)name;
    if (!rwlock) throw std::runtime_error("scePthreadRwlockInit: null rwlock");
    auto* p = new (std::nothrow) PthreadRwlockPrivate();
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    *rwlock = p;
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockRdlock(PthreadRwlock* rwlock) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (OwnsWrite(lock)) return SCE_KERNEL_ERROR_EDEADLK;
    lock->_lock.lock_shared();
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockTryrdlock(PthreadRwlock* rwlock) {
    auto* lock = RequireRwlock(rwlock, __func__);
    return lock->_lock.try_lock_shared() ? SCE_OK : SCE_KERNEL_ERROR_EBUSY;
}

int APS5_VABI scePthreadRwlockTimedrdlock(PthreadRwlock* rwlock, KernelUseconds usec) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (OwnsWrite(lock)) return SCE_KERNEL_ERROR_EDEADLK;
    return lock->_lock.try_lock_shared_for(std::chrono::microseconds(usec)) ? SCE_OK : SCE_KERNEL_ERROR_ETIMEDOUT;
}

int APS5_VABI scePthreadRwlockWrlock(PthreadRwlock* rwlock) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (OwnsWrite(lock)) return SCE_KERNEL_ERROR_EDEADLK;
    lock->_lock.lock();
    lock->_writer.store(std::this_thread::get_id(), std::memory_order_release);
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockTrywrlock(PthreadRwlock* rwlock) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (!lock->_lock.try_lock()) return SCE_KERNEL_ERROR_EBUSY;
    lock->_writer.store(std::this_thread::get_id(), std::memory_order_release);
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockTimedwrlock(PthreadRwlock* rwlock, KernelUseconds usec) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (OwnsWrite(lock)) return SCE_KERNEL_ERROR_EDEADLK;
    if (!lock->_lock.try_lock_for(std::chrono::microseconds(usec))) return SCE_KERNEL_ERROR_ETIMEDOUT;
    lock->_writer.store(std::this_thread::get_id(), std::memory_order_release);
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockUnlock(PthreadRwlock* rwlock) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (OwnsWrite(lock)) {
        lock->_writer.store(std::thread::id{}, std::memory_order_release);
        lock->_lock.unlock();
    } else {
        lock->_lock.unlock_shared();
    }
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockattrDestroy(PthreadRwlockattr* attr) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadRwlockattrDestroy: null attr");
    delete *attr;
    *attr = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockattrInit(PthreadRwlockattr* attr) {
    if (!attr) throw std::runtime_error("scePthreadRwlockattrInit: null attr");
    auto* p = new (std::nothrow) PthreadRwlockattrPrivate{0};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    *attr = p;
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockattrSettype(PthreadRwlockattr* attr, int type) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadRwlockattrSettype: null attr");
    (*attr)->type = type;
    return SCE_OK;
}

}
