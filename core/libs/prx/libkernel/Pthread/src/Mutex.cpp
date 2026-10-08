#include "../include/Pthread.hpp"
#include "../include/Mutex.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {

constexpr int sceNotPermitted = static_cast<int>(0x80020001u);
constexpr int sceDeadlock = static_cast<int>(0x8002000bu);
constexpr int sceBusy = static_cast<int>(0x80020010u);
constexpr int sceTimedOut = static_cast<int>(0x8002003cu);
std::mutex initializationMutex;

template<typename TNative>
bool lockInterruptibly(TNative& native) {
    TimedWait::AcquireInterruptibly([&] { return native.try_lock(); }, [&](std::uint64_t micros) { return native.try_lock_for(std::chrono::microseconds(micros)); });
    return true;
}

template<typename TNative>
bool lockInterruptiblyUntilNanos(TNative& native, std::uint64_t deadlineNanos) {
    return TimedWait::AcquireInterruptiblyUntil(deadlineNanos, [&] { return native.try_lock(); }, [&](std::uint64_t micros) { return native.try_lock_for(std::chrono::microseconds(micros)); });
}

template<typename TNative>
bool lockInterruptiblyUntilTime(TNative& native, std::chrono::system_clock::time_point deadline) {
    if (native.try_lock())
        return true;
    TimedWait::InterruptibleScope scope;
    for (;;) {
        const auto step = std::chrono::time_point_cast<std::chrono::system_clock::duration>(std::chrono::system_clock::now() + std::chrono::microseconds(TimedWait::INTERRUPT_SLICE_MICROS));
        if (native.try_lock_until(std::min(deadline, step)))
            return true;
        if (std::chrono::system_clock::now() >= deadline)
            return false;
        scope.Poll();
    }
}

PthreadMutex destroyedMutex() {
    return reinterpret_cast<PthreadMutex>(std::uintptr_t{2});
}

PthreadMutex adaptiveInitializer() {
    return reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
}

bool isInitializedMutex(PthreadMutex mutex) {
    return mutex && mutex != destroyedMutex() && mutex != adaptiveInitializer();
}

PthreadMutex resolveMutex(PthreadMutex* mutex, bool initialize) {
    if (!mutex)
        throw std::invalid_argument("Mutex pointer is null");
    std::atomic_ref<PthreadMutex> slot(*mutex);
    if (const auto current = slot.load(std::memory_order_acquire); isInitializedMutex(current))
        return current;
    std::lock_guard lock(initializationMutex);
    const auto current = slot.load(std::memory_order_acquire);
    if (current == destroyedMutex())
        throw std::runtime_error("Mutex has been destroyed");
    if (current && current != adaptiveInitializer())
        return current;
    if (!initialize)
        throw std::runtime_error("Mutex is not initialized");
    auto* created = new PthreadMutexPrivate();
    if (current == adaptiveInitializer())
        created->_type = MutexType::Adaptive;
    slot.store(created, std::memory_order_release);
    return created;
}

template<typename TAcquire>
int acquireMutex(PthreadMutex mutex, TAcquire acquire, int unavailable, bool tryOnly) {
    const auto thread = std::this_thread::get_id();
    const bool owned = mutex->_owner.load(std::memory_order_acquire) == thread;
    if (owned && mutex->_type != MutexType::Recursive) {
        if (tryOnly)
            return sceBusy;
        if (mutex->_type == MutexType::ErrorCheck || mutex->_type == MutexType::Adaptive)
            return sceDeadlock;
        throw std::runtime_error("Mutex is already owned by the current thread");
    }
    if (owned && mutex->_count == std::numeric_limits<int>::max())
        throw std::overflow_error("Recursive mutex lock count overflow");
    if (mutex->_type == MutexType::Recursive) {
        if (!acquire(mutex->_rmtx))
            return unavailable;
        ++mutex->_count;
    } else {
        if (!acquire(mutex->_mtx))
            return unavailable;
    }
    mutex->_owner.store(thread, std::memory_order_release);
    return 0;
}

}

int MutexOperations::Timedlock(PthreadMutex* mutex, const KernelTimespec* abstime) {
    if (!abstime || abstime->tv_sec < 0 || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000)
        throw std::invalid_argument("Invalid absolute mutex timeout");
    const auto maximum = std::chrono::nanoseconds::max().count();
    if (abstime->tv_sec > (maximum - abstime->tv_nsec) / 1000000000)
        throw std::overflow_error("Absolute mutex timeout exceeds the host clock range");
    const auto duration = std::chrono::nanoseconds(abstime->tv_sec * 1000000000 + abstime->tv_nsec);
    if (std::chrono::duration<long double>(duration) >= std::chrono::duration<long double>(std::chrono::system_clock::duration::max()))
        throw std::overflow_error("Absolute mutex timeout exceeds the host clock range");
    const auto deadline = std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(duration));
    return acquireMutex(resolveMutex(mutex, true), [&](auto& native) { return lockInterruptiblyUntilTime(native, deadline); }, sceTimedOut, false);
}

extern "C" {

int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr) {
    if (!attr)
        throw std::invalid_argument("Mutex attribute pointer is null");
    *attr = new PthreadMutexattrPrivate{MutexType::ErrorCheck};
    return 0;
}

int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr) {
    if (!attr || !*attr)
        throw std::invalid_argument("Mutex attributes are not initialized");
    delete *attr;
    *attr = nullptr;
    return 0;
}

int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type) {
    if (!attr || !*attr)
        throw std::invalid_argument("Mutex attributes are not initialized");
    switch (type) {
    case 1: (*attr)->type = MutexType::ErrorCheck; break;
    case 2: (*attr)->type = MutexType::Recursive; break;
    case 3: (*attr)->type = MutexType::Normal; break;
    case 4: (*attr)->type = MutexType::Adaptive; break;
    default: return SCE_KERNEL_ERROR_EINVAL;
    }
    return 0;
}

int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol) {
    if (!attr || !*attr)
        throw std::invalid_argument("Mutex attributes are not initialized");
    constexpr int PrioNone = 0;
    constexpr int PrioInherit = 1;
    if (protocol != PrioNone && protocol != PrioInherit)
        throw std::invalid_argument("Mutex priority protection is unsupported");
    return 0;
}

int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char*) {
    if (!mutex)
        throw std::invalid_argument("Mutex pointer is null");
    if (attr && !*attr)
        throw std::invalid_argument("Mutex attributes are not initialized");
    auto replacement = std::make_unique<PthreadMutexPrivate>();
    if (attr)
        replacement->_type = (*attr)->type;
    std::lock_guard lock(initializationMutex);
    std::atomic_ref<PthreadMutex>(*mutex).store(replacement.release(), std::memory_order_release);
    return 0;
}

int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex) {
    if (!mutex)
        throw std::invalid_argument("Mutex pointer is null");
    std::lock_guard lock(initializationMutex);
    if (*mutex == destroyedMutex())
        throw std::runtime_error("Mutex has already been destroyed");
    if (*mutex == adaptiveInitializer()) {
        std::atomic_ref<PthreadMutex>(*mutex).store(destroyedMutex(), std::memory_order_release);
        return 0;
    }
    if (*mutex && (*mutex)->_owner.load(std::memory_order_acquire) != std::thread::id{})
        throw std::runtime_error("Cannot destroy a locked mutex");
    delete *mutex;
    std::atomic_ref<PthreadMutex>(*mutex).store(destroyedMutex(), std::memory_order_release);
    return 0;
}

int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex) {
    return acquireMutex(resolveMutex(mutex, true), [](auto& native) { return lockInterruptibly(native); }, 0, false);
}

int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex) {
    auto* current = resolveMutex(mutex, false);
    if (current->_owner.load(std::memory_order_acquire) != std::this_thread::get_id())
        return sceNotPermitted;
    if (current->_type == MutexType::Recursive) {
        if (--current->_count == 0)
            current->_owner.store(std::thread::id{}, std::memory_order_release);
        current->_rmtx.unlock();
    } else {
        current->_owner.store(std::thread::id{}, std::memory_order_release);
        current->_mtx.unlock();
    }
    return 0;
}

int APS5_VABI scePthreadMutexTimedlock(PthreadMutex* mutex, KernelUseconds usec) {
    const auto deadline = TimedWait::DeadlineNanos(usec);
    return acquireMutex(resolveMutex(mutex, true), [=](auto& native) { return lockInterruptiblyUntilNanos(native, deadline); }, sceTimedOut, false);
}

int APS5_VABI scePthreadMutexTrylock(PthreadMutex* mutex) {
    return acquireMutex(resolveMutex(mutex, true), [](auto& native) { return native.try_lock(); }, sceBusy, true);
}

}
