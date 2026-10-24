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
int APS5_VABI sceKernelOpenEventFlag(KernelEventFlag* ef, const char* name);
int APS5_VABI sceKernelCloseEventFlag(KernelEventFlag ef);
int APS5_VABI sceKernelSetEventFlag(KernelEventFlag ef, std::uint64_t bit_pattern);
int APS5_VABI sceKernelClearEventFlag(KernelEventFlag ef, std::uint64_t bit_pattern);
int APS5_VABI sceKernelCancelEventFlag(KernelEventFlag ef, std::uint64_t set_pattern, int* num_wait_threads);
int APS5_VABI sceKernelPollEventFlag(KernelEventFlag ef, std::uint64_t bit_pattern, std::uint32_t wait_mode, std::uint64_t* result_pat);
int APS5_VABI sceKernelWaitEventFlag(KernelEventFlag ef, std::uint64_t bit_pattern, std::uint32_t wait_mode, std::uint64_t* result_pat, KernelUseconds* timeout);
int APS5_Test_GetEventFlagWaiterCount(KernelEventFlag handle);
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
static constexpr int SCE_KERNEL_ERROR_ENAMETOOLONG = static_cast<int>(0x8002003F);
static constexpr int SCE_KERNEL_ERROR_ECANCELED = static_cast<int>(0x80020055);
static constexpr std::uint32_t EVF_ATTR_SINGLE = 0x10;
static constexpr std::uint32_t EVF_ATTR_MULTI = 0x20;
static constexpr std::uint32_t EVF_ATTR_SHARED = 0x100;
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
    for (const std::uint32_t attr : {0x03u, 0x13u, 0x23u, 0x103u, 0x130u, 0x04u, 0x08u, 0x40u, 0x80u, 0x200u, 0x80000000u, 0xffffffffu}) {
        Require(sceKernelCreateEventFlag(&ef, "test_bad_attr", attr, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    }
    for (const std::uint32_t attr : {0x02u, 0x11u, 0x12u, 0x21u, 0x22u}) {
        Require(sceKernelCreateEventFlag(&ef, "test_attr", attr, 0, nullptr) == SCE_OK);
        Require(sceKernelDeleteEventFlag(ef) == SCE_OK);
    }
    Require(sceKernelDeleteEventFlag(nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelSetEventFlag(nullptr, 1) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelClearEventFlag(nullptr, 1) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelPollEventFlag(nullptr, 1, EVF_WAITMODE_OR, nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelWaitEventFlag(nullptr, 1, EVF_WAITMODE_OR, nullptr, nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelCancelEventFlag(nullptr, 0, nullptr) == SCE_KERNEL_ERROR_ESRCH);

    Require(sceKernelCreateEventFlag(&ef, "", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);
    Require(sceKernelCreateEventFlag(&ef, "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn", EVF_ATTR_SHARED | EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);
    for (const std::uint32_t attr : {EVF_ATTR_MULTI, EVF_ATTR_SHARED | EVF_ATTR_MULTI}) {
        Require(sceKernelCreateEventFlag(&ef, "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn", attr, 0, nullptr) == SCE_KERNEL_ERROR_ENAMETOOLONG);
    }

    KernelEventFlag opened = nullptr;
    Require(sceKernelOpenEventFlag(nullptr, "test") == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelOpenEventFlag(&opened, nullptr) == SCE_KERNEL_ERROR_EFAULT && opened == nullptr);
    Require(sceKernelOpenEventFlag(&opened, "") == SCE_KERNEL_ERROR_ESRCH && opened == nullptr);
    Require(sceKernelOpenEventFlag(&opened, "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn") == SCE_KERNEL_ERROR_ENAMETOOLONG && opened == nullptr);
    Require(sceKernelOpenEventFlag(&opened, "SceShellCoreUtilAppFocus") == SCE_KERNEL_ERROR_ESRCH && opened == nullptr);
    Require(sceKernelCloseEventFlag(nullptr) == SCE_KERNEL_ERROR_ESRCH);

    KernelEventFlag plain = nullptr;
    Require(sceKernelCreateEventFlag(&plain, "test_open", EVF_ATTR_MULTI, 0x01, nullptr) == SCE_OK);
    Require(sceKernelOpenEventFlag(&opened, "test_open") == SCE_KERNEL_ERROR_ESRCH && opened == nullptr);
    Require(sceKernelCloseEventFlag(plain) == SCE_KERNEL_ERROR_EPERM);
    Require(sceKernelSetEventFlag(plain, 0x02) == SCE_OK);
    KernelEventFlag plainTwin = nullptr;
    Require(sceKernelCreateEventFlag(&plainTwin, "test_open", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
    Require(sceKernelDeleteEventFlag(plainTwin) == SCE_OK);

    Require(sceKernelCreateEventFlag(&ef, "test_open", EVF_ATTR_SHARED | EVF_ATTR_MULTI, 0x05, nullptr) == SCE_OK);
    KernelEventFlag sharedTwin = nullptr;
    Require(sceKernelCreateEventFlag(&sharedTwin, "test_open", EVF_ATTR_SHARED | EVF_ATTR_MULTI, 0, nullptr) == SCE_KERNEL_ERROR_EPERM && sharedTwin == nullptr);
    Require(sceKernelOpenEventFlag(&opened, "test_open") == SCE_OK && opened != nullptr && opened != ef && opened != plain);
    Require(sceKernelOpenEventFlag(nullptr, "test_open") == SCE_KERNEL_ERROR_EFAULT);
    KernelEventFlag openedTwice = nullptr;
    Require(sceKernelOpenEventFlag(&openedTwice, "test_open") == SCE_OK && openedTwice != opened && openedTwice != ef);
    Require(sceKernelSetEventFlag(opened, 0x08) == SCE_OK);
    Require(sceKernelClearEventFlag(openedTwice, ~std::uint64_t{0x04}) == SCE_OK);
    std::uint64_t openedPattern = 0;
    Require(sceKernelPollEventFlag(ef, 0x08, EVF_WAITMODE_OR, &openedPattern) == SCE_OK && openedPattern == 0x09);
    Require(sceKernelDeleteEventFlag(opened) == SCE_KERNEL_ERROR_EPERM);
    Require(sceKernelCloseEventFlag(ef) == SCE_KERNEL_ERROR_EPERM);
    Require(sceKernelCloseEventFlag(openedTwice) == SCE_OK);
    Require(sceKernelCloseEventFlag(openedTwice) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelSetEventFlag(openedTwice, 0x01) == SCE_KERNEL_ERROR_ESRCH);

    std::atomic<int> orphanWaitRes{-1};
    std::uint64_t orphanWaitPat = 0xdeadbeef;
    std::thread orphanWaiter([&] {
        orphanWaitRes = sceKernelWaitEventFlag(opened, 0x80, EVF_WAITMODE_OR, &orphanWaitPat, nullptr);
    });
    WaitForEventFlagWaiters(opened, 1);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);
    orphanWaiter.join();
    Require(orphanWaitRes == SCE_KERNEL_ERROR_EACCES && orphanWaitPat == 0);
    KernelEventFlag reopened = nullptr;
    Require(sceKernelOpenEventFlag(&reopened, "test_open") == SCE_KERNEL_ERROR_ESRCH && reopened == nullptr);
    std::uint64_t orphanPattern = 0xdeadbeef;
    Require(sceKernelPollEventFlag(opened, 0, EVF_WAITMODE_OR, &orphanPattern) == SCE_KERNEL_ERROR_EINVAL && orphanPattern == 0xdeadbeef);
    Require(sceKernelPollEventFlag(opened, 0x01, EVF_WAITMODE_OR, &orphanPattern) == SCE_KERNEL_ERROR_EACCES && orphanPattern == 0);
    orphanPattern = 0xdeadbeef;
    KernelUseconds orphanTimeout = 1000;
    Require(sceKernelWaitEventFlag(opened, 0x01, EVF_WAITMODE_OR, &orphanPattern, &orphanTimeout) == SCE_KERNEL_ERROR_EACCES && orphanPattern == 0 && orphanTimeout == 1000);
    Require(sceKernelSetEventFlag(opened, 0x02) == SCE_OK);
    Require(sceKernelClearEventFlag(opened, 0) == SCE_OK);
    int orphanCount = -1;
    Require(sceKernelCancelEventFlag(opened, 0x04, &orphanCount) == SCE_OK && orphanCount == 0);
    Require(sceKernelDeleteEventFlag(opened) == SCE_KERNEL_ERROR_EPERM);

    Require(sceKernelCreateEventFlag(&ef, "test_open", EVF_ATTR_SHARED | EVF_ATTR_SINGLE, 0x01, nullptr) == SCE_OK);
    Require(sceKernelOpenEventFlag(&reopened, "test_open") == SCE_OK && reopened != opened);
    Require(sceKernelCloseEventFlag(opened) == SCE_OK);
    Require(sceKernelCloseEventFlag(opened) == SCE_KERNEL_ERROR_ESRCH);
    std::atomic<int> sharedSingleRes{-1};
    std::uint64_t sharedSinglePat = 0;
    std::thread sharedSingleWaiter([&] {
        sharedSingleRes = sceKernelWaitEventFlag(ef, 0x80, EVF_WAITMODE_OR, &sharedSinglePat, nullptr);
    });
    WaitForEventFlagWaiters(ef, 1);
    openedPattern = 0xdeadbeef;
    Require(sceKernelPollEventFlag(reopened, 0x01, EVF_WAITMODE_OR, &openedPattern) == SCE_KERNEL_ERROR_EPERM && openedPattern == 0);
    Require(sceKernelSetEventFlag(reopened, 0x80) == SCE_OK);
    sharedSingleWaiter.join();
    Require(sharedSingleRes == SCE_OK && sharedSinglePat == 0x81);
    Require(sceKernelCloseEventFlag(reopened) == SCE_OK);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);
    Require(sceKernelDeleteEventFlag(plain) == SCE_OK);

    Require(sceKernelCreateEventFlag(&ef, "flag", EVF_ATTR_MULTI, 0x15, nullptr) == SCE_OK);
    std::uint64_t pattern = 0;
    Require(sceKernelPollEventFlag(ef, 0, EVF_WAITMODE_OR, &pattern) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollEventFlag(ef, 1, 0, &pattern) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollEventFlag(ef, 1, EVF_WAITMODE_AND | EVF_WAITMODE_OR, &pattern) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollEventFlag(ef, 1, EVF_WAITMODE_AND | EVF_WAITMODE_CLEAR_ALL | EVF_WAITMODE_CLEAR_PAT, &pattern) == SCE_KERNEL_ERROR_EINVAL);
    for (const std::uint32_t mode : {0x04u, 0x40u, 0x100u, 0x80000000u, 0x103u}) {
        pattern = 0xdeadbeef;
        Require(sceKernelPollEventFlag(ef, 1, mode, &pattern) == SCE_KERNEL_ERROR_EINVAL && pattern == 0xdeadbeef);
    }
    for (const std::uint32_t mode : {EVF_WAITMODE_OR | 0x04u, EVF_WAITMODE_OR | 0x08u, EVF_WAITMODE_AND | 0x40u, EVF_WAITMODE_AND | 0x100u, EVF_WAITMODE_OR | 0x80000000u,
                                     EVF_WAITMODE_OR | 0xffffff00u}) {
        Require(sceKernelPollEventFlag(ef, 0x10, mode, &pattern) == SCE_OK && pattern == 0x15);
    }
    Require(sceKernelPollEventFlag(ef, 0x10, EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_PAT | 0x100u, &pattern) == SCE_OK && pattern == 0x15);
    Require(sceKernelSetEventFlag(ef, 0x10) == SCE_OK);

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

    Require(sceKernelCreateEventFlag(&ef, "single", EVF_ATTR_SINGLE | 0x02u, 0x04, nullptr) == SCE_OK);
    std::atomic<int> singleRes{-1};
    std::uint64_t singlePat = 0xdeadbeef;
    std::thread singleWaiter([&] {
        singleRes = sceKernelWaitEventFlag(ef, 0x01, EVF_WAITMODE_AND, &singlePat, nullptr);
    });
    WaitForEventFlagWaiters(ef, 1);
    pattern = 0xdeadbeef;
    Require(sceKernelPollEventFlag(ef, 0x04, EVF_WAITMODE_AND, &pattern) == SCE_KERNEL_ERROR_EPERM && pattern == 0);
    pattern = 0xdeadbeef;
    Require(sceKernelWaitEventFlag(ef, 0x04, EVF_WAITMODE_AND, &pattern, nullptr) == SCE_KERNEL_ERROR_EPERM && pattern == 0);
    waitThreads = -1;
    Require(sceKernelCancelEventFlag(ef, 0x08, &waitThreads) == SCE_OK && waitThreads == 1);
    singleWaiter.join();
    Require(singleRes == SCE_KERNEL_ERROR_ECANCELED && singlePat == 0);
    Require(sceKernelPollEventFlag(ef, 0x08, EVF_WAITMODE_AND, &pattern) == SCE_OK && pattern == 0x08);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);

    Require(sceKernelCreateEventFlag(&ef, "multi_cancel", EVF_ATTR_MULTI, 0x01, nullptr) == SCE_OK);
    std::atomic<int> cancelRes1{-1};
    std::atomic<int> cancelRes2{-1};
    std::uint64_t cancelPat1 = 0xdeadbeef;
    std::uint64_t cancelPat2 = 0xdeadbeef;
    std::thread cancelT1([&] {
        cancelRes1 = sceKernelWaitEventFlag(ef, 0x02, EVF_WAITMODE_AND, &cancelPat1, nullptr);
    });
    std::thread cancelT2([&] {
        cancelRes2 = sceKernelWaitEventFlag(ef, 0x04, EVF_WAITMODE_OR, &cancelPat2, nullptr);
    });
    WaitForEventFlagWaiters(ef, 2);
    waitThreads = -1;
    Require(sceKernelCancelEventFlag(ef, 0x05, &waitThreads) == SCE_OK && waitThreads == 2);
    cancelT1.join();
    cancelT2.join();
    Require(cancelRes1 == SCE_KERNEL_ERROR_ECANCELED && cancelPat1 == 0);
    Require(cancelRes2 == SCE_KERNEL_ERROR_ECANCELED && cancelPat2 == 0);
    Require(sceKernelPollEventFlag(ef, 0x05, EVF_WAITMODE_AND, &pattern) == SCE_OK && pattern == 0x05);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);

    Require(sceKernelCreateEventFlag(&ef, "imm_sat", EVF_ATTR_MULTI, 0x05, nullptr) == SCE_OK);
    pattern = 0;
    Require(sceKernelWaitEventFlag(ef, 0x01, EVF_WAITMODE_AND | EVF_WAITMODE_CLEAR_PAT, &pattern, nullptr) == SCE_OK);
    Require(pattern == 0x05);
    Require(sceKernelPollEventFlag(ef, 0x01, EVF_WAITMODE_AND, &pattern) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelPollEventFlag(ef, 0x04, EVF_WAITMODE_AND, &pattern) == SCE_OK);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);

    Require(sceKernelCreateEventFlag(&ef, "multi_order_nonclear_first", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
    std::atomic<int> a1Res{-1};
    std::atomic<int> a2Res{-1};
    std::uint64_t a1Pat = 0;
    std::uint64_t a2Pat = 0;
    std::thread a1([&] {
        a1Res = sceKernelWaitEventFlag(ef, 0x01, EVF_WAITMODE_AND, &a1Pat, nullptr);
    });
    WaitForEventFlagWaiters(ef, 1);
    std::thread a2([&] {
        a2Res = sceKernelWaitEventFlag(ef, 0x01, EVF_WAITMODE_AND | EVF_WAITMODE_CLEAR_PAT, &a2Pat, nullptr);
    });
    WaitForEventFlagWaiters(ef, 2);
    Require(sceKernelSetEventFlag(ef, 0x01) == SCE_OK);
    a1.join();
    a2.join();
    Require(a1Res == SCE_OK);
    Require(a2Res == SCE_OK);
    Require(a1Pat == 0x01);
    Require(a2Pat == 0x01);
    Require(sceKernelPollEventFlag(ef, 0x01, EVF_WAITMODE_AND, &pattern) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);

    Require(sceKernelCreateEventFlag(&ef, "multi_order_clear_first", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
    std::atomic<int> b1Res{-1};
    std::atomic<int> b2Res{-1};
    std::uint64_t b1Pat = 0;
    std::uint64_t b2Pat = 0;
    std::thread b1([&] {
        b1Res = sceKernelWaitEventFlag(ef, 0x01, EVF_WAITMODE_AND | EVF_WAITMODE_CLEAR_PAT, &b1Pat, nullptr);
    });
    WaitForEventFlagWaiters(ef, 1);
    std::thread b2([&] {
        b2Res = sceKernelWaitEventFlag(ef, 0x01, EVF_WAITMODE_AND, &b2Pat, nullptr);
    });
    WaitForEventFlagWaiters(ef, 2);
    Require(sceKernelSetEventFlag(ef, 0x01) == SCE_OK);
    b1.join();
    Require(b1Res == SCE_OK);
    Require(b1Pat == 0x01);
    Require(APS5_Test_GetEventFlagWaiterCount(ef) == 1);
    Require(b2Res == -1);
    Require(sceKernelSetEventFlag(ef, 0x01) == SCE_OK);
    b2.join();
    Require(b2Res == SCE_OK);
    Require(b2Pat == 0x01);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);

    Require(sceKernelCreateEventFlag(&ef, "del_waiters", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
    std::atomic<int> delRes1{-1};
    std::atomic<int> delRes2{-1};
    std::uint64_t delPat = 0xdeadbeef;
    std::thread delT1([&] {
        delRes1 = sceKernelWaitEventFlag(ef, 0x01, EVF_WAITMODE_AND, &delPat, nullptr);
    });
    std::thread delT2([&] {
        delRes2 = sceKernelWaitEventFlag(ef, 0x02, EVF_WAITMODE_AND, nullptr, nullptr);
    });
    WaitForEventFlagWaiters(ef, 2);
    Require(sceKernelDeleteEventFlag(ef) == SCE_OK);
    delT1.join();
    delT2.join();
    Require(delRes1 == SCE_KERNEL_ERROR_EACCES && delPat == 0);
    Require(delRes2 == SCE_KERNEL_ERROR_EACCES);

    for (int iter = 0; iter < 50; ++iter) {
        KernelEventFlag raceEf = nullptr;
        Require(sceKernelCreateEventFlag(&raceEf, "set_del_race", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
        std::atomic<int> setResult{-1};
        std::thread concurrentSet([&] {
            setResult = sceKernelSetEventFlag(raceEf, 0x01);
        });
        Require(sceKernelDeleteEventFlag(raceEf) == SCE_OK);
        concurrentSet.join();
        Require(setResult == SCE_OK || setResult == SCE_KERNEL_ERROR_ESRCH);
    }

    for (int iter = 0; iter < 50; ++iter) {
        KernelEventFlag pollEf = nullptr;
        Require(sceKernelCreateEventFlag(&pollEf, "poll_del_race", EVF_ATTR_MULTI, 0x01, nullptr) == SCE_OK);
        std::atomic<int> pollResult{-1};
        std::thread concurrentPoll([&] {
            std::uint64_t observed = 0;
            pollResult = sceKernelPollEventFlag(pollEf, 0x01, EVF_WAITMODE_AND, &observed);
        });
        Require(sceKernelDeleteEventFlag(pollEf) == SCE_OK);
        concurrentPoll.join();
        Require(pollResult == SCE_OK || pollResult == SCE_KERNEL_ERROR_ESRCH);
    }
}
