#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/Posix/Common.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <thread>

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

static KernelTimespec RealtimeAfter(std::int64_t seconds) {
    KernelTimespec now{};
    Require(sceKernelClockGettime(0, &now) == SCE_OK);
    return KernelTimespec{now.tv_sec + seconds, now.tv_nsec};
}

static void DistantAbsoluteDeadline() {
    constexpr std::uint64_t nanosPerSecond = 1000000000ULL;
    const auto remaining = [](const KernelTimespec& abstime) {
        std::uint64_t deadline = 0;
        Require(PosixThread::AbsoluteDeadlineNanos(0, &abstime, &deadline));
        const auto now = TimedWait::NowNanos();
        return deadline > now ? deadline - now : 0;
    };
    const auto twoHours = remaining(RealtimeAfter(7200));
    Require(twoHours > 7199 * nanosPerSecond && twoHours <= 7200 * nanosPerSecond);
    const auto oneDay = remaining(RealtimeAfter(86400));
    Require(oneDay > 86399 * nanosPerSecond && oneDay <= 86400 * nanosPerSecond);
    const auto forever = remaining(KernelTimespec{std::numeric_limits<std::int64_t>::max(), 999999999});
    Require(forever > 86400 * 30 * nanosPerSecond);
    Require(remaining(RealtimeAfter(-5)) == 0);

    Signalled signalled;
    Require(scePthreadMutexInit(&signalled.mutex, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondInit(&signalled.cond, nullptr, nullptr) == SCE_OK);
    Require(scePthreadMutexLock(&signalled.mutex) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Signaller, &signalled, nullptr) == SCE_OK);
    const auto distant = RealtimeAfter(7200);
    const auto waitStart = std::chrono::steady_clock::now();
    while (!signalled.ready)
        Require(pthread_cond_timedwait_nid_postfix(&signalled.cond, &signalled.mutex, &distant) == 0);
    Require(std::chrono::steady_clock::now() - waitStart < std::chrono::seconds(5));
    Require(scePthreadMutexUnlock(&signalled.mutex) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(scePthreadCondDestroy(&signalled.cond) == SCE_OK);
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
    const auto unknown = reinterpret_cast<Pthread>(static_cast<std::uintptr_t>(0x12345));
    Require(pthread_cond_signalto_np_nid_postfix(&signalled.cond, unknown) == 3);
    Require(scePthreadCondSignalto(&signalled.cond, unknown) == static_cast<int>(0x80020003u));
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


struct TargetedWait {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    std::atomic<int> ready{0};
    std::atomic<int> targetResult{-1};
    std::atomic<int> otherResult{-1};
};

struct TargetedWaitArgument {
    TargetedWait* state;
    bool target;
};

static void* APS5_VABI TargetedWaiter(void* arg) {
    const auto argument = *static_cast<TargetedWaitArgument*>(arg);
    auto& state = *argument.state;
    Require(scePthreadMutexLock(&state.mutex) == SCE_OK);
    state.ready.fetch_add(1);
    const int result = pthread_cond_reltimedwait_np_nid_postfix(&state.cond, &state.mutex, 100000);
    (argument.target ? state.targetResult : state.otherResult).store(result);
    Require(scePthreadMutexUnlock(&state.mutex) == SCE_OK);
    return nullptr;
}

static void SignalToTargetOnly() {
    for (int round = 0; round < 10; ++round) {
        TargetedWait state;
        Require(scePthreadMutexInit(&state.mutex, nullptr, nullptr) == SCE_OK);
        Require(scePthreadCondInit(&state.cond, nullptr, nullptr) == SCE_OK);
        Require(scePthreadMutexLock(&state.mutex) == SCE_OK);

        TargetedWaitArgument targetArgument{&state, true};
        TargetedWaitArgument otherArgument{&state, false};
        Pthread target = nullptr;
        Pthread other = nullptr;
        Require(scePthreadCreate(&target, nullptr, TargetedWaiter, &targetArgument, nullptr) == SCE_OK);
        Require(scePthreadCreate(&other, nullptr, TargetedWaiter, &otherArgument, nullptr) == SCE_OK);
        Require(scePthreadMutexUnlock(&state.mutex) == SCE_OK);

        while (state.ready.load() != 2)
            std::this_thread::yield();

        Require(scePthreadMutexLock(&state.mutex) == SCE_OK);
        Require(scePthreadCondSignalto(&state.cond, target) == SCE_OK);
        Require(scePthreadMutexUnlock(&state.mutex) == SCE_OK);

        Require(scePthreadJoin(target, nullptr) == SCE_OK);
        Require(scePthreadJoin(other, nullptr) == SCE_OK);
        Require(state.targetResult.load() == 0);
        Require(state.otherResult.load() == POSIX_ETIMEDOUT);

        Require(scePthreadCondDestroy(&state.cond) == SCE_OK);
        Require(scePthreadMutexDestroy(&state.mutex) == SCE_OK);
    }
}



struct DisabledCancelWait {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    Pthread target = nullptr;
    bool recursive = false;
    std::atomic<bool> entered{false};
    std::atomic<bool> cancelStarted{false};
    std::atomic<bool> cancelReturned{false};
    std::atomic<int> waitResult{-1};
};

static void* APS5_VABI DisabledCancelWaiter(void* arg) {
    auto& state = *static_cast<DisabledCancelWait*>(arg);
    Require(scePthreadSetcancelstate(GUEST_CANCEL_DISABLE, nullptr) == SCE_OK);
    Require(scePthreadMutexLock(&state.mutex) == SCE_OK);
    if (state.recursive)
        Require(scePthreadMutexLock(&state.mutex) == SCE_OK);
    state.entered.store(true, std::memory_order_release);
    state.waitResult.store(scePthreadCondWait(&state.cond, &state.mutex), std::memory_order_release);
    if (state.recursive)
        Require(scePthreadMutexUnlock(&state.mutex) == SCE_OK);
    Require(scePthreadMutexUnlock(&state.mutex) == SCE_OK);
    return nullptr;
}

static void* APS5_VABI DisabledWaitCanceller(void* arg) {
    auto& state = *static_cast<DisabledCancelWait*>(arg);
    state.cancelStarted.store(true, std::memory_order_release);
    Require(scePthreadCancel(state.target) == SCE_OK);
    state.cancelReturned.store(true, std::memory_order_release);
    return nullptr;
}

static void CancelDisabledWaitReturnsBeforeSignal(bool recursive) {
    DisabledCancelWait state;
    state.recursive = recursive;
    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    if (recursive)
        Require(scePthreadMutexattrSettype(&attr, MUTEX_TYPE_RECURSIVE) == SCE_OK);
    Require(scePthreadMutexInit(&state.mutex, &attr, nullptr) == SCE_OK);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
    Require(scePthreadCondInit(&state.cond, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCreate(&state.target, nullptr, DisabledCancelWaiter, &state, nullptr) == SCE_OK);
    while (!state.entered.load(std::memory_order_acquire))
        std::this_thread::yield();

    Require(scePthreadMutexLock(&state.mutex) == SCE_OK);
    Pthread canceller = nullptr;
    Require(scePthreadCreate(&canceller, nullptr, DisabledWaitCanceller, &state, nullptr) == SCE_OK);
    while (!state.cancelStarted.load(std::memory_order_acquire))
        std::this_thread::yield();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!state.cancelReturned.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    const bool returnedBeforeSignal = state.cancelReturned.load(std::memory_order_acquire);
    Require(scePthreadCondSignal(&state.cond) == SCE_OK);
    Require(scePthreadMutexUnlock(&state.mutex) == SCE_OK);

    Require(scePthreadJoin(canceller, nullptr) == SCE_OK);
    Require(scePthreadJoin(state.target, nullptr) == SCE_OK);
    Require(returnedBeforeSignal);
    Require(state.waitResult.load(std::memory_order_acquire) == SCE_OK);
    Require(scePthreadCondDestroy(&state.cond) == SCE_OK);
    Require(scePthreadMutexDestroy(&state.mutex) == SCE_OK);
}

struct UnlockedTargetedWait {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    std::atomic<bool> entered{false};
    std::atomic<int> result{-1};
};

static void* APS5_VABI UntimedTargetWaiter(void* arg) {
    auto& state = *static_cast<UnlockedTargetedWait*>(arg);
    Require(scePthreadMutexLock(&state.mutex) == SCE_OK);
    state.entered.store(true, std::memory_order_release);
    state.result.store(scePthreadCondWait(&state.cond, &state.mutex), std::memory_order_release);
    Require(scePthreadMutexUnlock(&state.mutex) == SCE_OK);
    return nullptr;
}

static void SignalToWithoutMutex() {
    for (int round = 0; round < 100; ++round) {
        UnlockedTargetedWait state;
        Require(scePthreadMutexInit(&state.mutex, nullptr, nullptr) == SCE_OK);
        Require(scePthreadCondInit(&state.cond, nullptr, nullptr) == SCE_OK);
        Pthread target = nullptr;
        Require(scePthreadCreate(&target, nullptr, UntimedTargetWaiter, &state, nullptr) == SCE_OK);
        while (!state.entered.load(std::memory_order_acquire))
            std::this_thread::yield();

        int signalResult = SCE_KERNEL_ERROR_EPERM;
        while (signalResult == SCE_KERNEL_ERROR_EPERM) {
            signalResult = scePthreadCondSignalto(&state.cond, target);
            if (signalResult == SCE_KERNEL_ERROR_EPERM)
                std::this_thread::yield();
        }
        Require(signalResult == SCE_OK);
        Require(scePthreadJoin(target, nullptr) == SCE_OK);
        Require(state.result.load(std::memory_order_acquire) == SCE_OK);
        Require(scePthreadCondDestroy(&state.cond) == SCE_OK);
        Require(scePthreadMutexDestroy(&state.mutex) == SCE_OK);
    }
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
    DistantAbsoluteDeadline();
    SignalTo();
    SignalToTargetOnly();
    CancelDisabledWaitReturnsBeforeSignal(false);
    CancelDisabledWaitReturnsBeforeSignal(true);
    SignalToWithoutMutex();
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
        errno = 71;
        Require(pthread_cond_timedwait_nid_postfix(&cond, &context.mutex, reinterpret_cast<const KernelTimespec*>(0x10)) == POSIX_EINVAL);
        Require(errno == 71);
        const KernelTimespec past{0, 0};
        Require(pthread_cond_timedwait_nid_postfix(&cond, &context.mutex, &past) == POSIX_ETIMEDOUT);
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
