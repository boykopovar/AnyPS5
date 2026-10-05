#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <vector>

extern "C" {
int APS5_VABI sceKernelSyncOnAddressWait(std::uint32_t* address, std::uint32_t expected, const KernelUseconds* timeout, const char* name);
int APS5_VABI sceKernelSyncOnAddressWait32(std::uint32_t* address, std::uint32_t expected, const KernelUseconds* timeout, const char* name);
int APS5_VABI sceKernelSyncOnAddressWait64(std::uint64_t* address, std::uint64_t expected, const KernelUseconds* timeout, const char* name);
int APS5_VABI sceKernelSyncOnAddressWake(void* address, std::int32_t count);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003C);
static constexpr KernelUseconds FAILSAFE_TIMEOUT = 10000000;
static constexpr auto SETTLE = std::chrono::milliseconds(100);

static void Require(bool value) { if (!value) std::abort(); }

template <class TCall>
static bool Rejects(TCall call) {
    try { call(); }
    catch (const std::invalid_argument&) { return true; }
    return false;
}

static void AwaitCount(const std::atomic<int>& counter, int value) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (counter.load() != value) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

static void ReturnsWhenTheValueDiffers() {
    std::uint32_t word = 2;
    Require(sceKernelSyncOnAddressWait(&word, 1, nullptr, "differs") == SCE_OK);
    const KernelUseconds timeout = 0;
    Require(sceKernelSyncOnAddressWait(&word, 1, &timeout, nullptr) == SCE_OK);
}

static void TimesOutWhileTheValueMatches() {
    std::uint32_t word = 1;
    const KernelUseconds none = 0;
    Require(sceKernelSyncOnAddressWait(&word, 1, &none, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT);

    const KernelUseconds timeout = 20000;
    const auto start = std::chrono::steady_clock::now();
    Require(sceKernelSyncOnAddressWait(&word, 1, &timeout, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));
    Require(sceKernelSyncOnAddressWake(&word, INT_MAX) == SCE_OK);
}

static void WakeReleasesAWaiterWithoutAValueChange() {
    std::uint32_t word = 1;
    std::atomic<int> finished{0};
    int result = -1;
    std::thread waiter([&] {
        result = sceKernelSyncOnAddressWait(&word, 1, &FAILSAFE_TIMEOUT, "wake");
        ++finished;
    });
    while (finished.load() == 0) {
        Require(sceKernelSyncOnAddressWake(&word, 1) == SCE_OK);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    waiter.join();
    Require(result == SCE_OK);
}

static void WakeHonoursTheCountAndTheAddress() {
    std::uint32_t word = 1;
    std::uint32_t other = 1;
    std::atomic<int> started{0};
    std::atomic<int> finished{0};
    std::atomic<int> otherFinished{0};
    std::vector<int> results(3, -1);
    int otherResult = -1;

    std::vector<std::thread> waiters;
    for (int i = 0; i < 3; ++i) {
        waiters.emplace_back([&, i] {
            ++started;
            results[i] = sceKernelSyncOnAddressWait(&word, 1, &FAILSAFE_TIMEOUT, "count");
            ++finished;
        });
    }
    std::thread otherWaiter([&] {
        ++started;
        otherResult = sceKernelSyncOnAddressWait(&other, 1, &FAILSAFE_TIMEOUT, "other");
        ++otherFinished;
    });
    AwaitCount(started, 4);
    std::this_thread::sleep_for(SETTLE);

    Require(sceKernelSyncOnAddressWake(&word, 0) == SCE_OK);
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 0);

    Require(sceKernelSyncOnAddressWake(&word, 1) == SCE_OK);
    AwaitCount(finished, 1);
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 1);

    Require(sceKernelSyncOnAddressWake(&word, INT_MAX) == SCE_OK);
    AwaitCount(finished, 3);
    std::this_thread::sleep_for(SETTLE);
    Require(otherFinished.load() == 0);

    Require(sceKernelSyncOnAddressWake(&other, 1) == SCE_OK);
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
            Require(sceKernelSyncOnAddressWait(word, 1, nullptr, "once") == SCE_OK);
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    state.store(2);
    Require(sceKernelSyncOnAddressWake(word, INT_MAX) == SCE_OK);
    waiter.join();
}

static void SizedWaitsTimeOutWhileTheValueMatches() {
    std::uint32_t word = 5;
    std::uint64_t value = 0x500000005ULL;
    const KernelUseconds none = 0;
    Require(sceKernelSyncOnAddressWait32(&word, 4, &none, "differs") == SCE_OK);
    Require(sceKernelSyncOnAddressWait32(&word, 5, &none, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(sceKernelSyncOnAddressWait64(&value, 0x500000005ULL, &none, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT);

    const KernelUseconds timeout = 20000;
    auto start = std::chrono::steady_clock::now();
    Require(sceKernelSyncOnAddressWait32(&word, 5, &timeout, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));
    start = std::chrono::steady_clock::now();
    Require(sceKernelSyncOnAddressWait64(&value, 0x500000005ULL, &timeout, "timeout") == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));
}

static void Wait64ComparesTheHighHalf() {
    std::uint64_t value = 0x100000001ULL;
    const KernelUseconds none = 0;
    Require(sceKernelSyncOnAddressWait64(&value, 0x1ULL, &none, "high") == SCE_OK);
    Require(sceKernelSyncOnAddressWait64(&value, 0x200000001ULL, &none, "high") == SCE_OK);
    Require(sceKernelSyncOnAddressWait64(&value, 0x100000001ULL, &none, "high") == SCE_KERNEL_ERROR_ETIMEDOUT);
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
        results[0] = sceKernelSyncOnAddressWait(word, 1, &FAILSAFE_TIMEOUT, "sized");
        ++finished;
    });
    waiters.emplace_back([&] {
        ++started;
        results[1] = sceKernelSyncOnAddressWait32(word, 1, &FAILSAFE_TIMEOUT, "sized");
        ++finished;
    });
    waiters.emplace_back([&] {
        ++started;
        results[2] = sceKernelSyncOnAddressWait64(&value, 1, &FAILSAFE_TIMEOUT, "sized");
        ++finished;
    });
    AwaitCount(started, 3);
    std::this_thread::sleep_for(SETTLE);

    Require(sceKernelSyncOnAddressWake(&value, 1) == SCE_OK);
    AwaitCount(finished, 1);
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 1);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (finished.load() != 3) {
        Require(std::chrono::steady_clock::now() < deadline);
        Require(sceKernelSyncOnAddressWake(&value, INT_MAX) == SCE_OK);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for (auto& waiter : waiters) waiter.join();
    for (const int result : results) Require(result == SCE_OK);
}

static void RejectsInvalidArguments() {
    std::uint32_t words[2] = {1, 1};
    auto* misaligned = reinterpret_cast<std::uint32_t*>(reinterpret_cast<unsigned char*>(words) + 1);
    Require(Rejects([&] { sceKernelSyncOnAddressWait(nullptr, 1, nullptr, nullptr); }));
    Require(Rejects([&] { sceKernelSyncOnAddressWait(misaligned, 1, nullptr, nullptr); }));
    Require(Rejects([&] { sceKernelSyncOnAddressWait32(nullptr, 1, nullptr, nullptr); }));
    Require(Rejects([&] { sceKernelSyncOnAddressWait32(misaligned, 1, nullptr, nullptr); }));
    Require(Rejects([&] { sceKernelSyncOnAddressWake(nullptr, 1); }));
    Require(Rejects([&] { sceKernelSyncOnAddressWake(misaligned, 1); }));
    Require(Rejects([&] { sceKernelSyncOnAddressWake(words, -1); }));

    std::uint64_t values[2] = {1, 1};
    auto* wordAligned = reinterpret_cast<std::uint64_t*>(reinterpret_cast<unsigned char*>(values) + sizeof(std::uint32_t));
    Require(Rejects([&] { sceKernelSyncOnAddressWait64(nullptr, 1, nullptr, nullptr); }));
    Require(Rejects([&] { sceKernelSyncOnAddressWait64(wordAligned, 1, nullptr, nullptr); }));
}

int main() {
    ReturnsWhenTheValueDiffers();
    TimesOutWhileTheValueMatches();
    WakeReleasesAWaiterWithoutAValueChange();
    WakeHonoursTheCountAndTheAddress();
    WaitsOnTheLowHalfOfAnOnceState();
    SizedWaitsTimeOutWhileTheValueMatches();
    Wait64ComparesTheHighHalf();
    WakeReleasesWaitersOfEverySize();
    RejectsInvalidArguments();
}
