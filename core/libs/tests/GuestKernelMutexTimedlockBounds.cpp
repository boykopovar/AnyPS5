#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

#define Require(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "%s:%d: Require failed: %s\n", __FILE__, __LINE__, #condition); \
            std::fflush(stderr); \
            std::abort(); \
        } \
    } while (false)

namespace {

constexpr int PosixEbusy = 16;
constexpr int PosixEinval = 22;
constexpr int PosixEtimedout = 60;
constexpr int GuestRealtimeClock = 0;
constexpr std::int64_t NanosPerSecond = 1000000000;
constexpr int MutexTypeRecursive = 2;

KernelTimespec FutureDeadline(std::int64_t millis) {
    KernelTimespec now{};
    clock_gettime_nid_postfix(GuestRealtimeClock, &now);
    const std::int64_t totalNanos = now.tv_nsec + millis * 1000000;
    return {now.tv_sec + totalNanos / NanosPerSecond, totalNanos % NanosPerSecond};
}

KernelTimespec PastDeadline() {
    KernelTimespec now{};
    clock_gettime_nid_postfix(GuestRealtimeClock, &now);
    return {now.tv_sec > 10 ? now.tv_sec - 10 : 0, 0};
}

void TestParameterValidationAndNullPointers() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0 && mutex != nullptr);

    Require(pthread_mutex_timedlock_nid_postfix(&mutex, nullptr) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Require(pthread_mutex_reltimedlock_np_nid_postfix(&mutex, 1000) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

void TestDestroyedMutexPrecedence() {
    PthreadMutex destroyed = reinterpret_cast<PthreadMutex>(std::uintptr_t{2});
    KernelTimespec valid = FutureDeadline(50);
    KernelTimespec invalid{valid.tv_sec, NanosPerSecond};
    KernelTimespec past = PastDeadline();

    Require(pthread_mutex_timedlock_nid_postfix(&destroyed, &valid) == PosixEinval);
    Require(pthread_mutex_timedlock_nid_postfix(&destroyed, &invalid) == PosixEinval);
    Require(pthread_mutex_timedlock_nid_postfix(&destroyed, &past) == PosixEinval);
    Require(pthread_mutex_timedlock_nid_postfix(&destroyed, nullptr) == PosixEinval);
    Require(pthread_mutex_reltimedlock_np_nid_postfix(&destroyed, 1000) == PosixEinval);
    Require(pthread_mutex_reltimedlock_np_nid_postfix(&destroyed, 0) == PosixEinval);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);

    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &valid) == PosixEinval);
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &invalid) == PosixEinval);
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &past) == PosixEinval);
    Require(pthread_mutex_reltimedlock_np_nid_postfix(&mutex, 1000) == PosixEinval);
    Require(pthread_mutex_reltimedlock_np_nid_postfix(&mutex, 0) == PosixEinval);
}

void TestInvalidTimespecBounds() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    std::atomic<bool> held{false};
    std::atomic<bool> allowRelease{false};

    std::thread holder([&] {
        Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
        held.store(true);
        while (!allowRelease.load()) {
            std::this_thread::yield();
        }
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    });

    while (!held.load()) {
        std::this_thread::yield();
    }

    KernelTimespec base = FutureDeadline(100);

    KernelTimespec invalidNsecNegative{base.tv_sec, -1};
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &invalidNsecNegative) == PosixEinval);

    KernelTimespec invalidNsecLargeNeg{base.tv_sec, -1000000};
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &invalidNsecLargeNeg) == PosixEinval);

    KernelTimespec invalidNsecEqualSecond{base.tv_sec, NanosPerSecond};
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &invalidNsecEqualSecond) == PosixEinval);

    KernelTimespec invalidNsecAboveSecond{base.tv_sec, NanosPerSecond + 1};
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &invalidNsecAboveSecond) == PosixEinval);

    KernelTimespec invalidNsecLargeAbove{base.tv_sec, 2 * NanosPerSecond};
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &invalidNsecLargeAbove) == PosixEinval);

    allowRelease.store(true);
    holder.join();

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

void TestTimeoutExpiration() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    std::atomic<bool> held{false};
    std::atomic<bool> allowRelease{false};

    std::thread holder([&] {
        Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
        held.store(true);
        while (!allowRelease.load()) {
            std::this_thread::yield();
        }
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    });

    while (!held.load()) {
        std::this_thread::yield();
    }

    KernelTimespec past = PastDeadline();
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &past) == PosixEtimedout);

    KernelTimespec shortTimeout = FutureDeadline(20);
    const auto start = std::chrono::steady_clock::now();
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &shortTimeout) == PosixEtimedout);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(2));

    Require(pthread_mutex_reltimedlock_np_nid_postfix(&mutex, 0) == PosixEtimedout);

    const auto startRel = std::chrono::steady_clock::now();
    Require(pthread_mutex_reltimedlock_np_nid_postfix(&mutex, 20000) == PosixEtimedout);
    Require(std::chrono::steady_clock::now() - startRel >= std::chrono::milliseconds(2));

    allowRelease.store(true);
    holder.join();

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

void TestSuccessfulTimedAcquisition() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    std::atomic<bool> held{false};

    std::thread holder([&] {
        Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
        held.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    });

    while (!held.load()) {
        std::this_thread::yield();
    }

    KernelTimespec generousDeadline = FutureDeadline(5000);
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &generousDeadline) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    holder.join();

    held.store(false);
    std::thread holder2([&] {
        Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
        held.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    });

    while (!held.load()) {
        std::this_thread::yield();
    }

    Require(pthread_mutex_reltimedlock_np_nid_postfix(&mutex, 5000000) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    holder2.join();

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

void TestStaticAutoInitialization() {
    PthreadMutex fresh = nullptr;
    KernelTimespec deadline = FutureDeadline(50);
    Require(pthread_mutex_timedlock_nid_postfix(&fresh, &deadline) == 0);
    Require(fresh != nullptr);
    Require(pthread_mutex_unlock_nid_postfix(&fresh) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&fresh) == 0);

    PthreadMutex freshAdaptive = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
    deadline = FutureDeadline(50);
    Require(pthread_mutex_timedlock_nid_postfix(&freshAdaptive, &deadline) == 0);
    Require(freshAdaptive != nullptr);
    Require(pthread_mutex_unlock_nid_postfix(&freshAdaptive) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&freshAdaptive) == 0);

    PthreadMutex freshRel = nullptr;
    Require(pthread_mutex_reltimedlock_np_nid_postfix(&freshRel, 50000) == 0);
    Require(freshRel != nullptr);
    Require(pthread_mutex_unlock_nid_postfix(&freshRel) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&freshRel) == 0);
}

void TestRecursiveMutexTimedlock() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, MutexTypeRecursive) == 0);

    PthreadMutex recMutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&recMutex, &attr) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);

    KernelTimespec deadline = FutureDeadline(50);
    Require(pthread_mutex_timedlock_nid_postfix(&recMutex, &deadline) == 0);
    Require(pthread_mutex_timedlock_nid_postfix(&recMutex, &deadline) == 0);
    Require(pthread_mutex_reltimedlock_np_nid_postfix(&recMutex, 50000) == 0);

    Require(pthread_mutex_unlock_nid_postfix(&recMutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&recMutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&recMutex) == 0);

    Require(pthread_mutex_destroy_nid_postfix(&recMutex) == 0);
}

void TestDestroyLockedMutexFails() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == PosixEbusy);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

} // namespace

int main() {
    TestParameterValidationAndNullPointers();
    TestDestroyedMutexPrecedence();
    TestInvalidTimespecBounds();
    TestTimeoutExpiration();
    TestSuccessfulTimedAcquisition();
    TestStaticAutoInitialization();
    TestRecursiveMutexTimedlock();
    TestDestroyLockedMutexFails();
    return 0;
}
