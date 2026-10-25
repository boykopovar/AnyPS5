#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"
#include "../include/Mutex.hpp"
#include "Common.hpp"
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
    created->SetType(reinterpret_cast<std::uintptr_t>(current) == POSIX_ADAPTIVE_MUTEX_INITIALIZER ? MutexType::Adaptive : MutexType::ErrorCheck);
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

int APS5_VABI pthread_mutexattr_setkind_np_nid_postfix(PthreadMutexattr* attr, int kind) {
    if (!attr || !*attr) {
        *__error_nid_postfix() = POSIX_EINVAL;
        return -1;
    }
    if (kind < static_cast<int>(MutexType::ErrorCheck) || kind > static_cast<int>(MutexType::Adaptive))
        throw std::runtime_error("pthread_mutexattr_setkind_np: kind " + std::to_string(kind) + " is not a mutex type, and pthread_mutex_init does not reject it with EINVAL");
    (*attr)->type = static_cast<MutexType>(kind);
    return 0;
}

int APS5_VABI pthread_mutexattr_getkind_np_nid_postfix(PthreadMutexattr attr) {
    if (!attr) {
        *__error_nid_postfix() = POSIX_EINVAL;
        return -1;
    }
    return static_cast<int>(attr->type);
}

int APS5_VABI scePthreadMutexattrGettype(const PthreadMutexattr* attr, int* type) {
    return PosixThread::ToSce(pthread_mutexattr_gettype_nid_postfix(attr, type));
}

int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol) {
    return PosixThread::ToSce(pthread_mutexattr_setprotocol_nid_postfix(attr, protocol));
}

int APS5_VABI scePthreadMutexattrGetprotocol(const PthreadMutexattr* attr, int* protocol) {
    return PosixThread::ToSce(pthread_mutexattr_getprotocol_nid_postfix(attr, protocol));
}

int APS5_VABI scePthreadMutexattrSetprioceiling(PthreadMutexattr* attr, int prioceiling) {
    return PosixThread::ToSce(pthread_mutexattr_setprioceiling_nid_postfix(attr, prioceiling));
}

int APS5_VABI scePthreadMutexattrGetprioceiling(const PthreadMutexattr* attr, int* prioceiling) {
    return PosixThread::ToSce(pthread_mutexattr_getprioceiling_nid_postfix(attr, prioceiling));
}

int APS5_VABI scePthreadMutexattrSetpshared(PthreadMutexattr* attr, int pshared) {
    return PosixThread::ToSce(pthread_mutexattr_setpshared_nid_postfix(attr, pshared));
}

int APS5_VABI scePthreadMutexattrGetpshared(const PthreadMutexattr* attr, int* pshared) {
    return PosixThread::ToSce(pthread_mutexattr_getpshared_nid_postfix(attr, pshared));
}

}
