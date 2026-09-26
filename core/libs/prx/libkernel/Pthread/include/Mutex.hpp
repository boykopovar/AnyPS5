#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_MUTEX_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_MUTEX_HPP

#include "SceTypes.hpp"

class MutexOperations {
public:
    static int Timedlock(PthreadMutex* mutex, const KernelTimespec* abstime);
};

extern "C" {

int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type);
int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol);
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexTimedlock(PthreadMutex* mutex, KernelUseconds usec);
int APS5_VABI scePthreadMutexTrylock(PthreadMutex* mutex);

}

#endif
