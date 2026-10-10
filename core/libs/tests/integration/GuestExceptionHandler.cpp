#include "SceTypes.hpp"
#include <cstdlib>
#include <initializer_list>

extern "C" {
int APS5_VABI sceKernelInstallExceptionHandler(int signum, void* handler);
int APS5_VABI sceKernelRemoveExceptionHandler(int signum);
}

static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int SCE_KERNEL_ERROR_EAGAIN = static_cast<int>(0x80020023);

static void APS5_VABI Handler(int, void*) {}
static void APS5_VABI Other(int, void*) {}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    auto* handler = reinterpret_cast<void*>(&Handler);
    auto* other = reinterpret_cast<void*>(&Other);
    for (const int rejected : {0, 2, 9, 31, 128, -1})
        Require(sceKernelInstallExceptionHandler(rejected, handler) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelInstallExceptionHandler(30, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelRemoveExceptionHandler(9) == SCE_KERNEL_ERROR_EINVAL);
    for (const int signum : {1, 4, 8, 10, 11, 30}) {
        Require(sceKernelInstallExceptionHandler(signum, handler) == 0);
        Require(sceKernelInstallExceptionHandler(signum, other) == SCE_KERNEL_ERROR_EAGAIN);
        Require(sceKernelRemoveExceptionHandler(signum) == 0);
        Require(sceKernelInstallExceptionHandler(signum, other) == 0);
        Require(sceKernelRemoveExceptionHandler(signum) == 0);
    }
    Require(sceKernelRemoveExceptionHandler(30) == 0);
}
