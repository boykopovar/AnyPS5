#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace {

constexpr int PosixEinval = 22;
constexpr int PosixEtimedout = 60;
constexpr int GuestRealtimeClock = 0;
constexpr std::int64_t NanosPerSecond = 1000000000;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename F>
bool ThrowsRuntimeError(F&& fn) {
    try {
        fn();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

KernelTimespec After(std::int64_t millis) {
    KernelTimespec now{};
    clock_gettime_nid_postfix(GuestRealtimeClock, &now);
    const std::int64_t nanos = now.tv_sec * NanosPerSecond + now.tv_nsec + millis * 1000000;
    return {nanos / NanosPerSecond, nanos % NanosPerSecond};
}

void TestParameterValidationAndNullPointers() {
    KernelTimespec validDeadline = After(100);
    Require(ThrowsRuntimeError([&] { pthread_rwlock_timedrdlock_nid_postfix(nullptr, &validDeadline); }));
    Require(ThrowsRuntimeError([&] { pthread_rwlock_timedwrlock_nid_postfix(nullptr, &validDeadline); }));

    PthreadRwlock rw = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rw, nullptr) == 0 && rw != nullptr);

    Require(pthread_rwlock_timedrdlock_nid_postfix(&rw, nullptr) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);

    Require(pthread_rwlock_timedwrlock_nid_postfix(&rw, nullptr) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);

    Require(pthread_rwlock_destroy_nid_postfix(&rw) == 0);
}

void TestDestroyedRwlockPrecedence() {
    PthreadRwlock destroyed = reinterpret_cast<PthreadRwlock>(std::uintptr_t{1});
    KernelTimespec valid = After(50);
    KernelTimespec invalid{valid.tv_sec, NanosPerSecond};
    KernelTimespec past = After(-1000);

    Require(pthread_rwlock_timedrdlock_nid_postfix(&destroyed, &valid) == PosixEinval);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&destroyed, &invalid) == PosixEinval);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&destroyed, &past) == PosixEinval);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&destroyed, nullptr) == PosixEinval);

    Require(pthread_rwlock_timedwrlock_nid_postfix(&destroyed, &valid) == PosixEinval);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&destroyed, &invalid) == PosixEinval);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&destroyed, &past) == PosixEinval);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&destroyed, nullptr) == PosixEinval);

    PthreadRwlock rw = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rw, nullptr) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rw) == 0);

    Require(pthread_rwlock_timedrdlock_nid_postfix(&rw, &valid) == PosixEinval);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rw, &invalid) == PosixEinval);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rw, &valid) == PosixEinval);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rw, &invalid) == PosixEinval);
}

void TestInvalidTimespecBounds() {
    PthreadRwlock rw = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rw, nullptr) == 0);

    std::atomic<bool> writeHeld{false};
    std::atomic<bool> allowRelease{false};

    std::thread holder([&] {
        Require(pthread_rwlock_wrlock_nid_postfix(&rw) == 0);
        writeHeld.store(true);
        while (!allowRelease.load()) {
            std::this_thread::yield();
        }
        Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);
    });

    while (!writeHeld.load()) {
        std::this_thread::yield();
    }

    KernelTimespec base = After(100);

    KernelTimespec invalidNsecNegative{base.tv_sec, -1};
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rw, &invalidNsecNegative) == PosixEinval);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rw, &invalidNsecNegative) == PosixEinval);

    KernelTimespec invalidNsecLargeNeg{base.tv_sec, -1000000};
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rw, &invalidNsecLargeNeg) == PosixEinval);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rw, &invalidNsecLargeNeg) == PosixEinval);

    KernelTimespec invalidNsecEqualSecond{base.tv_sec, NanosPerSecond};
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rw, &invalidNsecEqualSecond) == PosixEinval);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rw, &invalidNsecEqualSecond) == PosixEinval);

    KernelTimespec invalidNsecAboveSecond{base.tv_sec, NanosPerSecond + 1};
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rw, &invalidNsecAboveSecond) == PosixEinval);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rw, &invalidNsecAboveSecond) == PosixEinval);

    KernelTimespec invalidNsecLargeAbove{base.tv_sec, 2 * NanosPerSecond};
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rw, &invalidNsecLargeAbove) == PosixEinval);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rw, &invalidNsecLargeAbove) == PosixEinval);

    allowRelease.store(true);
    holder.join();

    Require(pthread_rwlock_destroy_nid_postfix(&rw) == 0);
}

void TestTimeoutExpiration() {
    PthreadRwlock rw = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rw, nullptr) == 0);

    std::atomic<bool> writeHeld{false};
    std::atomic<bool> allowRelease{false};

    std::thread writer([&] {
        Require(pthread_rwlock_wrlock_nid_postfix(&rw) == 0);
        writeHeld.store(true);
        while (!allowRelease.load()) {
            std::this_thread::yield();
        }
        Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);
    });

    while (!writeHeld.load()) {
        std::this_thread::yield();
    }

    KernelTimespec past = After(-2000);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rw, &past) == PosixEtimedout);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rw, &past) == PosixEtimedout);

    KernelTimespec shortTimeout = After(20);
    const auto startRd = std::chrono::steady_clock::now();
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rw, &shortTimeout) == PosixEtimedout);
    Require(std::chrono::steady_clock::now() - startRd >= std::chrono::milliseconds(10));

    shortTimeout = After(20);
    const auto startWr = std::chrono::steady_clock::now();
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rw, &shortTimeout) == PosixEtimedout);
    Require(std::chrono::steady_clock::now() - startWr >= std::chrono::milliseconds(10));

    allowRelease.store(true);
    writer.join();

    Require(pthread_rwlock_destroy_nid_postfix(&rw) == 0);
}

void TestSuccessfulTimedAcquisition() {
    PthreadRwlock rw = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rw, nullptr) == 0);

    std::atomic<bool> writeHeld{false};

    std::thread holder([&] {
        Require(pthread_rwlock_wrlock_nid_postfix(&rw) == 0);
        writeHeld.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);
    });

    while (!writeHeld.load()) {
        std::this_thread::yield();
    }

    KernelTimespec generousDeadline = After(5000);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rw, &generousDeadline) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rw) == 0);

    holder.join();

    Require(pthread_rwlock_destroy_nid_postfix(&rw) == 0);
}

void TestStaticAutoInitialization() {
    PthreadRwlock freshRd = nullptr;
    KernelTimespec deadline = After(50);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&freshRd, &deadline) == 0);
    Require(freshRd != nullptr);
    Require(pthread_rwlock_unlock_nid_postfix(&freshRd) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&freshRd) == 0);

    PthreadRwlock freshWr = nullptr;
    deadline = After(50);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&freshWr, &deadline) == 0);
    Require(freshWr != nullptr);
    Require(pthread_rwlock_unlock_nid_postfix(&freshWr) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&freshWr) == 0);
}

} // namespace

int main() {
    TestParameterValidationAndNullPointers();
    TestDestroyedRwlockPrecedence();
    TestInvalidTimespecBounds();
    TestTimeoutExpiration();
    TestSuccessfulTimedAcquisition();
    TestStaticAutoInitialization();
    return 0;
}
