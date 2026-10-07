#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <array>
#include <mutex>

namespace {

constexpr std::array<int, 6> AllowedSignals{1, 4, 8, 10, 11, 30};

std::mutex handlersLock;
std::array<void*, 32> handlers{};

bool Allowed(int signum) {
    for (const int allowed : AllowedSignals)
        if (allowed == signum)
            return true;
    return false;
}

}

extern "C" {

int APS5_VABI sceKernelInstallExceptionHandler(int signum, void* handler) {
    if (!Allowed(signum) || handler == nullptr)
        return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(handlersLock);
    if (handlers[signum] != nullptr)
        return SCE_KERNEL_ERROR_EAGAIN;
    handlers[signum] = handler;
    return 0;
}

int APS5_VABI sceKernelRemoveExceptionHandler(int signum) {
    if (!Allowed(signum))
        return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(handlersLock);
    handlers[signum] = nullptr;
    return 0;
}

int APS5_VABI sceKernelRaiseException(Pthread thread, int signum) {
    (void)thread;
    (void)signum;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceKernelDebugRaiseException(int c1, int c2) {
    APS5_LOG_OUT("sceKernelDebugRaiseException c1=%d c2=%d", c1, c2);
}

void APS5_VABI sceKernelDebugRaiseExceptionOnReleaseMode(int c1, int c2) {
    APS5_LOG_OUT("sceKernelDebugRaiseExceptionOnReleaseMode c1=%d c2=%d", c1, c2);
}
}
