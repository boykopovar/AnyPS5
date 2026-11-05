#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdio>

namespace {

constexpr int MutexTypeRecursive = 2;
constexpr std::size_t MutexNameSize = 32;

}

extern "C" {

int APS5_VABI _Mtxinit_nid_postfix(PthreadMutex* mutex, const char* name) {
    char mutexName[MutexNameSize];
    if (name != nullptr)
        std::snprintf(mutexName, sizeof(mutexName), "SceLibcI_%s", name);
    else
        std::snprintf(mutexName, sizeof(mutexName), "SceLibcI");
    PthreadMutexattr attr = nullptr;
    if (scePthreadMutexattrInit(&attr) != 0) return 1;
    if (scePthreadMutexattrSettype(&attr, MutexTypeRecursive) != 0) {
        scePthreadMutexattrDestroy(&attr);
        return 1;
    }
    const int initialized = scePthreadMutexInit(mutex, &attr, mutexName);
    const int released = scePthreadMutexattrDestroy(&attr);
    return initialized != 0 || released != 0;
}

int APS5_VABI _Mtxlock_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_lock_nid_postfix(mutex) != 0;
}

int APS5_VABI _Mtxunlock_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_unlock_nid_postfix(mutex) != 0;
}

int APS5_VABI _Mtxdst_nid_postfix(PthreadMutex* mutex) {
    return pthread_mutex_destroy_nid_postfix(mutex) != 0;
}

}
