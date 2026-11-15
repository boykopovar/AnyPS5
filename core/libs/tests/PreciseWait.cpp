#include "prx/libc/include/PreciseWait.hpp"
#include <algorithm>
#include <array>
#ifdef __linux__
#include <cstddef>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/syscall.h>
#include <thread>
#endif
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#ifdef __APPLE__
#include <sys/resource.h>
#endif

namespace {

#ifdef __linux__
bool CheckLinuxTimerSlackFailures() {
    for (unsigned mode = 0; mode < 5; ++mode) {
        bool passed = false;
        std::thread worker([&] {
            if (prctl(PR_SET_TIMERSLACK, mode == 3 ? 1UL : 50000UL, 0UL, 0UL, 0UL) != 0) return;
            if (mode == 4 && !PreciseLinuxLowTimerSlack()) return;
            const bool denyRead = mode == 1 || mode == 2 || mode == 4;
            const bool denyWrite = mode != 1;
            sock_filter filter[] = {
                BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
                BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_prctl, 0, 4),
                BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, args[0])),
                BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, PR_GET_TIMERSLACK, 1, 0),
                BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, PR_SET_TIMERSLACK, 0, 1),
                BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
                BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
            };
            filter[3].jt = denyRead ? 1 : 2;
            filter[4].jt = denyWrite ? 0 : 1;
            sock_fprog program{static_cast<unsigned short>(sizeof(filter) / sizeof(filter[0])), filter};
            if (prctl(PR_SET_NO_NEW_PRIVS, 1UL, 0UL, 0UL, 0UL) != 0 ||
                prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program) != 0) return;
            const bool expectedLowSlack = mode == 1 || mode == 3 || mode == 4;
            if (PreciseLinuxLowTimerSlack() != expectedLowSlack ||
                PreciseLinuxLowTimerSlack() != expectedLowSlack) return;
            for (const auto nanos : {1ULL, 15000ULL, 15001ULL, 99999ULL, 100000ULL, 100001ULL, 1000000ULL}) {
                const auto start = PreciseMonotonicNanos();
                PreciseSleepFromNanosWithSlack(start, nanos, true);
                if (PreciseMonotonicNanos() < start + nanos) return;
            }
            if (!denyRead && prctl(PR_GET_TIMERSLACK, 0UL, 0UL, 0UL, 0UL) !=
                (expectedLowSlack ? 1L : 50000L)) return;
            passed = true;
        });
        worker.join();
        if (!passed) {
            std::cerr << "Linux timer slack failure scenario " << mode << " failed\n";
            return false;
        }
    }
    return true;
}

bool CheckLinuxTimerSlackBoundaries() {
    bool passed = false;
    std::thread worker([&] {
        if (prctl(PR_SET_TIMERSLACK, 50000UL, 0UL, 0UL, 0UL) != 0) {
            std::cerr << "Could not initialize timer slack for boundary checks\n";
            return;
        }
        const auto checkWait = [](unsigned long long nanos, bool allowLowSlack, long expectedSlack) {
            const auto start = PreciseMonotonicNanos();
            PreciseSleepFromNanosWithSlack(start, nanos, allowLowSlack);
            if (PreciseMonotonicNanos() < start + nanos) {
                std::cerr << "Linux boundary wait returned before its deadline\n";
                return false;
            }
            if (prctl(PR_GET_TIMERSLACK, 0UL, 0UL, 0UL, 0UL) != expectedSlack) {
                std::cerr << "Linux boundary wait changed timer slack unexpectedly\n";
                return false;
            }
            return true;
        };
        for (const auto nanos : {0ULL, 1ULL, 14999ULL, 15000ULL}) {
            if (!checkWait(nanos, true, 50000L)) return;
        }
        for (const auto nanos : {15001ULL, 99999ULL, 100000ULL, 100001ULL}) {
            if (!checkWait(nanos, false, 50000L)) return;
        }
        for (const auto nanos : {15001ULL, 99999ULL, 100000ULL, 100001ULL}) {
            if (!checkWait(nanos, true, 1L)) return;
        }
        passed = true;
    });
    worker.join();
    return passed;
}
#endif

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
    using Nanos = std::chrono::nanoseconds;
    using FractionalNanos = std::chrono::duration<double, std::nano>;
    static_assert(PreciseDeadlineTicks<PreciseHundredNanos>(FractionalNanos{0.0}) == 0);
    static_assert(PreciseDeadlineTicks<Nanos>(FractionalNanos{-0.5}) == 0);
    static_assert(PreciseDeadlineTicks<Nanos>(FractionalNanos{0.5}) == 1);
    static_assert(PreciseDeadlineTicks<Nanos>(FractionalNanos{1.0}) == 1);
    static_assert(PreciseDeadlineTicks<Nanos>(FractionalNanos{1.5}) == 2);
    static_assert(PreciseDeadlineTicks<Nanos>(FractionalNanos{100.5}) == 101);
    static_assert(PreciseDeadlineTicks<PreciseHundredNanos>(FractionalNanos{99.5}) == 1);
    static_assert(PreciseDeadlineTicks<PreciseHundredNanos>(FractionalNanos{100.0}) == 1);
    static_assert(PreciseDeadlineTicks<PreciseHundredNanos>(FractionalNanos{100.5}) == 2);
    static_assert(PreciseDeadlineTicks<PreciseHundredNanos>(Nanos{101}) == 2);
    static_assert(PreciseDeadlineTicks<Nanos>(FractionalNanos{0x1.fffffffffffffp62}) == (Nanos::max)().count() - 1023);

    if (PreciseDeadlineTicks<PreciseHundredNanos>(FractionalNanos{std::nextafter(100.0, 101.0)}) != 2) {
        std::cerr << "PreciseDeadlineTicks rounded down just above a QPC tick\n";
        return 1;
    }
    try {
        PreciseDeadlineTicks<Nanos>(FractionalNanos{0x1p63});
        std::cerr << "PreciseDeadlineTicks accepted an out-of-range deadline\n";
        return 1;
    } catch (const std::overflow_error&) {
    }

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

#ifdef __linux__
    if (!CheckLinuxTimerSlackBoundaries()) return 1;
    if (!CheckLinuxTimerSlackFailures()) return 1;
#endif

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
    using FractionalDeadline = std::chrono::time_point<std::chrono::steady_clock, FractionalNanos>;
    PreciseSleepUntil(FractionalDeadline{FractionalNanos{-0.5}});
    const FractionalDeadline fractionalDeadline = std::chrono::steady_clock::now() + FractionalNanos{50000.5};
    PreciseSleepUntil(fractionalDeadline);
    if (std::chrono::steady_clock::now() < fractionalDeadline) {
        std::cerr << "PreciseSleepUntil returned before a fractional steady_clock deadline\n";
        return 1;
    }

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

#ifdef __APPLE__
    {
        const auto wake = PreciseMonotonicNanos() + 1000000ULL;
        if (!PreciseKernelTimerSleep_nid_no_patch(wake) || PreciseMonotonicNanos() < wake) {
            std::cerr << "PreciseKernelTimerSleep did not wait for its wake time\n";
            return 1;
        }
    }
    const auto threadCpuNanos = [] {
        struct rusage usage{};
        getrusage(RUSAGE_SELF, &usage);
        return (static_cast<std::uint64_t>(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000000ULL + static_cast<std::uint64_t>(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec)) * 1000ULL;
    };
    std::uint64_t bestCpuNanos = 0;
    for (int attempt = 0; attempt < 3; ++attempt) {
        const auto cpuStart = threadCpuNanos();
        for (int sleep = 0; sleep < 20; ++sleep) PreciseSleepUs(2000);
        const auto cpuNanos = threadCpuNanos() - cpuStart;
        if (attempt == 0 || cpuNanos < bestCpuNanos) bestCpuNanos = cpuNanos;
    }
    if (bestCpuNanos > 3000000ULL && !IsRunnerPreempted()) {
        std::cerr << "PreciseSleepUs(2000) spun for " << bestCpuNanos / 1000ULL << " us of thread CPU time over 20 sleeps\n";
        return 1;
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
