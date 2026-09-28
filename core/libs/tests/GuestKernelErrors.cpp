#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);
int APS5_VABI sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo);
int APS5_VABI sceKernelDeleteUserEvent(KernelEqueue eq, int id);
int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type);
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_ENOENT = static_cast<int>(0x80020002);
static constexpr int SCE_KERNEL_ERROR_EBADF = static_cast<int>(0x80020009);
static constexpr int SCE_KERNEL_ERROR_EDEADLK = static_cast<int>(0x8002000B);
static constexpr int SCE_KERNEL_ERROR_EFAULT = static_cast<int>(0x8002000E);
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003C);
static constexpr int MUTEX_TYPE_ERRORCHECK = 1;

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "errors") == SCE_OK);
    KernelEvent event{};
    int count = -1;
    const KernelUseconds timeout = 1000;
    Require(sceKernelWaitEqueue(eq, &event, 1, &count, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(count == 0);
    Require(sceKernelWaitEqueue(eq, nullptr, 1, &count, &timeout) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelWaitEqueue(eq, &event, 0, &count, &timeout) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelDeleteUserEvent(eq, 7) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);
    Require(sceKernelDeleteEqueue(eq) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelWaitEqueue(eq, &event, 1, &count, &timeout) == SCE_KERNEL_ERROR_EBADF);
    Require(sceKernelCreateEqueue(nullptr, "errors") == SCE_KERNEL_ERROR_EINVAL);

    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    Require(scePthreadMutexattrSettype(&attr, MUTEX_TYPE_ERRORCHECK) == SCE_OK);
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, &attr, nullptr) == SCE_OK);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
    Require(scePthreadMutexLock(&mutex) == SCE_OK);
    Require(scePthreadMutexLock(&mutex) == SCE_KERNEL_ERROR_EDEADLK);
    Require(scePthreadMutexUnlock(&mutex) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
}
