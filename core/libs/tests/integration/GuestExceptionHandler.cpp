#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <string>

extern "C" {
int APS5_VABI sceKernelInstallExceptionHandler(int signum, void* handler);
int APS5_VABI sceKernelRemoveExceptionHandler(int signum);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
constexpr int SCE_KERNEL_ERROR_EAGAIN = static_cast<int>(0x80020023);
constexpr int supportedSignals[] = {1, 4, 8, 10, 11, 30};

void APS5_VABI Handler(int, void*) {}
void APS5_VABI Other(int, void*) {}

void* HandlerAddress() {
    return reinterpret_cast<void*>(&Handler);
}

void* OtherAddress() {
    return reinterpret_cast<void*>(&Other);
}

class HandlerCleanup {
public:
    explicit HandlerCleanup(int signum) : signum(signum) {}
    ~HandlerCleanup() { sceKernelRemoveExceptionHandler(signum); }
    HandlerCleanup(const HandlerCleanup&) = delete;
    HandlerCleanup& operator=(const HandlerCleanup&) = delete;

private:
    int signum;
};

std::string Signal(int signum) {
    return "signal " + std::to_string(signum);
}

const Case unsupportedSignal{"InstallExceptionHandler_UnsupportedSignal_FailsWithEinval", [] {
    for (const int rejected : {0, 2, 9, 31, 128, -1}) {
        RequireEqual(sceKernelInstallExceptionHandler(rejected, HandlerAddress()), SCE_KERNEL_ERROR_EINVAL, Signal(rejected));
    }
}};

const Case nullHandler{"InstallExceptionHandler_NullHandler_FailsWithEinval", [] {
    const HandlerCleanup cleanup(30);
    RequireEqual(sceKernelInstallExceptionHandler(30, nullptr), SCE_KERNEL_ERROR_EINVAL, "null handler");
}};

const Case removeUnsupported{"RemoveExceptionHandler_UnsupportedSignal_FailsWithEinval", [] {
    RequireEqual(sceKernelRemoveExceptionHandler(9), SCE_KERNEL_ERROR_EINVAL, "signal 9");
}};

const Case installSupported{"InstallExceptionHandler_SupportedSignal_Succeeds", [] {
    for (const int signum : supportedSignals) {
        const HandlerCleanup cleanup(signum);
        RequireEqual(sceKernelInstallExceptionHandler(signum, HandlerAddress()), 0, Signal(signum));
    }
}};

const Case installTwice{"InstallExceptionHandler_AlreadyInstalled_FailsWithEagain", [] {
    for (const int signum : supportedSignals) {
        const HandlerCleanup cleanup(signum);
        RequireEqual(sceKernelInstallExceptionHandler(signum, HandlerAddress()), 0, Signal(signum) + " first install");
        RequireEqual(sceKernelInstallExceptionHandler(signum, OtherAddress()), SCE_KERNEL_ERROR_EAGAIN,
                     Signal(signum) + " second install");
    }
}};

const Case reinstall{"InstallExceptionHandler_AfterRemove_Succeeds", [] {
    for (const int signum : supportedSignals) {
        const HandlerCleanup cleanup(signum);
        RequireEqual(sceKernelInstallExceptionHandler(signum, HandlerAddress()), 0, Signal(signum) + " install");
        RequireEqual(sceKernelRemoveExceptionHandler(signum), 0, Signal(signum) + " remove");
        RequireEqual(sceKernelInstallExceptionHandler(signum, OtherAddress()), 0, Signal(signum) + " reinstall");
        RequireEqual(sceKernelRemoveExceptionHandler(signum), 0, Signal(signum) + " remove reinstalled");
    }
}};

const Case removeNotInstalled{"RemoveExceptionHandler_NotInstalled_Succeeds", [] {
    RequireEqual(sceKernelRemoveExceptionHandler(30), 0, "signal 30");
}};

} // namespace
