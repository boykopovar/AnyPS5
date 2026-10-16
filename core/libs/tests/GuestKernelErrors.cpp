#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>

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
int APS5_VABI scePthreadMutexTrylock(PthreadMutex* mutex);
int APS5_VABI sceKernelCreateEventFlag(KernelEventFlag* ef, const char* name, std::uint32_t attr, std::uint64_t init_pattern, const void* param);
int APS5_VABI sceKernelDeleteEventFlag(KernelEventFlag ef);
int APS5_VABI sceKernelSetEventFlag(KernelEventFlag ef, std::uint64_t bit_pattern);
int APS5_VABI sceKernelClearEventFlag(KernelEventFlag ef, std::uint64_t bit_pattern);
int APS5_VABI sceKernelCancelEventFlag(KernelEventFlag ef, std::uint64_t set_pattern, int* num_wait_threads);
int APS5_VABI sceKernelPollEventFlag(KernelEventFlag ef, std::uint64_t bit_pattern, std::uint32_t wait_mode, std::uint64_t* result_pat);
int APS5_VABI sceKernelWaitEventFlag(KernelEventFlag ef, std::uint64_t bit_pattern, std::uint32_t wait_mode, std::uint64_t* result_pat, KernelUseconds* timeout);
int APS5_VABI APS5_Test_GetEventFlagWaiterCount(KernelEventFlag handle);
void APS5_VABI APS5_Test_SetEventFlagApiPause(int kind);
int APS5_VABI APS5_Test_GetEventFlagApiPauseCount();
void APS5_VABI APS5_Test_ReleaseEventFlagApiPause();
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EPERM = static_cast<int>(0x80020001);
static constexpr int SCE_KERNEL_ERROR_ENOENT = static_cast<int>(0x80020002);
static constexpr int SCE_KERNEL_ERROR_ESRCH = static_cast<int>(0x80020003);
static constexpr int SCE_KERNEL_ERROR_EBADF = static_cast<int>(0x80020009);
static constexpr int SCE_KERNEL_ERROR_EDEADLK = static_cast<int>(0x8002000B);
static constexpr int SCE_KERNEL_ERROR_EFAULT = static_cast<int>(0x8002000E);
static constexpr int SCE_KERNEL_ERROR_EACCES = static_cast<int>(0x8002000D);
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int SCE_KERNEL_ERROR_EBUSY = static_cast<int>(0x80020010);
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003C);
static constexpr int SCE_KERNEL_ERROR_ECANCELED = static_cast<int>(0x80020055);
static constexpr std::uint32_t EVF_ATTR_SINGLE = 0x10;
static constexpr std::uint32_t EVF_ATTR_MULTI = 0x20;
static constexpr std::uint32_t EVF_WAITMODE_AND = 0x01;
static constexpr std::uint32_t EVF_WAITMODE_OR = 0x02;
static constexpr std::uint32_t EVF_WAITMODE_CLEAR_ALL = 0x10;
static constexpr std::uint32_t EVF_WAITMODE_CLEAR_PAT = 0x20;
static constexpr int MUTEX_TYPE_ERRORCHECK = 1;
static constexpr int PRIO_NONE = 0;
static constexpr int PRIO_INHERIT = 1;
static constexpr int PRIO_PROTECT = 2;
static constexpr int MUTEX_TYPE_ADAPTIVE = 4;
static constexpr int POSIX_EDEADLK = 11;

static void Require(bool value) { if (!value) std::abort(); }

static void WaitForEventFlagWaiters(KernelEventFlag handle, int expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (APS5_Test_GetEventFlagWaiterCount(handle) != expected) {
        if (std::chrono::steady_clock::now() >= deadline) Require(false);
        std::this_thread::yield();
    }
}

static void WaitForEventFlagApiPause() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (APS5_Test_GetEventFlagApiPauseCount() != 1) {
        if (std::chrono::steady_clock::now() >= deadline) Require(false);
        std::this_thread::yield();
    }
}

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
    Require(sceKernelSignalSema(semaphore, 0) == SCE_OK);
    Require(sceKernelPollSema(semaphore, 3) == SCE_OK);
    Require(sceKernelSignalSema(semaphore, 0) == SCE_OK);
    Require(sceKernelPollSema(semaphore, 1) == SCE_KERNEL_ERROR_EBUSY);
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
    Require(scePthreadMutexattrSettype(&attr, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSettype(&attr, 5) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_NONE) == SCE_OK);
    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_INHERIT) == SCE_OK);
    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_PROTECT) == SCE_OK);
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, &attr, nullptr) == SCE_OK);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
    Require(scePthreadMutexLock(&mutex) == SCE_OK);
    Require(scePthreadMutexLock(&mutex) == SCE_KERNEL_ERROR_EDEADLK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_KERNEL_ERROR_EBUSY);
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

    PthreadMutex adaptiveStatic = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
    Require(scePthreadMutexLock(&adaptiveStatic) == SCE_OK);
    Require(adaptiveStatic != reinterpret_cast<PthreadMutex>(std::uintptr_t{1}));
    Require(scePthreadMutexLock(&adaptiveStatic) == SCE_KERNEL_ERROR_EDEADLK);
    Require(scePthreadMutexTrylock(&adaptiveStatic) == SCE_KERNEL_ERROR_EBUSY);
    Require(scePthreadMutexUnlock(&adaptiveStatic) == SCE_OK);
    Require(scePthreadMutexDestroy(&adaptiveStatic) == SCE_OK);
    adaptiveStatic = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
    Require(scePthreadMutexTrylock(&adaptiveStatic) == SCE_OK);
    Require(scePthreadMutexTrylock(&adaptiveStatic) == SCE_KERNEL_ERROR_EBUSY);
    Require(scePthreadMutexUnlock(&adaptiveStatic) == SCE_OK);
    Require(scePthreadMutexDestroy(&adaptiveStatic) == SCE_OK);
    adaptiveStatic = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
    Require(scePthreadMutexDestroy(&adaptiveStatic) == SCE_OK);

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

    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(nullptr, "test", EVF_ATTR_MULTI, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateEventFlag(&ef, nullptr, EVF_ATTR_MULTI, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateEventFlag(&ef, "test_zero_attr", 0, 0, nullptr) == SCE_OK);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);
    Require(sceKernelCreateEventFlag(&ef, "test_fifo", 0x01, 0, nullptr) == SCE_OK);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);
    Require(sceKernelCreateEventFlag(&ef, "test_single_fifo", EVF_ATTR_SINGLE | 0x01u, 0, nullptr) == SCE_OK);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);
    Require(sceKernelCreateEventFlag(&ef, "test", EVF_ATTR_SINGLE | EVF_ATTR_MULTI, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateEventFlag(&ef, "test", EVF_ATTR_MULTI | 0x1000u, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelDeleteEventFlag(nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelSetEventFlag(nullptr, 1) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelClearEventFlag(nullptr, 1) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelPollEventFlag(nullptr, 1, EVF_WAITMODE_OR, nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelWaitEventFlag(nullptr, 1, EVF_WAITMODE_OR, nullptr, nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelCancelEventFlag(nullptr, 0, nullptr) == SCE_KERNEL_ERROR_ESRCH);

    Require(sceKernelCreateEventFlag(&ef, "flag", EVF_ATTR_MULTI, 0x15, nullptr) == SCE_OK);
    std::uint64_t pattern = 0;
    Require(sceKernelPollEventFlag(ef, 0, EVF_WAITMODE_OR, &pattern) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollEventFlag(ef, 1, 0, &pattern) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollEventFlag(ef, 1, EVF_WAITMODE_AND | EVF_WAITMODE_OR, &pattern) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollEventFlag(ef, 1, EVF_WAITMODE_AND | EVF_WAITMODE_CLEAR_ALL | EVF_WAITMODE_CLEAR_PAT, &pattern) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelPollEventFlag(ef, 0x20, EVF_WAITMODE_OR, &pattern) == SCE_KERNEL_ERROR_EBUSY);
    Require(pattern == 0x15);
    Require(sceKernelPollEventFlag(ef, 0x11, EVF_WAITMODE_AND, &pattern) == SCE_OK);
    Require(pattern == 0x15);

    Require(sceKernelClearEventFlag(ef, ~std::uint64_t{0x01}) == SCE_OK);
    Require(sceKernelPollEventFlag(ef, 0x01, EVF_WAITMODE_OR, &pattern) == SCE_KERNEL_ERROR_EBUSY);
    Require(pattern == 0x14);

    Require(sceKernelSetEventFlag(ef, 0x02) == SCE_OK);
    Require(sceKernelPollEventFlag(ef, 0x06, EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_PAT, &pattern) == SCE_OK);
    Require(pattern == 0x16);
    Require(sceKernelPollEventFlag(ef, 0x02, EVF_WAITMODE_OR, &pattern) == SCE_KERNEL_ERROR_EBUSY);
    Require(pattern == 0x10);

    Require(sceKernelPollEventFlag(ef, 0x10, EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_ALL, &pattern) == SCE_OK);
    Require(pattern == 0x10);
    Require(sceKernelPollEventFlag(ef, 0x10, EVF_WAITMODE_OR, &pattern) == SCE_KERNEL_ERROR_EBUSY);
    Require(pattern == 0);

    KernelUseconds efTimeout = 1000;
    Require(sceKernelWaitEventFlag(ef, 0x01, EVF_WAITMODE_OR, &pattern, &efTimeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(pattern == 0);

    int waitThreads = -1;
    Require(sceKernelCancelEventFlag(ef, 0x42, &waitThreads) == SCE_OK);
    Require(waitThreads == 0);
    Require(sceKernelPollEventFlag(ef, 0x42, EVF_WAITMODE_AND, &pattern) == SCE_OK);
    Require(pattern == 0x42);

    Require(sceKernelClearEventFlag(ef, 0) == SCE_OK);
    Require(sceKernelPollEventFlag(ef, 0x42, EVF_WAITMODE_AND, &pattern) == SCE_KERNEL_ERROR_EBUSY);
    Require(pattern == 0);

    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);

    Require(sceKernelCreateEventFlag(&ef, "single", EVF_ATTR_SINGLE | 0x02u, 0, nullptr) == SCE_OK);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);

    Require(sceKernelCreateEventFlag(&ef, "imm_sat", EVF_ATTR_MULTI, 0x05, nullptr) == SCE_OK);
    pattern = 0;
    Require(sceKernelWaitEventFlag(ef, 0x01, EVF_WAITMODE_AND | EVF_WAITMODE_CLEAR_PAT, &pattern, nullptr) == SCE_OK);
    Require(pattern == 0x05);
    Require(sceKernelPollEventFlag(ef, 0x01, EVF_WAITMODE_AND, &pattern) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelPollEventFlag(ef, 0x04, EVF_WAITMODE_AND, &pattern) == SCE_OK);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);

    Require(sceKernelCreateEventFlag(&ef, "multi_race", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
    std::atomic<int> t1Res{-1};
    std::atomic<int> t2Res{-1};
    std::uint64_t pat1 = 0;
    std::uint64_t pat2 = 0;
    std::thread t1([&] {
        t1Res = sceKernelWaitEventFlag(ef, 0x01, EVF_WAITMODE_AND, &pat1, nullptr);
    });
    std::thread t2([&] {
        t2Res = sceKernelWaitEventFlag(ef, 0x01, EVF_WAITMODE_AND | EVF_WAITMODE_CLEAR_ALL, &pat2, nullptr);
    });
    WaitForEventFlagWaiters(ef, 2);
    Require(sceKernelSetEventFlag(ef, 0x01) == SCE_OK);
    t1.join();
    t2.join();
    Require(t1Res == SCE_OK);
    Require(t2Res == SCE_OK);
    Require(pat1 == 0x01);
    Require(pat2 == 0x01);
    Require(sceKernelPollEventFlag(ef, 0x01, EVF_WAITMODE_AND, &pattern) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);

    Require(sceKernelCreateEventFlag(&ef, "del_waiters", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
    std::atomic<int> delRes1{-1};
    std::atomic<int> delRes2{-1};
    std::thread delT1([&] {
        delRes1 = sceKernelWaitEventFlag(ef, 0x01, EVF_WAITMODE_AND, nullptr, nullptr);
    });
    std::thread delT2([&] {
        delRes2 = sceKernelWaitEventFlag(ef, 0x02, EVF_WAITMODE_AND, nullptr, nullptr);
    });
    WaitForEventFlagWaiters(ef, 2);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);
    delT1.join();
    delT2.join();
    Require(delRes1 == SCE_KERNEL_ERROR_EACCES);
    Require(delRes2 == SCE_KERNEL_ERROR_EACCES);

    for (int apiKind = 1; apiKind <= 2; ++apiKind) {
        Require(sceKernelCreateEventFlag(&ef, "api_delete_race", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
        APS5_Test_SetEventFlagApiPause(apiKind);
        std::atomic<int> apiResult{-1};
        std::thread apiCall([&] {
            if (apiKind == 1) {
                apiResult = sceKernelSetEventFlag(ef, 0x01);
            } else {
                std::uint64_t observed = 0;
                apiResult = sceKernelPollEventFlag(ef, 0x01, EVF_WAITMODE_AND, &observed);
            }
        });
        WaitForEventFlagApiPause();
        Require(sceKernelDeleteEventFlag(ef) == SCE_OK);
        APS5_Test_ReleaseEventFlagApiPause();
        apiCall.join();
        Require(apiResult == SCE_KERNEL_ERROR_ESRCH);
    }
}
