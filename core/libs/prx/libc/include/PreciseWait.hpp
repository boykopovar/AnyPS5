#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_PRECISEWAIT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_PRECISEWAIT_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <thread>
#include <type_traits>

inline constexpr std::uint64_t PreciseHundredNanosToQpcTicks(unsigned long long hundredNanos, std::uint64_t freq) {
    if (hundredNanos == 0 || freq == 0) [[unlikely]] {
        return 0;
    }
    if (freq == 10000000ULL) [[likely]] {
        return static_cast<std::uint64_t>(hundredNanos);
    }
    constexpr std::uint64_t kHundredNanosPerSec = 10000000ULL;
    constexpr std::uint64_t kMaxTicks = (std::numeric_limits<std::uint64_t>::max)();
    const std::uint64_t totalHns = static_cast<std::uint64_t>(hundredNanos);
    const std::uint64_t wholeSeconds = totalHns / kHundredNanosPerSec;
    const std::uint64_t remHns = totalHns % kHundredNanosPerSec;
    if (wholeSeconds > 0 && freq > kMaxTicks / wholeSeconds) [[unlikely]] {
        return kMaxTicks;
    }
    const std::uint64_t wholeTicks = wholeSeconds * freq;
    const std::uint64_t freqWhole = freq / kHundredNanosPerSec;
    const std::uint64_t freqRem = freq % kHundredNanosPerSec;
    const std::uint64_t remTicks = remHns * freqWhole + (remHns * freqRem + kHundredNanosPerSec - 1ULL) / kHundredNanosPerSec;
    if (remTicks > kMaxTicks - wholeTicks) [[unlikely]] {
        return kMaxTicks;
    }
    return wholeTicks + remTicks;
}

inline constexpr std::uint64_t PrecisePauseThresholdTicks(std::uint64_t freq) {
    return PreciseHundredNanosToQpcTicks(2ULL, freq);
}

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#endif

inline void PreciseSpinPause() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    _mm_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#else
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

#ifdef _WIN32
#include <windows.h>
#include "prx/libc/include/HostThreadLocal.hpp"

inline void PreciseSleepWaitableTimerWithHandle(HANDLE handle, unsigned long long sleepHundredNanos) {
    bool slept = false;
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE) [[likely]] {
        constexpr unsigned long long kMaxTimerHundredNanos = static_cast<unsigned long long>((std::numeric_limits<LONGLONG>::max)());
        const unsigned long long clampedTimerHns = sleepHundredNanos > kMaxTimerHundredNanos ? kMaxTimerHundredNanos : sleepHundredNanos;
        LARGE_INTEGER due;
        due.QuadPart = -static_cast<LONGLONG>(clampedTimerHns);
        if (SetWaitableTimer(handle, &due, 0, nullptr, nullptr, FALSE) != FALSE) [[likely]] {
            slept = (WaitForSingleObject(handle, INFINITE) == WAIT_OBJECT_0);
        }
    }
    if (!slept) [[unlikely]] {
        constexpr unsigned long long kMaxFallbackHundredNanos =
            static_cast<unsigned long long>((std::chrono::nanoseconds::max)().count()) / 100ULL;
        const unsigned long long clampedFallbackHns =
            sleepHundredNanos > kMaxFallbackHundredNanos ? kMaxFallbackHundredNanos : sleepHundredNanos;
        std::this_thread::sleep_for(std::chrono::nanoseconds(clampedFallbackHns * 100ULL));
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
    constexpr std::uint64_t kMaxTicks = (std::numeric_limits<std::uint64_t>::max)();
    const std::uint64_t startTicks = static_cast<std::uint64_t>(counter.QuadPart);
    const std::uint64_t deadlineTicks = deltaTicks > kMaxTicks - startTicks ? kMaxTicks : startTicks + deltaTicks;
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
#elif !defined(__EMSCRIPTEN__)
#include <cerrno>
#include <time.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
inline std::uint64_t PreciseMonotonicNanos() {
    struct timespec ts;
#if defined(__APPLE__) && defined(_LIBCPP_VERSION) && defined(CLOCK_MONOTONIC_RAW)
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts.tv_nsec);
}

#ifdef __APPLE__
extern "C" bool PreciseKernelTimerSleep_nid_no_patch(std::uint64_t wakeNanos);
#endif

#ifdef __linux__
inline unsigned long long PreciseLinuxSpinTailNanos_nid_no_patch() {
    static thread_local unsigned long long cachedSpinTailNanos = 0;
    unsigned long long spinTailNanos = cachedSpinTailNanos;
    if (spinTailNanos == 0) [[unlikely]] {
        const long current = prctl(PR_GET_TIMERSLACK, 0UL, 0UL, 0UL, 0UL);
        const bool lowSlack = current == 1L || prctl(PR_SET_TIMERSLACK, 1UL, 0UL, 0UL, 0UL) == 0;
        spinTailNanos = lowSlack ? 15000ULL : 100000ULL;
        cachedSpinTailNanos = spinTailNanos;
    }
    return spinTailNanos;
}

inline bool PreciseLinuxLowTimerSlack() {
    return PreciseLinuxSpinTailNanos_nid_no_patch() == 15000ULL;
}

[[gnu::noinline]] inline void PreciseSleepLinuxTimer_nid_no_patch(std::uint64_t wakeNanos) {
    struct timespec req;
    req.tv_sec = static_cast<time_t>(wakeNanos / 1000000000ULL);
    req.tv_nsec = static_cast<long>(wakeNanos % 1000000000ULL);
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &req, nullptr) == EINTR) {
    }
}
#endif

inline void PreciseSleepFromNanosWithSlack(std::uint64_t startNanos, unsigned long long nanos, bool allowLowSlack) {
    if (nanos == 0) {
        return;
    }
    constexpr std::uint64_t kMaxNanos = (std::numeric_limits<std::uint64_t>::max)();
    const std::uint64_t deadlineNanos = nanos > kMaxNanos - startNanos ? kMaxNanos : startNanos + nanos;
#ifdef __linux__
    if (nanos > 15000ULL) [[unlikely]] {
        const unsigned long long spinTailNanos = allowLowSlack ? PreciseLinuxSpinTailNanos_nid_no_patch() : 100000ULL;
        if (nanos > spinTailNanos) {
            PreciseSleepLinuxTimer_nid_no_patch(deadlineNanos - spinTailNanos);
        }
    }
#elif defined(__APPLE__)
    (void)allowLowSlack;
    constexpr unsigned long long kKernelTimerTailNanos = 30000ULL;
    constexpr unsigned long long kSpinTailNanos = 100000ULL;
    constexpr unsigned long long kMaxWakeMarginNanos = 5000000ULL;
    const bool timed = nanos > kKernelTimerTailNanos && PreciseKernelTimerSleep_nid_no_patch(deadlineNanos - kKernelTimerTailNanos);
    if (!timed && nanos > kSpinTailNanos) [[unlikely]] {
        const unsigned long long beforeTail = nanos - kSpinTailNanos;
        const unsigned long long margin = beforeTail * 3ULL / 10ULL;
        const unsigned long long sleepNanos = beforeTail - (margin < kMaxWakeMarginNanos ? margin : kMaxWakeMarginNanos);
        struct timespec req;
        req.tv_sec = static_cast<time_t>(sleepNanos / 1000000000ULL);
        req.tv_nsec = static_cast<long>(sleepNanos % 1000000000ULL);
        while (nanosleep(&req, &req) == -1 && errno == EINTR) {
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
    constexpr unsigned long long kMaxUll = (std::numeric_limits<unsigned long long>::max)();
#ifdef _WIN32
    const unsigned long long hundredNanos = micros > kMaxUll / 10ULL ? kMaxUll : micros * 10ULL;
    PreciseSleepHundredNanos(hundredNanos);
    return;
#elif !defined(__EMSCRIPTEN__)
    const unsigned long long nanos = micros > kMaxUll / 1000ULL ? kMaxUll : micros * 1000ULL;
    PreciseSleepNanos(nanos);
    return;
#else
    std::this_thread::sleep_for(std::chrono::microseconds(micros));
#endif
}

using PreciseHundredNanos = std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>;

#ifndef __SIZEOF_INT128__
inline constexpr std::uint64_t PreciseCeilMulDiv64(std::uint64_t a, std::uint64_t b, std::uint64_t den) {
    constexpr std::uint64_t kMaxU64 = (std::numeric_limits<std::uint64_t>::max)();
    if (a <= (kMaxU64 - 1ULL) / b) {
        return (a * b - 1ULL) / den + 1ULL;
    }
    const std::uint64_t aLo = a & 0xffffffffULL;
    const std::uint64_t aHi = a >> 32;
    const std::uint64_t bLo = b & 0xffffffffULL;
    const std::uint64_t bHi = b >> 32;
    const std::uint64_t p0 = aLo * bLo;
    const std::uint64_t p1 = aLo * bHi;
    const std::uint64_t p2 = aHi * bLo;
    const std::uint64_t p3 = aHi * bHi;
    const std::uint64_t mid = (p0 >> 32) + (p1 & 0xffffffffULL) + (p2 & 0xffffffffULL);
    std::uint64_t lo = (p0 & 0xffffffffULL) | (mid << 32);
    std::uint64_t hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
    if (lo == 0ULL) {
        --hi;
    }
    --lo;
    std::uint64_t q = 0;
    std::uint64_t rem = hi;
    for (int i = 0; i < 64; ++i) {
        const bool carry = (rem >> 63) != 0;
        rem = (rem << 1) | (lo >> 63);
        lo <<= 1;
        q <<= 1;
        if (carry || rem >= den) {
            rem -= den;
            q |= 1ULL;
        }
    }
    return q + 1ULL;
}
#endif

template <class TTick, class TDuration>
inline constexpr std::uint64_t PreciseDeadlineTicks(TDuration deadline) {
    if (!(deadline.count() > 0)) [[unlikely]] {
        return 0;
    }
    if constexpr (std::chrono::treat_as_floating_point_v<typename TDuration::rep>) {
        const auto nanos = std::chrono::duration<long double, std::nano>(deadline);
        constexpr auto nanosLimit = static_cast<long double>((std::chrono::nanoseconds::max)().count()) + 1.0L;
        if (nanosLimit - nanos.count() < 1.0L) [[unlikely]] {
            throw std::overflow_error("precise deadline is out of range");
        }
        if (nanos.count() == 0.0L) {
            return 1;
        }
        const auto roundedNanos = std::chrono::ceil<std::chrono::nanoseconds>(nanos);
        return static_cast<std::uint64_t>(std::chrono::ceil<TTick>(roundedNanos).count());
    } else if constexpr (std::is_same_v<typename TDuration::period, std::nano>) {
        constexpr auto nanosPerTick = std::ratio_divide<typename TTick::period, std::nano>::num;
        const auto nanos = static_cast<std::uint64_t>(deadline.count());
        return (nanos - 1) / nanosPerTick + 1;
    } else {
        using Ratio = std::ratio_divide<typename TDuration::period, typename TTick::period>;
        constexpr std::uint64_t num = static_cast<std::uint64_t>(Ratio::num);
        constexpr std::uint64_t den = static_cast<std::uint64_t>(Ratio::den);
        constexpr std::uint64_t maxTicks = static_cast<std::uint64_t>((TTick::max)().count());
        const std::uint64_t count = static_cast<std::uint64_t>(deadline.count());
#ifdef __SIZEOF_INT128__
        const unsigned __int128 scaled = static_cast<unsigned __int128>(count) * num;
        const unsigned __int128 ceiled = (scaled - 1) / den + 1;
        if (ceiled > maxTicks) [[unlikely]] {
            throw std::overflow_error("precise deadline is out of range");
        }
        return static_cast<std::uint64_t>(ceiled);
#else
        const std::uint64_t qCount = count / den;
        const std::uint64_t rCount = count % den;
        if (qCount > 0 && num > maxTicks / qCount) [[unlikely]] {
            throw std::overflow_error("precise deadline is out of range");
        }
        const std::uint64_t wholeTicks = qCount * num;
        std::uint64_t remTicks = 0;
        if (rCount > 0) {
            const std::uint64_t qNum = num / den;
            const std::uint64_t rNum = num % den;
            const std::uint64_t remWhole = rCount * qNum;
            const std::uint64_t remFrac = rNum > 0 ? PreciseCeilMulDiv64(rCount, rNum, den) : 0ULL;
            if (remWhole > maxTicks || remFrac > maxTicks - remWhole) [[unlikely]] {
                throw std::overflow_error("precise deadline is out of range");
            }
            remTicks = remWhole + remFrac;
        }
        if (remTicks > maxTicks - wholeTicks) [[unlikely]] {
            throw std::overflow_error("precise deadline is out of range");
        }
        return wholeTicks + remTicks;
#endif
    }
}

template <class TClock, class TDuration>
inline void PreciseSleepUntil(const std::chrono::time_point<TClock, TDuration>& deadline) {
#ifdef _WIN32
    if constexpr (std::is_same_v<TClock, std::chrono::steady_clock>) {
        if (PreciseQpcFrequency() == 10000000ULL) [[likely]] {
            const auto targetHns = PreciseDeadlineTicks<PreciseHundredNanos>(deadline.time_since_epoch());
            if (targetHns == 0) [[unlikely]] {
                return;
            }
            LARGE_INTEGER counter;
            QueryPerformanceCounter(&counter);
            const std::uint64_t nowTicks = static_cast<std::uint64_t>(counter.QuadPart);
            if (nowTicks < targetHns) {
                PreciseSleepFromCounter(counter, targetHns - nowTicks);
            }
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
        const auto targetNanos = PreciseDeadlineTicks<std::chrono::nanoseconds>(deadline.time_since_epoch());
        if (targetNanos == 0) [[unlikely]] {
            return;
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
