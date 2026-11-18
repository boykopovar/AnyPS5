#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <climits>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int SceOk = 0;
constexpr std::int64_t NanosPerSecond = 1000000000LL;
constexpr std::uint32_t CpuClockBit = 0x40000000u;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestClockGetresNullPointerBounds() {
    const int validClocks[] = {0, 1, 2, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 21};
    for (const int clockId : validClocks) {
        *__error_nid_postfix() = 0;
        Require(clock_getres_nid_postfix(clockId, nullptr) == 0);
        Require(*__error_nid_postfix() == 0);
    }

    const int invalidClocks[] = {-1000, -50, -1, 3, 6, 15, 16, 20, 22, 100, 9999};
    for (const int clockId : invalidClocks) {
        *__error_nid_postfix() = 0;
        Require(clock_getres_nid_postfix(clockId, nullptr) == -1);
        Require(*__error_nid_postfix() == GUEST_EINVAL);
    }

    for (const int clockId : validClocks) {
        Require(sceKernelClockGetres(static_cast<KernelClockid>(clockId), nullptr) == SCE_KERNEL_ERROR_EFAULT);
    }
    for (const int clockId : invalidClocks) {
        Require(sceKernelClockGetres(static_cast<KernelClockid>(clockId), nullptr) == SCE_KERNEL_ERROR_EFAULT);
    }
}

void TestClockGetresClockIdBoundsAndResolutions() {
    KernelTimespec res{};

    const int invalidClocks[] = {-1000, -50, -1, 3, 6, 15, 16, 20, 22, 100, 9999};
    for (const int clockId : invalidClocks) {
        res = {123, 456};
        *__error_nid_postfix() = 0;
        Require(clock_getres_nid_postfix(clockId, &res) == -1);
        Require(*__error_nid_postfix() == GUEST_EINVAL);
        Require(res.tv_sec == 123 && res.tv_nsec == 456);

        Require(sceKernelClockGetres(static_cast<KernelClockid>(clockId), &res) == SCE_KERNEL_ERROR_EINVAL);
        Require(res.tv_sec == 123 && res.tv_nsec == 456);
    }

    const int clocks81Ns[] = {0, 4, 5, 7, 8, 9, 10, 11, 12};
    for (const int clockId : clocks81Ns) {
        res = {};
        *__error_nid_postfix() = 0;
        Require(clock_getres_nid_postfix(clockId, &res) == 0);
        Require(*__error_nid_postfix() == 0);
        Require(res.tv_sec == 0 && res.tv_nsec == 81);

        res = {};
        Require(sceKernelClockGetres(static_cast<KernelClockid>(clockId), &res) == SceOk);
        Require(res.tv_sec == 0 && res.tv_nsec == 81);
    }

    const int clocks1Ms[] = {1, 2};
    for (const int clockId : clocks1Ms) {
        res = {};
        *__error_nid_postfix() = 0;
        Require(clock_getres_nid_postfix(clockId, &res) == 0);
        Require(*__error_nid_postfix() == 0);
        Require(res.tv_sec == 0 && res.tv_nsec == 1000000);

        res = {};
        Require(sceKernelClockGetres(static_cast<KernelClockid>(clockId), &res) == SceOk);
        Require(res.tv_sec == 0 && res.tv_nsec == 1000000);
    }

    res = {};
    *__error_nid_postfix() = 0;
    Require(clock_getres_nid_postfix(13, &res) == 0);
    Require(*__error_nid_postfix() == 0);
    Require(res.tv_sec == 1 && res.tv_nsec == 0);

    res = {};
    Require(sceKernelClockGetres(static_cast<KernelClockid>(13), &res) == SceOk);
    Require(res.tv_sec == 1 && res.tv_nsec == 0);

    const int clocks1Us[] = {14, 21};
    for (const int clockId : clocks1Us) {
        res = {};
        *__error_nid_postfix() = 0;
        Require(clock_getres_nid_postfix(clockId, &res) == 0);
        Require(*__error_nid_postfix() == 0);
        Require(res.tv_sec == 0 && res.tv_nsec == 1000);

        res = {};
        Require(sceKernelClockGetres(static_cast<KernelClockid>(clockId), &res) == SceOk);
        Require(res.tv_sec == 0 && res.tv_nsec == 1000);
    }

    const int cpuClocks[] = {
        static_cast<int>(CpuClockBit | 0u),
        static_cast<int>(CpuClockBit | 1u),
        static_cast<int>(CpuClockBit | 42u)
    };
    for (const int clockId : cpuClocks) {
        res = {};
        *__error_nid_postfix() = 0;
        Require(clock_getres_nid_postfix(clockId, &res) == 0);
        Require(*__error_nid_postfix() == 0);
        Require(res.tv_sec == 0 && res.tv_nsec == 1000);

        res = {};
        Require(sceKernelClockGetres(static_cast<KernelClockid>(clockId), &res) == SceOk);
        Require(res.tv_sec == 0 && res.tv_nsec == 1000);
    }
}

void TestNanosleepInvalidTvNsecBounds() {
    const KernelTimespec upperBoundaryReq{0, NanosPerSecond};
    const KernelTimespec overReq{0, NanosPerSecond + 1};
    const KernelTimespec largeReq{2, NanosPerSecond * 2};
    const KernelTimespec maxNsecReq{0, INT64_MAX};

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&upperBoundaryReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&overReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&largeReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&maxNsecReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(&upperBoundaryReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(&overReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    Require(sceKernelNanosleep(&upperBoundaryReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&overReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&largeReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&maxNsecReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    const KernelTimespec negNsecReq{0, -1LL};
    const KernelTimespec negNsecFarReq{0, -500000LL};
    const KernelTimespec negBothReq{-5, -1LL};
    const KernelTimespec minNsecReq{0, INT64_MIN};

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&negNsecReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&negNsecFarReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&negBothReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&minNsecReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(&negNsecReq, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    Require(sceKernelNanosleep(&negNsecReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&negNsecFarReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&negBothReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNanosleep(&minNsecReq, nullptr) == SCE_KERNEL_ERROR_EINVAL);
}

void TestNanosleepNullPointerBounds() {
    KernelTimespec remaining{0, 0};

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EFAULT);

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(nullptr, &remaining) == -1);
    Require(*__error_nid_postfix() == GUEST_EFAULT);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EFAULT);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(nullptr, &remaining) == -1);
    Require(*__error_nid_postfix() == GUEST_EFAULT);

    Require(sceKernelNanosleep(nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelNanosleep(nullptr, &remaining) == SCE_KERNEL_ERROR_EFAULT);
}

void TestNanosleepNegativeAndZeroSecondsBounds() {
    const KernelTimespec negSecReq{-1, 0};
    const KernelTimespec negSecWithNsecReq{-10, 5000};
    const KernelTimespec largeNegSecReq{INT64_MIN / 2, 0};

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&negSecReq, nullptr) == 0);
    Require(*__error_nid_postfix() == 0);

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&negSecWithNsecReq, nullptr) == 0);
    Require(*__error_nid_postfix() == 0);

    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&largeNegSecReq, nullptr) == 0);
    Require(*__error_nid_postfix() == 0);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(&negSecReq, nullptr) == 0);
    Require(*__error_nid_postfix() == 0);

    Require(sceKernelNanosleep(&negSecReq, nullptr) == SceOk);
    Require(sceKernelNanosleep(&negSecWithNsecReq, nullptr) == SceOk);
    Require(sceKernelNanosleep(&largeNegSecReq, nullptr) == SceOk);

    const KernelTimespec zeroReq{0, 0};
    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&zeroReq, nullptr) == 0);
    Require(*__error_nid_postfix() == 0);

    *__error_nid_postfix() = 0;
    Require(_nanosleep_nid_postfix(&zeroReq, nullptr) == 0);
    Require(*__error_nid_postfix() == 0);

    Require(sceKernelNanosleep(&zeroReq, nullptr) == SceOk);

    const KernelTimespec shortReq{0, 1000};
    *__error_nid_postfix() = 0;
    Require(nanosleep_nid_postfix(&shortReq, nullptr) == 0);
    Require(*__error_nid_postfix() == 0);

    Require(sceKernelNanosleep(&shortReq, nullptr) == SceOk);
}

void TestUsleepBounds() {
    Require(usleep_nid_postfix(0) == 0);
    Require(usleep_nid_postfix(50) == 0);
    Require(sceKernelUsleep_nid_postfix(0) == 0);
    Require(sceKernelUsleep_nid_postfix(50) == 0);
}

void TestClockSettimeBounds() {
    *__error_nid_postfix() = 0;
    Require(clock_settime_nid_postfix(0, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EFAULT);

    *__error_nid_postfix() = 0;
    Require(clock_settime_nid_postfix(4, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EFAULT);

    const KernelTimespec ts{100, 0};
    *__error_nid_postfix() = 0;
    Require(clock_settime_nid_postfix(0, &ts) == -1);
    Require(*__error_nid_postfix() == GUEST_EPERM);

    *__error_nid_postfix() = 0;
    Require(clock_settime_nid_postfix(4, &ts) == -1);
    Require(*__error_nid_postfix() == GUEST_EPERM);
}

} // namespace

int main() {
    TestClockGetresNullPointerBounds();
    TestClockGetresClockIdBoundsAndResolutions();
    TestNanosleepInvalidTvNsecBounds();
    TestNanosleepNullPointerBounds();
    TestNanosleepNegativeAndZeroSecondsBounds();
    TestUsleepBounds();
    TestClockSettimeBounds();
    return 0;
}
