#include "../include/Pthread.hpp"
#include "../include/Mutex.hpp"
#include <chrono>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {

constexpr int sceBusy = static_cast<int>(0x80020010u);
constexpr int sceTimedOut = static_cast<int>(0x8002003cu);
std::mutex initializationMutex;

PthreadMutex destroyedMutex() {
    return reinterpret_cast<PthreadMutex>(std::uintptr_t{2});
}

PthreadMutex resolveMutex(PthreadMutex* mutex, bool initialize) {
    if (!mutex)
        throw std::invalid_argument("Mutex pointer is null");
    std::lock_guard lock(initializationMutex);
    if (*mutex == destroyedMutex())
        throw std::runtime_error("Mutex has been destroyed");
    if (reinterpret_cast<std::uintptr_t>(*mutex) == 1)
        throw std::runtime_error("Adaptive mutex initializer is unsupported");
    if (!*mutex) {
        if (!initialize)
            throw std::runtime_error("Mutex is not initialized");
        *mutex = new PthreadMutexPrivate();
    }
    return *mutex;
}

template<typename TAcquire>
int acquireMutex(PthreadMutex mutex, TAcquire acquire, int unavailable, bool tryOnly) {
    const auto thread = std::this_thread::get_id();
    const bool owned = mutex->_owner.load(std::memory_order_acquire) == thread;
    if (owned && mutex->_type != MutexType::Recursive) {
        if (tryOnly)
            return sceBusy;
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
    return acquireMutex(resolveMutex(mutex, true), [&](auto& native) { return native.try_lock_until(deadline); }, sceTimedOut, false);
}

extern "C" {

int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr) {
    if (!attr)
        throw std::invalid_argument("Mutex attribute pointer is null");
    *attr = new PthreadMutexattrPrivate{MutexType::Normal};
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
    default: throw std::invalid_argument("Invalid mutex type");
    }
    return 0;
}

int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol) {
    if (!attr || !*attr)
        throw std::invalid_argument("Mutex attributes are not initialized");
    if (protocol != 0)
        throw std::invalid_argument("Mutex priority inheritance and protection are unsupported");
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
    *mutex = replacement.release();
    return 0;
}

int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex) {
    if (!mutex)
        throw std::invalid_argument("Mutex pointer is null");
    std::lock_guard lock(initializationMutex);
    if (*mutex == destroyedMutex())
        throw std::runtime_error("Mutex has already been destroyed");
    if (reinterpret_cast<std::uintptr_t>(*mutex) == 1)
        throw std::runtime_error("Adaptive mutex initializer is unsupported");
    if (*mutex && (*mutex)->_owner.load(std::memory_order_acquire) != std::thread::id{})
        throw std::runtime_error("Cannot destroy a locked mutex");
    delete *mutex;
    *mutex = destroyedMutex();
    return 0;
}

int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex) {
    return acquireMutex(resolveMutex(mutex, true), [](auto& native) { native.lock(); return true; }, 0, false);
}

int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex) {
    auto* current = resolveMutex(mutex, false);
    if (current->_owner.load(std::memory_order_acquire) != std::this_thread::get_id())
        throw std::runtime_error("Cannot unlock a mutex owned by another thread");
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
    return acquireMutex(resolveMutex(mutex, true), [=](auto& native) { return native.try_lock_for(std::chrono::microseconds(usec)); }, sceTimedOut, false);
}

int APS5_VABI scePthreadMutexTrylock(PthreadMutex* mutex) {
    return acquireMutex(resolveMutex(mutex, true), [](auto& native) { return native.try_lock(); }, sceBusy, true);
}

}
