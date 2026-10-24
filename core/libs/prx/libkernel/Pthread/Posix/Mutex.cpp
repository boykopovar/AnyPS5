#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"
#include "../include/Mutex.hpp"
#include "Common.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <atomic>
#include <stdexcept>
#include <string>

namespace {

constexpr int POSIX_EINVAL = 22;
constexpr int POSIX_PROCESS_PRIVATE = 0;
constexpr int GUEST_REALTIME_CLOCK = 0;
constexpr int POSIX_PRIO_PROTECT = 2;
constexpr int POSIX_SCHED_RR = 3;
constexpr std::uintptr_t POSIX_ADAPTIVE_MUTEX_INITIALIZER = 1;
constexpr std::uintptr_t POSIX_MUTEX_DESTROYED = 2;

bool _isStaticInitializer(PthreadMutex mutex) {
    const auto value = reinterpret_cast<std::uintptr_t>(mutex);
    return value == 0 || value == POSIX_ADAPTIVE_MUTEX_INITIALIZER;
}

PthreadMutexPrivate* _priorityProtected(PthreadMutex* mutex, const char* funcName) {
    if (!mutex) throw std::runtime_error(std::string(funcName) + ": null mutex");
    const PthreadMutex current = std::atomic_ref<PthreadMutex>(*mutex).load(std::memory_order_acquire);
    if (reinterpret_cast<std::uintptr_t>(current) <= POSIX_MUTEX_DESTROYED || current->_protocol != POSIX_PRIO_PROTECT) return nullptr;
    return current;
}

int _toSce(int result) {
    return result == 0 ? 0 : SceKernelError(result);
}

void _initializeStatic(PthreadMutex* mutex, const char* funcName) {
    if (!mutex) throw std::runtime_error(std::string(funcName) + ": null mutex");
    std::atomic_ref<PthreadMutex> slot(*mutex);
    PthreadMutex current = slot.load(std::memory_order_acquire);
    if (!_isStaticInitializer(current)) return;
    auto* created = new PthreadMutexPrivate();
    created->SetType(reinterpret_cast<std::uintptr_t>(current) == POSIX_ADAPTIVE_MUTEX_INITIALIZER ? MutexType::Adaptive : MutexType::ErrorCheck);
    if (!slot.compare_exchange_strong(current, created, std::memory_order_acq_rel, std::memory_order_acquire))
        delete created;
}

}

extern "C" {

int APS5_VABI sched_get_priority_max_nid_postfix(int policy);

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
    if (result == 0 && attr && *attr) {
        (*mutex)->_protocol = (*attr)->protocol;
        (*mutex)->_ceiling = (*attr)->ceiling;
    }
    return PosixThread::ToErrno(result);
}

int APS5_VABI pthread_mutex_lock_nid_postfix(PthreadMutex* mutex) {
    _initializeStatic(mutex, __func__);
    return PosixThread::ToErrno(scePthreadMutexLock(mutex));
}

int APS5_VABI pthread_mutex_timedlock_nid_postfix(PthreadMutex* mutex, const KernelTimespec* abstime) {
    if (!abstime) return pthread_mutex_lock_nid_postfix(mutex);
    _initializeStatic(mutex, __func__);
    if (scePthreadMutexTrylock(mutex) == 0) return 0;
    KernelUseconds usec = 0;
    if (!PosixThread::RelativeMicroseconds(GUEST_REALTIME_CLOCK, abstime, &usec)) return POSIX_EINVAL;
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

int APS5_VABI pthread_mutex_reltimedlock_np_nid_postfix(PthreadMutex* mutex, KernelUseconds usec) {
    _initializeStatic(mutex, __func__);
    return PosixThread::ToErrno(scePthreadMutexTimedlock(mutex, usec));
}

int APS5_VABI sched_get_priority_min_nid_postfix(int policy);

int APS5_VABI pthread_mutex_getprioceiling_nid_postfix(const PthreadMutex* mutex, int* prioceiling) {
    if (!prioceiling) throw std::runtime_error("pthread_mutex_getprioceiling: null prioceiling");
    auto* current = _priorityProtected(const_cast<PthreadMutex*>(mutex), "pthread_mutex_getprioceiling");
    if (!current) return POSIX_EINVAL;
    *prioceiling = current->_ceiling;
    return 0;
}

int APS5_VABI pthread_mutex_setprioceiling_nid_postfix(PthreadMutex* mutex, int ceiling, int* oldCeiling) {
    auto* current = _priorityProtected(mutex, "pthread_mutex_setprioceiling");
    if (!current) return POSIX_EINVAL;
    if (ceiling < sched_get_priority_min_nid_postfix(POSIX_SCHED_RR) || ceiling > sched_get_priority_max_nid_postfix(POSIX_SCHED_RR))
        throw std::runtime_error("pthread_mutex_setprioceiling: ceiling " + std::to_string(ceiling) + " outside the SCHED_RR priority range");
    const bool owned = MutexOperations::IsOwned(mutex);
    if (!owned) {
        if (const int result = scePthreadMutexLock(mutex)) return PosixThread::ToErrno(result);
    }
    const int previous = current->_ceiling;
    current->_ceiling = ceiling;
    if (!owned && scePthreadMutexUnlock(mutex) != 0) throw std::runtime_error("pthread_mutex_setprioceiling: unlock failed");
    if (oldCeiling) *oldCeiling = previous;
    return 0;
}

int APS5_VABI scePthreadMutexGetprioceiling(PthreadMutex* mutex, int* prioceiling) {
    return _toSce(pthread_mutex_getprioceiling_nid_postfix(mutex, prioceiling));
}

int APS5_VABI scePthreadMutexSetprioceiling(PthreadMutex* mutex, int ceiling, int* oldCeiling) {
    return _toSce(pthread_mutex_setprioceiling_nid_postfix(mutex, ceiling, oldCeiling));
}

int APS5_VABI pthread_mutex_isowned_np_nid_postfix(PthreadMutex* mutex) {
    return MutexOperations::IsOwned(mutex) ? 1 : 0;
}

int APS5_VABI pthread_mutex_getspinloops_np_nid_postfix(PthreadMutex* mutex, int* count) {
    return PosixThread::ToErrno(scePthreadMutexGetspinloops(mutex, count));
}

int APS5_VABI pthread_mutex_setspinloops_np_nid_postfix(PthreadMutex* mutex, int count) {
    return PosixThread::ToErrno(scePthreadMutexSetspinloops(mutex, count));
}

int APS5_VABI pthread_mutex_getyieldloops_np_nid_postfix(PthreadMutex* mutex, int* count) {
    return PosixThread::ToErrno(scePthreadMutexGetyieldloops(mutex, count));
}

int APS5_VABI pthread_mutex_setyieldloops_np_nid_postfix(PthreadMutex* mutex, int count) {
    return PosixThread::ToErrno(scePthreadMutexSetyieldloops(mutex, count));
}

int APS5_VABI pthread_mutexattr_destroy_nid_postfix(PthreadMutexattr* attr) {
    return PosixThread::ToErrno(scePthreadMutexattrDestroy(attr));
}

int APS5_VABI pthread_mutexattr_init_nid_postfix(PthreadMutexattr* attr) {
    const int result = scePthreadMutexattrInit(attr);
    if (result == 0) (*attr)->type = MutexType::ErrorCheck;
    return PosixThread::ToErrno(result);
}

int APS5_VABI pthread_mutexattr_getpshared_nid_postfix(const PthreadMutexattr* attr, int* pshared) {
    if (!attr || !*attr) return POSIX_EINVAL;
    if (!pshared) throw std::runtime_error("pthread_mutexattr_getpshared: null pshared");
    *pshared = (*attr)->pshared;
    return 0;
}

int APS5_VABI pthread_mutexattr_setpshared_nid_postfix(PthreadMutexattr* attr, int pshared) {
    if (!attr || !*attr) return POSIX_EINVAL;
    if (pshared != POSIX_PROCESS_PRIVATE) return POSIX_EINVAL;
    (*attr)->pshared = pshared;
    return 0;
}

int APS5_VABI pthread_mutexattr_setprotocol_nid_postfix(PthreadMutexattr* attr, int protocol) {
    if (!attr || !*attr) return PosixThread::GUEST_EINVAL;
    if (protocol < 0 || protocol > POSIX_PRIO_PROTECT) return POSIX_EINVAL;
    (*attr)->protocol = protocol;
    (*attr)->ceiling = sched_get_priority_max_nid_postfix(POSIX_SCHED_RR);
    return 0;
}

int APS5_VABI pthread_mutexattr_getprotocol_nid_postfix(const PthreadMutexattr* attr, int* protocol) {
    if (!attr || !*attr) return PosixThread::GUEST_EINVAL;
    if (!protocol) throw std::runtime_error("pthread_mutexattr_getprotocol: null protocol");
    *protocol = (*attr)->protocol;
    return 0;
}

int APS5_VABI pthread_mutexattr_setprioceiling_nid_postfix(PthreadMutexattr* attr, int prioceiling) {
    if (!attr || !*attr) return PosixThread::GUEST_EINVAL;
    if ((*attr)->protocol != POSIX_PRIO_PROTECT) return POSIX_EINVAL;
    (*attr)->ceiling = prioceiling;
    return 0;
}

int APS5_VABI pthread_mutexattr_getprioceiling_nid_postfix(const PthreadMutexattr* attr, int* prioceiling) {
    if (!attr || !*attr) return PosixThread::GUEST_EINVAL;
    if (!prioceiling) throw std::runtime_error("pthread_mutexattr_getprioceiling: null prioceiling");
    if ((*attr)->protocol != POSIX_PRIO_PROTECT) return POSIX_EINVAL;
    *prioceiling = (*attr)->ceiling;
    return 0;
}

int APS5_VABI pthread_mutexattr_settype_nid_postfix(PthreadMutexattr* attr, int type) {
    return PosixThread::ToErrno(scePthreadMutexattrSettype(attr, type));
}

int APS5_VABI pthread_mutexattr_gettype_nid_postfix(const PthreadMutexattr* attr, int* type) {
    if (!attr || !*attr) return POSIX_EINVAL;
    if (!type) throw std::runtime_error("pthread_mutexattr_gettype: null type");
    *type = static_cast<int>((*attr)->type);
    return 0;
}

}
