#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static constexpr int SCE_OK = 0;
static constexpr int GUEST_EPERM = 1;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EACCES = 13;

static constexpr int GUEST_CLOCK_REALTIME = 0;
static constexpr int GUEST_CLOCK_VIRTUAL = 1;
static constexpr int GUEST_CLOCK_PROF = 2;
static constexpr int GUEST_CLOCK_MONOTONIC = 4;
static constexpr int GUEST_CLOCK_UPTIME = 5;
static constexpr int GUEST_CLOCK_UPTIME_PRECISE = 7;
static constexpr int GUEST_CLOCK_UPTIME_FAST = 8;
static constexpr int GUEST_CLOCK_REALTIME_PRECISE = 9;
static constexpr int GUEST_CLOCK_REALTIME_FAST = 10;
static constexpr int GUEST_CLOCK_MONOTONIC_PRECISE = 11;
static constexpr int GUEST_CLOCK_MONOTONIC_FAST = 12;
static constexpr int GUEST_CLOCK_SECOND = 13;
static constexpr int GUEST_CLOCK_THREAD_CPUTIME_ID = 14;
static constexpr int GUEST_CLOCK_PROCESS_CPUTIME_ID = 15;

static constexpr std::int64_t NANOS_PER_SECOND = 1000000000LL;

static void Require(bool value, const std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Require failed at line %u (errno %d)\n", static_cast<unsigned>(location.line()), *__error_nid_postfix());
        std::fflush(stderr);
        std::abort();
    }
}

static void TestClockSettimeNullAndFaultHandling() {
    const int allClocks[] = {
        GUEST_CLOCK_REALTIME,
        GUEST_CLOCK_VIRTUAL,
        GUEST_CLOCK_PROF,
        3,
        GUEST_CLOCK_MONOTONIC,
        GUEST_CLOCK_UPTIME,
        6,
        GUEST_CLOCK_UPTIME_PRECISE,
        GUEST_CLOCK_UPTIME_FAST,
        GUEST_CLOCK_REALTIME_PRECISE,
        GUEST_CLOCK_REALTIME_FAST,
        GUEST_CLOCK_MONOTONIC_PRECISE,
        GUEST_CLOCK_MONOTONIC_FAST,
        GUEST_CLOCK_SECOND,
        GUEST_CLOCK_THREAD_CPUTIME_ID,
        GUEST_CLOCK_PROCESS_CPUTIME_ID,
        16,
        21,
        31,
        -1,
        -100,
        32,
        100,
        1000
    };

    for (const int clockId : allClocks) {
        *__error_nid_postfix() = GUEST_EACCES;
        Require(clock_settime_nid_postfix(clockId, nullptr) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);

        for (const std::uintptr_t badAddress : {std::uintptr_t{1}, std::uintptr_t{0x10}}) {
            const auto* badTp = reinterpret_cast<const KernelTimespec*>(badAddress);
            *__error_nid_postfix() = GUEST_EACCES;
            Require(clock_settime_nid_postfix(clockId, badTp) == -1);
            Require(*__error_nid_postfix() == GUEST_EFAULT);
        }
    }
}

static void TestClockSettimeNegativeAndInvalidTimespecBounds() {
    const KernelTimespec invalidTimes[] = {
        {0, -1},
        {0, -500},
        {0, -1000000000LL},
        {INT64_MIN, -1},
        {-1, -1},
        {-10, -500},
        {0, NANOS_PER_SECOND},
        {0, NANOS_PER_SECOND + 1},
        {0, 2 * NANOS_PER_SECOND},
        {0, INT64_MAX},
        {100, NANOS_PER_SECOND},
        {-1, 0},
        {-100, 500},
        {INT64_MIN, 0}
    };

    const int testClocks[] = {
        GUEST_CLOCK_REALTIME,
        GUEST_CLOCK_MONOTONIC,
        GUEST_CLOCK_SECOND,
        GUEST_CLOCK_THREAD_CPUTIME_ID,
        GUEST_CLOCK_PROCESS_CPUTIME_ID,
        -1,
        16,
        32
    };

    for (const int clockId : testClocks) {
        for (const auto& invalid : invalidTimes) {
            *__error_nid_postfix() = GUEST_EACCES;
            Require(clock_settime_nid_postfix(clockId, &invalid) == -1);
            Require(*__error_nid_postfix() == GUEST_EPERM);
        }
    }
}

static void TestClockSettimeValidTimespecRefusal() {
    const KernelTimespec validTimes[] = {
        {1767225600LL, 5},
        {1767225600LL, 0},
        {1767225600LL, 999999999LL},
        {1000LL, 500000LL},
        {0, 0},
        {1, 1}
    };

    const int allClocks[] = {
        GUEST_CLOCK_REALTIME,
        GUEST_CLOCK_VIRTUAL,
        GUEST_CLOCK_PROF,
        3,
        GUEST_CLOCK_MONOTONIC,
        GUEST_CLOCK_UPTIME,
        6,
        GUEST_CLOCK_UPTIME_PRECISE,
        GUEST_CLOCK_UPTIME_FAST,
        GUEST_CLOCK_REALTIME_PRECISE,
        GUEST_CLOCK_REALTIME_FAST,
        GUEST_CLOCK_MONOTONIC_PRECISE,
        GUEST_CLOCK_MONOTONIC_FAST,
        GUEST_CLOCK_SECOND,
        GUEST_CLOCK_THREAD_CPUTIME_ID,
        GUEST_CLOCK_PROCESS_CPUTIME_ID,
        16,
        21,
        31,
        -1,
        -100,
        32,
        100
    };

    for (const int clockId : allClocks) {
        for (const auto& valid : validTimes) {
            *__error_nid_postfix() = GUEST_EACCES;
            Require(clock_settime_nid_postfix(clockId, &valid) == -1);
            Require(*__error_nid_postfix() == GUEST_EPERM);
        }
    }
}

static void TestClockImmutabilityAfterSettimeAttempts() {
    KernelTimespec rtBefore{};
    Require(clock_gettime_nid_postfix(GUEST_CLOCK_REALTIME, &rtBefore) == SCE_OK);
    Require(rtBefore.tv_sec > 0);

    KernelTimespec monoBefore{};
    Require(clock_gettime_nid_postfix(GUEST_CLOCK_MONOTONIC, &monoBefore) == SCE_OK);
    Require(monoBefore.tv_sec >= 0);

    const KernelTimespec futureTime{2000000000LL, 123456789LL};
    *__error_nid_postfix() = GUEST_EACCES;
    Require(clock_settime_nid_postfix(GUEST_CLOCK_REALTIME, &futureTime) == -1);
    Require(*__error_nid_postfix() == GUEST_EPERM);

    KernelTimespec rtAfter{};
    Require(clock_gettime_nid_postfix(GUEST_CLOCK_REALTIME, &rtAfter) == SCE_OK);
    Require(rtAfter.tv_sec != futureTime.tv_sec);
    Require(rtAfter.tv_sec >= rtBefore.tv_sec);

    KernelTimespec monoAfter{};
    Require(clock_gettime_nid_postfix(GUEST_CLOCK_MONOTONIC, &monoAfter) == SCE_OK);
    Require(monoAfter.tv_sec > monoBefore.tv_sec ||
            (monoAfter.tv_sec == monoBefore.tv_sec && monoAfter.tv_nsec >= monoBefore.tv_nsec));

    KernelTimespec sceNow{};
    Require(sceKernelClockGettime(GUEST_CLOCK_REALTIME, &sceNow) == SCE_OK);
    Require(sceNow.tv_sec != futureTime.tv_sec);
    Require(sceNow.tv_sec >= rtBefore.tv_sec);
}

int main() {
    TestClockSettimeNullAndFaultHandling();
    TestClockSettimeNegativeAndInvalidTimespecBounds();
    TestClockSettimeValidTimespecRefusal();
    TestClockImmutabilityAfterSettimeAttempts();
    return 0;
}
