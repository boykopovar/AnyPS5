#include "prx/libkernel/Pthread/include/Mutex.hpp"
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
std::mutex& InitializationMutex() {
    static std::mutex mutex;
    return mutex;
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
    std::lock_guard lock(InitializationMutex());
    const auto current = slot.load(std::memory_order_acquire);
    if (current == destroyedMutex())
        return nullptr;
    if (current && current != adaptiveInitializer())
        return current;
    if (!initialize)
        throw std::runtime_error("Mutex is not initialized");
    auto* created = new PthreadMutexPrivate();
    if (current == adaptiveInitializer())
        created->SetType(MutexType::Adaptive);
    slot.store(created, std::memory_order_release);
    return created;
}

template<typename TAcquire>
int acquireMutex(PthreadMutex mutex, TAcquire acquire, int unavailable, bool tryOnly) {
    if (!mutex)
        return SCE_KERNEL_ERROR_EINVAL;
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

bool MutexOperations::IsOwned(const PthreadMutex* mutex) {
    if (!mutex)
        throw std::invalid_argument("Mutex pointer is null");
    const auto current = std::atomic_ref<PthreadMutex>(*const_cast<PthreadMutex*>(mutex)).load(std::memory_order_acquire);
    if (!isInitializedMutex(current))
        return false;
    return current->_owner.load(std::memory_order_acquire) == std::this_thread::get_id();
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
    auto* current = resolveMutex(mutex, true);
    return acquireMutex(current, [&](auto& native) {
#ifdef _WIN32
        for (;;) {
            const auto now = std::chrono::system_clock::now();
            if (now >= deadline) return native.try_lock();
            const auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - now).count();
            const std::uint64_t slice = TimedWait::NowNanos() + static_cast<std::uint64_t>(std::min<std::int64_t>(remaining, 100000000));
            if (TimedWait::ParkAcquire(current, [&] { return native.try_lock(); }, &slice)) return true;
        }
#else
        static_cast<void>(current);
        return native.try_lock_until(deadline);
#endif
    }, sceTimedOut, false);
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
        return SCE_KERNEL_ERROR_EINVAL;
    delete *attr;
    *attr = nullptr;
    return 0;
}

int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type) {
    if (!attr || !*attr)
        return SCE_KERNEL_ERROR_EINVAL;
    switch (type) {
    case 1: (*attr)->type = MutexType::ErrorCheck; break;
    case 2: (*attr)->type = MutexType::Recursive; break;
    case 3: (*attr)->type = MutexType::Normal; break;
    case 4: (*attr)->type = MutexType::Adaptive; break;
    default: return SCE_KERNEL_ERROR_EINVAL;
    }
    return 0;
}

int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char*) {
    if (!mutex)
        throw std::invalid_argument("Mutex pointer is null");
    auto replacement = std::make_unique<PthreadMutexPrivate>();
    if (attr && *attr)
        replacement->SetType((*attr)->type);
    std::lock_guard lock(InitializationMutex());
    std::atomic_ref<PthreadMutex>(*mutex).store(replacement.release(), std::memory_order_release);
    return 0;
}

int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex) {
    if (!mutex)
        throw std::invalid_argument("Mutex pointer is null");
    std::lock_guard lock(InitializationMutex());
    if (*mutex == destroyedMutex())
        return SCE_KERNEL_ERROR_EINVAL;
    if (*mutex == adaptiveInitializer()) {
        std::atomic_ref<PthreadMutex>(*mutex).store(destroyedMutex(), std::memory_order_release);
        return 0;
    }
    if (*mutex && (*mutex)->_owner.load(std::memory_order_acquire) != std::thread::id{})
        return SCE_KERNEL_ERROR_EBUSY;
    delete *mutex;
    std::atomic_ref<PthreadMutex>(*mutex).store(destroyedMutex(), std::memory_order_release);
    return 0;
}

int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex) {
    auto* current = resolveMutex(mutex, true);
    return acquireMutex(current, [current](auto& native) {
#ifdef _WIN32
        return TimedWait::ParkAcquire(current, [&] { return native.try_lock(); }, nullptr);
#else
        static_cast<void>(current);
        native.lock();
        return true;
#endif
    }, 0, false);
}

int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex) {
    if (!mutex)
        throw std::invalid_argument("Mutex pointer is null");
    auto* current = std::atomic_ref<PthreadMutex>(*mutex).load(std::memory_order_acquire);
    if (current == destroyedMutex())
        return SCE_KERNEL_ERROR_EINVAL;
    if (!isInitializedMutex(current) || current->_owner.load(std::memory_order_acquire) != std::this_thread::get_id())
        return sceNotPermitted;
    bool released = true;
    if (current->_type == MutexType::Recursive) {
        released = --current->_count == 0;
        if (released)
            current->_owner.store(std::thread::id{}, std::memory_order_release);
        current->_rmtx.unlock();
    } else {
        current->_owner.store(std::thread::id{}, std::memory_order_release);
        current->_mtx.unlock();
    }
#ifdef _WIN32
    if (released) TimedWait::Unpark(current);
#else
    static_cast<void>(released);
#endif
    return 0;
}

int APS5_VABI scePthreadMutexTimedlock(PthreadMutex* mutex, KernelUseconds usec) {
    const auto deadline = TimedWait::DeadlineNanos(usec);
    auto* current = resolveMutex(mutex, true);
    if (current && current->_type != MutexType::Recursive && current->_owner.load(std::memory_order_acquire) == std::this_thread::get_id()) {
        TimedWait::SleepUntil(deadline);
        return sceTimedOut;
    }
    return acquireMutex(current, [=](auto& native) {
        return TimedWait::AcquireUntil(current, deadline, [&] { return native.try_lock(); }, [&](std::uint64_t micros) { return native.try_lock_for(std::chrono::microseconds(micros)); });
    }, sceTimedOut, false);
}

int APS5_VABI scePthreadMutexTrylock(PthreadMutex* mutex) {
    return acquireMutex(resolveMutex(mutex, true), [](auto& native) { return native.try_lock(); }, sceBusy, true);
}

int APS5_VABI scePthreadMutexGetspinloops(PthreadMutex* mutex, int* count) {
    auto* current = resolveMutex(mutex, true);
    if (!current)
        return SCE_KERNEL_ERROR_EINVAL;
    if (!count)
        throw std::invalid_argument("Spin loop count pointer is null");
    *count = current->_spinloops;
    return 0;
}

int APS5_VABI scePthreadMutexSetspinloops(PthreadMutex* mutex, int count) {
    auto* current = resolveMutex(mutex, true);
    if (!current)
        return SCE_KERNEL_ERROR_EINVAL;
    current->_spinloops = count;
    return 0;
}

int APS5_VABI scePthreadMutexGetyieldloops(PthreadMutex* mutex, int* count) {
    auto* current = resolveMutex(mutex, true);
    if (!current)
        return SCE_KERNEL_ERROR_EINVAL;
    if (!count)
        throw std::invalid_argument("Yield loop count pointer is null");
    *count = current->_yieldloops;
    return 0;
}

int APS5_VABI scePthreadMutexSetyieldloops(PthreadMutex* mutex, int count) {
    if (auto* current = resolveMutex(mutex, true))
        current->_yieldloops = count;
    return 0;
}

}
