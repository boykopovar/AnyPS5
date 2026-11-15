#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

static void TestValidTimevalBounds() {
    KernelTimeval tv{};
    Require(gettimeofday_nid_postfix(&tv, nullptr) == 0);
    Require(tv.tv_sec > 1600000000LL);
    Require(tv.tv_usec >= 0 && tv.tv_usec <= 999999LL);

    tv = {};
    Require(sceKernelGettimeofday(&tv) == 0);
    Require(tv.tv_sec > 1600000000LL);
    Require(tv.tv_usec >= 0 && tv.tv_usec <= 999999LL);

    tv = {};
    KernelTimezone tz{-1, -1};
    Require(gettimeofday_nid_postfix(&tv, &tz) == 0);
    Require(tv.tv_sec > 1600000000LL);
    Require(tv.tv_usec >= 0 && tv.tv_usec <= 999999LL);
    Require(tz.tz_minuteswest == 0 && tz.tz_dsttime == 0);
}

static void TestNullTimezonePointer() {
    KernelTimeval tv{};
    Require(gettimeofday_nid_postfix(&tv, nullptr) == 0);
    Require(tv.tv_sec > 1600000000LL);
    Require(tv.tv_usec >= 0 && tv.tv_usec <= 999999LL);
}

static void TestNullBothPointers() {
    *__error_nid_postfix() = 0;
    Require(gettimeofday_nid_postfix(nullptr, nullptr) == 0);
    Require(*__error_nid_postfix() == 0);

    KernelTimezone tz{-1, -1};
    *__error_nid_postfix() = 0;
    Require(gettimeofday_nid_postfix(nullptr, &tz) == 0);
    Require(*__error_nid_postfix() == 0);
    Require(tz.tz_minuteswest == 0 && tz.tz_dsttime == 0);

    Require(sceKernelGettimeofday(nullptr) == SCE_KERNEL_ERROR_EFAULT);
}

static void TestMemoryFaultPointerRejection() {
    auto* const badTimeval = reinterpret_cast<KernelTimeval*>(static_cast<std::uintptr_t>(0x10));
    auto* const badTimezone = reinterpret_cast<KernelTimezone*>(static_cast<std::uintptr_t>(0x10));
    KernelTimeval validTv{};
    KernelTimezone validTz{};

    *__error_nid_postfix() = 0;
    Require(gettimeofday_nid_postfix(badTimeval, nullptr) == -1);
    Require(*__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(gettimeofday_nid_postfix(&validTv, badTimezone) == -1);
    Require(*__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(gettimeofday_nid_postfix(badTimeval, badTimezone) == -1);
    Require(*__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(gettimeofday_nid_postfix(nullptr, badTimezone) == -1);
    Require(*__error_nid_postfix() == 14);

    Require(sceKernelGettimeofday(badTimeval) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGettimezone(badTimezone) == SCE_KERNEL_ERROR_EFAULT);
}

static void TestMonotonicProgression() {
    KernelTimeval tv{};
    Require(gettimeofday_nid_postfix(&tv, nullptr) == 0);
    std::int64_t previousUsec = tv.tv_sec * 1000000LL + tv.tv_usec;
    const std::int64_t initialUsec = previousUsec;
    for (int i = 0; i < 10000; ++i) {
        Require(gettimeofday_nid_postfix(&tv, nullptr) == 0);
        const std::int64_t currentUsec = tv.tv_sec * 1000000LL + tv.tv_usec;
        Require(currentUsec >= previousUsec);
        Require(tv.tv_usec >= 0 && tv.tv_usec <= 999999LL);
        previousUsec = currentUsec;
    }
    Require(previousUsec >= initialUsec);

    Require(sceKernelGettimeofday(&tv) == 0);
    previousUsec = tv.tv_sec * 1000000LL + tv.tv_usec;
    for (int i = 0; i < 10000; ++i) {
        Require(sceKernelGettimeofday(&tv) == 0);
        const std::int64_t currentUsec = tv.tv_sec * 1000000LL + tv.tv_usec;
        Require(currentUsec >= previousUsec);
        Require(tv.tv_usec >= 0 && tv.tv_usec <= 999999LL);
        previousUsec = currentUsec;
    }
}

int main() {
    TestValidTimevalBounds();
    TestNullTimezonePointer();
    TestNullBothPointers();
    TestMemoryFaultPointerRejection();
    TestMonotonicProgression();
    return 0;
}
