#include "../include/Mutex.hpp"
#include <cstdint>
#include <stdexcept>

namespace {

int toPosix(int result) {
    if (result == 0)
        return 0;
    const auto error = static_cast<std::uint32_t>(result);
    if ((error & 0xffff0000u) != 0x80020000u)
        throw std::runtime_error("Unexpected SCE mutex error");
    return static_cast<int>(error & 0xffffu);
}

}

extern "C" {

int APS5_VABI pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex) {
    return toPosix(scePthreadMutexDestroy(mutex));
}

int APS5_VABI pthread_mutex_init_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr) {
    return toPosix(scePthreadMutexInit(mutex, attr, nullptr));
}

int APS5_VABI pthread_mutex_lock_nid_postfix(PthreadMutex* mutex) {
    return toPosix(scePthreadMutexLock(mutex));
}

int APS5_VABI pthread_mutex_timedlock_nid_postfix(PthreadMutex* mutex, const KernelTimespec* abstime) {
    return toPosix(MutexOperations::Timedlock(mutex, abstime));
}

int APS5_VABI pthread_mutex_trylock_nid_postfix(PthreadMutex* mutex) {
    return toPosix(scePthreadMutexTrylock(mutex));
}

int APS5_VABI pthread_mutex_unlock_nid_postfix(PthreadMutex* mutex) {
    return toPosix(scePthreadMutexUnlock(mutex));
}

int APS5_VABI pthread_mutexattr_destroy_nid_postfix(PthreadMutexattr* attr) {
    return toPosix(scePthreadMutexattrDestroy(attr));
}

int APS5_VABI pthread_mutexattr_init_nid_postfix(PthreadMutexattr* attr) {
    return toPosix(scePthreadMutexattrInit(attr));
}

int APS5_VABI pthread_mutexattr_setprotocol_nid_postfix(PthreadMutexattr* attr, int protocol) {
    return toPosix(scePthreadMutexattrSetprotocol(attr, protocol));
}

int APS5_VABI pthread_mutexattr_settype_nid_postfix(PthreadMutexattr* attr, int type) {
    return toPosix(scePthreadMutexattrSettype(attr, type));
}

}