#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int ClockRealtime = 0;
constexpr int ClockMonotonic = 4;
constexpr int ClockUptime = 5;
constexpr int TimerAbstime = 1;
constexpr int SceKernelErrorEbadf = static_cast<int>(0x80020009u);
constexpr int SceKernelErrorEnoent = static_cast<int>(0x80020002u);
constexpr int SceKernelErrorEfault = static_cast<int>(0x8002000Eu);
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016u);
constexpr std::int64_t Second = 1000000000;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestKtimerInvalidDescriptors() {
    const GuestSigevent noSignal{0, 0, 0, {}};
    int validTimerId = -1;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &noSignal, &validTimerId) == 0);
    Require(validTimerId >= 3);
    Require(ktimer_delete_nid_postfix(validTimerId) == 0);

    GuestItimerspec spec{};
    GuestItimerspec oldSpec{};
    for (const int badTimerId : {-9999, -100, -1, 0, 1, 2, validTimerId, 32, 100, 9999}) {
        *__error_nid_postfix() = 0;
        Require(ktimer_delete_nid_postfix(badTimerId) == -1);
        Require(*__error_nid_postfix() == PosixEinval);

        *__error_nid_postfix() = 0;
        Require(ktimer_gettime_nid_postfix(badTimerId, &spec) == -1);
        Require(*__error_nid_postfix() == PosixEinval);

        *__error_nid_postfix() = 0;
        Require(ktimer_getoverrun_nid_postfix(badTimerId) == -1);
        Require(*__error_nid_postfix() == PosixEinval);

        *__error_nid_postfix() = 0;
        Require(ktimer_settime_nid_postfix(badTimerId, 0, &spec, nullptr) == -1);
        Require(*__error_nid_postfix() == PosixEinval);

        *__error_nid_postfix() = 0;
        Require(ktimer_settime_nid_postfix(badTimerId, TimerAbstime, &spec, nullptr) == -1);
        Require(*__error_nid_postfix() == PosixEinval);

        *__error_nid_postfix() = 0;
        Require(ktimer_settime_nid_postfix(badTimerId, 0, &spec, &oldSpec) == -1);
        Require(*__error_nid_postfix() == PosixEinval);
    }
}

void TestKtimerNullAndFlagBounds() {
    const GuestSigevent noSignal{0, 0, 0, {}};
    int timerId = -1;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &noSignal, &timerId) == 0);
    Require(timerId >= 3);

    *__error_nid_postfix() = 0;
    Require(ktimer_gettime_nid_postfix(timerId, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(timerId, 0, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    GuestItimerspec oldSpec{};
    *__error_nid_postfix() = 0;
    Require(ktimer_settime_nid_postfix(timerId, TimerAbstime, nullptr, &oldSpec) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    const GuestItimerspec validSpec{{0, 0}, {1, 0}};
    for (const int badFlag : {-9999, -1, 2, 3, 4, 100, ~TimerAbstime}) {
        *__error_nid_postfix() = 0;
        Require(ktimer_settime_nid_postfix(timerId, badFlag, &validSpec, nullptr) == -1);
        Require(*__error_nid_postfix() == PosixEinval);

        *__error_nid_postfix() = 0;
        Require(ktimer_settime_nid_postfix(timerId, badFlag, &validSpec, &oldSpec) == -1);
        Require(*__error_nid_postfix() == PosixEinval);
    }

    for (const int badId : {-9999, -1, 0, 1, 2, 32, 100, 9999}) {
        *__error_nid_postfix() = 0;
        Require(ktimer_settime_nid_postfix(badId, 0, nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == PosixEfault);

        *__error_nid_postfix() = 0;
        Require(ktimer_settime_nid_postfix(badId, TimerAbstime, nullptr, &oldSpec) == -1);
        Require(*__error_nid_postfix() == PosixEfault);

        *__error_nid_postfix() = 0;
        Require(ktimer_gettime_nid_postfix(badId, nullptr) == -1);
        Require(*__error_nid_postfix() == PosixEinval);
    }

    Require(ktimer_delete_nid_postfix(timerId) == 0);
}

void TestKtimerCreationAndClockBounds() {
    const GuestSigevent noSignal{0, 0, 0, {}};
    int idOut = -1;
    for (const int badClock : {-9999, -1, 1, 2, 3, ClockUptime, 6, 7, 8, 9999}) {
        *__error_nid_postfix() = 0;
        Require(ktimer_create_nid_postfix(badClock, &noSignal, &idOut) == -1);
        Require(*__error_nid_postfix() == PosixEinval);
        Require(idOut == -1);
    }

    const GuestSigevent badNotify{2, 0, 0, {}};
    const GuestSigevent badSigno{1, 0, 0, {}};
    const GuestSigevent overflowSigno{4, 129, 0, {}};

    *__error_nid_postfix() = 0;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &badNotify, &idOut) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &badSigno, &idOut) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &overflowSigno, &idOut) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &noSignal, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(ktimer_create_nid_postfix(ClockRealtime, &noSignal, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(ClockMonotonic, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(clock_gettime_nid_postfix(ClockRealtime, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    KernelTimespec now{};
    Require(clock_gettime_nid_postfix(ClockMonotonic, &now) == 0);
    Require(now.tv_sec > 0 || now.tv_nsec > 0);

    Require(clock_getres_nid_postfix(ClockMonotonic, nullptr) == 0);

    KernelTimespec res{};
    Require(clock_getres_nid_postfix(ClockMonotonic, &res) == 0);
    Require(res.tv_sec > 0 || res.tv_nsec > 0);
}

void TestEqueueTimerBounds() {
    Require(sceKernelCreateEqueue(nullptr, "timer_bounds") == SceKernelErrorEinval);

    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, nullptr) == SceKernelErrorEinval);
    Require(sceKernelCreateEqueue(&eq, "timer_bounds") == 0);
    Require(eq != 0);

    for (const int badTimerId : {-9999, -100, -1, 0, 1, 2, 32, 100, 9999}) {
        Require(sceKernelDeleteTimerEvent(eq, badTimerId) == SceKernelErrorEnoent);
        Require(sceKernelDeleteHRTimerEvent(eq, badTimerId) == SceKernelErrorEnoent);
    }

    Require(sceKernelAddHRTimerEvent(eq, 1, nullptr, nullptr) == SceKernelErrorEfault);

    const KernelTimespec negativeSec{-1, 0};
    Require(sceKernelAddHRTimerEvent(eq, 1, &negativeSec, nullptr) == SceKernelErrorEinval);

    const KernelTimespec negativeNsec{0, -1};
    Require(sceKernelAddHRTimerEvent(eq, 1, &negativeNsec, nullptr) == SceKernelErrorEinval);

    const KernelTimespec overflowNsec{0, Second};
    Require(sceKernelAddHRTimerEvent(eq, 1, &overflowNsec, nullptr) == SceKernelErrorEinval);

    const KernelTimespec validTs{1, 0};
    for (const KernelEqueue badEq : {static_cast<KernelEqueue>(0), static_cast<KernelEqueue>(-1), static_cast<KernelEqueue>(99999)}) {
        Require(sceKernelAddTimerEvent(badEq, 1, 1000, nullptr) == SceKernelErrorEbadf);
        Require(sceKernelDeleteTimerEvent(badEq, 1) == SceKernelErrorEbadf);
        Require(sceKernelAddHRTimerEvent(badEq, 1, &validTs, nullptr) == SceKernelErrorEbadf);
        Require(sceKernelDeleteHRTimerEvent(badEq, 1) == SceKernelErrorEbadf);
    }

    Require(sceKernelDeleteEqueue(eq) == 0);
    Require(sceKernelDeleteTimerEvent(eq, 1) == SceKernelErrorEbadf);
    Require(sceKernelDeleteHRTimerEvent(eq, 1) == SceKernelErrorEbadf);
    Require(sceKernelDeleteEqueue(eq) == SceKernelErrorEbadf);
}

}

int main() {
    TestKtimerInvalidDescriptors();
    TestKtimerNullAndFlagBounds();
    TestKtimerCreationAndClockBounds();
    TestEqueueTimerBounds();
    return 0;
}
