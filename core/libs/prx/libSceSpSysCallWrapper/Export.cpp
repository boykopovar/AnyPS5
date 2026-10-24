#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::int64_t APS5_VABI GuestNetRecv_nid_no_patch(int s, void* buf, std::size_t len, int flags);
std::int64_t APS5_VABI GuestNetSend_nid_no_patch(int s, const void* buf, std::size_t len, int flags);
int APS5_VABI getpid_nid_postfix(void);
int APS5_VABI sceKernelGettimeofday(KernelTimeval* tp);
int APS5_VABI sceKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp);
int APS5_VABI sceNetConnect(int s, const void* addr, std::uint32_t addrlen);
int APS5_VABI sceNetEpollAbort(int eid, int flags);
int APS5_VABI sceNetEpollControl(int eid, int op, int id, const void* event);
int APS5_VABI sceNetEpollCreate(const char* name, int flags);
int APS5_VABI sceNetEpollDestroy(int eid);
int APS5_VABI sceNetEpollWait(int eid, void* events, int maxevents, int timeout);
int* APS5_VABI sceNetErrnoLoc(void);
int APS5_VABI sceNetPoolCreate(const char* name, int size, int flags);
int APS5_VABI sceNetPoolDestroy(int memid);
int APS5_VABI sceNetResolverAbort(int rid, int flags);
int APS5_VABI sceNetResolverCreate(const char* name, int memid, int flags);
int APS5_VABI sceNetResolverDestroy(int rid);
int APS5_VABI sceNetResolverGetError(int rid, int* status);
int APS5_VABI sceNetResolverStartNtoa(int rid, const char* hostname, void* addr, int timeout, int retry, int flags);
int APS5_VABI sceNetSetsockopt(int s, int level, int optname, const void* optval, std::uint32_t optlen);
int APS5_VABI sceNetSocket(const char* name, int family, int type, int protocol);
int APS5_VABI sceNetSocketClose(int s);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inheritSched);
int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param);
int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stackSize);
int APS5_VABI scePthreadCondDestroy(PthreadCond* cond);
int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name);
int APS5_VABI scePthreadCondSignal(PthreadCond* cond);
int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex);
int APS5_VABI scePthreadCondattrDestroy(PthreadCondattr* attr);
int APS5_VABI scePthreadCondattrInit(PthreadCondattr* attr);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadDetach(Pthread thread);
void APS5_VABI scePthreadExit(void* retval);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol);
int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type);
int APS5_VABI scePthreadSetprio(Pthread thread, int prio);
int APS5_VABI select_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout);

int APS5_VABI sceSpGetPid() {
    return getpid_nid_postfix();
}

int APS5_VABI sceSpKernelGettimeofday(KernelTimeval* tp) {
    return sceKernelGettimeofday(tp);
}

int APS5_VABI sceSpKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return sceKernelNanosleep(rqtp, rmtp);
}

int APS5_VABI sceSpNetConnect(int s, const void* addr, std::uint32_t addrlen) {
    return sceNetConnect(s, addr, addrlen);
}

int APS5_VABI sceSpNetEpollAbort(int eid, int flags) {
    return sceNetEpollAbort(eid, flags);
}

int APS5_VABI sceSpNetEpollControl(int eid, int op, int id, const void* event) {
    return sceNetEpollControl(eid, op, id, event);
}

int APS5_VABI sceSpNetEpollCreate(const char* name, int flags) {
    return sceNetEpollCreate(name, flags);
}

int APS5_VABI sceSpNetEpollDestroy(int eid) {
    return sceNetEpollDestroy(eid);
}

int APS5_VABI sceSpNetEpollWait(int eid, void* events, int maxevents, int timeout) {
    return sceNetEpollWait(eid, events, maxevents, timeout);
}

int* APS5_VABI sceSpNetErrnoLoc() {
    return sceNetErrnoLoc();
}

int APS5_VABI sceSpNetPoolCreate(const char* name, int size, int flags) {
    return sceNetPoolCreate(name, size, flags);
}

int APS5_VABI sceSpNetPoolDestroy(int memid) {
    return sceNetPoolDestroy(memid);
}

std::int64_t APS5_VABI sceSpNetRecv(int s, void* buf, std::size_t len, int flags) {
    return GuestNetRecv_nid_no_patch(s, buf, len, flags);
}

int APS5_VABI sceSpNetResolverAbort(int rid, int flags) {
    return sceNetResolverAbort(rid, flags);
}

int APS5_VABI sceSpNetResolverCreate(const char* name, int memid, int flags) {
    return sceNetResolverCreate(name, memid, flags);
}

int APS5_VABI sceSpNetResolverDestroy(int rid) {
    return sceNetResolverDestroy(rid);
}

int APS5_VABI sceSpNetResolverGetError(int rid, int* status) {
    return sceNetResolverGetError(rid, status);
}

int APS5_VABI sceSpNetResolverStartNtoa(int rid, const char* hostname, void* addr, int timeout, int retry, int flags) {
    return sceNetResolverStartNtoa(rid, hostname, addr, timeout, retry, flags);
}

std::int64_t APS5_VABI sceSpNetSend(int s, const void* buf, std::size_t len, int flags) {
    return GuestNetSend_nid_no_patch(s, buf, len, flags);
}

int APS5_VABI sceSpNetSetsockopt(int s, int level, int optname, const void* optval, std::uint32_t optlen) {
    return sceNetSetsockopt(s, level, optname, optval, optlen);
}

int APS5_VABI sceSpNetSocket(const char* name, int family, int type, int protocol) {
    return sceNetSocket(name, family, type, protocol);
}

int APS5_VABI sceSpNetSocketClose(int s) {
    return sceNetSocketClose(s);
}

int APS5_VABI sceSpPthreadAttrDestroy(PthreadAttr* attr) {
    return scePthreadAttrDestroy(attr);
}

int APS5_VABI sceSpPthreadAttrInit(PthreadAttr* attr) {
    return scePthreadAttrInit(attr);
}

int APS5_VABI sceSpPthreadAttrSetinheritsched(PthreadAttr* attr, int inheritSched) {
    return scePthreadAttrSetinheritsched(attr, inheritSched);
}

int APS5_VABI sceSpPthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param) {
    return scePthreadAttrSetschedparam(attr, param);
}

int APS5_VABI sceSpPthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stackSize) {
    return scePthreadAttrSetstacksize(attr, stackSize);
}

int APS5_VABI sceSpPthreadCondDestroy(PthreadCond* cond) {
    return scePthreadCondDestroy(cond);
}

int APS5_VABI sceSpPthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name) {
    return scePthreadCondInit(cond, attr, name);
}

int APS5_VABI sceSpPthreadCondSignal(PthreadCond* cond) {
    return scePthreadCondSignal(cond);
}

int APS5_VABI sceSpPthreadCondWait(PthreadCond* cond, PthreadMutex* mutex) {
    return scePthreadCondWait(cond, mutex);
}

int APS5_VABI sceSpPthreadCondattrDestroy(PthreadCondattr* attr) {
    return scePthreadCondattrDestroy(attr);
}

int APS5_VABI sceSpPthreadCondattrInit(PthreadCondattr* attr) {
    return scePthreadCondattrInit(attr);
}

int APS5_VABI sceSpPthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    return scePthreadCreate(thread, attr, entry, arg, name);
}

int APS5_VABI sceSpPthreadDetach(Pthread thread) {
    return scePthreadDetach(thread);
}

void APS5_VABI sceSpPthreadExit(void* retval) {
    scePthreadExit(retval);
}

int APS5_VABI sceSpPthreadJoin(Pthread thread, void** retval) {
    return scePthreadJoin(thread, retval);
}

int APS5_VABI sceSpPthreadMutexDestroy(PthreadMutex* mutex) {
    return scePthreadMutexDestroy(mutex);
}

int APS5_VABI sceSpPthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name) {
    return scePthreadMutexInit(mutex, attr, name);
}

int APS5_VABI sceSpPthreadMutexLock(PthreadMutex* mutex) {
    return scePthreadMutexLock(mutex);
}

int APS5_VABI sceSpPthreadMutexUnlock(PthreadMutex* mutex) {
    return scePthreadMutexUnlock(mutex);
}

int APS5_VABI sceSpPthreadMutexattrDestroy(PthreadMutexattr* attr) {
    return scePthreadMutexattrDestroy(attr);
}

int APS5_VABI sceSpPthreadMutexattrInit(PthreadMutexattr* attr) {
    return scePthreadMutexattrInit(attr);
}

int APS5_VABI sceSpPthreadMutexattrSetprioceiling() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSpPthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol) {
    return scePthreadMutexattrSetprotocol(attr, protocol);
}

int APS5_VABI sceSpPthreadMutexattrSettype(PthreadMutexattr* attr, int type) {
    return scePthreadMutexattrSettype(attr, type);
}

int APS5_VABI sceSpPthreadSetprio(Pthread thread, int prio) {
    return scePthreadSetprio(thread, prio);
}

int APS5_VABI sceSpSelect(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout) {
    return select_nid_postfix(nfds, readfds, writefds, exceptfds, timeout);
}

}
