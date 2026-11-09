#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <utility>
#include <stdexcept>

struct AudiodecCtrl;
struct AudiodecCpuMemory;

extern "C" {
int APS5_VABI sceCustomMusicAudiodecClearContext(int32_t handle);
int APS5_VABI sceCustomMusicFileOpenRead(const char* path, int flags, std::uint16_t mode);
int APS5_VABI sceCustomMusicKernelGettimeofday(KernelTimeval* tp);
int APS5_VABI sceCustomMusicNetPoolCreate(const char* name, int size, int flags);
int APS5_VABI sceCustomMusicPthreadMutexLock(PthreadMutex* mutex);
Pthread APS5_VABI sceCustomMusicPthreadSelf();
void APS5_VABI sceCustomMusicPthreadYield();
int APS5_VABI sceCustomMusicNetCtlInit();
void APS5_VABI sceCustomMusicNetCtlTerm();

int32_t APS5_VABI sceAudiodecClearContext(int32_t handle);
int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode);
int APS5_VABI sceKernelGettimeofday(KernelTimeval* tp);
int APS5_VABI sceNetPoolCreate(const char* name, int size, int flags);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI sceCustomMusicNetShutdown(int s, int how);
int APS5_VABI sceCustomMusicNetSocketAbort(int s, int flags);
int APS5_VABI sceCustomMusicAudiodecDeleteDecoder(int32_t handle);
int APS5_VABI sceCustomMusicAudiodecCpuDecode(const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, uint32_t type);
int APS5_VABI sceCustomMusicKernelNanosleep(const KernelTimespec* rq, KernelTimespec* rm);
int APS5_VABI sceCustomMusicPthreadSetprio(Pthread thread, int prio);
int APS5_VABI sceCustomMusicNetBind(int s, const void* addr, uint32_t addrlen);
int APS5_VABI sceCustomMusicSelect(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout);
int* APS5_VABI sceCustomMusicNetErrnoLoc();
const char* APS5_VABI sceCustomMusicNetInetNtop(int af, const void* src, char* dst, uint32_t size);
const char* APS5_VABI sceNetInetNtop(int af, const void* src, char* dst, uint32_t size);
int APS5_VABI sceNetBind_nid_postfix(int s, const void* addr, uint32_t addrlen);
int APS5_VABI select_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout);
int* APS5_VABI sceNetErrnoLoc(void);
int APS5_VABI sceNetShutdown(int s, int how);
int APS5_VABI sceNetSocketAbort(int s, int flags);
int32_t APS5_VABI sceAudiodecDeleteDecoder(int32_t handle);
int32_t APS5_VABI sceAudiodecCpuDecode(const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, uint32_t type);
int APS5_VABI sceKernelNanosleep(const KernelTimespec* rq, KernelTimespec* rm);
int APS5_VABI scePthreadSetprio(Pthread thread, int prio);
Pthread APS5_VABI scePthreadSelf();
void APS5_VABI scePthreadYield();
int APS5_VABI sceNetCtlInit(void);
void APS5_VABI sceNetCtlTerm(void);
}

namespace {

void Require(bool value, unsigned line) {
    if (!value) {
        std::fprintf(stderr, "libSceCustomMusicSysCallWrapper mismatch at line %u\n", line);
        std::abort();
    }
}

void RequireEqual(auto wrapper, auto callee, unsigned line) {
    Require(wrapper == callee, line);
}

template <typename Wrapper, typename Callee, typename... Args>
void RequireSameOutcome(Wrapper wrapper, Callee callee, unsigned line, Args&&... args) {
    int wrapper_result = 0;
    int callee_result = 0;
    bool wrapper_threw = false;
    bool callee_threw = false;
    try {
        wrapper_result = static_cast<int>(wrapper(std::forward<Args>(args)...));
    } catch (const std::exception&) {
        wrapper_threw = true;
    }
    try {
        callee_result = static_cast<int>(callee(std::forward<Args>(args)...));
    } catch (const std::exception&) {
        callee_threw = true;
    }
    Require(wrapper_threw == callee_threw && (wrapper_threw || wrapper_result == callee_result), line);
}

}

#define RequireEq(a, b) RequireEqual(a, b, __LINE__)

int main() {
    KernelTimeval wrapper_tv {};
    KernelTimeval callee_tv {};
    RequireEq(sceCustomMusicAudiodecClearContext(1), sceAudiodecClearContext(1));
    RequireEq(sceCustomMusicAudiodecClearContext(0), sceAudiodecClearContext(0));
    RequireEq(sceCustomMusicFileOpenRead("/custom_music_forwarder_missing_file", 0, 0),
              sceKernelOpen("/custom_music_forwarder_missing_file", 0, 0));
    RequireEq(sceCustomMusicFileOpenRead(nullptr, 0, 0), sceKernelOpen(nullptr, 0, 0));
    Require(sceCustomMusicKernelGettimeofday(&wrapper_tv) == sceKernelGettimeofday(&callee_tv), __LINE__);
    RequireEq(sceCustomMusicKernelGettimeofday(nullptr), sceKernelGettimeofday(nullptr));
    const int pool_a = sceCustomMusicNetPoolCreate("anyps5-music-pool", 0, 0);
    const int pool_b = sceNetPoolCreate("anyps5-music-pool", 0, 0);
    Require((pool_a < 0) == (pool_b < 0) && (pool_a < 0 ? pool_a == pool_b : true), __LINE__);
    {
        bool wrapper_threw = false;
        bool callee_threw = false;
        try { sceCustomMusicPthreadMutexLock(nullptr); } catch (const std::invalid_argument&) { wrapper_threw = true; }
        try { scePthreadMutexLock(nullptr); } catch (const std::invalid_argument&) { callee_threw = true; }
        Require(wrapper_threw && callee_threw, __LINE__);
    }
    {
        Pthread wrapper_self = sceCustomMusicPthreadSelf();
        Pthread callee_self = scePthreadSelf();
        Require(wrapper_self != nullptr && callee_self != nullptr && wrapper_self == callee_self, __LINE__);
    }
    sceCustomMusicPthreadYield();
    scePthreadYield();
    Require(sceCustomMusicNetCtlInit() == sceNetCtlInit(), __LINE__);
    sceCustomMusicNetCtlTerm();
    RequireSameOutcome(sceCustomMusicNetShutdown, sceNetShutdown, __LINE__, -1, 0);
    RequireSameOutcome(sceCustomMusicNetSocketAbort, sceNetSocketAbort, __LINE__, -1, 0);
    RequireSameOutcome(sceCustomMusicAudiodecDeleteDecoder, sceAudiodecDeleteDecoder, __LINE__, -1);
    RequireSameOutcome(sceCustomMusicAudiodecCpuDecode, sceAudiodecCpuDecode, __LINE__, nullptr, nullptr, 0);
    RequireSameOutcome(sceCustomMusicKernelNanosleep, sceKernelNanosleep, __LINE__, nullptr, nullptr);
    RequireSameOutcome(sceCustomMusicPthreadSetprio, scePthreadSetprio, __LINE__, nullptr, 0);
    Require(sceCustomMusicNetCtlInit() == 0, __LINE__);
    {
        const unsigned char address[4] = {192, 0, 2, 5};
        std::array<char, 46> wrapper_text {};
        std::array<char, 46> callee_text {};
        Require(sceCustomMusicNetInetNtop(2, address, wrapper_text.data(), static_cast<std::uint32_t>(wrapper_text.size())) == wrapper_text.data(), __LINE__);
        Require(sceNetInetNtop(2, address, callee_text.data(), static_cast<std::uint32_t>(callee_text.size())) == callee_text.data(), __LINE__);
        Require(std::strcmp(wrapper_text.data(), "192.0.2.5") == 0, __LINE__);
        Require(std::strcmp(wrapper_text.data(), callee_text.data()) == 0, __LINE__);
        Require(sceCustomMusicNetInetNtop(2, address, wrapper_text.data(), 3) == nullptr, __LINE__);
        Require(sceCustomMusicNetInetNtop(2, nullptr, wrapper_text.data(), static_cast<std::uint32_t>(wrapper_text.size())) == nullptr, __LINE__);
    }
    Require(sceCustomMusicNetErrnoLoc() == sceNetErrnoLoc(), __LINE__);
    Require(sceCustomMusicNetErrnoLoc() != nullptr, __LINE__);
    RequireSameOutcome(sceCustomMusicNetBind, sceNetBind_nid_postfix, __LINE__, -1, static_cast<const void*>(nullptr), 0U);
    RequireSameOutcome(sceCustomMusicSelect, select_nid_postfix, __LINE__, -1, static_cast<void*>(nullptr), static_cast<void*>(nullptr), static_cast<void*>(nullptr), static_cast<const void*>(nullptr));
    return 0;
}
