#include "prx/libkernel/Time/include/Time.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

using Clock = std::chrono::steady_clock;

static constexpr KernelUseconds SHORT_MICROS = 20;
static constexpr unsigned LOOP_SLEEPS = 2000;

static void Require(bool value) {
    if (!value) std::abort();
}

static std::chrono::nanoseconds Usleep(KernelUseconds microseconds) {
    const auto start = Clock::now();
    Require(sceKernelUsleep_nid_postfix(microseconds) == 0);
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start);
}

static void EveryShortSleepTakesTheRequest() {
    for (const KernelUseconds microseconds : {1u, 5u, 20u, 49u}) {
        for (unsigned i = 0; i < 200; ++i) Require(Usleep(microseconds) >= std::chrono::microseconds(microseconds));
    }
}

static void ShortNanosleepTakesTheRequest() {
    const KernelTimespec request{0, 20000};
    for (unsigned i = 0; i < 200; ++i) {
        const auto start = Clock::now();
        Require(sceKernelNanosleep(&request, nullptr) == 0);
        Require(Clock::now() - start >= std::chrono::nanoseconds(20000));
    }
}

static void LoopOfShortSleepsTakesEveryRequest() {
    const auto start = Clock::now();
    for (unsigned i = 0; i < LOOP_SLEEPS; ++i) Require(sceKernelUsleep_nid_postfix(SHORT_MICROS) == 0);
    Require(Clock::now() - start >= std::chrono::microseconds(SHORT_MICROS) * LOOP_SLEEPS);
}

#ifdef _WIN32
static std::uint64_t CurrentThreadCpuNanos() {
    FILETIME creation{}, exit{}, kernel{}, user{};
    Require(GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user) != 0);
    ULARGE_INTEGER k, u;
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime;
    u.HighPart = user.dwHighDateTime;
    return (k.QuadPart + u.QuadPart) * 100ULL;
}

static bool BackToBackShortSleepsLeaveTheProcessor() {
    const auto cpuStart = CurrentThreadCpuNanos();
    const auto wallStart = Clock::now();
    std::uint64_t sleeps = 0;
    while (Clock::now() - wallStart < std::chrono::milliseconds(500)) {
        Require(Usleep(SHORT_MICROS) >= std::chrono::microseconds(SHORT_MICROS));
        ++sleeps;
    }
    const auto wallNanos = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - wallStart).count());
    const auto cpuNanos = CurrentThreadCpuNanos() - cpuStart;
    std::cout << "sceKernelUsleep(20 us) back to back: " << sleeps << " sleeps, calling-thread CPU " << std::fixed << std::setprecision(1)
              << (100.0 * static_cast<double>(cpuNanos) / static_cast<double>(wallNanos)) << "% (limit <= 40%)" << std::defaultfloat << std::endl;
    return cpuNanos <= (wallNanos * 40ULL) / 100ULL;
}

static void Work(std::chrono::microseconds length) {
    const auto until = Clock::now() + length;
    while (Clock::now() < until) YieldProcessor();
}

static bool ShortSleepAfterWorkStaysShort() {
    for (unsigned i = 0; i < LOOP_SLEEPS; ++i) Require(sceKernelUsleep_nid_postfix(SHORT_MICROS) == 0);
    std::array<std::int64_t, 101> samples{};
    for (auto& sample : samples) {
        Work(std::chrono::microseconds(200));
        sample = Usleep(SHORT_MICROS).count();
        Require(sample >= std::chrono::nanoseconds(std::chrono::microseconds(SHORT_MICROS)).count());
    }
    std::sort(samples.begin(), samples.end());
    const std::int64_t median = samples[samples.size() / 2];
    std::cout << "sceKernelUsleep(20 us) after 200 us of work: median " << median / 1000 << " us (limit < 100 us)" << std::endl;
    return median < 100000;
}

static void RequireInOneOfAttempts(bool (*test)(), int attempts) {
    for (int attempt = 0; attempt < attempts; ++attempt) {
        if (test()) return;
    }
    std::abort();
}
#endif

int main() {
    EveryShortSleepTakesTheRequest();
    ShortNanosleepTakesTheRequest();
    LoopOfShortSleepsTakesEveryRequest();
#ifdef _WIN32
    constexpr int ATTEMPTS = 3;
    RequireInOneOfAttempts(BackToBackShortSleepsLeaveTheProcessor, ATTEMPTS);
    RequireInOneOfAttempts(ShortSleepAfterWorkStaysShort, ATTEMPTS);
#endif
    return 0;
}
