#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int PosixEinval = 22;
constexpr int PosixEfault = 14;
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016);
constexpr int SceKernelErrorEfault = static_cast<int>(0x8002000E);

constexpr int ClockRealtime = 0;
constexpr int ClockMonotonic = 4;
constexpr int ClockUptime = 5;
constexpr int ClockUptimePrecise = 7;
constexpr int ClockUptimeFast = 8;
constexpr int ClockRealtimePrecise = 9;
constexpr int ClockRealtimeFast = 10;
constexpr int ClockMonotonicPrecise = 11;
constexpr int ClockMonotonicFast = 12;
constexpr int ClockSecond = 13;
constexpr int ClockThreadCputimeId = 14;
constexpr int ClockProcessCputimeId = 15;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestClockGettimeBounds() {
    KernelTimespec ts{};

    const int invalidClocks[] = {-1, -2, -100, 32, 33, 64, 1000};
    for (int clockId : invalidClocks) {
        *__error_nid_postfix() = 0;
        Require(clock_gettime_nid_postfix(clockId, &ts) == -1);
        Require(*__error_nid_postfix() == PosixEinval);

        *__error_nid_postfix() = 0;
        Require(clock_gettime_nid_postfix(clockId, nullptr) == -1);
        Require(*__error_nid_postfix() == PosixEinval);

        Require(sceKernelClockGettime(static_cast<KernelClockid>(clockId), &ts) == SceKernelErrorEinval);
        Require(sceKernelClockGettime(static_cast<KernelClockid>(clockId), nullptr) == SceKernelErrorEfault);
    }

    const int validClocks[] = {
        ClockRealtime,
        ClockMonotonic,
        ClockUptime,
        ClockUptimePrecise,
        ClockUptimeFast,
        ClockRealtimePrecise,
        ClockRealtimeFast,
        ClockMonotonicPrecise,
        ClockMonotonicFast,
        ClockSecond,
        ClockThreadCputimeId,
        ClockProcessCputimeId
    };

    for (int clockId : validClocks) {
        *__error_nid_postfix() = 0;
        Require(clock_gettime_nid_postfix(clockId, nullptr) == -1);
        Require(*__error_nid_postfix() == PosixEfault);

        Require(sceKernelClockGettime(static_cast<KernelClockid>(clockId), nullptr) == SceKernelErrorEfault);

        KernelTimespec validTs{-1, -1};
        *__error_nid_postfix() = 0;
        Require(clock_gettime_nid_postfix(clockId, &validTs) == 0);
        Require(*__error_nid_postfix() == 0);
        Require(validTs.tv_sec >= 0);
        Require(validTs.tv_nsec >= 0);
        Require(validTs.tv_nsec < 1000000000);

        KernelTimespec sceTs{-1, -1};
        Require(sceKernelClockGettime(static_cast<KernelClockid>(clockId), &sceTs) == 0);
        Require(sceTs.tv_sec >= 0);
        Require(sceTs.tv_nsec >= 0);
        Require(sceTs.tv_nsec < 1000000000);
    }

    KernelTimespec secondTs{};
    Require(clock_gettime_nid_postfix(ClockSecond, &secondTs) == 0);
    Require(secondTs.tv_nsec == 0);

    KernelTimespec mono1{};
    KernelTimespec mono2{};
    Require(clock_gettime_nid_postfix(ClockMonotonic, &mono1) == 0);
    Require(clock_gettime_nid_postfix(ClockMonotonic, &mono2) == 0);
    const bool monotonicOrder = (mono2.tv_sec > mono1.tv_sec) ||
        (mono2.tv_sec == mono1.tv_sec && mono2.tv_nsec >= mono1.tv_nsec);
    Require(monotonicOrder);
}

void TestClockGetresBounds() {
    Require(clock_getres_nid_postfix(ClockRealtime, nullptr) == 0);
    Require(clock_getres_nid_postfix(ClockMonotonic, nullptr) == 0);
    Require(clock_getres_nid_postfix(ClockSecond, nullptr) == 0);

    Require(sceKernelClockGetres(static_cast<KernelClockid>(ClockRealtime), nullptr) == SceKernelErrorEfault);
    Require(sceKernelClockGetres(static_cast<KernelClockid>(ClockMonotonic), nullptr) == SceKernelErrorEfault);

    KernelTimespec res{};
    Require(clock_getres_nid_postfix(ClockRealtime, &res) == 0);
    Require(res.tv_sec == 0);
    Require(res.tv_nsec > 0 && res.tv_nsec < 1000000000);

    Require(clock_getres_nid_postfix(ClockMonotonic, &res) == 0);
    Require(res.tv_sec == 0);
    Require(res.tv_nsec > 0 && res.tv_nsec < 1000000000);

    Require(clock_getres_nid_postfix(ClockSecond, &res) == 0);
    Require(res.tv_sec == 1);
    Require(res.tv_nsec == 0);

    KernelTimespec sceRes{};
    Require(sceKernelClockGetres(static_cast<KernelClockid>(ClockRealtime), &sceRes) == 0);
    Require(sceRes.tv_sec == 0);
    Require(sceRes.tv_nsec > 0 && sceRes.tv_nsec < 1000000000);

    bool threw = false;
    try {
        clock_getres_nid_postfix(100, &res);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

} // namespace

int main() {
    TestClockGettimeBounds();
    TestClockGetresBounds();
    return 0;
}
