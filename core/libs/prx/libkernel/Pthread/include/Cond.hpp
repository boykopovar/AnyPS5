#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_COND_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_COND_HPP

#include "SceTypes.hpp"

class CondOperations {
public:
    static int AbsoluteTimedwait(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime);
};

extern "C" {

int APS5_VABI scePthreadCondattrInit(PthreadCondattr* attr);
int APS5_VABI scePthreadCondattrDestroy(PthreadCondattr* attr);
int APS5_VABI scePthreadCondattrSetclock(PthreadCondattr* attr, KernelClockid clockId);
int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name);
int APS5_VABI scePthreadCondDestroy(PthreadCond* cond);
int APS5_VABI scePthreadCondSignal(PthreadCond* cond);
int APS5_VABI scePthreadCondBroadcast(PthreadCond* cond);
int APS5_VABI scePthreadCondSignalto(PthreadCond* cond, Pthread thread);
int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex);
int APS5_VABI scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, KernelUseconds usec);

}

#endif
