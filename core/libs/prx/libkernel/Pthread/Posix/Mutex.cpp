#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"
#include "../include/Mutex.hpp"
#include "Common.hpp"
#include <atomic>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr int POSIX_EINVAL = 22;
constexpr int POSIX_MUTEX_ADAPTIVE_NP = 4;
constexpr int POSIX_PRIO_PROTECT = 2;
constexpr std::uintptr_t POSIX_ADAPTIVE_MUTEX_INITIALIZER = 1;

bool _isStaticInitializer(PthreadMutex mutex) {
    const auto value = reinterpret_cast<std::uintptr_t>(mutex);
    return value == 0 || value == POSIX_ADAPTIVE_MUTEX_INITIALIZER;
}

void _initializeStatic(PthreadMutex* mutex, const char* funcName) {
    if (!mutex) throw std::runtime_error(std::string(funcName) + ": null mutex");
    std::atomic_ref<PthreadMutex> slot(*mutex);
    PthreadMutex current = slot.load(std::memory_order_acquire);
    if (!_isStaticInitializer(current)) return;
    auto* created = new PthreadMutexPrivate();
    created->_type = reinterpret_cast<std::uintptr_t>(current) == POSIX_ADAPTIVE_MUTEX_INITIALIZER ? MutexType::Normal : MutexType::ErrorCheck;
    if (!slot.compare_exchange_strong(current, created, std::memory_order_acq_rel, std::memory_order_acquire))
        delete created;
}

}

extern "C" {

int APS5_VABI pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex) {
    if (!mutex) throw std::runtime_error("pthread_mutex_destroy: null mutex");
    if (_isStaticInitializer(*mutex)) {
        *mutex = nullptr;
        return 0;
    }
    return PosixThread::ToErrno(scePthreadMutexDestroy(mutex));
}

int APS5_VABI pthread_mutex_init_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr) {
    const int result = scePthreadMutexInit(mutex, attr, nullptr);
    if (result == 0 && (!attr || !*attr)) (*mutex)->_type = MutexType::ErrorCheck;
    return PosixThread::ToErrno(result);
}

int APS5_VABI pthread_mutex_lock_nid_postfix(PthreadMutex* mutex) {
    _initializeStatic(mutex, __func__);
    return PosixThread::ToErrno(scePthreadMutexLock(mutex));
}

int APS5_VABI pthread_mutex_timedlock_nid_postfix(PthreadMutex* mutex, const KernelTimespec* abstime) {
    if (!abstime) throw std::runtime_error("pthread_mutex_timedlock: null abstime");
    if (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000) return POSIX_EINVAL;
    _initializeStatic(mutex, __func__);
    const auto deadline = std::chrono::seconds(abstime->tv_sec) + std::chrono::nanoseconds(abstime->tv_nsec);
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count();
    const auto usec = remaining <= 0 ? 0 : remaining >= std::numeric_limits<KernelUseconds>::max() ? std::numeric_limits<KernelUseconds>::max() : static_cast<KernelUseconds>(remaining);
    return PosixThread::ToErrno(scePthreadMutexTimedlock(mutex, usec));
}

int APS5_VABI pthread_mutex_trylock_nid_postfix(PthreadMutex* mutex) {
    _initializeStatic(mutex, __func__);
    return PosixThread::ToErrno(scePthreadMutexTrylock(mutex));
}

int APS5_VABI pthread_mutex_unlock_nid_postfix(PthreadMutex* mutex) {
    _initializeStatic(mutex, __func__);
    return PosixThread::ToErrno(scePthreadMutexUnlock(mutex));
}

int APS5_VABI pthread_mutexattr_destroy_nid_postfix(PthreadMutexattr* attr) {
    return PosixThread::ToErrno(scePthreadMutexattrDestroy(attr));
}

int APS5_VABI pthread_mutexattr_init_nid_postfix(PthreadMutexattr* attr) {
    const int result = scePthreadMutexattrInit(attr);
    if (result == 0) (*attr)->type = MutexType::ErrorCheck;
    return PosixThread::ToErrno(result);
}

int APS5_VABI pthread_mutexattr_setprotocol_nid_postfix(PthreadMutexattr* attr, int protocol) {
    if (!attr || !*attr) throw std::runtime_error("pthread_mutexattr_setprotocol: null attr");
    if (protocol < 0 || protocol > POSIX_PRIO_PROTECT) return POSIX_EINVAL;
    return 0;
}

int APS5_VABI pthread_mutexattr_settype_nid_postfix(PthreadMutexattr* attr, int type) {
    if (type == POSIX_MUTEX_ADAPTIVE_NP) {
        if (!attr || !*attr) throw std::runtime_error("pthread_mutexattr_settype: null attr");
        (*attr)->type = MutexType::Normal;
        return 0;
    }
    return PosixThread::ToErrno(scePthreadMutexattrSettype(attr, type));
}

}
