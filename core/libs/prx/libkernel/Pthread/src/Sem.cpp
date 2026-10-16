#include "../include/Pthread.hpp"
#include "../include/Cancel.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace {

constexpr int sceBusy = static_cast<int>(0x80020010u);
constexpr int sceTimedOut = static_cast<int>(0x8002003cu);
constexpr int sceSemValueMax = 0x7fffffff;
std::mutex semInitializationMutex;
std::unordered_map<PthreadSemPrivate*, int> semWaiters;

PthreadSem destroyedSem() {
    return reinterpret_cast<PthreadSem>(std::uintptr_t{2});
}

PthreadSemPrivate* resolveSem(PthreadSem* sem) {
    if (!sem)
        throw std::invalid_argument("Semaphore pointer is null");
    std::atomic_ref<PthreadSem> slot(*sem);
    const auto current = slot.load(std::memory_order_acquire);
    if (!current || current == destroyedSem())
        throw std::runtime_error("Semaphore is not initialized or has been destroyed");
    return current;
}

struct WaiterGuard {
    PthreadSemPrivate* sem;
    explicit WaiterGuard(PthreadSemPrivate* s) : sem(s) {
        std::lock_guard lock(semInitializationMutex);
        ++semWaiters[sem];
    }
    ~WaiterGuard() {
        std::lock_guard lock(semInitializationMutex);
        if (const auto it = semWaiters.find(sem); it != semWaiters.end()) {
            if (--it->second <= 0)
                semWaiters.erase(it);
        }
    }
};

}

extern "C" {

int APS5_VABI scePthreadSemInit(PthreadSem* sem, int flag, unsigned int value, const char* name) {
    if (!sem)
        throw std::invalid_argument("Semaphore pointer is null");
    if (value > static_cast<unsigned int>(sceSemValueMax))
        return SCE_KERNEL_ERROR_EINVAL;
    (void)flag;
    (void)name;
    auto replacement = std::make_unique<PthreadSemPrivate>(value);
    std::lock_guard lock(semInitializationMutex);
    std::atomic_ref<PthreadSem>(*sem).store(replacement.release(), std::memory_order_release);
    return 0;
}

int APS5_VABI scePthreadSemDestroy(PthreadSem* sem) {
    if (!sem)
        throw std::invalid_argument("Semaphore pointer is null");
    std::lock_guard lock(semInitializationMutex);
    auto current = *sem;
    if (current == destroyedSem())
        return SCE_KERNEL_ERROR_EINVAL;
    if (!current)
        throw std::runtime_error("Semaphore is not initialized");
    if (const auto it = semWaiters.find(current); it != semWaiters.end() && it->second > 0)
        return SCE_KERNEL_ERROR_EBUSY;
    *sem = destroyedSem();
    delete current;
    return 0;
}

int APS5_VABI scePthreadSemPost(PthreadSem* sem) {
    auto* current = resolveSem(sem);
    std::lock_guard lock(current->_mutex);
    if (current->_count == sceSemValueMax)
        return SCE_KERNEL_ERROR_EOVERFLOW;
    ++current->_count;
    current->_cv.NotifyOne();
    return 0;
}

int APS5_VABI scePthreadSemWait(PthreadSem* sem) {
    auto* current = resolveSem(sem);
    ThreadCancel::Check();
    std::unique_lock lock(current->_mutex);
    WaiterGuard waiterGuard(current);
    ThreadCancel::WaitUntil(current->_cv, lock, std::nullopt, [&] { return current->_count > 0; });
    --current->_count;
    return 0;
}

int APS5_VABI scePthreadSemTrywait(PthreadSem* sem) {
    auto* current = resolveSem(sem);
    std::lock_guard lock(current->_mutex);
    if (current->_count <= 0)
        return sceBusy;
    --current->_count;
    return 0;
}

int APS5_VABI scePthreadSemTimedwait(PthreadSem* sem, KernelUseconds usec) {
    auto* current = resolveSem(sem);
    ThreadCancel::Check();
    std::unique_lock lock(current->_mutex);
    WaiterGuard waiterGuard(current);
    const auto acquired = ThreadCancel::WaitUntil(current->_cv, lock, TimedWait::DeadlineNanos(usec), [&] { return current->_count > 0; });
    if (!acquired)
        return sceTimedOut;
    --current->_count;
    return 0;
}

int APS5_VABI scePthreadSemGetvalue(PthreadSem* sem, int* value) {
    auto* current = resolveSem(sem);
    if (!value)
        throw std::invalid_argument("Semaphore value pointer is null");
    std::lock_guard lock(current->_mutex);
    *value = current->_count;
    return 0;
}

}
