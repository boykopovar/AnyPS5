#include "../include/Pthread.hpp"
#include "../include/Cancel.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace {

constexpr int sceBusy = static_cast<int>(0x80020010u);
constexpr int sceTimedOut = static_cast<int>(0x8002003cu);
std::mutex semInitializationMutex;

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

}

extern "C" {

int APS5_VABI scePthreadSemInit(PthreadSem* sem, int flag, unsigned int value, const char* name) {
    if (!sem)
        throw std::invalid_argument("Semaphore pointer is null");
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
    if (!current || current == destroyedSem())
        throw std::runtime_error("Semaphore is not initialized or has been destroyed");
    *sem = destroyedSem();
    delete current;
    return 0;
}

int APS5_VABI scePthreadSemPost(PthreadSem* sem) {
    auto* current = resolveSem(sem);
    {
        std::lock_guard lock(current->_mutex);
        ++current->_count;
    }
    current->_cv.NotifyOne();
    return 0;
}

int APS5_VABI scePthreadSemWait(PthreadSem* sem) {
    auto* current = resolveSem(sem);
    ThreadCancel::Check();
    std::unique_lock lock(current->_mutex);
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
