#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>

extern "C" {
int APS5_VABI sceKernelCreateSema(KernelSema*, const char*, std::uint32_t, int, int, void*);
int APS5_VABI sceKernelDeleteSema(KernelSema);
int APS5_VABI sceKernelSignalSema(KernelSema, int);
int APS5_VABI sceKernelPollSema(KernelSema, int);
int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);
int APS5_VABI sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo);
int APS5_VABI sceKernelDeleteUserEvent(KernelEqueue eq, int id);
int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr);
int APS5_VABI pthread_mutexattr_init_nid_postfix(PthreadMutexattr* attr);
int APS5_VABI pthread_mutexattr_settype_nid_postfix(PthreadMutexattr* attr, int type);
int APS5_VABI pthread_mutexattr_destroy_nid_postfix(PthreadMutexattr* attr);
int APS5_VABI pthread_mutex_init_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr);
int APS5_VABI pthread_mutex_lock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_unlock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type);
int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol);
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
static constexpr int PRIO_NONE = 0;
static constexpr int PRIO_INHERIT = 1;
static constexpr int PRIO_PROTECT = 2;
static constexpr int MUTEX_TYPE_ADAPTIVE = 4;
static constexpr int POSIX_EDEADLK = 11;

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int maximum = std::numeric_limits<int>::max();
    KernelSema semaphore = nullptr;
    Require(sceKernelCreateSema(&semaphore, "overflow", 1, maximum - 1, maximum, nullptr) == SCE_OK);
    Require(sceKernelSignalSema(semaphore, 2) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelSignalSema(semaphore, maximum) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(semaphore, maximum - 1) == SCE_OK);
    Require(sceKernelSignalSema(semaphore, maximum) == SCE_OK);
    Require(sceKernelSignalSema(semaphore, 1) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(semaphore, maximum) == SCE_OK);
    Require(sceKernelDeleteSema(semaphore) == SCE_OK);
    Require(sceKernelCreateSema(&semaphore, "limit", 1, 1, 3, nullptr) == SCE_OK);
    Require(sceKernelSignalSema(semaphore, 3) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelSignalSema(semaphore, 2) == SCE_OK);
    Require(sceKernelPollSema(semaphore, 3) == SCE_OK);
    Require(sceKernelDeleteSema(semaphore) == SCE_OK);

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
    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_NONE) == SCE_OK);
    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_INHERIT) == SCE_OK);
    bool protectionRejected = false;
    try {
        scePthreadMutexattrSetprotocol(&attr, PRIO_PROTECT);
    } catch (const std::invalid_argument&) {
        protectionRejected = true;
    }
    Require(protectionRejected);
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, &attr, nullptr) == SCE_OK);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
    Require(scePthreadMutexLock(&mutex) == SCE_OK);
    Require(scePthreadMutexLock(&mutex) == SCE_KERNEL_ERROR_EDEADLK);
    Require(scePthreadMutexUnlock(&mutex) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);

    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    Require(scePthreadMutexattrSettype(&attr, MUTEX_TYPE_ADAPTIVE) == SCE_OK);
    PthreadMutex adaptive = nullptr;
    Require(scePthreadMutexInit(&adaptive, &attr, nullptr) == SCE_OK);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
    Require(scePthreadMutexLock(&adaptive) == SCE_OK);
    Require(scePthreadMutexLock(&adaptive) == SCE_KERNEL_ERROR_EDEADLK);
    Require(scePthreadMutexUnlock(&adaptive) == SCE_OK);
    Require(scePthreadMutexDestroy(&adaptive) == SCE_OK);

    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    PthreadMutex defaulted = nullptr;
    Require(scePthreadMutexInit(&defaulted, &attr, nullptr) == SCE_OK);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
    Require(scePthreadMutexLock(&defaulted) == SCE_OK);
    Require(scePthreadMutexLock(&defaulted) == SCE_KERNEL_ERROR_EDEADLK);
    Require(scePthreadMutexUnlock(&defaulted) == SCE_OK);
    Require(scePthreadMutexDestroy(&defaulted) == SCE_OK);

    PthreadMutex unattributed = nullptr;
    Require(scePthreadMutexInit(&unattributed, nullptr, nullptr) == SCE_OK);
    Require(scePthreadMutexLock(&unattributed) == SCE_OK);
    Require(scePthreadMutexLock(&unattributed) == SCE_KERNEL_ERROR_EDEADLK);
    Require(scePthreadMutexUnlock(&unattributed) == SCE_OK);
    Require(scePthreadMutexDestroy(&unattributed) == SCE_OK);

    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, MUTEX_TYPE_ADAPTIVE) == 0);
    PthreadMutex posixAdaptive = nullptr;
    Require(pthread_mutex_init_nid_postfix(&posixAdaptive, &attr) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&posixAdaptive) == 0);
    Require(pthread_mutex_lock_nid_postfix(&posixAdaptive) == POSIX_EDEADLK);
    Require(pthread_mutex_unlock_nid_postfix(&posixAdaptive) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&posixAdaptive) == 0);

    auto staticAdaptive = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
    Require(pthread_mutex_lock_nid_postfix(&staticAdaptive) == 0);
    Require(pthread_mutex_lock_nid_postfix(&staticAdaptive) == POSIX_EDEADLK);
    Require(pthread_mutex_unlock_nid_postfix(&staticAdaptive) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&staticAdaptive) == 0);
}
