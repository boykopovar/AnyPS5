#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int GuestEperm = 1;
constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;

constexpr int ClockRealtime = 0;
constexpr int ClockMonotonic = 4;
constexpr int ClockUptime = 5;
constexpr int ClockSecond = 13;

constexpr int TimerAbstime = 1;

constexpr std::int64_t OneBillion = 1000000000LL;
constexpr std::int64_t OneMillion = 1000000LL;

const GuestSigevent SigevNone{0, 0, 0, {}};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "guest_clock_timer_nanosleep_bounds check failed at line %u\n", location.line());
        std::abort();
    }
}

int GetError() {
    return *__error_nid_postfix();
}

void TestSystemClockAudit() {
    KernelTimespec validTs{123, 456};
    *__error_nid_postfix() = 0;
    Require(clock_settime_nid_postfix(ClockRealtime, nullptr) == -1);
    Require(GetError() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(clock_settime_nid_postfix(ClockMonotonic, nullptr) == -1);
    Require(GetError() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(clock_settime_nid_postfix(-1, nullptr) == -1);
    Require(GetError() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(clock_settime_nid_postfix(ClockRealtime, &validTs) == -1);
    Require(GetError() == GuestEperm);

    *__error_nid_postfix() = 0;
    Require(clock_settime_nid_postfix(ClockMonotonic, &validTs) == -1);
    Require(GetError() == GuestEperm);

    *__error_nid_postfix() = 0;
    Require(clock_settime_nid_postfix(999, &validTs) == -1);
    Require(GetError() == GuestEperm);

    KernelTimespec readTs{-1, -1};
    Require(clock_gettime_nid_postfix(ClockRealtime, &readTs) == 0);
    Require(readTs.tv_sec >= 0);
    Require(readTs.tv_nsec >= 0 && readTs.tv_nsec < OneBillion);

    Require(clock_gettime_nid_postfix(ClockMonotonic, &readTs) == 0);
    Require(readTs.tv_sec >= 0);
    Require(readTs.tv_nsec >= 0 && readTs.tv_nsec < OneBillion);

    Require(clock_gettime_nid_postfix(ClockUptime, &readTs) == 0);
    Require(readTs.tv_sec >= 0);
    Require(readTs.tv_nsec >= 0 && readTs.tv_nsec < OneBillion);

    Require(clock_gettime_nid_postfix(ClockSecond, &readTs) == 0);
    Require(readTs.tv_sec >= 0);
    Require(readTs.tv_nsec >= 0 && readTs.tv_nsec < OneBillion);

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(ClockRealtime, nullptr) == -1);
    Require(GetError() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(ClockMonotonic, nullptr) == -1);
    Require(GetError() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(-1, &readTs) == -1);
    Require(GetError() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(32, &readTs) == -1);
    Require(GetError() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(-1, nullptr) == -1);
    Require(GetError() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(32, nullptr) == -1);
    Require(GetError() == GuestEinval);

    Require(sceKernelClockGettime(ClockRealtime, &readTs) == 0);
    Require(readTs.tv_sec >= 0);
    Require(readTs.tv_nsec >= 0 && readTs.tv_nsec < OneBillion);

    Require(sceKernelClockGettime(ClockRealtime, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelClockGettime(ClockMonotonic, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelClockGettime(-1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelClockGettime(-1, &readTs) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelClockGettime(32, &readTs) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelClockGetres(ClockRealtime, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelClockGetres(ClockMonotonic, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    KernelTimespec resTs{-1, -1};
    Require(sceKernelClockGetres(ClockRealtime, &resTs) == 0);
    Require(resTs.tv_sec >= 0);
    Require(resTs.tv_nsec >= 0 && resTs.tv_nsec < OneBillion);

    Require(sceKernelClockGetres(ClockSecond, &resTs) == 0);
    Require(resTs.tv_sec == 1);
    Require(resTs.tv_nsec == 0);
}

void TestTimerAudit() {
    int timerId = -1;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &SigevNone, &timerId) == 0);
    Require(timerId >= 3 && timerId < 32);

    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(timerId, 0, nullptr, nullptr) == -1);
    Require(GetError() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(-1, 0, nullptr, nullptr) == -1);
    Require(GetError() == GuestEfault);

    GuestItimerspec validSpec{{0, 0}, {0, 0}};
    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(-1, 0, &validSpec, nullptr) == -1);
    Require(GetError() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(0, 0, &validSpec, nullptr) == -1);
    Require(GetError() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(32, 0, &validSpec, nullptr) == -1);
    Require(GetError() == GuestEinval);

    const GuestItimerspec negativeValueSec{{0, 0}, {-1, 0}};
    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(timerId, 0, &negativeValueSec, nullptr) == -1);
    Require(GetError() == GuestEinval);

    const GuestItimerspec negativeValueNsec{{0, 0}, {0, -1}};
    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(timerId, 0, &negativeValueNsec, nullptr) == -1);
    Require(GetError() == GuestEinval);

    const GuestItimerspec billionValueNsec{{0, 0}, {0, OneBillion}};
    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(timerId, 0, &billionValueNsec, nullptr) == -1);
    Require(GetError() == GuestEinval);

    const GuestItimerspec overflowValueNsec{{0, 0}, {0, OneBillion + 500}};
    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(timerId, 0, &overflowValueNsec, nullptr) == -1);
    Require(GetError() == GuestEinval);

    const GuestItimerspec negativeIntervalSec{{-1, 0}, {1, 0}};
    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(timerId, 0, &negativeIntervalSec, nullptr) == -1);
    Require(GetError() == GuestEinval);

    const GuestItimerspec negativeIntervalNsec{{0, -1}, {1, 0}};
    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(timerId, 0, &negativeIntervalNsec, nullptr) == -1);
    Require(GetError() == GuestEinval);

    const GuestItimerspec billionIntervalNsec{{0, OneBillion}, {1, 0}};
    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(timerId, 0, &billionIntervalNsec, nullptr) == -1);
    Require(GetError() == GuestEinval);

    const GuestItimerspec maxNsecBoundary{{0, OneBillion - 1}, {1, OneBillion - 1}};
    Require(ktimer_settime_nid_postfix(timerId, 0, &maxNsecBoundary, nullptr) == 0);

    const GuestItimerspec subMillisecondClamping{{0, 100}, {10, 0}};
    Require(ktimer_settime_nid_postfix(timerId, 0, &subMillisecondClamping, nullptr) == 0);
    GuestItimerspec checkSpec{{-1, -1}, {-1, -1}};
    Require(ktimer_gettime_nid_postfix(timerId, &checkSpec) == 0);
    Require(checkSpec.it_interval.tv_sec == 0);
    Require(checkSpec.it_interval.tv_nsec == OneMillion);

    GuestItimerspec oldSpec{{-1, -1}, {-1, -1}};
    const GuestItimerspec disarm{{0, 0}, {0, 0}};
    Require(ktimer_settime_nid_postfix(timerId, 0, &disarm, &oldSpec) == 0);
    Require(oldSpec.it_interval.tv_nsec == OneMillion);
    Require(oldSpec.it_value.tv_sec > 0);

    KernelTimespec nowMonotonic{};
    Require(clock_gettime_nid_postfix(ClockMonotonic, &nowMonotonic) == 0);
    const GuestItimerspec futureAbsolute{{0, 0}, {nowMonotonic.tv_sec + 60, nowMonotonic.tv_nsec}};
    Require(ktimer_settime_nid_postfix(timerId, TimerAbstime, &futureAbsolute, nullptr) == 0);
    Require(ktimer_gettime_nid_postfix(timerId, &checkSpec) == 0);
    Require(checkSpec.it_value.tv_sec >= 30 && checkSpec.it_value.tv_sec <= 60);

    *__error_nid_postfix() = 0;
    Require(ktimer_gettime_nid_postfix(timerId, nullptr) == -1);
    Require(GetError() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(ktimer_gettime_nid_postfix(-1, &checkSpec) == -1);
    Require(GetError() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(ktimer_gettime_nid_postfix(32, &checkSpec) == -1);
    Require(GetError() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(ktimer_gettime_nid_postfix(-1, nullptr) == -1);
    Require(GetError() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(ktimer_gettime_nid_postfix(32, nullptr) == -1);
    Require(GetError() == GuestEinval);

    Require(ktimer_getoverrun_nid_postfix(timerId) == 0);
    Require(ktimer_delete_nid_postfix(timerId) == 0);

    *__error_nid_postfix() = 0;
    Require(ktimer_delete_nid_postfix(timerId) == -1);
    Require(GetError() == GuestEinval);
}

void TestNanosleepAudit() {
    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(nullptr, nullptr) == -1);
    Require(GetError() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(nullptr, nullptr) == -1);
    Require(GetError() == GuestEfault);

    Require(sceKernelNanosleep(nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);

    const KernelTimespec negativeNanos{0, -1};
    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&negativeNanos, nullptr) == -1);
    Require(GetError() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(&negativeNanos, nullptr) == -1);
    Require(GetError() == GuestEinval);

    Require(sceKernelNanosleep(&negativeNanos, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    const KernelTimespec billionNanos{0, OneBillion};
    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&billionNanos, nullptr) == -1);
    Require(GetError() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(&billionNanos, nullptr) == -1);
    Require(GetError() == GuestEinval);

    Require(sceKernelNanosleep(&billionNanos, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    const KernelTimespec overflowNanos{0, OneBillion + 1000};
    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&overflowNanos, nullptr) == -1);
    Require(GetError() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(&overflowNanos, nullptr) == -1);
    Require(GetError() == GuestEinval);

    Require(sceKernelNanosleep(&overflowNanos, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    const KernelTimespec negativeSecInvalidNanos{-1, -1};
    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&negativeSecInvalidNanos, nullptr) == -1);
    Require(GetError() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(&negativeSecInvalidNanos, nullptr) == -1);
    Require(GetError() == GuestEinval);

    Require(sceKernelNanosleep(&negativeSecInvalidNanos, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    const KernelTimespec negativeSecBillionNanos{-5, OneBillion};
    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&negativeSecBillionNanos, nullptr) == -1);
    Require(GetError() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(&negativeSecBillionNanos, nullptr) == -1);
    Require(GetError() == GuestEinval);

    Require(sceKernelNanosleep(&negativeSecBillionNanos, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    const KernelTimespec validZero{0, 0};
    KernelTimespec remaining{42, 99};
    Require(nanosleep_nid_postfix(&validZero, &remaining) == 0);
    Require(remaining.tv_sec == 42 && remaining.tv_nsec == 99);

    Require(_nanosleep_nid_postfix(&validZero, &remaining) == 0);
    Require(remaining.tv_sec == 42 && remaining.tv_nsec == 99);

    Require(sceKernelNanosleep(&validZero, &remaining) == 0);
    Require(remaining.tv_sec == 42 && remaining.tv_nsec == 99);

    Require(nanosleep_nid_postfix(&validZero, nullptr) == 0);
    Require(_nanosleep_nid_postfix(&validZero, nullptr) == 0);
    Require(sceKernelNanosleep(&validZero, nullptr) == 0);

    const KernelTimespec negativeSecValidNanos{-1, OneBillion - 1};
    Require(nanosleep_nid_postfix(&negativeSecValidNanos, nullptr) == 0);
    Require(_nanosleep_nid_postfix(&negativeSecValidNanos, nullptr) == 0);
    Require(sceKernelNanosleep(&negativeSecValidNanos, nullptr) == 0);
}

} // namespace

int main() {
    TestSystemClockAudit();
    TestTimerAudit();
    TestNanosleepAudit();
    return 0;
}
