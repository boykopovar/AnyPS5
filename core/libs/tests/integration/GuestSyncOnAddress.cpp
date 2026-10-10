#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

extern "C" {
int APS5_VABI sceKernelSyncOnAddressWait(std::uint32_t* address, std::uint32_t expected, const KernelUseconds* timeout, const char* name);
int APS5_VABI sceKernelSyncOnAddressWait32(std::uint32_t* address, std::uint32_t expected, const KernelUseconds* timeout, const char* name);
int APS5_VABI sceKernelSyncOnAddressWait64(std::uint64_t* address, std::uint64_t expected, const KernelUseconds* timeout, const char* name);
int APS5_VABI sceKernelSyncOnAddressWait8(std::uint8_t* address, std::uint8_t expected, const KernelUseconds* timeout, const char* name);
int APS5_VABI sceKernelSyncOnAddressWait16(std::uint16_t* address, std::uint16_t expected, const KernelUseconds* timeout, const char* name);
int APS5_VABI sceKernelSyncOnAddressWake(void* address, std::int32_t count);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int SCE_OK = 0;
constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003C);
constexpr KernelUseconds failsafeTimeout = 10000000;
constexpr KernelUseconds noTimeout = 0;
constexpr KernelUseconds shortTimeout = 20000;
constexpr auto settle = std::chrono::milliseconds(100);
constexpr auto minimumElapsed = std::chrono::milliseconds(15);

class WaiterThreads {
public:
    explicit WaiterThreads(std::function<void()> release) : release(std::move(release)) {}

    ~WaiterThreads() {
        release();
        JoinAll();
    }

    WaiterThreads(const WaiterThreads&) = delete;
    WaiterThreads& operator=(const WaiterThreads&) = delete;

    template<typename TBody>
    void Start(TBody body) {
        threads.emplace_back(std::move(body));
    }

    void JoinAll() {
        for (auto& thread : threads) {
            if (thread.joinable()) thread.join();
        }
    }

private:
    std::function<void()> release;
    std::vector<std::thread> threads;
};

void AwaitCount(const std::atomic<int>& counter, int value, const std::string& message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (counter.load() != value) {
        Require(std::chrono::steady_clock::now() < deadline, message + ": timed out waiting for " + std::to_string(value));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

struct Rejection {
    const char* name;
    std::function<void()> call;
};

void RequireRejections(const std::vector<Rejection>& rejections) {
    for (const auto& rejection : rejections) {
        RequireThrows<std::invalid_argument>(rejection.call, rejection.name);
    }
}

const Case differentValue{"SyncOnAddressWait_ValueDiffers_ReturnsImmediately", [] {
    std::uint32_t word = 2;
    RequireEqual(sceKernelSyncOnAddressWait(&word, 1, nullptr, "differs"), SCE_OK, "no timeout");
    RequireEqual(sceKernelSyncOnAddressWait(&word, 1, &noTimeout, nullptr), SCE_OK, "zero timeout");
}};

const Case zeroTimeout{"SyncOnAddressWait_ZeroTimeoutWhileValueMatches_TimesOut", [] {
    std::uint32_t word = 1;
    RequireEqual(sceKernelSyncOnAddressWait(&word, 1, &noTimeout, "timeout"), SCE_KERNEL_ERROR_ETIMEDOUT, "zero timeout");
}};

const Case timedWait{"SyncOnAddressWait_TimeoutWhileValueMatches_TimesOutAfterTheTimeout", [] {
    std::uint32_t word = 1;
    const auto start = std::chrono::steady_clock::now();
    RequireEqual(sceKernelSyncOnAddressWait(&word, 1, &shortTimeout, "timeout"), SCE_KERNEL_ERROR_ETIMEDOUT, "20 ms timeout");
    Require(std::chrono::steady_clock::now() - start >= minimumElapsed, "waited at least 15 ms");
}};

const Case wakeWithoutWaiters{"SyncOnAddressWake_NoWaiters_Succeeds", [] {
    std::uint32_t word = 1;
    RequireEqual(sceKernelSyncOnAddressWake(&word, INT_MAX), SCE_OK, "wake all");
}};

const Case wakeWithoutChange{"SyncOnAddressWake_WaiterWithoutValueChange_ReleasesWaiter", [] {
    std::uint32_t word = 1;
    std::atomic<int> finished{0};
    int result = -1;
    WaiterThreads threads([&word] { sceKernelSyncOnAddressWake(&word, INT_MAX); });
    threads.Start([&] {
        result = sceKernelSyncOnAddressWait(&word, 1, &failsafeTimeout, "wake");
        ++finished;
    });
    while (finished.load() == 0) {
        RequireEqual(sceKernelSyncOnAddressWake(&word, 1), SCE_OK, "wake one");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    threads.JoinAll();
    RequireEqual(result, SCE_OK, "waiter result");
}};

const Case wakeCountAndAddress{"SyncOnAddressWake_CountAndAddress_ReleaseOnlyMatchingWaiters", [] {
    std::uint32_t word = 1;
    std::uint32_t other = 1;
    std::atomic<int> started{0};
    std::atomic<int> finished{0};
    std::atomic<int> otherFinished{0};
    std::array<int, 3> results{-1, -1, -1};
    int otherResult = -1;
    WaiterThreads threads([&word, &other] {
        sceKernelSyncOnAddressWake(&word, INT_MAX);
        sceKernelSyncOnAddressWake(&other, INT_MAX);
    });
    for (int index = 0; index < 3; ++index) {
        threads.Start([&, index] {
            ++started;
            results[index] = sceKernelSyncOnAddressWait(&word, 1, &failsafeTimeout, "count");
            ++finished;
        });
    }
    threads.Start([&] {
        ++started;
        otherResult = sceKernelSyncOnAddressWait(&other, 1, &failsafeTimeout, "other");
        ++otherFinished;
    });
    AwaitCount(started, 4, "waiters started");
    std::this_thread::sleep_for(settle);

    RequireEqual(sceKernelSyncOnAddressWake(&word, 0), SCE_OK, "wake zero");
    std::this_thread::sleep_for(settle);
    RequireEqual(finished.load(), 0, "finished after waking zero");

    RequireEqual(sceKernelSyncOnAddressWake(&word, 1), SCE_OK, "wake one");
    AwaitCount(finished, 1, "finished after waking one");
    std::this_thread::sleep_for(settle);
    RequireEqual(finished.load(), 1, "finished after waking one and settling");

    RequireEqual(sceKernelSyncOnAddressWake(&word, INT_MAX), SCE_OK, "wake all");
    AwaitCount(finished, 3, "finished after waking all");
    std::this_thread::sleep_for(settle);
    RequireEqual(otherFinished.load(), 0, "other address waiter finished");

    RequireEqual(sceKernelSyncOnAddressWake(&other, 1), SCE_OK, "wake other");
    threads.JoinAll();
    for (int index = 0; index < 3; ++index) {
        RequireEqual(results[index], SCE_OK, "waiter " + std::to_string(index) + " result");
    }
    RequireEqual(otherResult, SCE_OK, "other waiter result");
}};

const Case onceState{"SyncOnAddressWait_LowHalfOfOnceState_ReleasedByStateChangeAndWake", [] {
    std::atomic<std::uint64_t> state{1};
    auto* word = reinterpret_cast<std::uint32_t*>(&state);
    std::atomic<int> failure{SCE_OK};
    WaiterThreads threads([&state, word] {
        state.store(2);
        sceKernelSyncOnAddressWake(word, INT_MAX);
    });
    threads.Start([&] {
        while (state.load() == 1) {
            const int result = sceKernelSyncOnAddressWait(word, 1, nullptr, "once");
            if (result != SCE_OK) {
                failure.store(result);
                break;
            }
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    state.store(2);
    RequireEqual(sceKernelSyncOnAddressWake(word, INT_MAX), SCE_OK, "wake");
    threads.JoinAll();
    RequireEqual(failure.load(), SCE_OK, "wait result inside the loop");
}};

const Case sizedZeroTimeout{"SyncOnAddressWait32And64_ZeroTimeout_ComparesValue", [] {
    std::uint32_t word = 5;
    std::uint64_t value = 0x500000005ULL;
    RequireEqual(sceKernelSyncOnAddressWait32(&word, 4, &noTimeout, "differs"), SCE_OK, "wait32 differs");
    RequireEqual(sceKernelSyncOnAddressWait32(&word, 5, &noTimeout, "timeout"), SCE_KERNEL_ERROR_ETIMEDOUT, "wait32 matches");
    RequireEqual(sceKernelSyncOnAddressWait64(&value, 0x500000005ULL, &noTimeout, "timeout"), SCE_KERNEL_ERROR_ETIMEDOUT,
                 "wait64 matches");
}};

const Case sizedTimedWait{"SyncOnAddressWait32And64_TimeoutWhileValueMatches_TimesOutAfterTheTimeout", [] {
    std::uint32_t word = 5;
    std::uint64_t value = 0x500000005ULL;
    auto start = std::chrono::steady_clock::now();
    RequireEqual(sceKernelSyncOnAddressWait32(&word, 5, &shortTimeout, "timeout"), SCE_KERNEL_ERROR_ETIMEDOUT, "wait32");
    Require(std::chrono::steady_clock::now() - start >= minimumElapsed, "wait32 waited at least 15 ms");
    start = std::chrono::steady_clock::now();
    RequireEqual(sceKernelSyncOnAddressWait64(&value, 0x500000005ULL, &shortTimeout, "timeout"), SCE_KERNEL_ERROR_ETIMEDOUT,
                 "wait64");
    Require(std::chrono::steady_clock::now() - start >= minimumElapsed, "wait64 waited at least 15 ms");
}};

const Case wait64HighHalf{"SyncOnAddressWait64_HighHalfDiffers_ReturnsImmediately", [] {
    std::uint64_t value = 0x100000001ULL;
    RequireEqual(sceKernelSyncOnAddressWait64(&value, 0x1ULL, &noTimeout, "high"), SCE_OK, "expected 0x1");
    RequireEqual(sceKernelSyncOnAddressWait64(&value, 0x200000001ULL, &noTimeout, "high"), SCE_OK, "expected 0x200000001");
    RequireEqual(sceKernelSyncOnAddressWait64(&value, 0x100000001ULL, &noTimeout, "high"), SCE_KERNEL_ERROR_ETIMEDOUT,
                 "expected 0x100000001");
}};

const Case everySize{"SyncOnAddressWake_WaitersOfEverySize_ReleasesEachOnce", [] {
    std::uint64_t value = 1;
    auto* word = reinterpret_cast<std::uint32_t*>(&value);
    std::atomic<int> started{0};
    std::atomic<int> finished{0};
    std::array<int, 3> results{-1, -1, -1};
    WaiterThreads threads([&value] { sceKernelSyncOnAddressWake(&value, INT_MAX); });
    threads.Start([&] {
        ++started;
        results[0] = sceKernelSyncOnAddressWait(word, 1, &failsafeTimeout, "sized");
        ++finished;
    });
    threads.Start([&] {
        ++started;
        results[1] = sceKernelSyncOnAddressWait32(word, 1, &failsafeTimeout, "sized");
        ++finished;
    });
    threads.Start([&] {
        ++started;
        results[2] = sceKernelSyncOnAddressWait64(&value, 1, &failsafeTimeout, "sized");
        ++finished;
    });
    AwaitCount(started, 3, "waiters started");
    std::this_thread::sleep_for(settle);

    RequireEqual(sceKernelSyncOnAddressWake(&value, 1), SCE_OK, "wake one");
    AwaitCount(finished, 1, "finished after waking one");
    std::this_thread::sleep_for(settle);
    RequireEqual(finished.load(), 1, "finished after waking one and settling");

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (finished.load() != 3) {
        Require(std::chrono::steady_clock::now() < deadline, "every waiter finished within 5 s");
        RequireEqual(sceKernelSyncOnAddressWake(&value, INT_MAX), SCE_OK, "wake all");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    threads.JoinAll();
    const char* names[] = {"wait", "wait32", "wait64"};
    for (int index = 0; index < 3; ++index) RequireEqual(results[index], SCE_OK, names[index]);
}};

const Case narrowWidth{"SyncOnAddressWait8And16_ZeroTimeout_CompareOnlyTheirWidth", [] {
    alignas(4) std::uint8_t bytes[4] = {1, 5, 7, 7};
    RequireEqual(sceKernelSyncOnAddressWait8(&bytes[1], 4, &noTimeout, "differs"), SCE_OK, "wait8 differs");
    RequireEqual(sceKernelSyncOnAddressWait8(&bytes[1], 5, &noTimeout, "timeout"), SCE_KERNEL_ERROR_ETIMEDOUT, "wait8 matches");

    alignas(4) std::uint16_t halves[2] = {1, 0x0705};
    RequireEqual(sceKernelSyncOnAddressWait16(&halves[1], 0x0005, &noTimeout, "high"), SCE_OK, "wait16 differs in high byte");
    RequireEqual(sceKernelSyncOnAddressWait16(&halves[1], 0x0705, &noTimeout, "timeout"), SCE_KERNEL_ERROR_ETIMEDOUT,
                 "wait16 matches");
}};

const Case narrowAddress{"SyncOnAddressWake_NarrowWaiters_ReleasedOnlyAtTheirOwnAddress", [] {
    alignas(4) std::uint16_t halves[2] = {0, 9};
    auto* bytes = reinterpret_cast<std::uint8_t*>(halves);
    bytes[1] = 3;
    auto* half = &halves[1];
    std::atomic<int> started{0};
    std::atomic<int> byteFinished{0};
    std::atomic<int> halfFinished{0};
    int byteResult = -1;
    int halfResult = -1;
    WaiterThreads threads([bytes, half] {
        sceKernelSyncOnAddressWake(&bytes[1], INT_MAX);
        sceKernelSyncOnAddressWake(half, INT_MAX);
    });
    threads.Start([&] {
        ++started;
        byteResult = sceKernelSyncOnAddressWait8(&bytes[1], 3, &failsafeTimeout, "byte");
        ++byteFinished;
    });
    threads.Start([&] {
        ++started;
        halfResult = sceKernelSyncOnAddressWait16(half, 9, &failsafeTimeout, "half");
        ++halfFinished;
    });
    AwaitCount(started, 2, "waiters started");
    std::this_thread::sleep_for(settle);

    RequireEqual(sceKernelSyncOnAddressWake(&bytes[0], INT_MAX), SCE_OK, "wake byte 0");
    RequireEqual(sceKernelSyncOnAddressWake(&bytes[3], INT_MAX), SCE_OK, "wake byte 3");
    std::this_thread::sleep_for(settle);
    RequireEqual(byteFinished.load(), 0, "byte waiter finished after unrelated wakes");
    RequireEqual(halfFinished.load(), 0, "half waiter finished after unrelated wakes");

    RequireEqual(sceKernelSyncOnAddressWake(&bytes[1], 1), SCE_OK, "wake byte 1");
    AwaitCount(byteFinished, 1, "byte waiter finished");
    RequireEqual(halfFinished.load(), 0, "half waiter finished after byte wake");
    RequireEqual(sceKernelSyncOnAddressWake(half, 1), SCE_OK, "wake half");
    AwaitCount(halfFinished, 1, "half waiter finished");

    threads.JoinAll();
    RequireEqual(byteResult, SCE_OK, "byte waiter result");
    RequireEqual(halfResult, SCE_OK, "half waiter result");
}};

const Case invalidWait32{"SyncOnAddressWait32_NullOrMisalignedAddress_ThrowsInvalidArgument", [] {
    std::uint32_t words[2] = {1, 1};
    auto* misaligned = reinterpret_cast<std::uint32_t*>(reinterpret_cast<unsigned char*>(words) + 1);
    RequireRejections({
        {"wait null", [] { sceKernelSyncOnAddressWait(nullptr, 1, nullptr, nullptr); }},
        {"wait misaligned", [misaligned] { sceKernelSyncOnAddressWait(misaligned, 1, nullptr, nullptr); }},
        {"wait32 null", [] { sceKernelSyncOnAddressWait32(nullptr, 1, nullptr, nullptr); }},
        {"wait32 misaligned", [misaligned] { sceKernelSyncOnAddressWait32(misaligned, 1, nullptr, nullptr); }},
    });
}};

const Case invalidNarrow{"SyncOnAddressWait8And16_NullOrMisalignedAddress_ThrowsInvalidArgument", [] {
    std::uint32_t words[2] = {1, 1};
    auto* oddHalf = reinterpret_cast<std::uint16_t*>(reinterpret_cast<unsigned char*>(words) + 1);
    RequireRejections({
        {"wait8 null", [] { sceKernelSyncOnAddressWait8(nullptr, 1, nullptr, nullptr); }},
        {"wait16 null", [] { sceKernelSyncOnAddressWait16(nullptr, 1, nullptr, nullptr); }},
        {"wait16 misaligned", [oddHalf] { sceKernelSyncOnAddressWait16(oddHalf, 1, nullptr, nullptr); }},
    });
}};

const Case invalidWait64{"SyncOnAddressWait64_NullOrWordAlignedAddress_ThrowsInvalidArgument", [] {
    std::uint64_t values[2] = {1, 1};
    auto* wordAligned = reinterpret_cast<std::uint64_t*>(reinterpret_cast<unsigned char*>(values) + sizeof(std::uint32_t));
    RequireRejections({
        {"wait64 null", [] { sceKernelSyncOnAddressWait64(nullptr, 1, nullptr, nullptr); }},
        {"wait64 word aligned", [wordAligned] { sceKernelSyncOnAddressWait64(wordAligned, 1, nullptr, nullptr); }},
    });
}};

const Case invalidWake{"SyncOnAddressWake_NullAddressOrNegativeCount_ThrowsInvalidArgument", [] {
    std::uint32_t words[2] = {1, 1};
    RequireRejections({
        {"wake null", [] { sceKernelSyncOnAddressWake(nullptr, 1); }},
        {"wake count -1", [&words] { sceKernelSyncOnAddressWake(words, -1); }},
    });
}};

const Case misalignedWake{"SyncOnAddressWake_MisalignedAddress_Succeeds", [] {
    std::uint32_t words[2] = {1, 1};
    auto* misaligned = reinterpret_cast<std::uint32_t*>(reinterpret_cast<unsigned char*>(words) + 1);
    RequireEqual(sceKernelSyncOnAddressWake(misaligned, 1), SCE_OK, "misaligned wake");
}};

} // namespace
