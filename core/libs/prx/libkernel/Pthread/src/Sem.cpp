#include "../include/Pthread.hpp"
#include "../include/Cancel.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>

namespace {

constexpr int sceBusy = static_cast<int>(0x80020010u);
constexpr int sceTimedOut = static_cast<int>(0x8002003cu);
constexpr int sceSemValueMax = 0x7fffffff;
std::shared_mutex semRegistryMutex;
std::unordered_map<PthreadSemPrivate*, std::shared_ptr<PthreadSemPrivate>> semRegistry;

PthreadSem destroyedSem() {
    return reinterpret_cast<PthreadSem>(std::uintptr_t{2});
}

std::shared_ptr<PthreadSemPrivate> resolveSem(PthreadSem* sem) {
    if (!sem)
        throw std::invalid_argument("Semaphore pointer is null");
    std::shared_lock lock(semRegistryMutex);
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

static const auto CleanupSemaphoreWait = [](void* argument) APS5_VABI {
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
};

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
    std::unique_lock lock(semRegistryMutex);
    semRegistry.emplace(replacement.get(), replacement);
    std::atomic_ref<PthreadSem>(*sem).store(replacement.get(), std::memory_order_release);
    return 0;
}

int APS5_VABI scePthreadSemDestroy(PthreadSem* sem) {
    if (!sem)
        throw std::invalid_argument("Semaphore pointer is null");
    std::unique_lock registryLock(semRegistryMutex);
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
    owner->_destroyed = true;
    owner->_cv.NotifyAll();
    std::atomic_ref<PthreadSem>(*sem).store(destroyedSem(), std::memory_order_release);
    semRegistry.erase(it);
    return 0;
}

int APS5_VABI scePthreadSemPost(PthreadSem* sem) {
    const auto owner = resolveSem(sem);
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

}
