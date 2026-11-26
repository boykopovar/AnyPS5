#include "prx/libkernel/include/Api.hpp"
#include "../include/Pthread.hpp"
#include "../include/Cancel.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <cerrno>
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

struct ResolvedSem {
    std::shared_ptr<PthreadSemPrivate> owner;
    int error = 0;
};

ResolvedSem resolveSem(PthreadSem* sem) {
    if (!sem)
        throw std::invalid_argument("Semaphore pointer is null");
    const int savedErrno = errno;
    std::shared_lock lock(semRegistryMutex);
    PthreadSem current = nullptr;
    if (!GuestCopyIn_nid_no_patch(&current, sem, sizeof(current))) {
        errno = savedErrno;
        return {nullptr, SCE_KERNEL_ERROR_EFAULT};
    }
    errno = savedErrno;
    if (!current || current == destroyedSem())
        throw std::runtime_error("Semaphore is not initialized or has been destroyed");
    const auto it = semRegistry.find(current);
    if (it == semRegistry.end())
        throw std::runtime_error("Semaphore is not initialized or has been destroyed");
    return {it->second, 0};
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
    PthreadSem handle = replacement.get();
    std::unique_lock lock(semRegistryMutex);
    const int savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(sem, &handle, sizeof(handle))) {
        errno = savedErrno;
        return SCE_KERNEL_ERROR_EFAULT;
    }
    errno = savedErrno;
    semRegistry.emplace(handle, std::move(replacement));
    return 0;
}

int APS5_VABI scePthreadSemDestroy(PthreadSem* sem) {
    if (!sem)
        return SCE_KERNEL_ERROR_EINVAL;
    const int savedErrno = errno;
    std::unique_lock registryLock(semRegistryMutex);
    PthreadSem current = nullptr;
    if (!GuestCopyIn_nid_no_patch(&current, sem, sizeof(current)) || !GuestWritable_nid_no_patch(sem, sizeof(*sem))) {
        errno = savedErrno;
        return SCE_KERNEL_ERROR_EFAULT;
    }
    errno = savedErrno;
    if (current == destroyedSem())
        return SCE_KERNEL_ERROR_EINVAL;
    if (!current)
        throw std::runtime_error("Semaphore is not initialized");
    const auto it = semRegistry.find(current);
    if (it == semRegistry.end())
        return SCE_KERNEL_ERROR_EINVAL;
    const auto owner = it->second;
    const PthreadSem tombstone = destroyedSem();
    if (!GuestCopyOut_nid_no_patch(sem, &tombstone, sizeof(tombstone))) {
        errno = savedErrno;
        return SCE_KERNEL_ERROR_EFAULT;
    }
    errno = savedErrno;
    std::lock_guard semLock(owner->_mutex);
    owner->_destroyed = true;
    owner->_cv.NotifyAll();
    semRegistry.erase(it);
    return 0;
}

int APS5_VABI scePthreadSemPost(PthreadSem* sem) {
    const auto resolved = resolveSem(sem);
    if (resolved.error != 0)
        return resolved.error;
    const auto& owner = resolved.owner;
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
    auto resolved = resolveSem(sem);
    if (resolved.error != 0)
        return resolved.error;
    SemaphoreWaitCleanupContext cleanupContext{std::move(resolved.owner), false};
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
    const auto resolved = resolveSem(sem);
    if (resolved.error != 0)
        return resolved.error;
    const auto& owner = resolved.owner;
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
    auto resolved = resolveSem(sem);
    if (resolved.error != 0)
        return resolved.error;
    SemaphoreWaitCleanupContext cleanupContext{std::move(resolved.owner), false};
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
            return sceTimedOut;
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
    const auto resolved = resolveSem(sem);
    if (resolved.error != 0)
        return resolved.error;
    const auto& owner = resolved.owner;
    std::lock_guard lock(owner->_mutex);
    if (owner->_destroyed)
        return SCE_KERNEL_ERROR_EINVAL;
    const int count = owner->_count;
    const int savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(value, &count, sizeof(count))) {
        errno = savedErrno;
        return SCE_KERNEL_ERROR_EFAULT;
    }
    errno = savedErrno;
    return 0;
}

}
