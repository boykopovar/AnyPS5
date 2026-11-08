#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

namespace {

constexpr int SceOk = 0;
constexpr int PosixOk = 0;
constexpr int PosixEperm = 1;
constexpr int PosixEinval = 22;
constexpr int PosixEtimedout = 60;
constexpr int PosixEbusy = 16;
constexpr KernelClockid ClockRealtimeId = 0;
constexpr KernelClockid ClockMonotonicId = 4;

void Require(bool value) {
    if (!value)
        std::abort();
}

struct TrylockerContext {
    PthreadMutex mutex = nullptr;
    int trylockResult = PosixOk;
};

void* APS5_VABI TrylockerEntry(void* arg) {
    auto& context = *static_cast<TrylockerContext*>(arg);
    context.trylockResult = pthread_mutex_trylock_nid_postfix(&context.mutex);
    return nullptr;
}

void RequireHeldByCaller(PthreadMutex& mutex) {
    TrylockerContext context{mutex, PosixOk};
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, TrylockerEntry, &context, nullptr) == SceOk);
    Require(scePthreadJoin(thread, nullptr) == SceOk);
    Require(context.trylockResult == PosixEbusy);
}

struct SignalContext {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    std::atomic<bool> ready{false};
};

void* APS5_VABI SignalEntry(void* arg) {
    auto& context = *static_cast<SignalContext*>(arg);
    Require(pthread_mutex_lock_nid_postfix(&context.mutex) == PosixOk);
    context.ready.store(true, std::memory_order_release);
    Require(pthread_cond_signal_nid_postfix(&context.cond) == PosixOk);
    Require(pthread_mutex_unlock_nid_postfix(&context.mutex) == PosixOk);
    return nullptr;
}

struct HolderContext {
    PthreadMutex mutex = nullptr;
    std::atomic<bool> locked{false};
    std::atomic<bool> release{false};
};

void* APS5_VABI HolderEntry(void* arg) {
    auto& context = *static_cast<HolderContext*>(arg);
    Require(pthread_mutex_lock_nid_postfix(&context.mutex) == PosixOk);
    context.locked.store(true, std::memory_order_release);
    while (!context.release.load(std::memory_order_acquire))
        std::this_thread::yield();
    Require(pthread_mutex_unlock_nid_postfix(&context.mutex) == PosixOk);
    return nullptr;
}

void AuditMonotonicProgression() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == PosixOk);
    KernelClockid initialClock = -1;
    Require(pthread_condattr_getclock_nid_postfix(&attr, &initialClock) == PosixOk);
    Require(initialClock == ClockRealtimeId);

    Require(pthread_condattr_setclock_nid_postfix(&attr, 9999) == PosixEinval);
    Require(pthread_condattr_setclock_nid_postfix(&attr, ClockMonotonicId) == PosixOk);
    KernelClockid configuredClock = -1;
    Require(pthread_condattr_getclock_nid_postfix(&attr, &configuredClock) == PosixOk);
    Require(configuredClock == ClockMonotonicId);

    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, &attr) == PosixOk);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == PosixOk);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == PosixOk);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == PosixOk);
    const KernelTimespec negativeSec{-1, 0};
    const KernelTimespec negativeNsec{1, -1};
    const KernelTimespec overflowNsec{1, 1000000000};
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, nullptr) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &negativeSec) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &negativeNsec) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &overflowNsec) == PosixEinval);

    KernelTimespec beforeMono{};
    Require(clock_gettime_nid_postfix(ClockMonotonicId, &beforeMono) == PosixOk);

    KernelTimespec deadline = beforeMono;
    deadline.tv_nsec += 40000000;
    if (deadline.tv_nsec >= 1000000000) {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000;
    }

    const auto wallStart = std::chrono::steady_clock::now();
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &deadline) == PosixEtimedout);
    const auto wallElapsed = std::chrono::steady_clock::now() - wallStart;
    Require(wallElapsed >= std::chrono::milliseconds(30));

    KernelTimespec afterMono{};
    Require(clock_gettime_nid_postfix(ClockMonotonicId, &afterMono) == PosixOk);
    const bool progressed = (afterMono.tv_sec > beforeMono.tv_sec) ||
        (afterMono.tv_sec == beforeMono.tv_sec && afterMono.tv_nsec >= beforeMono.tv_nsec);
    Require(progressed);
    const std::int64_t elapsedMonoNs = (afterMono.tv_sec - beforeMono.tv_sec) * 1000000000LL + (afterMono.tv_nsec - beforeMono.tv_nsec);
    Require(elapsedMonoNs >= 30000000LL);
    RequireHeldByCaller(mutex);

    SignalContext signalContext{mutex, cond, false};
    Pthread signalThread = nullptr;
    Require(scePthreadCreate(&signalThread, nullptr, SignalEntry, &signalContext, nullptr) == SceOk);

    KernelTimespec farDeadline{};
    Require(clock_gettime_nid_postfix(ClockMonotonicId, &farDeadline) == PosixOk);
    farDeadline.tv_sec += 10;
    while (!signalContext.ready.load(std::memory_order_acquire))
        Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &farDeadline) == PosixOk);

    RequireHeldByCaller(mutex);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == PosixOk);
    Require(scePthreadJoin(signalThread, nullptr) == SceOk);

    Require(pthread_cond_destroy_nid_postfix(&cond) == PosixOk);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == PosixOk);
}

void AuditZeroTimeout() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == PosixOk);
    Require(pthread_condattr_setclock_nid_postfix(&attr, ClockMonotonicId) == PosixOk);

    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, &attr) == PosixOk);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == PosixOk);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == PosixOk);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == PosixOk);

    KernelTimespec currentMono{};
    Require(clock_gettime_nid_postfix(ClockMonotonicId, &currentMono) == PosixOk);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &currentMono) == PosixEtimedout);
    RequireHeldByCaller(mutex);

    KernelTimespec pastMono = currentMono;
    if (pastMono.tv_sec > 2)
        pastMono.tv_sec -= 2;
    else
        pastMono = KernelTimespec{0, 0};
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &pastMono) == PosixEtimedout);
    RequireHeldByCaller(mutex);

    const KernelTimespec zeroDeadline{0, 0};
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &zeroDeadline) == PosixEtimedout);
    RequireHeldByCaller(mutex);

    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &mutex, 0) == PosixEtimedout);
    RequireHeldByCaller(mutex);

    Require(scePthreadCondTimedwait(&cond, &mutex, 0) == SCE_KERNEL_ERROR_ETIMEDOUT);
    RequireHeldByCaller(mutex);

    Require(pthread_mutex_unlock_nid_postfix(&mutex) == PosixOk);
    Require(pthread_cond_destroy_nid_postfix(&cond) == PosixOk);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == PosixOk);
}

void AuditUnownedMutex() {
    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == PosixOk);

    KernelTimespec futureMono{};
    Require(clock_gettime_nid_postfix(ClockMonotonicId, &futureMono) == PosixOk);
    futureMono.tv_sec += 5;

    PthreadMutex unlockedMutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&unlockedMutex, nullptr) == PosixOk);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &unlockedMutex, &futureMono) == PosixEperm);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &unlockedMutex, 50000) == PosixEperm);
    Require(scePthreadCondTimedwait(&cond, &unlockedMutex, 50000) == SCE_KERNEL_ERROR_EPERM);
    Require(pthread_cond_wait_nid_postfix(&cond, &unlockedMutex) == PosixEperm);
    Require(scePthreadCondWait(&cond, &unlockedMutex) == SCE_KERNEL_ERROR_EPERM);
    Require(pthread_mutex_destroy_nid_postfix(&unlockedMutex) == PosixOk);

    PthreadMutex uninitializedMutex = nullptr;
    Require(pthread_cond_timedwait_nid_postfix(&cond, &uninitializedMutex, &futureMono) == PosixEperm);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &uninitializedMutex, 50000) == PosixEperm);
    Require(scePthreadCondTimedwait(&cond, &uninitializedMutex, 50000) == SCE_KERNEL_ERROR_EPERM);
    Require(pthread_cond_wait_nid_postfix(&cond, &uninitializedMutex) == PosixEperm);
    Require(scePthreadCondWait(&cond, &uninitializedMutex) == SCE_KERNEL_ERROR_EPERM);

    HolderContext holderContext;
    Require(pthread_mutex_init_nid_postfix(&holderContext.mutex, nullptr) == PosixOk);
    Pthread holderThread = nullptr;
    Require(scePthreadCreate(&holderThread, nullptr, HolderEntry, &holderContext, nullptr) == SceOk);
    while (!holderContext.locked.load(std::memory_order_acquire))
        std::this_thread::yield();

    Require(pthread_cond_timedwait_nid_postfix(&cond, &holderContext.mutex, &futureMono) == PosixEperm);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &holderContext.mutex, 50000) == PosixEperm);
    Require(scePthreadCondTimedwait(&cond, &holderContext.mutex, 50000) == SCE_KERNEL_ERROR_EPERM);
    Require(pthread_cond_wait_nid_postfix(&cond, &holderContext.mutex) == PosixEperm);
    Require(scePthreadCondWait(&cond, &holderContext.mutex) == SCE_KERNEL_ERROR_EPERM);

    holderContext.release.store(true, std::memory_order_release);
    Require(scePthreadJoin(holderThread, nullptr) == SceOk);
    Require(pthread_mutex_destroy_nid_postfix(&holderContext.mutex) == PosixOk);

    PthreadMutex destroyedMutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&destroyedMutex, nullptr) == PosixOk);
    Require(pthread_mutex_destroy_nid_postfix(&destroyedMutex) == PosixOk);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &destroyedMutex, &futureMono) == PosixEinval);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &destroyedMutex, 50000) == PosixEinval);
    Require(scePthreadCondTimedwait(&cond, &destroyedMutex, 50000) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_cond_wait_nid_postfix(&cond, &destroyedMutex) == PosixEinval);
    Require(scePthreadCondWait(&cond, &destroyedMutex) == SCE_KERNEL_ERROR_EINVAL);

    Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, &futureMono) == PosixEinval);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, nullptr, 50000) == PosixEinval);
    Require(pthread_cond_wait_nid_postfix(&cond, nullptr) == PosixEinval);

    Require(pthread_cond_destroy_nid_postfix(&cond) == PosixOk);
}

}

int main() {
    AuditMonotonicProgression();
    AuditZeroTimeout();
    AuditUnownedMutex();
    return 0;
}
