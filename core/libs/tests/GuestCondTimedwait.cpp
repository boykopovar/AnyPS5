#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>

static int cleanupAt = -1;
static void APS5_VABI Cleanup(void*) { cleanupAt = 0; }

static constexpr int SCE_OK = 0;
static constexpr int POSIX_EPERM = 1;
static constexpr int POSIX_EINVAL = 22;
static constexpr int POSIX_ETIMEDOUT = 60;
static constexpr int MUTEX_TYPE_RECURSIVE = 2;
static constexpr KernelClockid CLOCK_MONOTONIC_ID = 4;

static void Require(bool value) { if (!value) std::abort(); }

struct Context {
    PthreadMutex mutex = nullptr;
    std::atomic<bool> acquired{false};
};

static void* APS5_VABI Contender(void* arg) {
    auto& context = *static_cast<Context*>(arg);
    Require(scePthreadMutexLock(&context.mutex) == SCE_OK);
    context.acquired.store(true);
    Require(scePthreadMutexUnlock(&context.mutex) == SCE_OK);
    return nullptr;
}

struct Signalled {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    bool ready = false;
    int trylock = SCE_OK;
    Pthread target = nullptr;
};

static void* APS5_VABI Trylocker(void* arg) {
    auto& signalled = *static_cast<Signalled*>(arg);
    signalled.trylock = scePthreadMutexTrylock(&signalled.mutex);
    return nullptr;
}

static void* APS5_VABI Signaller(void* arg) {
    auto& signalled = *static_cast<Signalled*>(arg);
    Require(scePthreadMutexLock(&signalled.mutex) == SCE_OK);
    signalled.ready = true;
    Require(scePthreadCondSignal(&signalled.cond) == SCE_OK);
    Require(scePthreadMutexUnlock(&signalled.mutex) == SCE_OK);
    return nullptr;
}

static void* APS5_VABI SignallerTo(void* arg) {
    auto& signalled = *static_cast<Signalled*>(arg);
    Require(scePthreadMutexLock(&signalled.mutex) == SCE_OK);
    signalled.ready = true;
    Require(pthread_cond_signalto_np_nid_postfix(&signalled.cond, signalled.target) == 0);
    Require(scePthreadMutexUnlock(&signalled.mutex) == SCE_OK);
    return nullptr;
}

static void RequireHeldByCaller(Signalled& signalled) {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Trylocker, &signalled, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(signalled.trylock == SCE_KERNEL_ERROR_EBUSY);
}

static void RelativeWait() {
    Signalled signalled;
    Require(scePthreadMutexInit(&signalled.mutex, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondInit(&signalled.cond, nullptr, nullptr) == SCE_OK);

    Require(pthread_cond_reltimedwait_np_nid_postfix(&signalled.cond, &signalled.mutex, 1000) == POSIX_EPERM);

    Require(scePthreadMutexLock(&signalled.mutex) == SCE_OK);
    const auto start = std::chrono::steady_clock::now();
    Require(pthread_cond_reltimedwait_np_nid_postfix(&signalled.cond, &signalled.mutex, 50000) == POSIX_ETIMEDOUT);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
    RequireHeldByCaller(signalled);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&signalled.cond, &signalled.mutex, 0) == POSIX_ETIMEDOUT);
    RequireHeldByCaller(signalled);

    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Signaller, &signalled, nullptr) == SCE_OK);
    const auto waitStart = std::chrono::steady_clock::now();
    while (!signalled.ready)
        Require(pthread_cond_reltimedwait_np_nid_postfix(&signalled.cond, &signalled.mutex, 10000000) == 0);
    Require(std::chrono::steady_clock::now() - waitStart < std::chrono::seconds(5));
    RequireHeldByCaller(signalled);
    Require(scePthreadMutexUnlock(&signalled.mutex) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);

    Require(scePthreadCondDestroy(&signalled.cond) == SCE_OK);
    Require(scePthreadMutexDestroy(&signalled.mutex) == SCE_OK);
}

static KernelTimespec MonotonicAfter(std::int64_t nanoseconds) {
    KernelTimespec now{};
    Require(sceKernelClockGettime(CLOCK_MONOTONIC_ID, &now) == SCE_OK);
    const auto total = now.tv_nsec + nanoseconds;
    return KernelTimespec{now.tv_sec + total / 1000000000, total % 1000000000};
}

static void MonotonicClockWait() {
    Signalled signalled;
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == 0);
    Require(pthread_condattr_setclock_nid_postfix(&attr, CLOCK_MONOTONIC_ID) == 0);
    Require(scePthreadMutexInit(&signalled.mutex, nullptr, nullptr) == SCE_OK);
    Require(pthread_cond_init_nid_postfix(&signalled.cond, &attr) == 0);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0);

    Require(scePthreadMutexLock(&signalled.mutex) == SCE_OK);
    const auto start = std::chrono::steady_clock::now();
    const auto soon = MonotonicAfter(50000000);
    Require(pthread_cond_timedwait_nid_postfix(&signalled.cond, &signalled.mutex, &soon) == POSIX_ETIMEDOUT);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
    RequireHeldByCaller(signalled);

    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Signaller, &signalled, nullptr) == SCE_OK);
    const auto later = MonotonicAfter(10000000000);
    const auto waitStart = std::chrono::steady_clock::now();
    while (!signalled.ready)
        Require(pthread_cond_timedwait_nid_postfix(&signalled.cond, &signalled.mutex, &later) == 0);
    Require(std::chrono::steady_clock::now() - waitStart < std::chrono::seconds(5));
    RequireHeldByCaller(signalled);
    Require(scePthreadMutexUnlock(&signalled.mutex) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);

    Require(pthread_cond_destroy_nid_postfix(&signalled.cond) == 0);
    Require(scePthreadMutexDestroy(&signalled.mutex) == SCE_OK);
}

static void SignalTo() {
    Signalled signalled;
    signalled.target = scePthreadSelf();
    Require(scePthreadMutexInit(&signalled.mutex, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondInit(&signalled.cond, nullptr, nullptr) == SCE_OK);
    Require(pthread_cond_signalto_np_nid_postfix(&signalled.cond, signalled.target) == 1);
    Require(scePthreadCondSignalto(&signalled.cond, signalled.target) == static_cast<int>(0x80020001u));
    Require(pthread_cond_signalto_np_nid_postfix(&signalled.cond, nullptr) == 22);
    Require(scePthreadCondSignalto(&signalled.cond, nullptr) == static_cast<int>(0x80020016u));
    Require(scePthreadMutexLock(&signalled.mutex) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, SignallerTo, &signalled, nullptr) == SCE_OK);
    const auto waitStart = std::chrono::steady_clock::now();
    while (!signalled.ready)
        Require(pthread_cond_reltimedwait_np_nid_postfix(&signalled.cond, &signalled.mutex, 10000000) == 0);
    Require(std::chrono::steady_clock::now() - waitStart < std::chrono::seconds(5));
    Require(scePthreadMutexUnlock(&signalled.mutex) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(scePthreadCondDestroy(&signalled.cond) == SCE_OK);
    Require(pthread_cond_signalto_np_nid_postfix(&signalled.cond, signalled.target) == 22);
    Require(scePthreadMutexDestroy(&signalled.mutex) == SCE_OK);
}

static void SignalToEveryRound(bool recursive) {
    for (int round = 0; round < 200; ++round) {
        Signalled signalled;
        signalled.target = scePthreadSelf();
        PthreadMutexattr attr = nullptr;
        Require(scePthreadMutexattrInit(&attr) == SCE_OK);
        if (recursive)
            Require(scePthreadMutexattrSettype(&attr, MUTEX_TYPE_RECURSIVE) == SCE_OK);
        Require(scePthreadMutexInit(&signalled.mutex, &attr, nullptr) == SCE_OK);
        Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
        Require(scePthreadCondInit(&signalled.cond, nullptr, nullptr) == SCE_OK);
        Require(scePthreadMutexLock(&signalled.mutex) == SCE_OK);
        if (recursive)
            Require(scePthreadMutexLock(&signalled.mutex) == SCE_OK);
        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, nullptr, SignallerTo, &signalled, nullptr) == SCE_OK);
        while (!signalled.ready)
            Require(pthread_cond_reltimedwait_np_nid_postfix(&signalled.cond, &signalled.mutex, 2000000) == 0);
        if (recursive)
            Require(scePthreadMutexUnlock(&signalled.mutex) == SCE_OK);
        Require(scePthreadMutexUnlock(&signalled.mutex) == SCE_OK);
        Require(scePthreadJoin(thread, nullptr) == SCE_OK);
        Require(scePthreadCondDestroy(&signalled.cond) == SCE_OK);
        Require(scePthreadMutexDestroy(&signalled.mutex) == SCE_OK);
    }
}

int main() {
    RelativeWait();
    MonotonicClockWait();
    SignalTo();
    SignalToEveryRound(false);
    SignalToEveryRound(true);

    Context context;
    PthreadCond cond = nullptr;
    Require(scePthreadMutexInit(&context.mutex, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == SCE_OK);

    Require(scePthreadMutexLock(&context.mutex) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Contender, &context, nullptr) == SCE_OK);
    while (!context.acquired.load())
        Require(scePthreadCondTimedwait(&cond, &context.mutex, 1000) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(scePthreadMutexUnlock(&context.mutex) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);

    Require(scePthreadCondDestroy(&cond) == SCE_OK);

    PthreadCondattr unset = nullptr;
    Require(scePthreadCondInit(&cond, &unset, nullptr) == SCE_OK);
    Require(scePthreadMutexLock(&context.mutex) == SCE_OK);
    Require(scePthreadCondTimedwait(&cond, &context.mutex, 1000) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(scePthreadMutexUnlock(&context.mutex) == SCE_OK);
    Require(scePthreadCondDestroy(&cond) == SCE_OK);
    Require(pthread_cond_init_nid_postfix(&cond, &unset) == 0);
    {
        const KernelTimespec negativeSeconds{-1, 0};
        const KernelTimespec negativeNanoseconds{1, -1};
        Require(scePthreadMutexLock(&context.mutex) == SCE_OK);
        Require(pthread_cond_timedwait_nid_postfix(&cond, &context.mutex, &negativeSeconds) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(&cond, &context.mutex, &negativeNanoseconds) == POSIX_EINVAL);
        Require(scePthreadMutexUnlock(&context.mutex) == SCE_OK);
    }
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(scePthreadMutexDestroy(&context.mutex) == SCE_OK);

    Require(sceKernelSleep(0) == SCE_OK);
    void* finalizeHandle = reinterpret_cast<void*>(7);
    Require(__cxa_atexit_nid_postfix(Cleanup, nullptr, finalizeHandle) == SCE_OK);
    __pthread_cxa_finalize_nid_postfix(finalizeHandle);

    Require(cleanupAt == 0);
}
