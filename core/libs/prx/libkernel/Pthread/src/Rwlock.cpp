#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Rwlock.hpp"
#include "../include/Pthread.hpp"
#include "../include/Rwlock.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EPERM = 0x80020001;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;
static constexpr int SCE_KERNEL_ERROR_EDEADLK = 0x8002000B;
static constexpr int SCE_KERNEL_ERROR_EBUSY = 0x80020010;
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = 0x8002003C;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
static constexpr int FIRST_STORED_RWLOCK_TYPE = 1;
static constexpr int LAST_STORED_RWLOCK_TYPE = 2;
static constexpr int POSIX_EINVAL = 22;
static constexpr int POSIX_EAGAIN = 35;

static PthreadRwlockPrivate* const DestroyedRwlock = reinterpret_cast<PthreadRwlockPrivate*>(std::uintptr_t{1});

static PthreadRwlockPrivate* RequireRwlock(PthreadRwlock* rwlock, const char* funcName) {
    if (!rwlock) throw std::runtime_error(std::string(funcName) + ": null rwlock");
    std::atomic_ref<PthreadRwlock> slot(*rwlock);
    PthreadRwlock current = slot.load(std::memory_order_acquire);
    if (current == nullptr) {
        auto* created = new PthreadRwlockPrivate();
        if (slot.compare_exchange_strong(current, created, std::memory_order_acq_rel, std::memory_order_acquire))
            current = created;
        else
            delete created;
    }
    return current == DestroyedRwlock ? nullptr : current;
}

static bool OwnsWrite(const PthreadRwlockPrivate* lock) {
    return lock->_writer.load(std::memory_order_acquire) == std::this_thread::get_id();
}

static thread_local int readLocksHeld = 0;

static bool ReaderMayEnter(const PthreadRwlockPrivate* lock) {
    return lock->_writer.load(std::memory_order_acquire) == std::thread::id{} && (lock->_waitingWriters == 0 || readLocksHeld != 0);
}

static bool WriterMayEnter(const PthreadRwlockPrivate* lock) {
    return lock->_writer.load(std::memory_order_acquire) == std::thread::id{} && lock->_readers.load(std::memory_order_acquire) == 0;
}

static int EnterRead(PthreadRwlockPrivate* lock) {
    lock->_readers.fetch_add(1, std::memory_order_release);
    ++readLocksHeld;
    return SCE_OK;
}

static int EnterWrite(PthreadRwlockPrivate* lock) {
    lock->_writer.store(std::this_thread::get_id(), std::memory_order_release);
    return SCE_OK;
}

extern "C" {

int APS5_VABI scePthreadRwlockDestroy(PthreadRwlock* rwlock) {
    if (!rwlock) throw std::runtime_error("scePthreadRwlockDestroy: null rwlock");
    std::atomic_ref<PthreadRwlock> slot(*rwlock);
    PthreadRwlock lock = slot.load(std::memory_order_acquire);
    do {
        if (lock == nullptr) return SCE_OK;
        if (lock == DestroyedRwlock) return SCE_KERNEL_ERROR_EINVAL;
    } while (!slot.compare_exchange_weak(lock, DestroyedRwlock, std::memory_order_acq_rel, std::memory_order_acquire));
    if (lock->_writer.load(std::memory_order_acquire) == std::thread::id{} &&
        lock->_readers.load(std::memory_order_acquire) == 0)
        delete lock;
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
    if (!lock) return SCE_KERNEL_ERROR_EINVAL;
    if (OwnsWrite(lock)) return SCE_KERNEL_ERROR_EDEADLK;
    std::unique_lock state(lock->_state);
    lock->_changed.Wait(state, [&] { return ReaderMayEnter(lock); });
    return EnterRead(lock);
}

int APS5_VABI scePthreadRwlockTryrdlock(PthreadRwlock* rwlock) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (!lock) return SCE_KERNEL_ERROR_EINVAL;
    if (OwnsWrite(lock)) return SCE_KERNEL_ERROR_EBUSY;
    std::lock_guard state(lock->_state);
    return ReaderMayEnter(lock) ? EnterRead(lock) : SCE_KERNEL_ERROR_EBUSY;
}

int APS5_VABI scePthreadRwlockTimedrdlock(PthreadRwlock* rwlock, KernelUseconds usec) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (!lock) return SCE_KERNEL_ERROR_EINVAL;
    const std::uint64_t deadline = TimedWait::DeadlineNanos(usec);
    if (OwnsWrite(lock)) {
        TimedWait::SleepUntil(deadline);
        return SCE_KERNEL_ERROR_ETIMEDOUT;
    }
    std::unique_lock state(lock->_state);
    if (!lock->_changed.WaitUntil(state, deadline, [&] { return ReaderMayEnter(lock); })) return SCE_KERNEL_ERROR_ETIMEDOUT;
    return EnterRead(lock);
}

int APS5_VABI scePthreadRwlockWrlock(PthreadRwlock* rwlock) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (!lock) return SCE_KERNEL_ERROR_EINVAL;
    if (OwnsWrite(lock)) return SCE_KERNEL_ERROR_EDEADLK;
    std::unique_lock state(lock->_state);
    ++lock->_waitingWriters;
    lock->_changed.Wait(state, [&] { return WriterMayEnter(lock); });
    --lock->_waitingWriters;
    return EnterWrite(lock);
}

int APS5_VABI scePthreadRwlockTrywrlock(PthreadRwlock* rwlock) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (!lock) return SCE_KERNEL_ERROR_EINVAL;
    if (OwnsWrite(lock)) return SCE_KERNEL_ERROR_EBUSY;
    std::lock_guard state(lock->_state);
    return WriterMayEnter(lock) ? EnterWrite(lock) : SCE_KERNEL_ERROR_EBUSY;
}

int APS5_VABI scePthreadRwlockTimedwrlock(PthreadRwlock* rwlock, KernelUseconds usec) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (!lock) return SCE_KERNEL_ERROR_EINVAL;
    const std::uint64_t deadline = TimedWait::DeadlineNanos(usec);
    if (OwnsWrite(lock)) {
        TimedWait::SleepUntil(deadline);
        return SCE_KERNEL_ERROR_ETIMEDOUT;
    }
    std::unique_lock state(lock->_state);
    ++lock->_waitingWriters;
    const bool acquired = lock->_changed.WaitUntil(state, deadline, [&] { return WriterMayEnter(lock); });
    --lock->_waitingWriters;
    if (acquired) return EnterWrite(lock);
    lock->_changed.NotifyAll();
    return SCE_KERNEL_ERROR_ETIMEDOUT;
}

int APS5_VABI scePthreadRwlockUnlock(PthreadRwlock* rwlock) {
    auto* lock = RequireRwlock(rwlock, __func__);
    if (!lock) return SCE_KERNEL_ERROR_EINVAL;
    std::unique_lock state(lock->_state);
    if (OwnsWrite(lock)) {
        lock->_writer.store(std::thread::id{}, std::memory_order_release);
    } else {
        if (lock->_writer.load(std::memory_order_acquire) != std::thread::id{}) return SCE_KERNEL_ERROR_EPERM;
        if (lock->_readers.load(std::memory_order_acquire) == 0) return SCE_KERNEL_ERROR_EPERM;
        if (readLocksHeld != 0) --readLocksHeld;
        if (lock->_readers.fetch_sub(1, std::memory_order_acq_rel) != 1) return SCE_OK;
    }
    lock->_changed.NotifyAll();
    return SCE_OK;
}

int APS5_VABI scePthreadRwlockattrDestroy(PthreadRwlockattr* attr) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadRwlockattrDestroy: null attr");
    if (*attr == DESTROYED_RWLOCKATTR) throw std::runtime_error("scePthreadRwlockattrDestroy: destroyed attr");
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
    if (!attr || !*attr || *attr == DESTROYED_RWLOCKATTR || type < FIRST_STORED_RWLOCK_TYPE || type > LAST_STORED_RWLOCK_TYPE) return SCE_KERNEL_ERROR_EINVAL;
    (*attr)->type = type;
    return SCE_OK;
}

int APS5_VABI pthread_rwlock_setname_np_nid_postfix(PthreadRwlock* rwlock, const char* name) {
    if (!rwlock) throw std::runtime_error("pthread_rwlock_setname_np: null rwlock");
    PthreadRwlock current = std::atomic_ref<PthreadRwlock>(*rwlock).load(std::memory_order_acquire);
    if (current == nullptr) throw std::runtime_error("pthread_rwlock_setname_np: statically initialized rwlock");
    if (current == DestroyedRwlock) throw std::runtime_error("pthread_rwlock_setname_np: destroyed rwlock");
    std::lock_guard state(current->_state);
    if (current->_named) return POSIX_EAGAIN;
    if (!name) return POSIX_EINVAL;
    current->_name = name;
    current->_named = true;
    return SCE_OK;
}

}
