#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>
#include <time.h>

namespace {

void Require(bool value) {
    if (!value)
        std::abort();
}

template<typename TException, typename TCall>
void RequireThrows(TCall call) {
    try {
        call();
    } catch (const TException&) {
        return;
    }
    std::abort();
}

constexpr int threadSuccess = 0;
constexpr int threadBusy = 3;
constexpr int mutexTryable = 0x02;
constexpr int mutexRecursive = 0x100;
constexpr std::int64_t nanosPerSecond = 1000000000LL;

void TestNearbyintf() {
    const float half = nearbyintf_nid_postfix(0.5f);
    Require(half == 0.0f);
    Require(!std::signbit(half));

    const float negHalf = nearbyintf_nid_postfix(-0.5f);
    Require(negHalf == 0.0f);
    Require(std::signbit(negHalf));

    Require(nearbyintf_nid_postfix(1.5f) == 2.0f);
    Require(nearbyintf_nid_postfix(-1.5f) == -2.0f);

    Require(nearbyintf_nid_postfix(2.5f) == 2.0f);
    Require(nearbyintf_nid_postfix(-2.5f) == -2.0f);

    const float negZero = nearbyintf_nid_postfix(-0.0f);
    Require(negZero == 0.0f);
    Require(std::signbit(negZero));

    const float inf = std::numeric_limits<float>::infinity();
    const float posInf = nearbyintf_nid_postfix(inf);
    Require(std::isinf(posInf) && posInf > 0.0f);

    const float negInf = nearbyintf_nid_postfix(-inf);
    Require(std::isinf(negInf) && negInf < 0.0f);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    Require(std::isnan(nearbyintf_nid_postfix(nan)));
}

void TestMtxTrylock() {
    void* mutex = nullptr;
    Require(_Mtx_init_nid_postfix(&mutex, mutexTryable) == threadSuccess && mutex != nullptr);

    Require(_Mtx_trylock_nid_postfix(&mutex) == threadSuccess);
    Require(_Mtx_trylock_nid_postfix(&mutex) == threadBusy);
    Require(_Mtx_unlock_nid_postfix(&mutex) == threadSuccess);

    std::atomic<bool> held{false};
    std::atomic<bool> release{false};
    std::thread other([&] {
        Require(_Mtx_lock_nid_postfix(&mutex) == threadSuccess);
        held.store(true, std::memory_order_release);
        while (!release.load(std::memory_order_acquire))
            std::this_thread::yield();
        Require(_Mtx_unlock_nid_postfix(&mutex) == threadSuccess);
    });

    while (!held.load(std::memory_order_acquire))
        std::this_thread::yield();

    Require(_Mtx_trylock_nid_postfix(&mutex) == threadBusy);

    release.store(true, std::memory_order_release);
    other.join();

    Require(_Mtx_trylock_nid_postfix(&mutex) == threadSuccess);
    Require(_Mtx_unlock_nid_postfix(&mutex) == threadSuccess);
    _Mtx_destroy_nid_postfix(&mutex);

    void* recMutex = nullptr;
    Require(_Mtx_init_nid_postfix(&recMutex, mutexTryable | mutexRecursive) == threadSuccess && recMutex != nullptr);
    Require(_Mtx_trylock_nid_postfix(&recMutex) == threadSuccess);
    Require(_Mtx_trylock_nid_postfix(&recMutex) == threadSuccess);
    Require(_Mtx_trylock_nid_postfix(&recMutex) == threadSuccess);

    std::thread([&] {
        Require(_Mtx_trylock_nid_postfix(&recMutex) == threadBusy);
    }).join();

    Require(_Mtx_unlock_nid_postfix(&recMutex) == threadSuccess);
    std::thread([&] {
        Require(_Mtx_trylock_nid_postfix(&recMutex) == threadBusy);
    }).join();

    Require(_Mtx_unlock_nid_postfix(&recMutex) == threadSuccess);
    std::thread([&] {
        Require(_Mtx_trylock_nid_postfix(&recMutex) == threadBusy);
    }).join();

    Require(_Mtx_unlock_nid_postfix(&recMutex) == threadSuccess);

    std::thread([&] {
        Require(_Mtx_trylock_nid_postfix(&recMutex) == threadSuccess);
        Require(_Mtx_unlock_nid_postfix(&recMutex) == threadSuccess);
    }).join();

    _Mtx_destroy_nid_postfix(&recMutex);
}

void TestThrdSleep() {
    timespec before{};
    Require(clock_gettime(CLOCK_REALTIME, &before) == 0);

    const std::int64_t sleepNanos = 20000000LL;
    const std::int64_t targetTotal = static_cast<std::int64_t>(before.tv_sec) * nanosPerSecond + before.tv_nsec + sleepNanos;
    const Xtime target{targetTotal / nanosPerSecond, targetTotal % nanosPerSecond};

    Require(_Thrd_sleep_nid_postfix(&target, nullptr) == 0);

    timespec after{};
    Require(clock_gettime(CLOCK_REALTIME, &after) == 0);
    const std::int64_t afterTotal = static_cast<std::int64_t>(after.tv_sec) * nanosPerSecond + after.tv_nsec;
    Require(afterTotal >= targetTotal);

    const Xtime past{before.tv_sec - 1, before.tv_nsec};
    Require(_Thrd_sleep_nid_postfix(&past, nullptr) == 0);

    RequireThrows<std::invalid_argument>([] {
        _Thrd_sleep_nid_postfix(nullptr, nullptr);

    });
}

}

int main() {
    TestNearbyintf();
    TestMtxTrylock();
    TestThrdSleep();
    return 0;
}
