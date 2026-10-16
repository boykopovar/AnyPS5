#include "../include/Pthread.hpp"
#include "../include/Cancel.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace {

constexpr int sceBusy = static_cast<int>(0x80020010u);
constexpr int sceTimedOut = static_cast<int>(0x8002003cu);
constexpr int sceSemValueMax = 0x7fffffff;
std::mutex semInitializationMutex;
std::unordered_map<PthreadSemPrivate*, std::shared_ptr<PthreadSemPrivate>> semRegistry;
std::atomic<int> semApiPauseTarget{0};
std::atomic<int> semApiPauseEntered{0};
std::atomic<bool> semApiPauseReleased{true};

void PauseSemApi(int kind) {
    if (semApiPauseTarget.load(std::memory_order_acquire) != kind) return;
    semApiPauseEntered.fetch_add(1, std::memory_order_acq_rel);
    while (!semApiPauseReleased.load(std::memory_order_acquire)) std::this_thread::yield();
}

PthreadSem destroyedSem() {
    return reinterpret_cast<PthreadSem>(std::uintptr_t{2});
}

std::shared_ptr<PthreadSemPrivate> resolveSem(PthreadSem* sem) {
    if (!sem)
        throw std::invalid_argument("Semaphore pointer is null");
    std::lock_guard lock(semInitializationMutex);
    const auto current = std::atomic_ref<PthreadSem>(*sem).load(std::memory_order_acquire);
    if (!current || current == destroyedSem())
        throw std::runtime_error("Semaphore is not initialized or has been destroyed");
    const auto it = semRegistry.find(current);
    if (it == semRegistry.end())
        throw std::runtime_error("Semaphore is not initialized or has been destroyed");
    return it->second;
}

struct SemaphoreWaitCleanupContext {
    std::shared_ptr<PthreadSemPrivate> owner;
    bool waiterRegistered = false;
};

void APS5_VABI CleanupSemaphoreWait(void* argument) {
    auto* context = static_cast<SemaphoreWaitCleanupContext*>(argument);
    if (!context || !context->owner)
        return;
    auto* owner = context->owner.get();
    if (context->waiterRegistered) {
        std::lock_guard lock(owner->_mutex);
        if (owner->_waiters > 0)
            --owner->_waiters;
        context->waiterRegistered = false;
    }
    context->owner.reset();
}

void finishSemaphoreWait(PthreadSemPrivate* sem, SemaphoreWaitCleanupContext& context, void* cleanupMark) {
    if (context.waiterRegistered) {
        --sem->_waiters;
        context.waiterRegistered = false;
    }
    ThreadCancel::CleanupRestore(cleanupMark);
}

}

extern "C" {

int APS5_VABI scePthreadSemInit(PthreadSem* sem, int flag, unsigned int value, const char* name) {
    if (!sem)
        throw std::invalid_argument("Semaphore pointer is null");
    if (value > static_cast<unsigned int>(sceSemValueMax))
        return SCE_KERNEL_ERROR_EINVAL;
    (void)flag;
    (void)name;
    auto replacement = std::make_shared<PthreadSemPrivate>(value);
    std::lock_guard lock(semInitializationMutex);
    semRegistry.emplace(replacement.get(), replacement);
    std::atomic_ref<PthreadSem>(*sem).store(replacement.get(), std::memory_order_release);
    return 0;
}

int APS5_VABI scePthreadSemDestroy(PthreadSem* sem) {
    if (!sem)
        throw std::invalid_argument("Semaphore pointer is null");
    std::lock_guard registryLock(semInitializationMutex);
    const auto current = std::atomic_ref<PthreadSem>(*sem).load(std::memory_order_acquire);
    if (current == destroyedSem())
        return SCE_KERNEL_ERROR_EINVAL;
    if (!current)
        throw std::runtime_error("Semaphore is not initialized");
    const auto it = semRegistry.find(current);
    if (it == semRegistry.end())
        return SCE_KERNEL_ERROR_EINVAL;
    const auto owner = it->second;
    std::lock_guard semLock(owner->_mutex);
    if (owner->_waiters != 0)
        return SCE_KERNEL_ERROR_EBUSY;
    owner->_destroyed = true;
    std::atomic_ref<PthreadSem>(*sem).store(destroyedSem(), std::memory_order_release);
    semRegistry.erase(it);
    return 0;
}

int APS5_VABI scePthreadSemPost(PthreadSem* sem) {
    const auto owner = resolveSem(sem);
    PauseSemApi(1);
    std::lock_guard lock(owner->_mutex);
    if (owner->_destroyed)
        return SCE_KERNEL_ERROR_EINVAL;
    if (owner->_count == sceSemValueMax)
        return SCE_KERNEL_ERROR_EOVERFLOW;
    ++owner->_count;
    owner->_cv.NotifyOne();
    return 0;
}

int APS5_VABI scePthreadSemWait(PthreadSem* sem) {
    ThreadCancel::Check();
    SemaphoreWaitCleanupContext cleanupContext{resolveSem(sem), false};
    auto* current = cleanupContext.owner.get();
    std::unique_lock lock(current->_mutex);
    if (current->_destroyed)
        return SCE_KERNEL_ERROR_EINVAL;
    ++current->_waiters;
    cleanupContext.waiterRegistered = true;
    CleanupHandler cleanup{};
    void* const cleanupMark = ThreadCancel::CleanupMark();
    __pthread_cleanup_push_imp_nid_postfix(CleanupSemaphoreWait, &cleanupContext, &cleanup);
    try {
        ThreadCancel::WaitUntil(current->_cv, lock, std::nullopt, [&] { return current->_count > 0 || current->_destroyed; });
        if (current->_destroyed) {
            finishSemaphoreWait(current, cleanupContext, cleanupMark);
            return SCE_KERNEL_ERROR_EINVAL;
        }
        --current->_count;
        finishSemaphoreWait(current, cleanupContext, cleanupMark);
        return 0;
    } catch (...) {
        if (cleanupContext.waiterRegistered) {
            if (!lock.owns_lock())
                lock.lock();
            --current->_waiters;
            cleanupContext.waiterRegistered = false;
        }
        ThreadCancel::CleanupRestore(cleanupMark);
        throw;
    }
}

int APS5_VABI scePthreadSemTrywait(PthreadSem* sem) {
    const auto owner = resolveSem(sem);
    std::lock_guard lock(owner->_mutex);
    if (owner->_destroyed)
        return SCE_KERNEL_ERROR_EINVAL;
    if (owner->_count <= 0)
        return sceBusy;
    --owner->_count;
    return 0;
}

int APS5_VABI scePthreadSemTimedwait(PthreadSem* sem, KernelUseconds usec) {
    ThreadCancel::Check();
    SemaphoreWaitCleanupContext cleanupContext{resolveSem(sem), false};
    auto* current = cleanupContext.owner.get();
    std::unique_lock lock(current->_mutex);
    if (current->_destroyed)
        return SCE_KERNEL_ERROR_EINVAL;
    ++current->_waiters;
    cleanupContext.waiterRegistered = true;
    CleanupHandler cleanup{};
    void* const cleanupMark = ThreadCancel::CleanupMark();
    __pthread_cleanup_push_imp_nid_postfix(CleanupSemaphoreWait, &cleanupContext, &cleanup);
    try {
        const auto acquired = ThreadCancel::WaitUntil(current->_cv, lock, TimedWait::DeadlineNanos(usec), [&] { return current->_count > 0 || current->_destroyed; });
        if (current->_destroyed) {
            finishSemaphoreWait(current, cleanupContext, cleanupMark);
            return SCE_KERNEL_ERROR_EINVAL;
        }
        if (!acquired) {
            finishSemaphoreWait(current, cleanupContext, cleanupMark);
            return sceTimedOut;
        }
        --current->_count;
        finishSemaphoreWait(current, cleanupContext, cleanupMark);
        return 0;
    } catch (...) {
        if (cleanupContext.waiterRegistered) {
            if (!lock.owns_lock())
                lock.lock();
            --current->_waiters;
            cleanupContext.waiterRegistered = false;
        }
        ThreadCancel::CleanupRestore(cleanupMark);
        throw;
    }
}

int APS5_VABI scePthreadSemGetvalue(PthreadSem* sem, int* value) {
    const auto owner = resolveSem(sem);
    if (!value)
        throw std::invalid_argument("Semaphore value pointer is null");
    std::lock_guard lock(owner->_mutex);
    if (owner->_destroyed)
        return SCE_KERNEL_ERROR_EINVAL;
    *value = owner->_count;
    return 0;
}

int APS5_VABI APS5_Test_GetPthreadSemWaiterCount(PthreadSem* sem) {
    std::shared_ptr<PthreadSemPrivate> owner;
    try {
        owner = resolveSem(sem);
    } catch (...) {
        return -1;
    }
    std::lock_guard lock(owner->_mutex);
    return owner->_destroyed ? -1 : owner->_waiters;
}

void APS5_VABI APS5_Test_SetPthreadSemApiPause(int kind) {
    semApiPauseEntered.store(0, std::memory_order_relaxed);
    semApiPauseReleased.store(false, std::memory_order_relaxed);
    semApiPauseTarget.store(kind, std::memory_order_release);
}

int APS5_VABI APS5_Test_GetPthreadSemApiPauseCount() {
    return semApiPauseEntered.load(std::memory_order_acquire);
}

void APS5_VABI APS5_Test_ReleasePthreadSemApiPause() {
    semApiPauseReleased.store(true, std::memory_order_release);
    semApiPauseTarget.store(0, std::memory_order_release);
}

}
