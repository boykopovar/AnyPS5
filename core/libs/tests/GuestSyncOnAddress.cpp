#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

static constexpr int SCE_OK = 0;
static constexpr KernelUseconds FAILSAFE_TIMEOUT = 10000000;
static constexpr KernelUseconds SHORT_TIMEOUT = 1000;
static constexpr auto SETTLE = std::chrono::milliseconds(100);

static void Require(bool value) { if (!value) std::abort(); }

static void AwaitCount(const std::atomic<int>& counter, int value) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (counter.load() != value) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

static void FailsAgainWhenTheValueDiffers() {
    std::uint32_t word = 2;
    Require(sceKernelSyncOnAddressWait(&word, 1, nullptr, "differs") == SCE_KERNEL_ERROR_EAGAIN);
    KernelUseconds timeout = SHORT_TIMEOUT;
    Require(sceKernelSyncOnAddressWait(&word, 1, &timeout, nullptr) == SCE_KERNEL_ERROR_EAGAIN && timeout == SHORT_TIMEOUT);
    timeout = 0;
    Require(sceKernelSyncOnAddressWait(&word, 1, &timeout, nullptr) == SCE_KERNEL_ERROR_ETIMEDOUT);
}

static void TimesOutWhileTheValueMatches() {
    std::uint32_t word = 1;
    KernelUseconds none = 0;
    Require(sceKernelSyncOnAddressWait(&word, 1, &none, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT && none == 0);

    KernelUseconds timeout = 20000;
    const auto start = std::chrono::steady_clock::now();
    Require(sceKernelSyncOnAddressWait(&word, 1, &timeout, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));
    Require(timeout == 0);
    Require(sceKernelSyncOnAddressWake(&word, INT_MAX) == 0);
}

static void WakeReleasesAWaiterAndReportsTheTimeLeft() {
    std::uint32_t word = 1;
    std::atomic<int> started{0};
    std::atomic<int> finished{0};
    int result = -1;
    KernelUseconds timeout = FAILSAFE_TIMEOUT;
    std::thread waiter([&] {
        ++started;
        result = sceKernelSyncOnAddressWait(&word, 1, &timeout, "wake");
        ++finished;
    });
    AwaitCount(started, 1);
    std::this_thread::sleep_for(SETTLE);
    int woken = 0;
    while (finished.load() == 0) {
        woken += sceKernelSyncOnAddressWake(&word, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    waiter.join();
    Require(result == SCE_OK && woken == 1);
    Require(timeout > 0 && timeout < FAILSAFE_TIMEOUT);
}

static void WakeCountsOfOneOrLessWakeOneWaiter() {
    std::uint32_t word = 1;
    std::uint32_t other = 1;
    std::atomic<int> started{0};
    std::atomic<int> finished{0};
    std::atomic<int> otherFinished{0};
    std::vector<int> results(5, -1);
    int otherResult = -1;

    std::vector<std::thread> waiters;
    for (int i = 0; i < 5; ++i) {
        waiters.emplace_back([&, i] {
            ++started;
            KernelUseconds timeout = FAILSAFE_TIMEOUT;
            results[i] = sceKernelSyncOnAddressWait(&word, 1, &timeout, "count");
            ++finished;
        });
    }
    std::thread otherWaiter([&] {
        ++started;
        KernelUseconds timeout = FAILSAFE_TIMEOUT;
        otherResult = sceKernelSyncOnAddressWait(&other, 1, &timeout, "other");
        ++otherFinished;
    });
    AwaitCount(started, 6);
    std::this_thread::sleep_for(SETTLE);

    int expected = 0;
    for (const std::int32_t count : {0, -1, 1}) {
        Require(sceKernelSyncOnAddressWake(&word, count) == 1);
        AwaitCount(finished, ++expected);
        std::this_thread::sleep_for(SETTLE);
        Require(finished.load() == expected);
    }

    Require(sceKernelSyncOnAddressWake(&word, INT_MAX) == 2);
    AwaitCount(finished, 5);
    std::this_thread::sleep_for(SETTLE);
    Require(otherFinished.load() == 0);
    Require(sceKernelSyncOnAddressWake(&word, INT_MAX) == 0);

    Require(sceKernelSyncOnAddressWake(&other, 1) == 1);
    for (auto& waiter : waiters) waiter.join();
    otherWaiter.join();
    for (const int result : results) Require(result == SCE_OK);
    Require(otherResult == SCE_OK);
}

static void WaitsOnTheLowHalfOfAnOnceState() {
    std::atomic<std::uint64_t> state{1};
    auto* word = reinterpret_cast<std::uint32_t*>(&state);
    std::thread waiter([&] {
        while (state.load() == 1) {
            const int result = sceKernelSyncOnAddressWait(word, 1, nullptr, "once");
            Require(result == SCE_OK || result == SCE_KERNEL_ERROR_EAGAIN);
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    state.store(2);
    Require(sceKernelSyncOnAddressWake(word, INT_MAX) >= 0);
    waiter.join();
}

static void SizedWaitsTimeOutWhileTheValueMatches() {
    std::uint32_t word = 5;
    std::uint64_t value = 0x500000005ULL;
    KernelUseconds timeout = SHORT_TIMEOUT;
    Require(sceKernelSyncOnAddressWait32(&word, 4, &timeout, "differs") == SCE_KERNEL_ERROR_EAGAIN);
    KernelUseconds none = 0;
    Require(sceKernelSyncOnAddressWait32(&word, 4, &none, "differs") == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(sceKernelSyncOnAddressWait32(&word, 5, &none, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(sceKernelSyncOnAddressWait64(&value, 0x500000005ULL, &none, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT);

    timeout = 20000;
    auto start = std::chrono::steady_clock::now();
    Require(sceKernelSyncOnAddressWait32(&word, 5, &timeout, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT && timeout == 0);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));
    timeout = 20000;
    start = std::chrono::steady_clock::now();
    Require(sceKernelSyncOnAddressWait64(&value, 0x500000005ULL, &timeout, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT && timeout == 0);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));
}

static void Wait64ComparesTheHighHalf() {
    std::uint64_t value = 0x100000001ULL;
    KernelUseconds timeout = SHORT_TIMEOUT;
    Require(sceKernelSyncOnAddressWait64(&value, 0x1ULL, &timeout, "high") == SCE_KERNEL_ERROR_EAGAIN);
    Require(sceKernelSyncOnAddressWait64(&value, 0x200000001ULL, &timeout, "high") == SCE_KERNEL_ERROR_EAGAIN);
    Require(sceKernelSyncOnAddressWait64(&value, 0x100000001ULL, &timeout, "high") == SCE_KERNEL_ERROR_ETIMEDOUT);
}

static void WakeReleasesWaitersOfEverySize() {
    std::uint64_t value = 1;
    auto* word = reinterpret_cast<std::uint32_t*>(&value);
    std::atomic<int> started{0};
    std::atomic<int> finished{0};
    std::vector<int> results(3, -1);

    std::vector<std::thread> waiters;
    waiters.emplace_back([&] {
        ++started;
        KernelUseconds timeout = FAILSAFE_TIMEOUT;
        results[0] = sceKernelSyncOnAddressWait(word, 1, &timeout, "sized");
        ++finished;
    });
    waiters.emplace_back([&] {
        ++started;
        KernelUseconds timeout = FAILSAFE_TIMEOUT;
        results[1] = sceKernelSyncOnAddressWait32(word, 1, &timeout, "sized");
        ++finished;
    });
    waiters.emplace_back([&] {
        ++started;
        KernelUseconds timeout = FAILSAFE_TIMEOUT;
        results[2] = sceKernelSyncOnAddressWait64(&value, 1, &timeout, "sized");
        ++finished;
    });
    AwaitCount(started, 3);
    std::this_thread::sleep_for(SETTLE);

    Require(sceKernelSyncOnAddressWake(&value, 1) == 1);
    AwaitCount(finished, 1);
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 1);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (finished.load() != 3) {
        Require(std::chrono::steady_clock::now() < deadline);
        Require(sceKernelSyncOnAddressWake(&value, INT_MAX) >= 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for (auto& waiter : waiters) waiter.join();
    for (const int result : results) Require(result == SCE_OK);
}

static void NarrowWaitsCompareOnlyTheirWidth() {
    KernelUseconds timeout = SHORT_TIMEOUT;
    alignas(4) std::uint8_t bytes[4] = {1, 5, 7, 7};
    Require(sceKernelSyncOnAddressWait8(&bytes[1], 4, &timeout, "differs") == SCE_KERNEL_ERROR_EAGAIN);
    Require(sceKernelSyncOnAddressWait8(&bytes[1], 5, &timeout, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT);

    timeout = SHORT_TIMEOUT;
    alignas(4) std::uint16_t halves[2] = {1, 0x0705};
    Require(sceKernelSyncOnAddressWait16(&halves[1], 0x0005, &timeout, "high") == SCE_KERNEL_ERROR_EAGAIN);
    Require(sceKernelSyncOnAddressWait16(&halves[1], 0x0705, &timeout, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT);
}

static void WakeReleasesNarrowWaitersAtTheirOwnAddress() {
    alignas(4) std::uint16_t halves[2] = {0, 9};
    auto* bytes = reinterpret_cast<std::uint8_t*>(halves);
    bytes[1] = 3;
    auto* half = &halves[1];
    std::atomic<int> started{0};
    std::atomic<int> byteFinished{0};
    std::atomic<int> halfFinished{0};
    int byteResult = -1;
    int halfResult = -1;

    std::thread byteWaiter([&] {
        ++started;
        KernelUseconds timeout = FAILSAFE_TIMEOUT;
        byteResult = sceKernelSyncOnAddressWait8(&bytes[1], 3, &timeout, "byte");
        ++byteFinished;
    });
    std::thread halfWaiter([&] {
        ++started;
        KernelUseconds timeout = FAILSAFE_TIMEOUT;
        halfResult = sceKernelSyncOnAddressWait16(half, 9, &timeout, "half");
        ++halfFinished;
    });
    AwaitCount(started, 2);
    std::this_thread::sleep_for(SETTLE);

    Require(sceKernelSyncOnAddressWake(&bytes[0], INT_MAX) == 0);
    Require(sceKernelSyncOnAddressWake(&bytes[3], INT_MAX) == 0);
    std::this_thread::sleep_for(SETTLE);
    Require(byteFinished.load() == 0 && halfFinished.load() == 0);

    Require(sceKernelSyncOnAddressWake(&bytes[1], 1) == 1);
    AwaitCount(byteFinished, 1);
    Require(halfFinished.load() == 0);
    Require(sceKernelSyncOnAddressWake(half, 1) == 1);
    AwaitCount(halfFinished, 1);

    byteWaiter.join();
    halfWaiter.join();
    Require(byteResult == SCE_OK && halfResult == SCE_OK);
}

static void MisalignedAddressesWaitAndWake() {
    alignas(16) unsigned char storage[16] = {};
    auto* word = reinterpret_cast<std::uint32_t*>(storage + 1);
    auto* quad = reinterpret_cast<std::uint64_t*>(storage + 4);
    const std::uint32_t wordValue = 0x11223344;
    std::memcpy(word, &wordValue, sizeof(wordValue));
    KernelUseconds timeout = SHORT_TIMEOUT;
    Require(sceKernelSyncOnAddressWait32(word, 1, &timeout, "misaligned") == SCE_KERNEL_ERROR_EAGAIN);
    Require(sceKernelSyncOnAddressWait32(word, wordValue, &timeout, "misaligned") == SCE_KERNEL_ERROR_ETIMEDOUT && timeout == 0);
    timeout = SHORT_TIMEOUT;
    Require(sceKernelSyncOnAddressWait64(quad, 1, &timeout, "misaligned") == SCE_KERNEL_ERROR_EAGAIN);
    timeout = SHORT_TIMEOUT;
    Require(sceKernelSyncOnAddressWait16(reinterpret_cast<std::uint16_t*>(storage + 9), 1, &timeout, "misaligned") == SCE_KERNEL_ERROR_EAGAIN);

    std::atomic<int> started{0};
    int result = -1;
    std::thread waiter([&] {
        ++started;
        KernelUseconds waitTimeout = FAILSAFE_TIMEOUT;
        result = sceKernelSyncOnAddressWait32(word, wordValue, &waitTimeout, "misaligned");
    });
    AwaitCount(started, 1);
    std::this_thread::sleep_for(SETTLE);
    Require(sceKernelSyncOnAddressWake(storage, INT_MAX) == 0);
    Require(sceKernelSyncOnAddressWake(word, 1) == 1);
    waiter.join();
    Require(result == SCE_OK);

    auto* shared = reinterpret_cast<std::uint64_t*>(storage + 3);
    std::atomic<bool> stop{false};
    std::atomic<int> rounds{0};
    std::thread changer([&] {
        std::uint64_t next = 1;
        while (!stop.load()) {
            __atomic_store_n(reinterpret_cast<unsigned char*>(shared), static_cast<unsigned char>(next), __ATOMIC_RELAXED);
            ++next;
            sceKernelSyncOnAddressWake(shared, INT_MAX);
        }
    });
    for (int round = 0; round < 2000; ++round) {
        unsigned char bytes[sizeof(std::uint64_t)];
        bytes[0] = __atomic_load_n(reinterpret_cast<unsigned char*>(shared), __ATOMIC_RELAXED);
        std::memcpy(bytes + 1, reinterpret_cast<unsigned char*>(shared) + 1, sizeof(bytes) - 1);
        std::uint64_t seen = 0;
        std::memcpy(&seen, bytes, sizeof(seen));
        KernelUseconds waitTimeout = SHORT_TIMEOUT;
        const int waited = sceKernelSyncOnAddressWait64(shared, seen, &waitTimeout, "changing");
        Require(waited == SCE_OK || waited == SCE_KERNEL_ERROR_EAGAIN || waited == SCE_KERNEL_ERROR_ETIMEDOUT);
        ++rounds;
    }
    stop.store(true);
    changer.join();
    Require(rounds.load() == 2000);
}

static void NullAddresses() {
    KernelUseconds timeout = SHORT_TIMEOUT;
    Require(sceKernelSyncOnAddressWait(nullptr, 1, &timeout, nullptr) == SCE_KERNEL_ERROR_EFAULT && timeout == SHORT_TIMEOUT);
    Require(sceKernelSyncOnAddressWait8(nullptr, 1, nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelSyncOnAddressWait16(nullptr, 1, nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelSyncOnAddressWait32(nullptr, 1, nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelSyncOnAddressWait64(nullptr, 1, nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    KernelUseconds none = 0;
    Require(sceKernelSyncOnAddressWait32(nullptr, 1, &none, nullptr) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(sceKernelSyncOnAddressWake(nullptr, 1) == 0);
    Require(sceKernelSyncOnAddressWake(nullptr, 0) == 0);
    std::uint32_t word = 0;
    Require(sceKernelSyncOnAddressWake(&word, -1) == 0);
}

int main() {
    FailsAgainWhenTheValueDiffers();
    TimesOutWhileTheValueMatches();
    WakeReleasesAWaiterAndReportsTheTimeLeft();
    WakeCountsOfOneOrLessWakeOneWaiter();
    WaitsOnTheLowHalfOfAnOnceState();
    SizedWaitsTimeOutWhileTheValueMatches();
    Wait64ComparesTheHighHalf();
    WakeReleasesWaitersOfEverySize();
    NarrowWaitsCompareOnlyTheirWidth();
    WakeReleasesNarrowWaitersAtTheirOwnAddress();
    MisalignedAddressesWaitAndWake();
    NullAddresses();
}
