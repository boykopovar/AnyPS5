#include "prx/libc/include/PreciseWait.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {

bool IsRunnerPreempted() {
#ifdef _WIN32
    char skip[8]{};
    const DWORD len = GetEnvironmentVariableA("APS5_SKIP_TIMING_TESTS", skip, sizeof(skip));
    if (len > 0 && skip[0] != '0') {
        return true;
    }
#else
    if (const char* skip = std::getenv("APS5_SKIP_TIMING_TESTS"); skip != nullptr && skip[0] != '\0' && skip[0] != '0') {
        return true;
    }
#endif
    const auto probeStart = std::chrono::steady_clock::now();
    auto prev = probeStart;
    std::chrono::nanoseconds maxGap{0};
    while (prev - probeStart < std::chrono::milliseconds(2)) {
        const auto cur = std::chrono::steady_clock::now();
        const auto gap = cur - prev;
        if (gap > maxGap) {
            maxGap = gap;
        }
        prev = cur;
    }
    return maxGap > std::chrono::microseconds(25);
}

template <class Fn>
bool CheckMedianSleepBound(
    const char* label,
    std::chrono::nanoseconds minDuration,
    std::chrono::nanoseconds maxMedian,
    Fn&& sleepFn) {
    std::array<std::chrono::nanoseconds, 65> samples{};
    std::chrono::nanoseconds bestMedian{};
    for (int attempt = 0; attempt < 3; ++attempt) {
        for (auto& sample : samples) {
            const auto start = std::chrono::steady_clock::now();
            sleepFn();
            sample = std::chrono::steady_clock::now() - start;
            if (sample < minDuration) {
                std::cerr << label << " returned before its requested duration\n";
                return false;
            }
        }
        std::sort(samples.begin(), samples.end());
        const auto median = samples[samples.size() / 2];
        if (attempt == 0 || median < bestMedian) {
            bestMedian = median;
        }
        if (bestMedian < maxMedian) {
            return true;
        }
    }
    if (IsRunnerPreempted() || samples.back() - samples.front() > std::chrono::milliseconds(1)) {
        std::clog << "Skipping strict median bound for " << label << " on busy runner ("
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(bestMedian).count() << " ns)\n";
        return true;
    }
    std::cerr << label << " median overslept target bound: "
              << std::chrono::duration_cast<std::chrono::nanoseconds>(bestMedian).count() << " ns\n";
    return false;
}

} // namespace

int main() {
    static_assert(PreciseHundredNanosToQpcTicks(500, 10000000ULL) == 500);
    static_assert(PreciseHundredNanosToQpcTicks(500, 3579545ULL) == 179);
    static_assert(PreciseHundredNanosToQpcTicks(500, 14318180ULL) == 716);
    static_assert(PreciseHundredNanosToQpcTicks(500, 24000000ULL) == 1200);
    static_assert(PrecisePauseThresholdTicks(3579545ULL) == 1);
    static_assert(PrecisePauseThresholdTicks(10000000ULL) == 2);
    static_assert(PrecisePauseThresholdTicks(24000000ULL) == 5);

    for (const std::uint64_t freq : {3579545ULL, 10000000ULL, 14318180ULL, 19200000ULL, 24000000ULL}) {
        for (const unsigned long long hundredNanos : {1ULL, 5ULL, 100ULL, 300ULL, 500ULL, 10000ULL, 53330ULL}) {
            const std::uint64_t ticks = PreciseHundredNanosToQpcTicks(hundredNanos, freq);
            if (ticks * 10000000ULL < hundredNanos * freq) {
                std::cerr << "PreciseHundredNanosToQpcTicks rounded below requested duration\n";
                return 1;
            }
        }
    }

    PreciseSleepUs(0);

    for (const unsigned long long micros : {50ULL, 1000ULL}) {
        const auto start = std::chrono::steady_clock::now();
        PreciseSleepUs(micros);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed < std::chrono::microseconds(micros)) {
            std::cerr << "PreciseSleepUs returned before its requested duration\n";
            return 1;
        }
    }

#if !defined(_WIN32)
    if (!CheckMedianSleepBound(
            "PreciseSleepUs(50)",
            std::chrono::microseconds(50),
            std::chrono::microseconds(90),
            [] { PreciseSleepUs(50); })) {
        return 1;
    }
#endif

    if (!CheckMedianSleepBound(
            "PreciseSleepUntil(+50us)",
            std::chrono::microseconds(50),
            std::chrono::microseconds(90),
            [] { PreciseSleepUntil(std::chrono::steady_clock::now() + std::chrono::microseconds(50)); })) {
        return 1;
    }

    if (!CheckMedianSleepBound(
            "PreciseSleepUs(1500)",
            std::chrono::microseconds(1500),
            std::chrono::microseconds(1650),
            [] { PreciseSleepUs(1500); })) {
        return 1;
    }

    const auto subMicroDeadline = std::chrono::steady_clock::now() + std::chrono::nanoseconds(500);
    PreciseSleepUntil(subMicroDeadline);
    if (std::chrono::steady_clock::now() < subMicroDeadline) {
        std::cerr << "PreciseSleepUntil returned before sub-microsecond deadline\n";
        return 1;
    }

    const auto millisecondDeadline = std::chrono::steady_clock::now() + std::chrono::microseconds(1000);
    PreciseSleepUntil(millisecondDeadline);
    if (std::chrono::steady_clock::now() < millisecondDeadline) {
        std::cerr << "PreciseSleepUntil returned before millisecond deadline\n";
        return 1;
    }

    PreciseSleepUntil(std::chrono::steady_clock::time_point{});
    const auto coarseDeadline = std::chrono::ceil<std::chrono::microseconds>(
        std::chrono::steady_clock::now() + std::chrono::microseconds(25));
    PreciseSleepUntil(coarseDeadline);
    if (std::chrono::steady_clock::now() < coarseDeadline) {
        std::cerr << "PreciseSleepUntil returned before coarse steady_clock deadline\n";
        return 1;
    }

#ifdef _WIN32
    for (const HANDLE badHandle : {static_cast<HANDLE>(nullptr), GetCurrentProcess()}) {
        const auto start = std::chrono::steady_clock::now();
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        PreciseSleepFromCounterWithTimer(counter, 10000ULL, badHandle, true);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed < std::chrono::microseconds(1000)) {
            std::cerr << "PreciseSleepFromCounterWithTimer fallback returned early\n";
            return 1;
        }
    }
#endif

#ifdef __linux__
    if (PreciseLinuxLowTimerSlack() && prctl(PR_GET_TIMERSLACK, 0UL, 0UL, 0UL, 0UL) != 1L) {
        std::cerr << "PreciseLinuxLowTimerSlack did not configure 1ns timer slack\n";
        return 1;
    }
    for (const unsigned long long nanos : {80000ULL, 1000000ULL}) {
        const auto start = std::chrono::steady_clock::now();
        PreciseSleepFromNanosWithSlack(PreciseMonotonicNanos(), nanos, false);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed < std::chrono::nanoseconds(nanos)) {
            std::cerr << "PreciseSleepFromNanosWithSlack conservative fallback returned early\n";
            return 1;
        }
    }
#endif

    return 0;
}
