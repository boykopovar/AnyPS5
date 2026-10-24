#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceSpGetPid();
int APS5_VABI sceSpSelect(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout);
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
std::int64_t APS5_VABI sceSpNetSend(int s, const void* buf, std::size_t len, int flags);
int APS5_VABI sceSpNetSetsockopt(int s, int level, int optname, const void* optval, std::uint32_t optlen);
int APS5_VABI sceSpNetSocket(const char* name, int family, int type, int protocol);
int APS5_VABI sceSpNetSocketClose(int s);
int APS5_VABI sceSpPthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI sceSpPthreadAttrInit(PthreadAttr* attr);
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
int APS5_VABI sceSpPthreadMutexattrSettype(PthreadMutexattr* attr, int type);

int* APS5_VABI sceNetErrnoLoc();
int APS5_VABI sceNetConnect(int s, const void* addr, std::uint32_t addrlen);
int APS5_VABI sceNetEpollAbort(int eid, int flags);
int APS5_VABI sceNetEpollControl(int eid, int op, int id, const void* event);
int APS5_VABI sceNetEpollWait(int eid, void* events, int maxevents, int timeout);
std::int64_t APS5_VABI sceNetRecv(int s, void* buf, std::size_t len, int flags);
int APS5_VABI sceNetResolverAbort(int rid, int flags);
int APS5_VABI sceNetResolverGetError(int rid, int* status);
std::int64_t APS5_VABI sceNetSend(int s, const void* buf, std::size_t len, int flags);
int APS5_VABI sceNetSetsockopt(int s, int level, int optname, const void* optval, std::uint32_t optlen);
int APS5_VABI sceSpNetResolverStartNtoa(int rid, const char* hostname, void* addr, int timeout, int retry, int flags);
int APS5_VABI sceSpPthreadAttrSetinheritsched(PthreadAttr* attr, int inheritSched);
int APS5_VABI sceSpPthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param);
int APS5_VABI sceSpPthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol);
int APS5_VABI sceSpPthreadSetprio(Pthread thread, int prio);
int APS5_VABI sceNetResolverStartNtoa(int rid, const char* hostname, void* addr, int timeout, int retry, int flags);
int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inheritSched);
int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param);
int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol);
int APS5_VABI scePthreadSetprio(Pthread thread, int prio);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI getpid_nid_postfix(void);
int APS5_VABI select_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout);
int* APS5_VABI __error_nid_postfix();
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

struct Shared {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    bool ready = false;
};

void* APS5_VABI ExitWithArgument(void* arg) {
    sceSpPthreadExit(arg);
    return nullptr;
}

void* APS5_VABI SignalReady(void* arg) {
    auto* shared = static_cast<Shared*>(arg);
    Require(sceSpPthreadMutexLock(&shared->mutex) == 0);
    shared->ready = true;
    Require(sceSpPthreadCondSignal(&shared->cond) == 0);
    Require(sceSpPthreadMutexUnlock(&shared->mutex) == 0);
    return nullptr;
}

void Threads() {
    PthreadAttr attr = nullptr;
    Require(sceSpPthreadAttrInit(&attr) == 0);
    Require(sceSpPthreadAttrSetstacksize(&attr, 0x10000) == 0);
    Require(sceSpPthreadAttrSetinheritsched(&attr, 0) == scePthreadAttrSetinheritsched(&attr, 0));
    const KernelSchedParam param{700};
    Require(sceSpPthreadAttrSetschedparam(&attr, &param) == scePthreadAttrSetschedparam(&attr, &param));

    Pthread thread = nullptr;
    int marker = 0;
    Require(sceSpPthreadCreate(&thread, &attr, ExitWithArgument, &marker, "sp_exit") == 0);
    void* result = nullptr;
    Require(sceSpPthreadJoin(thread, &result) == 0);
    Require(result == &marker);

    Pthread detached = nullptr;
    Require(sceSpPthreadCreate(&detached, &attr, ExitWithArgument, nullptr, "sp_detach") == 0);
    Require(sceSpPthreadDetach(detached) == 0);
    Require(sceSpPthreadAttrDestroy(&attr) == 0);

    PthreadMutexattr mutexAttr = nullptr;
    Require(sceSpPthreadMutexattrInit(&mutexAttr) == 0);
    Require(sceSpPthreadMutexattrSettype(&mutexAttr, 2) == 0);
    Require(sceSpPthreadMutexattrSetprotocol(&mutexAttr, 0) == scePthreadMutexattrSetprotocol(&mutexAttr, 0));
    Require(sceSpPthreadSetprio(scePthreadSelf(), 700) == scePthreadSetprio(scePthreadSelf(), 700));
    PthreadCondattr condAttr = nullptr;
    Require(sceSpPthreadCondattrInit(&condAttr) == 0);

    Shared shared;
    Require(sceSpPthreadMutexInit(&shared.mutex, &mutexAttr, "sp_mutex") == 0);
    Require(sceSpPthreadCondInit(&shared.cond, &condAttr, "sp_cond") == 0);
    Require(sceSpPthreadMutexattrDestroy(&mutexAttr) == 0);
    Require(sceSpPthreadCondattrDestroy(&condAttr) == 0);

    Require(sceSpPthreadMutexLock(&shared.mutex) == 0);
    Pthread signaller = nullptr;
    Require(sceSpPthreadCreate(&signaller, nullptr, SignalReady, &shared, "sp_signal") == 0);
    while (!shared.ready) Require(sceSpPthreadCondWait(&shared.cond, &shared.mutex) == 0);
    Require(sceSpPthreadMutexUnlock(&shared.mutex) == 0);
    Require(sceSpPthreadJoin(signaller, nullptr) == 0);
    Require(sceSpPthreadCondDestroy(&shared.cond) == 0);
    Require(sceSpPthreadMutexDestroy(&shared.mutex) == 0);
}

void Time() {
    KernelTimeval now{};
    Require(sceSpKernelGettimeofday(&now) == 0);
    Require(now.tv_sec > 0 && now.tv_usec >= 0 && now.tv_usec < 1000000);
    const KernelTimespec delay{0, 1000000};
    Require(sceSpKernelNanosleep(&delay, nullptr) == 0);
}

template <typename TWrapped, typename TDirect>
void SameError(TWrapped wrapped, TDirect direct) {
    const auto wrappedResult = wrapped();
    const int wrappedErrno = *sceNetErrnoLoc();
    const auto directResult = direct();
    Require(wrappedResult == directResult);
    Require(wrappedErrno == *sceNetErrnoLoc());
}

void Net() {
    Require(sceSpNetErrnoLoc() == sceNetErrnoLoc());

    const int pool = sceSpNetPoolCreate("sp_pool", 0x4000, 0);
    Require(pool >= 0);
    const int resolver = sceSpNetResolverCreate("sp_resolver", pool, 0);
    Require(resolver >= 0);
    int status = 0;
    Require(sceSpNetResolverGetError(resolver, &status) == sceNetResolverGetError(resolver, &status));
    Require(sceSpNetResolverDestroy(resolver) == 0);
    Require(sceSpNetPoolDestroy(pool) == 0);

    const int epoll = sceSpNetEpollCreate("sp_epoll", 0);
    Require(epoll >= 0);
    Require(sceSpNetEpollDestroy(epoll) == 0);

    const int socket = sceSpNetSocket("sp_socket", 2, 2, 0);
    Require(socket >= 0);
    const int enable = 1;
    Require(sceSpNetSetsockopt(socket, 0xffff, 0x4, &enable, sizeof(enable)) == sceNetSetsockopt(socket, 0xffff, 0x4, &enable, sizeof(enable)));
    Require(sceSpNetSocketClose(socket) == 0);

    char byte = 0;
    const std::uint8_t address[16]{16, 2};
    SameError([&] { return sceSpNetSend(-1, &byte, 1, 0); }, [&] { return sceNetSend(-1, &byte, 1, 0); });
    SameError([&] { return sceSpNetRecv(-1, &byte, 1, 0); }, [&] { return sceNetRecv(-1, &byte, 1, 0); });
    SameError([&] { return sceSpNetConnect(-1, address, sizeof(address)); }, [&] { return sceNetConnect(-1, address, sizeof(address)); });
    SameError([&] { return sceSpNetEpollControl(-1, 1, 0, nullptr); }, [&] { return sceNetEpollControl(-1, 1, 0, nullptr); });
    SameError([&] { return sceSpNetEpollWait(-1, nullptr, 0, 0); }, [&] { return sceNetEpollWait(-1, nullptr, 0, 0); });
    SameError([&] { return sceSpNetEpollAbort(-1, 0); }, [&] { return sceNetEpollAbort(-1, 0); });
    SameError([&] { return sceSpNetResolverAbort(-1, 0); }, [&] { return sceNetResolverAbort(-1, 0); });
    std::uint32_t resolved = 0;
    SameError([&] { return sceSpNetResolverStartNtoa(-1, "localhost", &resolved, 0, 0, 0); }, [&] { return sceNetResolverStartNtoa(-1, "localhost", &resolved, 0, 0, 0); });
}

void Posix() {
    Require(sceSpGetPid() > 0);
    Require(sceSpGetPid() == getpid_nid_postfix());

    const std::int64_t zero[2]{0, 0};
    Require(sceSpSelect(0, nullptr, nullptr, nullptr, zero) == 0);
    const std::int64_t invalid[2]{0, 1000000};
    *__error_nid_postfix() = 0;
    Require(sceSpSelect(0, nullptr, nullptr, nullptr, invalid) == -1);
    const int wrappedErrno = *__error_nid_postfix();
    *__error_nid_postfix() = 0;
    Require(select_nid_postfix(0, nullptr, nullptr, nullptr, invalid) == -1);
    Require(wrappedErrno == 22 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(sceSpSelect(-1, nullptr, nullptr, nullptr, nullptr) == -1 && *__error_nid_postfix() == 22);
}

}

int main() {
    Threads();
    Time();
    Net();
    Posix();
}
