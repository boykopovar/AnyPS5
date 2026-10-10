#include "../include/Pthread.hpp"
#include "../include/Rwlock.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <chrono>
#include <cstdint>
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
    auto* lock = RequireRwlock(rwlock, __func__);
    if (lock->_writer.load(std::memory_order_acquire) != std::thread::id{} ||
        lock->_readers.load(std::memory_order_acquire) != 0)
        return SCE_KERNEL_ERROR_EBUSY;
    delete lock;
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
#ifdef _WIN32
    while (!lock->_lock.try_lock_shared()) TimedWait::SleepNanos(1000000);
#else
    lock->_lock.lock_shared();
#endif
    lock->_readers.fetch_add(1, std::memory_order_release);
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockTryrdlock(PthreadRwlock* rwlock) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (OwnsWrite(lock)) return SCE_KERNEL_ERROR_EBUSY;
    if (!lock->_lock.try_lock_shared()) return SCE_KERNEL_ERROR_EBUSY;
    lock->_readers.fetch_add(1, std::memory_order_release);
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockTimedrdlock(PthreadRwlock* rwlock, KernelUseconds usec) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (OwnsWrite(lock)) return SCE_KERNEL_ERROR_EDEADLK;
    const bool locked = TimedWait::AcquireUntil(TimedWait::DeadlineNanos(usec), [&] { return lock->_lock.try_lock_shared(); }, [&](std::uint64_t micros) { return lock->_lock.try_lock_shared_for(std::chrono::microseconds(micros)); });
    if (!locked) return SCE_KERNEL_ERROR_ETIMEDOUT;
    lock->_readers.fetch_add(1, std::memory_order_release);
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockWrlock(PthreadRwlock* rwlock) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (OwnsWrite(lock)) return SCE_KERNEL_ERROR_EDEADLK;
#ifdef _WIN32
    while (!lock->_lock.try_lock()) TimedWait::SleepNanos(1000000);
#else
    lock->_lock.lock();
#endif
    lock->_writer.store(std::this_thread::get_id(), std::memory_order_release);
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockTrywrlock(PthreadRwlock* rwlock) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (OwnsWrite(lock) || !lock->_lock.try_lock()) return SCE_KERNEL_ERROR_EBUSY;
    lock->_writer.store(std::this_thread::get_id(), std::memory_order_release);
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockTimedwrlock(PthreadRwlock* rwlock, KernelUseconds usec) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (OwnsWrite(lock)) return SCE_KERNEL_ERROR_EDEADLK;
    const bool locked = TimedWait::AcquireUntil(TimedWait::DeadlineNanos(usec), [&] { return lock->_lock.try_lock(); }, [&](std::uint64_t micros) { return lock->_lock.try_lock_for(std::chrono::microseconds(micros)); });
    if (!locked) return SCE_KERNEL_ERROR_ETIMEDOUT;
    lock->_writer.store(std::this_thread::get_id(), std::memory_order_release);
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockUnlock(PthreadRwlock* rwlock) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (OwnsWrite(lock)) {
        lock->_writer.store(std::thread::id{}, std::memory_order_release);
        lock->_lock.unlock();
    } else {
        lock->_readers.fetch_sub(1, std::memory_order_release);
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
