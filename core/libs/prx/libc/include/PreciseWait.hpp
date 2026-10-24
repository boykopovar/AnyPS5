#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_PRECISEWAIT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_PRECISEWAIT_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <type_traits>

inline constexpr std::uint64_t PreciseHundredNanosToQpcTicks(unsigned long long hundredNanos, std::uint64_t freq) {
    if (freq == 10000000ULL) [[likely]] {
        return hundredNanos;
    }
    const std::uint64_t wholeSeconds = hundredNanos / 10000000ULL;
    const std::uint64_t remHundredNanos = hundredNanos % 10000000ULL;
    return wholeSeconds * freq + (remHundredNanos * freq + 9999999ULL) / 10000000ULL;
}

inline constexpr std::uint64_t PrecisePauseThresholdTicks(std::uint64_t freq) {
    return PreciseHundredNanosToQpcTicks(2ULL, freq);
}

#ifdef _WIN32
#include <windows.h>
#include "prx/libc/include/HostThreadLocal.hpp"

inline void PreciseSleepWaitableTimerWithHandle(HANDLE handle, unsigned long long sleepHundredNanos) {
    bool slept = false;
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE) [[likely]] {
        LARGE_INTEGER due;
        due.QuadPart = -static_cast<LONGLONG>(sleepHundredNanos);
        if (SetWaitableTimer(handle, &due, 0, nullptr, nullptr, FALSE) != FALSE) [[likely]] {
            slept = (WaitForSingleObject(handle, INFINITE) == WAIT_OBJECT_0);
        }
    }
    if (!slept) [[unlikely]] {
        std::this_thread::sleep_for(std::chrono::nanoseconds(sleepHundredNanos * 100ULL));
    }
}

inline HANDLE PreciseWaitThreadTimer() {
    struct Timer {
        HANDLE handle = CreateWaitableTimerExW(nullptr, nullptr,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        ~Timer() {
            if (handle != nullptr) {
                CloseHandle(handle);
            }
        }
    };
    return HostThreadLocal<Timer, Timer>().handle;
}

inline std::uint64_t PreciseQpcFrequency() {
    static std::atomic<std::uint64_t> cachedFreq{0};
    std::uint64_t freq = cachedFreq.load(std::memory_order_relaxed);
    if (freq == 0) [[unlikely]] {
        LARGE_INTEGER f;
        if (QueryPerformanceFrequency(&f) && f.QuadPart > 0) {
            freq = static_cast<std::uint64_t>(f.QuadPart);
            cachedFreq.store(freq, std::memory_order_relaxed);
        }
    }
    return freq;
}

inline void PreciseSleepFromCounterWithTimer(
    LARGE_INTEGER counter,
    unsigned long long hundredNanos,
    HANDLE timerHandle,
    bool useCustomTimer,
    bool allowSpinTail = true) {
    if (hundredNanos == 0) {
        return;
    }
    constexpr unsigned long long kSpinTailHundredNanos = 2500;
    constexpr unsigned long long kMinTimerSpinSleepHundredNanos = 10000;
    const std::uint64_t freq = PreciseQpcFrequency();
    std::uint64_t deltaTicks = hundredNanos;
    std::uint64_t pauseThresholdTicks = 2ULL;
    if (freq != 10000000ULL) [[unlikely]] {
        deltaTicks = PreciseHundredNanosToQpcTicks(hundredNanos, freq);
        pauseThresholdTicks = PrecisePauseThresholdTicks(freq);
    }
    const std::uint64_t deadlineTicks = static_cast<std::uint64_t>(counter.QuadPart) + deltaTicks;
    const bool useSpinTail = allowSpinTail || hundredNanos >= kMinTimerSpinSleepHundredNanos;
    if (!useSpinTail || hundredNanos > kSpinTailHundredNanos) [[unlikely]] {
        const HANDLE handle = useCustomTimer ? timerHandle : PreciseWaitThreadTimer();
        const unsigned long long timerHundredNanos = useSpinTail ? (hundredNanos - kSpinTailHundredNanos) : hundredNanos;
        PreciseSleepWaitableTimerWithHandle(handle, timerHundredNanos);
    }
    for (;;) {
        QueryPerformanceCounter(&counter);
        const std::uint64_t nowTicks = static_cast<std::uint64_t>(counter.QuadPart);
        if (nowTicks >= deadlineTicks) {
            break;
        }
        if (deadlineTicks - nowTicks > pauseThresholdTicks) {
            YieldProcessor();
        }
    }
}

inline void PreciseSleepFromCounter(LARGE_INTEGER counter, unsigned long long hundredNanos) {
    PreciseSleepFromCounterWithTimer(counter, hundredNanos, nullptr, false, true);
}

inline void PreciseSleepHundredNanos(unsigned long long hundredNanos) {
    if (hundredNanos == 0) {
        return;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    PreciseSleepFromCounterWithTimer(counter, hundredNanos, nullptr, false, false);
}

inline constexpr std::uint64_t PreciseTicksToHundredNanos24MHz(std::uint64_t ticks) {
    const std::uint64_t seconds = ticks / 24000000ULL;
    const std::uint64_t remTicks = ticks % 24000000ULL;
    return seconds * 10000000ULL + (remTicks * 5ULL) / 12ULL;
}

inline constexpr std::uint64_t PreciseFractionalProductDiv(
    std::uint64_t value,
    std::uint64_t multiplier,
    std::uint64_t divisor,
    std::uint64_t& remainder) {
    std::uint64_t quotient = 0;
    remainder = 0;
    for (int bit = 63; bit >= 0; --bit) {
        quotient *= 2ULL;
        if (remainder >= divisor - remainder) {
            remainder -= divisor - remainder;
            ++quotient;
        } else {
            remainder += remainder;
        }
        if (((multiplier >> bit) & 1ULL) != 0ULL) {
            if (remainder >= divisor - value) {
                remainder -= divisor - value;
                ++quotient;
            } else {
                remainder += value;
            }
        }
    }
    return quotient;
}

inline void PreciseSleepUntilTicks24MHz(LARGE_INTEGER counter, std::uint64_t targetTicks) {
    constexpr std::uint64_t kSpinTailTicks = 6000ULL;
    constexpr std::uint64_t kPauseThresholdTicks = 5ULL;
    const std::uint64_t nowTicks = static_cast<std::uint64_t>(counter.QuadPart);
    if (nowTicks >= targetTicks) {
        return;
    }
    const std::uint64_t remainingTicks = targetTicks - nowTicks;
    if (remainingTicks > kSpinTailTicks) [[unlikely]] {
        const std::uint64_t timerTicks = remainingTicks - kSpinTailTicks;
        const unsigned long long timerHundredNanos = static_cast<unsigned long long>(
            PreciseTicksToHundredNanos24MHz(timerTicks));
        if (timerHundredNanos > 0) {
            PreciseSleepWaitableTimerWithHandle(PreciseWaitThreadTimer(), timerHundredNanos);
        }
    }
    for (;;) {
        QueryPerformanceCounter(&counter);
        const std::uint64_t currentTicks = static_cast<std::uint64_t>(counter.QuadPart);
        if (currentTicks >= targetTicks) {
            break;
        }
        if (targetTicks - currentTicks > kPauseThresholdTicks) {
            YieldProcessor();
        }
    }
}

template <class TDuration>
inline constexpr std::uint64_t PreciseDurationToTicks24MHz(TDuration duration) {
    const auto raw = duration.count();
    if (raw <= 0) [[unlikely]] {
        return 0;
    }
    using ScaleRatio = std::ratio_divide<typename TDuration::period, std::ratio<1, 24000000>>;
    constexpr std::uint64_t kNum = static_cast<std::uint64_t>(ScaleRatio::num);
    constexpr std::uint64_t kDen = static_cast<std::uint64_t>(ScaleRatio::den);
    const std::uint64_t uVal = static_cast<std::uint64_t>(raw);
    if constexpr (kDen == 1ULL) {
        if (uVal > UINT64_MAX / kNum) [[unlikely]] {
            return UINT64_MAX;
        }
        return uVal * kNum;
    } else {
        const std::uint64_t q = uVal / kDen;
        const std::uint64_t r = uVal % kDen;
        std::uint64_t rem = 0;
        if (r <= (UINT64_MAX - (kDen - 1ULL)) / kNum) {
            rem = (r * kNum + (kDen - 1ULL)) / kDen;
        } else {
            std::uint64_t fractionalRemainder = 0;
            rem = PreciseFractionalProductDiv(r, kNum, kDen, fractionalRemainder);
            if (fractionalRemainder != 0ULL) {
                ++rem;
            }
        }
        if (q > (UINT64_MAX - rem) / kNum) [[unlikely]] {
            return UINT64_MAX;
        }
        return q * kNum + rem;
    }
}
#elif !defined(__EMSCRIPTEN__)
#include <cerrno>
#include <time.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#endif

inline void PreciseSpinPause() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    _mm_pause();
#else
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

inline std::uint64_t PreciseMonotonicNanos() {
    struct timespec ts;
#if defined(__APPLE__) && defined(_LIBCPP_VERSION) && defined(CLOCK_MONOTONIC_RAW)
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts.tv_nsec);
}

#ifdef __linux__
inline bool PreciseLinuxLowTimerSlack() {
    static thread_local const bool lowSlack = [] {
        const long current = prctl(PR_GET_TIMERSLACK, 0UL, 0UL, 0UL, 0UL);
        return current == 1L || prctl(PR_SET_TIMERSLACK, 1UL, 0UL, 0UL, 0UL) == 0;
    }();
    return lowSlack;
}
#endif

inline void PreciseSleepFromNanosWithSlack(std::uint64_t startNanos, unsigned long long nanos, bool allowLowSlack) {
    if (nanos == 0) {
        return;
    }
    const std::uint64_t deadlineNanos = startNanos + nanos;
#ifdef __linux__
    const bool lowSlack = allowLowSlack && PreciseLinuxLowTimerSlack();
    const unsigned long long spinTailNanos = lowSlack ? 15000ULL : 100000ULL;
    if (nanos > spinTailNanos) [[unlikely]] {
        const std::uint64_t wakeNanos = deadlineNanos - spinTailNanos;
        struct timespec req;
        req.tv_sec = static_cast<time_t>(wakeNanos / 1000000000ULL);
        req.tv_nsec = static_cast<long>(wakeNanos % 1000000000ULL);
        while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &req, nullptr) == EINTR) {
        }
    }
#else
    (void)allowLowSlack;
    constexpr unsigned long long kSpinTailNanos = 100000ULL;
    if (nanos > kSpinTailNanos) [[unlikely]] {
        const unsigned long long sleepNanos = nanos - kSpinTailNanos;
        struct timespec req;
        req.tv_sec = static_cast<time_t>(sleepNanos / 1000000000ULL);
        req.tv_nsec = static_cast<long>(sleepNanos % 1000000000ULL);
        while (nanosleep(&req, &req) == -1 && errno == EINTR) {
        }
    }
#endif
    for (;;) {
        const std::uint64_t nowNanos = PreciseMonotonicNanos();
        if (nowNanos >= deadlineNanos) {
            break;
        }
        if (deadlineNanos - nowNanos > 200ULL) {
            PreciseSpinPause();
        }
    }
}

inline void PreciseSleepFromNanos(std::uint64_t startNanos, unsigned long long nanos) {
    PreciseSleepFromNanosWithSlack(startNanos, nanos, true);
}

inline void PreciseSleepNanos(unsigned long long nanos) {
    if (nanos == 0) {
        return;
    }
    PreciseSleepFromNanos(PreciseMonotonicNanos(), nanos);
}
#endif

inline constexpr unsigned long long kSubTickWaitUs = 2000;

inline void PreciseSleepUs(unsigned long long micros) {
    if (micros == 0) {
        return;
    }
#ifdef _WIN32
    PreciseSleepHundredNanos(micros * 10ULL);
    return;
#elif !defined(__EMSCRIPTEN__)
    PreciseSleepNanos(micros * 1000ULL);
    return;
#else
    std::this_thread::sleep_for(std::chrono::microseconds(micros));
#endif
}

template <class TClock, class TDuration>
inline void PreciseSleepUntil(const std::chrono::time_point<TClock, TDuration>& deadline) {
#ifdef _WIN32
    if constexpr (std::is_same_v<TClock, std::chrono::steady_clock>) {
        const std::uint64_t freq = PreciseQpcFrequency();
        if (freq == 10000000ULL) [[likely]] {
            const auto rawDeadline = deadline.time_since_epoch().count();
            if (rawDeadline <= 0) [[unlikely]] {
                return;
            }
            using HundredNanos = std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>;
            std::uint64_t targetHns = 0;
            if constexpr (std::is_same_v<typename TDuration::period, std::nano>) {
                targetHns = (static_cast<std::uint64_t>(rawDeadline) + 99ULL) / 100ULL;
            } else {
                targetHns = static_cast<std::uint64_t>(
                    std::chrono::ceil<HundredNanos>(deadline.time_since_epoch()).count());
            }
            LARGE_INTEGER counter;
            QueryPerformanceCounter(&counter);
            const std::uint64_t nowTicks = static_cast<std::uint64_t>(counter.QuadPart);
            if (nowTicks < targetHns) {
                PreciseSleepFromCounter(counter, targetHns - nowTicks);
            }
            return;
        } else if (freq == 24000000ULL) {
            const auto rawDeadline = deadline.time_since_epoch().count();
            if (rawDeadline <= 0) [[unlikely]] {
                return;
            }
            const std::uint64_t targetTicks = PreciseDurationToTicks24MHz(deadline.time_since_epoch());
            LARGE_INTEGER counter;
            QueryPerformanceCounter(&counter);
            PreciseSleepUntilTicks24MHz(counter, targetTicks);
            return;
        }
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    const auto now = TClock::now();
    if (now < deadline) {
        const auto remainingNs = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - now).count();
        if (remainingNs > 0) {
            PreciseSleepFromCounter(counter, (static_cast<unsigned long long>(remainingNs) + 99ULL) / 100ULL);
        }
    }
    for (auto current = TClock::now(); current < deadline; current = TClock::now()) {
        if (deadline - current > std::chrono::nanoseconds(200)) {
            YieldProcessor();
        }
    }
#elif !defined(__EMSCRIPTEN__)
#if (defined(__linux__) && (defined(__GLIBCXX__) || (defined(_LIBCPP_VERSION) && !defined(_LIBCPP_HAS_TIMESPEC_GET)))) || \
    (defined(__APPLE__) && defined(_LIBCPP_VERSION) && defined(CLOCK_MONOTONIC_RAW))
    if constexpr (std::is_same_v<TClock, std::chrono::steady_clock>) {
        const auto rawDeadline = deadline.time_since_epoch().count();
        if (rawDeadline <= 0) [[unlikely]] {
            return;
        }
        std::uint64_t targetNanos = 0;
        if constexpr (std::is_same_v<typename TDuration::period, std::nano>) {
            targetNanos = static_cast<std::uint64_t>(rawDeadline);
        } else {
            targetNanos = static_cast<std::uint64_t>(
                std::chrono::ceil<std::chrono::nanoseconds>(deadline.time_since_epoch()).count());
        }
        const std::uint64_t startNanos = PreciseMonotonicNanos();
        if (startNanos < targetNanos) {
            PreciseSleepFromNanos(startNanos, targetNanos - startNanos);
        }
        return;
    }
#endif
    const std::uint64_t startNanos = PreciseMonotonicNanos();
    const auto now = TClock::now();
    if (now < deadline) {
        const auto remainingNs = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - now).count();
        if (remainingNs > 0) {
            PreciseSleepFromNanos(startNanos, static_cast<unsigned long long>(remainingNs));
        }
    }
    for (auto current = TClock::now(); current < deadline; current = TClock::now()) {
        if (deadline - current > std::chrono::nanoseconds(200)) {
            PreciseSpinPause();
        }
    }
#else
    for (auto now = TClock::now(); now < deadline; now = TClock::now()) {
        PreciseSleepUs(static_cast<unsigned long long>(std::chrono::ceil<std::chrono::microseconds>(deadline - now).count()));
    }
#endif
}

#endif
