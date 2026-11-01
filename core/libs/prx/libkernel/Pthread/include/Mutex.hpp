#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_MUTEX_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_MUTEX_HPP

#include "SceTypes.hpp"
#include <mutex>

class MutexOperations {
public:
    static int Timedlock(PthreadMutex* mutex, const KernelTimespec* abstime);
    static bool IsOwned(const PthreadMutex* mutex);
    static bool SpinAcquire(PthreadMutex mutex, std::timed_mutex& native);
    static bool SpinAcquire(PthreadMutex mutex, std::recursive_timed_mutex& native);

    template<typename TTry, typename TPause, typename TYield>
    static bool SpinAcquire(int spins, int yields, TTry tryAcquire, TPause pause, TYield yield) {
        if (tryAcquire())
            return true;
        for (int spin = 0; spin < spins; ++spin) {
            if (tryAcquire())
                return true;
            pause();
        }
        for (int round = 0; round < yields; ++round) {
            yield();
            if (tryAcquire())
                return true;
        }
        return false;
    }
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
int APS5_VABI scePthreadMutexGetspinloops(PthreadMutex* mutex, int* count);
int APS5_VABI scePthreadMutexSetspinloops(PthreadMutex* mutex, int count);
int APS5_VABI scePthreadMutexGetyieldloops(PthreadMutex* mutex, int* count);
int APS5_VABI scePthreadMutexSetyieldloops(PthreadMutex* mutex, int count);

}

#endif
