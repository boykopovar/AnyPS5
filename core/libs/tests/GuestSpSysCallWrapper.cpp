#include "SceTypes.hpp"
#include "prx/libSceSpSysCallWrapper/libSceSpSysCallWrapper.hpp"
#include "prx/libSceNet/libSceNet.hpp"
#include "prx/libkernel/libkernel.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

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
    PthreadMutexattr ceilingAttr = nullptr;
    Require(sceSpPthreadMutexattrInit(&ceilingAttr) == 0);
    Require(sceSpPthreadMutexattrSetprioceiling(&ceilingAttr, 300) == static_cast<int>(0x80020016u));
    Require(sceSpPthreadMutexattrSetprotocol(&ceilingAttr, 2) == 0);
    Require(sceSpPthreadMutexattrSetprioceiling(&ceilingAttr, 300) == 0);
    int ceiling = 0;
    Require(scePthreadMutexattrGetprioceiling(&ceilingAttr, &ceiling) == 0 && ceiling == 300);
    Require(sceSpPthreadMutexattrDestroy(&ceilingAttr) == 0);
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
