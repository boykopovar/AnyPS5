#pragma once

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSpGetPid();
int APS5_VABI sceSpKernelGettimeofday(KernelTimeval* tp);
int APS5_VABI sceSpKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp);
int APS5_VABI sceSpNetConnect(int s, const void* addr, std::uint32_t addrlen);
int APS5_VABI sceSpNetEpollAbort(int eid, int flags);
int APS5_VABI sceSpNetEpollControl(int eid, int op, int id, const void* event);
int APS5_VABI sceSpNetEpollCreate(const char* name, int flags);
int APS5_VABI sceSpNetEpollDestroy(int eid);
int APS5_VABI sceSpNetEpollWait(int eid, void* events, int maxevents, int timeout);
int* APS5_VABI sceSpNetErrnoLoc();
int APS5_VABI sceSpNetPoolCreate(const char* name, int size, int flags);
int APS5_VABI sceSpNetPoolDestroy(int memid);
std::int64_t APS5_VABI sceSpNetRecv(int s, void* buf, std::size_t len, int flags);
int APS5_VABI sceSpNetResolverAbort(int rid, int flags);
int APS5_VABI sceSpNetResolverCreate(const char* name, int memid, int flags);
int APS5_VABI sceSpNetResolverDestroy(int rid);
int APS5_VABI sceSpNetResolverGetError(int rid, int* status);
int APS5_VABI sceSpNetResolverStartNtoa(int rid, const char* hostname, void* addr, int timeout, int retry, int flags);
std::int64_t APS5_VABI sceSpNetSend(int s, const void* buf, std::size_t len, int flags);
int APS5_VABI sceSpNetSetsockopt(int s, int level, int optname, const void* optval, std::uint32_t optlen);
int APS5_VABI sceSpNetSocket(const char* name, int family, int type, int protocol);
int APS5_VABI sceSpNetSocketClose(int s);
int APS5_VABI sceSpPthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI sceSpPthreadAttrInit(PthreadAttr* attr);
int APS5_VABI sceSpPthreadAttrSetinheritsched(PthreadAttr* attr, int inheritSched);
int APS5_VABI sceSpPthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param);
int APS5_VABI sceSpPthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stackSize);
int APS5_VABI sceSpPthreadCondDestroy(PthreadCond* cond);
int APS5_VABI sceSpPthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name);
int APS5_VABI sceSpPthreadCondSignal(PthreadCond* cond);
int APS5_VABI sceSpPthreadCondWait(PthreadCond* cond, PthreadMutex* mutex);
int APS5_VABI sceSpPthreadCondattrDestroy(PthreadCondattr* attr);
int APS5_VABI sceSpPthreadCondattrInit(PthreadCondattr* attr);
int APS5_VABI sceSpPthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI sceSpPthreadDetach(Pthread thread);
void APS5_VABI sceSpPthreadExit(void* retval);
int APS5_VABI sceSpPthreadJoin(Pthread thread, void** retval);
int APS5_VABI sceSpPthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI sceSpPthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI sceSpPthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI sceSpPthreadMutexUnlock(PthreadMutex* mutex);
int APS5_VABI sceSpPthreadMutexattrDestroy(PthreadMutexattr* attr);
int APS5_VABI sceSpPthreadMutexattrInit(PthreadMutexattr* attr);
int APS5_VABI sceSpPthreadMutexattrSetprioceiling(PthreadMutexattr* attr, int prioceiling);
int APS5_VABI sceSpPthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol);
int APS5_VABI sceSpPthreadMutexattrSettype(PthreadMutexattr* attr, int type);
int APS5_VABI sceSpPthreadSetprio(Pthread thread, int prio);
int APS5_VABI sceSpSelect(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout);

}
